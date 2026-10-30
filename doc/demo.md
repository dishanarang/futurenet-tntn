# futurenet-tntn — Demonstration Instructions

A 10–15 minute live demonstration. Every command runs from the ns-3 root
(`ns-allinone-3.46.1/ns-3.46.1`). Expected output is shown below each step;
the numbers are deterministic for the default seed and run.

## 0. Before the demo

```
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

Build once beforehand so the demo does not wait on compilation. Have
`contrib/futurenet-tntn/README.md` open to show the class overview.

## 1. The idea (1 min, no commands)

A node has a terrestrial access (fast, can fail or congest) and an NTN access
(slow, always there). Every second its manager asks a policy which one to use
and performs a handover, with hysteresis and a dwell time so it does not
oscillate. Policies are interchangeable.

## 2. Tests pass (1 min)

```
./test.py -s futurenet-tntn
```

```
[1/1] PASS: TestSuite futurenet-tntn
1 of 1 tests passed (1 passed, 0 skipped, 0 failed, 0 crashed, 0 valgrind errors)
```

To list the 19 individual test cases:

```
./build/utils/ns3.46.1-test-runner-default --test-name=futurenet-tntn --verbose
```

Point out: T01–T06 from the SRS, FR-09 and FR-10 tests, the repeatability
test, and the T6 integration test on a real ns-3 link.

## 3. Three policies, same situations (2 min)

```
./ns3 run "tntn-policy-demo --policy=terrestrial"
./ns3 run "tntn-policy-demo --policy=lowest-delay"
./ns3 run "tntn-policy-demo --policy=weighted"
```

Point to these two rows:

| Situation | terrestrial | lowest-delay | weighted |
|---|---|---|---|
| TER gets slow (200 ms > 150 ms limit) | NTN, `DELAY_EXCEEDED` | TER (still faster than 540 ms) | TER |
| TER 30% loss, NTN 3% faster | NTN, `LOSS_EXCEEDED` | **TER** (ignores loss; 3% < 5% hysteresis) | NTN, `BETTER_CANDIDATE` |

Talking point: same input, different decisions; the manager does not change.

## 4. 25 nodes for 60 s with CSV output (3 min)

```
./ns3 run tntn-example-basic
```

```
Nodes: 25, policy: terrestrial, run: +60s
Handovers by reason:
  CURRENT_UNAVAILABLE: 24
  DELAY_EXCEEDED: 18
  INITIAL_SELECTION: 25
  PREFERRED_RESTORED: 41
Nodes on terrestrial at end: 24/25
CSV: tntn-example-basic-decisions.csv, tntn-example-basic-events.csv
```

Show one node's story from the CSV (dwell time in action):

```
awk -F, 'NR==1 || ($2==5 && ($10==1 || $5!="NONE"))' tntn-example-basic-decisions.csv
```

```
time_s,node_id,current_access,selected_access,reason,ter_delay_ms,ntn_delay_ms,ter_loss,ntn_loss,handover
0,5,,0,INITIAL_SELECTION,30,540,0.01,0.02,1
17,5,0,1,CURRENT_UNAVAILABLE,30,540,0.01,0.02,1
20,5,1,0,PREFERRED_RESTORED,30,540,0.01,0.02,0
21,5,1,0,PREFERRED_RESTORED,30,540,0.01,0.02,0
22,5,1,0,PREFERRED_RESTORED,30,540,0.01,0.02,0
23,5,1,0,PREFERRED_RESTORED,30,540,0.01,0.02,1
53,5,0,1,DELAY_EXCEEDED,200,540,0.01,0.02,1
```

Talking points:
- 17 s: terrestrial fails → immediate fallback (dwell never blocks this).
- 20–22 s: the policy wants terrestrial back, but `handover=0`: blocked by
  the 5 s dwell time since the 17.05 s handover.
- 23 s: dwell elapsed → return.
- 53 s: terrestrial congested at 200 ms → `DELAY_EXCEEDED`.

Optional: change any attribute from the command line, e.g. turn dwell off and
see more returns:

```
./ns3 run "tntn-example-basic --ns3::TntnAccessManager::MinDwellTime=0s"
```

(`PREFERRED_RESTORED` rises from 41 to 44.)

## 5. Real ns-3 links and real packets (3 min)

```
./ns3 run tntn-example-integration
./ns3 run "tntn-example-integration --makeBeforeBreak=true"
```

```
UEs: 3, policy: terrestrial, makeBeforeBreak: false
  UE 0: sent 5900, received 5830, lost 70
...
UEs: 3, policy: terrestrial, makeBeforeBreak: true
  UE 0: sent 5900, received 5845, lost 55
```

Show the moment packets change path (UE 0; run the default command last so
the CSV matches):

```
./ns3 run tntn-example-integration
awk -F, '$2==0 && $1>20.48 && $1<21.35' tntn-example-integration-packets.csv
```

```
20.486,0,TER,1947,16.0386
20.496,0,TER,1948,16.0386
20.506,0,TER,1949,16.0386
21.3212,0,NTN,1950,271.186
21.3312,0,NTN,1951,271.186
21.3412,0,NTN,1952,271.186
```

Columns: `time_s,ue_node_id,path,seq,one_way_delay_ms`. Terrestrial goes
down at 20.5 s; the handover to NTN completes at 21.05 s; the first NTN
packet arrives 271 ms later.

Talking points:
- Delay is ~16 ms on the terrestrial path and ~271 ms on the NTN path; the
  `path` column comes from the packet's source address.
- On every handover the UE's default route is moved (FR-07).
- 55 packets are lost in the 0.55 s between the terrestrial outage and the
  completed fallback, in both modes: a failed access cannot be kept.
- Make-before-break saves the 15 packets of the three optional 50 ms
  handovers (FR-10).

## 6. Validation against closed-form expectations (2 min)

```
./ns3 run "tntn-validation --case=all"
```

```
V1 terrestrial failure: 25 nodes, max latency 1.03 s (bound 1.05 s) -> PASS
V2 terrestrial restored at 12.5 s:
  MinDwellTime 0 s: return expected 13.05 s, measured 13.05 s -> PASS
  ...
ALL VALIDATION CASES PASSED
```

Talking point: each measured handover time equals a value computed by hand
from `DecisionInterval`, `HandoverExecutionDelay`, `MinDwellTime`,
`MaxAcceptableDelay` and `HysteresisFraction` (details in `validation.md`).

## 7. Extension and integration (1 min, no commands)

Show the "Extension points" section of `futurenet-tntn.rst`:
- a new policy only implements `DoSelect()`; `TntnWeightedScorePolicy` was
  added without changing any other class;
- an M01 satellite model plugs in as a `TntnAccessStateProvider`, like
  `TntnPointToPointAccessStateProvider`;
- M10 can drive the manager with `ForceAccess`, `ForceEvaluation` and
  `SetPolicy`.

Close with the known limitations (README): no M01 adapter yet, instantaneous
metrics (no `MetricWindow`), no learning policy.

## Troubleshooting

| Problem | Fix |
|---|---|
| `./ns3: no such file or directory` | Run from the ns-3 root, not from `contrib/futurenet-tntn` |
| `.lock-ns3 file was not found` | Run `./ns3 configure --enable-examples --enable-tests` first |
| Example not found | Configure with `--enable-examples` |
| CSV files not found | They are written to the directory the command was run from |
