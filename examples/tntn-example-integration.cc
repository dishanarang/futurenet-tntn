/*
 * Integration example (SRS "example-tntn"): M02 driving real ns-3 links.
 *
 *            terrestrial link (50 Mbit/s, 15 ms)
 *   UE i  ====================================  Router ---- Server
 *         ====================================          LAN
 *            NTN link (10 Mbit/s, 270 ms, GEO-like)
 *
 * Each UE sends UDP packets to the server. Its TntnAccessManager reads both
 * links through TntnPointToPointAccessStateProvider. When a handover
 * completes, the UE's default route is moved to the new link, so traffic
 * really changes path (M02-FR-07). The server records the one-way delay and
 * the path of every packet.
 *
 * Scenario on every UE (offset by i seconds for UE i):
 *   20.5-23.5 s  terrestrial IPv4 interface down -> fallback to NTN; return held by dwell
 *   40.5-43.5 s  terrestrial delay 200 ms        -> DELAY_EXCEEDED; return held by dwell
 *
 * Packets sent while the current access is down are lost until the fallback
 * completes. Right after a return, packets on the fast link overtake those
 * still in flight on the slow link, so the server sees reordering.
 *
 * The NTN link stands in for the M01 satellite path until an M01 provider
 * is available; only the provider would change.
 *
 * Output:
 *   <outputPrefix>-packets.csv     one row per received packet
 *   <outputPrefix>-decisions.csv   manager decisions (TntnHelper)
 *   <outputPrefix>-events.csv      handover events (TntnHelper)
 *
 *   ./ns3 run tntn-example-integration
 *   ./ns3 run "tntn-example-integration --numUes=5 --policy=lowest-delay"
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tntn-helper.h"
#include "ns3/tntn-point-to-point-access-state-provider.h"

#include <fstream>
#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TntnExampleIntegration");

/// Terrestrial access id on every UE
static constexpr uint32_t TER = 0;
/// NTN access id on every UE
static constexpr uint32_t NTN = 1;
/// First UDP port; UE i uses BASE_PORT + i
static constexpr uint16_t BASE_PORT = 9000;

/// Routing information of one UE
struct UeRoutes
{
    Ptr<Node> ue;                         //!< UE node
    std::map<uint32_t, Ipv4Address> gw;   //!< Next hop per access id
    std::map<uint32_t, uint32_t> ifIndex; //!< UE IPv4 interface per access id
};

/**
 * Point the UE's default route at the access that just became current.
 * This is the forwarding update of a logical handover (M02-FR-07).
 * @param routes the UE's routing information
 * @param nodeId node id
 * @param oldId previous access
 * @param newId new access
 */
static void
OnHandoverComplete(UeRoutes* routes, uint32_t nodeId, uint32_t oldId, uint32_t newId)
{
    Ipv4StaticRoutingHelper helper;
    Ptr<Ipv4StaticRouting> table = helper.GetStaticRouting(routes->ue->GetObject<Ipv4>());
    for (uint32_t i = table->GetNRoutes(); i > 0; --i)
    {
        if (table->GetRoute(i - 1).IsDefault())
        {
            table->RemoveRoute(i - 1);
        }
    }
    table->SetDefaultRoute(routes->gw.at(newId), routes->ifIndex.at(newId));
    NS_LOG_INFO("UE " << nodeId << " default route now via access " << newId);
}

/**
 * Record a packet received by the server.
 * @param csv output file
 * @param ueNodeId sender
 * @param packet received packet
 * @param from sender address
 * @param to local address
 */
static void
OnServerRx(std::ofstream* csv,
           uint32_t ueNodeId,
           Ptr<const Packet> packet,
           const Address& from,
           const Address& to)
{
    SeqTsHeader header;
    packet->PeekHeader(header);
    Time delay = Simulator::Now() - header.GetTs();
    // The source address shows which link the packet used: 10.1.x.x or 10.2.x.x.
    uint32_t secondOctet = (InetSocketAddress::ConvertFrom(from).GetIpv4().Get() >> 16) & 0xff;
    *csv << Simulator::Now().GetSeconds() << ',' << ueNodeId << ','
         << (secondOctet == 1 ? "TER" : "NTN") << ',' << header.GetSeq() << ','
         << delay.ToDouble(Time::MS) << '\n';
}

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

