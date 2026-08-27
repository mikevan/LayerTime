# LayerTime GPS Holding Page

Project-root overlay. Extract directly into the LayerTime project root.

Replaces only:
- `src/ui/GpsScreen.cpp`
- `src/ui/GpsScreen.h`

The page uses the GNSS data already present in `WatchState` and adds an in-page WGS84-to-UTM conversion. No map engine or SD-card dependency is introduced yet.

Layout:
- Back button: upper left
- GPS/GNSS values: stacked top-down on the right
- UTM zone/easting/northing: lower left

The footer is gold.
