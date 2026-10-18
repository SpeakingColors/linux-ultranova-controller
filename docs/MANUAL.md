# Linux UltraNova Controller: manual (draft)

Status: design stage. The behavior below is what the mockups do today and what the plugin should copy.

## Oscillator waveform displays

Each oscillator shows a small picture of its current waveform.

- **Drag up or down on the picture** to step through waveforms. The picture and its name update at once.
- **Double-click the picture** to open the waveform directory: a scrolling list of every waveform by number and name. Click one to pick it.
- **Ctrl-click** (Cmd-click on macOS) resets the control to its saved value, like every other control in the suite.
- The list starts with Sine, Triangle, Sawtooth and Square, followed by the wavetables. In the mockup the list has 64 slots; the final list will come from the device, so it must be checked against Novation's UltraNova documentation before release.

## Mixer

All mixer faders are draggable. Double-click a fader to reset it.

## General rules shared by all cat plugins

- Knobs: drag up or down, mouse wheel, double-click to reset.
- Visuals (meters, scopes, lights) pause when the plugin window is hidden or not in front. Audio, automation and MIDI keep running.

Not affiliated with or endorsed by Novation. All trademarks belong to their owners.
