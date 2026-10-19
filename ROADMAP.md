# Roadmap: UltraNova controller

Editor for the Novation UltraNova, in the look of the Novation editor.

## Done
- [x] Plugin skeleton: parameter map from the manual, CC and NRPN send/receive, host automation, cat-ui look, CI on three systems

## Next
- [ ] Test every map entry on the real device and flip it to verified (`hw` column in map/params.csv)
- [ ] Test the VST3 in Bitwig and Studio One on Linux
- [ ] Two-CC controls: find out which CC is the coarse one on the device

- [ ] Wave table names and the CC value to name mapping (the MIDI doc gives CC 19, 29 and 41 for the oscillator waveforms but no value list)
- [ ] Real MIDI I/O behind the mockup controls
- [ ] Librarian page with backup and restore

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
