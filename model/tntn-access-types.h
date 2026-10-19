#ifndef TNTN_ACCESS_TYPES_H
#define TNTN_ACCESS_TYPES_H

#include "ns3/data-rate.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <limits>
#include <string>

namespace ns3
{

// Means "no access" (e.g., nothing is available)
static constexpr uint32_t TNTN_NO_ACCESS = std::numeric_limits<uint32_t>::max();

// What kind of network an access is
enum class TntnAccessType : uint8_t
{
    TERRESTRIAL = 0,
    NTN = 1
};

// Why a decision or handover happened (FR-08)
enum class TntnReason : uint8_t
{
    NONE = 0,
    INITIAL_SELECTION,
    CURRENT_UNAVAILABLE,
    DELAY_EXCEEDED,
    LOSS_EXCEEDED,
    RATE_TOO_LOW,
    BETTER_CANDIDATE,
    PREFERRED_RESTORED,
    FORCED
};

// Converts a reason to text, for CSV output
inline std::string
TntnReasonToString(TntnReason r)
{
    switch (r)
    {
    case TntnReason::NONE: return "NONE";
    case TntnReason::INITIAL_SELECTION: return "INITIAL_SELECTION";
    case TntnReason::CURRENT_UNAVAILABLE: return "CURRENT_UNAVAILABLE";
    case TntnReason::DELAY_EXCEEDED: return "DELAY_EXCEEDED";
    case TntnReason::LOSS_EXCEEDED: return "LOSS_EXCEEDED";
    case TntnReason::RATE_TOO_LOW: return "RATE_TOO_LOW";
    case TntnReason::BETTER_CANDIDATE: return "BETTER_CANDIDATE";
    case TntnReason::PREFERRED_RESTORED: return "PREFERRED_RESTORED";
    case TntnReason::FORCED: return "FORCED";
    }
    return "UNKNOWN";
}

// A snapshot of one access at one moment (FR-02)
struct AccessState
{
    uint32_t accessId{0};
    TntnAccessType type{TntnAccessType::TERRESTRIAL};
    bool available{false};
    Time delay{Seconds(0)};
    double lossRate{0.0};
    DataRate rate{0};
    double load{0.0};
};

// What a policy returns after deciding
struct AccessDecision
{
    uint32_t selectedAccessId{TNTN_NO_ACCESS};
    TntnReason reason{TntnReason::NONE};
    double score{0.0};
};

} // namespace ns3

#endif // TNTN_ACCESS_TYPES_H