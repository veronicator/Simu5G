//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "nodes/mec/MECPlatform/MECServices/ApplicationMobilityService/ApplicationMobilityService.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/FilterCriteriaAssocHo.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/CellChangeNotification.h"

namespace simu5g {

Define_Module(ApplicationMobilityService);


ApplicationMobilityService::ApplicationMobilityService()
{
    registrationResources_ = new ApplicationMobilityResource();
    baseUriQueries_ = "/example/amsi/v1/queries/";
    baseUriServiceRegistration_ = "/example/amsi/v1/app_mobility_services"; // registration and deregistration uri
    callbackUri_ = "/example/amsi/v1/eventNotification";
    baseUriSubscriptions_ = "/example/amsi/v1/subscriptions";
    baseSubscriptionLocation_ = host_+ baseUriSubscriptions_;

    applicationServiceIds = 0;

    subscriptionId_ = 0;
    subscriptions_.clear();


}

ApplicationMobilityService::~ApplicationMobilityService()
{

}

void ApplicationMobilityService::initialize(int stage)
{
    EV << "AMS::Initializing..." << endl;
    migrationCounter_ = 0;
    totalMigrationsSignal_ = registerSignal("totalMigrations");

    MecServiceBase2::initialize(stage);

    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        // connect to the RNI service
        cMessage *m = new cMessage("connectRNIS");
        scheduleAt(simTime() + 0.3, m);
    }
}

void ApplicationMobilityService::handleMessage(cMessage *msg)
{
    if(msg->isSelfMessage()) {
        if (std::strcmp(msg->getFullName(),"localMigration") == 0) {
            EV << "AMS::LOCAL MIGRATION RECEIVED: to be processed " << registrationResources_->getMigratedApps().size() << endl;
            if(registrationResources_->getMigratedApps().size() > 0) {
                EV << "AMS::an app has been correctly migrated -- generate local event" << endl;
                for(auto &el : registrationResources_->getMigratedApps()) {
                    // Generating notification
                    MobilityProcedureNotification *notification = new MobilityProcedureNotification();
                    RegistrationInfo *r = registrationResources_->getRegistrationInfoFromContext(el.second->getAppInstanceId());
                    if(r != nullptr) {
                        notification->setMobilityStatus(INTERHOST_MOVEOUT_COMPLETED);
                        notification->setTargetAppInfo(*el.second);

                        // statistics
//                        simtime_t migrationUpdateTime = simTime();
                        // total migration from the start
                        migrationCounter_++;
                        std::cout << "Migration counter has been updated: " << migrationCounter_ << endl;

                        // migration at this time (useful to set grop migration per an interval of time)
                        // value = 1 is just an indication
                        emit(totalMigrationsSignal_, 1);


                        std::vector<AssociateId> associateId;
                        for(auto devInfo : r->getDeviceInformation()) {
                            associateId.push_back(devInfo.getAssociateId());
                            EV << "AMS::local notification generated - associateID " << devInfo.getAssociateId().getValue() << " added" << endl;
                        }
                        notification->setAssociateId(associateId);

                        // removing migrated app from the list
                        registrationResources_->removingMigratedApp(el.first);


                        nlohmann::ordered_json jsonObject = notification->toJson();
                        EV << "AMS::local notification generated " << jsonObject.dump(2) << endl;
                        handleNotificationCallback(jsonObject);
                    }
                    else {
                        EV << "AMS:: no information regarding migrated app - " << el.second->getAppInstanceId() << " - has been found" << endl;
                    }
                }
            }
            delete msg;
        }
        else if (strcmp(msg->getName(), "connectRNIS") == 0) {
            EV << "ApplicationMobilityService::handleMessage " << msg->getName() << endl;
            if (mecPlatformManager_ != nullptr) {
                auto mecServices = mecPlatformManager_->getAvailableMecServices();
                nlohmann::json rnisInfo;
                for (const auto& service : *mecServices) {
                    EV << "ApplicationMobilityService::connectRNIS - mec serv: " << service.getName() << endl;
                    if (service.getName() == "RNIService") {
                        rnisInfo = service.toJson();
                        if (service.getMecHost() == meHost_->getName())
                            break;
                    }
                }
                if (!rnisInfo.empty()) {
                    rnisSocket_ = new TcpSocket();
                    rnisSocket_->setOutputGate(gate("socketOut"));
                    rnisSocket_->setCallback(this);
                    socketMap.addSocket(rnisSocket_);

                    std::string address = rnisInfo["transportInfo"]["endPoint"]["addresses"]["host"];
                    inet::L3Address rnisAddr = L3AddressResolver().resolve(address.c_str());
                    EV << "ApplicationMobilityService::connectRNIS - rnisInfo addr: " << address << " port: " << rnisInfo["transportInfo"]["endPoint"]["addresses"]["port"] << endl;
                    rnisSocket_->connect(rnisAddr, rnisInfo["transportInfo"]["endPoint"]["addresses"]["port"]);

                }
                else {
                    EV << "ApplicationMobilityService::connectRNIS - RNIService not found" << endl;

                }
            }
            else
                EV << "ApplicationMobilityService::handleMessage - mecPlatformManager_ is null " << endl;
        }
    }
    MecServiceBase::handleMessage(msg);
}

