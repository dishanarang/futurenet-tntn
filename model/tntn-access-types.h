#ifndef TNTN_ACCESS_TYPES_H
#define TNTN_ACCESS_TYPES_H

#include "ns3/data-rate.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <limits>
#include <string>

/**
 * @defgroup futurenet-tntn FutureNet Terrestrial-NTN access selection and handover
 *
 * Technology-agnostic access selection and logical handover between a
 * terrestrial access and a non-terrestrial (NTN) access (FutureNet M02).
 */

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Sentinel access identifier meaning "no access selected".
 *
 * The maximum uint32_t is used so that 0 stays a valid user-chosen access id.
 */
static constexpr uint32_t TNTN_NO_ACCESS = std::numeric_limits<uint32_t>::max();

/**
 * @ingroup futurenet-tntn
 * Kind of network an access belongs to.
 */
enum class TntnAccessType : uint8_t
{
    TERRESTRIAL = 0, //!< Ground network (cellular, Wi-Fi, wired, ...)
    NTN = 1          //!< Non-terrestrial network (satellite, HAPS, ...)
};

/**
 * @ingroup futurenet-tntn
 * Reason attached to an access decision or handover (M02-FR-08).
 */
enum class TntnReason : uint8_t
{
    NONE = 0,            //!< No change; current access kept
    INITIAL_SELECTION,   //!< First access chosen after start
    CURRENT_UNAVAILABLE, //!< Current access reported unavailable
    DELAY_EXCEEDED,      //!< Current access exceeds the delay constraint
    LOSS_EXCEEDED,       //!< Current access exceeds the loss constraint
    RATE_TOO_LOW,        //!< Current access is below the rate constraint
    BETTER_CANDIDATE,    //!< Another access beats the current one by more than the hysteresis
    PREFERRED_RESTORED,  //!< Preferred (terrestrial) access is usable again
    FORCED               //!< Switch requested externally (e.g., by an M10 agent)
};

/**
 * @ingroup futurenet-tntn
 * Convert a reason to its upper-case name, used in CSV output.
 *
 * @param r the reason
 * @return the reason name, or "UNKNOWN" for an out-of-range value
 */
inline std::string
TntnReasonToString(TntnReason r)
{
    switch (r)
    {
    case TntnReason::NONE:
        return "NONE";
    case TntnReason::INITIAL_SELECTION:
        return "INITIAL_SELECTION";
    case TntnReason::CURRENT_UNAVAILABLE:
        return "CURRENT_UNAVAILABLE";
    case TntnReason::DELAY_EXCEEDED:
        return "DELAY_EXCEEDED";
    case TntnReason::LOSS_EXCEEDED:
        return "LOSS_EXCEEDED";
    case TntnReason::RATE_TOO_LOW:
        return "RATE_TOO_LOW";
    case TntnReason::BETTER_CANDIDATE:
        return "BETTER_CANDIDATE";
    case TntnReason::PREFERRED_RESTORED:
        return "PREFERRED_RESTORED";
    case TntnReason::FORCED:
        return "FORCED";
    }
    return "UNKNOWN";
}

/**
 * @ingroup futurenet-tntn
 * Snapshot of one candidate access at one instant (M02-FR-02).
 *
 * Kept independent of any provider so policies can compare accesses from
 * different technologies on the same terms.
 */
struct TntnAccessState
{
    uint32_t accessId{0};                             //!< Access identifier within the node
    TntnAccessType type{TntnAccessType::TERRESTRIAL}; //!< Network kind
    bool available{false};                            //!< Whether the access can carry traffic now
    Time delay{Seconds(0)};                           //!< One-way delay estimate
    double lossRate{0.0};                             //!< Packet loss ratio, [0,1]
    DataRate rate{0};                                 //!< Available/nominal data rate
    double load{0.0};                                 //!< Normalized load, [0,1]
};

/**
 * @ingroup futurenet-tntn
 * Result returned by an access selection policy.
 */
struct TntnAccessDecision
{
    uint32_t selectedAccessId{TNTN_NO_ACCESS}; //!< Chosen access, or TNTN_NO_ACCESS if none
    TntnReason reason{TntnReason::NONE};       //!< Why this access was chosen
    double score{0.0};                         //!< Policy-specific score of the chosen access
};

} // namespace ns3

#endif // TNTN_ACCESS_TYPES_H
