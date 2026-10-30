#include "ns3/boolean.h"
#include "ns3/channel.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/error-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/tntn-access-manager.h"
#include "ns3/tntn-lowest-delay-policy.h"
#include "ns3/tntn-mock-access-state-provider.h"
#include "ns3/tntn-point-to-point-access-state-provider.h"
#include "ns3/tntn-terrestrial-preferred-policy.h"
#include "ns3/tntn-weighted-score-policy.h"

using namespace ns3;

/**
 * @defgroup futurenet-tntn-tests Tests for futurenet-tntn
 * @ingroup futurenet-tntn
 * @ingroup tests
 */

/// Terrestrial access id used by the policy tests
static constexpr uint32_t TER = 0;
/// NTN access id used by the policy tests
static constexpr uint32_t NTN = 1;

/**
 * @ingroup futurenet-tntn-tests
 * Build an access state for policy tests.
 * @param id access id
 * @param type access type
 * @param available availability
 * @param delayMs one-way delay in ms
 * @param loss loss ratio
 * @param rateMbps data rate in Mbit/s
 * @return the state
 */
static TntnAccessState
MakeState(uint32_t id,
          TntnAccessType type,
          bool available,
          double delayMs,
          double loss = 0.0,
          double rateMbps = 50.0)
{
    TntnAccessState s;
    s.accessId = id;
    s.type = type;
    s.available = available;
    s.delay = MilliSeconds(delayMs);
    s.lossRate = loss;
    s.rate = DataRate(static_cast<uint64_t>(rateMbps * 1e6));
    return s;
}

/**
 * @ingroup futurenet-tntn-tests
 * Mock provider reports its documented defaults and the requested access id.
 */
class TntnMockDefaultsTestCase : public TestCase
{
  public:
    TntnMockDefaultsTestCase()
        : TestCase("Mock provider default attribute values")
    {
    }

