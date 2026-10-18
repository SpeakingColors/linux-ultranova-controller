# Roadmap: UltraNova controller

Editor for the Novation UltraNova, in the look of the Novation editor.

## Next
- [ ] Wave table names and the CC value to name mapping (the MIDI doc gives CC 19, 29 and 41 for the oscillator waveforms but no value list)
- [ ] Real MIDI I/O behind the mockup controls
- [ ] Librarian page with backup and restore

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
