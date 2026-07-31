//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "nodes/mec/MECPlatform/MECServices/RNIService/RNIService.h"

#include <iostream>
#include <string>
#include <vector>

#include <inet/applications/tcpapp/GenericAppMsg_m.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/lifecycle/NodeStatus.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/networklayer/contract/ipv4/Ipv4Address.h>
#include <inet/transportlayer/contract/tcp/TcpCommand_m.h>
#include <inet/transportlayer/contract/tcp/TcpSocket.h>

#include "common/utils/utils.h"
#include "nodes/mec/MECPlatform/EventNotification/CellChangeEvent.h"
#include "nodes/mec/MECPlatform/MECServices/Resources/SubscriptionBase.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/CellChangeSubscription.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"

namespace simu5g {

Define_Module(RNIService);

RNIService::RNIService():L2MeasResource_() {
    baseUriQueries_ = "/example/rni/v2/queries";
    baseUriSubscriptions_ = "/example/rni/v2/subscriptions";
    supportedQueryParams_.insert("cell_id");
    supportedQueryParams_.insert("ue_ipv4_address");
    // supportedQueryParams_.insert("ue_ipv6_address");
}

void RNIService::initialize(int stage)
{
    MecServiceBase2::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        L2MeasResource_.setBinder(binder_);
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        mecPlatformManager_->registerMecServiceReference(this, serviceName_);

        L2MeasResource_.addEnodeB(eNodeB_);
        baseSubscriptionLocation_ = host_ + baseUriSubscriptions_ + "/";
    }
}

void RNIService::receiveHandoverSignal(MacNodeId nodeId, MacNodeId srcCellId, MacNodeId trgCellId) {
    Enter_Method_Silent("RNIService::receiveHandoverSignal");
    EV << "RNIService::receiveHandoverSignal" << endl;
    inet::Ipv4Address ipAddress = binder_->getIPv4Address(nodeId);
    std::vector<unsigned int> subIds;
    // find all cellChangeSubscriptions where the associateId corresponds to the ip address of UE performing handover
    for (auto subscription: subscriptions_) {
        if (subscription.second->getSubscriptionType() == "CellChangeSubscription") {
            CellChangeSubscription *cellChangeSub = check_and_cast<CellChangeSubscription*>(subscription.second);
            std::vector<AssociateId> associateIds = check_and_cast<FilterCriteriaAssocHo*>(cellChangeSub->getFilterCriteria())->getAssociateId();

            for (auto associateId: associateIds) {
                if (associateId.getValue() == ipAddress.str()) {
                    subIds.push_back(subscription.first);
                    // if one value found, go to next subscription
                    break;
                }
            }
        }
    }
    // for all subscription found -> send a CellChangeNotification
    for (auto subId: subIds) {
        nlohmann::ordered_json notificationBody_;
        notificationBody_["notificationType"] = "CellChangeNotification";
        TimeStamp ts = new TimeStamp();
        ts.setSeconds();
        notificationBody_["timeStamp"] = ts.toJson();
        nlohmann::ordered_json associateId;
        associateId["type"] = "UE_IPv4_ADDRESS";    // other types are not managed yet
        associateId["value"] = ipAddress.str();
        notificationBody_["associateId"] = nlohmann::ordered_json::array(); //associateId;
        notificationBody_["associateId"].push_back(associateId);

        notificationBody_["srcEcgi"]["plmn"]["mcc"] = "001";    // test value, not used -> change if needed
        notificationBody_["srcEcgi"]["plmn"]["mnc"] = "01";     // test value
        notificationBody_["srcEcgi"]["cellId"] = srcCellId;
        notificationBody_["trgEcgi"]["plmn"]["mcc"] = "001";    // test value, not used -> change if needed
        notificationBody_["trgEcgi"]["plmn"]["mnc"] = "01";     // test value
        notificationBody_["trgEcgi"]["cellId"] = trgCellId;
        notificationBody_["hoStatus"] = hoStatusString[COMPLETED];
        notificationBody_["_links"]["href"] = baseUriSubscriptions_ + "/" + std::to_string(subId);

        CellChangeSubscription *cellChangeSub = check_and_cast<CellChangeSubscription*>(subscriptions_[subId]);

        inet::TcpSocket *sock = static_cast<inet::TcpSocket *>(socketMap.getSocketById(cellChangeSub->getSocketConnId()));
        if (sock != nullptr) {
            std::string callbackRef = cellChangeSub->toJson()["callbackReference"];
            std::size_t found = callbackRef.find("/");
            if (found != std::string::npos) {
                std::string host  = callbackRef.substr(0, found);
                std::string uri = callbackRef.substr(found);
                EV << "receiveHandoverSignal - callbackRef: " << callbackRef << " - host: " << host << " - uri: " << uri << endl;
                Http::sendPostRequest(sock, notificationBody_.dump().c_str(), host.c_str(), uri.c_str());
            }
        }
    }
}


