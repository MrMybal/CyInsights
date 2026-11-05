# Historical validation: retired native reader, 5 November 2026

This record predates the CyDoc migration. See ../CyDoc/VALIDATION.md for the current reader.

- A clean Windows x64 CMake configuration built CyInsightsApp and the reader's
  tests. CyManualTests and the existing CyUiModuleTests pass. The manual suite
  renders all 24 chapters in both languages, checks offline PNG bytes, history,
  internal-link isolation, accent-insensitive search, image caching/context
  reset, narrow illustration tabs, explicit scrolling and two theme/density
  combinations. Fixtures use a declared test texture; they do not prove hardware
  upload or headset visibility.
- The actual Windows executable produced inspected screenshots of the English
  timeline guide, French tile-cost example and French HUD-selection diagram.
  Original product screenshot upload succeeded. Light/dark themes and 1x/1.5x
  DPI were exercised; narrow layouts use Read/Illustrations tabs. The window is
  constrained to the available work area, and image zoom uses the shared Images
  transform. These runs opened no recording and started no capture.
- The same complete guide is compiled into both Standard and Vulkan arm64 APKs.
  Android uses its system PNG decoder and an application-owned GLES texture
  cache, plus a native Android text-entry dialog for the manual search rather
  than assuming an ImGui field opens the Quest keyboard. The language setting
  is saved in the app's existing private display preferences.
- The Vulkan APK was updated in place on the connected Quest 3. Its installed
  SHA-256 matched the built package and the existing MainActivity preferences
  were byte-identical before launch. No active recording was interrupted and no
  game was started or restarted. The Help-page launch was requested. The headset
  was asleep, so this does not validate wearer-visible Android layout, keyboard,
  image upload or controller gestures.
- The build validator checks both languages, full chapter coverage, UTF-8,
  stable IDs, all manual links, allowed local screenshot paths and PNG dimensions.
  The pinned core/CyUI dependency verification passes. The worktree publication
  audit passes; no push or release was performed for this change.

This document does not upgrade the outstanding custom Quest HUD-curve visual
test to a success. The manual explicitly retains that alpha limitation.