void ApplicationMobilityService::handleGETRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket)
{
    EV << "AMS::handleGETRequest" << endl;
    std::string uri = std::string(currentRequestMessageServed->getUri());
    if(uri.compare(baseUriQueries_ + "/adjacent_app_instances") == 0)
    {
        EV << "AMS::This API has not been implemented yet!" << endl;
        Http::send404Response(socket);
    }
    else if(uri.compare(baseUriServiceRegistration_) == 0)
    {
        EV << "AMS::Retrieve information about the registered application mobility service" << endl;
        nlohmann::ordered_json response = registrationResources_->toJson();
        Http::send200Response(socket, response.dump().c_str());
    }
    else if(uri.find(baseUriServiceRegistration_) == 0)
    {
        uri.erase(0, baseUriServiceRegistration_.length());
        if(uri.length() > 0 && uri.find("/deregister_task") == std::string::npos)
        {
            EV << "AMS::Getting specific id " << endl;

            Http::send200Response(socket,registrationResources_->toJsonFromId(uri.c_str()).dump().c_str());
        }
        else
        {
            EV << "AMS::deregister_task API not implemented" << endl;
        }

    }
    else if(uri.compare(baseUriSubscriptions_) == 0)
    {
        /*
         * TODO to be implemented
         * Here we should return the list of links to requestor's subscriptions
         */
        EV << "AMS::subscriptions get - all - received (not implemented yet)" << endl;

    }
    else if(uri.find(baseUriSubscriptions_) == 0)
    {
        EV << "AMS::subscriptions get - single - received" << endl;
        uri.erase(0, baseUriSubscriptions_.length());
        if(uri.length() > 0 && uri.find("sub") != std::string::npos)
        {
            EV << "AMS::retrieving information of subscriber: " << uri << endl;
            int id = std::stoi(uri.erase(0, std::string("sub").length())); // sub is the prefix added in subscription phase
            if(subscriptions_.find(id) != subscriptions_.end())
            {
                EV << "AMS::subscriber found!" << endl;
                Http::send200Response(socket, subscriptions_[id]->toJson().dump().c_str());
            }
            else
            {
                EV << "AMS::subscriber not found" << endl;
                Http::send404Response(socket);
            }
        }
        else
        {
            EV << "AMS::Bad Request" << endl;
            Http::send400Response(socket);
        }

    }
    else {
        EV << "AMS::Bad Request" << endl;
        Http::send400Response(socket);
    }
}

