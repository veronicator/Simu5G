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

#include "apps/mec/WarningAlert/MECWarningAlertApp.h"

#include <fstream>

#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/Packet_m.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/transportlayer/common/L4PortTag_m.h>

#include "apps/mec/DeviceApp/DeviceAppMessages/DeviceAppPacket_Types.h"
#include "nodes/mec/MECPlatform/MECServices/packets/HttpResponseMessage/HttpResponseMessage.h"
#include "apps/mec/WarningAlert/packets/WarningAlertPacket_Types.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"
#include "nodes/mec/utils/httpUtils/json.hpp"

namespace simu5g {

Define_Module(MECWarningAlertApp);

using namespace inet;
using namespace omnetpp;

MECWarningAlertApp::~MECWarningAlertApp()
{
    if (circle != nullptr) {
        if (getSimulation()->getSystemModule()->getCanvas()->findFigure(circle) != -1)
            getSimulation()->getSystemModule()->getCanvas()->removeFigure(circle);
        delete circle;
    }
}

void MECWarningAlertApp::initialize(int stage)
{
    MecAppBase::initialize(stage);

    // avoid multiple initializations
    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;

    // retrieving parameters
    size_ = par("packetSize");

    // set Udp Socket
    ueAppSocket_.setOutputGate(gate("socketOut"));

    localUePort_ = par("localUePort");
    ueAppSocket_.bind(localUePort_);

    // testing
    EV << "MECWarningAlertApp::initialize - Mec application " << getClassName() << " with mecAppId[" << mecAppId << "] has started!" << endl;
}

void MECWarningAlertApp::finish() {
    MecAppBase::finish();
    EV << "MECWarningAlertApp::finish()" << endl;

    if (gate("socketOut")->isConnected()) {
    }
}

void MECWarningAlertApp::handleUeMessage(cMessage *msg)
{
    // determine its source address/port
    auto pk = check_and_cast<Packet *>(msg);
    ueAppAddress_ = pk->getTag<L3AddressInd>()->getSrcAddress();
    ueAppPort_ = pk->getTag<L4PortInd>()->getSrcPort();

    auto mecPk = pk->peekAtFront<WarningAppPacket>();

    if (strcmp(mecPk->getType(), START_WARNING) == 0) {
        /*
         * Read center and radius from the message
         */
        EV << "MECWarningAlertApp::handleUeMessage - WarningStartPacket arrived" << endl;
        auto warnPk = dynamicPtrCast<const WarningStartPacket>(mecPk);
        if (warnPk == nullptr)
            throw cRuntimeError("MECWarningAlertApp::handleUeMessage - WarningStartPacket is null");

        centerPositionX = warnPk->getCenterPositionX();
        centerPositionY = warnPk->getCenterPositionY();
        radius = warnPk->getRadius();

        if (par("logger").boolValue()) {
            std::ofstream myfile;
            myfile.open("example.txt", std::ios::app);
            if (myfile.is_open()) {
                myfile << "[" << NOW << "] MEWarningAlertApp - Received message from UE, connecting to the Location Service\n";
                myfile.close();
            }
        }

        cMessage *m = new cMessage("connectService");
        scheduleAt(simTime() + 0.005, m);
    }
    else if (strcmp(mecPk->getType(), STOP_WARNING) == 0) {
        sendDeleteSubscription();
    }
    else {
        throw cRuntimeError("MECWarningAlertApp::handleUeMessage - packet not recognized");
    }
}

void MECWarningAlertApp::modifySubscription()
{
    std::string body = "{  \"circleNotificationSubscription\": {"
                       "\"callbackReference\" : {"
                       "\"callbackData\":\"1234\","
                       "\"notifyURL\":\"example.com/notification/1234\"},"
                       "\"checkImmediate\": \"false\","
                       "\"address\": \"" + ueAppAddress_.str() + "\","
                       "\"clientCorrelator\": \"null\","
                       "\"enteringLeavingCriteria\": \"Leaving\","
                       "\"frequency\": 5,"
                       "\"radius\": " + std::to_string(radius) + ","
                       "\"trackingAccuracy\": 10,"
                       "\"latitude\": " + std::to_string(centerPositionX) + ","           // as x
                       "\"longitude\": " + std::to_string(centerPositionY) + ""        // as y
                       "}"
                       "}\r\n";
    std::string uri = "/example/location/v2/subscriptions/area/circle/" + subId;
    std::string host = mecServices[LS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[LS]->serviceSocket_->getRemotePort());
    Http::sendPutRequest(mecServices[LS]->serviceSocket_, body.c_str(), host.c_str(), uri.c_str());
}

void MECWarningAlertApp::sendSubscription()
{
    EV << "MECWarningAlertApp::sendSubscription" << endl;

    std::string body = "{  \"circleNotificationSubscription\": {"
                       "\"callbackReference\" : {"
                       "\"callbackData\":\"1234\","
                       "\"notifyURL\":\"example.com/notification/1234\"},"
                       "\"checkImmediate\": \"false\","
                       "\"address\": \"" + ueAppAddress_.str() + "\","
                       "\"clientCorrelator\": \"null\","
                       "\"enteringLeavingCriteria\": \"Entering\","
                       "\"frequency\": 5,"
                       "\"radius\": " + std::to_string(radius) + ","
                       "\"trackingAccuracy\": 10,"
                       "\"latitude\": " + std::to_string(centerPositionX) + ","           // as x
                       "\"longitude\": " + std::to_string(centerPositionY) + ""        // as y
                       "}"
                       "}\r\n";
    std::string uri = "/example/location/v2/subscriptions/area/circle";
    std::string host = mecServices[LS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[LS]->serviceSocket_->getRemotePort());

    if (par("logger").boolValue()) {
        std::ofstream myfile;
        myfile.open("example.txt", std::ios::app);
        if (myfile.is_open()) {
            myfile << "[" << NOW << "] MEWarningAlertApp - Sent POST circleNotificationSubscription to the Location Service \n";
            myfile.close();
        }
    }

    Http::sendPostRequest(mecServices[LS]->serviceSocket_, body.c_str(), host.c_str(), uri.c_str());
}

void MECWarningAlertApp::sendDeleteSubscription()
{
    std::string uri = "/example/location/v2/subscriptions/area/circle/" + subId;
    std::string host = mecServices[LS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[LS]->serviceSocket_->getRemotePort());
    Http::sendDeleteRequest(mecServices[LS]->serviceSocket_, host.c_str(), uri.c_str());
}

void MECWarningAlertApp::established(int connId)
{
    if (connId == mp1Socket_->getSocketId()) {
        EV << "MECWarningAlertApp::established - Mp1Socket" << endl;
        // get endPoint of the required service
        requiredSerName_ = "LocationService";
        MecAppBase::established(connId);

    }
    else if (mecServices[LS] != nullptr && connId == mecServices[LS]->serviceSocket_->getSocketId()) {
        EV << "MECWarningAlertApp::established - serviceSocket" << endl;
        // the connectService message is scheduled after a start mec app from the UE app, so I can
        // respond to her here, once the socket is established
        auto ack = inet::makeShared<WarningAppPacket>();
        ack->setType(START_ACK);
        ack->setChunkLength(inet::B(2));
        inet::Packet *packet = new inet::Packet("WarningAlertPacketInfo");
        packet->insertAtBack(ack);
        ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
        sendSubscription();
        return;
    }
    else {
        MecAppBase::established(connId);
//        throw cRuntimeError("MecAppBase::socketEstablished - Socket %d not recognized", connId);
    }
}

void MECWarningAlertApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}

void MECWarningAlertApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[LS]->serviceSocket_->getUserData());
    serviceHttpMessage = check_and_cast_nullable<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());

    if (serviceHttpMessage->getType() == REQUEST) {
        Http::send204Response(mecServices[LS]->serviceSocket_); // send back 204 no content
        nlohmann::json jsonBody;
        EV << "MECWarningAlertApp::handleTcpMsg - REQUEST " << serviceHttpMessage->getBody() << endl;
        try {
            jsonBody = nlohmann::json::parse(serviceHttpMessage->getBody()); // get the JSON structure
        }
        catch (nlohmann::detail::parse_error e) {
            std::cout << e.what() << std::endl;
            // body is not correctly formatted in JSON, manage it
            return;
        }
        if (jsonBody.contains("subscriptionNotification")) {
            if (jsonBody["subscriptionNotification"].contains("enteringLeavingCriteria")) {
                nlohmann::json criteria = jsonBody["subscriptionNotification"]["enteringLeavingCriteria"];
                auto alert = inet::makeShared<WarningAlertPacket>();
                alert->setType(WARNING_ALERT);

                if (criteria == "Entering") {
                    EV << "MECWarningAlertApp::handleTcpMsg - UE has entered the danger zone " << endl;
                    alert->setDanger(true);

                    if (par("logger").boolValue()) {
                        ofstream myfile;
                        myfile.open("example.txt", ios::app);
                        if (myfile.is_open()) {
                            myfile << "[" << NOW << "] MECWarningAlertApp - Received circleNotificationSubscription notification from Location Service. UE has entered the red zone! \n";
                            myfile.close();
                        }
                    }

                    // send subscription for leaving..
                    modifySubscription();
                }
                else if (criteria == "Leaving") {
                    EV << "MECWarningAlertApp::handleTcpMsg - UE has left the danger zone " << endl;
                    alert->setDanger(false);
                    if (par("logger").boolValue()) {
                        ofstream myfile;
                        myfile.open("example.txt", ios::app);
                        if (myfile.is_open()) {
                            myfile << "[" << NOW << "] MECWarningAlertApp - Received circleNotificationSubscription notification from Location Service. UE has exited the red zone! \n";
                            myfile.close();
                        }
                    }
                    sendDeleteSubscription();
                }

                alert->setPositionX(jsonBody["subscriptionNotification"]["terminalLocationList"]["currentLocation"]["x"]);
                alert->setPositionY(jsonBody["subscriptionNotification"]["terminalLocationList"]["currentLocation"]["y"]);
                alert->setChunkLength(inet::B(20));
                alert->addTagIfAbsent<inet::CreationTimeTag>()->setCreationTime(simTime());

                inet::Packet *packet = new inet::Packet("WarningAlertPacketInfo");
                packet->insertAtBack(alert);
                ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
            }
        }
    }
    else if (serviceHttpMessage->getType() == RESPONSE) {
        HttpResponseMessage *rspMsg = dynamic_cast<HttpResponseMessage *>(serviceHttpMessage);

        if (rspMsg->getCode() == 204) { // in response to a DELETE
            EV << "MECWarningAlertApp::handleTcpMsg - response 204, removing circle" << rspMsg->getBody() << endl;
            mecServices[LS]->serviceSocket_->close();
            getSimulation()->getSystemModule()->getCanvas()->removeFigure(circle);
        }
        else if (rspMsg->getCode() == 201) { // in response to a POST
            nlohmann::json jsonBody;
            EV << "MECWarningAlertApp::handleTcpMsg - response 201 " << rspMsg->getBody() << endl;
            try {
                jsonBody = nlohmann::json::parse(rspMsg->getBody()); // get the JSON structure
            }
            catch (nlohmann::detail::parse_error e) {
                EV << e.what() << endl;
                // body is not correctly formatted in JSON, manage it
                return;
            }
            std::string resourceUri = jsonBody["circleNotificationSubscription"]["resourceURL"];
            std::size_t lastPart = resourceUri.find_last_of("/");
            if (lastPart == std::string::npos) {
                EV << "1" << endl;
                return;
            }
            // find_last_of does not take into account if the uri has a last /
            // in this case subscriptionType would be empty and the baseUri == uri
            // by the way the next if statement solves this problem
            std::string baseUri = resourceUri.substr(0, lastPart);
            //save the id
            subId = resourceUri.substr(lastPart + 1);
            EV << "subId: " << subId << endl;

            circle = new cOvalFigure("circle");
            circle->setBounds(cFigure::Rectangle(centerPositionX - radius, centerPositionY - radius, radius * 2, radius * 2));
            circle->setLineWidth(2);
            circle->setLineColor(cFigure::RED);
            getSimulation()->getSystemModule()->getCanvas()->addFigure(circle);
        }
    }
}

