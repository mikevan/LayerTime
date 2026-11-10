> Documentation reference copy. Original repository path: `docs/layerscope-visual-design.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# LayerScope non-watch visual design

The November 4, 2026 handoff sets the direction: a straight-on circular silver/gold watch mounted over midnight-indigo spacecraft panels, with smoked glass and narrow cyan light channels. Brass mounting hardware, taped jumper wires, and a zip tie should read as restrained improvised assembly. Dynamic readings and controls remain native UI.

This redesign applies to phones and tablets. The LOKMAT hardware watch retains its separate sliding layout and physical-button behavior. `LocalSpacecraft` scopes the new panel material to the non-watch console; its default is false. Keep the legacy drawing path for the watch.

The first implementation adds deterministic angular panel housings, faint glass grids, inset lighting, and an Ops watch/telemetry row. It preserves the existing control callbacks and data sources. Full reference fidelity, mounting details, and device-size visual verification remain ongoing work.

Use the existing Gradle wrapper and installed Android SDK. Validate the full diagnostic flavor with `:app:testFullDebugUnitTest :app:assembleFullDebug`, then install its APK with ADB on an explicitly identified non-watch device. Do not deploy to the hardware watch as a side effect.

Commits and pushes are separate operations and are prohibited during the day. Keep unrelated pre-existing work intact.

The upper assembly and its branding/link plate are the title section. The watch intentionally overhangs the plate and neighboring telemetry, including obscuring duplicated data. Preserve this foreground/background hierarchy rather than adding clearance around every reading. The watch, bracket, and jumper cast contact shadows onto the title plate. This visual treatment is phone/tablet-only; retain the hardware watch's separate layout.

Detection reference: retain a compact crosshair/DETECTION header and real Hunter-count badge, a two-segment All/Deauth selector, one Start/Stop/Clear row with equal button widths, and a full-width Hunt Vixen action. Use explicit line heights and shared row alignment. Controls retain existing enabled conditions and callbacks; do not manufacture a connected state to match the sample image. The hardware watch continues on its legacy component path.

Builder is now a sixth persistent phone/tablet destination (Ops, Events, Tools, Builder, Mission, System), also available in the wide-screen navigation rail. Its initial scope is a blank 10,000 by 6,000 dp editing board, with pinch zoom, drag-to-pan, zoom buttons, Fit and Reset. Keep zoom anchored to the gesture and bound panning to the board. Native controls and the indigo housing stay stationary. The grid adapts to zoom and draws only visible lines. No example nodes, execution pipeline, or Hunter frame transport is implemented by this visual scaffold. This user-approved navigation addition supersedes the earlier five-tab limit and tablet-only Builder restriction. The hardware-watch layout remains separate.