void ApplicationMobilityService::handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket) {
    EV << "AMS::handlePOSTRequest" << endl;
    std::string uri = currentRequestMessageServed->getUri();
    EV << "AMS::handlePOSTRequest - uri: " << uri << endl;
    if(uri.compare(baseUriServiceRegistration_) == 0) {
        EV << "AMS::handlePOSTRequest - baseUriSerDer_" << endl;

        nlohmann::ordered_json request = nlohmann::json::parse(currentRequestMessageServed->getBody());
        RegistrationInfo* regInfo = new RegistrationInfo();
        bool res = regInfo->fromJson(request);
        if(!res) {
            EV << "AMS::Post request - bad request " << endl;
            Http::send400Response(socket);
            return;
        }
        regInfo->setAppMobilityServiceId(std::to_string(applicationServiceIds));
        registrationResources_->addRegistrationInfo(regInfo);

        applicationServiceIds++;

        nlohmann::ordered_json response = regInfo->toJson();
        std::pair<std::string, std::string> p("Location: ", baseUriServiceRegistration_);
        EV << "AMS::Correctly subscribed sending: " << response.dump() <<endl;
        Http::send201Response(socket, response.dump(2).c_str(), p);

        if (rnisSocket_ != nullptr) {
            // send RNIS CellChangeSubscription
            sendCellChangeSubscription(regInfo->getDeviceInformation()[0].getAssociateId());

        }
        // if the registration request comes from a mecApp on a mobileMecHost -> subscribe to HostMobilityNotification
        if (regInfo->getMobileMecHost()) {  // is mobile
            bool hmsConnected = false;
            for (auto sockId: sockIdToMecHost) {
                if (sockId.second == regInfo->getMecHostName()) {
                    hmsConnected = true;
//                    sendHostMobilitySubscription();
                    break;
                }
            }
            if (!hmsConnected)
                connectToHms(regInfo->getMecHostName());
//            todo: if connected: send subscription
////            sendHostMobilitySubscription();
//            }
        }
    }
    else if(uri.compare(baseUriSubscriptions_) == 0) {
        // New subscriber
        nlohmann::ordered_json request = nlohmann::json::parse(currentRequestMessageServed->getBody());
        if(request.contains("subscriptionType")) {
            SubscriptionBase *subscription = nullptr;

            if(request["subscriptionType"] == "MobilityProcedureSubscription") {
                subscription = new MobilityProcedureSubscription(subscriptionId_, socket, baseSubscriptionLocation_, eNodeB_);
            }
            // Here should be added AdjacentAppInfoSubscription

            if(subscription == nullptr) {
                EV << "AMS::Subscription type not recognized" << endl;
                Http::send400Response(socket);
            }
            else
                handleSubscriptionRequest(subscription, socket, request); // This method helps to avoid repeated code for the other type of subscription


        }
        else {
            EV << "AMS::Bad request!" << endl;
            Http::send400Response(socket);
        }
    }
    else if(uri.compare(callbackUri_) == 0) {
        EV << "AMS::Received a notification event from: " << currentRequestMessageServed->getHost() << endl;
        // notification type
        nlohmann::ordered_json request = nlohmann::json::parse(currentRequestMessageServed->getBody());
        if(request.contains("notificationType")) {
            if(request["notificationType"] == "CellChangeNotification")
                handleCellChangeNotification(request);
            else
                handleNotificationCallback(request);
        }
        else {
            EV << "AMS::Bad request!" << endl;
            Http::send400Response(socket);
        }
    }
    else {
        EV << "AMS::Post request - bad uri " << endl;
        Http::send400Response(socket);
    }
}

void ApplicationMobilityService::handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket)
{
    EV << "AMS::handlePUTRequest" << endl;
    std::string uri = currentRequestMessageServed->getUri();
    if(uri.find(baseUriServiceRegistration_) == 0)
    {
        uri.erase(0, uri.find(baseUriServiceRegistration_) + baseUriServiceRegistration_.length());
        EV << "AMS::received registration update from " << uri << endl;
        nlohmann::ordered_json request = nlohmann::json::parse(currentRequestMessageServed->getBody());
        // build registration info
        RegistrationInfo *r = new RegistrationInfo();
        bool res = r->fromJson(request);
        if(!res)
        {
            EV << "AMS::PUT- BAD REQUEST" << endl;
            Http::send400Response(socket);
            return;
        }
        res = registrationResources_->updateRegistrationInfo(uri, r);
        if(res)
        {
            EV << "AMS::Service Consumer " << uri << " correctly updated! " << endl;
            Http::send200Response(socket, request.dump().c_str());

            // this message is generated for each put: it allows to generate self-notification, useful for
            // app migrated from dynamic resource to local resouce
            cMessage *message = new cMessage("localMigration");
            if(!message->isScheduled())
            {

                double time = 0; //exponential(0.0005);
                EV << "AMS::localMigration in " << time << endl;
                scheduleAt(simTime() + time, message);
            }
        }
        else
        {
            EV << "AMS::Service Consumer " << uri << " not found! " << endl;
            Http::send404Response(socket);
        }
    }
    else if(uri.find(baseUriSubscriptions_) == 0)
    {

        uri.erase(0, baseUriSubscriptions_.length());
        EV << "AMS::Received subscriptions update from " << uri << endl;

        nlohmann::ordered_json request = nlohmann::json::parse(currentRequestMessageServed->getBody());
        SubscriptionBase *subscription = nullptr;

        if(request["subscriptionType"] == "MobilityProcedureSubscription")
        {
            int subscriptionId = std::stoi(uri.erase(0, std::string("sub").length()));
            subscription = new MobilityProcedureSubscription(subscriptionId, socket, baseSubscriptionLocation_, eNodeB_);
        }

        if(subscription != nullptr)
        {
            subscription->fromJson(request);

            auto sub = subscriptions_.find(subscription->getSubscriptionId());
            if(sub != subscriptions_.end())
            {
                subscription->set_links(baseSubscriptionLocation_);
                subscriptions_[sub->first] = subscription;
                EV << "AMS::Subscription Updated" << endl;
                std::cout << "UPDATED subscription" << endl;
                std::cout << "AMS::Subscription " << subscription->toJson() << endl;

                printAllSubscriptions();

                Http::send200Response(socket, request.dump().c_str());
            }
            else
            {
                EV << "AMS::PUT request - subscription not found " << endl;
                Http::send404Response(socket);
            }
        }
        else
        {
            EV << "AMS::Subscription type not recognised!" << endl;
            Http::send400Response(socket);
        }

    }
    else
    {
        EV << "AMS::PUT request - bad uri " << endl;
        Http::send400Response(socket);
    }
}

