//
//      CellChangeSubscription.cc -  Implementation of CellChangeSubcription 
//
// Author: Angelo Feraudo (University of Bologna)
//

#include "CellChangeSubscription.h"
#include "FilterCriteriaAssocHo.h"

namespace simu5g {

CellChangeSubscription::CellChangeSubscription() {
    subscriptionType_ = "CellChangeSubscription";
    filterCriteria_ = new FilterCriteriaAssocHo();
}

CellChangeSubscription::CellChangeSubscription(unsigned int subId, inet::TcpSocket *socket , const std::string& baseResLocation,  std::set<cModule *, simu5g::utils::cModule_LessId>& eNodeBs):
        SubscriptionBase(subId,socket,baseResLocation, eNodeBs)
{
    subscriptionType_ = "CellChangeSubscription";
    filterCriteria_ = new FilterCriteriaAssocHo();
}

CellChangeSubscription::~CellChangeSubscription() {
    // TODO Auto-generated destructor stub
}



void CellChangeSubscription::sendSubscriptionResponse()
{

}

void CellChangeSubscription::sendNotification(EventNotification *event)
{

}

EventNotification* CellChangeSubscription::handleSubscription()
{
    return nullptr;
}


nlohmann::ordered_json CellChangeSubscription::toJson() const
{
    nlohmann::ordered_json val;
    val["subscriptionType"] = subscriptionType_;
    val["callbackReference"] = callbackReference_;
    val["_links"]["self"]["href"] = links_;
    val["websockNotifConfig"] = websockNotifConfig_.toJson();
    val["exipryDeadline"] = expiryTime_.toJson();
    val["filterCriteriaAssocHo"] = filterCriteria_->toJson();
    val["requestTestNotification"] = requestTestNotification_;
    val["anyOf"] = nlohmann::json::object(); // not defined
    return val;
}

bool CellChangeSubscription::fromJson(const nlohmann::ordered_json& json)
{
    bool result = SubscriptionBase::fromJson(json);
    
    EV << "CellChangeSubscription::from Json\n" << json << endl;

    if(!json.contains("subscriptionType") || !json.contains("filterCriteriaAssocHo"))
    {
        EV << "CellChangeSubscription::required parameters not specified in the request!" << endl;
        return false;
    }

    if(json["subscriptionType"] != subscriptionType_)
    {
        EV << "MobilityProcedureSubscription::subscription type not valid!" << endl;
        return false;
    }

    if(json.contains("_links"))
        links_ = json["_links"]["self"]["href"];


    if(json.contains("requestTestNotification"))
        requestTestNotification_ = json["requestTestNotification"];
    
    if (json.contains("callbackReference")) {
        std::string callbackReference = json["callbackReference"];
        // parse it to retrieve the resource uri and the host
        std::size_t found = callbackReference.find("/");
        if (found != std::string::npos) {
            clientHost_ = callbackReference.substr(0, found);
            clientUri_ = callbackReference.substr(found);
        }
    }

    if(json.contains("websockNotifConfig"))
        result = result && websockNotifConfig_.fromJson(json["websockNotifConfig"]);
    // TODO missing anyof
    return result;
}

}   // namespace
