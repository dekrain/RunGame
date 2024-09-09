This is a recreation of a popular web browser game "Run"

To compile using `compile.sh` script you must provide a `GLEW_PATH` environment variable pointing to GLEW library root directory.

## Controls
General:
-   [`B`] Change between modes

While in editor mode (default):
-   [`W`] Move forward
-   [`S`] Move backward
-   [`Space`] Place/remove tile
-   [`PageUp`] Append sector/segment after current position
-   [`PageDown`] Delete sector/segment at current position and move back
-   [`Insert`] Insert sector/segment before current position
-   [`Delete`] Delete sector/segment at current position and move forward
-   [`P`] Save current level to `level.dat`
-   [`L`] Load current level from `level.dat`
-   [_Arrow keys_] Select tile
-   [`m`] Toggle selection mode

While in playing mode:
-   Currently nothing (change state to reset game)

## Misc.
To change the shape of the level (including number of floors and number of tiles per floor), edit the `run.hpp` file, by changing the `num_floors` and `num_floor_planes` constants respectively (don't modify the `num_slots` constant!). The default level `sLevelData` inside `main.cpp` must be changed accordingly to fit the desired layout (the total numbers of tiles per section in the array must match to preserve well-defined behaviour of level loader).

Default "checkerboard" level is included in the `level.dat` file, but it can be overwritten.
