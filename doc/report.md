# futurenet-tntn — Technical Report

FutureNet M02: Integrated Terrestrial–NTN Access and Handover, for ns-3.46.1.

This report summarizes what was built, the main design decisions and why they
were made, how each requirement is met, and the known limitations. The
architecture and algorithms are described in
[`futurenet-tntn.rst`](futurenet-tntn.rst); validation details are in
[`validation.md`](validation.md); presentation steps are in [`demo.md`](demo.md).

## 1. Summary

The module lets an ns-3 node with a terrestrial and a non-terrestrial (NTN)
access decide which one to use and switch between them. Each node gets a
`TntnAccessManager` that, every `DecisionInterval`, reads the state of its
accesses through `TntnAccessStateProvider` objects, asks a replaceable
`TntnAccessSelectionPolicy` for a decision, and performs a logical handover
subject to a minimum dwell time. Decisions and handovers are exposed as trace
sources and written to CSV by `TntnHelper`.

Delivered:

- 3 policies: terrestrial-preferred, lowest-delay (hysteresis), weighted score (hysteresis)
- 2 providers: a scriptable mock and an adapter for real ns-3 point-to-point links
- Manager with dwell time, handover execution delay, optional make-before-break,
  4 trace sources and an external control interface (`ForceAccess`,
  `ForceEvaluation`, `SetPolicy`)
- Helper installing everything on a `NodeContainer`, with two CSV outputs
- 4 examples, 19 test cases, 3 validation cases (all passing)

## 2. Design decisions

| # | Decision | Reason |
|---|---|---|
| 1 | **Policy decides, manager acts.** A policy is a stateless function of (access states, current access). The manager owns time (evaluation timer, dwell, execution delay), state and traces. | Guidelines §1.3 mechanism/policy separation. A policy can be replaced at run time (e.g. by M10) and unit-tested without a simulation. |
| 2 | **The base policy filters unavailable accesses** before calling `DoSelect()`, and aborts if a subclass returns an access it was not offered. | Guarantees SRS T01 for every policy, including ones written later by other teams. |
| 3 | **Hysteresis lives in the policy; dwell time in the manager.** | Hysteresis depends on the policy's metric (delay, score); dwell depends on a clock. Each is placed where its information is. |
| 4 | **Dwell time never blocks leaving an unavailable access**, the initial selection or a forced switch. | Waiting would leave the node with no working access or ignore an explicit command. |
| 5 | **Break-before-make by default; make-before-break only if enabled and the old access still works.** | SRS §5 default `MakeBeforeBreak = false`; FR-10 applies "when both accesses are concurrently available". A failed access cannot carry traffic. |
| 6 | **Forwarding is updated by the user through `HandoverStart`/`HandoverComplete`**, not inside the manager. | Keeps the manager technology-agnostic. The integration example shows the route update for IPv4 point-to-point links. |
| 7 | **All public names prefixed `Tntn`** (`TntnAccessStateProvider`, …). | M07 is likely to define `AccessStateProvider` / `AccessSelectionPolicy`; avoids collisions in the combined build (QG-08). |
| 8 | **Local `TntnAccessState`; common CSV schema written directly.** | No `futurenet-common` package was supplied. Field meanings and units follow guidelines §4. |
| 9 | **One manager, policy and set of providers per node**, built from `ObjectFactory` objects and aggregated to the node. | No shared mutable state between nodes (CG-08); per-node control for M10. |
| 10 | **Weighted-score marks are relative** for delay and rate (best/this, this/best), absolute for loss and load (1 − value). Weights are divided by their sum. | Delay and rate have no natural upper bound; SRS §5 allows normalizing weights internally. |
| 11 | **Deterministic tie-breaking** (first access in list order) in all policies; outage scenarios use ns-3 `RandomVariableStream`. | Reproducible runs (CG-09, CG-10, guidelines §8.4). |
| 12 | **`NS_ABORT_MSG_IF` for input validation** rather than `NS_ASSERT`. | Assertions are removed in optimized builds; invalid input must always fail clearly (CG-11). |
| 13 | **`DecisionEvaluated` carries the reason** in addition to the SRS signature. | The decisions CSV needs the reason column (SRS §7). |
| 14 | **Point-to-point outages are modelled with `Ipv4::SetDown`.** | A point-to-point device reports its link as up whenever a channel is attached. |

## 3. Requirement compliance

### Functional requirements (SRS §4)

| Req | Level | Status | Where | Evidence |
|---|---|---|---|---|
| FR-01 ≥ 2 candidate accesses | M | Met | `TntnAccessManager::AddAccess` | all manager tests, examples |
| FR-02 availability, delay, loss, rate, load | M | Met | `TntnAccessState` | provider tests, T6 |
| FR-03 periodic evaluation | M | Met | `DecisionInterval` | T05/T06, V1 |
| FR-04 terrestrial-preferred with constraints | M | Met | `TntnTerrestrialPreferredPolicy` | T02, V3 |
| FR-05 lowest-delay policy | M | Met | `TntnLowestDelayPolicy` | T03, V3 |
| FR-06 hysteresis and minimum dwell | M | Met | policies' `HysteresisFraction`, `MinDwellTime` | T04, T05, V2, V3 |
| FR-07 logical handover updates access state | M | Met | `StartHandover`/`CompleteHandover`; route update in integration example | T05/T06, integration example |
| FR-08 reason and old/new ids | M | Met | `HandoverStart`, `HandoverComplete` | T06 |
| FR-09 weighted score, normalized weights | R | Met | `TntnWeightedScorePolicy` | 4 FR-09 tests |
| FR-10 make-before-break | R | Met | `MakeBeforeBreak` | FR-10 test; integration example 70 vs 55 lost packets |
| FR-11 prediction/learning policy | O | Not implemented | extension point documented | — |