  private:
    void DoRun() override
    {
        auto mock = CreateObject<TntnMockAccessStateProvider>();
        TntnAccessState s = mock->GetAccessState(7);

        NS_TEST_ASSERT_MSG_EQ(s.accessId, 7, "accessId not copied into state");
        NS_TEST_ASSERT_MSG_EQ((s.type == TntnAccessType::TERRESTRIAL), true, "default type");
        NS_TEST_ASSERT_MSG_EQ(s.available, true, "default availability");
        NS_TEST_ASSERT_MSG_EQ(s.delay, MilliSeconds(30), "default delay");
        NS_TEST_ASSERT_MSG_EQ_TOL(s.lossRate, 0.01, 1e-12, "default loss rate");
        NS_TEST_ASSERT_MSG_EQ(s.rate, DataRate("50Mbps"), "default rate");
        NS_TEST_ASSERT_MSG_EQ_TOL(s.load, 0.2, 1e-12, "default load");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Attributes accept boundary values and the checkers reject out-of-range ones.
 */
class TntnMockAttributeBoundsTestCase : public TestCase
{
  public:
    TntnMockAttributeBoundsTestCase()
        : TestCase("Mock provider attribute range checks")
    {
    }

  private:
    void DoRun() override
    {
        auto mock = CreateObject<TntnMockAccessStateProvider>();

        NS_TEST_ASSERT_MSG_EQ(mock->SetAttributeFailSafe("LossRate", DoubleValue(0.0)),
                              true,
                              "LossRate 0 should be accepted");
        NS_TEST_ASSERT_MSG_EQ(mock->SetAttributeFailSafe("LossRate", DoubleValue(1.0)),
                              true,
                              "LossRate 1 should be accepted");
        NS_TEST_ASSERT_MSG_EQ(mock->SetAttributeFailSafe("LossRate", DoubleValue(1.01)),
                              false,
                              "LossRate > 1 should be rejected");
        NS_TEST_ASSERT_MSG_EQ(mock->SetAttributeFailSafe("Load", DoubleValue(-0.01)),
                              false,
                              "negative Load should be rejected");
        NS_TEST_ASSERT_MSG_EQ(mock->SetAttributeFailSafe("Delay", TimeValue(Seconds(-1))),
                              false,
                              "negative Delay should be rejected");
        NS_TEST_ASSERT_MSG_EQ(
            mock->SetAttributeFailSafe("AccessType", EnumValue(TntnAccessType::NTN)),
            true,
            "AccessType Ntn should be accepted");
        NS_TEST_ASSERT_MSG_EQ((mock->GetAccessState(0).type == TntnAccessType::NTN),
                              true,
                              "AccessType attribute not applied");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Scheduled changes take effect exactly at their absolute time, not before.
 */
class TntnMockScheduleTestCase : public TestCase
{
  public:
    TntnMockScheduleTestCase()
        : TestCase("Mock provider scheduled changes apply at the requested time")
    {
    }

  private:
    /**
     * Record the availability of the probed access.
     * @param mock the provider
     * @param out where to store the availability
     */
    static void Probe(Ptr<TntnMockAccessStateProvider> mock, bool* out)
    {
        *out = mock->GetAccessState(0).available;
    }

    void DoRun() override
    {
        auto mock = CreateObject<TntnMockAccessStateProvider>();
        mock->ScheduleAvailable(Seconds(2), false);
        mock->ScheduleDelay(Seconds(2), MilliSeconds(200));

        bool before = false;
        bool after = true;
        Simulator::Schedule(Seconds(1), &Probe, mock, &before);
        Simulator::Schedule(Seconds(3), &Probe, mock, &after);
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(before, true, "change applied too early");
        NS_TEST_ASSERT_MSG_EQ(after, false, "scheduled availability change not applied");
        NS_TEST_ASSERT_MSG_EQ(mock->GetAccessState(0).delay,
                              MilliSeconds(200),
                              "scheduled delay change not applied");
        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * T01: an unavailable access is never selected, and with nothing available
 * the decision is TNTN_NO_ACCESS.
 */
class TntnUnavailableNeverSelectedTestCase : public TestCase
{
  public:
    TntnUnavailableNeverSelectedTestCase()
        : TestCase("T01 unavailable access is never selected")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnTerrestrialPreferredPolicy>();

        // Terrestrial would win on every metric, but it is down.
        std::vector<TntnAccessState> c = {MakeState(TER, TntnAccessType::TERRESTRIAL, false, 10),
                                          MakeState(NTN, TntnAccessType::NTN, true, 540)};
        NS_TEST_ASSERT_MSG_EQ(policy->Select(c, TER).selectedAccessId,
                              NTN,
                              "unavailable terrestrial was selected");

        c[1].available = false;
        TntnAccessDecision d = policy->Select(c, NTN);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId,
                              TNTN_NO_ACCESS,
                              "an access was selected although none is available");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * T02: terrestrial-preferred policy follows its documented rules and reasons.
 */
class TntnTerrestrialPreferredRuleTestCase : public TestCase
{
  public:
    TntnTerrestrialPreferredRuleTestCase()
        : TestCase("T02 terrestrial-preferred policy follows its rule")
    {
    }

  private:
    /**
     * Run the policy and check the selected access and reason.
     * @param policy the policy
     * @param c candidates
     * @param current current access id
     * @param expectedId expected selected access
     * @param expectedReason expected reason
     * @param what description used in failure messages
     */
    void Check(Ptr<TntnTerrestrialPreferredPolicy> policy,
               const std::vector<TntnAccessState>& c,
               uint32_t current,
               uint32_t expectedId,
               TntnReason expectedReason,
               const std::string& what)
    {
        TntnAccessDecision d = policy->Select(c, current);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, expectedId, what << ": wrong access");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(expectedReason),
                              what << ": wrong reason");
    }

    void DoRun() override
    {
        auto policy = CreateObject<TntnTerrestrialPreferredPolicy>();
        const auto ter = TntnAccessType::TERRESTRIAL;
        const auto ntn = TntnAccessType::NTN;

        Check(policy,
              {MakeState(TER, ter, true, 30), MakeState(NTN, ntn, true, 540)},
              TNTN_NO_ACCESS,
              TER,
              TntnReason::INITIAL_SELECTION,
              "first decision with good terrestrial");
        Check(policy,
              {MakeState(TER, ter, true, 30), MakeState(NTN, ntn, true, 20)},
              TER,
              TER,
              TntnReason::NONE,
              "good terrestrial kept even if NTN is faster");
        Check(policy,
              {MakeState(TER, ter, false, 30), MakeState(NTN, ntn, true, 540)},
              TER,
              NTN,
              TntnReason::CURRENT_UNAVAILABLE,
              "terrestrial down");
        Check(policy,
              {MakeState(TER, ter, true, 200), MakeState(NTN, ntn, true, 540)},
              TER,
              NTN,
              TntnReason::DELAY_EXCEEDED,
              "terrestrial delay above 150 ms");
        Check(policy,
              {MakeState(TER, ter, true, 30, 0.2), MakeState(NTN, ntn, true, 540)},
              TER,
              NTN,
              TntnReason::LOSS_EXCEEDED,
              "terrestrial loss above 0.10");
        Check(policy,
              {MakeState(TER, ter, true, 30, 0.0, 0.5), MakeState(NTN, ntn, true, 540)},
              TER,
              NTN,
              TntnReason::RATE_TOO_LOW,
              "terrestrial rate below 1 Mbit/s");
        Check(policy,
              {MakeState(TER, ter, true, 30), MakeState(NTN, ntn, true, 540)},
              NTN,
              TER,
              TntnReason::PREFERRED_RESTORED,
              "terrestrial good again while on NTN");
        Check(policy,
              {MakeState(TER, ter, true, 150), MakeState(NTN, ntn, true, 540)},
              TER,
              TER,
              TntnReason::NONE,
              "delay exactly at the limit is acceptable");
        Check(policy,
              {MakeState(TER, ter, true, 200), MakeState(NTN, ntn, false, 540)},
              TER,
              TER,
              TntnReason::NONE,
              "degraded terrestrial kept when NTN is down");

        // The constraint is configurable: raising the limit keeps terrestrial.
        policy->SetAttribute("MaxAcceptableDelay", TimeValue(MilliSeconds(250)));
        Check(policy,
              {MakeState(TER, ter, true, 200), MakeState(NTN, ntn, true, 540)},
              TER,
              TER,
              TntnReason::NONE,
              "200 ms acceptable after raising MaxAcceptableDelay");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Terrestrial-preferred policy defaults and attribute ranges (SRS section 5).
 */
class TntnTerrestrialPreferredAttributesTestCase : public TestCase
{
  public:
    TntnTerrestrialPreferredAttributesTestCase()
        : TestCase("Terrestrial-preferred policy defaults and attribute ranges")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnTerrestrialPreferredPolicy>();

        TimeValue delay;
        policy->GetAttribute("MaxAcceptableDelay", delay);
        NS_TEST_ASSERT_MSG_EQ(delay.Get(), MilliSeconds(150), "default MaxAcceptableDelay");
        DoubleValue loss;
        policy->GetAttribute("MaxAcceptableLoss", loss);
        NS_TEST_ASSERT_MSG_EQ_TOL(loss.Get(), 0.10, 1e-12, "default MaxAcceptableLoss");
        DataRateValue rate;
        policy->GetAttribute("MinAcceptableRate", rate);
        NS_TEST_ASSERT_MSG_EQ(rate.Get(), DataRate("1Mbps"), "default MinAcceptableRate");

        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("MaxAcceptableDelay", TimeValue(MilliSeconds(1))),
            true,
            "MaxAcceptableDelay 1 ms should be accepted");
        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("MaxAcceptableDelay", TimeValue(Seconds(5))),
            true,
            "MaxAcceptableDelay 5 s should be accepted");
        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("MaxAcceptableDelay", TimeValue(MicroSeconds(999))),
            false,
            "MaxAcceptableDelay below 1 ms should be rejected");
        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("MaxAcceptableDelay", TimeValue(Seconds(6))),
            false,
            "MaxAcceptableDelay above 5 s should be rejected");
        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("MaxAcceptableLoss", DoubleValue(1.5)),
                              false,
                              "MaxAcceptableLoss above 1 should be rejected");
        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("MinAcceptableRate", DataRateValue(DataRate("10Gbps"))),
            true,
            "MinAcceptableRate 10 Gbit/s should be accepted");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * T03: lowest-delay policy selects the minimum delay among available accesses.
 */
class TntnLowestDelaySelectsMinimumTestCase : public TestCase
{
  public:
    TntnLowestDelaySelectsMinimumTestCase()
        : TestCase("T03 lowest-delay policy selects minimum eligible delay")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnLowestDelayPolicy>();
        const auto ter = TntnAccessType::TERRESTRIAL;
        const auto ntn = TntnAccessType::NTN;

