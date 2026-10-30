# TOTKEnhanced 0.3

Based on TotK Recomp v0.1.1.

TOTKEnhanced 0.3 is based on TotK Recomp v0.1.1 (upstream commit 701bd628209956a70863d864844bc97cdaf074f1).

An installer-based local build with native ultrawide output and stable HUD/UI safe-area changes.

**No game files are included.** You must provide your own legally obtained game dump, supported update, and console keys.
The installer extracts and builds the game locally on your PC, exactly like the upstream TotK Recomp release.

The installer continues to support game updates 1.2.1 and 1.4.3. Ultrawide changes were developed and tested mainly on
1.4.3; 1.2.1 ultrawide has not been fully validated.

## Ultrawide changes

- Adds 3440x1440 and 5120x2160 to the Game resolution menu.
- Uses a dynamic output aspect ratio for the camera correction.
- Supports native scene sizing above 4096 pixels, with dynamic memory sizing and a 7680x4320 runtime cap.
- Keeps supported HUD, save/load, Inventory 2D, Materials menu, and minimap elements within a centered 16:9 safe area.
- Includes on-demand renderer frame capture diagnostics.

## Known limitations

- Link's 3D Inventory preview can remain horizontally displaced in ultrawide.
- The orange world/screen-space indicator is unresolved.
- 1.2.1 ultrawide has not been fully validated.
- 5120x2160 requires considerably more GPU and system memory and has been validated mainly at the technical level.

See `CHANGELOG-ULTRAWIDE.md` for details.
