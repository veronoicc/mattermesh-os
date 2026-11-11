#!/usr/bin/env python3
"""NeMo QuartzNet (.nemo) -> MMQIT002 model for src/offline_it/asr2.{h,cpp}.

Reads the archive with plain torch + pyyaml (no NeMo toolkit): model_config.yaml + model_weights.ckpt.
Maps the Jasper/QuartzNet encoder (separable blocks, add-residual, dense residual off) and the 1x1 conv
decoder onto the engine's two operators (depthwise, pointwise), folds every BatchNorm into the preceding
convolution, quantizes to int8 with one scale per output row (pointwise rows optionally packed to 6 bits,
the format of the shipped Italian model) and writes header, operator table, vocabulary and CRCs.

    python export_nemo.py stt_en_quartznet15x5.nemo asr-en.mmq [--bits 6]

File layout (all little endian), see asr2.cpp open():
    0   "MMQIT002", 18 u32 fields, table CRC at 80, header CRC-32 (bytes 0..123) at 124
    128 operator table, 64 bytes per op (16 u32), then the vocabulary ([len][utf-8] per class), padded to 16
    then one blob per op, 16-byte aligned: int8 weights (rows padded to 16), f32 scales, f32 bias.
"""
import argparse, io, re, struct, sys, tarfile, zlib
import numpy as np, torch, yaml

HEADER, OPREC, MAX_OPS, MAX_CLASSES, MAX_KERNEL, MAX_MELS = 128, 64, 256, 64, 127, 80
DEPTHWISE, POINTWISE = 2, 3
RELU, SAVE_RESIDUAL, ADD_RESIDUAL, CLASSIFIER = 1, 2, 4, 8
BN_EPS = 1e-3  # NeMo JasperBlock builds nn.BatchNorm1d(planes, eps=1e-3, momentum=0.1)


def load_nemo(path):
    """(config dict, state dict) of a .nemo archive."""
    with tarfile.open(path) as tar:
        names = {m.name.split('/')[-1]: m for m in tar.getmembers() if m.isfile()}
        cfg = yaml.safe_load(tar.extractfile(names['model_config.yaml']).read())
        state = torch.load(io.BytesIO(tar.extractfile(names['model_weights.ckpt']).read()), map_location='cpu', weights_only=True)
    return cfg, state


def frontend(cfg):
    """Validate that the checkpoint uses the front end the engine implements; returns (mels, pad_to, preemph)."""
    p = cfg['preprocessor']
    sr, win, hop, nfft = p.get('sample_rate', 16000), p.get('window_size', 0.02), p.get('window_stride', 0.01), p.get('n_fft') or 512
    ok = sr == 16000 and abs(win * sr - 320) < 1e-6 and abs(hop * sr - 160) < 1e-6 and nfft == 512
    ok = ok and p.get('window', 'hann') == 'hann' and p.get('normalize', 'per_feature') == 'per_feature' and p.get('log', True)
    ok = ok and p.get('frame_splicing', 1) == 1 and p.get('mag_power', 2.0) == 2.0 and p.get('log_zero_guard_type', 'add') == 'add'
    ok = ok and abs(p.get('log_zero_guard_value', 2 ** -24) - 2 ** -24) < 1e-12 and p.get('highfreq') in (None, 8000) and p.get('lowfreq', 0) == 0
    if not ok:
        raise SystemExit('unsupported preprocessor: %s' % p)
    mels = int(p.get('features', p.get('nfilt', 64)))
    pre = p.get('preemph', 0.97)
    if pre not in (None, 0.97):
        raise SystemExit('engine implements preemphasis 0.97 only, checkpoint has %s' % pre)
    return mels, int(p.get('pad_to', 16)), bool(pre)

