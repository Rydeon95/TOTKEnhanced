# TOTKEnanched 0.3

Based on TotK Recomp v0.1.1.

TOTKEnanched 0.3 is based on TotK Recomp v0.1.1 (upstream commit 701bd628209956a70863d864844bc97cdaf074f1).

This local installer release keeps the upstream installer workflow: users provide their own game files and the installer
builds the game locally.

## Base

- Retains the upstream 0.1.1 installer, game extraction, local build, shader preparation, and install-folder workflow.
- Gives this variant the public release name and version `TOTKEnanched 0.3`.
- Keeps support for the upstream 1.2.1 and 1.4.3 game updates. Ultrawide work was developed and tested mainly on 1.4.3.

## Ultrawide resolutions

- Adds 3440x1440 and 5120x2160 to the Game resolution menu without removing existing modes.
- Uses output width divided by output height for the runtime aspect ratio and the camera projection correction.
- Keeps window size, swapchain size, game output resolution, and 3D scene resolution as separate settings and dimensions.
- Logs the active output, aspect ratio, ultrawide correction state, scene size, and UI safe area.

## Native scene rendering

- Raises the runtime's scene-size cap to 7680x4320 while retaining dynamic sizing from output resolution and render scale.
- Allows 5120x2160 output at 100% render scale to use a native 5120x2160 scene when the GPU and available memory support it.
- Grows the application pool and texture heap using checked, aligned calculations rather than a fixed 4096-pixel scene cap.

## HUD/UI

- Adds a centered, dynamic 16:9 safe area for HUD and supported 2D overlay draws at wider output ratios.
- Keeps viewport and scissor correction tied to the same UI draw and its narrowly matched sibling layers.
- Preserves the existing 16:9 layout and does not alter camera, FOV, or 3D projection outside the ultrawide camera patch.

## Save/Load

- Applies the stable safe-area handling to save-list cards and the load-confirmation screen, including their matching layers.

## Inventory/Materials

- Applies the stable 2D safe-area handling to Inventory and the Materials contextual menu.
- Leaves the 3D Link inventory preview correction disabled; see Known limitations.

## Minimap

- Keeps the minimap and its yellow marker within the stable gameplay HUD correction path.

## Renderer diagnostics

- Includes the on-demand renderer capture source changes. A request file beside `totk.exe` arms a capture for the next
  complete frame; output is written under `logs/ui-capture-*` with ordered draw and pass TSVs and a summary.
- The capture path remains idle unless the request file is present and does not change the normal rendering path.

## Validation

- `tools/make_release.py` built the portable installer bundle and its runtime library.
- The release builder's content scan found no game-derived content in the release folder; a second scan of the final
  folder also found no hits.
- The bundled installer check reached the missing user game inputs as expected, and its window self-test built every
  screen in English and Russian.
- The final ZIP was recursively audited: every entry matches the assembled folder, with no game/update/key files,
  game-derived folders, saves, logs, captures, or `totk.exe`.
- `tools/check_content.py` and `git diff --check` passed for the clean release worktree.
- `--prove` was not run because authentic `generated/v121` and `build/extracted/v121` inputs are not present in the
  workspace.

## Known limitations

1. Link's 3D Inventory preview can still be horizontally displaced in ultrawide.
2. The orange world/screen-space indicator has not been definitively corrected.
3. Ultrawide on game version 1.2.1 has not been fully validated.
4. 5120x2160 uses substantially more VRAM and system memory; it has been validated mainly at the technical/runtime level.
