# NFSMW 2005 macOS Modded Port

This is a source-based development fork of
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker), pinned to
v0.2.0 commit `312ce69796937fed118400cef6a1975c4cbe8659`. It compiles the Xbox
360 game to a native Apple Silicon executable. Windows `.asi` plugins are
not used. Preserve the upstream copyright notices, [COPYING](../COPYING),
[NOTICE](../NOTICE) and third-party license information.

## What works, and what is pending

- Native ARM64 build and isolated macOS test app: built and launched on an
  Apple M3 MacBook Air, 16 GB RAM.
- Race police and native FairCops: positive Circuit/Sprint tests, including
  a completed user race and police visibly following AI racers.
- Speedbreaker collision mass: configurable, activation-only; the user
  confirmed stronger impacts and normal handling afterward.
- Unlimited Speedbreaker: saved Game setting, on by default; no drain,
  immediate replenishment, and no minimum speed. Built, settings tests
  passed, user reported it working; live readbacks remained fully charged
  during repeated use.
- New police extensions: non-pursuit minimap visibility, racer-based single-car
  placement and distance retention, and budgets up to 2x with four racers.
  Built with initial runtime readbacks; higher populations, extra minimap art
  and cleanup regressions still need validation. See [police population](POLICE_POPULATION.md).
- Pending: matched performance comparison and investigation of reported stutters.
- Boss rewards: [All Boss Rewards](BOSS_REWARDS.md) defaults to On, allowing all
  six cards. Native build and original guest-routine integration tests cover
  the limits, labels and reward paths; a live boss defeat still needs checking.
- Career, remaining race types, long-term physics stability, roadblocks,
  save/load and heat-transition regressions still need wider validation.

This is a progress snapshot, not a finished release. See
[engineering evidence](NATIVE_MODS.md), [playtests](PLAYTEST.md),
[unlimited Speedbreaker](SPEEDBREAKER_UNLIMITED.md),
[collision mass](SPEEDBREAKER_MASS.md) and [benchmark procedure](GAMEPLAY_BENCHMARK.md).

## Required files that are not in GitHub

Clone the source first; run the remaining commands from its directory:

```sh
git clone https://github.com/arman-dogru/NFSMW_2005_MacOS_Modded_Port.git
cd NFSMW_2005_MacOS_Modded_Port
```

Supply a legally obtained backup of your own **Xbox 360 USA / NTSC-U**
*Need for Speed: Most Wanted* (2005) disc. You need the full verified extracted
game directory, including `default.xex` and the disc's other files. The PC
`speed.exe` is not compatible. The supported USA `default.xex` SHA-256 is:

```text
aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c
```

SpeedBreaker's installed game directory can be reused read-only. On macOS its
usual location is `~/Library/Application Support/SpeedBreaker/game`. The
upstream app's installer verifies a disc image and extracts the files.
Alternatively, use the upstream extraction instructions in [README](../README.md).
Place the extracted directory at `game/files`, or link it once:

```sh
mkdir -p game
ln -s "$HOME/Library/Application Support/SpeedBreaker/game" game/files
shasum -a 256 game/files/default.xex
```

Do not replace an existing `game/files` path or modify its target. Never upload
the ISO/XEX, extracted assets, generated `ppc/` code, builds/app packages,
private disassembly, saves, caches or diagnostic dumps. Those belong only on
the machine building/running the game. The repository ignores these paths;
do not force-add them. Game-data requirements are unchanged by the mods.

## Build on Apple Silicon macOS

The tested deployment floor is macOS 15.0. Install Xcode Command Line Tools
and the build tools described by the upstream README. The working toolchain
here used Homebrew LLVM/LLD 23.1.4, CMake 4.4.5, Ninja 1.13.2 and MoltenVK
1.4.2. Packaging checks the MoltenVK version and linked libraries' deployment
targets. The Objective-C++ compatibility flag for Clang 23 is feature-checked
in CMake; no global compiler downgrade is needed.

For a fresh clone, after preparing game files:

```sh
# Inspect these scripts and their downloads before running them.
scripts/setup_tools.sh
scripts/macos/build_deps.sh build/deps

mkdir -p ppc
tools/XenonRecomp/build/XenonRecomp/XenonRecomp \
  config/nfsmw.toml tools/XenonRecomp/XenonUtils/ppc_context.h

cmake -S . -B build/main -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps" \
  -DFFMPEG_XMA="$PWD/build/deps/ffmpeg-xma" \
  -DSB_TESTS=ON
ninja -C build/main -j4 SpeedBreaker
file build/main/runtime/SpeedBreaker
```