        // Access 2 has the lowest delay of all but is down, so access 1 must win.
        std::vector<TntnAccessState> c = {MakeState(0, ter, true, 80),
                                          MakeState(1, ntn, true, 40),
                                          MakeState(2, ter, false, 10)};
        TntnAccessDecision d = policy->Select(c, TNTN_NO_ACCESS);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, 1, "did not pick the minimum available delay");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::INITIAL_SELECTION),
                              "wrong reason for first decision");

        // Type does not matter: an NTN access can win if it is faster.
        d = policy->Select(c, 0);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, 1, "faster NTN access not selected");

        // Current access down: switch immediately to the minimum, whatever the hysteresis.
        c[0].available = false;
        c[1].delay = MilliSeconds(500);
        c[2].available = true;
        c[2].delay = MilliSeconds(499);
        d = policy->Select(c, 0);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, 2, "minimum not selected after current failed");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::CURRENT_UNAVAILABLE),
                              "wrong reason after current failed");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * T04: hysteresis blocks a switch when the delay improvement is insufficient.
 */
class TntnLowestDelayHysteresisTestCase : public TestCase
{
  public:
    TntnLowestDelayHysteresisTestCase()
        : TestCase("T04 hysteresis blocks switch when improvement is insufficient")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnLowestDelayPolicy>();
        const auto ter = TntnAccessType::TERRESTRIAL;
        const auto ntn = TntnAccessType::NTN;

        // Default hysteresis 0.05: from 100 ms, a switch needs <= 95 ms.
        TntnAccessDecision d =
            policy->Select({MakeState(TER, ter, true, 100), MakeState(NTN, ntn, true, 97)}, TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, TER, "3% improvement should not switch");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::NONE),
                              "wrong reason when staying");

        d = policy->Select({MakeState(TER, ter, true, 100), MakeState(NTN, ntn, true, 90)}, TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, NTN, "10% improvement should switch");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::BETTER_CANDIDATE),
                              "wrong reason when switching");

        // Larger hysteresis blocks the same 10% improvement.
        policy->SetAttribute("HysteresisFraction", DoubleValue(0.2));
        d = policy->Select({MakeState(TER, ter, true, 100), MakeState(NTN, ntn, true, 90)}, TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, TER, "10% improvement blocked by 20% hysteresis");

        // Zero hysteresis switches on any improvement.
        policy->SetAttribute("HysteresisFraction", DoubleValue(0.0));
        d = policy->Select({MakeState(TER, ter, true, 100), MakeState(NTN, ntn, true, 99)}, TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, NTN, "any improvement switches at 0 hysteresis");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Lowest-delay policy default and attribute range (SRS section 5).
 */
class TntnLowestDelayAttributesTestCase : public TestCase
{
  public:
    TntnLowestDelayAttributesTestCase()
        : TestCase("Lowest-delay policy default and attribute range")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnLowestDelayPolicy>();
        DoubleValue h;
        policy->GetAttribute("HysteresisFraction", h);
        NS_TEST_ASSERT_MSG_EQ_TOL(h.Get(), 0.05, 1e-12, "default HysteresisFraction");

        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("HysteresisFraction", DoubleValue(0.5)),
                              true,
                              "HysteresisFraction 0.5 should be accepted");
        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("HysteresisFraction", DoubleValue(0.51)),
                              false,
                              "HysteresisFraction above 0.5 should be rejected");
        NS_TEST_ASSERT_MSG_EQ(
            policy->SetAttributeFailSafe("HysteresisFraction", DoubleValue(-0.01)),
            false,
            "negative HysteresisFraction should be rejected");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * One manager with a terrestrial and an NTN mock access, recording every
 * trace it fires. Defaults: 1 s interval, 5 s dwell, 50 ms execution delay.
 */
class TntnManagerScenario
{
  public:
    /// A HandoverStart or HandoverComplete event
    struct Handover
    {
        double timeS;      //!< Simulation time, s
        uint32_t oldId;    //!< Old access
        uint32_t newId;    //!< New access
        TntnReason reason; //!< Reason (NONE for HandoverComplete)
    };

