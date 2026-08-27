# LayerTime display timeout fix

- Turns the AMOLED panel off after 15 seconds without touch input.
- Wakes the panel from either a touchscreen interaction or a short crown press.
- Keeps the application, BLE weather, GPS, mesh, and recon services running while the panel is off.
- Restores the brightness selected in LayerTime Settings after wake.

This uses the T-Watch Ultra's verified `sleepDisplay()` and `wakeupDisplay()` APIs rather than deep sleep, so active services are not torn down.
