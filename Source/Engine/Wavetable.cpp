#include "Wavetable.h"
#include <cmath>

namespace newsynth
{

void WavetableSet::build (const std::vector<std::vector<float>>& rawFrames)
{
    numFrames = (int) rawFrames.size();
    data.assign ((size_t) numFrames * (size_t) numMips * (size_t) stride, 0.0f);

    juce::dsp::FFT fft (11);   // 2^11 = 2048
    std::vector<float> spectrum ((size_t) tableSize * 2, 0.0f);
    std::vector<float> work ((size_t) tableSize * 2, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        const auto& src = rawFrames[(size_t) f];
        std::copy (src.begin(), src.end(), spectrum.begin());
        fft.performRealOnlyForwardTransform (spectrum.data());

        for (int m = 0; m < numMips; ++m)
        {
            const int maxHarm = std::max (1, 1024 >> m);
            std::copy (spectrum.begin(), spectrum.end(), work.begin());

            // zero DC and every bin above this mip's harmonic limit, in both
            // halves of the conjugate-symmetric spectrum
            work[0] = work[1] = 0.0f;
            for (int k = maxHarm + 1; k <= tableSize - maxHarm - 1; ++k)
            {
                work[(size_t) (2 * k)]     = 0.0f;
                work[(size_t) (2 * k + 1)] = 0.0f;
            }

            fft.performRealOnlyInverseTransform (work.data());

            float* dst = data.data() + ((size_t) f * (size_t) numMips + (size_t) m) * (size_t) stride;
            for (int i = 0; i < tableSize; ++i)
                dst[i] = work[(size_t) i];
            dst[tableSize] = dst[0];
        }
    }

    // normalise the whole set so the loudest full-band frame peaks at +-1
    float peak = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        const float* t = table (f, 0);
        for (int i = 0; i < tableSize; ++i)
            peak = std::max (peak, std::abs (t[i]));
    }
    if (peak > 1.0e-6f)
        for (auto& v : data)
            v /= peak;
}

std::unique_ptr<WavetableSet> WavetableSet::createDefault()
{
    constexpr int frames = 16;
    const double twoPi = 6.283185307179586;

    auto shapeSample = [twoPi] (int shape, double ph) -> double
    {
        switch (shape)
        {
            case 0:  return std::sin (twoPi * ph);
            case 1:  return 1.0 - 4.0 * std::abs (ph - 0.5);   // triangle
            case 2:  return 2.0 * ph - 1.0;                    // saw
            default: return ph < 0.5 ? 1.0 : -1.0;             // square
        }
    };

    std::vector<std::vector<float>> raw ((size_t) frames, std::vector<float> ((size_t) tableSize));
    for (int f = 0; f < frames; ++f)
    {
        const double morph = (double) f / (double) (frames - 1) * 3.0;   // 0..3 across 4 shapes
        const int s0 = std::min (2, (int) morph);
        const double t = morph - (double) s0;
        for (int i = 0; i < tableSize; ++i)
        {
            const double ph = (double) i / (double) tableSize;
            const double v  = (1.0 - t) * shapeSample (s0, ph) + t * shapeSample (s0 + 1, ph);
            raw[(size_t) f][(size_t) i] = (float) v;
        }
    }

    auto set = std::make_unique<WavetableSet>();
    set->build (raw);
    return set;
}

std::unique_ptr<WavetableSet> WavetableSet::fromAudioData (const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples <= 0)
        return createDefault();

    int frames = numSamples / tableSize;
    frames = std::max (1, std::min (frames, maxFrames));

    std::vector<std::vector<float>> raw ((size_t) frames, std::vector<float> ((size_t) tableSize, 0.0f));
    for (int f = 0; f < frames; ++f)
    {
        const int start = f * tableSize;
        const int count = std::min (tableSize, numSamples - start);
        for (int i = 0; i < count; ++i)
            raw[(size_t) f][(size_t) i] = samples[start + i];
    }

    auto set = std::make_unique<WavetableSet>();
    set->build (raw);
    return set;
}

} // namespace newsynth
