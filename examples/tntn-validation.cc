/*
 * M02 validation cases (SRS section 9, guidelines section 10). Each case runs
 * deterministic scenarios, compares the measured handover times with a
 * closed-form expectation, prints PASS/FAIL and writes a summary CSV
 * (<prefix>-v1.csv, -v2.csv, -v3.csv) plus the raw helper traces of every run.
 *
 * Evaluations happen at whole seconds (DecisionInterval = 1 s, start at 0 s)
 * and a handover completes HandoverExecutionDelay = 50 ms after it starts.
 * Scenario changes are placed between evaluation ticks so the order of
 * same-time events never matters.
 *
 *   V1  terrestrial fails             -> every node moves to NTN within one
 *                                        interval + execution delay
 *   V2  terrestrial restored          -> return waits for MinDwellTime
 *                                        (varied: 0, 2, 5, 10 s)
 *   V3  terrestrial delay ramps up    -> switch happens exactly where the policy
 *                                        allows (MaxAcceptableDelay varied for
 *                                        terrestrial-preferred, HysteresisFraction
 *                                        varied for lowest-delay)
 *
 *   ./ns3 run "tntn-validation --case=all"
 *   ./ns3 run "tntn-validation --case=v2 --prefix=out/tntn-validation"
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/tntn-helper.h"
#include "ns3/tntn-mock-access-state-provider.h"

#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TntnValidation");

/// Terrestrial access id
static constexpr uint32_t TER = 0;
/// NTN access id
static constexpr uint32_t NTN = 1;
/// Evaluation period used by every case, s
static constexpr double INTERVAL_S = 1.0;
/// Handover execution delay used by every case, s
static constexpr double EXEC_DELAY_S = 0.05;
/// Tolerance when comparing measured and expected times, s
static constexpr double TIME_TOL_S = 1e-6;

/**
 * Completion time of the first evaluation tick at or after a time.
 * @param t time, s
 * @return tick time + execution delay, s
 */
static double
SwitchDoneAfter(double t)
{
    return std::ceil(t / INTERVAL_S) * INTERVAL_S + EXEC_DELAY_S;
}

/// Handover completions seen in one run, per node
struct Recorder
{
    /// One completed handover
    struct Done
    {
        double timeS;   //!< Completion time, s
        uint32_t newId; //!< Access switched to
    };

    std::vector<std::vector<Done>> perNode; //!< Completions per node id
};

/**
 * HandoverComplete sink.
 * @param rec recorder
 * @param nodeId node id
 * @param oldId old access
 * @param newId new access
 */
static void
OnComplete(Recorder* rec, uint32_t nodeId, uint32_t oldId, uint32_t newId)
{
    rec->perNode.at(nodeId).push_back({Simulator::Now().GetSeconds(), newId});
}

/**
 * Time of the first completed switch to an access at or after a time.
 * @param rec recorder
 * @param nodeId node id
 * @param accessId target access
 * @param afterS earliest time, s
 * @return completion time, or NaN if none
 */
static double
FirstSwitchTo(const Recorder& rec, uint32_t nodeId, uint32_t accessId, double afterS)
{
    for (const auto& d : rec.perNode.at(nodeId))
    {
        if (d.newId == accessId && d.timeS >= afterS)
        {
            return d.timeS;
        }
    }
    return std::nan("");
}

/**
 * Create nodes and install managers with a terrestrial and an NTN mock access.
 * @param helper configured helper (policy and manager attributes already set)
 * @param numNodes number of nodes
 * @param ntnDelay NTN delay
 * @param rawPrefix prefix of the raw CSV traces of this run
 * @param rec recorder to connect
 * @return the managers
 */
