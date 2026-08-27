# LayerTime BLE Weather Bridge

This overlay adds a BLE GATT service so a phone companion can push current temperature to LayerTime.

## GATT service

- Device name: `LayerTime`
- Service UUID: `7e2a0001-6c74-4d54-8d47-4c4159455254`
- Temperature write characteristic: `7e2a0002-6c74-4d54-8d47-4c4159455254`
- Status read characteristic: `7e2a0003-6c74-4d54-8d47-4c4159455254`

Write temperature in Celsius as UTF-8 text, for example:

```text
23.4
```

or:

```text
T=23.4
```

The watch converts Celsius to Fahrenheit when LayerTime is using Imperial units. Weather expires after 30 minutes without an update and returns to `WX -- F/C`.

## Test before the phone companion exists

A generic BLE GATT client such as nRF Connect can connect to `LayerTime` and write `23.4` to the temperature characteristic. The watch face should then show approximately `WX 74 F` in Imperial mode.

This overlay also preserves the existing NimBLE stack for Recon BLE scans instead of attempting to initialize NimBLE a second time.
