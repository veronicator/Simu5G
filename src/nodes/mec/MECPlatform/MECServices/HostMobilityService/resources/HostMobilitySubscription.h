//
//                  Simu5G
// 

#ifndef NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYSUBSCRIPTION_H_
#define NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYSUBSCRIPTION_H_

#include "nodes/mec/MECPlatform/MECServices/Resources/SubscriptionBase.h"

namespace simu5g {

class HostMobilitySubscription: public SubscriptionBase {
protected:
    std::string servingMecHostName_;

  public:
    HostMobilitySubscription();
    HostMobilitySubscription(std::string hostName);
    HostMobilitySubscription(unsigned int subId, inet::TcpSocket *socket , const std::string& baseResLocation,  std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs);
    HostMobilitySubscription(std::string hostName, unsigned int subId, inet::TcpSocket *socket , const std::string& baseResLocation,  std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs);
    virtual ~HostMobilitySubscription();

    virtual bool fromJson(const nlohmann::ordered_json& json) override;
    virtual nlohmann::ordered_json toJson() const override;

    virtual void sendSubscriptionResponse() override;
    virtual void sendNotification(EventNotification *event) override;
    virtual EventNotification *handleSubscription() override;
};

} /* namespace simu5g */

#endif /* NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYSUBSCRIPTION_H_ */
