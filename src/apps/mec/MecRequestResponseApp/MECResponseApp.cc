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

#include "apps/mec/MecRequestResponseApp/MECResponseApp.h"

#include <fstream>

#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/Packet_m.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/transportlayer/common/L4PortTag_m.h>

#include "apps/mec/MecRequestResponseApp/packets/RequestResponsePacket_m.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"
#include "nodes/mec/utils/httpUtils/json.hpp"
#include "nodes/mec/MECPlatform/MECServices/packets/HttpResponseMessage/HttpResponseMessage.h"

namespace simu5g {

Define_Module(MECResponseApp);

using namespace inet;
using namespace omnetpp;

MECResponseApp::~MECResponseApp()
{
    delete currentRequestfMsg_;
    cancelAndDelete(processingTimer_);
}

void MECResponseApp::initialize(int stage)
{
    MecAppBase::initialize(stage);

    // avoid multiple initializations
    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;

    EV << "MECResponseApp::initialize - MEC application " << getClassName() << " with mecAppId[" << mecAppId << "] has started!" << endl;

    localUePort_ = par("localUePort");
    ueAppSocket_.setOutputGate(gate("socketOut"));
    ueAppSocket_.bind(localUePort_);

    packetSize_ = B(par("responsePacketSize"));

    processingTimer_ = new cMessage("computeMsg");

    minInstructions_ = par("minInstructions");
    maxInstructions_ = par("maxInstructions");

}

void MECResponseApp::handleProcessedMessage(cMessage *msg)
{
    if (!msg->isSelfMessage()) {
        if (ueAppSocket_.belongsToSocket(msg)) {
            EV << "MECResponseApp::handleProcessedMessage: received message from UE" << endl;
            auto msgDup = msg->dup();
            inet::Packet *packet = check_and_cast<inet::Packet *>(msgDup);
            auto req = packet->peekAtFront<RequestResponseAppPacket>();
            if (req->getType() == UEAPP_REQUEST)
                handleRequest(msgDup);
            else if (req->getType() == UEAPP_STOP)
                handleStopRequest(msgDup);
            else
                throw cRuntimeError("MECResponseApp::handleProcessedMessage - Type not recognized!");
//            return;
        }
    }
    MecAppBase::handleProcessedMessage(msg);
}

void MECResponseApp::finish()
{
    MecAppBase::finish();
    EV << "MECResponseApp::finish()" << endl;
    if (gate("socketOut")->isConnected()) {
    }
}

double MECResponseApp::scheduleNextMsg(cMessage *msg)
{
    return MecAppBase::scheduleNextMsg(msg);
}

void MECResponseApp::handleSelfMessage(cMessage *msg)
{
    if (!strcmp(msg->getName(), "computeMsg")) {
        sendResponse();
    }
}

void MECResponseApp::handleRequest(cMessage *msg)
{
    EV << "MECResponseApp::handleRequest" << endl;
    // this method pretends to perform some computation after having
    //.request some info to the RNI
    if (currentRequestfMsg_ != nullptr)
        throw cRuntimeError("MECResponseApp::handleRequest - currentRequestfMsg_ not null!");

    msgArrived_ = simTime();
    currentRequestfMsg_ = msg;
    sendGetRequest();
    getRequestSent_ = simTime();
}

void MECResponseApp::handleStopRequest(cMessage *msg)
{
    EV << "MECResponseApp::handleStopRequest" << endl;
    mecServices[RNIS]->serviceSocket_->close();
}

void MECResponseApp::sendResponse()
{
    inet::Packet *packet = check_and_cast<inet::Packet *>(currentRequestfMsg_);
    ueAppAddress_ = packet->getTag<L3AddressInd>()->getSrcAddress();
    ueAppPort_ = packet->getTag<L4PortInd>()->getSrcPort();

    auto req = packet->removeAtFront<RequestResponseAppPacket>();
    req->setType(MECAPP_RESPONSE);
    req->setRequestArrivedTimestamp(msgArrived_);
    req->setServiceResponseTime(getRequestArrived_ - getRequestSent_);
    req->setResponseSentTimestamp(simTime());
    req->setProcessingTime(processingTime_);
    req->setChunkLength(packetSize_);
    inet::Packet *pkt = new inet::Packet("ResponseAppPacket");
    pkt->insertAtBack(req);

    ueAppSocket_.sendTo(pkt, ueAppAddress_, ueAppPort_);

    //clean current request
    delete packet;
    currentRequestfMsg_ = nullptr;
    msgArrived_ = 0;
    processingTime_ = 0;
    getRequestArrived_ = 0;
    getRequestSent_ = 0;
}

void MECResponseApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}


void MECResponseApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[RNIS]->serviceSocket_->getUserData());
    HttpBaseMessage *httpMessage = check_and_cast<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());

    if (httpMessage->getType() == RESPONSE) {
        HttpResponseMessage *rspMsg = dynamic_cast<HttpResponseMessage *>(httpMessage);
        if (rspMsg->getCode() == 200) { // in response to a successful GET request
            EV << "MECResponseApp::handleServiceMessage - response 200 from Socket with Id [" << connId << "]" << endl;
            getRequestArrived_ = simTime();
            EV << "response time " << getRequestArrived_ - getRequestSent_ << endl;
            doComputation();
        }
        // some error occured, show the HTTP code for now
        else {
            EV << "MECResponseApp::handleServiceMessage - response with HTTP code:  " << rspMsg->getCode() << endl;
        }
    }
}

void MECResponseApp::doComputation()
{
    processingTime_ = vim->calculateProcessingTime(mecAppId, uniform(minInstructions_, maxInstructions_));
    EV << "time " << processingTime_ << endl;
    scheduleAt(simTime() + processingTime_, processingTimer_);
}

void MECResponseApp::sendGetRequest()
{
    //check if the ueAppAddress is specified
    if (mecServices[RNIS]->serviceSocket_->getState() == inet::TcpSocket::CONNECTED) {
        EV << "MECResponseApp::sendGetRequest(): send request to the Location Service" << endl;
        std::stringstream uri;
        uri << "/example/rni/v2/queries/layer2_meas"; //TODO filter the request to get less data
        EV << "MECResponseApp::requestLocation(): uri: " << uri.str() << endl;
        std::string host = mecServices[RNIS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[RNIS]->serviceSocket_->getRemotePort());
        Http::sendGetRequest(mecServices[RNIS]->serviceSocket_, host.c_str(), uri.str().c_str());
    }
    else {
        EV << "MECResponseApp::sendGetRequest(): Location Service not connected" << endl;
    }
}

void MECResponseApp::established(int connId)
{
    EV << "MECResponseApp::established - connId [" << connId << "]" << endl;

    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        EV << "MECResponseApp::established - Mp1Socket" << endl;

        // once the connection with the Service Registry has been established, obtain the
        // endPoint (address+port) of the Location Service
        requiredSerName_ = "LocationService";
        MecAppBase::established(connId);

    }
    else {
        MecAppBase::established(connId);
    }
}

void MECResponseApp::socketClosed(inet::TcpSocket *sock)
{
    EV << "MECResponseApp::socketClosed" << endl;
    std::cout << "MECResponseApp::socketClosed with sockId " << sock->getSocketId() << std::endl;
    if (mp1Socket_ != nullptr && sock->getSocketId() == mp1Socket_->getSocketId()) {
        removeSocket(sock);
        mp1Socket_ = nullptr;
    }
    else {
        EV << "Service socket closed" << endl;
        removeSocket(sock);
        sendStopAck();
    }
}

void MECResponseApp::sendStopAck()
{
    inet::Packet *pkt = new inet::Packet("RequestResponseAppPacket");
    auto req = inet::makeShared<RequestResponseAppPacket>();
    req->setType(UEAPP_ACK_STOP);
    req->setChunkLength(packetSize_);
    pkt->insertAtBack(req);

    ueAppSocket_.sendTo(pkt, ueAppAddress_, ueAppPort_);
}

} //namespace

