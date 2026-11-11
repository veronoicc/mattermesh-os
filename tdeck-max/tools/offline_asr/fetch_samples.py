#!/usr/bin/env python3
"""Download a few public speech clips with reference text (Google FLEURS, CC-BY 4.0, via the Hugging Face dataset server).

    python fetch_samples.py out_dir [count]       # writes out_dir/{en,de}-NN.wav (16 kHz mono) and .txt (reference)

Clips longer than 10 s (the engine's MaxSeconds) are skipped; the shortest ones are kept.
"""
import json, os, subprocess, sys, tempfile, urllib.request

ROWS = 'https://datasets-server.huggingface.co/rows?dataset=google/fleurs&config=%s&split=%s&offset=%d&length=100'
LANGS = {'en': 'en_us', 'de': 'de_de'}


def main(out, count=6):
    os.makedirs(out, exist_ok=True)
    for lang, config in LANGS.items():
        rows = []
        for offset in (0, 100):
            with urllib.request.urlopen(ROWS % (config, 'validation', offset)) as r:
                rows += [x['row'] for x in json.load(r)['rows']]
        rows = sorted((x for x in rows if x['num_samples'] <= 160000), key=lambda x: x['num_samples'])[:count]
        for n, row in enumerate(rows):
            base = os.path.join(out, '%s-%02d' % (lang, n))
            with tempfile.NamedTemporaryFile(suffix='.wav') as raw:
                raw.write(urllib.request.urlopen(row['audio'][0]['src']).read())
                raw.flush()
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', raw.name, '-ar', '16000', '-ac', '1', '-sample_fmt', 's16', base + '.wav'], check=True)
            with open(base + '.txt', 'w') as f:
                f.write(row['transcription'].strip() + '\n')
            print(base, row['num_samples'] / 16000.0, 's:', row['transcription'])


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 6)
