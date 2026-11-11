#!/usr/bin/env python3
"""Float reference vs the firmware's MMQIT002 engine on real speech clips; prints both transcripts and word error rates.

    python compare.py stt_en_quartznet15x5.nemo asr-en.mmq clip.wav [clip2.wav ...]    # clip.txt (reference text) optional

The engine side is src/offline_it/asr2.cpp built into a host binary (tools/offline_asr/asr2_cli.cpp), i.e. the same code
that runs on the device, fed from the .mmq file.
"""
import os, re, subprocess, sys
from reference_nemo import Reference, read_wav

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def engine_binary():
    exe = os.path.join(ROOT, 'artifacts', 'offline-asr', 'asr2_cli')
    sources = [os.path.join(ROOT, 'src/offline_it/asr2.cpp'), os.path.join(HERE, 'asr2_cli.cpp')]
    if not os.path.exists(exe) or os.path.getmtime(exe) < max(os.path.getmtime(s) for s in sources):
        subprocess.run(['g++', '-std=c++14', '-O2', '-I' + os.path.join(ROOT, 'src')] + sources + ['-o', exe], check=True)
    return exe


def words(text):
    return re.sub(r"[^a-zäöüß' ]", '', re.sub(r'[-.]', lambda m: ' ' if m.group() == '-' else '', text.lower())).split()


def edits(a, b):
    row = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        prev, row[0] = row[0], i
        for j, y in enumerate(b, 1):
            prev, row[j] = row[j], min(row[j] + 1, row[j - 1] + 1, prev + (x != y))
    return row[-1]


def main(nemo, mmq, clips):
    ref = Reference(nemo)
    out = subprocess.run([engine_binary(), mmq] + clips, check=True, capture_output=True, text=True)
    sys.stderr.write(out.stderr)
    engine = dict(line.split('\t') for line in out.stdout.splitlines())
    total = {'n': 0, 'float~truth': 0, 'engine~truth': 0, 'engine~float': 0}
    for clip in clips:
        f, e = ref.transcribe(read_wav(clip)), engine[clip].strip()
        print('%s\n  float : %s\n  engine: %s' % (os.path.basename(clip), f, e))
        txt = clip[:-4] + '.txt'
        if os.path.exists(txt):
            truth = words(open(txt, encoding='utf-8').read())
            print('  truth : %s' % ' '.join(truth))
            total['n'] += len(truth)
            total['float~truth'] += edits(truth, words(f))
            total['engine~truth'] += edits(truth, words(e))
        total['engine~float'] += edits(words(f), words(e))
    if total['n']:
        print('WER float vs truth %.1f%%, engine vs truth %.1f%%, engine vs float %.1f%% (%d reference words)' % (
            100.0 * total['float~truth'] / total['n'], 100.0 * total['engine~truth'] / total['n'], 100.0 * total['engine~float'] / total['n'], total['n']))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
