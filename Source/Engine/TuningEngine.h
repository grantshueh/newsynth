#pragma once

// TuningEngine — dynamic just-intonation tuning, pure C++ (no JUCE) so it can
// be unit-tested headlessly. Tracks held notes, detects the root (lowest,
// highest, or most-played pitch class, gated by a notes-to-trigger threshold,
// or manually overridden) and maps every MIDI note to a frequency using JI
// ratios configured for each of the 12 chromatic degrees above the root.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace newsynth
{

struct Ratio
{
    int num = 1, den = 1;
    double value() const noexcept { return (double) num / (double) den; }
};

class TuningEngine
{
public:
    enum class RootSource { automatic = 0, manual };
    enum class DetectMode { outermost = 0, mostPlayed };   // outermost = lowest held note, or highest when "note from top"

    static const std::array<Ratio, 12>& defaultRatios()
    {
        static const std::array<Ratio, 12> r { { { 1, 1 },  { 16, 15 }, { 9, 8 },  { 6, 5 },
                                                 { 5, 4 },  { 4, 3 },   { 45, 32 }, { 3, 2 },
                                                 { 8, 5 },  { 5, 3 },   { 7, 4 },  { 15, 8 } } };
        return r;
    }

    TuningEngine()
    {
        ratios = defaultRatios();
        held.reserve (32);
    }

    void reset()
    {
        held.clear();
        rootPc = (rootSource == RootSource::manual) ? manualRoot : 0;
    }

    // --- configuration ------------------------------------------------------
    void setRatio (int degree, int num, int den)
    {
        ratios[(size_t) degree].num = std::max (1, num);
        ratios[(size_t) degree].den = std::max (1, den);
    }
    Ratio getRatio (int degree) const             { return ratios[(size_t) degree]; }

    void setCents (int degree, double cents)      { centOffsets[(size_t) degree] = cents; }
    double getCents (int degree) const            { return centOffsets[(size_t) degree]; }

    void setRootSource (RootSource s)             { if (rootSource != s) { rootSource = s; refreshRoot(); } }
    void setDetectMode (DetectMode m)             { if (detectMode != m) { detectMode = m; refreshRoot(); } }
    void setNoteFromTop (bool b)                  { if (fromTop != b)    { fromTop = b;    refreshRoot(); } }
    void setManualRoot (int pc)                   { pc = ((pc % 12) + 12) % 12; if (manualRoot != pc) { manualRoot = pc; refreshRoot(); } }
    void setNotesToTrigger (int n)                { notesToTrigger = std::max (1, n); }

    // --- note tracking ------------------------------------------------------
    void noteOn (int note)
    {
        held.push_back (note);
        refreshRoot();
    }

    void noteOff (int note)
    {
        auto it = std::find (held.begin(), held.end(), note);
        if (it != held.end())
            held.erase (it);
        // the root is deliberately kept on note-off: it only moves when a new
        // chord of at least notesToTrigger notes establishes a new root.
    }

    int heldCount() const noexcept  { return (int) held.size(); }
    int getRootPc() const noexcept  { return rootPc; }

    // --- frequency mapping --------------------------------------------------
    // The root pitch class is anchored to its 12-TET frequency (around middle
    // C); every other note is that frequency times the JI ratio of its degree
    // above the root, shifted by whole octaves and the per-degree cent offset.
    double frequencyForNote (int note) const
    {
        const int anchor = 60 + rootPc;
        const int diff   = note - anchor;
        const int oct    = (int) std::floor ((double) diff / 12.0);
        const int deg    = diff - oct * 12;   // 0..11
        const double anchorHz = 440.0 * std::exp2 (((double) anchor - 69.0) / 12.0);
        return anchorHz * ratios[(size_t) deg].value()
                        * std::exp2 (centOffsets[(size_t) deg] / 1200.0)
                        * std::exp2 ((double) oct);
    }

private:
    void refreshRoot()
    {
        if (rootSource == RootSource::manual)
        {
            rootPc = manualRoot;
            return;
        }

        if (held.empty() || (int) held.size() < notesToTrigger)
            return;   // not enough notes to establish a new root — keep the old one

        if (detectMode == DetectMode::mostPlayed)
        {
            int counts[12] = {};
            for (int n : held)
                ++counts[((n % 12) + 12) % 12];

            int bestPc = -1, bestCount = 0;
            int bestExtreme = fromTop ? -1 : 1 << 30;
            for (int n : held)
            {
                const int pc = ((n % 12) + 12) % 12;
                const bool better = counts[pc] > bestCount
                                 || (counts[pc] == bestCount
                                     && (fromTop ? n > bestExtreme : n < bestExtreme));
                if (better)
                {
                    bestPc = pc;
                    bestCount = counts[pc];
                    bestExtreme = n;
                }
            }
            if (bestPc >= 0)
                rootPc = bestPc;
        }
        else
        {
            int extreme = held.front();
            for (int n : held)
                extreme = fromTop ? std::max (extreme, n) : std::min (extreme, n);
            rootPc = ((extreme % 12) + 12) % 12;
        }
    }

    std::array<Ratio, 12> ratios;
    std::array<double, 12> centOffsets {};
    std::vector<int> held;
    RootSource rootSource = RootSource::automatic;
    DetectMode detectMode = DetectMode::outermost;
    bool fromTop = false;
    int manualRoot = 0;
    int notesToTrigger = 1;
    int rootPc = 0;
};

} // namespace newsynth
