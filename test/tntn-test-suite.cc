#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
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
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Static variable for test initialization
 */
static TntnTestSuite g_tntnTestSuite;
