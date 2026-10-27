#!/usr/bin/env python3
"""Builds the NRPN part of map/params.csv from the structure of Novation's
"UltraNova Program Parameter MIDI Assignments" (see docs/MIDI-IMPLEMENTATION.md).
The CC lines already in the file are kept as they are. Run: python3 tools/gen_map.py
Ranges are not printed by the source, so they are marked "assumed" (0..127), except
the Gator levels (0..7, printed) and ClockBPM (14-bit word, printed)."""
import os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
path = os.path.join(root, "map", "params.csv")
rows = []   # (msb, lsb, group, name, kind, lo, hi, src)

def add(msb, lsb, group, name, kind="nrpn", lo=0, hi=127, src="assumed"):
    rows.append((msb, lsb, group, name, kind, lo, hi, src))

ENV_FILT = ["Velocity", "Attack", "Decay", "Sustain Level", "Release", "Sustain Rate", "Sustain Time", "A/D Repeats",
            "Attack Track", "Decay Track", "Level Track", "Attack Slope", "Decay Slope", "Touch Trigger"]
ENV_N = ["Delay"] + ENV_FILT[1:]
for i, n in enumerate(ENV_FILT): add(0, i, "Filter envelope", "FiltEnv " + n)
for e in range(3, 7):
    for i, n in enumerate(ENV_N): add(0, 14 * (e - 2) + i, "Env %d" % e, "Env%d %s" % (e, n))
LFO = ["Waveform", "Phase Offset", "Slew Rate", "Shape", "Delay", "Delay Sync", "Rate", "Rate Sync", "Packed Data (bits 4 Fade, 3 DelayMulti, 2 Common, 1 KeySync, 0 OneShot)"]
for l in range(3):
    for i, n in enumerate(LFO): add(0, 70 + 9 * l + i, "LFO %d" % (l + 1), "Lfo%d %s" % (l + 1, n))
add(0, 97, "FX", "Fx Configuration"); add(0, 98, "FX", "Fx Feedback")
for s in range(5): add(0, 99 + s, "FX", "Fx Slot%d Effect Select" % (s + 1))
for i, n in enumerate(["Bass EQ Frequency", "Bass EQ Level", "Mid EQ Frequency", "Mid EQ Level", "Treble EQ Frequency", "Treble EQ Level"]): add(0, 104 + i, "EQ", n)
CMP = ["Ratio", "Threshold", "Attack", "Release", "Hold", "Auto-Gain"]
for c in range(2):
    for i, n in enumerate(CMP): add(0, 110 + 6 * c + i, "Compressor %d" % (c + 1), "Compress%d %s" % (c + 1, n))
# 0:122 to 0:127 (packed parameter specials) are not exposed: their bit layout is in a Novation document we do not have.

for d in range(2):
    for i, n in enumerate(["Type", "Compensation", "Output Level"]): add(1, 3 * d + i, "Distortion", "Distort%d %s" % (d + 1, n))
for d in range(2):
    for i, n in enumerate(["Time", "Sync Time", "Feedback", "Width", "L/R Ratio", "Slew Rate"]): add(1, 6 + 6 * d + i, "Delay", "Delay%d %s" % (d + 1, n))
for d in range(2):
    for i, n in enumerate(["Type", "Decay", "Damping"]): add(1, 18 + 3 * d + i, "Reverb", "Reverb%d %s" % (d + 1, n))
for d in range(4):
    for i, n in enumerate(["Type", "Rate", "Rate Sync", "Feedback", "Mod Depth", "Delay"]): add(1, 24 + 6 * d + i, "Phaser Chorus", "Phaser/Chorus%d %s" % (d + 1, n))
