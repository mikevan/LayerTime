# Abstract the mesh UI layer so both protocols share one screen codebase

NOTE: this file previously held a "decide what happens to MeshCore" issue. That framing was wrong. **Dual mesh is a permanent, deliberate feature of LayerTime, not a temporary state.** The point is that you can wear one watch into a Meshtastic area or a MeshCore area and use whichever the people around you are on, with the same look and feel either way. Neither protocol is going anywhere.

What is actually wanted is one UI serving both.

## The problem

The two mesh screens were written independently and duplicate a great deal:

| | lines |
|---|---|
| `src/ui/MeshScreen.{h,cpp}` | 457 |
| `src/ui/MeshtasticScreen.{h,cpp}` | 1,174 |

MeshtasticScreen has the fuller interface - nodes, chats, delivery-state threads, channels, map, quick phrases - while MeshScreen is a simpler single-page view. So the two protocols currently look and behave differently, which is exactly what this feature is not supposed to feel like.

Both also carry their own copies of the same furniture: page containers, list building, row context pools, BACK handling, the quick-phrase composer, and the same `lv_obj_clean()`-only-from-render() discipline.

## The work

- [ ] Pull a protocol-agnostic mesh UI out of `MeshtasticScreen` - nodes, chats, threads, composer, channels, map - driven by an interface rather than by `MeshtasticService` directly
- [ ] Define that interface against what both services can actually answer (heard nodes, messages, send, identity, channels where applicable)
- [ ] Back it with `MeshService` for MeshCore and `MeshtasticService` for Meshtastic
- [ ] Retire the duplicated screen code

## Why it is worth doing

- **Memory.** Both screen trees are built and held at once today. Merging them is the single largest UI-side saving available.
- **Consistency.** One implementation means MeshCore automatically gains the delivery states, threads, map and composer that only Meshtastic has now - and any future improvement lands on both at once.
- **Maintenance.** Every UI fix currently has to be made twice, or silently isn't.

Note the deliberate earlier decision NOT to build this abstraction until Meshtastic was finished, so it could be designed against a complete implementation rather than guessed at. Meshtastic is finished, so that condition is met.

Features that are genuinely protocol-specific - MeshCore's signed Ed25519 adverts, Meshtastic's PKC and multi-channel - need somewhere sensible to live rather than being flattened away.