static std::vector<Ptr<TntnAccessManager>>
Build(TntnHelper& helper,
      uint32_t numNodes,
      Time ntnDelay,
      const std::string& rawPrefix,
      Recorder& rec)
{
    helper.SetManagerAttribute("DecisionInterval", TimeValue(Seconds(INTERVAL_S)));
    helper.SetManagerAttribute("HandoverExecutionDelay", TimeValue(Seconds(EXEC_DELAY_S)));
    helper.AddAccess(TER,
                     "ns3::TntnMockAccessStateProvider",
                     "AccessType",
                     EnumValue(TntnAccessType::TERRESTRIAL),
                     "Delay",
                     TimeValue(MilliSeconds(30)));
    helper.AddAccess(NTN,
                     "ns3::TntnMockAccessStateProvider",
                     "AccessType",
                     EnumValue(TntnAccessType::NTN),
                     "Delay",
                     TimeValue(ntnDelay));
    NodeContainer nodes;
    nodes.Create(numNodes);
    auto managers = helper.Install(nodes);
    helper.EnableTracing(rawPrefix);

    rec.perNode.assign(NodeList::GetNNodes(), {});
    for (const auto& m : managers)
    {
        m->TraceConnectWithoutContext("HandoverComplete", MakeBoundCallback(&OnComplete, &rec));
    }
    return managers;
}

/**
 * Get a node's terrestrial mock.
 * @param m manager
 * @return the mock
 */
static Ptr<TntnMockAccessStateProvider>
TerOf(Ptr<TntnAccessManager> m)
{
    return DynamicCast<TntnMockAccessStateProvider>(m->GetAccessProvider(TER));
}

/**
 * Compare and report one measurement.
 * @param expected expected time, s
 * @param measured measured time, s (NaN if the event never happened)
 * @return true if equal within tolerance
 */
static bool
Matches(double expected, double measured)
{
    return !std::isnan(measured) && std::abs(expected - measured) < TIME_TOL_S;
}

/**
 * V1: terrestrial failure moves every node to NTN.
 * Node i loses terrestrial at 10.30 + 0.03 i s, spreading failures over a
 * whole evaluation interval. Expected switch completion is the next tick plus
 * the execution delay, so the latency is at most INTERVAL_S + EXEC_DELAY_S.
 * @param prefix output prefix
 * @return true if every node passed
 */
static bool
RunV1(const std::string& prefix)
{
    const uint32_t numNodes = 25;
    std::ofstream csv(prefix + "-v1.csv");
    csv << "node_id,failure_s,expected_switch_s,measured_switch_s,latency_s,on_ntn_at_end,pass\n";

    TntnHelper helper;
    Recorder rec;
    auto managers = Build(helper, numNodes, MilliSeconds(540), prefix + "-v1-raw", rec);
    std::vector<double> failure(numNodes);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        failure[i] = 10.30 + 0.03 * i;
        TerOf(managers[i])->ScheduleAvailable(Seconds(failure[i]), false);
    }
    Simulator::Stop(Seconds(20));
    Simulator::Run();

    bool allPass = true;
    double maxLatency = 0;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t id = managers[i]->GetNodeId();
        double expected = SwitchDoneAfter(failure[i]);
        double measured = FirstSwitchTo(rec, id, NTN, failure[i]);
        bool onNtn = managers[i]->GetCurrentAccess() == NTN;
        bool pass = Matches(expected, measured) && onNtn;
        allPass &= pass;
        maxLatency = std::max(maxLatency, measured - failure[i]);
        csv << id << ',' << failure[i] << ',' << expected << ',' << measured << ','
            << measured - failure[i] << ',' << onNtn << ',' << pass << '\n';
    }
    Simulator::Destroy();

    std::cout << "V1 terrestrial failure: " << numNodes << " nodes, max latency " << maxLatency
              << " s (bound " << INTERVAL_S + EXEC_DELAY_S << " s) -> "
              << (allPass ? "PASS" : "FAIL") << "\n";
    return allPass;
}

