# futurenet-tntn

**FutureNet M02: Integrated Terrestrial–NTN Access and Handover** for ns-3.46.1.

A node that can reach the network through both a terrestrial access and a
non-terrestrial (NTN, e.g. satellite) access needs to decide which one to use
and when to switch. This module provides that decision and switching layer:

- each node gets a **manager** that periodically reads the state of its
  accesses (availability, delay, loss, rate, load),
- a replaceable **selection policy** picks the access to use,
- the manager performs a **logical handover**, with hysteresis and a minimum
  dwell time to prevent oscillation,
- every decision and handover is exposed through **trace sources** and
  written to **CSV**.

The decision layer is access-technology agnostic: an access is anything that
implements `TntnAccessStateProvider`. The module focuses on selection and
orchestration, not on detailed cellular handover signalling.

## Contents

| Class | Role |
|---|---|
| `TntnAccessState`, `TntnAccessDecision`, `TntnReason` | Common types (`model/tntn-access-types.h`) |
| `TntnAccessStateProvider` | Interface: reports the state of one access |
| `TntnMockAccessStateProvider` | Scriptable provider for testing without a real network |
| `TntnPointToPointAccessStateProvider` | Provider that reads a real ns-3 point-to-point link |
| `TntnAccessSelectionPolicy` | Policy base class; never offers an unavailable access to a subclass |
| `TntnTerrestrialPreferredPolicy` | Terrestrial if it meets delay/loss/rate constraints, else NTN |
| `TntnLowestDelayPolicy` | Lowest delay, with hysteresis |
| `TntnWeightedScorePolicy` | Highest weighted score of delay, loss, rate and load, with hysteresis |
| `TntnAccessManager` | Periodic evaluation, dwell time, handover execution, traces |
| `TntnHelper` | Installs managers on a `NodeContainer` and enables CSV tracing |

Design details: [`doc/futurenet-tntn.rst`](doc/futurenet-tntn.rst).
Validation results: [`doc/validation.md`](doc/validation.md).

## Dependencies

- ns-3.46.1
- ns-3 modules: `core`, `network`, `internet`, `point-to-point`
- Examples additionally use `applications`
- No external libraries. No other FutureNet module is required.

## Build

Place this folder at `contrib/futurenet-tntn` in an ns-3.46.1 tree, then from
the ns-3 root:

```
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

To build only what this module needs:

```
./ns3 configure --enable-examples --enable-tests \
    --enable-modules="futurenet-tntn;point-to-point;internet;applications"
./ns3 build
```

## Basic usage

```cpp
#include "ns3/tntn-helper.h"

NodeContainer nodes;
nodes.Create(25);

TntnHelper tntn;
tntn.SetPolicy("ns3::TntnTerrestrialPreferredPolicy",
               "MaxAcceptableDelay", TimeValue(MilliSeconds(150)));
tntn.SetManagerAttribute("MinDwellTime", TimeValue(Seconds(5)));
tntn.AddAccess(0, "ns3::TntnMockAccessStateProvider",
               "AccessType", EnumValue(TntnAccessType::TERRESTRIAL),
               "Delay", TimeValue(MilliSeconds(30)));
tntn.AddAccess(1, "ns3::TntnMockAccessStateProvider",
               "AccessType", EnumValue(TntnAccessType::NTN),
               "Delay", TimeValue(MilliSeconds(540)));