### Tests and validation (SRS §9)

| Item | Status | Test / case |
|---|---|---|
| T01 unavailable access never selected | Pass | `T01 unavailable access is never selected` |
| T02 terrestrial-preferred rule | Pass | `T02 terrestrial-preferred policy follows its rule` |
| T03 lowest-delay minimum | Pass | `T03 lowest-delay policy selects minimum eligible delay` |
| T04 hysteresis blocks small gains | Pass | `T04 …`, `Weighted-score hysteresis …` |
| T05 dwell blocks early return | Pass | `T05/T06 …` |
| T06 traces report old/new | Pass | `T05/T06 …` |
| V1 terrestrial failure → NTN | Pass | 25 nodes, max latency 1.03 s (bound 1.05 s) |
| V2 restore follows dwell | Pass | returns at 13.05/14.05/17.05/22.05 s for dwell 0/2/5/10 s |
| V3 threshold crossing | Pass | 6 configurations switch exactly where predicted |

### Acceptance criteria (SRS §12)

| AC | Status | Note |
|---|---|---|
| AC-01 two-access switching with mock providers | Met | `tntn-example-basic`, manager tests |
| AC-02 integrates with M01 through a public adapter/provider | **Partly met** | The public provider interface and a complete reference adapter (`TntnPointToPointAccessStateProvider`) exist. An M01 adapter was not written because M01 code was not available. |
| AC-03 at least two baseline policies | Met | three policies |
| AC-04 hysteresis and dwell tested | Met | T04, T05, V2, V3 |
| AC-05 handover events traceable, machine-readable | Met | trace sources, two CSV files |
| AC-06 no access to another module's private members | Met | no dependency on other FutureNet modules |

### Parameters (SRS §5)

| Parameter | Status |
|---|---|
| DecisionInterval, MinDwellTime, HandoverExecutionDelay, MakeBeforeBreak | Manager attributes, SRS defaults and ranges |
| HysteresisFraction | Attribute of the lowest-delay and weighted-score policies (where the compared metric is known) |
| MaxAcceptableDelay, MaxAcceptableLoss, MinAcceptableRate | Terrestrial-preferred policy attributes |
| Delay/Loss/Rate/LoadWeight | Weighted-score policy attributes |
| TerrestrialPreference | Expressed by choosing `TntnTerrestrialPreferredPolicy` rather than a boolean |
| MetricWindow | **Not implemented** — providers report instantaneous values |

### Quality gate (guidelines §18)

| Gate | Status |
|---|---|
| QG-01 clean build | Module builds without warnings; full ns-3 build checked before submission |
| QG-02 mandatory tests pass | 19/19 |
| QG-03 basic example runs unmodified | `./ns3 run tntn-example-basic` |
| QG-04 configurable parameters | all model parameters are attributes; examples expose CommandLine options |
| QG-05 machine-readable traces | CSV |
| QG-06 documented public API | Doxygen comments on all public classes and methods |
| QG-07 no ns-3 core changes | module lives in `contrib/` only |
| QG-08 no name collisions | `Tntn` prefix |
| QG-09 ≥ 3 validation cases | V1–V3 |
| QG-10 limitations documented | README, design document, section 5 below |

## 4. Results

- **Scale** (SRS §10): 25 nodes × 2 accesses, 60 s, in `tntn-example-basic`.
  With terrestrial-preferred: 25 initial selections, 24 fallbacks on outage,
  18 on congestion, 41 returns; 24/25 nodes on terrestrial at the end.
- **Policies differ as designed** (`tntn-policy-demo`): with terrestrial 100 ms
  and 30 % loss against NTN 97 ms, terrestrial-preferred leaves on loss,
  lowest-delay stays (it ignores loss), weighted score leaves on overall score.
- **Real traffic** (`tntn-example-integration`, 10 ms packets): one-way delay
  16 ms on terrestrial, 271 ms on NTN. 70 of 5900 packets lost per UE with
  break-before-make, 55 with make-before-break; the 55 are the 0.55 s gap
  between the terrestrial outage and the completed fallback, the 15 saved are
  three optional 50 ms handovers.
- **Validation**: every measured handover time equals its closed-form value.

## 5. Limitations and future work

| Limitation | Possible future work |
|---|---|
| No M01 integration; the NTN path is a fixed-delay point-to-point link | Write an M01 provider once its public API is available; only the provider changes |
| Instantaneous metrics; no `MetricWindow` | Averaging provider wrapper (e.g. EWMA or sliding window) around any provider |
| Point-to-point provider reports propagation delay only, and loss only from a packet-unit `RateErrorModel` | Measure delay/loss from traffic (e.g. FlowMonitor or probe packets) |
| Weighted scores are relative to the accesses compared in one evaluation | Optional absolute reference values per metric |
| Forwarding update is left to the user (shown for static IPv4 routing) | Optional helper for common routing setups; integration with M07 |
| No prediction/learning policy (FR-11, optional) | Subclass `TntnAccessSelectionPolicy`; M10 can drive the manager through `SetPolicy`/`ForceAccess` |
| Out of scope (SRS §11): RRC signalling, measurement reports, core-network mobility, satellite PHY, 3GPP compliance | — |

Default parameter values are project teaching values (SRS §5), not
3GPP-mandated values.

## 6. Development record

Development followed the `main` / `develop` / `feature/<name>` workflow with
reviewed pull requests: access selection policies, access manager, helper and
CSV tracing, validation, integration example, documentation, weighted-score
policy, make-before-break. Validation runs are recorded in
[`validation.md`](validation.md) (ns-3.46.1, default seed and run, commands
and platform).
