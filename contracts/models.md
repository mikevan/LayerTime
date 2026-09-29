# LayerTime Models

Contract version 0.1. Enum values are in `vectors/enums.json`. The C++
binding is `src/core/model/`.

The "T-Ultra source" column says where the T-Watch Ultra gets each value
today. It is there so the adapter work maps existing behaviour instead of
reinventing it.

## Timestamp

| Field | Type | Meaning |
|---|---|---|
| uptimeMs | uint32 | Milliseconds since the producing device booted. Only meaningful on that device. Never compared across devices. |
| wallClockValid | bool | The producer had a trustworthy wall clock. |
| unixSeconds | uint32 | Seconds since 1970-01-01 UTC. 0 and not shown when wallClockValid is false. |

## NavigationState

Where the wearer is and how much to trust it.

| Field | Type | Units | T-Ultra source |
|---|---|---|---|
| receiverEnabled | bool | | WatchState.gpsEnabled |
| fixType | FixType | | WatchState.gpsFixType (UBX-NAV-PVT) |
| fixUsable | bool | | WatchState.gpsFix |
| everHadFix | bool | | WatchState.gpsEverHadFix |
| fixAgeMs | uint32 | ms | WatchState.gpsFixAgeMs |
| satellites | uint8 | | WatchState.gpsSatellites |
| latitudeDeg, longitudeDeg | double | degrees | WatchState.latitude, longitude |
| altitudeValid, altitudeM | bool, float | m | WatchState.gpsAltitudeValid, altitudeFt converted |
| horizontalAccuracyValid, horizontalAccuracyM | bool, float | m | WatchState.gpsHorizontalAccuracy* |
| verticalAccuracyValid, verticalAccuracyM | bool, float | m | WatchState.gpsVerticalAccuracy* |
| hdopValid, hdop | bool, float | | WatchState.gpsHdop* |
| speedValid, speedMps | bool, float | m/s | WatchState.gpsSpeedMph converted |
| courseValid, courseDeg | bool, float | degrees | WatchState.gpsCourse* |
| headingValid, headingDeg | bool, float | degrees | Never set. The T-Ultra has no magnetometer. |
| updated | Timestamp | | |

Rules:

- Position fields are meaningful only while `fixUsable` is true.
- Position confidence is sized from `horizontalAccuracyM`, never from HDOP.
  HDOP is satellite geometry. It is carried only because the T-Ultra GPS
  screen displays it.
- Course is direction of travel from the receiver. Heading is direction faced
  from a compass. They are different quantities and are never substituted
  for each other.

## MonitorEvent

One emitter matched by one detector. Repeat sightings of the same emitter
(same `sourceId`) by the same detector update the existing event. They do not
create a new one.

| Field | Type | Size | T-Ultra source |
|---|---|---|---|
| eventId | uint32 | | Assigned when the record is created; +1 per new record |
| detector | ReconTarget | | The detector that matched. Always a single detector, Deauth through GoogleTag. |
| confidence | Confidence | | ReconDetection.confidence |
| sourceKind | SourceKind | | The radio that saw it (Wi-Fi or BLE). Flock can be either. |
| sourceId | text | 19 | ReconDetection.address. Untrusted over-the-air text. |
| detail | text | 40 | ReconDetection.detail |
| rssi | int8 | dBm | ReconDetection.rssi |
| channel | uint8 | | ReconDetection.channel. 0 when unknown or BLE. |
| band | Band | | 2.4 GHz on the T-Ultra for Wi-Fi, Unknown for BLE |
| count | uint32 | | ReconDetection.encounterCount |
| lastSeen | Timestamp | | ReconDetection.lastSeenMs |

Update rules, characterized from the T-Ultra:

- On a repeat sighting, `rssi`, `channel`, and `lastSeen` take the new values
  and `count` goes up by one.
- `confidence` only ever rises. A Low match never downgrades a High one.
- When the list is full (`ReconState.kMaxEvents`, 40), the oldest event is
  dropped to make room.

A detector's display name is not stored on the event. Clients look it up in
`reconTargetDisplayNames` and `reconTargetShortNames` in
`vectors/enums.json`.

## ReconState

What Recon is doing right now. The event list is kept by the application
core and read from it, not copied into this record. The monitor source only
acquires; it does not own event history.

| Field | Type | T-Ultra source |
|---|---|---|
| selected | ReconTarget | ReconStatus.detector |
| active | ReconTarget | ReconStatus.activeDetector |
| monitoring | bool | ReconStatus.monitoring |
| earlyWarningEnabled | bool | ReconStatus.earlyWarningEnabled |
| earlyWarningResting | bool | ReconStatus.earlyWarningResting |
| alertPending | bool | ReconStatus.alertPending |
| lastEventId | uint32 | ReconStatus.eventSerial |
| eventCount | uint8 | ReconStatus.detectionCount |

Capacity: `kMaxEvents` = 40.

A client shows when monitoring is rotating or resting. It never implies
continuous coverage.

## Mesh

Meshtastic and MeshCore normalized into one model. Protocol-specific detail
stays in each network's own service until a UI has a demonstrated need for it
here.

### MeshNodeId

