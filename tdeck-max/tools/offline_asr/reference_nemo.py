#!/usr/bin/env python3
"""Float PyTorch forward of a NeMo QuartzNet checkpoint (no NeMo toolkit), the host reference for the MMQIT002 engine.

Unfolded BatchNorm, float32 everywhere, greedy CTC. The NeMo front end uses the checkpoint's own mel filterbank and window;
framing follows the engine (samples // 160 + 1 frames, statistics over all of them, zero padding to a multiple of pad_to).

    python reference_nemo.py stt_en_quartznet15x5.nemo a.wav [b.wav ...]      # 16 kHz mono 16-bit WAV
"""
import sys, wave
import numpy as np, torch
import torch.nn.functional as F
from export_nemo import BN_EPS, load_nemo, frontend


def read_wav(path):
    with wave.open(path) as w:
        assert w.getframerate() == 16000 and w.getnchannels() == 1 and w.getsampwidth() == 2, 'need 16 kHz mono 16-bit: ' + path
        return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2')


class Reference:
    def __init__(self, nemo_path, bn_eps=BN_EPS):
        self.cfg, self.state = load_nemo(nemo_path)
        self.mels, self.pad_to, self.preemph = frontend(self.cfg)
        self.eps = bn_eps
        self.vocab = list(self.cfg['decoder']['vocabulary'])

    def features(self, pcm):
        s = self.state
        x = torch.from_numpy(pcm.astype(np.float32) / 32768.0)
        if self.preemph:
            x = torch.cat([x[:1], x[1:] - 0.97 * x[:-1]])
        spec = torch.stft(x, 512, hop_length=160, win_length=320, center=True, window=s['preprocessor.featurizer.window'].float(), pad_mode='reflect', return_complex=True)
        power = spec.real ** 2 + spec.imag ** 2                       # [257, frames]
        mel = torch.log(torch.matmul(s['preprocessor.featurizer.fb'].float()[0], power) + 2 ** -24)
        valid = len(pcm) // 160 + 1
        mel = mel[:, :valid]
        mean = mel.mean(dim=1, keepdim=True)
        std = torch.sqrt(((mel - mean) ** 2).sum(dim=1, keepdim=True) / (valid - 1)) + 1e-5
        mel = (mel - mean) / std
        padded = (valid + self.pad_to - 1) // self.pad_to * self.pad_to
        return F.pad(mel, (0, padded - valid))[None]                   # [1, mels, padded]

    def bn(self, x, base):
        s = self.state
        return F.batch_norm(x, s[base + 'running_mean'], s[base + 'running_var'], s[base + 'weight'], s[base + 'bias'], False, 0.0, self.eps)

    def logits(self, pcm):
        s, x = self.state, self.features(pcm)
        for i, spec in enumerate(self.cfg['encoder']['jasper']):
            pre, block_in = 'encoder.encoder.%d.' % i, x
            ids = sorted({int(k[len(pre):].split('.')[1]) for k in s if k.startswith(pre + 'mconv.')})
            stride, dilation, repeats, seen = spec['stride'][0], spec['dilation'][0], spec['repeat'], 0
            for j in ids:
                base = pre + 'mconv.%d.' % j
                if base + 'conv.weight' in s:
                    w = s[base + 'conv.weight'].float()
                    depthwise = w.shape[1] == 1 and w.shape[2] > 1
                    k = w.shape[2]
                    x = F.conv1d(x, w, s.get(base + 'conv.bias'), stride=stride if depthwise and seen == 0 else 1, padding=(dilation * (k - 1)) // 2 if depthwise else 0, dilation=dilation if depthwise else 1, groups=w.shape[0] if depthwise else 1)
                    if not depthwise and w.shape[2] > 1:
                        raise SystemExit('dense convolution unsupported')
                elif base + 'running_mean' in s:
                    x = self.bn(x, base)
                    seen += 1
                    if seen == repeats:
                        if spec.get('residual', True):
                            r = F.conv1d(block_in, s[pre + 'res.0.0.conv.weight'].float())
                            x = x + self.bn(r, pre + 'res.0.1.')
                        x = torch.relu(x)
                    else:
                        x = torch.relu(x)
        return F.conv1d(x, s['decoder.decoder_layers.0.weight'].float(), s['decoder.decoder_layers.0.bias'].float())[0].T

    def transcribe(self, pcm):
        best = self.logits(pcm).argmax(dim=1).tolist()
        blank, text, previous = len(self.vocab), '', None
        for c in best:
            if c != blank and c != previous:
                text += self.vocab[c]
            previous = c
        return ' '.join(text.split())


if __name__ == '__main__':
    ref = Reference(sys.argv[1])
    for path in sys.argv[2:]:
        print('%s\t%s' % (path, ref.transcribe(read_wav(path))))
