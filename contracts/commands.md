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
| MeshSendText | network, destination, text (up to 160 characters) | Send typed or composed text | MeshService::sendPublicMessage, MeshtasticService::sendChannelMessage, MeshtasticService::sendDirectMessage |
| MeshSendQuickMessage | network, destination, quickMessageId | Send a library message by id | None yet |
| MeshSetChannel | index, name (up to 11 characters), key (up to 48 characters) | Create or replace a channel | MeshtasticService::setChannel |
| MeshRemoveChannel | index | Remove a channel | MeshtasticService::removeChannel |
| SetClockFormat | enabled (true = 24-hour) | Set the clock format | Settings CLOCK FORMAT row |
| SetUnits | enabled (true = metric) | Set display units | Settings UNITS row |
| SetSleepMode | enabled | Suppress alerts; events are still logged | Settings SLEEP MODE row |
| SetEarlyWarning | enabled | Run the background Recon sweep | Settings EARLY WARNING row |
| MeshSetAdvertising | network, enabled | Announce this node on a mesh | Settings MESHCORE ADVERTISE and MESHTASTIC ADVERTISE rows |
| MeshSetOwnName | network, name (up to 19 characters) | Name this node on a mesh. Empty lets the platform generate one. | Settings MESHTASTIC NAME page |

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

The settings commands change `ApplicationSettings` (see `models.md`). A
network that is not present returns `Unsupported`, as does
`MeshSetOwnName` for MeshCore, whose name is generated from its key. A name
that does not end inside its buffer returns `InvalidArgument`.

## Not commands

Platform settings are not in the contract. On the T-Ultra those are the
backlight brightness, GPS receiver power, SD card logging, Squachify, and
which mesh network holds the shared LoRa radio. Another target has its own.
