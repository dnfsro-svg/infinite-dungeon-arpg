# Stage 12 comic material pack validation

Run `ctest --test-dir out/build/windows-msvc-debug -R stage12 --output-on-failure`.
The formal runner creates its own evidence directory, copies the three built-in atlases next to its validation executable, runs ordinary raylib gameplay at 1280x720 and 1920x1080, and records both PNGs plus `stage12-material-evidence.txt`.

The validator rejects missing/wrong PNG dimensions, a missing manifest validation, texture memory above 64 MiB, missing fallback record, missing input/hole regression record, missing screenshot isolation, or fewer than eight monster entries. F12 remains backward compatible by default; `--screenshot-dir <directory>` isolates its legacy filename for concurrent runs.
