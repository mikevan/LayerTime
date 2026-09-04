# Meshtastic: node long-press filter

MUI parity gap. The Nodes list supports tap-to-select (opens a DM thread) but not long-press to filter.

Note the hazard already hit twice in this codebase: lists are rebuilt with `lv_obj_clean()` from `render()` only, never from inside a row callback, so a row is never deleted while its own event is still dispatching. Any filter has to set state and let the next render tick rebuild.
