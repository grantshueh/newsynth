// Headless unit tests for the JI TuningEngine — pure C++, no JUCE required.
// Build: part of the CMake project (target TuningTests), or directly:
//   clang++ -std=c++17 -I Source Tests/TuningTests.cpp -o tuning_tests && ./tuning_tests

#include "Engine/TuningEngine.h"

#include <cstdio>
#include <cmath>

using newsynth::TuningEngine;

static int failures = 0;

static void expectNear (const char* what, double actual, double expected, double relTol = 1.0e-6)
{
    const double err = std::abs (actual - expected) / std::max (1.0e-12, std::abs (expected));
    if (err > relTol)
    {
        std::printf ("FAIL  %-50s expected %.6f, got %.6f\n", what, expected, actual);
        ++failures;
    }
    else
        std::printf ("pass  %-50s %.4f Hz\n", what, actual);
}

static void expectEq (const char* what, int actual, int expected)
{
    if (actual != expected)
    {
        std::printf ("FAIL  %-50s expected %d, got %d\n", what, expected, actual);
        ++failures;
    }
    else
        std::printf ("pass  %-50s %d\n", what, actual);
}

static double etHz (int note) { return 440.0 * std::exp2 ((note - 69) / 12.0); }

int main()
{
    std::printf ("NewSynth TuningEngine tests\n===========================\n");

    // --- 1. root note itself matches 12-TET ---------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (1);
        t.noteOn (60);
        expectEq   ("single C4 sets root C", t.getRootPc(), 0);
        expectNear ("root C4 anchored to 12-TET", t.frequencyForNote (60), etHz (60));
    }

    // --- 2. C major chord in 5-limit JI --------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (3);
        t.noteOn (60); t.noteOn (64); t.noteOn (67);
        expectEq   ("C-E-G root is C", t.getRootPc(), 0);
        expectNear ("E4 = 5/4 above C4", t.frequencyForNote (64), etHz (60) * 5.0 / 4.0);
        expectNear ("G4 = 3/2 above C4", t.frequencyForNote (67), etHz (60) * 3.0 / 2.0);
        expectNear ("C5 = exactly one octave", t.frequencyForNote (72), etHz (60) * 2.0);
        expectNear ("Bb4 = 7/4 above C4 (harmonic 7th)", t.frequencyForNote (70), etHz (60) * 7.0 / 4.0);
        expectNear ("A3 = 5/3 below the octave", t.frequencyForNote (57), etHz (60) * 5.0 / 3.0 / 2.0);
    }

    // --- 3. root moves with a new chord ---------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (3);
        t.noteOn (60); t.noteOn (64); t.noteOn (67);
        t.noteOff (60); t.noteOff (64); t.noteOff (67);
        expectEq ("root kept after release", t.getRootPc(), 0);
        t.noteOn (65); t.noteOn (69); t.noteOn (72);
        expectEq   ("F-A-C root is F", t.getRootPc(), 5);
        expectNear ("A4 = 5/4 above F4", t.frequencyForNote (69), etHz (65) * 5.0 / 4.0);
        expectNear ("C5 = 3/2 above F4", t.frequencyForNote (72), etHz (65) * 3.0 / 2.0);
    }

    // --- 4. notes-to-trigger threshold ----------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (3);
        t.noteOn (60); t.noteOn (64); t.noteOn (67);   // establish C
        t.noteOff (60); t.noteOff (64); t.noteOff (67);
        t.noteOn (62); t.noteOn (66);                   // only two notes: below threshold
        expectEq ("2 notes < threshold keeps root C", t.getRootPc(), 0);
        t.noteOn (69);                                  // third note arrives
        expectEq ("3rd note triggers new root D", t.getRootPc(), 2);
    }

    // --- 5. note-from-top -------------------------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (3);
        t.setNoteFromTop (true);
        t.noteOn (60); t.noteOn (64); t.noteOn (67);
        expectEq   ("C-E-G from top gives root G", t.getRootPc(), 7);
        expectNear ("G4 anchored to 12-TET", t.frequencyForNote (67), etHz (67));
        expectNear ("C4 = 4/3 above G3", t.frequencyForNote (60), etHz (67) * 4.0 / 3.0 / 2.0);
    }

    // --- 6. most-played detection ------------------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (1);
        t.setDetectMode (TuningEngine::DetectMode::mostPlayed);
        t.noteOn (64);                 // E
        expectEq ("first note E is root", t.getRootPc(), 4);
        t.noteOn (60); t.noteOn (72);  // two Cs
        expectEq ("doubled C outvotes E", t.getRootPc(), 0);
    }

    // --- 7. manual root override ---------------------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (1);
        t.noteOn (60);
        t.setRootSource (TuningEngine::RootSource::manual);
        t.setManualRoot (2);           // D
        expectEq   ("manual root D wins", t.getRootPc(), 2);
        expectNear ("D4 anchored to 12-TET", t.frequencyForNote (62), etHz (62));
        expectNear ("F#4 = 5/4 above D4", t.frequencyForNote (66), etHz (62) * 5.0 / 4.0);
        t.noteOn (65); t.noteOn (69);
        expectEq ("new notes don't move a manual root", t.getRootPc(), 2);
    }

    // --- 8. per-degree cent offsets --------------------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (1);
        t.noteOn (60);
        t.setCents (7, 10.0);          // raise the fifth 10 cents
        expectNear ("fifth raised by 10 cents", t.frequencyForNote (67),
                    etHz (60) * 1.5 * std::exp2 (10.0 / 1200.0));
    }

    // --- 9. custom ratio -----------------------------------------------------------
    {
        TuningEngine t;
        t.setNotesToTrigger (1);
        t.noteOn (60);
        t.setRatio (6, 7, 5);          // septimal tritone
        expectNear ("tritone retuned to 7/5", t.frequencyForNote (66), etHz (60) * 7.0 / 5.0);
    }

    std::printf ("===========================\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
