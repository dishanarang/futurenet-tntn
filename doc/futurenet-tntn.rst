FutureNet T-NTN Access Selection and Handover (futurenet-tntn)
==============================================================

.. heading hierarchy:
   ============= Module Name
   ------------- Section (#.#)
   ~~~~~~~~~~~~~ Subsection (#.#.#)

This module (FutureNet M02) lets an ns-3 node that has both a terrestrial
access and a non-terrestrial (NTN) access decide which one to use, and switch
between them with a logical handover. It models selection and orchestration
at the network/system level; it does not model cellular handover signalling.

The source code lives in ``contrib/futurenet-tntn``.

Model Description
-----------------

Overview
~~~~~~~~

Every managed node owns one ``TntnAccessManager``. Every
``DecisionInterval`` the manager:

1. asks each registered **access state provider** for the current state of
   its access (availability, delay, loss, rate, load);
2. passes those states and the current access to a **selection policy**,
   which returns the access to use and a reason;
3. if the policy chose a different access and the **dwell time** allows it,
   starts a **logical handover** that completes after
   ``HandoverExecutionDelay``;
4. reports the decision and the handover through **trace sources**.

::

     Terrestrial provider --+
                            +--> TntnAccessManager --> current access
     NTN provider ----------+          |      |
                                       |      +--> traces --> CSV (TntnHelper)
                              TntnAccessSelectionPolicy

Design principles
~~~~~~~~~~~~~~~~~

**Mechanism/policy separation** (guidelines 1.3). The manager is the
mechanism: it owns time (periodic evaluation, dwell time, execution delay),
state (current access, handover in progress) and tracing. The policy is a
pure decision function: given the access states and the current access it
returns a decision, and it keeps no state between calls. A policy can
therefore be replaced, including at run time, without touching the manager,
and can be unit-tested without a simulation.

**Technology agnostic.** The manager and the policies only see
``TntnAccessState``. Anything that can fill one in can be an access: a mock,
a point-to-point link, an M01 satellite model, an M07 path.

**Unavailable accesses are filtered once.** ``TntnAccessSelectionPolicy::Select()``
removes unavailable accesses before calling the subclass's ``DoSelect()``
and checks that the subclass returned one of the accesses it was offered.
No policy, including ones written by other teams, can select an unavailable
access (SRS T01).

**Hysteresis in the policy, dwell time in the manager.** Hysteresis is about
*how much better* a candidate must be, which depends on the policy's metric,
so it belongs to the policy. Dwell time is about *how long ago* the last
handover was, which needs a clock, so it belongs to the manager. Both prevent
oscillation (SRS FR-06).

**Dwell time never strands a node.** Dwell time blocks only optional
switches. Switching away from an unavailable access, the initial selection
and a forced switch are never delayed, because waiting would leave the node
without a working access or ignore an explicit command.

Class relationships
~~~~~~~~~~~~~~~~~~~

::

                    ns3::Object
                         ^
       +-----------------+------------------------------+
       |                 |                              |
   TntnAccessStateProvider   TntnAccessSelectionPolicy   TntnAccessManager
   (abstract)            (abstract: Select / DoSelect)   |  uses 1 policy
       ^                         ^                      |  uses N providers
       |                         |                      |  aggregated to Node
   +---+-------------------+     +------------------+
   |                       |     |                  |
   TntnMockAccess-     TntnPointToPoint-   TntnTerrestrial-  TntnLowestDelay-
   StateProvider       AccessStateProvider PreferredPolicy   Policy

   TntnHelper (not an Object): builds managers, policies and providers from
   ObjectFactory objects, aggregates managers to nodes, writes CSV.

   Shared types (tntn-access-types.h): TntnAccessState, TntnAccessDecision,
   TntnAccessType, TntnReason, TNTN_NO_ACCESS.

``TntnAccessManager`` holds its providers and its policy by ``Ptr``
(composition, guidelines CG-07). Inheritance is used only for real "is-a"
relationships: a provider *is an* access state provider, a policy *is a*
selection policy.

Access state
~~~~~~~~~~~~

``TntnAccessState`` is the snapshot a provider returns (SRS FR-02):

=============  ==================  ============================================
Field          Type                Meaning
=============  ==================  ============================================
accessId       ``uint32_t``        Access id within the node
type           ``TntnAccessType``  ``TERRESTRIAL`` or ``NTN``
available      ``bool``            Whether the access can carry traffic now
delay          ``Time``            One-way delay estimate
lossRate       ``double``          Packet loss ratio, [0,1]
rate           ``DataRate``        Available/nominal data rate
load           ``double``          Normalized load, [0,1]
=============  ==================  ============================================

``TntnAccessDecision`` holds ``selectedAccessId`` (``TNTN_NO_ACCESS`` if
nothing is available), a ``TntnReason`` and a policy-specific ``score``.

Reason codes (SRS FR-08):

=======================  ======================================================
Reason                   Meaning
=======================  ======================================================
``INITIAL_SELECTION``    First access chosen after start
``NONE``                 Current access kept
``CURRENT_UNAVAILABLE``  Current access is unavailable
``DELAY_EXCEEDED``       Current terrestrial access exceeds the delay constraint
``LOSS_EXCEEDED``        Current terrestrial access exceeds the loss constraint
``RATE_TOO_LOW``         Current terrestrial access is below the rate constraint
``BETTER_CANDIDATE``     Another access is better by more than the hysteresis
``PREFERRED_RESTORED``   Terrestrial is acceptable again while on NTN
``FORCED``               Requested through ``ForceAccess()``
=======================  ======================================================

Algorithms
~~~~~~~~~~

**Base policy** (``TntnAccessSelectionPolicy::Select``)::

    available = [c in candidates where c.available]
    if available is empty:
        return {TNTN_NO_ACCESS, NONE}
    decision = DoSelect(available, current)        # subclass
    abort if decision.selectedAccessId not in available
    return decision

**Terrestrial-preferred policy** (SRS FR-04)::

    meets(s) = s.delay <= MaxAcceptableDelay
               and s.lossRate <= MaxAcceptableLoss
               and s.rate >= MinAcceptableRate

    best(type, needMeets):
        among available accesses of this type (that meet the constraints
        if needMeets): the current access if it qualifies, else the one
        with the lowest delay (first in list order on a tie)

    chosen = best(TERRESTRIAL, true)
          or best(NTN, false)
          or best(TERRESTRIAL, false)    # degraded terrestrial beats nothing

    reason = INITIAL_SELECTION   if current == TNTN_NO_ACCESS
             NONE                if chosen == current
             CURRENT_UNAVAILABLE if current is not available
             PREFERRED_RESTORED  if current is NTN and chosen is terrestrial
             DELAY_EXCEEDED / LOSS_EXCEEDED / RATE_TOO_LOW
                                 by the first constraint the current access fails
             BETTER_CANDIDATE    otherwise

A delay exactly equal to ``MaxAcceptableDelay`` is acceptable.

**Lowest-delay policy** (SRS FR-05, FR-06)::

    best = available access with the lowest delay (first on a tie)
    if current == TNTN_NO_ACCESS:      select best, INITIAL_SELECTION
    elif current is not available:     select best, CURRENT_UNAVAILABLE
    elif best == current:              keep current, NONE
    elif best.delay <= current.delay * (1 - HysteresisFraction):
                                       select best, BETTER_CANDIDATE
    else:                              keep current, NONE

**Manager evaluation** (``TntnAccessManager::Evaluate``, SRS FR-03, FR-06, FR-07)::

    if handover in progress: return          # decide again once it completes
    states = [provider.GetAccessState(id) for each access, registration order]
    fire AccessUnavailable for each access that went available -> unavailable
    decision = policy.Select(states, current)
    fire DecisionEvaluated(node, current, decision.selected, decision.reason)
    if decision.selected is TNTN_NO_ACCESS or == current: return
    if dwell applies to decision.reason
       and now - lastHandoverCompletion < MinDwellTime: return
    StartHandover(decision.selected, decision.reason)

    StartHandover(target, reason):
        inProgress = true
        fire HandoverStart(node, current, target, reason)
        schedule CompleteHandover after HandoverExecutionDelay

    CompleteHandover():
        old = current; current = target; inProgress = false
        lastHandoverCompletion = now
        fire HandoverComplete(node, old, current)

Dwell applies to every reason except ``INITIAL_SELECTION``,
``CURRENT_UNAVAILABLE`` and ``FORCED``. When nothing is available the manager
keeps its current access, so it returns to it as soon as it recovers.
``GetCurrentAccess()`` keeps returning the old access until the handover
completes.

**Cost.** One evaluation reads each of the node's *A* accesses once and the
policy scans them once: O(*A*) per node per interval, O(*N*·*A*) per interval
for *N* nodes. There is no node-to-node interaction and no O(*N*\ :sup:`2`)
step (guidelines section 11).

Providers
~~~~~~~~~

``TntnMockAccessStateProvider`` returns values set through attributes or
setters, and can schedule changes at absolute times
(``ScheduleAvailable``, ``ScheduleDelay``, ``ScheduleLossRate``,
``ScheduleRate``, ``ScheduleLoad``). It lets the module be tested without
any network model (SRS section 3).

``TntnPointToPointAccessStateProvider`` reads a real point-to-point link
through ``SetDevice()``:

===========  ==================================================================
Field        Source
===========  ==================================================================
available    ``PointToPointNetDevice::IsLinkUp()`` and the device's IPv4
             interface ``Ipv4::IsUp()``
delay        the channel's ``Delay`` attribute
rate         the device's ``DataRate`` attribute
lossRate     error rate of a packet-unit ``RateErrorModel`` set as
             ``ReceiveErrorModel`` on the far-end device, else 0
load         transmit queue occupancy, current size / maximum size
===========  ==================================================================

Values are read on every call, so changes during the run are seen at the
next evaluation.

Scope and Limitations
~~~~~~~~~~~~~~~~~~~~~

- Selection and logical switching only. 5G RRC signalling, measurement
  reports, authentication, core-network mobility and satellite PHY are out of
  scope (SRS section 11). No 3GPP compliance is claimed.
- The manager changes which access is current; moving traffic is done by the
  user through the ``HandoverComplete`` trace (see
  ``tntn-example-integration``, which updates the default route).
- Metrics are instantaneous; ``MetricWindow`` averaging is not implemented.
- M01 is not integrated yet. The integration example uses a fixed-delay
  point-to-point link as the NTN path.
- No ``futurenet-common`` package was available; ``TntnAccessState`` is
  local to this module and the common event CSV schema is written directly.
- ``WeightedScorePolicy`` (FR-09) and make-before-break (FR-10) are not
  implemented.
- Default values are project teaching values (SRS section 5), not
  3GPP-mandated values.

References
~~~~~~~~~~

- FutureNet M02 SRS, "Integrated Terrestrial-NTN Access and Handover", v1.0.
- FutureNet Common Guidelines for ns-3.46.1, v1.0.
- 3GPP, "Non-Terrestrial Networks (NTN)", https://www.3gpp.org/technologies/ntn-overview
- 3GPP TR 38.821, "Solutions for NR to support Non-Terrestrial Networks".
- ns-3 Manual, Release 3.46 (attributes, callbacks, tracing).

Usage
-----

Building
~~~~~~~~

The module depends on ``core``, ``network``, ``internet`` and
``point-to-point``; the examples also use ``applications``::

    ./ns3 configure --enable-examples --enable-tests
    ./ns3 build

Helper
~~~~~~

.. code-block:: cpp

    TntnHelper tntn;
    tntn.SetPolicy("ns3::TntnLowestDelayPolicy",
                   "HysteresisFraction", DoubleValue(0.05));
    tntn.SetManagerAttribute("MinDwellTime", TimeValue(Seconds(5)));
    tntn.AddAccess(0, "ns3::TntnMockAccessStateProvider",
                   "AccessType", EnumValue(TntnAccessType::TERRESTRIAL));
    tntn.AddAccess(1, "ns3::TntnMockAccessStateProvider",
                   "AccessType", EnumValue(TntnAccessType::NTN),
                   "Delay", TimeValue(MilliSeconds(540)));
    auto managers = tntn.Install(nodes);
    tntn.EnableTracing("run");

For every node ``Install`` creates a manager, a policy and one provider per
``AddAccess`` call from object factories, so no state is shared between
nodes. It sets the manager's node id, aggregates it to the node and calls
``Start()``. ``EnableTracing`` must be called after ``Install``.

Without the helper:

.. code-block:: cpp

    auto manager = CreateObject<TntnAccessManager>();
    manager->AddAccess(0, terProvider);
    manager->AddAccess(1, ntnProvider);
    manager->SetPolicy(CreateObject<TntnTerrestrialPreferredPolicy>());
    manager->Start();

Attributes
~~~~~~~~~~

Ranges are enforced by attribute checkers or by ``NS_ABORT_MSG_IF``
(guidelines CG-11).

======================================================  ===========  ================  =========================================
Attribute                                               Default      Range             Meaning
======================================================  ===========  ================  =========================================
``TntnAccessManager::DecisionInterval``                 1 s          100 ms - 10 s     Time between evaluations
``TntnAccessManager::MinDwellTime``                     5 s          0 - 60 s          Minimum time before an optional switch
``TntnAccessManager::HandoverExecutionDelay``           50 ms        0 - 2 s           Handover start to completion
``TntnTerrestrialPreferredPolicy::MaxAcceptableDelay``  150 ms       1 ms - 5 s        Largest acceptable terrestrial delay
``TntnTerrestrialPreferredPolicy::MaxAcceptableLoss``   0.10         0 - 1             Largest acceptable terrestrial loss ratio
``TntnTerrestrialPreferredPolicy::MinAcceptableRate``   1 Mbps       0 - 10 Gbps       Smallest acceptable terrestrial rate
``TntnLowestDelayPolicy::HysteresisFraction``           0.05         0 - 0.5           Required relative delay improvement
``TntnMockAccessStateProvider::AccessType``             Terrestrial  Terrestrial, Ntn  Network kind
``TntnMockAccessStateProvider::Available``              true         bool              Availability
``TntnMockAccessStateProvider::Delay``                  30 ms        >= 0              One-way delay
``TntnMockAccessStateProvider::LossRate``               0.01         0 - 1             Loss ratio
``TntnMockAccessStateProvider::Rate``                   50 Mbps      >= 0              Data rate
``TntnMockAccessStateProvider::Load``                   0.2          0 - 1             Normalized load
``TntnPointToPointAccessStateProvider::AccessType``     Terrestrial  Terrestrial, Ntn  Network kind
======================================================  ===========  ================  =========================================

Attributes can be set from the command line of any example, e.g.
``--ns3::TntnAccessManager::MinDwellTime=10s``.

Trace sources
~~~~~~~~~~~~~

All on ``TntnAccessManager``:

=====================  =====================================================  =============================================
Trace source           Signature                                              Fired when
=====================  =====================================================  =============================================
``DecisionEvaluated``  (nodeId, currentAccess, selectedAccess, reason)        After every evaluation that consulted the policy
``HandoverStart``      (nodeId, oldAccess, newAccess, reason)                 A handover starts
``HandoverComplete``   (nodeId, oldAccess, newAccess)                         A handover completes; ``newAccess`` is now current
``AccessUnavailable``  (nodeId, accessId)                                     An access goes from available to unavailable
=====================  =====================================================  =============================================

``DecisionEvaluated`` adds the reason to the signature given in SRS section 7,
so that the decisions CSV can report it.

Output
~~~~~~

``TntnHelper::EnableTracing(prefix)`` writes ``<prefix>-decisions.csv``
(columns of SRS section 7, one row per evaluation; ``handover`` is 1 when the
evaluation started a handover) and ``<prefix>-events.csv`` (common FutureNet
schema of guidelines section 4.5). Column meanings are listed in the README.

Control interface
~~~~~~~~~~~~~~~~~

For external controllers such as an M10 agent:

- ``ForceAccess(id)``: start a handover to ``id`` now, ignoring the policy and
  the dwell time (reason ``FORCED``). Returns false if the target is
  unavailable or a handover is in progress.
- ``ForceEvaluation()``: evaluate now and restart the periodic schedule.
- ``SetPolicy(policy)``: replace the policy; used from the next evaluation.
- ``GetCurrentAccess()``, ``IsHandoverInProgress()``,
  ``GetLastAccessStates()``: observations.

Examples
~~~~~~~~

``tntn-example-basic``
    25 nodes with two mock accesses for 60 s (SRS minimum demonstration
    scale). Each node's terrestrial access has one outage and one congestion
    period at random times drawn from an ns-3 ``UniformRandomVariable``
    (reproducible for a given ``--RngRun``). Writes both CSV files.

``tntn-example-integration``
    UEs connected to a router by a terrestrial (50 Mbit/s, 15 ms) and an NTN
    (10 Mbit/s, 270 ms) point-to-point link, using
    ``TntnPointToPointAccessStateProvider``. On ``HandoverComplete`` the UE's
    default route moves to the new link, so UDP traffic changes path. Writes
    per-packet path and one-way delay.

``tntn-policy-demo``
    Calls a policy directly on hand-made situations and prints the choices.

``tntn-validation``
    Validation cases V1-V3 (see below).

Extension points
~~~~~~~~~~~~~~~~

**New policy** (e.g. SRS FR-09 weighted score, FR-11 prediction or learning):
derive from ``TntnAccessSelectionPolicy``, register a ``TypeId`` with its
attributes, and implement ``DoSelect()``. It receives only available
accesses and must return one of them.

.. code-block:: cpp

    class MyPolicy : public TntnAccessSelectionPolicy
    {
      public:
        static TypeId GetTypeId();
      protected:
        TntnAccessDecision DoSelect(const std::vector<TntnAccessState>& available,
                                    uint32_t currentAccessId) const override;
    };

    tntn.SetPolicy("ns3::MyPolicy");

**New access** (e.g. an M01 satellite adapter): derive from
``TntnAccessStateProvider`` and implement ``GetAccessState()`` from the
other model's public API. Pass the TypeId to ``TntnHelper::AddAccess``.
``TntnPointToPointAccessStateProvider`` is the reference adapter.

**Acting on handovers** (e.g. M07 moving flows): connect to
``HandoverComplete``.

Validation
----------

Testing
~~~~~~~

``./test.py -s futurenet-tntn`` runs 14 test cases:

=============================================  ================================================
Test                                           Checks
=============================================  ================================================
Mock defaults, ranges, scheduled changes       Provider defaults, checkers, timing of changes
T01                                            Unavailable access never selected; none -> TNTN_NO_ACCESS
T02                                            Terrestrial-preferred rules and reasons, incl. limit edge
Terrestrial-preferred attributes               Defaults and ranges
T03                                            Lowest delay among available accesses
T04                                            Hysteresis blocks small improvements
Lowest-delay attributes                        Default and range
T05 / T06                                      Dwell blocks early return; traces report old/new, reason, times
ForceAccess                                    Overrides policy and dwell; refuses unavailable target
Repeatability                                  Identical runs give identical handover traces (guidelines 8.4)
Manager attributes                             Defaults and ranges
T6 integration                                 Point-to-point provider on a real ns-3 link drives the manager
=============================================  ================================================

Validation cases
~~~~~~~~~~~~~~~~

``./ns3 run "tntn-validation --case=all"`` runs three deterministic
validation cases and compares each measured handover time with a
closed-form expectation:

- **V1** terrestrial failure on 25 nodes: every node moves to NTN within
  ``DecisionInterval + HandoverExecutionDelay``.
- **V2** terrestrial restored with ``MinDwellTime`` of 0, 2, 5 and 10 s: the
  return happens at the first evaluation after the dwell time, with no
  oscillation.
- **V3** terrestrial delay ramp: terrestrial-preferred switches just above
  ``MaxAcceptableDelay``; lowest-delay switches only when NTN is better by
  ``HysteresisFraction``.

All cases pass. Formulas, tables and the run record are in
``doc/validation.md``.
