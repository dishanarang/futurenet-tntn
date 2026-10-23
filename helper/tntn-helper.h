#ifndef TNTN_HELPER_H
#define TNTN_HELPER_H

#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/tntn-access-manager.h"

#include <string>
#include <utility>
#include <vector>

namespace ns3
{

/**
 * @ingroup futurenet-tntn
 * Installs a TntnAccessManager, its accesses and its policy on many nodes,
 * and writes CSV traces.
 *
 * Each installed node gets its own manager, policy and providers, all built
 * from the factories configured here, so per-node state is never shared.
 * The manager is aggregated to the node and started at the current time.
 *
 * @code
 *   TntnHelper tntn;
 *   tntn.SetPolicy("ns3::TntnTerrestrialPreferredPolicy");
 *   tntn.AddAccess(0, "ns3::TntnMockAccessStateProvider", "AccessType", EnumValue(...));
 *   tntn.AddAccess(1, "ns3::TntnMockAccessStateProvider", "AccessType", EnumValue(...));
 *   tntn.Install(nodes);
 *   tntn.EnableTracing("tntn");
 * @endcode
 */
class TntnHelper
{
  public:
    /// Create a helper using TntnTerrestrialPreferredPolicy and no accesses.
    TntnHelper();

    /**
     * Set an attribute on every manager created from now on.
     * @param name attribute name (e.g. "MinDwellTime")
     * @param value attribute value
     */
    void SetManagerAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Choose the policy type for every manager created from now on.
     * @tparam Ts attribute name/value pair types
     * @param type policy TypeId name (e.g. "ns3::TntnLowestDelayPolicy")
     * @param args optional attribute name/value pairs for the policy
     */
    template <typename... Ts>
    void SetPolicy(const std::string& type, Ts&&... args);

    /**
     * Declare a candidate access every installed node will get. One provider
     * of the given type is created per node.
     * @tparam Ts attribute name/value pair types
     * @param accessId access id, unique within a node
     * @param providerType provider TypeId name (e.g. "ns3::TntnMockAccessStateProvider")
     * @param args optional attribute name/value pairs for the provider
     */
    template <typename... Ts>
    void AddAccess(uint32_t accessId, const std::string& providerType, Ts&&... args);

    /**
     * Install and start a manager on each node.
     * @param nodes the nodes
     * @return the managers, in node order
     */
    std::vector<Ptr<TntnAccessManager>> Install(NodeContainer nodes);
    /**
     * Install and start a manager on one node.
     * @param node the node
     * @return the manager
     */
    Ptr<TntnAccessManager> Install(Ptr<Node> node);

    /**
     * Write CSV traces for every manager installed so far:
     *  - <prefix>-decisions.csv: one row per evaluation (M02 SRS section 7)
     *  - <prefix>-events.csv: handover and availability events in the common
     *    FutureNet event schema (guidelines section 4.5)
     * @param prefix output file prefix
     */
    void EnableTracing(const std::string& prefix);

  private:
    ObjectFactory m_managerFactory; //!< Builds managers
    ObjectFactory m_policyFactory;  //!< Builds one policy per manager
    std::vector<std::pair<uint32_t, ObjectFactory>>
        m_accessFactories;                          //!< Builds one provider per access per node
    std::vector<Ptr<TntnAccessManager>> m_managers; //!< Managers installed so far
};

template <typename... Ts>
void
TntnHelper::SetPolicy(const std::string& type, Ts&&... args)
{
    m_policyFactory.SetTypeId(type);
    m_policyFactory.Set(std::forward<Ts>(args)...);
}

template <typename... Ts>
void
TntnHelper::AddAccess(uint32_t accessId, const std::string& providerType, Ts&&... args)
{
    ObjectFactory factory(providerType);
    factory.Set(std::forward<Ts>(args)...);
    m_accessFactories.emplace_back(accessId, factory);
}

} // namespace ns3

#endif // TNTN_HELPER_H
