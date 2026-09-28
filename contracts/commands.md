# LayerTime Commands

Contract version 0.1. Enum values are in `vectors/enums.json`. The C++
binding is `src/core/model/LayerTimeCommand.h`.

A screen never calls a service. It issues a `LayerTimeCommand` and renders
the state that results. Every command returns exactly one `CommandResult`.

## CommandType

The set is exactly what the T-Ultra screens do today, plus
`MeshSendQuickMessage`.

| Command | Arguments | What it does | T-Ultra call it replaces |
|---|---|---|---|
| ReconStart | target | Start monitoring a detector, a group, or All | ReconService::startDetector |
| ReconStop | none | Leave manual monitoring. Early warning resumes if enabled. | ReconService::exitManualMode |
| ReconClearEvents | none | Empty the event list | ReconService::clearDetections |
| ReconAcknowledgeAlert | none | Clear the pending alert | ReconService::acknowledgeAlert |
| MeshSendText | network, destination, text (160) | Send typed or composed text | MeshService::sendPublicMessage, MeshtasticService::sendChannelMessage, MeshtasticService::sendDirectMessage |
| MeshSendQuickMessage | network, destination, quickMessageId | Send a library message by id | None yet |
| MeshSetChannel | index, name (12), key (48) | Create or replace a channel | MeshtasticService::setChannel |
| MeshRemoveChannel | index | Remove a channel | MeshtasticService::removeChannel |

For the two send commands, `network` selects the mesh and `destination` is a
node or a channel on that mesh (see `MeshDestination` in `models.md`). A
destination that was never set, or a node identity from a different network,
returns `InvalidArgument`.

`MeshSetChannel` and `MeshRemoveChannel` act on Meshtastic channels. A
platform without Meshtastic returns `Unsupported`.

## CommandResult

| Result | Meaning |
|---|---|
| Ok | Done |
| Unsupported | This platform or network cannot do this at all |
| InvalidArgument | It can, but not with these arguments (bad channel key, unknown quick message id, text too long) |
| NotReady | It can, but not now (radio off or not ready) |
| Failed | It tried and the attempt failed |

## Not commands yet

Settings changes (radio on or off, advertising, early warning, sleep mode,
units, and the rest) are not in contract 0.1. They get commands when
`AppSettings` is split into application settings and platform settings.
