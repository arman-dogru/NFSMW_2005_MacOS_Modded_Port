#!/usr/bin/env python3
"""Local USA guest integration test. Generated game code stays under ignored build/.

Run after building SpeedBreaker:
  python3 tests/boss_rewards_guest_test.py build/main
Uses the actual compiled native hook and original generated selection/count/redraw
bodies; engine UI and boss-car delivery are mocked by the companion harness.
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
build = pathlib.Path(sys.argv[1]).resolve()
out = build / "tests/boss-rewards-guest"
if not out.is_relative_to(ROOT / "build"):
    raise SystemExit("Guest test output must remain in this checkout's ignored build directory")
out.mkdir(parents=True, exist_ok=True)
sources = {}
for name in ("164", "191"):
    sources[name] = (ROOT / f"ppc/ppc_recomp.{name}.cpp").read_text()
parts = ['#include "ppc_recomp_shared.h"\n']
for source, name in [("164", "sub_82586768"), ("164", "sub_82586E28"),
                     ("164", "sub_82586E78"), ("191", "__savegprlr_27"),
                     ("191", "__savegprlr_28"), ("191", "__restgprlr_27"),
                     ("191", "__restgprlr_28")]:
    text = sources[source]
    start = text.index(f"PPC_FUNC_IMPL(__imp__{name})")
    end = text.index("\n}\n", text.index(f"PPC_WEAK_FUNC({name})", start)) + 3
    parts.append(text[start:end])
generated = out / "guest.cpp"
generated.write_text("\n".join(parts))
cache = (build / "CMakeCache.txt").read_text()
compiler = re.search(r"^CMAKE_CXX_COMPILER:(?:FILEPATH|STRING)=(.+)$", cache, re.M)
if not compiler:
    raise SystemExit("Configured C++ compiler not found")
hook = build / "runtime/CMakeFiles/SpeedBreaker.dir/game/boss_rewards.cpp.o"
exe = out / "boss_rewards_guest_test"
subprocess.run([compiler[1], "-std=c++23", "-O2", "-UNDEBUG", "-DSIMDE_ENABLE_NATIVE_ALIASES",
                "-I" + str(ROOT / "ppc"), "-I" + str(ROOT / "runtime"),
                "-I" + str(ROOT / "tools/XenonRecomp/XenonUtils"),
                "-I" + str(ROOT / "tools/XenonRecomp/thirdparty/simde"),
                str(generated), str(ROOT / "tests/boss_rewards_guest_test.cpp"),
                str(hook), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
