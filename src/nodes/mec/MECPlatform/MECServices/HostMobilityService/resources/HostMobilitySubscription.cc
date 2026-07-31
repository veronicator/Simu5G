//
//                  Simu5G
// 

#include "nodes/mec/MECPlatform/MECServices/HostMobilityService/resources/HostMobilitySubscription.h"

namespace simu5g {

HostMobilitySubscription::HostMobilitySubscription() {
    subscriptionType_ = "HostMobilitySubscription";
}

HostMobilitySubscription::HostMobilitySubscription(std::string hostName) {
    subscriptionType_ = "HostMobilitySubscription";
    servingMecHostName_ = hostName;
}

HostMobilitySubscription::HostMobilitySubscription(unsigned int subId, inet::TcpSocket *socket , const std::string& baseResLocation,  std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs):
        SubscriptionBase(subId,socket,baseResLocation, eNodeBs)
{
    subscriptionType_ = "HostMobilitySubscription";
}

HostMobilitySubscription::HostMobilitySubscription(std::string hostName, unsigned int subId, inet::TcpSocket *socket , const std::string& baseResLocation,  std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs):
        SubscriptionBase(subId,socket,baseResLocation, eNodeBs)
{
    subscriptionType_ = "HostMobilitySubscription";
    servingMecHostName_ = hostName;
}

HostMobilitySubscription::~HostMobilitySubscription() {
    // TODO Auto-generated destructor stub
}

bool HostMobilitySubscription::fromJson(const nlohmann::ordered_json& json) {
    bool result = SubscriptionBase::fromJson(json);

    EV << "HostMobilitySubscription::from Json\n" << json << endl;

    if(!json.contains("subscriptionType")) {
        EV << "HostMobilitySubscription::required parameters not specified in the request!" << endl;
        return false;
    }

    if(json["subscriptionType"] != subscriptionType_) {
        EV << "MobilityProcedureSubscription::subscription type not valid!" << endl;
        return false;
    }

    if(json.contains("mecHostName"))
        servingMecHostName_ = json["mecHostName"];

    if(json.contains("_links"))
        links_ = json["_links"]["self"]["href"];

    if (json.contains("callbackReference")) {
        std::string callbackReference = json["callbackReference"];
        // parse it to retrieve the resource uri and the host
        std::size_t found = callbackReference.find("/");
        if (found != std::string::npos) {
            clientHost_ = callbackReference.substr(0, found);
            clientUri_ = callbackReference.substr(found);
        }
    }

    return result;
}

nlohmann::ordered_json HostMobilitySubscription::toJson() const {
    nlohmann::ordered_json val;
    val["subscriptionType"] = subscriptionType_;
    val["callbackReference"] = callbackReference_;
    val["_links"]["self"]["href"] = links_;
    val["exipryDeadline"] = expiryTime_.toJson();
    val["mecHostName"] = servingMecHostName_;
    return val;
}

void HostMobilitySubscription::sendSubscriptionResponse()
{

}

void HostMobilitySubscription::sendNotification(EventNotification *event)
{

}

EventNotification* HostMobilitySubscription::handleSubscription()
{
    return nullptr;
}


} /* namespace simu5g */
