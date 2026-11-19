// End-to-end headless render test: plays notes through the full SynthEngine
// (voices + wavetable oscillators + JI tuning) and measures the rendered
// frequency by zero-crossing analysis on the sine frame.

#include "Engine/SynthEngine.h"
#include <cstdio>
#include <vector>

using namespace newsynth;

static int failures = 0;

static double measureFreq (const float* buf, int n, double sr)
{
    int first = -1, last = -1, count = 0;
    for (int i = 1; i < n; ++i)
    {
        if (buf[i - 1] <= 0.0f && buf[i] > 0.0f)
        {
            if (first < 0) first = i;
            last = i;
            ++count;
        }
    }
    if (count < 2 || last <= first)
        return 0.0;
    return (double) (count - 1) * sr / (double) (last - first);
}

static void expectFreq (const char* what, double actual, double expected)
{
    const double err = std::abs (actual - expected) / expected;
    if (err > 0.002)
    {
        std::printf ("FAIL  %-44s expected %.3f Hz, got %.3f Hz\n", what, expected, actual);
        ++failures;
    }
    else
        std::printf ("pass  %-44s %.3f Hz (expected %.3f)\n", what, actual, expected);
}

static double etHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

int main()
{
    const double sr = 48000.0;

    SynthEngine engine;
    engine.prepare (sr);
    std::shared_ptr<const WavetableSet> wt (WavetableSet::createDefault().release());
    for (int i = 0; i < numOscs; ++i)
        engine.setWavetable (i, wt);

    EngineParams p;
    p.osc[0].on = true;
    p.osc[0].position = 0.0f;   // frame 0 = pure sine
    p.osc[0].unison = 1;
    p.osc[0].level = 1.0f;
    p.attack = 0.001f;
    p.decay = 0.01f;
    p.sustain = 1.0f;
    p.release = 0.03f;
    p.glideMs = 0.0f;
    engine.setParams (p);
    engine.tuning.setNotesToTrigger (3);

    std::vector<float> buf (48000, 0.0f);
    auto renderInto = [&] (int n) { std::fill (buf.begin(), buf.end(), 0.0f); engine.render (buf.data(), n); };

    // --- single note before any chord: anchored to ET of default root C ------
    engine.noteOn (60, 0.8f);
    renderInto (48000);
    expectFreq ("solo C4 at 12-TET", measureFreq (buf.data() + 2400, 45000, sr), etHz (60));
    engine.noteOff (60);
    renderInto (9600);   // let the release die

    // --- establish root C with a chord, release it ----------------------------
    engine.noteOn (60, 0.8f);
    engine.noteOn (64, 0.8f);
    engine.noteOn (67, 0.8f);
    renderInto (24000);
    engine.noteOff (60);
    engine.noteOff (64);
    engine.noteOff (67);
    renderInto (9600);

    // --- a lone E4 must still be tuned 5/4 above the retained root C ----------
    engine.noteOn (64, 0.8f);
    renderInto (48000);
    expectFreq ("E4 over retained root C = 5/4", measureFreq (buf.data() + 2400, 45000, sr), etHz (60) * 1.25);
    engine.noteOff (64);
    renderInto (9600);

    // --- lone G4 = pure fifth over root C --------------------------------------
    engine.noteOn (67, 0.8f);
    renderInto (48000);
    expectFreq ("G4 over retained root C = 3/2", measureFreq (buf.data() + 2400, 45000, sr), etHz (60) * 1.5);
    engine.noteOff (67);
    renderInto (9600);

    // --- new chord moves the root; lone A4 is then 5/4 over F -------------------
    engine.noteOn (65, 0.8f);
    engine.noteOn (69, 0.8f);
    engine.noteOn (72, 0.8f);
    renderInto (24000);
    engine.noteOff (65);
    engine.noteOff (69);
    engine.noteOff (72);
    renderInto (9600);
    engine.noteOn (69, 0.8f);
    renderInto (48000);
    expectFreq ("A4 over new root F = 5/4", measureFreq (buf.data() + 2400, 45000, sr), etHz (65) * 1.25);
    engine.noteOff (69);

    std::printf ("%s (%d failure%s)\n",
                 failures == 0 ? "RENDER TEST PASSED" : "RENDER TEST FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
