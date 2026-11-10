> Documentation reference copy. Original repository path: `tools/ai-training/domain-data/ble-feature-knowledge-v1/FEATURE_MEANINGS.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# BLE feature meanings and limits

These describe the versioned project extractor, not incident labels. No model training occurred.

## log_observed_rate

Natural log of one plus accepted records per window-second.

This is not a raw packets-per-second value; use a unit-labelled tool measurement.

## unique_address_fraction

Distinct accepted address values divided by accepted record count.

A small positive fraction is not zero addresses or a verified count of physical devices.

## address_entropy

Shannon entropy of accepted address frequencies divided by log2 of record count, with a one-record fallback of zero.

Address concentration is not the random/public address-type flag or proof of address-generation behavior.

## new_address_fraction

Fraction of accepted records whose address is absent from the supplied previous-address set.

Zero newly observed addresses does not mean public addresses or absence of random-address flags.

## payload_repeat_fraction

One minus distinct accepted advertising-data hashes divided by accepted record count.

Hash equality is exact data equality, not approximate similarity, intent, constant length, or identity.

## random_address_fraction

Fraction of accepted records carrying the decoded random-address-type flag.

This does not measure how often an address changes or verify how an address was generated.

## manufacturer_ad_fraction

Fraction of accepted records with a parsed manufacturer-specific AD structure.

This does not authenticate a manufacturer, device, or sender.

## adv_ind_fraction

Fraction of accepted records whose legacy advertising PDU kind is 0 (ADV_IND).

The advertised capability is not proof that a connection was established.

## adv_nonconn_fraction

Fraction of accepted records whose legacy advertising PDU kind is 2 (ADV_NONCONN_IND).

Nonconnectable advertising is not a record of whether scanning occurred or a full connection history.

## scan_response_fraction

Fraction of accepted records whose legacy advertising PDU kind is 4 (SCAN_RSP).

A record fraction is not a count of unique scanners or all devices present.

## adv_scan_fraction

Fraction of accepted records whose legacy advertising PDU kind is 6 (ADV_SCAN_IND).

Scannable advertising is not an observed scan request or proof that a scanner was present.

## iat_cv

Population standard deviation of adjacent accepted-record time gaps divided by their positive mean; otherwise zero.

Variability alone is not hostility; gaps depend on capture visibility and timestamp validity.

## log_mean_iat

Natural log of one plus mean adjacent accepted-record interval in seconds, with an insufficient-interval fallback of zero.

This is not raw milliseconds; zero may reflect insufficient intervals, not instantaneous activity.

## log_max_iat

Natural log of one plus maximum adjacent accepted-record interval in seconds, with an insufficient-interval fallback of zero.

It is not a proof of environmental radio silence or a complete capture-duration measurement.

## mean_ad_data_length

Mean length in bytes of extracted accepted advertising data.

An average does not establish constant packet or payload size.

## ad_data_length_std

Population standard deviation of extracted accepted advertising-data lengths in bytes.

A positive value means lengths vary; do not call them constant.

## accepted_kind_contract

This decoder admits legacy advertising PDU kinds 0, 1, 2, 4 and 6.

Scan-request and connection-request traffic is outside these accepted features; do not infer their absence from this vector.
