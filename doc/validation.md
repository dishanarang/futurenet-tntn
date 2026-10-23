# futurenet-tntn — Validation Results (V1–V3)

Validation asks whether the modelled behaviour is sensible (guidelines §10).
Each case below compares the simulated handover time against a closed-form
expectation derived from the configured parameters. All cases pass.

## How to reproduce

```
./ns3 configure --enable-examples --enable-tests
./ns3 build
./ns3 run "tntn-validation --case=all"
```

The program exits with status 0 only if every case passes. It writes:

| File | Content |
|---|---|
| `tntn-validation-v1.csv`, `-v2.csv`, `-v3.csv` | Summary: expected vs measured, pass flag (copies in [`validation/`](validation/)) |
| `tntn-validation-*-raw-decisions.csv`, `*-raw-events.csv` | Raw helper traces of every run (22 files, not committed; regenerate with the command above) |

### Run record (guidelines §8.3)

| Item | Value |
|---|---|
| ns-3 version | ns-3.46.1 |
| Module code | `develop` at `5737a4e` + `examples/tntn-validation.cc` (branch `feature/validation`) |
| Seed / run | Default (`RngSeed=1`, `RngRun=1`); the validation scenarios use no random variables |
| Command line | `./ns3 run "tntn-validation --case=all"` |
| Platform | macOS 26.5, Apple clang 15.0.0 |
| External tools | None |

## Common setup

- Each node has a terrestrial access (id 0) and an NTN access (id 1), both
  `TntnMockAccessStateProvider`.
- `DecisionInterval` = 1 s, evaluations at whole seconds from 0 s.
- `HandoverExecutionDelay` = 50 ms: a handover started at tick *t* completes at *t* + 0.05 s.
- Scenario changes are placed between ticks, so the order of same-time events
  never affects the result.

Helper used in the formulas: `next(x)` = first evaluation tick at or after
time *x*, i.e. `ceil(x)` for a 1 s interval.

## V1 — Terrestrial failure moves nodes to NTN

**Scenario.** 25 nodes, terrestrial-preferred policy, default dwell (5 s).
Node *i* loses terrestrial at 10.30 + 0.03·*i* s, so failures are spread over a
full evaluation interval (10.30–11.02 s).

**Expected.** A switch away from an unavailable access is never delayed by
dwell time, so the switch completes at `next(failure) + 0.05`. The latency
is therefore bounded by `DecisionInterval + HandoverExecutionDelay` = 1.05 s.

**Result.** All 25 nodes moved to NTN at exactly the expected time.
Latency ranged from 0.06 s to 1.03 s (bound 1.05 s). **PASS**

| node_id | failure_s | expected_switch_s | measured_switch_s | latency_s |
|---|---|---|---|---|
| 0 | 10.30 | 11.05 | 11.05 | 0.75 |
| 12 | 10.66 | 11.05 | 11.05 | 0.39 |
| 23 | 10.99 | 11.05 | 11.05 | 0.06 |
| 24 | 11.02 | 12.05 | 12.05 | 1.03 |

Full table: [`validation/tntn-validation-v1.csv`](validation/tntn-validation-v1.csv).

## V2 — Restoring terrestrial follows the dwell time

**Scenario.** One node, terrestrial-preferred policy. Terrestrial fails at
10.3 s (switch to NTN completes at 11.05 s) and is restored at 12.5 s.
`MinDwellTime` is varied: 0, 2, 5, 10 s.

**Expected.** Returning to terrestrial (`PREFERRED_RESTORED`) is an optional
switch, so it waits until the dwell time has elapsed since the last handover:
`return = next(max(12.5, 11.05 + MinDwellTime)) + 0.05`.
Exactly three handovers should occur (initial, fallback, return); any more
would mean oscillation.

**Result.** **PASS** for every dwell value; return time grows with dwell time
exactly as predicted, and no extra handovers occurred.

| MinDwellTime (s) | expected return (s) | measured return (s) | handovers |
|---|---|---|---|
| 0 | 13.05 | 13.05 | 3 |
| 2 | 14.05 | 14.05 | 3 |
| 5 | 17.05 | 17.05 | 3 |
| 10 | 22.05 | 22.05 | 3 |

## V3 — Delay crossing a threshold switches only when the policy permits

**Scenario.** One node. Terrestrial delay ramps from 100 ms at 0 s by
+10 ms per second (value at tick *t* = 100 + 10·*t* ms); NTN delay is fixed at
250 ms. `MinDwellTime` = 0 so the switch time depends on the policy alone.

**Expected.**
- Terrestrial-preferred leaves terrestrial at the first tick where the delay
  exceeds `MaxAcceptableDelay` (a delay equal to the limit is acceptable).
- Lowest-delay leaves terrestrial at the first tick where
  NTN < TER and NTN ≤ TER · (1 − `HysteresisFraction`).

**Result.** **PASS** for all six configurations. The two policies switch at
different delays for the same ramp, and a larger hysteresis delays the switch.

| policy | parameter | value | TER delay at switch (ms) | expected (s) | measured (s) |
|---|---|---|---|---|---|
| terrestrial-preferred | MaxAcceptableDelay | 100 ms | 110 | 1.05 | 1.05 |
| terrestrial-preferred | MaxAcceptableDelay | 150 ms | 160 | 6.05 | 6.05 |
| terrestrial-preferred | MaxAcceptableDelay | 200 ms | 210 | 11.05 | 11.05 |
| lowest-delay | HysteresisFraction | 0 | 260 | 16.05 | 16.05 |
| lowest-delay | HysteresisFraction | 0.05 | 270 | 17.05 | 17.05 |
| lowest-delay | HysteresisFraction | 0.2 | 320 | 22.05 | 22.05 |

With `HysteresisFraction` = 0.05 the switch needs TER ≥ 250 / 0.95 ≈ 263 ms,
so it happens at 270 ms rather than at the first value above 250 ms.

## Mapping to requirements

| Case | Covers |
|---|---|
| V1 | FR-03 periodic evaluation, FR-04 fallback to NTN, FR-07 logical handover, AC-01 |
| V2 | FR-06 minimum dwell time, AC-04 |
| V3 | FR-04 configurable constraints, FR-05 lowest-delay, FR-06 hysteresis, AC-04 |

Guideline §10 categories: V1 baseline/sanity, V2 controlled variation of one
key parameter, V3 boundary behaviour around a threshold.

## Limitations

- Access metrics come from mock providers, so these cases validate the
  decision and handover logic, not the realism of the delay values.
- Validation uses a 1 s decision interval and 50 ms execution delay; the
  formulas generalise to other values (`next(x)` becomes the next multiple of
  `DecisionInterval`).