void RNIService::handleMessage(cMessage *msg)
{
    EV << "RNIService::handleMessage"  << endl;
    if (msg->isSelfMessage()) {
        EV << "RNIService::handleMessage - self"  << endl;
    }
    MecServiceBase::handleMessage(msg);
}

void RNIService::handleGETRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)
{
    std::string uri = currentRequestMessageServed->getUri();

    // check if it is a GET for a query or a subscription
    if (uri == (baseUriQueries_ + "/layer2_meas")) { //queries
        EV << "RNIService::handleGETRequest - queries"  << endl;
        std::string params = currentRequestMessageServed->getParameters();
        //look for query parameters
        if (!params.empty()) {
            std::vector<std::string> queryParameters = simu5g::utils::splitString(params, "&");
            /*
             * supported parameters:
             * - cell_id
             * - ue_ipv4_address
             * - ue_ipv6_address // not implemented yet
             */

            std::vector<MacNodeId> cellIds;
            std::vector<inet::Ipv4Address> ues;

            std::map<std::string, std::vector<std::string>> queryParamsMap; // e.g cell_id -> [0, 1]

            std::vector<std::string> params;
            std::vector<std::string> splittedParams;
            for (const auto &queryParam : queryParameters) {
                if (queryParam.rfind("cell_id", 0) == 0) { // cell_id=par1,par2
                    params = simu5g::utils::splitString(queryParam, "=");
                    if (params.size() != 2) { //must be param=values
                        Http::send400Response(socket);
                        return;
                    }
                    splittedParams = simu5g::utils::splitString(params[1], ",");
                    for (const auto &cellIdParam : splittedParams) {
                        cellIds.push_back((MacNodeId)std::stoi(cellIdParam));
                    }
                }
                else if (queryParam.rfind("ue_ipv4_address", 0) == 0) {
                    // TO DO manage acr:10.12
                    params = simu5g::utils::splitString(queryParam, "=");
                    if (params.size() != 2) { //must be param=values
                        Http::send400Response(socket);
                        return;
                    }
                    splittedParams = simu5g::utils::splitString(params[1], ",");
                    for (const auto &ueAddress : splittedParams)
                        ues.push_back(inet::Ipv4Address(ueAddress.c_str()));
                }
                else { // bad parameters
                    Http::send400Response(socket);
                    return;
                }
            }

            //send response
            if (!ues.empty() && !cellIds.empty()) {
                Http::send200Response(socket, L2MeasResource_.toJson(cellIds, ues).dump().c_str());
            }
            else if (ues.empty() && !cellIds.empty()) {
                Http::send200Response(socket, L2MeasResource_.toJsonCell(cellIds).dump().c_str());
            }
            else if (!ues.empty() && cellIds.empty()) {
                Http::send200Response(socket, L2MeasResource_.toJsonUe(ues).dump().c_str());
            }
            else {
                Http::send400Response(socket);
            }
        }
        else {
            //no query params
            EV << "RNIService::handleGETRequest - no query params" << endl;
            Http::send200Response(socket, L2MeasResource_.toJson().dump().c_str());
            return;
        }
    }
    else if (uri == baseUriSubscriptions_) { //subs
        // TODO implement subscription?
        Http::send404Response(socket);
    }
    else { // not found
        Http::send404Response(socket);
    }
}