void ApplicationMobilityService::handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket)
{
    EV << "AMS::handleDELETERequest" << endl;
    std::string uri = currentRequestMessageServed->getUri();
    if(uri.find(baseUriServiceRegistration_) == 0)
    {
        EV << "AMS::Delete registration info " <<  endl;
        uri.erase(0, uri.find(baseUriServiceRegistration_) + baseUriServiceRegistration_.length());
        if(!registrationResources_->removeRegistrationInfo(uri.c_str()))
        {
            EV << "AMS::Delete request - service consumer not found!" << endl;
            Http::send404Response(socket);
        }
        else
        {
            Http::send204Response(socket);
        }
    }
    else if(uri.find(baseUriSubscriptions_) == 0)
    {
        EV << "AMS::Delete subscription " <<  uri <<endl;
        uri.erase(0,uri.find(baseUriSubscriptions_+"/sub") + baseUriSubscriptions_.length() + 4);
        EV << "AMS::Deleting " <<  uri << endl;
        auto it = subscriptions_.find(std::atoi(uri.c_str()));
        if(it == subscriptions_.end())
        {
            EV << "AMS:: delete subscription : subscriber " << uri << " not found" << endl;
            Http::send404Response(socket);
            return;
        }

        subscriptions_.erase(it);
        std::cout << "DELETED subscription" << endl;
        printAllSubscriptions();
        Http::send204Response(socket);
    }
    else
    {
        EV << "AMS::DELETE request - bad uri " << endl;
        Http::send404Response(socket);
    }
}


void ApplicationMobilityService::socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool)
{
    EV << "ApplicationMobilityService::socketDataArrived" << endl;
    if (socket == rnisSocket_) {
        EV << "ApplicationMobilityService::socketDataArrived - rnisSocket" << endl;

        if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
            inet::Packet *packet = check_and_cast<inet::Packet *>(msg);
            int connId = socket->getSocketId();
            ChunkQueue& queue = socketQueue[connId];
            auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
            queue.push(chunk);

            while (queue.has<HttpBaseMessage>(b(-1))) {
                auto baseMsg = queue.pop<HttpBaseMessage>(b(-1));
                handleRnisMessage(new Packet("RnisMessage", baseMsg));
            }
        }
    }
    else {
        int sockId = socket->getSocketId();
        if (sockIdToMecHost.find(sockId) != sockIdToMecHost.end()) {
            EV << "ApplicationMobilityService::socketDataArrived - hmsSocket" << endl;

            if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
                inet::Packet *packet = check_and_cast<inet::Packet *>(msg);
                int connId = socket->getSocketId();
                ChunkQueue& queue = socketQueue[connId];
                auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
                queue.push(chunk);

                while (queue.has<HttpBaseMessage>(b(-1))) {
                    auto baseMsg = queue.pop<HttpBaseMessage>(b(-1));
                    // todo
    //                handleHmsMessage(new Packet("HmsMessage", baseMsg), socket);
                }
            }
        }
    }
    delete msg;
}

