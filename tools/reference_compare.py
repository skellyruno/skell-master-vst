#!/usr/bin/env python3
"""Private, reproducible, level-matched master comparison; no audio uploaded.

Dependencies: pip install numpy scipy soundfile pyloudnorm
Inputs: same song/take, 2-channel PCM WAV, same sample rate.
Important: --reference-delay/--candidate-delay are latency samples to skip,
NOT independently-estimated best correlations. Establish alignment separately.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import pyloudnorm as pyln
import soundfile as sf
from scipy import signal

BAND_EDGES = [20, 90, 500, 2500, 9310, 20000]
DEFAULT_SECTIONS = [(0.25, 20.0), (20.0, 85.0), (85.0, 98.75)]


def gain_db(value):
    return float(20 * np.log10(max(float(value), 1e-15)))


def load_aligned(path, delay, start, stop, expected_rate=None):
    with sf.SoundFile(path) as audio:
        if audio.channels != 2:
            raise ValueError(f"Expected stereo WAV: {path}")
        rate = audio.samplerate
        if expected_rate is not None and rate != expected_rate:
            raise ValueError(f"Mismatched sample rates: {rate} vs {expected_rate}")
        begin = delay + int(round(start * rate))
        end = delay + int(round(stop * rate))
        if begin < 0 or end > audio.frames or end <= begin:
            raise ValueError(f"Out of bounds segment {start}-{stop}s, delay {delay}: {path}")
        audio.seek(begin)
        return audio.read(end - begin, dtype="float64", always_2d=True), rate


def rms(x):
    return float(np.sqrt(np.mean(np.square(x))))


def basic_stats(x, rate):
    peak = float(np.max(np.abs(x)))
    power = rms(x)
    mid = (x[:, 0] + x[:, 1]) * .5
    side = (x[:, 0] - x[:, 1]) * .5
    return {
        "integrated_lufs": float(pyln.Meter(rate).integrated_loudness(x)),
        "sample_peak_dbfs": gain_db(peak),
        "rms_dbfs": gain_db(power),
        "crest_db": gain_db(peak / max(power, 1e-15)),
        "side_mid_db": gain_db(rms(side) / max(rms(mid), 1e-15)),
    }


def five_band_power(x, rate):
    freqs, spec = signal.welch(x, fs=rate, axis=0, nperseg=16384, window="hann")
    energy = np.mean(spec, axis=1)
    return np.array([
        float(np.trapezoid(energy[(freqs >= lo) & (freqs < hi)], freqs[(freqs >= lo) & (freqs < hi)]))
        for lo, hi in zip(BAND_EDGES, BAND_EDGES[1:])
    ])


def envelope_match(ref, cand, gain, rate, window_ms):
    length = max(1, int(round(rate * window_ms / 1000)))
    windows = min(len(ref), len(cand)) // length
    if windows < 2:
        return {"mae_db": None, "p95_abs_db": None}
    a = ref[: windows * length].reshape(windows, length, 2)
    b = cand[: windows * length].reshape(windows, length, 2)
    ref_db = 10 * np.log10(np.maximum(np.mean(a * a, axis=(1, 2)), 1e-14))
    cand_db = 10 * np.log10(np.maximum(np.mean(b * b, axis=(1, 2)), 1e-14))
    good = (ref_db > -29) & (cand_db > -35)
    if not np.any(good):
        return {"mae_db": None, "p95_abs_db": None}
    delta = np.abs(cand_db[good] + gain - ref_db[good])
    return {"mae_db": float(np.mean(delta)), "p95_abs_db": float(np.percentile(delta, 95))}


def compare(ref, cand, rate):
    x = basic_stats(ref, rate)
    y = basic_stats(cand, rate)
    makeup = x["integrated_lufs"] - y["integrated_lufs"]
    a = five_band_power(ref, rate)
    b = five_band_power(cand, rate)
    band_delta = 10 * np.log10(np.maximum(b, 1e-16) / np.maximum(a, 1e-16)) + makeup
    return {
        "reference": x,
        "candidate": y,
        "delta_candidate_minus_reference": {
            "integrated_lufs": y["integrated_lufs"] - x["integrated_lufs"],
            "crest_db": y["crest_db"] - x["crest_db"],
            "side_mid_db": y["side_mid_db"] - x["side_mid_db"],
        },
        "loudness_match_gain_db": makeup,
        "bands_hz": [[lo, hi] for lo, hi in zip(BAND_EDGES, BAND_EDGES[1:])],
        "bands_loudness_matched_db": [round(float(v), 4) for v in band_delta],
        "five_band_mae_db": float(np.mean(np.abs(band_delta))),
        "envelope_20ms": envelope_match(ref, cand, makeup, rate, 20),
        "envelope_50ms": envelope_match(ref, cand, makeup, rate, 50),
    }


def peak16x(path, chunk=262144, overlap=1000):
    """Optional streamed 16x Kaiser-window resample true-peak estimate."""
    peak = 0.
    with sf.SoundFile(path) as f:
        cursor = 0
        while cursor < f.frames:
            begin = max(0, cursor - overlap)
            end = min(f.frames, cursor + chunk + overlap)
            f.seek(begin)
            samples = f.read(end - begin, dtype="float64", always_2d=True)
            up = signal.resample_poly(samples, 16, 1, axis=0, window=("kaiser", 12.0))
            first = (cursor - begin) * 16
            last = min(len(up), (cursor + chunk - begin) * 16)
            peak = max(peak, float(np.max(np.abs(up[first:last]))))
            cursor += chunk
    return gain_db(peak)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--reference", type=Path, required=True, help="Owner master (same song/take)")
    ap.add_argument("--candidate", type=Path, required=True, help="Rendered Skell VST3 PCM WAV")
    ap.add_argument("--reference-delay", type=int, default=31, help="Source/reference alignment samples")
    ap.add_argument("--candidate-delay", type=int, default=105, help="VST3 reported latency samples at 44.1kHz")
    ap.add_argument("--end", type=float, default=98.75, help="Full-analysis end, seconds")
    ap.add_argument("--start", type=float, default=.25, help="Full-analysis start, seconds")
    ap.add_argument("--true-peak", action="store_true", help="Also calculate streamed 16x estimates on full files (slower)")
    ap.add_argument("--out", type=Path, help="Write JSON to local disk")
    args = ap.parse_args()
    ref, rate = load_aligned(args.reference, args.reference_delay, args.start, args.end)
    cand, _ = load_aligned(args.candidate, args.candidate_delay, args.start, args.end, rate)
    result = {
        "method": "Integrated LUFS level matching (EBU R128 by pyloudnorm); equal-take sample offsets are explicit",
        "source_rate": rate,
        "reference_delay_samples": args.reference_delay,
        "candidate_delay_samples": args.candidate_delay,
        "whole": {"time_seconds": [args.start, args.end], **compare(ref, cand, rate)},
        "segments": {},
    }
    for begin, end in DEFAULT_SECTIONS:
        if begin < args.start or end > args.end:
            continue
        a, _ = load_aligned(args.reference, args.reference_delay, begin, end, rate)
        b, _ = load_aligned(args.candidate, args.candidate_delay, begin, end, rate)
        result["segments"][f"{begin}-{end}"] = compare(a, b, rate)
    if args.true_peak:
        result["true_peak_16x_dbTP"] = {
            "reference": peak16x(args.reference),
            "candidate": peak16x(args.candidate),
        }
    encoded = json.dumps(result, indent=2)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)


if __name__ == "__main__":
    main()
