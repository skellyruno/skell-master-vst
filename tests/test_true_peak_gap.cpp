// Portable release-gate diagnostic: sample-peak safety != true-peak safety.
// This test intentionally demonstrates the existing protection gap. It must
// remain a separate regression when a future true-peak limiter is designed.
// No audio algorithm or user-visible parameter is changed by this test.
#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    using namespace skelly;
    constexpr double sampleRate = 48000.0;
    const double ceiling = detail::dbToLinear(-1.0);
    StereoPeakGuard sampleGuard;
    StereoTruePeakMonitor truePeak;
    sampleGuard.prepare(sampleRate);
    truePeak.reset();

    // 12 kHz at 48 kHz, 45-degree starting phase: the discrete samples are
    // only +/-0.7778, while the reconstructed waveform exceeds 0 dBFS.
    // The linked sample guard never sees a sample over the -1 dBFS ceiling.
    // Guarded output retains opposite-polarity stereo channel relationships.
    double maximumSample = 0.0;
    double maximumReconstructed = 0.0;
    double maximumRatioError = 0.0;
    for (int i = 0; i < 4800; ++i) {
        const float x = static_cast<float>(
            1.1 * std::sin(2.0 * detail::pi * 12000.0 * i / sampleRate + detail::pi / 4.0));
        const StereoFrame out = sampleGuard.process({x, -x * 0.4f}, ceiling);
        require(std::isfinite(out.left) && std::isfinite(out.right),
                "sample guard generated non-finite output");
        maximumSample = std::max(maximumSample,
            static_cast<double>(std::max(std::abs(out.left), std::abs(out.right))));
        if (i > 128) {
            maximumReconstructed = std::max(maximumReconstructed,
                static_cast<double>(truePeak.process(out.left, out.right)));
            maximumRatioError = std::max(maximumRatioError,
                std::abs(static_cast<double>(out.right) + 0.4 * out.left));
        } else {
            truePeak.process(out.left, out.right);
        }
    }
    require(maximumSample <= ceiling + 1.0e-6,
            "sample-peak guard exceeded sample ceiling");
    require(maximumRatioError < 1.0e-6,
            "sample-peak guard altered stereo polarity or balance");
    require(maximumReconstructed > ceiling + 0.04,
            "expected true-peak overshoot fixture is not measuring correctly");

    std::cout << "PASS: documented sample-vs-true-peak limitation; ceiling "
              << detail::linearToDb(ceiling)
              << " dBFS; sample peak " << detail::linearToDb(maximumSample)
              << " dBFS; reconstructed peak " << detail::linearToDb(maximumReconstructed)
              << " dBTP; excess "
              << detail::linearToDb(maximumReconstructed / ceiling)
              << " dB\n";

    // Below-ceiling material is untouched, and the TP meter must remain
    // finite through a long, ordinary lower-frequency stereo program.
    sampleGuard.reset();
    truePeak.reset();
    double cleanError = 0.0, cleanTp = 0.0;
    for (int i = 0; i < 48000; ++i) {
        const float x = static_cast<float>(0.12 *
            std::sin(2.0 * detail::pi * 997.0 * i / sampleRate));
        const auto out = sampleGuard.process({x, x * 0.75f}, ceiling);
        cleanError = std::max(cleanError, std::abs(static_cast<double>(out.left) - x));
        cleanTp = std::max(cleanTp, static_cast<double>(truePeak.process(out.left, out.right)));
    }
    require(cleanError < 1.0e-7, "sample guard changed low-level clean audio");
    require(cleanTp < ceiling && std::isfinite(cleanTp),
            "true-peak meter reported invalid low-level program");
    std::cout << "PASS: clean-program transparency and finite true-peak telemetry\n";
}
