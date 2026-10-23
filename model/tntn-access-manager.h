#ifndef TNTN_ACCESS_MANAGER_H
#define TNTN_ACCESS_MANAGER_H

#include "tntn-access-selection-policy.h"
#include "tntn-access-state-provider.h"
#include "tntn-access-types.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"

#include <utility>
#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Periodically selects the access a node uses and performs logical handovers.
 *
 * Every DecisionInterval the manager reads the state of each registered
 * access, asks the policy for a decision and, if the policy picks another
 * access, starts a handover that completes after HandoverExecutionDelay.
 *
 * Division of work: the policy decides which access is best right now
 * (including hysteresis); the manager owns everything time-related
 * (periodic evaluation, dwell time, execution delay) and the traces.
 *
 * Dwell rule (M02-FR-06): after a handover completes, optional switches
 * (better candidate, preferred access restored, constraint violations) are
 * blocked for MinDwellTime. Switches away from an unavailable access, the
 * initial selection and forced switches are never blocked, because staying
 * would leave the node without a working access or ignore an explicit order.
 *
 * While a handover is in progress GetCurrentAccess() still returns the old
 * access and periodic decisions are skipped.
 */
class TntnAccessManager : public Object
{
  public:
    /**
     * Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TntnAccessManager();
    ~TntnAccessManager() override;

    /**
     * Callback signature for DecisionEvaluated.
     * @param nodeId node id
     * @param currentAccessId access in use when the decision was made
     * @param selectedAccessId access chosen by the policy
     * @param reason reason given by the policy
     */
    typedef void (*DecisionEvaluatedTracedCallback)(uint32_t nodeId,
                                                    uint32_t currentAccessId,
                                                    uint32_t selectedAccessId,
                                                    TntnReason reason);
    /**
     * Callback signature for HandoverStart.
     * @param nodeId node id
     * @param oldAccessId access before the handover
     * @param newAccessId access after the handover
     * @param reason why the handover happens
     */
    typedef void (*HandoverStartTracedCallback)(uint32_t nodeId,
                                                uint32_t oldAccessId,
                                                uint32_t newAccessId,
                                                TntnReason reason);
    /**
     * Callback signature for HandoverComplete.
     * @param nodeId node id
     * @param oldAccessId access before the handover
     * @param newAccessId access after the handover
     */
    typedef void (*HandoverCompleteTracedCallback)(uint32_t nodeId,
                                                   uint32_t oldAccessId,
                                                   uint32_t newAccessId);
    /**
     * Callback signature for AccessUnavailable.
     * @param nodeId node id
     * @param accessId access that became unavailable
     */
    typedef void (*AccessUnavailableTracedCallback)(uint32_t nodeId, uint32_t accessId);

    /**
     * Set the id reported in traces. The helper sets it to the node's id.
     * @param nodeId node id
     */
    void SetNodeId(uint32_t nodeId);
    /**
     * Get the id reported in traces.
     * @return node id
     */
    uint32_t GetNodeId() const;

    /**
     * Register a candidate access (M02-FR-01).
     * @param accessId unique id of the access within this manager
     * @param provider source of the access state
     */
    void AddAccess(uint32_t accessId, Ptr<TntnAccessStateProvider> provider);

    /**
     * Set the selection policy. May be called while running (e.g., by an
     * M10 agent); the new policy is used from the next evaluation.
     * @param policy the policy
     */
    void SetPolicy(Ptr<TntnAccessSelectionPolicy> policy);
    /**
     * Get the selection policy.
     * @return the policy
     */
    Ptr<TntnAccessSelectionPolicy> GetPolicy() const;

    /**
     * Start periodic evaluation; the first evaluation happens now.
     * Requires a policy and at least one access.
     */
    void Start();
    /**
     * Stop periodic evaluation. A handover already in progress still completes.
     */
    void Stop();

