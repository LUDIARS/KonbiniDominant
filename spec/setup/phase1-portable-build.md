# Phase 1 Windows ZIP

## Build

Use CMake 3.28+, Visual Studio 2022 x64 and the Vulkan SDK with glslc.
Dependencies stay pinned to the revisions in the repository.

```powershell
cmake -S . -B build-native -G "Visual Studio 17 2022" -A x64 -DKONBINI_BUILD_TESTS=OFF
cmake --build build-native --config Release --target konbini_dominant --parallel 4
```

The executable is emitted in `build-native/Release/`. The app asset target copies
all six required SPIR-V files to `shaders/` next to the executable. The historical
Phase 1 packager places `data/content/phase1.json` in the archive as
`data/content/first-playable.json`. These are executable-relative defaults.
Explicit KONBINI_SHADER_DIR / KONBINI_CONTENT_FILE overrides still take precedence.
No source checkout, working directory or developer's absolute asset paths are required.

Package the executable, these two asset directories, available dependency license
notices and the included playing instructions. MSVC uses the static release CRT,
so the package needs no separately installed VC redistributable. Inspect the PE
imports before delivery to confirm that only Windows and Vulkan driver DLLs remain.
Do not package the Vulkan SDK/validation layers or build/intermediate files. The
recipient uses the Vulkan runtime supplied by their graphics driver.

Build success does not establish startup or gameplay success. This task does not
run the executable or any tests. Development startup verification, when authorized,
is performed only by Excubitor from the main project directory.

After committing the source, create the archive without launching the game:

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
python tools/package_phase1.py --build-dir build-native --revision (git rev-parse HEAD)
```

The packager includes `build-native/notices/*.txt` when supplied. Copy applicable
third-party notices there before running it. A successful executable link writes a
revision stamp; packaging requires that stamp, the requested revision, a clean
worktree and repository HEAD to agree. It records the commit and asset hashes in
BUILD_INFO.json and verifies normal and delay-load PE imports plus archive contents.
The archive is created beside the Release directory and an existing archive is never
overwritten.

## Controls and rules

1 / 2 / 3: choose a chain. Left click selects a lot, a second click confirms
construction. Right click / Escape cancels. WASD / middle drag pans. Wheel zooms.
P pauses the simulation; F1 toggles help. R retries after Result.

Start with funds for five stores. Capture customers through store influence,
reinvest the regular income, and form triangles with three nearby stores.
Triangle vertices gain a 1.5x income multiplier (after base income rounding).
Enemy stores enclosed continuously for three seconds are destroyed. The warning
arc empties as the deadline approaches. Breaking the enclosing triangle cancels
its warning. Destroyed lots can be rebuilt.

Destroy every rival store and capture 60 percent of the current population to win.
At five minutes, the largest customer population wins; a tie for first involving
the player is a draw. Having no store and insufficient rebuilding funds is a loss.
The other two chains accelerate their construction from every six seconds to every
two seconds over four minutes. Numbers are tunable in content version 3.