void ApplicationMobilityService::connectToHms(std::string mecHostName) {
    if (mecPlatformManager_ != nullptr) {
        auto mecServices = mecPlatformManager_->getAvailableMecServices();
        nlohmann::json hmsInfo;
        for (const auto& service : *mecServices) {
            EV << "ApplicationMobilityService::connectToHms - mec serv: " << service.getName() << endl;
            if (service.getName() == "HostMobilityService") {
                if (service.getMecHost() == mecHostName) {
                    // ams needs to connect to hms in the serving mobile mec host
                    hmsInfo = service.toJson();
                    break;
                }
            }
        }
        if (!hmsInfo.empty()) {
            inet::TcpSocket *hmsSocket = new TcpSocket();
            hmsSocket->setOutputGate(gate("socketOut"));
            hmsSocket->setCallback(this);
            socketMap.addSocket(hmsSocket);
            sockIdToMecHost.insert({hmsSocket->getSocketId(), mecHostName});

            std::string address = hmsInfo["transportInfo"]["endPoint"]["addresses"]["host"];
            inet::L3Address hmsAddr = L3AddressResolver().resolve(address.c_str());
            EV << "ApplicationMobilityService::connectToHms - hmsInfo addr: " << address << " port: " << hmsInfo["transportInfo"]["endPoint"]["addresses"]["port"] << endl;
            hmsSocket->connect(hmsAddr, hmsInfo["transportInfo"]["endPoint"]["addresses"]["port"]);

        }
        else {
            EV << "ApplicationMobilityService::connectToHms - HostMobilityService not found" << endl;

        }
    }
    else
        EV << "ApplicationMobilityService::handleMessage - mecPlatformManager_ is null " << endl;
}

void ApplicationMobilityService::handleRnisMessage(cMessage *msg) {
    EV_INFO << "ApplicationMobilityService::handleRnisMessage - parsing message from Rnis" << endl;
    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto baseMsg = pkt->peekAtFront<HttpBaseMessage>();
    auto response = dynamicPtrCast<const HttpResponseMessage>(baseMsg);
    if (response != nullptr) {
        EV_DEBUG << "ApplicationMobilityService::handleRnisMessage - RESPONSE" << endl;
        handleRnisResponseMessage(response.get());
        return;
    }

    auto request = dynamicPtrCast<const HttpRequestMessage>(baseMsg);
    if (request != nullptr) {
        EV_DEBUG << "ApplicationMobilityService::handleRnisMessage - REQUEST" << endl;
        handleRnisRequestMessage(request.get());
        return;
    }

    EV << "ApplicationMobilityService::handleRnisMessage - Unknown message type" << endl;
    delete msg;
}


void ApplicationMobilityService::handleRnisRequestMessage(const HttpRequestMessage *request) {
    EV_INFO << "ApplicationMobilityService::handleRnisRequestMessage" << endl;
    EV_DEBUG << "Message body: " << endl << request->getBody() << endl;
    nlohmann::json jsonBody = nlohmann::json::parse(request->getBody());
    if(!jsonBody.empty()) {
        if(jsonBody.contains("notificationType")) {
            if(jsonBody["notificationType"] == "CellChangeNotification")
                handleCellChangeNotification(jsonBody);
            else
                handleNotificationCallback(jsonBody);
        }
    }
}

void ApplicationMobilityService::handleRnisResponseMessage(const HttpResponseMessage *response) {
    EV_INFO << "ApplicationMobilityService::handleRnisResponseMessage" << endl;

    EV_DEBUG << "Message body: " << endl << response->getBody() << endl;

    // Manage subscription response
    EV << "ApplicationMobilityService::handling Rnis response" << endl;
    if(response->getCode() == 201) {
       EV << "ApplicationMobilityService::handling RNI response - subscription ok: " << response->getBody() << endl;
       nlohmann::json jsonBody = nlohmann::json::parse(response->getBody());
       if(!jsonBody.empty()) {
           // Correct subscription
           std::stringstream stream;
           stream << "sub" << jsonBody["subscriptionId"];
           std::string subId_ = stream.str();
           // todo: save the subId of each subscription request (?)

           EV << "ApplicationMobilityService::handling Rnis response - jsonBody: " << jsonBody << endl;
       }
    }
    else if(response->getCode() == 204) {
       EV << "ApplicationMobilityService::handling RNI response - delete subscription ok" << endl;
    }
    else if(response->getCode() == 400) {
       EV << "ApplicationMobilityService::handling RNI response - bad request" << endl;
    }
    else if(response->getCode() == 404) {
       EV << "ApplicationMobilityService::handling RNI response - not found" << endl;
    }
    else {
       EV << "ApplicationMobilityService::handling RNI response - not recognized code error: " << response->getCode() << ", body:\n" << response->getBody() << endl;
    }
}

