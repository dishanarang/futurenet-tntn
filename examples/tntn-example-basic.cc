/*
 * Two abstract accesses (terrestrial + NTN) driven by mock providers with
 * scheduled availability/delay changes. Prints both access states once per
 * second; the access manager and CSV tracing are added in later steps.
 *
 *   ./ns3 run tntn-example-basic
 */

#include "ns3/core-module.h"
#include "ns3/tntn-mock-access-state-provider.h"

#include <iostream>

using namespace ns3;

/**
 * Print the state of both accesses at the current simulation time.
 * @param ter terrestrial access provider
 * @param ntn NTN access provider
 */
static void
PrintState(Ptr<TntnMockAccessStateProvider> ter, Ptr<TntnMockAccessStateProvider> ntn)
{
    TntnAccessState t = ter->GetAccessState(0);
    TntnAccessState n = ntn->GetAccessState(1);
    std::cout << "t=" << Simulator::Now().GetSeconds() << "s  "
              << "TER avail=" << t.available << " delay=" << t.delay.GetMilliSeconds() << "ms  |  "
              << "NTN avail=" << n.available << " delay=" << n.delay.GetMilliSeconds() << "ms"
              << std::endl;
}

int
main(int argc, char* argv[])
{
    Time simulationTime = Seconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation duration", simulationTime);
    cmd.Parse(argc, argv);

    Ptr<TntnMockAccessStateProvider> ter = CreateObject<TntnMockAccessStateProvider>();
    ter->SetAccessType(TntnAccessType::TERRESTRIAL);
    ter->SetDelay(MilliSeconds(30));

    // Illustrative GEO-like figures, not taken from any standard
    Ptr<TntnMockAccessStateProvider> ntn = CreateObject<TntnMockAccessStateProvider>();
    ntn->SetAccessType(TntnAccessType::NTN);
    ntn->SetDelay(MilliSeconds(540));
    ntn->SetLossRate(0.02);
    ntn->SetRate(DataRate("10Mbps"));

    // Terrestrial outage from 3 s to 6 s, then congestion from 8 s
    ter->ScheduleAvailable(Seconds(3), false);
    ter->ScheduleAvailable(Seconds(6), true);
    ter->ScheduleDelay(Seconds(8), MilliSeconds(200));

    for (Time t = Seconds(0); t <= simulationTime; t += Seconds(1))
    {
        Simulator::Schedule(t, &PrintState, ter, ntn);
    }

    Simulator::Stop(simulationTime + MilliSeconds(1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
