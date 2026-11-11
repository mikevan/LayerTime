# Corrective wireless reporting examples

Twelve authored synthetic training candidates. These are not field evidence or an independent evaluation set. No model has been trained on them.

Review the observations, the proposed wording, and whether its significance is clear. Technical correctness and readability both matter.

## same client same bssid

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 1 | True | 100 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 2 | True | 200 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 3 | True | 300 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 4 | True | 400 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 5 | True | 500 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 6 | True | 900 | 32 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | True | None |

**Repeated disconnect requests were followed by a security exchange for the same device and network**

Device 02:00:00:00:03:03 was addressed by six deauthentication frames claiming network 02:00:00:00:01:01, followed by one EAPOL frame for that same device and BSSID. This is a significant disruption pattern; increase your threat posture and examine the surrounding traffic. The frames request disconnection; they do not alone prove the device disconnected. EAPOL shows a security exchange, not a completed reconnect, a complete captured handshake, or password loss. The physical sender is not authenticated by its displayed address.

## same client different bssid

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 1 | True | 90 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 2 | True | 180 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 3 | True | 270 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 4 | True | 360 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 5 | True | 800 | 32 | 02:00:00:00:02:02 | 02:00:00:00:03:03 | True | None |

**The later security exchange belongs to a different network**

Five deauthentication frames address 02:00:00:00:03:03 on BSSID 02:00:00:00:01:01. The later EAPOL frame involves the same client but BSSID 02:00:00:00:02:02. Do not chain these as a reconnect to the targeted network. Roaming or another connection is possible; the recording does not establish the cause. The repeated disconnect requests still deserve attention.

## different client same bssid

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 1 | True | 80 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 2 | True | 160 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 3 | True | 240 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 4 | True | 600 | 32 | 02:00:00:00:01:01 | 02:00:00:00:04:04 | True | None |

**The security exchange involves a different device**

Four deauthentication frames address 02:00:00:00:03:03; the later EAPOL frame involves 02:00:00:00:04:04. Both name BSSID 02:00:00:00:01:01, but that network match does not make the client match. This sample does not show the targeted device reconnecting. Review the repeated disconnect requests without assigning the other device's exchange to this target.

## eapol precedes disconnects

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 32 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | True | None |
| 1 | True | 500 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 2 | True | 600 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 3 | True | 700 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 4 | True | 800 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |

**The security exchange happened before the disconnect requests**

One EAPOL frame precedes four deauthentication frames for the same device and BSSID. It cannot be described as a reconnect caused by those later requests. Repeated requests may indicate disruption, but the observed ordering does not establish a follow-on exchange.

## inactivity reason

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 11 | True | 0 | 10 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 4 |

**A disconnect request reports inactivity; this sample does not establish an attack**

One disassociation frame reports reason code 4, meaning inactivity. That is the sender's stated reason, not an independently verified cause. A single such frame does not establish deliberate disruption. No completed disconnect or reconnect is established by this frame alone.

## nonassociated reason

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 7 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |

**A disconnect request reports a device that was not associated**

One deauthentication frame reports reason code 7: a class-3 frame received from a nonassociated station. Code 7 does not mean inactivity. This is the stated reason in a frame, not proof of an attack or of the transmitter's identity.

## broadcast target

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 1 | True | 50 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 2 | True | 100 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 3 | True | 150 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 4 | True | 200 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 5 | True | 250 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 6 | True | 300 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 7 | True | 350 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |
| 8 | True | 400 | 12 | 02:00:00:00:01:01 | ff:ff:ff:ff:ff:ff | False | 7 |

**Repeated disconnect requests were broadcast to devices on this network**

Nine deauthentication frames claim BSSID 02:00:00:00:01:01 and the broadcast destination. This is a significant disruption pattern that warrants increased threat posture. There is no single targeted client MAC in these frames, and the recording does not establish how many devices received or acted on the requests.

## same ssid two bssids

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 1 | True | 0 | 8 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | None |
| 2 | True | 100 | 8 | 02:00:00:00:02:02 | 02:00:00:00:03:03 | False | None |

**Two access points advertise the same network name; ownership is unresolved**

Two beacon frames advertise CampNet with different BSSIDs. Legitimate networks can use multiple access points with the same name. These observations justify comparison with known network details, but do not alone establish an evil twin, malicious ownership, or that a device joined either access point.

## partial capture no eapol

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 1 | True | 60 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 2 | True | 120 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 3 | True | 180 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 4 | True | 240 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 5 | True | 300 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 6 | True | 360 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 7 | True | 420 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |

Coverage: {"endsImmediatelyAfterRequests": true}

**Repeated disconnect requests are visible; the later outcome was not captured**

Eight deauthentication frames address the same device and BSSID. No EAPOL appears in the supplied observations. The capture ends immediately after the requests, so absence here does not establish that the device failed to reconnect or that no security exchange occurred elsewhere.

## invalid clock

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 3 | False | 1000 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 7 |
| 9 | False | 0 | 32 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | True | None |

**The device and network match, but the event order cannot be established**

One deauthentication frame and one EAPOL frame name the same device and BSSID. Their timestamps are marked invalid. Frame identifiers are references, not reliable timestamps; do not describe either event as following or causing the other.

## ordinary exchange

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 0 | True | 0 | 32 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | True | None |
| 1 | True | 100 | 32 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | True | None |

No reportable issue established from these observations. This does not guarantee network safety.

## unknown reason

| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |
|---|---|---|---|---|---|---|---|

| 4 | True | 0 | 12 | 02:00:00:00:01:01 | 02:00:00:00:03:03 | False | 65535 |

**A disconnect request carries an unrecognized reason value**

One deauthentication frame carries reason value 65535. No supported meaning is supplied for this value; do not translate it into inactivity or a named attack. The observation warrants technical review, while its cause and whether a device disconnected remain unresolved.
