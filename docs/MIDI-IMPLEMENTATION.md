# UltraNova: MIDI implementation

Source: Novation "UltraNova Program Parameter MIDI Assignments" (MIDI controller list and NRPN list), file `ultranova-midi-implementation2.pdf` in our project files. Everything here is from that document. Value ranges and enum meanings are **not** in it.

## Source quirks

- CC 57 is printed twice: "Noise Type" and "Pre-FX Level". CC 58 is absent and CC 59 is "Post-FX Level". Most likely Pre-FX Level is 58 **(check on hardware)**.
- CC 11 is listed as expression. CC 118 and 119 are free.
- Typos in the source: "Outut" (Distort2 Output Level), "Sustain Rate" naming repeats per envelope.

## NRPN rules

- nrpnMSB defaults to 0 and is reset to 0 after any CC 6 data entry. So NRPN 0:xxx needs only CC 98 (LSB) then CC 6. The panel does not send MSB 0 either; assume MSB 0 unless set.
- High resolution (Tweak Data, ClockBPM 2:63): send/receive four messages in order NRPN MSB, NRPN LSB, data MSB, data LSB. The 14-bit value applies when the data LSB arrives; later LSBs are ignored.
- Packed parameters: 0:122 (up to 64 1-bit values), 0:123 (32 2-bit), 0:124 (16 3-bit), 0:125 (8 4-bit), 0:126 (4 5-bit), 0:127 (2 6-bit). They set bit-fields without knowing the other fields. The "Patch & Global spec" packed byte details are in a separate Novation document we do not have.
- Touch and solo controls: the unit sends 0 = off, 127 = on; it receives 0 to 63 = off, 64 to 127 = on.
- Global/Audio setup is NRPN MSB 64 (Memory Protect must be off for edits, per our notes).

## CC map

Standard MIDI numbers marked (std).

| CC | Function | CC | Function |
|---|---|---|---|
| 0 | Bank MSB (std) | 64 | Sustain pedal (std) |
| 1 | Mod wheel (std) | 65 | Filter1 Drive Type |
| 2 | Breath (std) | 66 | Sostenuto (std) |
| 3 | Polyphony Mode | 67 | Soft pedal (std) |
| 4 | Foot controller (std) | 68 | Filter1 Type |
| 5 | Portamento Rate | 69 | Filter1 Track |
| 6 | Data entry MSB (std) | 70 | AmpEnv Sustain Level |
| 7 | Channel volume (std) | 71 | Filter1 Resonance |
| 8 | Dry:Wet Balance | 72 | AmpEnv Release |
| 9 | Pre-Glide | 73 | AmpEnv Attack |
| 10 | Pan Position | 74 | Filter1 Frequency |
| 11 | Expression (std) | 75 | AmpEnv Decay |
| 12 | Portamento expo/linear | 76 | Easy Vibrato Rate |
| 13 | Keyboard Octave Offset | 77 | Easy Vibrato Depth |
| 14 | Unison Count | 78 | Filter1 Q-normalise |
| 15 | Unison Detune | 79 | FiltEnv > Filter1 Freq |
| 16 | Osc123 Drift | 80 | Filter2 Drive |
| 17 | Osc123 Start Phase | 81 | Filter2 Drive Type |
| 18 | Osc123 Fixed Note | 82 | Filter2 Type |
| 19 | Osc1 Waveform/WT/Audio | 83 | Filter2 Frequency |
| 20 | Osc1 Wavetable Interpolate | 84 | Filter2 Track |
| 21 | Osc1 Pulsewidth/WT index | 85 | Filter2 Resonance |
| 22 | Osc1 Sync Depth | 86 | Filter2 Q-normalise |
| 23 | Osc1 Hardness | 87 | FiltEnv > Filter2 Freq |
| 24 | Osc1 Density | 88 | Pan Mod Rate |
| 25 | Osc1 Density Detune | 89 | Pan Mod Rate Sync |
| 26 | Osc1 Semitone Offset | 90 | Pan Mod Depth |
| 27 | Osc1 Cents Offset | 91 | FxSlot1 Level |
| 28 | Osc1 Bendwheel > Pitch | 92 | FxSlot2 Level |
| 29 | Osc2 Waveform/WT/Audio | 93 | FxSlot3 Level |
| 30 | Osc2 Wavetable Interpolate | 94 | FxSlot4 Level |
| 31 | Osc2 Pulsewidth/WT index | 95 | FxSlot5 Level |
| 32 | Bank LSB (std) | 96 / 97 | Data inc / dec (std) |
| 33 | Osc2 Sync Depth | 98 / 99 | NRPN LSB / MSB (std) |
| 34 | Osc2 Hardness | 100 / 101 | RPN LSB / MSB (std) |
| 35 | Osc2 Density | 102 | Pan L/R Phase Offset |
| 36 | Osc2 Density Detune | 103 | L/R Width Enhance |
| 37 | Osc2 Semitone Offset | 104 | Pan Spare |
| 38 | Data entry LSB (std) | 105 | Env Packed Mono/Multi Triggers (bits 5:E6 4:E5 3:E4 2:E3 1:F 0:A) |
| 39 | Osc2 Cents Offset | 106 | Env Level Track Centre |
| 40 | Osc2 Bendwheel > Pitch | 107 | Drum Time |
| 41 | Osc3 Waveform/WT/Audio | 108 | AmpEnv Velocity |
| 42 | Osc3 Wavetable Interpolate | 109 | AmpEnv Sustain Rate |
| 43 | Osc3 Pulsewidth/WT index | 110 | AmpEnv Sustain Time |
| 44 | Osc3 Sync Depth | 111 | AmpEnv A/D Repeats |
| 45 | Osc3 Hardness | 112 | AmpEnv Attack Track |
| 46 | Osc3 Density | 113 | AmpEnv Decay Track |
| 47 | Osc3 Density Detune | 114 | AmpEnv Level Track |
| 48 | Osc3 Semitone Offset | 115 | AmpEnv Attack Slope |
| 49 | Osc3 Cents Offset | 116 | AmpEnv Decay Slope |
| 50 | Osc3 Bendwheel > Pitch | 117 | AmpEnv Touch Trigger |
| 51 | Osc1 Mix Level | 118, 119 | Free |
| 52 | Osc2 Mix Level | 120 | All Sounds Off (std) |
| 53 | Osc3 Mix Level | 121 | Reset All Controllers (std) |
| 54 | Osc13 Ringmod Mix Level | 122 | Local Control (std) |
| 55 | Osc23 Ringmod Mix Level | 123 | All Notes Off (std) |
| 56 | Noise Mix Level | 124 to 127 | Omni off/on, Mono on, Poly on (std) |
| 57 | Noise Type (and "Pre-FX Level", see quirks) | | |
| 59 | Post-FX Level | | |
| 60 | Filter Configuration | | |
| 61 | Filter Balance | | |
| 62 | Filter Packed Data (bit 1: Res-Link, bit 0: Freq-Link) | | |
| 63 | Filter1 Drive | | |

