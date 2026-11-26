#!/usr/bin/env python3
"""Validate native VST3 DEFAULT vs host BYPASS without assuming identical host routing.

The JUCE UI/processor test separately checks sample-exact internal dry-path audio.
A VST3 host can skip oversampling and reported latency when its native bypass
is on, changing individual samples while retaining transparent level and tone.
"""
import math
import sys
import wave


def read_pcm24_stereo(path):
    with wave.open(path, "rb") as f:
        assert f.getnchannels() == 2 and f.getsampwidth() == 3, (
            f"{path}: expected stereo PCM24")
        rate, frames = f.getframerate(), f.getnframes()
        data = f.readframes(frames)
    n = len(data) // 6
    a, b = [0.0] * n, [0.0] * n
    scale = 1.0 / 8388608.0
    for i in range(n):
        off = 6 * i
        a[i] = int.from_bytes(data[off:off+3], "little", signed=True) * scale
        b[i] = int.from_bytes(data[off+3:off+6], "little", signed=True) * scale
    return rate, (a, b)


def db_ratio(a, b):
    return 20.0 * math.log10(max(a, 1e-15) / max(b, 1e-15))


def rms(a):
    return math.sqrt(sum(x*x for x in a) / len(a))


def max_lag_corr(a, b, rate):
    start = min(len(a)//3, max(4096, rate//5))
    stop = min(len(a), len(b)) - max(2048, rate//12)
    stride = 8
    if stop-start < rate//4:
        return 0.0, 0
    best = (-2.0, 0)
    for shift in range(-256, 257):
        dot = sa = sb = 0.0
        for i in range(start, stop, stride):
            x, y = a[i], b[i+shift]
            dot += x*y
            sa += x*x
            sb += y*y
        corr = dot / math.sqrt(max(sa*sb, 1e-25))
        if corr > best[0]:
            best = (corr, shift)
    return best


def main():
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: python tests/check_transparent_default.py DEFAULT.wav BYPASS.wav")
    r1, c1 = read_pcm24_stereo(sys.argv[1])
    r2, c2 = read_pcm24_stereo(sys.argv[2])
    assert r1 == r2 and len(c1[0]) == len(c2[0]), (
        "rendered WAV sampling/length differs")
    for ch in range(2):
        p1, p2 = max(map(abs, c1[ch])), max(map(abs, c2[ch]))
        mid, end = max(4096, r1//5), len(c1[ch])-max(2048, r1//12)
        a, b = c1[ch][mid:end], c2[ch][mid:end]
        delta_rms = db_ratio(rms(a), rms(b))
        delta_peak = db_ratio(p1, p2)
        corr, shift = max_lag_corr(c1[ch], c2[ch], r1)
        print(f"channel {ch+1}: RMS diff {delta_rms:+.6f} dB, "
              f"peak diff {delta_peak:+.6f} dB, "
              f"max aligned corr {corr:.6f} (lag {shift} samples)")
        assert abs(delta_rms) < 0.10, (
            f"DEFAULT changes channel {ch+1} RMS level")
        assert abs(delta_peak) < 0.15, (
            f"DEFAULT changes channel {ch+1} peak level")
        assert corr > 0.92, (
            f"DEFAULT changes channel {ch+1} waveform excessively")
    print("PASS: VST3 DEFAULT vs native host bypass neutral level, peak and waveform")


if __name__ == "__main__":
    main()
