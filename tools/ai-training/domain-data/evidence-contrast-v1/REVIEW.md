# Case-specific evidence contrast lessons

Authored synthetic decoded-feature fixtures; not raw captures or independently reviewed labels.

## client-match

Does this exchange belong to the targeted client and network?

### Variant 1

{"exchange":{"bssid":"radio-a","client":"client-a"},"request":{"bssid":"radio-a","client":"client-a"}}

The exchange matches the targeted client and network.

Both identity keys match; timing still needs to be checked before declaring a follow-on link.

Identity agreement alone does not establish time order, a completed reconnect or hostile intent.

### Variant 2

{"exchange":{"bssid":"radio-a","client":"client-b"},"request":{"bssid":"radio-a","client":"client-a"}}

The exchange belongs to a different client.

The network matches, but the client does not, so this is not a same-pair exchange.

Identity agreement alone does not establish time order, a completed reconnect or hostile intent.

## network-match

Do these observations establish a same-network link?

### Variant 1

{"exchange":{"bssid":"radio-a","client":"client-a","ssid":"Camp"},"request":{"bssid":"radio-a","client":"client-a","ssid":"Camp"}}

The client and network-radio identities match.

The shared client and BSSID satisfy the identity part of the link, with timing still unresolved.

A displayed network name is not enough to merge distinct network-radio observations.

### Variant 2

{"exchange":{"bssid":"radio-b","client":"client-a","ssid":"Camp"},"request":{"bssid":"radio-a","client":"client-a","ssid":"Camp"}}

The familiar name hides a different network radio.

The SSID and client match, but the different BSSID fails this same-radio rule.

A displayed network name is not enough to merge distinct network-radio observations.

## time-order

Is the exchange a later observation under this follow-on rule?

### Variant 1

{"exchangeTimeMs":1500,"requestTimeMs":1000,"samePair":true}

The same-pair exchange occurred after the request.

The measured order is forward with a 500 ms gap; the configured window must also be satisfied.

Event order is not proof that the request caused any later exchange.

### Variant 2

{"exchangeTimeMs":500,"requestTimeMs":1000,"samePair":true}

The exchange occurred before the request.

The exchange is 500 ms earlier, so it cannot be the later event under this rule.

Event order is not proof that the request caused any later exchange.

## missing-time

Can the configured window establish the measured time gap?

### Variant 1

{"configuredWindowMs":30000,"exchangeTimeMs":1500,"requestTimeMs":1000}

The observed gap is 500 ms.

The two timestamps establish a measured gap independently of the 30-second rule setting.

A configured rule interval is not a measurement of event duration.

### Variant 2

{"configuredWindowMs":30000,"exchangeTimeMs":null,"requestTimeMs":1000}

The exchange time is unknown.

The 30-second setting supplies no missing timestamp and cannot establish a measured gap.

A configured rule interval is not a measurement of event duration.

## window-boundary

Does the supplied delay meet this explicitly inclusive rule?

### Variant 1

{"observedDelayMs":1000,"rule":{"maximumMs":1000,"minimumMs":0,"upperBoundInclusive":true}}

The delay meets the inclusive upper boundary.

Exactly 1,000 ms is included by the supplied rule; no extra tolerance has been requested.

A rule match or near miss is not a verdict about hostile intent.

### Variant 2

{"observedDelayMs":1001,"rule":{"maximumMs":1000,"minimumMs":0,"upperBoundInclusive":true}}

The delay exceeds the configured boundary.

A 1,001 ms delay is outside this 1,000 ms rule, even though the difference is small.

A rule match or near miss is not a verdict about hostile intent.

## disconnect-outcome

What can we report about the outcome of the disconnect request?

### Variant 1

{"clientDisconnectObserved":false,"requestObserved":true}

A disconnect request was observed; the outcome is unknown.

The request alone does not show that the addressed client disconnected.

Even an observed disconnection does not by itself establish which event caused it or who controlled the sender.

### Variant 2

{"clientDisconnectObserved":true,"requestObserved":true}

Both a request and a client disconnection were observed.

The supplied client-state observation supports reporting a disconnection as well as the request.

Even an observed disconnection does not by itself establish which event caused it or who controlled the sender.

## address-role

Which address is the decoded receiver rather than the claimed transmitter?

### Variant 1

{"decodedFields":{"wlan.ra":"client-a","wlan.ta":"radio-a"}}

The receiver is client-a; the claimed transmitter is radio-a.

The two fields have different roles, so the target must not be reported as the sender.

Neither decoded address authenticates the physical person or device that transmitted the frame.

### Variant 2

{"decodedFields":{"wlan.ra":"client-a","wlan.ta":"client-a"}}

Both decoded address fields contain client-a.

Equal address values do not remove the distinction between receiver and transmitter roles.

Neither decoded address authenticates the physical person or device that transmitted the frame.

## pmf-capability

Does this advertisement say protection is required?

### Variant 1

{"decodedFields":{"wlan.rsn.capabilities.mfpc":true,"wlan.rsn.capabilities.mfpr":false}}

Protection is advertised as capable, not required.

The capability flag is set while the requirement flag is clear; those are different decoded claims.

An advertisement does not prove session negotiation or acceptance of a particular disconnect request.

### Variant 2

{"decodedFields":{"wlan.rsn.capabilities.mfpc":true,"wlan.rsn.capabilities.mfpr":true}}