void ApplicationMobilityService::handleSubscriptionRequest(SubscriptionBase *subscription, inet::TcpSocket *socket, const nlohmann::ordered_json &request)
{
    EV << "AMS::handleSubscriptionRequest" << endl;

    EV << "AMS::" << subscription->getSubscriptionType() << endl;
    bool res = subscription->fromJson(request);
    if(res)
    {

        subscription->set_links(baseSubscriptionLocation_);

        subscriptions_[subscriptionId_] = subscription;

        nlohmann::ordered_json response = subscription->toJson();
        response["subscriptionId"] = subscriptionId_;
        EV << "AMS::subscribed with id " << subscriptionId_ << "\n" << subscription->toJson() << endl;
        subscriptionId_++;
        Http::send201Response(socket, response.dump().c_str());

        // TODO Add new parameter in MobilityProcedureSubscription:
        // requestTestNotification - here you can start the test!

        printAllSubscriptions();
        EV << serviceName_ << " - correct subscription created!" << endl;
    }
    else
    {
        EV << "AMS::an error occurred during request parsing!" << endl;
        Http::send400Response(socket);
    }
}

void ApplicationMobilityService::sendCellChangeSubscription(AssociateId associateId) {
    // send RNIS CellChangeSubscription
     std::string uristring = "/example/rni/v2/subscriptions";
     std::string host = rnisSocket_->getRemoteAddress().str()+":"+std::to_string(rnisSocket_->getRemotePort());

     nlohmann::ordered_json subscriptionBody_;
     subscriptionBody_ = nlohmann::ordered_json();
     subscriptionBody_["subscriptionType"] = "CellChangeSubscription";
     inet::L3Address localAddress = inet::L3AddressResolver().resolve(meHost_->getFullPath().c_str());
     EV << "ApplicationMobilityService::handlePOSTRequest - send RNIS CellChangeSubscription - localAddress: " << localAddress << endl;
     subscriptionBody_["callbackReference"] =  localAddress.str() + ":" + std::to_string(par("localPort").intValue()) + callbackUri_;
     EV_DEBUG << "ApplicationMobilityService::handlePOSTRequest - send RNIS CellChangeSubscription - callbackReference: " << subscriptionBody_["callbackReference"] << endl;
     subscriptionBody_["filterCriteriaAssocHo"]["appInstanceId"] = getName();
     subscriptionBody_["filterCriteriaAssocHo"]["associateId"] = nlohmann::ordered_json::array();

     subscriptionBody_["filterCriteriaAssocHo"]["associateId"].push_back(associateId.toJson());
     subscriptionBody_["filterCriteriaAssocHo"]["hoStatus"] = nlohmann::ordered_json::array();
     subscriptionBody_["filterCriteriaAssocHo"]["hoStatus"].push_back(hoStatusString[IN_PREPARATION]);
     // subscriptionBody_["filterCriteriaAssocHo"]["ecgi"] = nlohmann::ordered_json::array();
     subscriptionBody_["requestTestNotification"] = false;

     Http::sendPostRequest(rnisSocket_, subscriptionBody_.dump().c_str(), host.c_str(), uristring.c_str());
}