/**
 * V2: return after restoration follows the dwell time.
 * Terrestrial fails at 10.3 s (switch to NTN completes at 11.05 s) and is
 * restored at 12.5 s. With terrestrial-preferred the return is optional, so it
 * happens at the first tick at or after max(restore, 11.05 + MinDwellTime).
 * @param prefix output prefix
 * @return true if every dwell value passed
 */
static bool
RunV2(const std::string& prefix)
{
    const double failS = 10.3;
    const double restoreS = 12.5;
    std::ofstream csv(prefix + "-v2.csv");
    csv << "min_dwell_s,ntn_switch_s,restore_s,expected_return_s,measured_return_s,handovers,"
           "pass\n";

    bool allPass = true;
    std::cout << "V2 terrestrial restored at " << restoreS << " s:\n";
    for (double dwellS : {0.0, 2.0, 5.0, 10.0})
    {
        TntnHelper helper;
        helper.SetManagerAttribute("MinDwellTime", TimeValue(Seconds(dwellS)));
        Recorder rec;
        std::ostringstream raw;
        raw << prefix << "-v2-raw-dwell" << dwellS;
        auto managers = Build(helper, 1, MilliSeconds(540), raw.str(), rec);
        TerOf(managers[0])->ScheduleAvailable(Seconds(failS), false);
        TerOf(managers[0])->ScheduleAvailable(Seconds(restoreS), true);
        Simulator::Stop(Seconds(30));
        Simulator::Run();

        uint32_t id = managers[0]->GetNodeId();
        double ntnSwitch = FirstSwitchTo(rec, id, NTN, failS);
        double expected = SwitchDoneAfter(std::max(restoreS, ntnSwitch + dwellS));
        double measured = FirstSwitchTo(rec, id, TER, restoreS);
        std::size_t handovers = rec.perNode.at(id).size();
        // Initial selection, fallback, return: anything else is oscillation.
        bool pass = Matches(expected, measured) && handovers == 3;
        allPass &= pass;
        csv << dwellS << ',' << ntnSwitch << ',' << restoreS << ',' << expected << ',' << measured
            << ',' << handovers << ',' << pass << '\n';
        std::cout << "  MinDwellTime " << dwellS << " s: return expected " << expected
                  << " s, measured " << measured << " s -> " << (pass ? "PASS" : "FAIL") << "\n";
        Simulator::Destroy();
    }
    return allPass;
}

/**
 * One V3 run: terrestrial delay ramps 100 ms + 10 ms per second, NTN fixed.
 * Dwell is disabled so the switch time depends only on the policy.
 * @param prefix output prefix
 * @param label run label for the raw trace file name
 * @param helper helper with the policy already set
 * @param ntnMs NTN delay, ms
 * @param switches predicate on the terrestrial delay (ms) telling when the
 *        policy should leave terrestrial
 * @param csv summary CSV
 * @param policyName policy column value
 * @param paramName parameter column value
 * @param paramValue parameter value
 * @return true if passed
 */
