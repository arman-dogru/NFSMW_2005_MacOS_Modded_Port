# Shared native gameplay validation

Use `build/faircops-test/SpeedBreaker (test).app`. This package uses
`build/dev-profile/data` and `build/dev-profile/cache`. The installed app,
original saves and installed game files are left in place.

Prepared settings: police enabled, original heat behavior, maximum density, three-second initial
spawn delay; FairCops enabled with rebalance every 30 manager updates,
undersupply rotation every 600 updates and static roadblocks excluded;
Speedbreaker mass 1000. The normal runtime default mass remains 2.
Unlimited Speedbreaker is now a saved setting under Settings > Game, enabled
by default on this branch. Cmd+, opens native Settings on macOS.

With four racers (including the human), the ground-cop budget multiplier defaults
to 2x. This does not multiply heat. Earlier November 1 tests used an explicit
heat-2 development override; it was removed from the implementation on November 4.

The three-second setting makes spawning eligible; road placement and pursuit
engagement can take additional time. FairCops distributes active pursuit cops,
not inactive pool vehicles. Formal pursuit membership and AI bust/DNF rules
remain the game's own.

Start with Quick Race > Custom Race > Circuit, three opponents, three laps.
Ironwood Estates and the stock Lexus IS300 provide a repeatable reference.

1. Confirm ordinary steering/throttle/braking work and releasing controls has
   the expected effect. Report any unexplained autonomous input.
2. Drive for at least a minute. Note when police first appear and whether any
   cops follow or hit AI racers. Logs independently record each racer's assigned
   and actual cop-target counts.
3. Compare a traffic/cop impact with Speedbreaker off and on. At 1000 the effect
   should be conspicuous. Note controllability, launches, jitter or crashes.
4. After Speedbreaker ends, check normal driving and collisions again. Logs
   should show the override clear to zero with the base mass unchanged.
5. Restart and repeat one activation, then return to the menus. Check for old
   assignments, stuck physics or a transition crash.

For unlimited Speedbreaker: activate while stopped, leave it on at least 30
seconds, then switch it off and immediately on again without driving to
recharge. The meter should stay full. Turn Unlimited Speedbreaker off in
Settings > Game and check normal depletion and the speed requirement, then
restore On. Its charge behavior and the collision-mass setting are independent.

After this pass: Sprint, Lap Knockout, Speedtrap, Tollbooth, Drag and Career
events; zero-opponent events; heat changes and roadblocks; destruction;
Save/Load; and a run with all modifications disabled. These are separate tests,
not implied by the earlier Circuit/Sprint successes.

Logs stay local in the development profile's `logs/` folder. The directly
launched shared run also writes `build/diagnostics/user-validation-2026-11-01.log`.
For feedback, report the race/course, car, approximate race time and what
happened. No game files or save uploads are needed.

## User playtest, 2026-11-01

Build `0f545a8`, completed Circuit race with a Golf GTI and three AI opponents
(course not recorded): the user reported substantially stronger Speedbreaker
impacts, ordinary handling returning afterward, and police clearly following
AI opponents away from the player. Police and collisions worked through the
finish. The user also reported occasional stutter/FPS drops. This validates
the observed behavior in that run, not all modes or a performance comparison.

The local session `2026-11-01_10-06-52.log` independently recorded Golf base
mass 1328 kg, Speedbreaker override 1328000 kg and zero after deactivation.
In a captured sample, five active cops were distributed 2/1/1/1 across four
racers with matching actual targets and zero failed target acquisitions.
The log contains real long-frame events, but includes loading/menu/pause and
interactive inspection. No matched unmodified run has been measured yet;
their cause cannot be attributed to the mods from this session alone.

The user completed a second race on build `69dbb89`, reported unlimited
Speedbreaker working, and still experienced stutter (possibly around cop
groups). Session `2026-11-01_10-45-32.log` contains 84 charge readbacks, all 1.0,
through repeated activation/deactivation. This confirms the sampled full charge;
it is not a timed 30-second activation test or matched performance comparison.
The spawning hypothesis remains unproven. Original save/settings hashes were
checked again after the initial population smoke run and remained unchanged.

## Race controls smoke test, 2026-11-09

In the isolated ARM64 desktop build, a four-racer Ironwood Estates Circuit in a
stock Lexus IS 300 ran with Race Police Off and no cops in the manager during
the observed window. F6 engaged Auto Drive, the Lexus accelerated and steered
without input, and a second press released control. In a second race with
police enabled, the user toggled Auto Drive both ways on a Bluetooth DualSense.
The runtime recorded both handoffs and a ground-cop manager ceiling of 8 → 16
for four starting racers. Split-screen behavior, race restarts, and matched
performance runs still need testing.

A scripted headless Circuit loaded with Starting Heat 3 and read back 3.00
from the stock world-heat interface. Police became eligible after the opening
sequence. This verifies initialization for one local driver.
