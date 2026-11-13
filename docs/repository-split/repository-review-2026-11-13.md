# Repository overlap review, 2026-11-13

The authenticated GitHub repository inventory contains the seven canonical
LayerTime repositories below and the independent Notella product. There is no
separate LayerScope or old Vixen/beacon repository in that inventory. No
confirmed redundant repository in this product family is eligible for archive.
No repository was archived or deleted.

| Repository | Responsibility | Keep active |
|---|---|---|
| LayerTime | Watch firmware and watch integration tests | Yes |
| Hunter | ESP32-C5 T-Dongle sensors and SD tooling | Yes |
| Vixen | Flagship Android analysis and its AI workflow | Yes |
| LayerTime-Core | Shared application logic and Link implementation | Yes |
| LayerTime-C5 | Shared C5 board support | Yes |
| LayerTime-Sensors | Shared detector implementation | Yes |
| LayerTime-Contracts | Shared protocol specification and test vectors | Yes |
| Notella | Independent model training product | Yes |

LayerTime no longer tracks the extracted Hunter/Android implementations. The
remaining watch source, integration tests and public web material have distinct
roles. The old fox-hunt beacon environment inside Hunter is legacy source, not
another GitHub repository; renaming it remains separate from Vixen's identity.

Local history/recovery folders and worktrees are not redundant GitHub
repositories. Their evidence and unrelated local files were preserved. Other
unrelated account repositories were not classified merely because their names
resembled local development folders.

Future archive candidates need a verified replacement, a check for unique
branches/tags/releases/issues, and a dependency search. Add a replacement link
to their README before archiving; retain history. This review found no such
candidate, so no archive action was necessary.