A node's native identity on its own network, carried without truncation.

| Field | Type | Size | Meaning |
|---|---|---|---|
| kind | MeshIdKind | | What form the identity is in. Fixes both the network and the meaning of the bytes. |
| length | uint8 | | Number of bytes used. Must match the kind. |
| bytes | bytes | 32 | The identity |

| Kind | Network | Length | Bytes |
|---|---|---|---|
| None | none | 0 | No identity: unknown or not applicable |
| MeshtasticNodeNum | Meshtastic | 4 | The 32-bit NodeNum, big-endian |
| MeshCorePublicKey | MeshCore | 32 | The full public key |
| MeshCorePublicKeyPrefix | MeshCore | 1 to 31 | The leading bytes of the public key |

Rules:

- A node's network is the network of its id kind. There is no separate
  network field to disagree with it.
- Two identities are the same node only when both are well formed, of the
  same kind and length, and byte-for-byte equal. A prefix is never the same
  node as a full key, even when the leading bytes match, because two keys can
  share a prefix. `None` never matches anything, including `None`.
- `MeshCorePublicKeyPrefix` exists to characterize the T-Ultra honestly. Its
  MeshCore service keeps only the first four bytes of each key today, so its
  adapter reports a four-byte prefix, not a full identity. A platform that
  holds the full key reports `MeshCorePublicKey`.

### MeshDestination

Where a message goes, stated as what it means. No network gets a made-up
node number for "everyone."

| Field | Type | Meaning |
|---|---|---|
| kind | MeshDestinationKind | Node (one node) or Channel (everyone on a channel) |
| node | MeshNodeId | The node, when kind is Node |
| channel | uint8 | Index into that network's channel list as the platform exposes it, when kind is Channel |

The network travels alongside the destination, in the message or the command.
On the T-Ultra, Meshtastic channels are its channel slots, and MeshCore
exposes one channel, the public group, at index 0.

The default destination is a Node with no identity. That is not a valid
target, so a command whose destination was never set is rejected as
`InvalidArgument` rather than going out on a channel.

### MeshNode

| Field | Type | Size | Meaning |
|---|---|---|---|
| id | MeshNodeId | | Its native identity. Its kind gives the node's network. |
| displayName | text | 40 | Meshtastic long name, MeshCore advert name |
| shortName | text | 8 | Meshtastic only. Empty on MeshCore. |
| lastSeen | Timestamp | | Last packet heard from it |
| rssiValid, rssi | bool, float | | dBm |
| snrValid, snr | bool, float | | dB |
| batteryValid, batteryPercent | bool, uint8 | | 0 to 100 |
| externalPower | bool | | Meshtastic reports battery over 100 for this |
| positionValid, latitudeDeg, longitudeDeg | bool, double, double | | |
| altitudeValid, altitudeM | bool, int32 | | metres |
| positionTimeValid, positionTime | bool, Timestamp | | When the position was reported. Not the same as lastSeen. |
| hopsAwayValid, hopsAway | bool, uint8 | | |

### MeshMessage

| Field | Type | Size | Meaning |
|---|---|---|---|
| network | MeshNetwork | | |
| source | MeshNodeId | | Kind None when unknown. MeshCore group text names its sender only inside the text. |
| destination | MeshDestination | | A node, or a channel |
| fromSelf | bool | | Sent by this wearer |
| delivery | DeliveryState | | |
| text | text | 160 | |
| received | Timestamp | | |
| rssiValid, rssi / snrValid, snr / hopsValid, hops | | | As heard |

### MeshNetworkStatus and MeshState

| Field | Type | Meaning |
|---|---|---|
| supported | bool | This platform has this network |
| radioEnabled | bool | The wearer turned it on |
| radioReady | bool | It is actually running |
| radioError | int16 | Platform error code when enabled but not ready, else 0 |
| advertisingEnabled | bool | Announcing this node |
| ownName | text, 24 | This node's name on that network |
| nodeCount, messageCount | uint8 | How many nodes and messages the network currently holds |

`MeshState` holds one `MeshNetworkStatus` per network, indexed by
`MeshNetwork` value. It is application state, owned by the core. Each
network's transport supplies observations to the core and carries its
commands out; it does not own or serve the shared representation.

The shared node and message lists are not built yet. In Phase 0 the T-Ultra
screens still render from each network's own service; moving them onto the
shared model is the unified mesh UI work.

## Alert

Something the wearer is interrupted for. The application decides whether to
alert. The platform decides how.

| Field | Type | Meaning |
|---|---|---|
| kind | AlertKind | Only ReconDetection exists today |
| eventId | uint32 | The MonitorEvent that caused it |
| raised | Timestamp | |

Suppression policy, characterized from the T-Ultra and binding on every
platform: every event is logged and counted, but **no alert is raised when
sleep mode is on, or when the match is Low confidence.** Nothing is missed;
only the interruption is suppressed.

## QuickMessage

| Field | Type | Size | Meaning |
|---|---|---|---|
| id | uint8 | | Stable within a library. Commands refer to it. |
| text | text | 32 | |

A library holds at most 20 messages. The default library is
`vectors/quick_messages_default.json`.
