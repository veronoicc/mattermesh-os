#!/usr/bin/env python3
"""Self-check of the exporter's numeric pieces: bit packing against the engine's unpackBlob() order, row quantization, BN folding.
Run with the project venv: tools/offline_asr/.venv/bin/python tools/offline_asr/test_export.py"""
import numpy as np, torch
import export_nemo as e


def unpack(data, bits, n):
    """What asr2.cpp unpackBlob() does: 8 values per `bits` bytes, little-endian bit stream, sign-extended."""
    out = []
    for g in range(n // 8):
        v = int.from_bytes(data[g * bits:(g + 1) * bits] + bytes(8 - bits), 'little')
        for k in range(8):
            u = (v >> (bits * k)) & ((1 << bits) - 1)
            out.append((u ^ (1 << (bits - 1))) - (1 << (bits - 1)))
    return np.array(out, dtype=np.int8)


rng = np.random.default_rng(1)
for bits in (4, 5, 6, 7, 8):
    q = rng.integers(-(2 ** (bits - 1) - 1), 2 ** (bits - 1), size=64 * 16).astype(np.int8)
    packed = e.pack_bits(q, bits) if bits < 8 else q.tobytes()
    assert len(packed) == q.size * bits // 8
    assert (unpack(packed, bits, q.size) == q).all(), bits

# per-row scale: the clip search never loses to plain max scaling, and values stay inside the signed range
w = rng.standard_normal((32, 256)).astype(np.float32)
w[:, 0] *= 12  # one outlier per row
for bits in (6, 8):
    q, scale = e.quantize_rows(w, bits)
    qmax = 2 ** (bits - 1) - 1
    assert np.abs(q).max() <= qmax and scale.shape == (32,)
    plain = np.rint(w / (np.abs(w).max(axis=1, keepdims=True) / qmax)) * (np.abs(w).max(axis=1, keepdims=True) / qmax)
    assert ((q * scale[:, None] - w) ** 2).sum() <= ((plain - w) ** 2).sum() + 1e-6

# BatchNorm folded into a 1x1 convolution equals convolution followed by BatchNorm (eval mode, NeMo eps)
x = torch.randn(1, 8, 20)
conv = torch.nn.Conv1d(8, 12, 1, bias=False)
bn = torch.nn.BatchNorm1d(12, eps=e.BN_EPS).eval()
bn.running_mean.normal_(); bn.running_var.uniform_(0.5, 2); bn.weight.data.normal_(); bn.bias.data.normal_()
params = dict(weight=bn.weight.data, bias=bn.bias.data, running_mean=bn.running_mean, running_var=bn.running_var)
fw, fb = e.fold(conv.weight.data, None, params, e.BN_EPS)
assert torch.allclose(bn(conv(x)), torch.nn.functional.conv1d(x, fw[:, :, None], fb), atol=1e-5)
print('PASS: exporter bit packing, row quantization, BatchNorm folding')
