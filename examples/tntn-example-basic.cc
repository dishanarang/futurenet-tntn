/*
 * Basic M02 example: many nodes, each with a terrestrial and an NTN access
 * modelled by mock providers. Every node's terrestrial access suffers one
 * outage and one congestion period at random times, so nodes fall back to NTN
 * and later return, subject to the policy, hysteresis and dwell time.
 *
 * Output (machine-readable):
 *   <outputPrefix>-decisions.csv  one row per evaluation per node
 *   <outputPrefix>-events.csv     handover and availability events
 *
 *   ./ns3 run "tntn-example-basic --numNodes=25 --simulationTime=60s"
 *   ./ns3 run "tntn-example-basic --policy=lowest-delay --RngRun=2"
 *   ./ns3 run "tntn-example-basic --ns3::TntnAccessManager::MinDwellTime=0s"
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tntn-helper.h"
#include "ns3/tntn-mock-access-state-provider.h"

#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TntnExampleBasic");

/// Terrestrial access id on every node
static constexpr uint32_t TER = 0;
/// NTN access id on every node
static constexpr uint32_t NTN = 1;

/**
 * Count handovers per reason.
 * @param counts map to update
 * @param nodeId node id
 * @param oldId old access
 * @param newId new access
 * @param reason reason
 */
static void
CountHandover(std::map<std::string, uint32_t>* counts,
              uint32_t nodeId,
              uint32_t oldId,
              uint32_t newId,
              TntnReason reason)
{
    (*counts)[TntnReasonToString(reason)]++;
}

int
main(int argc, char* argv[])
{
    uint32_t numNodes = 25;
    Time simulationTime = Seconds(60);
    std::string policy = "terrestrial";
    std::string outputPrefix = "tntn-example-basic";
    Time terDelay = MilliSeconds(30);
    Time terCongestedDelay = MilliSeconds(200);
    Time ntnDelay = MilliSeconds(540);
    double ntnLoss = 0.02;
    int64_t stream = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of managed nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation duration", simulationTime);
    cmd.AddValue("policy", "Selection policy: terrestrial or lowest-delay", policy);
    cmd.AddValue("outputPrefix", "Prefix of the CSV output files", outputPrefix);
    cmd.AddValue("terDelay", "Normal terrestrial one-way delay", terDelay);
    cmd.AddValue("terCongestedDelay", "Terrestrial delay while congested", terCongestedDelay);
    cmd.AddValue("ntnDelay", "NTN one-way delay", ntnDelay);
    cmd.AddValue("ntnLoss", "NTN loss ratio [0,1]", ntnLoss);
    cmd.AddValue("stream", "First random stream index for the scenario", stream);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(numNodes == 0, "numNodes must be > 0");
    NS_ABORT_MSG_IF(!simulationTime.IsStrictlyPositive(), "simulationTime must be > 0");

    NodeContainer nodes;
    nodes.Create(numNodes);

    TntnHelper tntn;
    if (policy == "terrestrial")
    {
        tntn.SetPolicy("ns3::TntnTerrestrialPreferredPolicy");
    }
    else if (policy == "lowest-delay")
    {
        tntn.SetPolicy("ns3::TntnLowestDelayPolicy");
    }
    else
    {
        NS_ABORT_MSG("Unknown --policy '" << policy << "'; use terrestrial or lowest-delay");
    }
    tntn.AddAccess(TER,
                   "ns3::TntnMockAccessStateProvider",
                   "AccessType",
                   EnumValue(TntnAccessType::TERRESTRIAL),
                   "Delay",
                   TimeValue(terDelay));
    tntn.AddAccess(NTN,
                   "ns3::TntnMockAccessStateProvider",
                   "AccessType",
                   EnumValue(TntnAccessType::NTN),
                   "Delay",
                   TimeValue(ntnDelay),
                   "LossRate",
                   DoubleValue(ntnLoss),
                   "Rate",
                   DataRateValue(DataRate("10Mbps")));
    std::vector<Ptr<TntnAccessManager>> managers = tntn.Install(nodes);
    tntn.EnableTracing(outputPrefix);

    // Scenario: one outage and one congestion period per node, at random times
    // within the run. Fractions of the run keep the script valid for any length.
    double runS = simulationTime.GetSeconds();
    auto rv = CreateObject<UniformRandomVariable>();
    rv->SetStream(stream);
    for (const auto& manager : managers)
    {
        auto ter = DynamicCast<TntnMockAccessStateProvider>(manager->GetAccessProvider(TER));

        double outageStart = rv->GetValue(0.1 * runS, 0.6 * runS);
        double outageLength = rv->GetValue(0.03 * runS, 0.25 * runS);
        ter->ScheduleAvailable(Seconds(outageStart), false);
        ter->ScheduleAvailable(Seconds(outageStart + outageLength), true);

        double congestionStart = rv->GetValue(0.1 * runS, 0.9 * runS);
        double congestionLength = rv->GetValue(0.03 * runS, 0.15 * runS);
        ter->ScheduleDelay(Seconds(congestionStart), terCongestedDelay);
        ter->ScheduleDelay(Seconds(congestionStart + congestionLength), terDelay);
    }

    std::map<std::string, uint32_t> handovers;
    for (const auto& manager : managers)
    {
        manager->TraceConnectWithoutContext("HandoverStart",
                                            MakeBoundCallback(&CountHandover, &handovers));
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();

    uint32_t onTer = 0;
    for (const auto& manager : managers)
    {
        onTer += (manager->GetCurrentAccess() == TER) ? 1 : 0;
    }
    std::cout << "Nodes: " << numNodes << ", policy: " << policy
              << ", run: " << simulationTime.As(Time::S) << "\nHandovers by reason:\n";
    for (const auto& [reason, count] : handovers)
    {
        std::cout << "  " << reason << ": " << count << "\n";
    }
    std::cout << "Nodes on terrestrial at end: " << onTer << "/" << numNodes << "\n"
              << "CSV: " << outputPrefix << "-decisions.csv, " << outputPrefix << "-events.csv"
              << std::endl;

    Simulator::Destroy();
    return 0;
}