void RNIService::handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) {

    EV << "RNIService::handlePOSTRequest - Received a POST request" << endl;
    std::string uri = currentRequestMessageServed->getUri();
    std::string body = currentRequestMessageServed->getBody();

    if(uri.compare(baseUriSubscriptions_) == 0) {
        nlohmann::ordered_json request;
        try {
            request = nlohmann::json::parse(body);
        } catch (nlohmann::detail::parse_error e) {
            std::cout << "RNIService::handlePOSTRequest" << e.what() << "\n" << body << std::endl;
            // body is not correctly formatted in JSON, manage it
            Http::send400Response(socket); // bad body JSON
            return;
        }
        if(request.contains("subscriptionType")) {
            SubscriptionBase *newSubscription = nullptr;
            if(request["subscriptionType"] == "CellChangeSubscription") {
            EV << "RNIService::handlePOSTRequest - cellChangeSub "<< endl;
                newSubscription = new CellChangeSubscription(subscriptionId_, socket, baseSubscriptionLocation_, eNodeB_);
                bool res = newSubscription->fromJson(request);
                if (res) {
                    FilterCriteriaAssocHo *filterCriteria = check_and_cast<FilterCriteriaAssocHo *>(newSubscription->getFilterCriteria());
                    filterCriteria->setFilterCriteriaValueFromJson(request["filterCriteriaAssocHo"]);

                    for (auto& associateId: request["filterCriteriaAssocHo"]["associateId"].items()) {
                        nlohmann::ordered_json val = associateId.value();
                        EV << "RNIService::handlePOSTRequest - associateId: " << val["value"].get<std::string>() << endl;
                        inet::L3Address ueAppAddr = inet::L3AddressResolver().resolve(val["value"].get<std::string>().c_str());
                        if (val["type"] == "UE_IPv4_ADDRESS") {
                            EV<< " associateId Type" << endl;
                            MacNodeId macNodeId = binder_->getMacNodeId(ueAppAddr.toIpv4());
                            binder_->registerMacNodeToRnis(macNodeId, this);
                        }
                    }
                }
                else {
                    delete newSubscription;
                    return;
                }

            }
            // TODO define other type of subscriptions
            if(newSubscription == nullptr)
            {
                EV << "RNIService::Subscription type not recognized" << endl;
                Http::send400Response(socket);
            }
            else
                // This method helps to avoid repeated code for the other type of subscription
                handleSubscriptionRequest(newSubscription, socket, request);
        }
        else
        {
            EV << "RNIService::handlePOSTRequest - bad request: subscriptionType not found" << endl;
            Http::send400Response(socket);
        }


    }
    else // not found
    {
        Http::send404Response(socket);
    }
}

void RNIService::handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) {}

void RNIService::handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)
{
}

void RNIService::handleSubscriptionRequest(SubscriptionBase *subscription, inet::TcpSocket* socket, const nlohmann::ordered_json& request)
{
    EV << "RNIService::handleSubscriptionRequest - " << subscription->getSubscriptionType() << endl;

    subscription->set_links(baseSubscriptionLocation_);
    subscriptions_[subscriptionId_] = subscription;

    // sending response
    nlohmann::ordered_json response = subscription->toJson();
    EV << "RNIService::handleSubscriptionRequest  - response: " << response << endl;
    response["subscriptionId"] = subscriptionId_;

    Http::send201Response(socket, response.dump().c_str());

    EV << "RNIService::handleSubscriptionRequest - Added new subscriber [" << subscriptionId_ << "]" << endl;
    subscriptionId_++;

    // printing all subscriptions
    printAllSubscriptions();

    return;
}

bool RNIService::manageSubscription()
{
    EV << "RNIService::manageSubscription()" << endl;
    int subId = currentSubscriptionServed_->getSubId();
    if (subscriptions_.find(subId) != subscriptions_.end()) {
        EV << "RNIService::manageSubscription() - subscription with id: " << subId << " found" << endl;
        SubscriptionBase *sub = subscriptions_[subId];//upcasting (getSubscriptionType is in SubscriptionBase)
        sub->sendNotification(currentSubscriptionServed_);
        if (currentSubscriptionServed_ != nullptr)
            delete currentSubscriptionServed_;
        currentSubscriptionServed_ = nullptr;
        return true;
    }
    return false;
}

void RNIService::printAllSubscriptions()
{
    auto it  = subscriptions_.begin();
    auto end = subscriptions_.end();
    for(; it != end; ++it)
    {
        EV << "SubscriptionId: " << it->first << " " << it->second->toJson() << endl;
    }
}

void RNIService::finish()
{
}


} //namespace