The result should be an ARM64 Mach-O executable. Existing checkouts with
completed tools, dependencies and `ppc/` should reuse them and rebuild only
changed native source. The setup script pins/patches XenonRecomp and checks
FFmpeg archives; its extract-xiso checkout is currently unpinned. No new
dependency was added for these gameplay modifications.

## Run with separate saves and settings

For this existing, built checkout, double-click
**`scripts/macos/run_modded.command`** in Finder. It enables race police,
FairCops and mass 1000, using the isolated development profile below. It
refuses a second launch while SpeedBreaker is running. Quit normally with
**Cmd+Q**. A fresh clone must first complete the build steps above.

Back up saves/settings before using any experimental build. The launcher
below uses `build/dev-profile/data` and `build/dev-profile/cache`, leaving
the installed application's profile alone. It starts a fresh development
profile; copying an existing save into it is optional and should be done
while the game is closed.

```sh
NFSMW_RACE_POLICE=1 NFSMW_POLICE_START_HEAT=2 \
NFSMW_FAIRCOPS=1 NFSMW_SB_MASS=1000 \
bash scripts/macos/run_dev.sh
```

Open native Settings with **Cmd+,** or **F1**. Under **Game**, Unlimited
Speedbreaker is saved and applies immediately. Its normal button still
toggles slow motion; turning the option off restores ordinary drain/speed
checks. `NFSMW_SB_UNLIMITED=0` disables it for one launch without changing
the saved preference. Mass accepts 0..1,000,000, defaults to stock 2 and
caps effective collision mass at 1e9 kg; high permitted values are not a
guarantee of collision-solver stability. See its dedicated document.

The same Game page saves police visibility, racer-based placement, retention
near other racers and a maximum police population multiplier (1x..2x at four
racers). These default to On/2x. The existing race-police and FairCops launch
flags still enable their respective systems. See [settings and limitations](POLICE_POPULATION.md).

**All Boss Rewards** on the same page lets you pick all six cards after a boss.
It defaults to On and is included in the updated run command's executable.

For a Finder-launchable isolated package, capture the same mod settings:

```sh
NFSMW_RACE_POLICE=1 NFSMW_POLICE_START_HEAT=2 \
NFSMW_FAIRCOPS=1 NFSMW_SB_MASS=1000 \
python3 scripts/macos/make_app.py build/main build/faircops-test \
  --test --dev-profile="$PWD/build/dev-profile"
```

Open `build/faircops-test/SpeedBreaker (test).app`. The `--test` label
distinguishes this unsigned-for-distribution development app; gameplay
features are native source changes and do not depend on that label. The
packager signs locally and embeds isolated paths for Finder launches.
An ordinary package without `--dev-profile` uses the installed app's profile;
avoid that during development. Keep `/Applications/SpeedBreaker.app` unchanged.

If a locally signed Finder launch stalls before opening a window, the isolated
`run_dev.sh` launch above is the verified fallback. One local launch blocked
while opening its log directory before reaching gameplay; the cause is not yet
established. Do not reset the installed app's profile to work around it.

## Verification and version control

```sh
ninja -C build/main -j4 fair_cops_test police_population_test speedbreaker_mass_test settings_test \
  dev_profile_test paths_test language_test
ctest --test-dir build/main \
  -R '^(fair_cops_test|police_population_test|speedbreaker_mass_test|settings_test|dev_profile_test|paths_test|language_test)$' \
  --output-on-failure
```

Logs stay in the isolated profile. [PLAYTEST.md](PLAYTEST.md) distinguishes
completed tests from pending gameplay checks. Stutter has been reported;
there is no measured matched baseline yet. Do not treat short FPS samples
or the target-balancer's microsecond timings as a full performance result.

The source history retains its SpeedBreaker base and small modification
commits. Development stays on `mods/faircops-xbox`; `main` records published
progress snapshots. Credit for the base port belongs to SpeedBreaker;
[FairCops](https://github.com/arman-dogru/NFSMW_FairCops_Mod) supplies the
balancing behavior, with other PC mods used only as behavioral references
listed in the engineering record. Xbox addresses/layouts are independently
verified; PC patch addresses are never reused.
