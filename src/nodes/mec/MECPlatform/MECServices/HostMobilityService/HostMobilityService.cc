//
//                  Simu5G
// 

#include "nodes/mec/MECPlatform/MECServices/HostMobilityService/HostMobilityService.h"
#include "nodes/mec/MECPlatform/MECServices/HostMobilityService/resources/HostMobilitySubscription.h"
#include "nodes/mec/MECPlatform/MECServices/Resources/SubscriptionBase.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/CellChangeNotification.h"

namespace simu5g {

Define_Module(HostMobilityService);


HostMobilityService::HostMobilityService() {
    baseUriQueries_ = "/example/hms/v1/queries";
    baseUriSubscriptions_ = "/example/hms/v1/subscriptions";
    callbackRefUri_ = "/example/hms/v1/eventNotification";
}

HostMobilityService::~HostMobilityService() {
    // TODO Auto-generated destructor stub
}

void HostMobilityService::initialize(int stage)
{
    EV << "HMS::Initializing..." << endl;

    MecServiceBase2::initialize(stage);

    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        baseSubscriptionLocation_ = host_ + baseUriSubscriptions_ + "/";

        // register hms reference ptr to mepm
//        mecPlatformManager_->registerHostMobilityService(this);
        mecPlatformManager_->registerMecServiceReference(this, serviceName_);
//        // register to the MEO
//        cMessage *m = new cMessage("registerToMeo");
//        scheduleAt(simTime() + 0.1, m);

        // connect to the RNI service
        cMessage *m = new cMessage("connectRnis");
        scheduleAt(simTime() + 0.2, m);

    }
}


