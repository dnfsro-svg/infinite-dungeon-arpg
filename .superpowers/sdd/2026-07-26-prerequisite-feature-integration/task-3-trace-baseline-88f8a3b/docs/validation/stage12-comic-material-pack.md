# Stage 12 comic material pack validation

Run `ctest --test-dir out/build/windows-msvc-debug -R stage12 --output-on-failure`.
The formal runner accepts only the CTest build evidence parent named `stage12 material evidence`, then clears only its owned `stage12-run` child. It rejects project/workspace/temp parents and leaves their sentinels untouched. It copies the three built-in atlases next to its validation executable, runs ordinary raylib gameplay at 1280x720 and 1920x1080, and records both PNGs plus `stage12-material-evidence.txt`.

The validator rejects missing/wrong PNG dimensions, undecodable-by-runner screenshots, a missing manifest validation, texture memory above 64 MiB, missing corrupted-resource fallback, missing Stage10 input/hole descent evidence, missing isolated F12 output, or a missing eight-monster renderer-showcase frame. F12 remains backward compatible by default; `--screenshot-dir <directory>` isolates its legacy filename for concurrent runs.