    TntnManagerScenario()
    {
        ter = CreateObject<TntnMockAccessStateProvider>();
        ter->SetAccessType(TntnAccessType::TERRESTRIAL);
        ntn = CreateObject<TntnMockAccessStateProvider>();
        ntn->SetAccessType(TntnAccessType::NTN);
        ntn->SetDelay(MilliSeconds(540));

        manager = CreateObject<TntnAccessManager>();
        manager->SetNodeId(7);
        manager->AddAccess(TER, ter);
        manager->AddAccess(NTN, ntn);
        manager->SetPolicy(CreateObject<TntnTerrestrialPreferredPolicy>());

        manager->TraceConnectWithoutContext("DecisionEvaluated",
                                            MakeCallback(&TntnManagerScenario::OnDecision, this));
        manager->TraceConnectWithoutContext("HandoverStart",
                                            MakeCallback(&TntnManagerScenario::OnStart, this));
        manager->TraceConnectWithoutContext("HandoverComplete",
                                            MakeCallback(&TntnManagerScenario::OnComplete, this));
        manager->TraceConnectWithoutContext(
            "AccessUnavailable",
            MakeCallback(&TntnManagerScenario::OnUnavailable, this));
    }

    /**
     * Record a DecisionEvaluated event.
     * @param nodeId node id
     * @param current current access
     * @param selected selected access
     * @param reason reason
     */
    void OnDecision(uint32_t nodeId, uint32_t current, uint32_t selected, TntnReason reason)
    {
        decisions++;
        lastNodeId = nodeId;
    }

    /**
     * Record a HandoverStart event.
     * @param nodeId node id
     * @param oldId old access
     * @param newId new access
     * @param reason reason
     */
    void OnStart(uint32_t nodeId, uint32_t oldId, uint32_t newId, TntnReason reason)
    {
        starts.push_back({Simulator::Now().GetSeconds(), oldId, newId, reason});
    }

    /**
     * Record a HandoverComplete event.
     * @param nodeId node id
     * @param oldId old access
     * @param newId new access
     */
    void OnComplete(uint32_t nodeId, uint32_t oldId, uint32_t newId)
    {
        completes.push_back({Simulator::Now().GetSeconds(), oldId, newId, TntnReason::NONE});
    }

    /**
     * Record an AccessUnavailable event.
     * @param nodeId node id
     * @param accessId access that became unavailable
     */
    void OnUnavailable(uint32_t nodeId, uint32_t accessId)
    {
        unavailable.push_back(accessId);
    }

    Ptr<TntnMockAccessStateProvider> ter; //!< Terrestrial access
    Ptr<TntnMockAccessStateProvider> ntn; //!< NTN access
    Ptr<TntnAccessManager> manager;       //!< Manager under test
    uint32_t decisions{0};                //!< DecisionEvaluated count
    uint32_t lastNodeId{0};               //!< Node id seen in the last decision
    std::vector<Handover> starts;         //!< HandoverStart events
    std::vector<Handover> completes;      //!< HandoverComplete events
    std::vector<uint32_t> unavailable;    //!< AccessUnavailable events
};

/**
 * @ingroup futurenet-tntn-tests
 * T05 dwell time and T06 traces, on one outage-and-recovery run.
 *
 * Terrestrial is down from 1.5 s to 2.5 s. Expected: initial selection of
 * terrestrial at 0 s, immediate fallback to NTN at 2 s despite the dwell time
 * (current unavailable), return to terrestrial blocked until 2.05 + 5 s, so it
 * happens at the 8 s evaluation.
 */
class TntnManagerDwellAndTraceTestCase : public TestCase
{
  public:
    TntnManagerDwellAndTraceTestCase()
        : TestCase("T05/T06 dwell blocks early return; traces report old/new access")
    {
    }

  private:
    void DoRun() override
    {
        TntnManagerScenario s;
        s.ter->ScheduleAvailable(Seconds(1.5), false);
        s.ter->ScheduleAvailable(Seconds(2.5), true);
        s.manager->Start();

        uint32_t at2s02 = 0;
        bool inProgressAt2s02 = false;
        uint32_t at7s5 = 0;
        Simulator::Schedule(Seconds(2.02), [&]() {
            at2s02 = s.manager->GetCurrentAccess();
            inProgressAt2s02 = s.manager->IsHandoverInProgress();
        });
        Simulator::Schedule(Seconds(7.5), [&]() { at7s5 = s.manager->GetCurrentAccess(); });
        Simulator::Stop(Seconds(10));
        Simulator::Run();

        // T05
        NS_TEST_ASSERT_MSG_EQ(at7s5, NTN, "return to terrestrial not blocked by dwell");
        NS_TEST_ASSERT_MSG_EQ(s.manager->GetCurrentAccess(), TER, "never returned to terrestrial");

        // Handover execution: the failed terrestrial access cannot be kept, so
        // the node is detached until the handover completes.
        NS_TEST_ASSERT_MSG_EQ(inProgressAt2s02, true, "handover should be in progress at 2.02 s");
        NS_TEST_ASSERT_MSG_EQ(at2s02, TNTN_NO_ACCESS, "node should be detached during handover");

        // T06
        NS_TEST_ASSERT_MSG_EQ(s.starts.size(), 3, "expected 3 handovers");
        NS_TEST_ASSERT_MSG_EQ(s.completes.size(), 3, "expected 3 handover completions");
        const uint32_t expectedOld[] = {TNTN_NO_ACCESS, TER, NTN};
        const uint32_t expectedNew[] = {TER, NTN, TER};
        const TntnReason expectedReason[] = {TntnReason::INITIAL_SELECTION,
                                             TntnReason::CURRENT_UNAVAILABLE,
                                             TntnReason::PREFERRED_RESTORED};
        const double expectedStartS[] = {0.0, 2.0, 8.0};
        for (std::size_t i = 0; i < s.starts.size() && i < 3; ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(s.starts[i].oldId, expectedOld[i], "HandoverStart old " << i);
            NS_TEST_ASSERT_MSG_EQ(s.starts[i].newId, expectedNew[i], "HandoverStart new " << i);
            NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(s.starts[i].reason),
                                  TntnReasonToString(expectedReason[i]),
                                  "HandoverStart reason " << i);
            NS_TEST_ASSERT_MSG_EQ_TOL(s.starts[i].timeS,
                                      expectedStartS[i],
                                      1e-9,
                                      "HandoverStart time " << i);
            NS_TEST_ASSERT_MSG_EQ(s.completes[i].oldId,
                                  expectedOld[i],
                                  "HandoverComplete old " << i);
            NS_TEST_ASSERT_MSG_EQ(s.completes[i].newId,
                                  expectedNew[i],
                                  "HandoverComplete new " << i);
            NS_TEST_ASSERT_MSG_EQ_TOL(s.completes[i].timeS,
                                      expectedStartS[i] + 0.05,
                                      1e-9,
                                      "HandoverComplete time " << i);
        }
        NS_TEST_ASSERT_MSG_EQ(s.unavailable.size(), 1, "AccessUnavailable should fire once");
        NS_TEST_ASSERT_MSG_EQ(s.unavailable.at(0), TER, "wrong access reported unavailable");
        NS_TEST_ASSERT_MSG_EQ(s.lastNodeId, 7, "wrong node id in traces");
        // Evaluations at 0..9 s, all ticks fall outside the 50 ms handover windows.
        NS_TEST_ASSERT_MSG_EQ(s.decisions, 10, "wrong number of DecisionEvaluated events");

        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * ForceAccess overrides the policy and the dwell time, and refuses an
 * unavailable target.
 */
class TntnManagerForceAccessTestCase : public TestCase
{
  public:
    TntnManagerForceAccessTestCase()
        : TestCase("ForceAccess switches immediately and refuses unavailable access")
    {
    }