void HostMobilityService::handleMessage(cMessage *msg) {
    EV << "HostMobilityService::handleMessage " << msg->getName() << endl;

    if(msg->isSelfMessage()) {
        if (std::strcmp(msg->getName(),"connectToMeo") == 0) {
            // meHost_   parent MEC host
            // eNodeB_  bs connected to the MEC host
            // todo: create a new MEOMessage, type "HOST_REGISTRATION", indicating:
            //      - mecHost name (from which get the cModule reference in the MEO and the associated bsList)
        }
        else if (strcmp(msg->getName(), "connectRnis") == 0) {
            if (mecPlatformManager_ != nullptr) {
                auto mecServices = mecPlatformManager_->getAvailableMecServices();
                nlohmann::json rnisInfo;
                for (const auto& service : *mecServices) {
                    EV << "HostMobilityService::connectRnis - mec serv: " << service.getName() << endl;
                    if (service.getName() == "RNIService") {
                        rnisInfo = service.toJson();
                        // if service is on the same mec host of the consumer
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
                    EV << "HostMobilityService::connectRnis - rnisInfo addr: " << address << " port: " << rnisInfo["transportInfo"]["endPoint"]["addresses"]["port"] << endl;
                    rnisSocket_->connect(rnisAddr, rnisInfo["transportInfo"]["endPoint"]["addresses"]["port"]);

                    // schedule msg for sending cellChangeSubscription
                    cMessage *m = new cMessage("sendCellChangeSubscription");
                    scheduleAt(simTime() + 0.1, m);
                }
                else {
                    EV << "HostMobilityService::connectRnis - RNIService not found" << endl;

                }
            }
            else
                EV << "HostMobilityService::handleMessage - mecPlatformManager_ is null " << endl;
        }
        else if (strcmp(msg->getName(), "sendCellChangeSubscription") == 0) {
            std::string uristring = "/example/rni/v2/subscriptions";
            std::string host = rnisSocket_->getRemoteAddress().str() + ":" + std::to_string(rnisSocket_->getRemotePort());

            nlohmann::ordered_json subscriptionBody_;
            subscriptionBody_ = nlohmann::ordered_json();
            subscriptionBody_["subscriptionType"] = "CellChangeSubscription";
            inet::L3Address localAddress = inet::L3AddressResolver().resolve(meHost_->getFullPath().c_str());
            EV_DEBUG << "HostMobilityService::handlePOSTRequest - send RNIS CellChangeSubscription - localAddress: " << localAddress << endl;
            subscriptionBody_["callbackReference"] =  localAddress.str() + ":" + std::to_string(par("localPort").intValue()) + callbackRefUri_;
            EV_DEBUG << "HostMobilityService::handlePOSTRequest - send RNIS CellChangeSubscription - callbackReference: " << subscriptionBody_["callbackReference"] << endl;

            subscriptionBody_["filterCriteriaAssocHo"]["associateId"] = nlohmann::ordered_json::array();
            subscriptionBody_["filterCriteriaAssocHo"]["associateId"].push_back((new AssociateId("UE_IPv4_ADDRESS", localAddress.str()))->toJson());
            subscriptionBody_["requestTestNotification"] = false;

            Http::sendPostRequest(rnisSocket_, subscriptionBody_.dump().c_str(), host.c_str(), uristring.c_str());

        }
    }
    MecServiceBase::handleMessage(msg);
}
void HostMobilityService::handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)
{
    EV << "HostMobilityService::handlePOSTRequest" << endl;
    std::string uri = currentRequestMessageServed->getUri();
    std::string body = currentRequestMessageServed->getBody();

    if(uri.compare(baseUriSubscriptions_) == 0) {
        nlohmann::ordered_json request;
        try {
            request = nlohmann::json::parse(body);
        } catch (nlohmann::detail::parse_error e) {
            std::cout << "HostMobilityService::handlePOSTRequest" << e.what() << "\n" << body << std::endl;
            // body is not correctly formatted in JSON, manage it
            Http::send400Response(socket); // bad body JSON
            return;
        }
        if(request.contains("subscriptionType")) {
            SubscriptionBase *newSubscription = nullptr;
            if(request["subscriptionType"] == "HostMobilitySubscription") {
            EV << "HostMobilityService::handlePOSTRequest - HostMobilitySubscription "<< endl;
                newSubscription = new HostMobilitySubscription(subscriptionId_, socket, baseSubscriptionLocation_, eNodeB_);
                bool res = newSubscription->fromJson(request);
                if (res) {
                    newSubscription->set_links(baseSubscriptionLocation_);
                    subscriptions_[subscriptionId_] = newSubscription;

                    // sending response
                    nlohmann::ordered_json response = newSubscription->toJson();
                    response["subscriptionId"] = subscriptionId_;
                    EV << "HostMobilityService::handlePOSTRequest  - response: " << response << endl;

                    Http::send201Response(socket, response.dump().c_str());

                    EV << "HostMobilityService::handlePOSTRequest - Added new subscriber [" << subscriptionId_ << "]" << endl;
                    subscriptionId_++;
                }
                else {
                    delete newSubscription;
                    return;
                }

            }
            // TODO define other type of subscriptions
            if(newSubscription == nullptr) {
                EV << "HostMobilityService::Subscription type not recognized" << endl;
                Http::send400Response(socket);
            }
//            else // todo
//                // This method helps to avoid repeated code for the other type of subscription
//                handleSubscriptionRequest(newSubscription, socket, request);
        }
        else {
            EV << "HostMobilityService::handlePOSTRequest - bad request: subscriptionType not found" << endl;
            Http::send400Response(socket);
        }
    }
    else {// not found
        Http::send404Response(socket);
    }
}

void HostMobilityService::handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket) {
    EV << "HostMobilityService::handlePUTRequest" << endl;
}

void HostMobilityService::handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) {
    EV << "HostMobilityService::handleDELETERequest" << endl;

    // "/example/hms/v1/subscriptions/"
    std::string uri = currentRequestMessageServed->getUri();

    if (uri.find(baseUriSubscriptions_) == 0) {
        uri.erase(0, uri.find(baseUriSubscriptions_ + "/") + baseUriSubscriptions_.length() + 1);
        EV << "HMS - Deleting subscription" << endl;
        auto it = subscriptions_.find(std::stoi(uri));
        if (it != subscriptions_.end()) {
            subscriptions_.erase(it);
            Http::send204Response(socket);
        }
        else {
            EV << "HMS::delete subscription: subscriber not found" << endl;
            Http::send404Response(socket);
        }
    }
    else {
        Http::send400Response(socket);
        EV << "HMS::DELETERequest - bad uri" << endl;
    }
}