def blocks_of(cfg, state):
    """Encoder blocks: list of dict(index, stride, dilation, residual, repeats=[[convs.., bn]], res=(conv, bn) or None)."""
    jasper = cfg['encoder']['jasper']
    out = []
    for i, spec in enumerate(jasper):
        if spec.get('residual_dense') or spec.get('se') or spec.get('stride_last') or spec.get('residual_mode', 'add') != 'add':
            raise SystemExit('block %d uses unsupported residual/SE/stride_last options' % i)
        pre = 'encoder.encoder.%d.' % i
        layers = sorted({int(m.group(1)) for k in state for m in [re.match(re.escape(pre) + r'mconv\.(\d+)\.', k)] if m})
        repeats, current = [], []
        for j in layers:
            base = pre + 'mconv.%d.' % j
            if base + 'conv.weight' in state:
                w = state[base + 'conv.weight'].float()
                bias = state.get(base + 'conv.bias')
                current.append(dict(w=w, bias=None if bias is None else bias.float()))
            elif base + 'running_mean' in state:
                current.append(dict(bn={n: state[base + n].float() for n in ('weight', 'bias', 'running_mean', 'running_var')}))
                repeats.append(current)
                current = []
        if current or len(repeats) != spec['repeat']:
            raise SystemExit('block %d: found %d repeats, config says %d' % (i, len(repeats), spec['repeat']))
        res = None
        if spec.get('residual', True):
            rb = pre + 'res.0.'
            rbias = state.get(rb + '0.conv.bias')
            res = (dict(w=state[rb + '0.conv.weight'].float(), bias=None if rbias is None else rbias.float()), {n: state[rb + '1.' + n].float() for n in ('weight', 'bias', 'running_mean', 'running_var')})
        out.append(dict(index=i, stride=spec['stride'][0], dilation=spec['dilation'][0], residual=res, repeats=repeats))
    return out


def fold(w, bias, bn, eps):
    """Fold BatchNorm into a 1x1 convolution: returns (weight [co, ci], bias [co])."""
    w = w[:, :, 0] if w.dim() == 3 else w
    g = bn['weight'] / torch.sqrt(bn['running_var'] + eps)
    b = bn['bias'] - bn['running_mean'] * g + (0 if bias is None else bias * g)
    return w * g[:, None], b


def round_up(n, m=16):
    return (n + m - 1) // m * m


def quantize_rows(w, bits):
    """Symmetric per-row quantization to `bits`; the clip point of each row is searched to minimise its squared error
    (matters at 6 bits, where one outlier otherwise costs the whole row a bit). Returns (int8 [co, ci], scale f32 [co])."""
    w = np.asarray(w, dtype=np.float64)
    qmax = 2 ** (bits - 1) - 1
    peak = np.maximum(np.abs(w).max(axis=1), 1e-12)
    best_err, best_scale = None, None
    for ratio in np.linspace(0.55, 1.0, 19):
        scale = peak * ratio / qmax
        q = np.clip(np.rint(w / scale[:, None]), -qmax, qmax)
        err = ((q * scale[:, None] - w) ** 2).sum(axis=1)
        if best_err is None:
            best_err, best_scale = err, scale
        else:
            better = err < best_err
            best_err, best_scale = np.where(better, err, best_err), np.where(better, scale, best_scale)
    q = np.clip(np.rint(w / best_scale[:, None]), -qmax, qmax).astype(np.int8)
    return q, best_scale.astype(np.float32)


def pack_bits(q, bits):
    """int8 [n] (n % 8 == 0) -> little-endian bit stream, 8 values in `bits` bytes (two's complement), as unpackBlob() reads it."""
    u = (q.astype(np.int64) & ((1 << bits) - 1)).astype(np.uint64).reshape(-1, 8)
    word = np.zeros(u.shape[0], dtype=np.uint64)
    for k in range(8):
        word |= u[:, k] << np.uint64(bits * k)
    return word.astype('<u8').view(np.uint8).reshape(-1, 8)[:, :bits].tobytes()


def pointwise_blob(w, bias, bits):
    """(stored bytes, blobBytes, bits) of a pointwise op: weights [co, row], scales, bias. Packed ops store the packed weights
    followed by the f32 scales and bias; blobBytes is the unpacked int8 image the loader rebuilds."""
    co, ci = w.shape
    row = round_up(ci)
    padded = np.zeros((co, row), dtype=np.float32)
    padded[:, :ci] = w
    q, scale = quantize_rows(padded, bits)
    tail = scale.astype('<f4').tobytes() + np.asarray(bias, dtype='<f4').tobytes()
    weights = q.reshape(-1).tobytes() if bits == 8 else pack_bits(q.reshape(-1), bits)
    return weights + tail, co * row + 8 * co, row


