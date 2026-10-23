#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/tntn-mock-access-state-provider.h"

using namespace ns3;

/**
 * @defgroup futurenet-tntn-tests Tests for futurenet-tntn
 * @ingroup futurenet-tntn
 * @ingroup tests
 */

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
    }
};

/**
 * @ingroup futurenet-tntn-tests
 * Static variable for test initialization
 */
static TntnTestSuite g_tntnTestSuite;