  private:
    void DoRun() override
    {
        TntnManagerScenario s;
        s.manager->Start();

        bool forced = false;
        bool refused = true;
        uint32_t at1s1 = 0;
        Simulator::Schedule(Seconds(1.5), [&]() { forced = s.manager->ForceAccess(NTN); });
        Simulator::Schedule(Seconds(1.6), [&]() { at1s1 = s.manager->GetCurrentAccess(); });
        Simulator::Schedule(Seconds(2.5), [&]() {
            s.ter->SetAvailable(false);
            refused = s.manager->ForceAccess(TER);
        });
        Simulator::Stop(Seconds(3));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(forced, true, "ForceAccess to an available access failed");
        NS_TEST_ASSERT_MSG_EQ(at1s1, NTN, "ForceAccess did not switch");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(s.starts.at(1).reason),
                              TntnReasonToString(TntnReason::FORCED),
                              "forced handover has wrong reason");
        NS_TEST_ASSERT_MSG_EQ(refused, false, "ForceAccess to an unavailable access succeeded");
        // The 2 s evaluation wants terrestrial back, but dwell holds NTN.
        NS_TEST_ASSERT_MSG_EQ(s.manager->GetCurrentAccess(), NTN, "dwell ignored after forcing");

        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * FR-10: make-before-break keeps the old access during a handover only when
 * it is enabled and the old access still works.
 *
 * Terrestrial is down from 1.5 s to 2.5 s: the 2 s handover leaves a failed
 * access, the 8 s handover (return after dwell) leaves a working one.
 */
class TntnManagerMakeBeforeBreakTestCase : public TestCase
{
  public:
    TntnManagerMakeBeforeBreakTestCase()
        : TestCase("FR-10 make-before-break only when enabled and old access works")
    {
    }

  private:
    /// Access in use at the probe times
    struct Probes
    {
        uint32_t leavingFailed;  //!< At 2.02 s, leaving failed terrestrial
        uint32_t leavingWorking; //!< At 8.02 s, leaving working NTN
        uint32_t after;          //!< At 9 s
        uint32_t lastOld;        //!< Old access reported by the last HandoverComplete
    };

    /**
     * Run the outage scenario.
     * @param makeBeforeBreak value of the MakeBeforeBreak attribute
     * @return the probed accesses
     */
    static Probes Run(bool makeBeforeBreak)
    {
        TntnManagerScenario s;
        s.manager->SetAttribute("MakeBeforeBreak", BooleanValue(makeBeforeBreak));
        s.ter->ScheduleAvailable(Seconds(1.5), false);
        s.ter->ScheduleAvailable(Seconds(2.5), true);
        s.manager->Start();

        Probes p{};
        Simulator::Schedule(Seconds(2.02),
                            [&]() { p.leavingFailed = s.manager->GetCurrentAccess(); });
        Simulator::Schedule(Seconds(8.02),
                            [&]() { p.leavingWorking = s.manager->GetCurrentAccess(); });
        Simulator::Schedule(Seconds(9), [&]() { p.after = s.manager->GetCurrentAccess(); });
        Simulator::Stop(Seconds(10));
        Simulator::Run();
        p.lastOld = s.completes.empty() ? TNTN_NO_ACCESS : s.completes.back().oldId;
        Simulator::Destroy();
        return p;
    }