def depthwise_blob(w, kernel):
    """Depthwise [c, 1, K] -> weights [c, row] int8 (row padded to 16) + f32 scales. bias slot reuses the scale slot."""
    w = w[:, 0, :].numpy()
    c = w.shape[0]
    row = round_up(kernel)
    padded = np.zeros((c, row), dtype=np.float32)
    padded[:, :kernel] = w
    q, scale = quantize_rows(padded, 8)
    return q.reshape(-1).tobytes() + scale.astype('<f4').tobytes(), c * row + 4 * c, row


def identity_blob(c):
    """Kernel-1 depthwise that passes its input through exactly: int8 weight 1, scale 1.0."""
    weights = np.zeros((c, 16), dtype=np.int8)
    weights[:, 0] = 1
    return weights.tobytes() + np.ones(c, dtype='<f4').tobytes(), 16 * c + 4 * c, 16


class Op:
    def __init__(self, kind, ci, co, kernel, stride, flags, row, stored, blob_bytes, bits, dilation=1):
        self.kind, self.ci, self.co, self.kernel, self.stride, self.flags, self.row = kind, ci, co, kernel, stride, flags, row
        self.stored, self.blob_bytes, self.bits, self.dilation = stored, blob_bytes, bits, dilation


def build_ops(cfg, state, bits, eps):
    mels, pad_to, preemph = frontend(cfg)
    ops, channels = [], mels

    def depthwise(w, kernel, stride, dilation, ci):
        stored, blob, row = depthwise_blob(w, kernel)
        ops.append(Op(DEPTHWISE, ci, ci, kernel, stride, 0, row, stored, blob, 8, dilation))

    def pointwise(w, b, flags, op_bits):
        stored, blob, row = pointwise_blob(w.numpy(), b.numpy(), op_bits)
        ops.append(Op(POINTWISE, w.shape[1], w.shape[0], 1, 1, flags, row, stored, blob, op_bits))

    for block in blocks_of(cfg, state):
        out = block['repeats'][-1][-2]['w'].shape[0]
        if block['residual']:
            w, b = fold(block['residual'][0]['w'], block['residual'][0]['bias'], block['residual'][1], eps)
            pointwise(w, b, SAVE_RESIDUAL, bits)
        for r, repeat in enumerate(block['repeats']):
            convs, bn = repeat[:-1], repeat[-1]['bn']
            last = r == len(block['repeats']) - 1
            flags = RELU | (ADD_RESIDUAL if last and block['residual'] else 0)
            if len(convs) == 2:  # separable: depthwise (groups == channels) then 1x1
                dw, pw = convs
                if dw['w'].shape[1] != 1 or dw['bias'] is not None and float(dw['bias'].abs().max()) > 0 or pw['w'].shape[2] != 1:
                    raise SystemExit('block %d: not a bias-free depthwise + 1x1 pair' % block['index'])
                kernel = dw['w'].shape[2]
                if kernel > MAX_KERNEL or dw['w'].shape[0] != channels:
                    raise SystemExit('block %d: kernel %d / channels %d unsupported' % (block['index'], kernel, dw['w'].shape[0]))
                depthwise(dw['w'], kernel, block['stride'] if r == 0 else 1, block['dilation'], channels)
                pwc = pw
            else:  # plain kernel-1 convolution: the engine runs a pointwise after a depthwise, so insert an identity one
                pwc = convs[0]
                if pwc['w'].shape[2] != 1:
                    raise SystemExit('block %d: dense convolution with kernel %d is not supported' % (block['index'], pwc['w'].shape[2]))
                stored, blob, row = identity_blob(channels)
                ops.append(Op(DEPTHWISE, channels, channels, 1, 1, 0, row, stored, blob, 8))
            w, b = fold(pwc['w'], pwc['bias'], bn, eps)
            pointwise(w, b, flags, bits)
            channels = w.shape[0]
        assert channels == out
    dw = state['decoder.decoder_layers.0.weight'].float()[:, :, 0]
    pointwise(dw, state['decoder.decoder_layers.0.bias'].float(), CLASSIFIER, 8)
    return ops, mels, pad_to, preemph, dw.shape[0]


