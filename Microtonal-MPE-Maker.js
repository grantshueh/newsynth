/* ============================================================================
   Microtonal MPE Maker
   Logic Pro MIDI FX (Scripter) plug-in

   What it does
   ------------
   Maps your 12-TET keyboard onto any equal division of any period
   (EDO / EDx) and outputs the result as MPE: each note gets its own MIDI
   channel (2-16) with a pitch bend that puts it exactly on the microtonal
   grid. Works with Serum, Vital, and any MPE synth.

   Mapping: each key = one scale step. The Reference Key keeps its normal
   12-TET pitch; every key above/below it moves by (Period / Divisions)
   cents per key. Examples:
     - 24 divisions of 1200 ct  -> quarter tones
     - 19 divisions of 1200 ct  -> 19-EDO
     - 31 divisions of 1200 ct  -> 31-EDO
     - 13 divisions of 1902 ct  -> Bohlen-Pierce
     - 12 divisions of 1200 ct  -> plain 12-TET (bypass)

   IMPORTANT: the "Pitch Bend Range" parameter MUST match the pitch bend
   range in the synth (Serum MPE default is 48 semitones). Far away from
   the Reference Key the required bend grows; notes that would need more
   bend than the range allows are clamped (a Trace warning is printed).

   Install
   -------
   Logic Pro > instrument track > MIDI FX slot > Scripter >
   "Open Script Editor" > select all, paste this file, press "Run Script",
   then save it as a Scripter preset from the plug-in header.
   ============================================================================ */

var PluginParameters = [
    { name: "Divisions (steps per period)", type: "lin", minValue: 1, maxValue: 96,
      numberOfSteps: 95, defaultValue: 24 },
    { name: "Period", type: "lin", minValue: 300, maxValue: 2400,
      numberOfSteps: 2100, defaultValue: 1200, unit: "ct" },
    { name: "Reference Key", type: "lin", minValue: 0, maxValue: 127,
      numberOfSteps: 127, defaultValue: 60 },
    { name: "Reference Offset", type: "lin", minValue: -100, maxValue: 100,
      numberOfSteps: 400, defaultValue: 0, unit: "ct" },
    { name: "Pitch Bend Range", type: "lin", minValue: 1, maxValue: 96,
      numberOfSteps: 95, defaultValue: 48, unit: "st" },
    { name: "Glide Time", type: "lin", minValue: 0, maxValue: 2000,
      numberOfSteps: 200, defaultValue: 0, unit: "ms" },
    { name: "Debug Trace", type: "checkbox", defaultValue: 0 }
];

// --------------------------------- state ------------------------------------

var held = [];              // [{pitch, inCh, outCh}]
var busy = {};              // outCh -> true
var lastBend = {};          // outCh -> last bend value
var chanQuietUntil = {};    // outCh -> ms timestamp
var rr = 0;

function now() {
    return (typeof Date !== "undefined" && Date.now) ? Date.now() : 0;
}

function p(name) { return GetParameter(name); }

function clampBend(v) { return Math.max(-8192, Math.min(8191, Math.round(v))); }

function dbg(msg) { if (p("Debug Trace")) Trace("[Microtonal MPE] " + msg); }

// ------------------------------ tuning math ----------------------------------

// Deviation from the key's own 12-TET pitch, in cents.
function deviationForPitch(pitch) {
    var div = Math.max(1, Math.round(p("Divisions (steps per period)")));
    var period = p("Period");
    var ref = Math.round(p("Reference Key"));
    var steps = pitch - ref;
    var targetCents = steps * (period / div);
    return targetCents - steps * 100 + p("Reference Offset");
}

function sendBendCents(ch, cents, allowGlide) {
    var range = Math.max(1, Math.round(p("Pitch Bend Range")));
    var maxCents = range * 100;
    if (cents > maxCents || cents < -maxCents) {
        Trace("[Microtonal MPE] note needs " + Math.round(cents) +
              " ct but bend range only covers ±" + maxCents +
              " ct — raise Pitch Bend Range (and match it in the synth).");
    }
    var target = clampBend(cents / maxCents * 8192);
    var from = lastBend[ch] || 0;
    if (target === from && allowGlide) return;

    var glide = allowGlide ? Math.round(p("Glide Time")) : 0;
    if (glide < 20) {
        var pb = new PitchBend;
        pb.channel = ch;
        pb.value = target;
        pb.send();
    } else {
        var steps = Math.min(120, Math.max(2, Math.round(glide / 15)));
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

function retuneAll() {
    for (var i = 0; i < held.length; i++)
        sendBendCents(held[i].outCh, deviationForPitch(held[i].pitch), true);
}

// ---------------------------- channel allocation -----------------------------

function allocChannel() {
    var k, ch;
    for (k = 0; k < 15; k++) {
        ch = 2 + ((rr + k) % 15);
        if (!busy[ch] && (chanQuietUntil[ch] || 0) <= now()) {
            rr = (rr + k + 1) % 15; busy[ch] = true; return ch;
        }
    }
    for (k = 0; k < 15; k++) {
        ch = 2 + ((rr + k) % 15);
        if (!busy[ch]) { rr = (rr + k + 1) % 15; busy[ch] = true; return ch; }
    }
    ch = 2 + (rr % 15); rr = (rr + 1) % 15;
    return ch;
}

// -------------------------------- MIDI I/O -----------------------------------

function HandleMIDI(e) {
    var isNoteOn = (e instanceof NoteOn) && e.velocity > 0;
    var isNoteOff = (e instanceof NoteOff) || ((e instanceof NoteOn) && e.velocity === 0);

    if (isNoteOn) {
        var rec = { pitch: e.pitch, inCh: e.channel, outCh: allocChannel() };
        held.push(rec);
        sendBendCents(rec.outCh, deviationForPitch(rec.pitch), false);
        var on = new NoteOn(e);
        on.channel = rec.outCh;
        on.send();
        dbg("note " + e.pitch + " -> ch " + rec.outCh + ", " +
            Math.round(deviationForPitch(e.pitch)) + " ct");

    } else if (isNoteOff) {
        var idx = -1;
        for (var i = 0; i < held.length; i++)
            if (held[i].pitch === e.pitch && held[i].inCh === e.channel) { idx = i; break; }
        if (idx === -1) { e.send(); return; }
        var gone = held[idx];
        held.splice(idx, 1);
        var off = new NoteOff(e);
        off.channel = gone.outCh;
        off.send();
        busy[gone.outCh] = false;

    } else if (e instanceof ControlChange && (e.number === 123 || e.number === 120)) {
        panic();
        e.send();

    } else {
        e.send();   // pitch bend stays on ch 1 (MPE master = global bend), CCs pass
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
    retuneAll();    // tuning parameters are live and automatable
}

function Reset() {
    panic();
    lastBend = {};
    chanQuietUntil = {};
    rr = 0;
}