std::vector<Ptr<TntnAccessManager>> managers = tntn.Install(nodes);
tntn.EnableTracing("my-run");   // my-run-decisions.csv, my-run-events.csv
```

Each node gets its own manager, policy and providers; the manager is
aggregated to the node and started immediately. Use
`manager->GetAccessProvider(id)` to script a provider, and connect to the
manager's trace sources to react to handovers (e.g. to update routes).

## Examples

Run from the ns-3 root.

| Example | Shows | Command |
|---|---|---|
| `tntn-example-basic` | 25 nodes × 2 mock accesses, 60 s, random outage and congestion per node; CSV output | `./ns3 run "tntn-example-basic --numNodes=25 --simulationTime=60s"` |
| `tntn-example-integration` | Real point-to-point links; handover moves the UE's default route; per-packet path and delay | `./ns3 run tntn-example-integration` |
| `tntn-policy-demo` | One policy answering hand-made situations, printed as a table | `./ns3 run "tntn-policy-demo --policy=lowest-delay"` |
| `tntn-validation` | Validation cases V1–V3 with expected vs measured times | `./ns3 run "tntn-validation --case=all"` |

Useful options:

```
--policy=terrestrial | lowest-delay | weighted
--RngRun=2                                         # another random scenario (basic example)
--ns3::TntnAccessManager::MinDwellTime=0s          # any attribute can be set this way
--ns3::TntnLowestDelayPolicy::HysteresisFraction=0.2
```

## Tests

```
./test.py -s futurenet-tntn
```

18 test cases: SRS T01–T06, default values and attribute ranges, scheduled
mock changes, `ForceAccess`, repeatability of identical runs, an
integration test with a real ns-3 point-to-point link, and the weighted-score
formula, weight normalization and hysteresis (FR-09).

## Output

`TntnHelper::EnableTracing(prefix)` writes:

**`<prefix>-decisions.csv`** — one row per evaluation per node (SRS §7):

```
time_s,node_id,current_access,selected_access,reason,ter_delay_ms,ntn_delay_ms,ter_loss,ntn_loss,handover
```

`handover` is 1 if the evaluation started a handover, 0 otherwise (no change,
or blocked by dwell time). An empty access field means "no access".

**`<prefix>-events.csv`** — handover and availability events in the common
FutureNet schema (guidelines §4.5):

```
time_s,module,event,node_id,peer_id,flow_id,value1,value2,unit,detail
```

| event | value1 | value2 | detail |
|---|---|---|---|
| `HandoverStart` | old access | new access | reason |
| `HandoverComplete` | old access | new access | |
| `AccessUnavailable` | access | | |

The integration example also writes `<prefix>-packets.csv`
(`time_s,ue_node_id,path,seq,one_way_delay_ms`).

## Main parameters

Project defaults from SRS §5 (not 3GPP values). Full table in the design document.

| Attribute | Default | Range |
|---|---|---|
| `TntnAccessManager::DecisionInterval` | 1 s | 100 ms – 10 s |
| `TntnAccessManager::MinDwellTime` | 5 s | 0 – 60 s |
| `TntnAccessManager::HandoverExecutionDelay` | 50 ms | 0 – 2 s |
| `TntnTerrestrialPreferredPolicy::MaxAcceptableDelay` | 150 ms | 1 ms – 5 s |
| `TntnTerrestrialPreferredPolicy::MaxAcceptableLoss` | 0.10 | 0 – 1 |
| `TntnTerrestrialPreferredPolicy::MinAcceptableRate` | 1 Mbit/s | 0 – 10 Gbit/s |
| `TntnLowestDelayPolicy::HysteresisFraction` | 0.05 | 0 – 0.5 |
| `TntnWeightedScorePolicy::DelayWeight` | 0.40 | ≥ 0 |
| `TntnWeightedScorePolicy::LossWeight` | 0.20 | ≥ 0 |
| `TntnWeightedScorePolicy::RateWeight` | 0.20 | ≥ 0 |
| `TntnWeightedScorePolicy::LoadWeight` | 0.20 | ≥ 0 |
| `TntnWeightedScorePolicy::HysteresisFraction` | 0.05 | 0 – 0.5 |

Weighted-score weights are normalized by their sum, so they need not add up to 1.

## Integrating other modules

- **M01 (NTN):** implement `TntnAccessStateProvider::GetAccessState()` on top
  of the M01 satellite model and pass its TypeId to `TntnHelper::AddAccess`.
  `TntnPointToPointAccessStateProvider` is a complete example of such an adapter.
- **M07 (multi-access):** the same provider interface can wrap any path; the
  `HandoverComplete` trace tells M07 when to move traffic.
- **M10 (agent):** `TntnAccessManager::ForceAccess(id)`, `ForceEvaluation()`
  and `SetPolicy()` allow external control; `DecisionEvaluated` and the
  handover traces provide observations. A learning policy can subclass
  `TntnAccessSelectionPolicy` and implement `DoSelect()`.

## Known limitations

- **No M01 integration yet.** The NTN path in `tntn-example-integration` is a
  fixed-delay point-to-point link standing in for the M01 satellite model.
- **No `futurenet-common` package** was available, so the module defines its
  own `TntnAccessState` and writes the common event CSV schema itself.
- **Metrics are instantaneous.** Providers report the current value;
  `MetricWindow` averaging (SRS §5) is not implemented.
- **Point-to-point outages** are modelled by switching the IPv4 interface down,
  because a point-to-point device always reports its link as up.
- `TntnPointToPointAccessStateProvider` reports propagation delay only, not
  queueing or transmission delay, and loss only from a packet-unit
  `RateErrorModel` on the far end.
- Not implemented yet: make-before-break switching (FR-10, recommended).
- `TntnWeightedScorePolicy` normalizes delay and rate relative to the accesses
  being compared, so a score is only meaningful next to the others from the
  same evaluation.
- Out of scope per SRS §11: 5G RRC signalling, measurement reports,
  core-network mobility, 3GPP compliance claims, satellite PHY.
- Parameter defaults are project teaching values, not 3GPP-mandated values.
