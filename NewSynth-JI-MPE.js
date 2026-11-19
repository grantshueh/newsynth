/* ============================================================================
   NewSynth JI — Dynamic Just-Intonation MPE Retuner
   Logic Pro MIDI FX (Scripter) plug-in

   What it does
   ------------
   Listens to the chords you play on a normal 12-TET keyboard, detects the
   root note (lowest or most-played, or a manual root), and retunes every
   note of the chord to a just-intonation ratio relative to that root by
   sending per-note pitch bends on separate MIDI channels (MPE).

   Use it in front of any MPE synth: Serum, Vital, Surge, Pigments, etc.

   IMPORTANT: the "Pitch Bend Range" parameter here MUST match the pitch
   bend range set in the synth (Serum MPE default is 48 semitones).

   Modes
   -----
   - Tuning presets: 5-Limit JI (default, same ratios as the Alobend UI:
     1/1 16/15 9/8 6/5 5/4 4/3 45/32 3/2 8/5 5/3 7/4 15/8), 7-Limit JI,
     Pythagorean, 1/4-Comma Meantone, 12-TET, or your own ratios / cents.
   - "Custom Ratios": uses the 24 Num/Den sliders (= Alobend Ratio mode).
   - "Custom Cents": uses the 12 Cents sliders as deviation from 12-TET
     (= Alobend Detune mode).
   - The Cents sliders are ALWAYS added on top of ratio/preset tunings as
     fine per-degree detune, and they are automatable.

   Install
   -------
   Logic Pro > instrument track > MIDI FX slot > Scripter >
   "Open Script Editor" > select all, paste this file, press "Run Script",
   then save it as a Scripter preset from the plug-in header.
   ============================================================================ */

// ------------------------------- tuning data --------------------------------

var NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
var INTERVAL_NAMES = ["Unison", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"];

var DEFAULT_RATIOS = [
    [1, 1], [16, 15], [9, 8], [6, 5], [5, 4], [4, 3],
    [45, 32], [3, 2], [8, 5], [5, 3], [7, 4], [15, 8]
];

function ratioCentsTable(ratios) {
    var t = [];
    for (var i = 0; i < 12; i++)
        t.push(1200 * Math.log(ratios[i][0] / ratios[i][1]) / Math.LN2);
    return t;
}

var PRESET_TABLES = {
    "5-Limit JI": ratioCentsTable(DEFAULT_RATIOS),
    "7-Limit JI": ratioCentsTable([
        [1, 1], [16, 15], [8, 7], [6, 5], [5, 4], [4, 3],
        [7, 5], [3, 2], [8, 5], [5, 3], [7, 4], [15, 8]
    ]),
    "Pythagorean": ratioCentsTable([
        [1, 1], [256, 243], [9, 8], [32, 27], [81, 64], [4, 3],
        [729, 512], [3, 2], [128, 81], [27, 16], [16, 9], [243, 128]
    ]),
    "1/4-Comma Meantone": [0, 76.05, 193.16, 310.26, 386.31, 503.42,
                           579.47, 696.58, 772.63, 889.74, 1006.84, 1082.89],
    "12-TET": [0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100]
};

var TUNING_SOURCES = ["5-Limit JI", "7-Limit JI", "Pythagorean",
                      "1/4-Comma Meantone", "12-TET", "Custom Ratios", "Custom Cents"];

// ------------------------------- parameters ---------------------------------

var NUM_NAMES = [], DEN_NAMES = [], CENT_NAMES = [];

var PluginParameters = [
    { name: "Output", type: "menu",
      valueStrings: ["MPE (per-note bend, ch 2-16)", "Single Channel (bend whole chord)"],
      defaultValue: 0 },
    { name: "Pitch Bend Range", type: "lin", minValue: 1, maxValue: 96,
      numberOfSteps: 95, defaultValue: 48, unit: "st" },
    { name: "Tuning", type: "menu", valueStrings: TUNING_SOURCES, defaultValue: 0 },
    { name: "Auto Root Note", type: "checkbox", defaultValue: 1 },
    { name: "Root Detection", type: "menu",
      valueStrings: ["Lowest Note", "Most Played"], defaultValue: 1 },
    { name: "Notes To Trigger", type: "lin", minValue: 1, maxValue: 8,
      numberOfSteps: 7, defaultValue: 2 },
    { name: "Root Note (manual)", type: "menu", valueStrings: NOTE_NAMES, defaultValue: 0 },
    { name: "Single Note Pitchbend", type: "checkbox", defaultValue: 0 },
    { name: "Note From Top", type: "checkbox", defaultValue: 1 },
    { name: "Note Index", type: "lin", minValue: 1, maxValue: 8,
      numberOfSteps: 7, defaultValue: 1 },
    { name: "Glide Time", type: "lin", minValue: 0, maxValue: 2000,
      numberOfSteps: 200, defaultValue: 0, unit: "ms" },
    { name: "Glide New Notes", type: "checkbox", defaultValue: 0 },
    { name: "Debug Trace", type: "checkbox", defaultValue: 0 }
];

(function buildTuningParams() {
    var i;
    for (i = 0; i < 12; i++) {
        var nn = "Num " + i + " (" + INTERVAL_NAMES[i] + ")";
        var dn = "Den " + i + " (" + INTERVAL_NAMES[i] + ")";
        NUM_NAMES.push(nn);
        DEN_NAMES.push(dn);
        PluginParameters.push({ name: nn, type: "lin", minValue: 1, maxValue: 999,
                                numberOfSteps: 998, defaultValue: DEFAULT_RATIOS[i][0] });
        PluginParameters.push({ name: dn, type: "lin", minValue: 1, maxValue: 999,
                                numberOfSteps: 998, defaultValue: DEFAULT_RATIOS[i][1] });
    }
    for (i = 0; i < 12; i++) {
        var cn = "Cents " + i + " (" + INTERVAL_NAMES[i] + ")";
        CENT_NAMES.push(cn);
        PluginParameters.push({ name: cn, type: "lin", minValue: -100, maxValue: 100,
                                numberOfSteps: 400, defaultValue: 0, unit: "ct" });
    }
})();

// --------------------------------- state ------------------------------------

var held = [];                    // [{pitch, inCh, outCh}] in order of arrival
var busy = {};                    // outCh -> true while a note sounds on it
var lastBend = {};                // outCh -> last pitch bend value sent
var chanQuietUntil = {};          // outCh -> ms timestamp until which glides are pending
var rr = 0;                       // round-robin pointer over channels 2..16
var root = 0;                     // current root pitch class 0..11
var wheel = 0;                    // last incoming pitch bend (single-channel mode)
var chordBend = 0;                // our bend contribution in single-channel mode

function now() {
    return (typeof Date !== "undefined" && Date.now) ? Date.now() : 0;
}

function p(name) { return GetParameter(name); }

function clampBend(v) { return Math.max(-8192, Math.min(8191, Math.round(v))); }

function dbg(msg) { if (p("Debug Trace")) Trace("[NewSynth JI] " + msg); }

// ------------------------------ tuning math ---------------------------------

// Deviation from 12-TET in cents for chromatic degree i (0..11) above the root.
function centsDeviationForDegree(i) {
    var src = TUNING_SOURCES[Math.round(p("Tuning"))];
    var fine = p(CENT_NAMES[i]);
    if (src === "Custom Cents") return fine;
    if (src === "Custom Ratios") {
        var n = Math.max(1, Math.round(p(NUM_NAMES[i])));
        var d = Math.max(1, Math.round(p(DEN_NAMES[i])));
        return 1200 * Math.log(n / d) / Math.LN2 - 100 * i + fine;
    }
    return PRESET_TABLES[src][i] - 100 * i + fine;
}

function deviationForPitch(pitch) {
    var i = ((pitch - root) % 12 + 12) % 12;
    return centsDeviationForDegree(i);
}

// Which held note is "the" note for Single Note Pitchbend / single-channel mode.
function selectedHeld() {
    if (held.length === 0) return null;
    var sorted = held.slice().sort(function (a, b) { return a.pitch - b.pitch; });
    var idx = Math.min(held.length, Math.max(1, Math.round(p("Note Index")))) - 1;
    return p("Note From Top") ? sorted[sorted.length - 1 - idx] : sorted[idx];
}

// ------------------------------ root detection ------------------------------

function computeRoot() {
    if (!p("Auto Root Note")) return Math.round(p("Root Note (manual)"));
    if (held.length === 0) return root;
    if (Math.round(p("Root Detection")) === 0) {
        // Lowest note
        var lowest = held[0];
        for (var i = 1; i < held.length; i++)
            if (held[i].pitch < lowest.pitch) lowest = held[i];
        return lowest.pitch % 12;
    }
    // Most played: the pitch class occurring most often among held notes;
    // ties go to the one containing the lowest note.
    var counts = {}, j;
    for (j = 0; j < held.length; j++) {
        var pc = held[j].pitch % 12;
        counts[pc] = (counts[pc] || 0) + 1;
    }
    var best = -1, bestCount = -1, bestLowest = 128;
    for (j = 0; j < held.length; j++) {
        var pc2 = held[j].pitch % 12;
        var lowestOfPc = 128;
        for (var k = 0; k < held.length; k++)
            if (held[k].pitch % 12 === pc2 && held[k].pitch < lowestOfPc)
                lowestOfPc = held[k].pitch;
        if (counts[pc2] > bestCount ||
            (counts[pc2] === bestCount && lowestOfPc < bestLowest)) {
            best = pc2; bestCount = counts[pc2]; bestLowest = lowestOfPc;
        }
    }
    return best;
}

function maybeRecalcRoot() {
    if (!p("Auto Root Note")) { root = Math.round(p("Root Note (manual)")); return; }
    if (held.length >= Math.round(p("Notes To Trigger"))) {
        var r = computeRoot();
        if (r !== root) {
            root = r;
            dbg("root -> " + NOTE_NAMES[root]);
        }
    }
}

// ------------------------------- bend output ---------------------------------

// Send a pitch bend to reach `cents` deviation on channel ch, optionally gliding.
function sendBendCents(ch, cents, allowGlide) {
    var range = Math.max(1, Math.round(p("Pitch Bend Range")));
    var target = clampBend(cents / (range * 100) * 8192);
    var from = lastBend[ch] || 0;
    if (target === from && allowGlide) return;

    var glide = allowGlide ? Math.round(p("Glide Time")) : 0;
    if (glide < 20) {
        var pb = new PitchBend;
        pb.channel = ch;
        pb.value = target;
        pb.send();
    } else {
        var stepMs = 15;
        var steps = Math.min(120, Math.max(2, Math.round(glide / stepMs)));
        for (var s = 1; s <= steps; s++) {
            var pb2 = new PitchBend;
            pb2.channel = ch;
            pb2.value = clampBend(from + (target - from) * (s / steps));
            pb2.sendAfterMilliseconds(glide * (s / steps));
        }
        chanQuietUntil[ch] = now() + glide + 50;
    }
    lastBend[ch] = target;
}

// Re-send bends for all held notes (after a root change, parameter move, etc.)
function retuneAll(allowGlide) {
    if (held.length === 0) return;
    if (Math.round(p("Output")) === 1) {    // single-channel mode
        var sel = selectedHeld();
        chordBend = sel ? deviationForPitch(sel.pitch) : 0;
        sendMergedSingleChannelBend(sel ? sel.inCh : 1, allowGlide);
        return;
    }
    var single = p("Single Note Pitchbend") ? selectedHeld() : null;
    for (var i = 0; i < held.length; i++) {
        var rec = held[i];
        var cents;
        if (p("Single Note Pitchbend"))
            cents = (rec === single) ? deviationForPitch(rec.pitch) : 0;
        else
            cents = deviationForPitch(rec.pitch);
        sendBendCents(rec.outCh, cents, allowGlide);
    }
}

function sendMergedSingleChannelBend(ch, allowGlide) {
    var range = Math.max(1, Math.round(p("Pitch Bend Range")));
    var chordBendValue = chordBend / (range * 100) * 8192;
    var target = clampBend(wheel + chordBendValue);
    var from = lastBend[ch] || 0;
    var glide = allowGlide ? Math.round(p("Glide Time")) : 0;
    if (glide < 20) {
        var pb = new PitchBend;
        pb.channel = ch;
        pb.value = target;
        pb.send();
    } else {
        var stepMs = 15;
        var steps = Math.min(120, Math.max(2, Math.round(glide / stepMs)));
        for (var s = 1; s <= steps; s++) {
            var pb2 = new PitchBend;
            pb2.channel = ch;
            pb2.value = clampBend(from + (target - from) * (s / steps));
            pb2.sendAfterMilliseconds(glide * (s / steps));
        }
    }
    lastBend[ch] = target;
}

// ---------------------------- channel allocation -----------------------------

function allocChannel() {
    var k, ch;
    // Prefer a free channel with no pending glide ramps.
    for (k = 0; k < 15; k++) {
        ch = 2 + ((rr + k) % 15);
        if (!busy[ch] && (chanQuietUntil[ch] || 0) <= now()) {
            rr = (rr + k + 1) % 15; busy[ch] = true; return ch;
        }
    }
    // Then any free channel.
    for (k = 0; k < 15; k++) {
        ch = 2 + ((rr + k) % 15);
        if (!busy[ch]) { rr = (rr + k + 1) % 15; busy[ch] = true; return ch; }
    }
    // All 15 in use: double up on the round-robin channel.
    ch = 2 + (rr % 15); rr = (rr + 1) % 15;
    return ch;
}

// -------------------------------- MIDI I/O -----------------------------------

function HandleMIDI(e) {
    var mpe = Math.round(p("Output")) === 0;

    var isNoteOn = (e instanceof NoteOn) && e.velocity > 0;
    var isNoteOff = (e instanceof NoteOff) || ((e instanceof NoteOn) && e.velocity === 0);

    if (isNoteOn) {
        var rec = { pitch: e.pitch, inCh: e.channel, outCh: mpe ? allocChannel() : e.channel };
        held.push(rec);
        maybeRecalcRoot();

        if (mpe) {
            // Tune the new note (immediately, or gliding in from 12-TET).
            var cents = deviationForPitch(rec.pitch);
            if (p("Single Note Pitchbend")) {
                var sel = selectedHeld();
                cents = (rec === sel) ? cents : 0;
            }
            if (p("Glide New Notes") && Math.round(p("Glide Time")) >= 20) {
                lastBend[rec.outCh] = 0;
                var pb0 = new PitchBend;
                pb0.channel = rec.outCh;
                pb0.value = 0;
                pb0.send();
                sendBendCents(rec.outCh, cents, true);
            } else {
                sendBendCents(rec.outCh, cents, false);
            }
            var on = new NoteOn(e);
            on.channel = rec.outCh;
            on.send();
            // Root change or Single Note selection change may retune the others.
            retuneAll(true);
        } else {
            e.send();
            retuneAll(true);
        }

    } else if (isNoteOff) {
        var idx = -1;
        for (var i = 0; i < held.length; i++)
            if (held[i].pitch === e.pitch && held[i].inCh === e.channel) { idx = i; break; }
        if (idx === -1) { e.send(); return; }
        var gone = held[idx];
        held.splice(idx, 1);
        if (mpe) {
            var off = new NoteOff(e);
            off.channel = gone.outCh;
            off.send();
            busy[gone.outCh] = false;
            // Selection may move to another note in Single Note mode.
            if (p("Single Note Pitchbend")) retuneAll(true);
        } else {
            e.send();
            retuneAll(true);
        }

    } else if (e instanceof PitchBend) {
        if (mpe) {
            e.send();                 // ch 1 = MPE master channel: global bend
        } else {
            wheel = e.value;
            var sel2 = selectedHeld();
            sendMergedSingleChannelBend(sel2 ? sel2.inCh : e.channel, false);
        }

    } else if (e instanceof ControlChange && (e.number === 123 || e.number === 120)) {
        panic();
        e.send();

    } else {
        e.send();                     // CC / pressure / program changes pass through
    }
}

function panic() {
    for (var i = 0; i < held.length; i++) {
        var off = new NoteOff;
        off.pitch = held[i].pitch;
        off.channel = held[i].outCh;
        off.send();
        busy[held[i].outCh] = false;
    }
    held = [];
}

function ParameterChanged(param, value) {
    if (!p("Auto Root Note")) root = Math.round(p("Root Note (manual)"));
    retuneAll(false);                 // live tuning sliders are automatable
}

function Reset() {
    panic();
    lastBend = {};
    chanQuietUntil = {};
    wheel = 0;
    chordBend = 0;
    rr = 0;
}
