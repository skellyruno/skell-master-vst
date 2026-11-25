#!/usr/bin/env python3
"""Private local VST3 quality audit; requires numpy, scipy, soundfile, pyloudnorm.
Never commit generated WAVs or source audio. Output directory must be new.
"""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess

import numpy as np
import pyloudnorm as loudness
import scipy.signal as signal
import soundfile as sf


def sha256(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runner', required=True, type=pathlib.Path)
    p.add_argument('--plugin', required=True, type=pathlib.Path)
    p.add_argument('--input', required=True, type=pathlib.Path)
    p.add_argument('--output', required=True, type=pathlib.Path)
    p.add_argument('--rates', nargs='+', type=int, choices=[44100, 48000, 96000],
                   default=[44100, 48000, 96000])
    p.add_argument('--creative', action='store_true', help='Stress Width100/Sauce100/Input12')
    a = p.parse_args()
    runner, plugin, source, output = [x.resolve() for x in (a.runner, a.plugin, a.input, a.output)]
    x, sr = sf.read(source, always_2d=True)
    if x.shape[1] != 2 or sr not in (44100, 48000, 96000) or len(x) < 3*sr:
        p.error('Expected >=3 seconds of stereo audio at 44.1/48/96 kHz')
    if not np.isfinite(x).all() or np.max(np.abs(x)) > 1:
        p.error('Input must be finite, within PCM range')
    output.mkdir(parents=True, exist_ok=False)
    result = {'input_sha256': sha256(source), 'runner_sha256': sha256(runner),
              'creative': a.creative, 'measurements': []}
    for rate in sorted(set(a.rates)):
        inp = output / f'input-{rate}.wav'
        y = x if rate == sr else signal.resample_poly(x, rate, sr, axis=0)
        # Reject resampling overshoots rather than silently clipping the source.
        if np.max(np.abs(y)) > 1:
            raise ValueError('Resampled input exceeds PCM range; supply headroom')
        sf.write(inp, y, rate, subtype='PCM_24')
        rows = []
        for factor in (2, 4, 8):
            wav = output / f'quality-{rate}-{factor}.wav'
            cmd = [str(runner), str(plugin), str(inp), str(wav), '--oversampling', str(factor)]
            if a.creative:
                cmd += ['--factory', '1', '--set', 'Input=12', '--set', 'Sauce=100',
                        '--set', 'Spatial Width=100']
            run = subprocess.run(cmd, capture_output=True, text=True, check=False)
            (output / f'host-{rate}-{factor}.txt').write_text(run.stdout+run.stderr, encoding='utf-8')
            run.check_returncode()
            line = next((s for s in run.stdout.splitlines() if 'VST3_HOST_RENDER PASS' in s), '')
            fields = dict(re.findall(r'(\w+)=(-?\d+(?:\.\d+)?(?:e[-+]?\d+)?)', line))
            if not line or int(fields['oversampling']) != factor:
                raise RuntimeError('Host did not confirm requested quality')
            latency = int(fields['latency_samples'])
            z, got_rate = sf.read(wav, always_2d=True)
            if got_rate != rate or z.shape[1] != 2 or len(z) < len(y)+latency:
                raise RuntimeError('Unexpected output format or length')
            z = z[latency+rate:latency+len(y)-rate]
            if not np.isfinite(z).all():
                raise RuntimeError('Nonfinite render')
            peak = float(20*np.log10(max(1e-15, np.max(np.abs(signal.resample_poly(z, 16, 1, axis=0))))))
            lu = float(loudness.Meter(rate).integrated_loudness(z))
            if not np.isfinite(lu) or peak > 0:
                raise RuntimeError('Silent or above-full-scale measured render')
            rows.append({'rate': rate, 'factor': factor, 'latency_samples': latency,
                         'cpu_percent_one_core': float(fields['cpu_percent_one_core']),
                         'host_peak_dbtp': float(fields['reconstructed_peak_dbtp']),
                         'interior_16x_peak_dbtp_estimate': peak, 'lufs': lu,
                         'output_sha256': sha256(wav)})
        standard = rows[1]['lufs']
        for row in rows:
            row['loudness_delta_vs_4x_lu'] = row['lufs'] - standard
        result['measurements'].extend(rows)
        (output / 'report.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n', encoding='utf-8')
        print(json.dumps(rows, indent=2), flush=True)


if __name__ == '__main__':
    main()