// trigger mec App migration sending a message to MEO through MEPM
void ApplicationMobilityService::handleCellChangeNotification(const nlohmann::ordered_json& request) {

    EV << "AMS::CellChangeNotification received - trigger MecApp migration" << endl;
    std::vector<RegistrationInfo *> registrationInfo;

    CellChangeNotification *cellChangeNotification = new CellChangeNotification();
    bool res = cellChangeNotification->fromJson(request);
    if (res) {
        EV << "Cell Change Notification processed: " << cellChangeNotification->toJson() << endl;
        std::vector<std::string> appInstanceIds = registrationResources_->getAppInstanceIds(cellChangeNotification->getAssociateId());

        Ecgi srcEcgi = cellChangeNotification->getSrcEcgi();
        Ecgi trgEcgi = cellChangeNotification->getTrgEcgi();
        auto associateIds = cellChangeNotification->getAssociateId();

        EV << "AMS::CellChangeNotification - srcCellId " << srcEcgi.getCellId() << "; trgCellId " << trgEcgi.getCellId() << endl;

        // more than one mec app instance associate to a single ue could be present
        for (auto instanceId: appInstanceIds)
            registrationInfo.push_back(registrationResources_->getRegistrationInfoFromAppId(instanceId));

        for (auto regInfo: registrationInfo) {
            for (auto devInfo: regInfo->getDeviceInformation()) {
                if (devInfo.getAppMobilityServiceLevelString() == "APP_MOBILITY_NOT_ALLOWED") {
                    EV << "AMS::CellChangeNotification - APP_MOBILITY_NOT_ALLOWED" << endl;
                    continue;
                }

                // app mobility allowed
                for (auto id: associateIds) {   // in our simple case, associateIds is a vector with a single element
                    if (devInfo.getAssociateId().getValue().compare(id.getValue()) == 0) {
                        EV << "AMS::CellChangeNotification - trigger migration" << endl;
                        mecPlatformManager_->triggerMecAppMigration(id, appInstanceIds, srcEcgi.getCellId(), trgEcgi.getCellId());
                    }
                }
            }
        }
    }

    Http::send204Response(rnisSocket_); // no content
}

void ApplicationMobilityService::handleNotificationCallback(const nlohmann::ordered_json &request)
{
    /*
     * This method manages two types of notification
     * - MobilityProcedureNotification
     * - AdjacentAppInfoNotification
     * */
    NotificationBase *notification = nullptr;
    std::string subscriptionType = "";
    std::vector<int> ids;
    EV << "AMS::received notification request: " << request.dump(2) << endl;
    if(request["notificationType"] == "MobilityProcedureNotification") {
        notification = new MobilityProcedureNotification(registrationResources_);
        subscriptionType = "MobilityProcedureSubscription";
    }
    else if(request["notificationType"] == "AdjacentAppInfoNotification") {
        EV << "AMS::Notification type not implemented" << endl;
        //Http::send400Response(socket);
        return;
    }
    else {
        EV << "AMS::Notification type not managed " << endl;
//        Http::send400Response(socket);
        return;
    }
    EV << "AMS::Trigger - " << request["notificationType"] << " - received!" << endl;

    bool res = notification->fromJson(request);
    EV << "Notification processed: " << notification->toJson() << endl;
    bool removeChecks;
    if(res) {
        std::vector<std::string> appInstanceId = registrationResources_->getAppInstanceIds(notification->getAssociateId());

        for(auto subscriber : subscriptions_) {
            EV << "AMS::processing subscriber: "<< subscriber.second->getSubscriptionId() << endl;
            EventNotification *event = nullptr;
            if(subscriptionType==subscriber.second->getSubscriptionType())
            {
                removeChecks = false;
                if(!appInstanceId.empty() && std::find(appInstanceId.begin(), appInstanceId.end(), subscriber.second->getFilterCriteria()->getAppInstanceId())
                        != appInstanceId.end())
                {
                    EV << "AMS::notification associated id checks removed" << endl;
                    removeChecks = true;
                }
                event = notification->handleNotification(subscriber.second->getFilterCriteria(), removeChecks);
                if(event != nullptr)
                {
                  event->setSubId(subscriber.second->getSubscriptionId());
                  EV << "AMS::next notification to -> " << subscriber.second->getSubscriptionId() << endl;
                  newSubscriptionEvent(event);
                }
            }

        }
    }
}

bool ApplicationMobilityService::manageSubscription()
{
    int subId = currentSubscriptionServed_->getSubId();
    if(subscriptions_.find(subId) != subscriptions_.end()) {
        EV << "ApplicationMobilityService::manageSubscription() - subscription with id: " << subId << " found" << endl;
        SubscriptionBase *sub = subscriptions_[subId]; //upcasting (getSubscriptionType is in Subscriptionbase)
        sub->sendNotification(currentSubscriptionServed_);
        if(currentSubscriptionServed_!= nullptr)
            delete currentSubscriptionServed_;
        currentSubscriptionServed_ = nullptr;

        return true;
    }

    return false;
}

void ApplicationMobilityService::printAllSubscriptions() {

    for(auto subscriber : subscriptions_)
    {
        subscriber.second->to_string();
    }
}

void ApplicationMobilityService::finish()
{
    MecServiceBase::finish();
    return;
}

}   // namespace
