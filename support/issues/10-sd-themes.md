# SD-card themes

Generalise Squachify into full theming: `/themes/<name>/theme.txt` plus `logo.png`, with a THEME row in Settings cycling the directories under `/themes` plus a built-in DEFAULT. Squachify then stops being a special case and becomes the theme named `squachy`.

Cheaper than it sounds: every colour already goes through a `Theme::` accessor - 264 call sites, 8 colours, no raw hex outside `Theme.h`. Returning variables instead of literals changes zero call sites.

Two constraints that shape it:

- LVGL bakes colours into styles at object-create time, so a theme must load before screens are built and switching needs a reboot. Live re-theming means rebuilding every screen, which walks into the delete-during-dispatch hazard.
- A bad theme file can brick the UI - set `background` and `white` the same and you cannot read your way back to Settings to undo it. Needs a contrast sanity check on load and a boot escape (hold a button for DEFAULT).