def vocabulary(cfg, classes):
    labels = cfg.get('decoder', {}).get('vocabulary') or cfg.get('labels') or cfg['decoder'].get('labels')
    if labels is None or len(labels) + 1 != classes:
        raise SystemExit('vocabulary (%s entries) does not match the decoder output (%d classes, last one blank)' % (None if labels is None else len(labels), classes))
    if classes > MAX_CLASSES:
        raise SystemExit('too many classes for the engine: %d' % classes)
    entries = [str(label).encode('utf-8') for label in labels] + [b'']
    if any(len(e) > 4 for e in entries):
        raise SystemExit('vocabulary entry longer than 4 UTF-8 bytes')
    return b''.join(bytes([len(e)]) + e for e in entries)


def write_model(ops, mels, pad_to, preemph, classes, vocab, path):
    if len(ops) > MAX_OPS or mels > MAX_MELS:
        raise SystemExit('model exceeds engine limits (%d ops, %d mels)' % (len(ops), mels))
    vocab_offset = HEADER + OPREC * len(ops)
    blob_start = round_up(vocab_offset + len(vocab))
    offset, records, blobs = blob_start, [], []
    for op in ops:
        n = op.co * op.row
        weights_end = n  # unpacked int8 image: weights [co*row], scales [co], bias [co]
        scale_at = offset + weights_end
        bias_at = scale_at + (4 * op.co if op.kind == POINTWISE else 0)
        stored = op.stored
        crc = zlib.crc32(stored)
        records.append(struct.pack('<16I', op.kind, op.ci, op.co, op.kernel, op.stride, op.flags, op.row, offset, op.blob_bytes,
                                   offset, scale_at, bias_at, op.dilation, crc, len(stored), op.bits))
        blobs.append((offset, stored))
        offset = round_up(offset + len(stored))
    size = offset
    table = b''.join(records)
    tail = vocab + bytes(blob_start - vocab_offset - len(vocab))
    max_blob = max(op.blob_bytes for op in ops)
    fields = [2, size, len(ops), 1, mels, classes, 16000, 1, 320, 512, 160, pad_to, classes - 1, vocab_offset, len(vocab), max_blob, 1 if preemph else 0, max(max(op.ci, op.co) for op in ops)]
    header = bytearray(HEADER)
    header[0:8] = b'MMQIT002'
    struct.pack_into('<18I', header, 8, *fields)
    struct.pack_into('<I', header, 80, zlib.crc32(tail, zlib.crc32(table)))
    struct.pack_into('<I', header, 124, zlib.crc32(bytes(header[:124])))
    image = bytearray(size)
    image[:HEADER] = header
    image[HEADER:vocab_offset] = table
    image[vocab_offset:blob_start] = tail
    for at, data in blobs:
        image[at:at + len(data)] = data
    with open(path, 'wb') as f:
        f.write(image)
    return size, len(ops), max_blob


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('nemo')
    ap.add_argument('out')
    ap.add_argument('--bits', type=int, default=6, choices=(4, 5, 6, 7, 8), help='pointwise weight bits (default 6, the shipped Italian format; 8 = unpacked)')
    ap.add_argument('--bn-eps', type=float, default=BN_EPS)
    args = ap.parse_args()
    cfg, state = load_nemo(args.nemo)
    ops, mels, pad_to, preemph, classes = build_ops(cfg, state, args.bits, args.bn_eps)
    size, count, max_blob = write_model(ops, mels, pad_to, preemph, classes, vocabulary(cfg, classes), args.out)
    print('%s: %d bytes, %d ops, %d classes, %d mels, maxBlob %d, pointwise %d-bit' % (args.out, size, count, classes, mels, max_blob, args.bits))


if __name__ == '__main__':
    main()
