# LayerTime MeshCore Stage 3

Adds working MeshCore public-channel RX/TX on the USA/Canada preset.

- Node identity name generated as `LayerTime-XXXX` from the watch MAC.
- Receives/decrypts the default MeshCore public channel.
- Shows received public messages on the Mesh screen.
- CHAT opens an on-watch keyboard and sends public-channel messages.
- Existing MeshCore advert/node discovery remains enabled.
- Footer remains gold.

This stage does not yet transmit signed node advertisements. MeshCore advertisements require a persistent Ed25519 identity/signature; that will be the next Mesh increment rather than sending invalid unsigned adverts.

Radio profile: USA/Canada 910.525 MHz / BW 62.5 kHz / SF7 / CR5.