Protection is advertised as both capable and required.

Both flags are set in the supplied advertisement, including the explicit requirement flag.

An advertisement does not prove session negotiation or acceptance of a particular disconnect request.

## relative-origin

Does the negative relative time invalidate this event ordering?

### Variant 1

{"exchangeTimeMs":-100,"requestTimeMs":0,"timeBasis":"milliseconds relative to recording marker; negative values are permitted"}

The exchange is 100 ms before the marker and request.

A negative relative timestamp is permitted by this time basis and establishes an earlier event.

Relative time needs its declared origin; its sign alone is not a capture-health verdict.

### Variant 2

{"exchangeTimeMs":100,"requestTimeMs":0,"timeBasis":"milliseconds relative to recording marker; negative values are permitted"}

The exchange is 100 ms after the marker and request.

The positive relative timestamp establishes a later event, with any window check still separate.

Relative time needs its declared origin; its sign alone is not a capture-health verdict.

## entropy-type

Does low address entropy tell us the address-type flags?

### Variant 1

{"features":{"address_entropy":0.0,"random_address_fraction":1.0}}

All accepted records carry random-address flags despite low entropy.

Low entropy describes address concentration; the separate address-type fraction is one.

The flag does not measure address rotation or establish a physical-device identity.

### Variant 2

{"features":{"address_entropy":0.0,"random_address_fraction":0.0}}

No accepted records carry random-address flags in this example.

The address-type fraction is zero here; that conclusion comes from the flag measurement, not entropy.

The flag does not measure address rotation or establish a physical-device identity.

## newness-type

Do random-address flags establish that new addresses appeared?

### Variant 1

{"features":{"new_address_fraction":0.0,"random_address_fraction":1.0}}

Random-address flags are present, but none of these address records are new to the comparison set.

Address type and address newness measure different properties; a random-format address can have been seen before.

New address values are not a verified count of newly arrived physical devices.

### Variant 2

{"features":{"new_address_fraction":1.0,"random_address_fraction":1.0}}

Random-address flags are present and every record uses an address absent from the comparison set.

The supplied new-address fraction, rather than the address-type flag, supports the newness observation.

New address values are not a verified count of newly arrived physical devices.

## repetition-variation

Can repeated advertising data coexist with variation in its length?

### Variant 1

{"features":{"ad_data_length_std":2.0,"payload_repeat_fraction":0.9}}

Advertising data repeats often, while its length still varies.

High exact-data repetition does not require every accepted data item to have the same length.

Neither measurement alone establishes a benign or hostile purpose.

### Variant 2

{"features":{"ad_data_length_std":0.0,"payload_repeat_fraction":0.9}}

Advertising data repeats often and has no measured length variation.

Zero length variation does not mean all payload bytes are identical; the repetition statistic is separate.

Neither measurement alone establishes a benign or hostile purpose.

## advertisement-gatt

Was a GATT read observed in the supplied evidence?

### Variant 1

{"advertisedService":true,"decodedGattReadObserved":false}

A service was advertised, but no GATT read was supplied.

An advertisement describes advertised service information; it does not establish a characteristic read.

A read observation does not by itself establish authorization, hostile intent or a completed application outcome.

### Variant 2

{"advertisedService":true,"decodedGattReadObserved":true}

The supplied evidence includes a decoded GATT read as well as advertising.

The separate read observation, rather than the advertisement alone, supports reporting GATT activity.

A read observation does not by itself establish authorization, hostile intent or a completed application outcome.

## activity-drop

Does the measurement support reporting complete radio silence?

### Variant 1

{"currentObservedRecords":20,"previousObservedRecords":100,"recorderHealthKnown":false}

Observed activity fell, but twenty records remain.

The count fell from one hundred to twenty, so calling it zero activity would contradict the observation.

An observation drop does not establish deliberate suppression or predict a physical attack.

### Variant 2

{"currentObservedRecords":0,"previousObservedRecords":100,"recorderHealthKnown":false}

No records were observed in the current interval.

The current observed count is zero, but unresolved recorder health prevents a claim of environmental silence.

An observation drop does not establish deliberate suppression or predict a physical attack.

## rate-exposure

Are the observed counts evidence of a higher per-second rate?

### Variant 1

{"current":{"records":600,"seconds":60},"reference":{"records":100,"seconds":10}}

The counts differ, but both observed rates are ten records per second.

The current interval is six times longer, which explains its sixfold count without a rate increase.

A measured rate change still needs comparable coverage and context before an abnormality or intent claim.

### Variant 2

{"current":{"records":600,"seconds":10},"reference":{"records":100,"seconds":10}}

The current observed rate is six times the reference rate.

Six hundred records in ten seconds is sixty per second, compared with ten per second in the reference.

A measured rate change still needs comparable coverage and context before an abnormality or intent claim.

## mic-result

Does the Key MIC flag establish successful verification?

### Variant 1

{"independentMicVerification":"not_performed","keyMicFlag":true}

The flag is present, but verification was not performed.

A decoded flag is not a cryptographic verification result.

A verification result alone does not establish a physical sender, authorization or overall network safety.

### Variant 2

{"independentMicVerification":"passed","keyMicFlag":true}

A separate supplied verification result reports a pass.

The pass claim comes from the independent verification result, not from the flag alone.

A verification result alone does not establish a physical sender, authorization or overall network safety.