void MECWarningAlertApp::handleSelfMessage(cMessage *msg)
{
    if (strcmp(msg->getName(), "connectService") == 0) {
        EV << "MecAppBase::handleMessage- " << msg->getName() << endl;
        bool sendWarningAlertPacketInfo;
        if (mecServices[LS]->serviceAddress_.isUnspecified()) {
            EV << "MECWarningAlertApp::handleSelfMessage - service IP address is unspecified (maybe response from the service registry is arriving)" << endl;
            sendWarningAlertPacketInfo = true;
        }
        else
            switch (mecServices[LS]->serviceSocket_->getState()) {
                case inet::TcpSocket::PEER_CLOSED:
                case inet::TcpSocket::LOCALLY_CLOSED:
                case inet::TcpSocket::CLOSED:
                case inet::TcpSocket::SOCKERROR:
                    mecServices[LS]->serviceSocket_->renewSocket();
                    // nobreak;
                case inet::TcpSocket::NOT_BOUND:
                case inet::TcpSocket::BOUND:
                    connect(mecServices[LS]->serviceSocket_, mecServices[LS]->serviceAddress_, mecServices[LS]->servicePort_);
                    sendWarningAlertPacketInfo = false;
                    break;
                case inet::TcpSocket::CONNECTED:
                    EV << "MECWarningAlertApp::handleSelfMessage - service socket is already connected" << endl;
                    sendWarningAlertPacketInfo = true;
                    break;
                case inet::TcpSocket::CONNECTING:
                    EV << "MECWarningAlertApp::handleSelfMessage - service socket is already connecting" << endl;
                    sendWarningAlertPacketInfo = true;
                    break;
                default:
                    throw cRuntimeError("Unhandled socket state: %d", (int)mecServices[LS]->serviceSocket_->getState());
            }

        if (sendWarningAlertPacketInfo) {
            auto nack = inet::makeShared<WarningAppPacket>();
            // the connectService message is scheduled after a start mec app from the UE app, so I can
            // respond to her here
            nack->setType(START_NACK);
            nack->setChunkLength(inet::B(2));
            inet::Packet *packet = new inet::Packet("WarningAlertPacketInfo");
            packet->insertAtBack(nack);
            ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
        }
    }

    delete msg;
}

void MECWarningAlertApp::handleProcessedMessage(cMessage *msg)
{
    EV << "MECWarningAlertApp::handleProcessedMessage " << endl;
    if (!msg->isSelfMessage()) {
        if (ueAppSocket_.belongsToSocket(msg)) {
            handleUeMessage(msg);
            delete msg;
            return;
        }
    }
    MecAppBase::handleProcessedMessage(msg);
}

} //namespace