    /**
     * Run one evaluation now without changing the periodic schedule.
     */
    void Evaluate();
    /**
     * Run one evaluation now and restart the periodic schedule from now.
     * Useful when an external event makes waiting for the next tick undesirable.
     */
    void ForceEvaluation();
    /**
     * Switch to an access immediately, ignoring the policy and the dwell time.
     * @param accessId target access; must be registered
     * @return true if a handover started or the access is already current;
     *         false if the target is unavailable or a handover is in progress
     */
    bool ForceAccess(uint32_t accessId);

    /**
     * Get the access in use.
     * @return the access id, or TNTN_NO_ACCESS before the first handover completes
     */
    uint32_t GetCurrentAccess() const;
    /**
     * Check whether a handover has started but not completed.
     * @return true if a handover is in progress
     */
    bool IsHandoverInProgress() const;
    /**
     * Get the access states read at the most recent evaluation.
     * @return the states, in registration order
     */
    const std::vector<TntnAccessState>& GetLastAccessStates() const;
    /**
     * Get the provider of a registered access, e.g. to script a mock.
     * @param accessId access id
     * @return the provider, or nullptr if not registered
     */
    Ptr<TntnAccessStateProvider> GetAccessProvider(uint32_t accessId) const;

  protected:
    void DoDispose() override;

  private:
    /// Timer callback: evaluate and schedule the next evaluation.
    void PeriodicEvaluate();
    /**
     * Read the current state of every access and fire AccessUnavailable for
     * accesses that went from available to unavailable.
     */
    void CollectStates();
    /**
     * Check whether a switch for this reason must wait for the dwell time.
     * @param reason the policy's reason
     * @return true if the switch is optional and the dwell time has not elapsed
     */
    bool BlockedByDwell(TntnReason reason) const;
    /**
     * Begin a handover.
     * @param newAccessId target access
     * @param reason why
     */
    void StartHandover(uint32_t newAccessId, TntnReason reason);
    /// Finish the handover started by StartHandover().
    void CompleteHandover();

    Time m_decisionInterval; //!< Time between periodic evaluations
    Time m_minDwellTime;     //!< Minimum time on an access before an optional switch
    Time m_handoverDelay;    //!< Time from handover start to completion

    uint32_t m_nodeId{0}; //!< Id reported in traces
    std::vector<std::pair<uint32_t, Ptr<TntnAccessStateProvider>>>
        m_accesses;                            //!< Registered accesses, in registration order
    std::vector<bool> m_lastAvailable;         //!< Availability at the previous evaluation
    std::vector<TntnAccessState> m_lastStates; //!< States read at the last evaluation
    Ptr<TntnAccessSelectionPolicy> m_policy;   //!< Selection policy

    uint32_t m_currentAccess{TNTN_NO_ACCESS}; //!< Access in use
    uint32_t m_targetAccess{TNTN_NO_ACCESS};  //!< Target of the handover in progress
    bool m_handoverInProgress{false};         //!< Whether a handover is in progress
    bool m_hasSwitched{false};                //!< Whether any handover has completed
    Time m_lastHandoverTime;                  //!< Completion time of the last handover
    EventId m_evaluateEvent;                  //!< Next periodic evaluation
    EventId m_completeEvent;                  //!< Pending handover completion

    /// Fired after every evaluation (M02 SRS section 7)
    TracedCallback<uint32_t, uint32_t, uint32_t, TntnReason> m_decisionEvaluatedTrace;
    /// Fired when a handover starts (M02-FR-08)
    TracedCallback<uint32_t, uint32_t, uint32_t, TntnReason> m_handoverStartTrace;
    /// Fired when a handover completes
    TracedCallback<uint32_t, uint32_t, uint32_t> m_handoverCompleteTrace;
    /// Fired when an access goes from available to unavailable
    TracedCallback<uint32_t, uint32_t> m_accessUnavailableTrace;
};

} // namespace ns3

#endif // TNTN_ACCESS_MANAGER_H