static bool
RunRamp(const std::string& prefix,
        const std::string& label,
        TntnHelper& helper,
        double ntnMs,
        const std::function<bool(double)>& switches,
        std::ofstream& csv,
        const std::string& policyName,
        const std::string& paramName,
        double paramValue)
{
    const double startMs = 100;
    const double stepMs = 10;
    const int lastTick = 30;

    helper.SetManagerAttribute("MinDwellTime", TimeValue(Seconds(0)));
    Recorder rec;
    auto managers = Build(helper, 1, MilliSeconds(ntnMs), prefix + "-v3-raw-" + label, rec);
    auto ter = TerOf(managers[0]);
    ter->SetDelay(MilliSeconds(startMs));
    // The value seen at tick t is set half an interval earlier.
    for (int t = 1; t <= lastTick; ++t)
    {
        ter->ScheduleDelay(Seconds(t - 0.5 * INTERVAL_S), MilliSeconds(startMs + stepMs * t));
    }
    Simulator::Stop(Seconds(lastTick + 0.5));
    Simulator::Run();

    double expected = std::nan("");
    double terAtSwitch = std::nan("");
    for (int t = 0; t <= lastTick; ++t)
    {
        if (switches(startMs + stepMs * t))
        {
            expected = t + EXEC_DELAY_S;
            terAtSwitch = startMs + stepMs * t;
            break;
        }
    }
    double measured = FirstSwitchTo(rec, managers[0]->GetNodeId(), NTN, 0);
    bool pass = std::isnan(expected) ? std::isnan(measured) : Matches(expected, measured);
    csv << policyName << ',' << paramName << ',' << paramValue << ',' << ntnMs << ',' << terAtSwitch
        << ',' << expected << ',' << measured << ',' << pass << '\n';
    std::cout << "  " << policyName << " " << paramName << "=" << paramValue
              << ": switch expected at TER " << terAtSwitch << " ms (t=" << expected
              << " s), measured t=" << measured << " s -> " << (pass ? "PASS" : "FAIL") << "\n";
    Simulator::Destroy();
    return pass;
}

/**
 * V3: delay crossing a threshold changes the selection only when the policy permits.
 * @param prefix output prefix
 * @return true if every run passed
 */
static bool
RunV3(const std::string& prefix)
{
    const double ntnMs = 250;
    std::ofstream csv(prefix + "-v3.csv");
    csv << "policy,parameter,value,ntn_delay_ms,expected_ter_delay_at_switch_ms,"
           "expected_switch_s,measured_switch_s,pass\n";
    std::cout << "V3 terrestrial delay ramp 100 ms + 10 ms/s, NTN " << ntnMs << " ms:\n";

    bool allPass = true;
    for (double maxMs : {100.0, 150.0, 200.0})
    {
        TntnHelper helper;
        helper.SetPolicy("ns3::TntnTerrestrialPreferredPolicy",
                         "MaxAcceptableDelay",
                         TimeValue(MilliSeconds(maxMs)));
        std::ostringstream label;
        label << "tp-max" << maxMs;
        allPass &= RunRamp(
            prefix,
            label.str(),
            helper,
            ntnMs,
            [maxMs](double terMs) { return terMs > maxMs; },
            csv,
            "terrestrial-preferred",
            "MaxAcceptableDelay_ms",
            maxMs);
    }
    for (double h : {0.0, 0.05, 0.2})
    {
        TntnHelper helper;
        helper.SetPolicy("ns3::TntnLowestDelayPolicy", "HysteresisFraction", DoubleValue(h));
        std::ostringstream label;
        label << "ld-h" << h;
        allPass &= RunRamp(
            prefix,
            label.str(),
            helper,
            ntnMs,
            [ntnMs, h](double terMs) { return ntnMs < terMs && ntnMs <= terMs * (1 - h); },
            csv,
            "lowest-delay",
            "HysteresisFraction",
            h);
    }
    return allPass;
}

int
main(int argc, char* argv[])
{
    std::string which = "all";
    std::string prefix = "tntn-validation";

    CommandLine cmd(__FILE__);
    cmd.AddValue("case", "Validation case: v1, v2, v3 or all", which);
    cmd.AddValue("prefix", "Prefix of the output CSV files", prefix);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(which != "all" && which != "v1" && which != "v2" && which != "v3",
                    "Unknown --case '" << which << "'; use v1, v2, v3 or all");

    bool pass = true;
    if (which == "all" || which == "v1")
    {
        pass &= RunV1(prefix);
    }
    if (which == "all" || which == "v2")
    {
        pass &= RunV2(prefix);
    }
    if (which == "all" || which == "v3")
    {
        pass &= RunV3(prefix);
    }
    std::cout << (pass ? "ALL VALIDATION CASES PASSED" : "SOME VALIDATION CASES FAILED")
              << std::endl;
    return pass ? 0 : 1;
}