/**
 * Count a send attempt. UdpClient fires Tx before sending and does not
 * advance its sequence number when the send fails (e.g. no route during an
 * outage), so the server alone cannot see those losses.
 * @param attempts counter to update
 * @param packet the packet
 */
static void
CountAttempt(uint32_t* attempts, Ptr<const Packet> packet)
{
    (*attempts)++;
}

/**
 * Change a point-to-point channel delay.
 * @param device a device on the channel
 * @param delay new delay
 */
static void
SetLinkDelay(Ptr<NetDevice> device, Time delay)
{
    device->GetChannel()->SetAttribute("Delay", TimeValue(delay));
}

int
main(int argc, char* argv[])
{
    uint32_t numUes = 3;
    Time simulationTime = Seconds(60);
    std::string policy = "terrestrial";
    std::string outputPrefix = "tntn-example-integration";
    Time packetInterval = MilliSeconds(100);

    CommandLine cmd(__FILE__);
    cmd.AddValue("numUes", "Number of UEs", numUes);
    cmd.AddValue("simulationTime", "Simulation duration", simulationTime);
    cmd.AddValue("policy", "Selection policy: terrestrial, lowest-delay or weighted", policy);
    cmd.AddValue("outputPrefix", "Prefix of the CSV output files", outputPrefix);
    cmd.AddValue("packetInterval", "Time between UDP packets of a UE", packetInterval);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(numUes == 0 || numUes > 250, "numUes must be in [1,250]");
    NS_ABORT_MSG_IF(simulationTime < Seconds(60), "the scripted scenario needs >= 60 s");

    // ---- Topology ----
    NodeContainer ues;
    ues.Create(numUes);
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> server = CreateObject<Node>();

    InternetStackHelper stack;
    stack.InstallAll();

    PointToPointHelper lan;
    lan.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    lan.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer lanDevices = lan.Install(router, server);
    Ipv4AddressHelper address;
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer lanIf = address.Assign(lanDevices);
    Ipv4Address serverAddress = lanIf.GetAddress(1);

    PointToPointHelper terLink;
    terLink.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    terLink.SetChannelAttribute("Delay", StringValue("15ms"));
    PointToPointHelper ntnLink;
    ntnLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    ntnLink.SetChannelAttribute("Delay", StringValue("270ms"));

    std::vector<NetDeviceContainer> terDevices(numUes);
    std::vector<NetDeviceContainer> ntnDevices(numUes);
    std::vector<UeRoutes> routes(numUes);
    for (uint32_t i = 0; i < numUes; ++i)
    {
        Ptr<Node> ue = ues.Get(i);
        terDevices[i] = terLink.Install(ue, router);
        ntnDevices[i] = ntnLink.Install(ue, router);

        std::ostringstream terNet;
        terNet << "10.1." << i << ".0";
        address.SetBase(terNet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer terIf = address.Assign(terDevices[i]);
        std::ostringstream ntnNet;
        ntnNet << "10.2." << i << ".0";
        address.SetBase(ntnNet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer ntnIf = address.Assign(ntnDevices[i]);

        Ptr<Ipv4> ipv4 = ue->GetObject<Ipv4>();
        routes[i].ue = ue;
        routes[i].gw[TER] = terIf.GetAddress(1);
        routes[i].gw[NTN] = ntnIf.GetAddress(1);
        routes[i].ifIndex[TER] = ipv4->GetInterfaceForDevice(terDevices[i].Get(0));
        routes[i].ifIndex[NTN] = ipv4->GetInterfaceForDevice(ntnDevices[i].Get(0));
    }
    // The server never sends, and the router reaches every UE and the server
    // directly, so only the UEs need routes: their default route, set on handover.

    // ---- M02 ----
    TntnHelper tntn;
    if (policy == "terrestrial")
    {
        tntn.SetPolicy("ns3::TntnTerrestrialPreferredPolicy");
    }
    else if (policy == "lowest-delay")
    {
        tntn.SetPolicy("ns3::TntnLowestDelayPolicy");
    }
    else if (policy == "weighted")
    {
        tntn.SetPolicy("ns3::TntnWeightedScorePolicy");
    }
    else
    {
        NS_ABORT_MSG("Unknown --policy '" << policy
                                          << "'; use terrestrial, lowest-delay or weighted");
    }
    tntn.AddAccess(TER,
                   "ns3::TntnPointToPointAccessStateProvider",
                   "AccessType",
                   EnumValue(TntnAccessType::TERRESTRIAL));
    tntn.AddAccess(NTN,
                   "ns3::TntnPointToPointAccessStateProvider",
                   "AccessType",
                   EnumValue(TntnAccessType::NTN));
    std::vector<Ptr<TntnAccessManager>> managers = tntn.Install(ues);
    tntn.EnableTracing(outputPrefix);

    for (uint32_t i = 0; i < numUes; ++i)
    {
        // The helper creates the providers; tell each one which link it watches.
        DynamicCast<TntnPointToPointAccessStateProvider>(managers[i]->GetAccessProvider(TER))
            ->SetDevice(DynamicCast<PointToPointNetDevice>(terDevices[i].Get(0)));
        DynamicCast<TntnPointToPointAccessStateProvider>(managers[i]->GetAccessProvider(NTN))
            ->SetDevice(DynamicCast<PointToPointNetDevice>(ntnDevices[i].Get(0)));
        managers[i]->TraceConnectWithoutContext("HandoverComplete",
                                                MakeBoundCallback(&OnHandoverComplete, &routes[i]));
    }

    // ---- Traffic ----
    std::ofstream packetsCsv(outputPrefix + "-packets.csv");
    packetsCsv << "time_s,ue_node_id,path,seq,one_way_delay_ms\n";
    std::vector<Ptr<UdpServer>> servers(numUes);
    std::vector<uint32_t> attempts(numUes, 0);
    for (uint32_t i = 0; i < numUes; ++i)
    {
        uint16_t port = BASE_PORT + i;
        UdpServerHelper serverHelper(port);
        ApplicationContainer serverApp = serverHelper.Install(server);
        servers[i] = DynamicCast<UdpServer>(serverApp.Get(0));
        servers[i]->TraceConnectWithoutContext(
            "RxWithAddresses",
            MakeBoundCallback(&OnServerRx, &packetsCsv, ues.Get(i)->GetId()));

        UdpClientHelper client(serverAddress, port);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(packetInterval));
        client.SetAttribute("PacketSize", UintegerValue(200));
        ApplicationContainer clientApp = client.Install(ues.Get(i));
        clientApp.Get(0)->TraceConnectWithoutContext(
            "Tx",
            MakeBoundCallback(&CountAttempt, &attempts[i]));
        // Start after the initial handover has installed a default route.
        clientApp.Start(Seconds(1));
        clientApp.Stop(simulationTime);
    }

    // ---- Scenario ----
    for (uint32_t i = 0; i < numUes; ++i)
    {
        Ptr<Ipv4> ipv4 = ues.Get(i)->GetObject<Ipv4>();
        uint32_t terIf = routes[i].ifIndex[TER];
        Time offset = Seconds(i);
        Simulator::Schedule(Seconds(20.5) + offset, &Ipv4::SetDown, ipv4, terIf);
        Simulator::Schedule(Seconds(23.5) + offset, &Ipv4::SetUp, ipv4, terIf);
        Simulator::Schedule(Seconds(40.5) + offset,
                            &SetLinkDelay,
                            terDevices[i].Get(0),
                            MilliSeconds(200));
        Simulator::Schedule(Seconds(43.5) + offset,
                            &SetLinkDelay,
                            terDevices[i].Get(0),
                            MilliSeconds(15));
    }

    std::map<std::string, uint32_t> handovers;
    for (const auto& m : managers)
    {
        m->TraceConnectWithoutContext("HandoverStart",
                                      MakeBoundCallback(&CountHandover, &handovers));
    }

    Simulator::Stop(simulationTime + Seconds(1));
    Simulator::Run();

    std::cout << "UEs: " << numUes << ", policy: " << policy << "\n";
    for (uint32_t i = 0; i < numUes; ++i)
    {
        std::cout << "  UE " << ues.Get(i)->GetId() << ": sent " << attempts[i] << ", received "
                  << servers[i]->GetReceived() << ", lost "
                  << attempts[i] - servers[i]->GetReceived() << "\n";
    }
    std::cout << "Handovers by reason:\n";
    for (const auto& [reason, count] : handovers)
    {
        std::cout << "  " << reason << ": " << count << "\n";
    }
    std::cout << "CSV: " << outputPrefix << "-packets.csv, -decisions.csv, -events.csv"
              << std::endl;

    Simulator::Destroy();
    return 0;
}