void HostMobilityService::handleSubscriptionRequest(SubscriptionBase *subscription, inet::TcpSocket* socket, const nlohmann::ordered_json& request)
{
    EV << "HostMobilityService::handleSubscriptionRequest - " << subscription->getSubscriptionType() << endl;

    subscription->set_links(baseSubscriptionLocation_);
    subscriptions_[subscriptionId_] = subscription;

    // sending response
    nlohmann::ordered_json response = subscription->toJson();
    EV << "HostMobilityService::handleSubscriptionRequest  - response: " << response << endl;
    response["subscriptionId"] = subscriptionId_;

    Http::send201Response(socket, response.dump().c_str());

    EV << "HostMobilityService::handleSubscriptionRequest - Added new subscriber [" << subscriptionId_ << "]" << endl;
    subscriptionId_++;

    return;
}

void HostMobilityService::socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool)
{
    EV << "HostMobilityService::socketDataArrived" << endl;
    if (socket == rnisSocket_) {
        EV_DEBUG << "HostMobilityService::socketDataArrived - rnisSocket" << endl;

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
    delete msg;
}

void HostMobilityService::handleRnisMessage(cMessage *msg) {
    EV_INFO << "HostMobilityService::handleRnisMessage - parsing message from Rnis" << endl;
    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto baseMsg = pkt->peekAtFront<HttpBaseMessage>();
    auto response = dynamicPtrCast<const HttpResponseMessage>(baseMsg);
    if (response != nullptr) {
        EV_DEBUG << "HostMobilityService::handleRnisMessage - RESPONSE" << endl;
        handleRnisResponseMessage(response.get());
        return;
    }

    auto request = dynamicPtrCast<const HttpRequestMessage>(baseMsg);
    if (request != nullptr) {
        EV_DEBUG << "HostMobilityService::handleRnisMessage - REQUEST" << endl;
        handleRnisRequestMessage(request.get());
        return;
    }

    EV << "HostMobilityService::handleRnisMessage - Unknown message type" << endl;
    delete msg;
}


void HostMobilityService::handleRnisRequestMessage(const HttpRequestMessage *request) {
    EV_INFO << "HostMobilityService::handleRnisRequestMessage" << endl;
    EV_DEBUG << "Message body: " << endl << request->getBody() << endl;
    nlohmann::json jsonBody = nlohmann::json::parse(request->getBody());
    if(!jsonBody.empty()) {
        if(jsonBody.contains("notificationType")) {
            if(jsonBody["notificationType"] == "CellChangeNotification")
                handleCellChangeNotification(jsonBody);
        }
    }
}

void HostMobilityService::handleRnisResponseMessage(const HttpResponseMessage *response) {
    EV_INFO << "HostMobilityService::handleRnisResponseMessage" << endl;

    EV_DEBUG << "Message body: " << endl << response->getBody() << endl;

    // Manage subscription response
    EV << "HostMobilityService::handling Rnis response" << endl;
    if(response->getCode() == 201) {
       EV << "HostMobilityService::handling RNI response - subscription ok: " << response->getBody() << endl;
       nlohmann::json jsonBody = nlohmann::json::parse(response->getBody());
       if(!jsonBody.empty()) {
           // Correct subscription
           std::stringstream stream;
           cellChangeSubId_ = jsonBody["subscriptionId"];

           EV << "HostMobilityService::handling Rnis response - jsonBody: " << jsonBody << endl;
       }
    }
    else if(response->getCode() == 204) {
       EV << "HostMobilityService::handling RNI response - delete subscription ok" << endl;
    }
    else if(response->getCode() == 400) {
       EV << "HostMobilityService::handling RNI response - bad request" << endl;
    }
    else if(response->getCode() == 404) {
       EV << "HostMobilityService::handling RNI response - not found" << endl;
    }
    else {
       EV << "HostMobilityService::handling RNI response - not recognized code error: " << response->getCode() << ", body:\n" << response->getBody() << endl;
    }
}

// update serving area, sending a message to meo through the mepm
// send a notification to serving consumer (i.e. ams) to notify the mec host handover
void HostMobilityService::handleCellChangeNotification(const nlohmann::ordered_json& request) {

    EV << "HMS::CellChangeNotification received - trigger MecApp migration" << endl;

    CellChangeNotification *cellChangeNotification = new CellChangeNotification();
    bool res = cellChangeNotification->fromJson(request);
    if (res) {
        EV << "Cell Change Notification processed: " << cellChangeNotification->toJson() << endl;

        Ecgi srcEcgi = cellChangeNotification->getSrcEcgi();
        Ecgi trgEcgi = cellChangeNotification->getTrgEcgi();
        auto associateId = cellChangeNotification->getAssociateId()[0];

        EV_DEBUG << "HMS::CellChangeNotification - srcCellId " << srcEcgi.getCellId() << "- trgCellId " << trgEcgi.getCellId() << endl;

        // add target cell in the set
        cModule *trgCellModule = binder_->getModuleByMacNodeId(trgEcgi.getCellId());
        EV << "HMS::CellChangeNotification - insert eNodeB_ trgCell: " << trgCellModule << endl;
        auto inserted = eNodeB_.insert(trgCellModule);

        if (inserted.second) {
//            auto eraseElem =
            eNodeB_.erase(binder_->getModuleByMacNodeId(srcEcgi.getCellId()));

//        if (eraseElem > 0)
            // update serving/coverage area -> msg to meo
            mecPlatformManager_->updateServigArea(meHost_->getName(), srcEcgi.getCellId(), trgEcgi.getCellId());
        }
    }

    Http::send204Response(rnisSocket_); // no content
}


void HostMobilityService::handleHostMobilityUpdate(MacNodeId srcCellId, MacNodeId trgCellId) {
    Enter_Method_Silent("HostMobilityService::handleHostMobilityUpdate");
    sendHostMobilityNotification(srcCellId, trgCellId);
}

void HostMobilityService::sendHostMobilityNotification(MacNodeId srcCellId, MacNodeId trgCellId) {
//    Enter_Method_Silent("HostMobilityService::sendHostMobilityNotification");
    EV_DEBUG << "HostMobilityService::sendHostMobilityNotification" << endl;
    // send hostMobilityNotification to consumers (ams)
    nlohmann::ordered_json notificationBody_;
    notificationBody_["notificationType"] = "HostMobilityNotification";
    TimeStamp ts = new TimeStamp();
    ts.setSeconds();
    notificationBody_["timeStamp"] = ts.toJson();
    notificationBody_["mecHostName"] = meHost_->getName();
    notificationBody_["mecHostAddress"] = inet::L3AddressResolver().resolve(meHost_->getFullPath().c_str()).str();
    notificationBody_["srcEcgi"]["cellId"] = srcCellId;
    notificationBody_["trgEcgi"]["cellId"] = trgCellId;

    for (auto subscription: subscriptions_) {
        notificationBody_["_links"]["href"] = baseUriSubscriptions_ + "/" + std::to_string(subscription.first);
        HostMobilitySubscription *hmSub = check_and_cast<HostMobilitySubscription *>(subscription.second);
        inet::TcpSocket *sock = static_cast<inet::TcpSocket *>(socketMap.getSocketById(hmSub->getSocketConnId()));
        if (sock != nullptr) {
            std::string callbackRef = hmSub->toJson()["callbackReference"];
            std::size_t found = callbackRef.find("/");
            if (found != std::string::npos) {
                std::string host  = callbackRef.substr(0, found);
                std::string uri = callbackRef.substr(found);
                EV_DEBUG << "sendHostMobilityNotification - callbackRef: " << callbackRef << " - host: " << host << " - uri: " << uri << endl;
                Http::sendPostRequest(sock, notificationBody_.dump().c_str(), host.c_str(), uri.c_str());
            }
        }
    }
}

bool HostMobilityService::manageSubscription()
{
    int subId = currentSubscriptionServed_->getSubId();
    if (subscriptions_.find(subId) != subscriptions_.end()) {
        EV << "HostMobilityService::manageSubscription() - subscription with id: " << subId << " found" << endl;
        SubscriptionBase *sub = subscriptions_[subId];//upcasting (getSubscriptionType is in SubscriptionBase)
        sub->sendNotification(currentSubscriptionServed_);
        if (currentSubscriptionServed_ != nullptr)
            delete currentSubscriptionServed_;
        currentSubscriptionServed_ = nullptr;
        return true;
    }
    return false;
}

} /* namespace simu5g */