## NRPN map (MSB:LSB)

### MSB 0

| NRPN | Function |
|---|---|
| 0:0 to 0:13 | FiltEnv: Velocity, Attack, Decay, Sustain Level, Release, Sustain Rate, Sustain Time, A/D Repeats, Attack Track, Decay Track, Level Track, Attack Slope, Decay Slope, Touch Trigger |
| 0:14 to 0:27 | Env3: Delay, Attack, Decay, Sustain Level, Release, Sustain Rate, Sustain Time, A/D Repeats, Attack Track, Decay Track, Level Track, Attack Slope, Decay Slope, Touch Trigger |
| 0:28 to 0:41 | Env4, same 14 params as Env3 |
| 0:42 to 0:55 | Env5, same |
| 0:56 to 0:69 | Env6, same |
| 0:70 to 0:78 | LFO1: Waveform, Phase Offset, Slew Rate, Shape, Delay, Delay Sync, Rate, Rate Sync, Packed (bit 4 Fade, 3 DelayMulti, 2 Common, 1 KeySync, 0 OneShot) |
| 0:79 to 0:87 | LFO2, same 9 |
| 0:88 to 0:96 | LFO3, same 9 |
| 0:97, 0:98 | Fx Configuration, Fx Feedback |
| 0:99 to 0:103 | Fx Slot 1 to 5 Effect Select |
| 0:104 to 0:109 | EQ: Bass Freq, Bass Level, Mid Freq, Mid Level, Treble Freq, Treble Level |
| 0:110 to 0:115 | Compress1: Ratio, Threshold, Attack, Release, Hold, Auto-Gain |
| 0:116 to 0:121 | Compress2, same 6 |
| 0:122 to 0:127 | Packed parameter specials (1 to 6 bit) |

### MSB 1

