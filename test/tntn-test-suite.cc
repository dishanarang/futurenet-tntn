#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/tntn-lowest-delay-policy.h"
#include "ns3/tntn-mock-access-state-provider.h"
#include "ns3/tntn-terrestrial-preferred-policy.h"

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
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Static variable for test initialization
 */
static TntnTestSuite g_tntnTestSuite;