    void DoRun() override
    {
        BooleanValue mbb;
        CreateObject<TntnAccessManager>()->GetAttribute("MakeBeforeBreak", mbb);
        NS_TEST_ASSERT_MSG_EQ(mbb.Get(), false, "MakeBeforeBreak default should be false");

        Probes off = Run(false);
        NS_TEST_ASSERT_MSG_EQ(off.leavingFailed, TNTN_NO_ACCESS, "off: detached from failed");
        NS_TEST_ASSERT_MSG_EQ(off.leavingWorking,
                              TNTN_NO_ACCESS,
                              "off: break-before-make should detach even from a working access");
        NS_TEST_ASSERT_MSG_EQ(off.after, TER, "off: handover did not complete");
        NS_TEST_ASSERT_MSG_EQ(off.lastOld, NTN, "off: HandoverComplete old access");

        Probes on = Run(true);
        NS_TEST_ASSERT_MSG_EQ(on.leavingFailed,
                              TNTN_NO_ACCESS,
                              "on: a failed access cannot be kept");
        NS_TEST_ASSERT_MSG_EQ(on.leavingWorking,
                              NTN,
                              "on: working old access should stay current during handover");
        NS_TEST_ASSERT_MSG_EQ(on.after, TER, "on: handover did not complete");
        NS_TEST_ASSERT_MSG_EQ(on.lastOld, NTN, "on: HandoverComplete old access");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Guideline section 8.4: two identical runs produce identical handover traces.
 */
class TntnManagerRepeatabilityTestCase : public TestCase
{
  public:
    TntnManagerRepeatabilityTestCase()
        : TestCase("Identical runs produce identical handover traces")
    {
    }

  private:
    /**
     * Run a scripted outage scenario.
     * @return the HandoverStart events
     */
    static std::vector<TntnManagerScenario::Handover> RunOnce()
    {
        TntnManagerScenario s;
        s.ter->ScheduleAvailable(Seconds(3.5), false);
        s.ter->ScheduleAvailable(Seconds(4.5), true);
        s.ter->ScheduleDelay(Seconds(12.5), MilliSeconds(200));
        s.manager->Start();
        Simulator::Stop(Seconds(20));
        Simulator::Run();
        Simulator::Destroy();
        return s.starts;
    }

    void DoRun() override
    {
        auto a = RunOnce();
        auto b = RunOnce();
        NS_TEST_ASSERT_MSG_EQ(a.size(), b.size(), "different number of handovers");
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
        {
            NS_TEST_ASSERT_MSG_EQ(a[i].timeS, b[i].timeS, "handover " << i << " time differs");
            NS_TEST_ASSERT_MSG_EQ(a[i].newId, b[i].newId, "handover " << i << " target differs");
        }
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Manager defaults and attribute ranges (SRS section 5).
 */
class TntnManagerAttributesTestCase : public TestCase
{
  public:
    TntnManagerAttributesTestCase()
        : TestCase("Access manager defaults and attribute ranges")
    {
    }

  private:
    void DoRun() override
    {
        auto m = CreateObject<TntnAccessManager>();
        TimeValue t;
        m->GetAttribute("DecisionInterval", t);
        NS_TEST_ASSERT_MSG_EQ(t.Get(), Seconds(1), "default DecisionInterval");
        m->GetAttribute("MinDwellTime", t);
        NS_TEST_ASSERT_MSG_EQ(t.Get(), Seconds(5), "default MinDwellTime");
        m->GetAttribute("HandoverExecutionDelay", t);
        NS_TEST_ASSERT_MSG_EQ(t.Get(), MilliSeconds(50), "default HandoverExecutionDelay");
        NS_TEST_ASSERT_MSG_EQ(m->GetCurrentAccess(), TNTN_NO_ACCESS, "initial access");
        NS_TEST_ASSERT_MSG_EQ(m->IsHandoverInProgress(), false, "initial handover state");

        NS_TEST_ASSERT_MSG_EQ(
            m->SetAttributeFailSafe("DecisionInterval", TimeValue(MilliSeconds(99))),
            false,
            "DecisionInterval below 100 ms should be rejected");
        NS_TEST_ASSERT_MSG_EQ(m->SetAttributeFailSafe("DecisionInterval", TimeValue(Seconds(11))),
                              false,
                              "DecisionInterval above 10 s should be rejected");
        NS_TEST_ASSERT_MSG_EQ(m->SetAttributeFailSafe("MinDwellTime", TimeValue(Seconds(0))),
                              true,
                              "MinDwellTime 0 should be accepted");
        NS_TEST_ASSERT_MSG_EQ(m->SetAttributeFailSafe("MinDwellTime", TimeValue(Seconds(61))),
                              false,
                              "MinDwellTime above 60 s should be rejected");
        NS_TEST_ASSERT_MSG_EQ(
            m->SetAttributeFailSafe("HandoverExecutionDelay", TimeValue(Seconds(2.5))),
            false,
            "HandoverExecutionDelay above 2 s should be rejected");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * T6 (guidelines section 9): the point-to-point provider reports a real ns-3
 * link correctly, follows live changes, and drives the manager.
 */
class TntnPointToPointProviderTestCase : public TestCase
{
  public:
    TntnPointToPointProviderTestCase()
        : TestCase("T6 point-to-point provider reads a real ns-3 link and drives the manager")
    {
    }

  private:
    void DoRun() override
    {
        // Two nodes joined by two links: link A plays terrestrial, link B NTN.
        NodeContainer nodes;
        nodes.Create(2);
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("15ms"));
        NetDeviceContainer linkA = p2p.Install(nodes);
        p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("270ms"));
        NetDeviceContainer linkB = p2p.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        address.Assign(linkA);
        address.SetBase("10.2.1.0", "255.255.255.0");
        address.Assign(linkB);

        // 2% packet loss on link B, applied by the far end's receive error model.
        auto errors = CreateObject<RateErrorModel>();
        errors->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
        errors->SetRate(0.02);
        linkB.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errors));

        auto ter = CreateObject<TntnPointToPointAccessStateProvider>();
        ter->SetDevice(DynamicCast<PointToPointNetDevice>(linkA.Get(0)));
        auto ntn = CreateObject<TntnPointToPointAccessStateProvider>();
        ntn->SetAttribute("AccessType", EnumValue(TntnAccessType::NTN));
        ntn->SetDevice(DynamicCast<PointToPointNetDevice>(linkB.Get(0)));

        // The report card matches the link configuration.
        TntnAccessState a = ter->GetAccessState(TER);
        NS_TEST_ASSERT_MSG_EQ(a.accessId, TER, "access id not copied");
        NS_TEST_ASSERT_MSG_EQ((a.type == TntnAccessType::TERRESTRIAL), true, "wrong type");
        NS_TEST_ASSERT_MSG_EQ(a.available, true, "link A should be available");
        NS_TEST_ASSERT_MSG_EQ(a.delay, MilliSeconds(15), "link A delay");
        NS_TEST_ASSERT_MSG_EQ(a.rate, DataRate("50Mbps"), "link A rate");
        NS_TEST_ASSERT_MSG_EQ_TOL(a.lossRate, 0.0, 1e-12, "link A has no error model");
        NS_TEST_ASSERT_MSG_EQ_TOL(a.load, 0.0, 1e-12, "idle link should have zero load");

        TntnAccessState b = ntn->GetAccessState(NTN);
        NS_TEST_ASSERT_MSG_EQ((b.type == TntnAccessType::NTN), true, "AccessType attribute");
        NS_TEST_ASSERT_MSG_EQ(b.delay, MilliSeconds(270), "link B delay");
        NS_TEST_ASSERT_MSG_EQ(b.rate, DataRate("10Mbps"), "link B rate");
        NS_TEST_ASSERT_MSG_EQ_TOL(b.lossRate, 0.02, 1e-12, "link B loss from error model");

        // Live changes to the link are visible on the next read.
        linkA.Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(MilliSeconds(200)));
        NS_TEST_ASSERT_MSG_EQ(ter->GetAccessState(TER).delay,
                              MilliSeconds(200),
                              "delay change not seen");
        linkA.Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(MilliSeconds(15)));

        Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();
        int32_t ifA = ipv4->GetInterfaceForDevice(linkA.Get(0));
        ipv4->SetDown(ifA);
        NS_TEST_ASSERT_MSG_EQ(ter->GetAccessState(TER).available,
                              false,
                              "interface down should mean unavailable");
        ipv4->SetUp(ifA);
        NS_TEST_ASSERT_MSG_EQ(ter->GetAccessState(TER).available,
                              true,
                              "interface up should mean available");

        // The manager reacts to a real interface outage.
        auto manager = CreateObject<TntnAccessManager>();
        manager->AddAccess(TER, ter);
        manager->AddAccess(NTN, ntn);
        manager->SetPolicy(CreateObject<TntnTerrestrialPreferredPolicy>());
        manager->Start();
        uint32_t beforeOutage = 0;
        Simulator::Schedule(Seconds(1.5), [&]() {
            beforeOutage = manager->GetCurrentAccess();
            ipv4->SetDown(ifA);
        });
        Simulator::Stop(Seconds(3));
        Simulator::Run();

        NS_TEST_ASSERT_MSG_EQ(beforeOutage, TER, "terrestrial not selected before outage");
        NS_TEST_ASSERT_MSG_EQ(manager->GetCurrentAccess(), NTN, "no fallback to NTN link");
        Simulator::Destroy();
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Build the terrestrial/NTN pair used by the weighted-score tests.
 * @param terLoss terrestrial loss ratio
 * @param terLoad terrestrial load
 * @return terrestrial 30 ms / 50 Mbit/s and NTN 540 ms / 10 Mbit/s / 2% loss / 0.2 load
 */
static std::vector<TntnAccessState>
WeightedPair(double terLoss, double terLoad)
{
    TntnAccessState ter = MakeState(TER, TntnAccessType::TERRESTRIAL, true, 30, terLoss, 50);
    ter.load = terLoad;
    TntnAccessState ntn = MakeState(NTN, TntnAccessType::NTN, true, 540, 0.02, 10);
    ntn.load = 0.2;
    return {ter, ntn};
}

/**
 * @ingroup futurenet-tntn-tests
 * FR-09: scores follow the documented formula, and weights are normalized.
 */
class TntnWeightedScoreFormulaTestCase : public TestCase
{
  public:
    TntnWeightedScoreFormulaTestCase()
        : TestCase("FR-09 weighted score formula and weight normalization")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnWeightedScorePolicy>();
        std::vector<TntnAccessState> c = WeightedPair(0.01, 0.2);

        // Marks: TER delay 1, loss 0.99, rate 1, load 0.8
        //        NTN delay 30/540, loss 0.98, rate 10/50, load 0.8
        const double terExpected = 0.4 * 1.0 + 0.2 * 0.99 + 0.2 * 1.0 + 0.2 * 0.8;
        const double ntnExpected = 0.4 * (30.0 / 540.0) + 0.2 * 0.98 + 0.2 * 0.2 + 0.2 * 0.8;
        std::vector<double> s = policy->ComputeScores(c);
        NS_TEST_ASSERT_MSG_EQ_TOL(s.at(0), terExpected, 1e-9, "terrestrial score");
        NS_TEST_ASSERT_MSG_EQ_TOL(s.at(1), ntnExpected, 1e-9, "NTN score");

        TntnAccessDecision d = policy->Select(c, TNTN_NO_ACCESS);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, TER, "highest score not selected");
        NS_TEST_ASSERT_MSG_EQ_TOL(d.score, terExpected, 1e-9, "decision score");

        // Weights 4/2/2/2 have the same proportions as 0.4/0.2/0.2/0.2.
        policy->SetAttribute("DelayWeight", DoubleValue(4));
        policy->SetAttribute("LossWeight", DoubleValue(2));
        policy->SetAttribute("RateWeight", DoubleValue(2));
        policy->SetAttribute("LoadWeight", DoubleValue(2));
        std::vector<double> scaled = policy->ComputeScores(c);
        NS_TEST_ASSERT_MSG_EQ_TOL(scaled.at(0), terExpected, 1e-9, "weights not normalized");
        NS_TEST_ASSERT_MSG_EQ_TOL(scaled.at(1), ntnExpected, 1e-9, "weights not normalized");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * FR-09: the weights decide which metric dominates.
 */
class TntnWeightedScoreWeightsTestCase : public TestCase
{
  public:
    TntnWeightedScoreWeightsTestCase()
        : TestCase("FR-09 changing the weights changes the selection")
    {
    }

  private:
    void DoRun() override
    {
        // Terrestrial is fast but loses 30% and is nearly saturated.
        std::vector<TntnAccessState> c = WeightedPair(0.3, 0.95);

        auto delayFirst = CreateObject<TntnWeightedScorePolicy>();
        NS_TEST_ASSERT_MSG_EQ(delayFirst->Select(c, TNTN_NO_ACCESS).selectedAccessId,
                              TER,
                              "default weights should favour the much faster access");

        auto lossFirst = CreateObject<TntnWeightedScorePolicy>();
        lossFirst->SetAttribute("DelayWeight", DoubleValue(0.1));
        lossFirst->SetAttribute("LossWeight", DoubleValue(0.8));
        lossFirst->SetAttribute("RateWeight", DoubleValue(0.05));
        lossFirst->SetAttribute("LoadWeight", DoubleValue(0.05));
        NS_TEST_ASSERT_MSG_EQ(lossFirst->Select(c, TNTN_NO_ACCESS).selectedAccessId,
                              NTN,
                              "loss-heavy weights should favour the reliable access");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Weighted-score hysteresis and reasons.
 */
class TntnWeightedScoreHysteresisTestCase : public TestCase
{
  public:
    TntnWeightedScoreHysteresisTestCase()
        : TestCase("Weighted-score hysteresis blocks small score improvements")
    {
    }

  private:
    void DoRun() override
    {
        // Identical accesses except load, so score = 0.8 + 0.2 * (1 - load).
        auto pair = [](double terLoad, double ntnLoad) {
            TntnAccessState ter = MakeState(TER, TntnAccessType::TERRESTRIAL, true, 100);
            ter.load = terLoad;
            TntnAccessState ntn = MakeState(NTN, TntnAccessType::NTN, true, 100);
            ntn.load = ntnLoad;
            return std::vector<TntnAccessState>{ter, ntn};
        };
        auto policy = CreateObject<TntnWeightedScorePolicy>();

        // 0.96 -> 1.00 is about 4% better: blocked by the default 5%.
        TntnAccessDecision d = policy->Select(pair(0.2, 0.0), TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, TER, "4% improvement should not switch");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::NONE),
                              "wrong reason when staying");

        // 0.88 -> 1.00 is about 14% better: switch.
        d = policy->Select(pair(0.6, 0.0), TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, NTN, "14% improvement should switch");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::BETTER_CANDIDATE),
                              "wrong reason when switching");

        // With no hysteresis the 4% improvement is enough.
        policy->SetAttribute("HysteresisFraction", DoubleValue(0.0));
        NS_TEST_ASSERT_MSG_EQ(policy->Select(pair(0.2, 0.0), TER).selectedAccessId,
                              NTN,
                              "any improvement switches at 0 hysteresis");

        // Current access down: switch at once whatever the scores.
        policy->SetAttribute("HysteresisFraction", DoubleValue(0.5));
        auto c = pair(0.2, 0.0);
        c[0].available = false;
        d = policy->Select(c, TER);
        NS_TEST_ASSERT_MSG_EQ(d.selectedAccessId, NTN, "unavailable current not left");
        NS_TEST_ASSERT_MSG_EQ(TntnReasonToString(d.reason),
                              TntnReasonToString(TntnReason::CURRENT_UNAVAILABLE),
                              "wrong reason when current is unavailable");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Weighted-score defaults and attribute ranges (SRS section 5).
 */
class TntnWeightedScoreAttributesTestCase : public TestCase
{
  public:
    TntnWeightedScoreAttributesTestCase()
        : TestCase("Weighted-score policy defaults and attribute ranges")
    {
    }

  private:
    void DoRun() override
    {
        auto policy = CreateObject<TntnWeightedScorePolicy>();
        const std::pair<const char*, double> defaults[] = {{"DelayWeight", 0.40},
                                                           {"LossWeight", 0.20},
                                                           {"RateWeight", 0.20},
                                                           {"LoadWeight", 0.20},
                                                           {"HysteresisFraction", 0.05}};
        for (const auto& [name, value] : defaults)
        {
            DoubleValue v;
            policy->GetAttribute(name, v);
            NS_TEST_ASSERT_MSG_EQ_TOL(v.Get(), value, 1e-12, "default " << name);
        }
        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("DelayWeight", DoubleValue(-0.1)),
                              false,
                              "negative weight should be rejected");
        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("LoadWeight", DoubleValue(0.0)),
                              true,
                              "zero weight should be accepted");
        NS_TEST_ASSERT_MSG_EQ(policy->SetAttributeFailSafe("HysteresisFraction", DoubleValue(0.51)),
                              false,
                              "HysteresisFraction above 0.5 should be rejected");
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Test suite for module futurenet-tntn.
 */
class TntnTestSuite : public TestSuite
{
  public:
    TntnTestSuite()
        : TestSuite("futurenet-tntn", Type::UNIT)
    {
        AddTestCase(new TntnMockDefaultsTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnMockAttributeBoundsTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnMockScheduleTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnUnavailableNeverSelectedTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnTerrestrialPreferredRuleTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnTerrestrialPreferredAttributesTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnLowestDelaySelectsMinimumTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnLowestDelayHysteresisTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnLowestDelayAttributesTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnManagerDwellAndTraceTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnManagerForceAccessTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnManagerMakeBeforeBreakTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnManagerRepeatabilityTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnManagerAttributesTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnPointToPointProviderTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnWeightedScoreFormulaTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnWeightedScoreWeightsTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnWeightedScoreHysteresisTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TntnWeightedScoreAttributesTestCase, TestCase::Duration::QUICK);
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Static variable for test initialization
 */
static TntnTestSuite g_tntnTestSuite;