| NRPN | Function |
|---|---|
| 1:0 to 1:2 | Distort1: Type, Compensation, Output Level |
| 1:3 to 1:5 | Distort2, same |
| 1:6 to 1:11 | Delay1: Time, Sync Time, Feedback, Width, L/R Ratio, Slew Rate |
| 1:12 to 1:17 | Delay2, same |
| 1:18 to 1:20 | Reverb1: Type, Decay, Damping |
| 1:21 to 1:23 | Reverb2, same |
| 1:24 to 1:29 | Phaser/Chorus1: Type, Rate, Rate Sync, Feedback, Mod Depth, Delay |
| 1:30 to 1:35 | Phaser/Chorus2, same |
| 1:36 to 1:41 | Phaser/Chorus3, same |
| 1:42 to 1:47 | Phaser/Chorus4, same |
| 1:48 to 1:54 | Gator: Packed (bit 2 Latch, 1 KeySync, 0 On), Rate Sync, Mode, Level, Edge Slew, Decay, Delay |
| 1:55 to 1:60 | Vocoder: Packed (bit 1 SibType, 0 On), Balance, Width, Sibilance, Spectrum Shift, Spectrum Magnify |
| 1:61 | Arpeggiator Packed (bits 6-5 OutputMode, 2 Latch, 1 KeySync, 0 On) |
| 1:62 to 1:67 | Arp: Octaves, Rate Sync, Gate Time, Mode, Pattern, Velocity |
| 1:69, 1:70 | Arp Sequence, Quantise (1:68 not listed) |
| 1:78, 1:79 | Chord Transpose, Chord Spare |
| 1:80, 1:81, 1:82 | ModMatrix Velocity Slew Rate, Touch Slew Rate, Select |
| 1:83 to 1:127 | ModMatrix slots 1 to 9 |

### MSB 2

| NRPN | Function |
|---|---|
| 2:0 to 2:54 | ModMatrix slots 10 to 20 |
| 2:55 to 2:62 | Program Spare 1 to 8 |
| 2:63 | ClockBPM (14-bit, see rules) |

Mod matrix slot layout: five consecutive NRPNs per slot: Source1, Source2, Touch Enable, Depth, Destination. Slot k from 1 to 9 starts at 1:(83 + 5(k-1)); slot k from 10 to 20 starts at 2:(5(k-10)).

### MSB 5, 60, 61

| NRPN | Function |
|---|---|
| 5:0 to 5:15 | Gator Level L1 to L16 (each 0 to 7) |
| 5:16 to 5:31 | Gator Level R1 to R16 (each 0 to 7) |
| 60:0 to 60:7 | Touch 1 to 8 (touch performance controls) |
| 61:0 to 61:5 | Solo: Osc1, Osc2, Osc3, Ringmod 1*3, Ringmod 2*3, Noise |

### MSB 64 (Global / Audio setup, data structure version 0.0.34 onwards)

| NRPN | Function |
|---|---|
| 64:0 | Protect |
| 64:2, 64:3 | Power-up Bank, Power-up Patch |
| 64:4 | Global MIDI Channel |
| 64:6, 64:7 | Master Tune Cents, Master Transpose Semitones |
| 64:8, 64:9 | Keyboard Velocity Curve, Synth Engine Velocity Response |
| 64:11 | MIDI Clock Source |
| 64:14, 64:15 | Footswitch Mode, Touch Filter Lock |
| 64:16, 64:17 | MIDI In Socket Function, MIDI Out Socket Function |
| 64:18 | USB-disconnected Sample Rate |
| 64:19 | Patch Find Method |
| 64:28, 64:29 | Input 1 Gain dB, Input 2 Gain dB |
| 64:30, 64:31 | Input 1 to Fx, Input 2 to Fx |
| 64:32 | Inputs 1/2 Link |
| 64:33 to 64:35 | Synth, Input 1, Input 2 to Outputs 1/2 |
| 64:36 | Record Mode |
| 64:37 to 64:39 | Synth, Input 1, Input 2 to Outputs 3/4 |
| 64:40, 64:41 | Outputs 3/4 Level, Balance |
| 64:42 to 64:44 | Phones Level Mode, Phones Level (levbal mode), Phones Balance (levbal mode) |
| spares | 64:1, 5, 10, 12, 13, 20 to 27, 45 to 127; 65:0 to 109 |

## Open items

1. Value ranges and enum lists (filter types, waveforms, effect types, mod sources/destinations): not in this document; likely in Novation's Patch & Global spec or the manual. Needed before the UI can label values.
2. The CC 57 duplicate.
3. Patch SysEx dump/request format (not in this document).
4. Behaviour of NRPN reads: whether the unit answers requests, or only transmits on panel edits, is not stated.