for i, n in enumerate(["Packed Data (bits 2 Latch, 1 KeySync, 0 On)", "Rate Sync", "Mode", "Level", "Edge Slew", "Decay", "Delay"]): add(1, 48 + i, "Gator", "Gator " + n)
for i, n in enumerate(["Packed Data (bits 1 SibType, 0 On)", "Balance", "Width", "Sibilance", "Spectrum Shift", "Spectrum Magnify"]): add(1, 55 + i, "Vocoder", "Vocoder " + n)
add(1, 61, "Arpeggiator", "Arpeggiator Packed Data (bits 6-5 OutputMode, 2 Latch, 1 KeySync, 0 On)")
for lsb, n in [(62, "Octaves"), (63, "Rate Sync"), (64, "Gate Time"), (65, "Mode"), (66, "Pattern"), (67, "Velocity"), (69, "Sequence"), (70, "Quantise")]: add(1, lsb, "Arpeggiator", "Arpeggiator " + n)
add(1, 78, "Chord", "Chord Transpose")   # 1:79 Chord Spare is not exposed
add(1, 80, "Mod matrix", "ModMatrix Velocity Slew Rate"); add(1, 81, "Mod matrix", "ModMatrix Touch Slew Rate"); add(1, 82, "Mod matrix", "ModMatrix Select")
MM = ["Source1", "Source2", "Touch Enable", "Depth", "Destination"]
for k in range(1, 21):
    base = 83 + 5 * (k - 1) if k <= 9 else 5 * (k - 10)
    msb = 1 if k <= 9 else 2
    for i, n in enumerate(MM): add(msb, base + i, "Mod matrix", "ModMatrix%d %s" % (k, n))
add(2, 63, "Clock", "ClockBPM (14-bit)", kind="nrpn14", lo=0, hi=16383, src="manual")
for i in range(16): add(5, i, "Gator levels", "Gator Level L%d" % (i + 1), hi=7, src="manual")
for i in range(16): add(5, 16 + i, "Gator levels", "Gator Level R%d" % (i + 1), hi=7, src="manual")
for i in range(8): add(60, i, "Touch controls", "Touch %d (0..63 off, 64..127 touched)" % (i + 1), src="manual")
for i, n in enumerate(["Osc1", "Osc2", "Osc3", "Ringmod 1*3", "Ringmod 2*3", "Noise"]): add(61, i, "Solo controls", "Solo " + n + " (0..63 off, 64..127 on)", src="manual")
GL = [(0, "Protect"), (2, "Power-up Bank"), (3, "Power-up Patch"), (4, "Global MIDI Channel"), (6, "Master Tune Cents"), (7, "Master Transpose Semitones"),
      (8, "Keyboard Velocity Curve"), (9, "Synth Engine Velocity Response"), (11, "MIDI Clock Source"), (14, "Footswitch Mode"), (15, "Touch Filter Lock"),
      (16, "MIDI In Socket Function"), (17, "MIDI Out Socket Function"), (18, "USB-disconnected Sample Rate"), (19, "Patch Find Method"),
      (28, "Input1 Gain dB"), (29, "Input2 Gain dB"), (30, "Input1 to Fx"), (31, "Input2 to Fx"), (32, "Inputs 1/2 Link"), (33, "Synth to Outputs 1/2"),
      (34, "Input1 to Outputs 1/2"), (35, "Input2 to Outputs 1/2"), (36, "Record Mode"), (37, "Synth to Outputs 3/4"), (38, "Input1 to Outputs 3/4"),
      (39, "Input2 to Outputs 3/4"), (40, "Outputs 3/4 Level"), (41, "Outputs 3/4 Balance"), (42, "Phones Level Mode"), (43, "Phones Level (levbal mode)"),
      (44, "Phones Balance (levbal mode)")]
for lsb, n in GL: add(64, lsb, "Global", n)

DEFAULTS = {(0, 3): 127}
for _k in range(9): DEFAULTS[(1, 86 + 5 * _k)] = 64   # mod matrix depth, centre = 0 (slots 1 to 9)
for _k in range(11): DEFAULTS[(2, 3 + 5 * _k)] = 64   # mod matrix depth, slots 10 to 20   # start values that are not 0: Filter envelope sustain level full
lines = open(path).read().splitlines()
keep = [l for l in lines if l.startswith("id,") or l.startswith("cc_") or l.startswith("cc14_")]
out = list(keep)
seen = set()
for msb, lsb, g, n, kind, lo, hi, src in rows:
    key = (msb, lsb)
    if key in seen: sys.exit("duplicate address %s" % (key,))
    seen.add(key)
    q = '"%s"' % n if "," in n else n
    out.append("nrpn_%d_%d,%s,%s,%s,%d,%d,%d,%d,%d,%s,0" % (msb, lsb, g, q, kind, msb, lsb, lo, hi, DEFAULTS.get(key, 0), src))
open(path, "w").write("\n".join(out) + "\n")
print("wrote %d CC lines + %d NRPN lines" % (len(keep) - 1, len(rows)))
