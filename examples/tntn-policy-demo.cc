/*
 * Calls a selection policy directly with a few hand-made situations and
 * prints what it chose and why. No simulation time, manager or network is
 * involved; it only shows how a policy turns access states into a decision.
 *
 *   ./ns3 run "tntn-policy-demo --policy=terrestrial"
 *   ./ns3 run "tntn-policy-demo --policy=lowest-delay"
 */

#include "ns3/core-module.h"
#include "ns3/tntn-lowest-delay-policy.h"
#include "ns3/tntn-terrestrial-preferred-policy.h"

#include <iomanip>
#include <iostream>

using namespace ns3;

/// Terrestrial access id
static constexpr uint32_t TER = 0;
/// NTN access id
static constexpr uint32_t NTN = 1;

/**
 * Build an access state.
 * @param id access id
 * @param type access type
 * @param available availability
 * @param delay one-way delay
 * @return the state
 */
static TntnAccessState
MakeState(uint32_t id, TntnAccessType type, bool available, Time delay)
{
    TntnAccessState s;
    s.accessId = id;
    s.type = type;
    s.available = available;
    s.delay = delay;
    s.rate = DataRate("50Mbps");
    return s;
}

/**
 * Convert an access id to a short name.
 * @param id access id
 * @return "TER", "NTN" or "none"
 */
static std::string
Name(uint32_t id)
{
    if (id == TER)
    {
        return "TER";
    }
    if (id == NTN)
    {
        return "NTN";
    }
    return "none";
}

/**
 * Ask the policy for a decision and print the situation and the result.
 * @param policy the policy
 * @param situation description of the situation
 * @param ter terrestrial state
 * @param ntn NTN state
 * @param current access in use now
 */
static void
Ask(Ptr<TntnAccessSelectionPolicy> policy,
    const std::string& situation,
    const TntnAccessState& ter,
    const TntnAccessState& ntn,
    uint32_t current)
{
    TntnAccessDecision d = policy->Select({ter, ntn}, current);
    std::cout << std::left << std::setw(40) << situation << std::setw(8) << Name(current)
              << std::setw(8) << Name(d.selectedAccessId) << TntnReasonToString(d.reason)
              << std::endl;
}

int
main(int argc, char* argv[])
{
    std::string policyName = "terrestrial";

    CommandLine cmd(__FILE__);
    cmd.AddValue("policy", "Selection policy: terrestrial or lowest-delay", policyName);
    cmd.Parse(argc, argv);

    // The researcher chooses the policy here; the rest of the program is the same for any policy.
    Ptr<TntnAccessSelectionPolicy> policy;
    if (policyName == "terrestrial")
    {
        policy = CreateObject<TntnTerrestrialPreferredPolicy>();
    }
    else if (policyName == "lowest-delay")
    {
        policy = CreateObject<TntnLowestDelayPolicy>();
    }
    else
    {
        NS_ABORT_MSG("Unknown --policy '" << policyName << "'; use terrestrial or lowest-delay");
    }

    const auto ter = TntnAccessType::TERRESTRIAL;
    const auto ntn = TntnAccessType::NTN;

    std::cout << "Policy: " << policy->GetInstanceTypeId().GetName() << "\n\n"
              << std::left << std::setw(40) << "Situation" << std::setw(8) << "Using"
              << std::setw(8) << "Chosen"
              << "Reason\n"
              << std::string(75, '-') << std::endl;

    Ask(policy,
        "Start: both up (TER 30ms, NTN 540ms)",
        MakeState(TER, ter, true, MilliSeconds(30)),
        MakeState(NTN, ntn, true, MilliSeconds(540)),
        TNTN_NO_ACCESS);
    Ask(policy,
        "Nothing changed",
        MakeState(TER, ter, true, MilliSeconds(30)),
        MakeState(NTN, ntn, true, MilliSeconds(540)),
        TER);
    Ask(policy,
        "TER goes down",
        MakeState(TER, ter, false, MilliSeconds(30)),
        MakeState(NTN, ntn, true, MilliSeconds(540)),
        TER);
    Ask(policy,
        "TER comes back",
        MakeState(TER, ter, true, MilliSeconds(30)),
        MakeState(NTN, ntn, true, MilliSeconds(540)),
        NTN);
    Ask(policy,
        "TER gets slow (200ms > 150ms limit)",
        MakeState(TER, ter, true, MilliSeconds(200)),
        MakeState(NTN, ntn, true, MilliSeconds(540)),
        TER);
    Ask(policy,
        "TER slow, but NTN also down",
        MakeState(TER, ter, true, MilliSeconds(200)),
        MakeState(NTN, ntn, false, MilliSeconds(540)),
        NTN);
    Ask(policy,
        "NTN slightly faster (TER 100, NTN 97ms)",
        MakeState(TER, ter, true, MilliSeconds(100)),
        MakeState(NTN, ntn, true, MilliSeconds(97)),
        TER);
    Ask(policy,
        "NTN much faster (TER 100, NTN 80ms)",
        MakeState(TER, ter, true, MilliSeconds(100)),
        MakeState(NTN, ntn, true, MilliSeconds(80)),
        TER);
    Ask(policy,
        "Both down",
        MakeState(TER, ter, false, MilliSeconds(30)),
        MakeState(NTN, ntn, false, MilliSeconds(540)),
        TER);

    return 0;
}
