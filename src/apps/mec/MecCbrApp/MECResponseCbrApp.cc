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

#include "apps/mec/MecCbrApp/MECResponseCbrApp.h"

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

Define_Module(MECResponseCbrApp);

using namespace inet;
using namespace omnetpp;

MECResponseCbrApp::~MECResponseCbrApp()
{
    delete currentRequestMsg_;
    cancelAndDelete(requestMsg_);
    cancelAndDelete(processingTimer_);
}

void MECResponseCbrApp::initialize(int stage)
{
    MecAppBase::initialize(stage);

    // avoid multiple initializations
    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;

    EV << "MECResponseCbrApp::initialize - MEC application " << getClassName() << " with mecAppId[" << mecAppId << "] has started!" << endl;

    localUePort_ = par("localUePort");
    ueAppSocket_.setOutputGate(gate("socketOut"));
    ueAppSocket_.bind(localUePort_);

    packetSize_ = B(par("responsePacketSize"));

    requestMsg_ = new cMessage("requestMsg");
    processingTimer_ = new cMessage("computeMsg");

    minInstructions_ = par("minInstructions");
    maxInstructions_ = par("maxInstructions");
}


void MECResponseCbrApp::handleProcessedMessage(cMessage *msg)
{
    EV << "MECResponseCbrApp::handleProcessedMessage" << endl;
    if (!msg->isSelfMessage()) {
        if (ueAppSocket_.belongsToSocket(msg)) {
            EV << "MECResponseCbrApp::handleProcessedMessage: received message from UE" << endl;
            auto msgDup = msg->dup();
            inet::Packet *packet = check_and_cast<inet::Packet *>(msgDup);
            auto req = packet->peekAtFront<RequestResponseAppPacket>();
            if (req->getType() == UEAPP_REQUEST)
                handleRequest(msgDup);
            else if (req->getType() == UEAPP_STOP)
                handleStopRequest(msgDup);
            else
                throw cRuntimeError("MECResponseCbrApp::handleProcessedMessage - Type not recognized!");
//            return;
        }
    }
    MecAppBase::handleProcessedMessage(msg);
}

void MECResponseCbrApp::finish()
{
    MecAppBase::finish();
    EV << "MECResponseCbrApp::finish()" << endl;
    if (gate("socketOut")->isConnected()) {
    }
}

double MECResponseCbrApp::scheduleNextMsg(cMessage *msg)
{
    return MecAppBase::scheduleNextMsg(msg);
}

void MECResponseCbrApp::handleSelfMessage(cMessage *msg)
{
    if (!strcmp(msg->getName(), "computeMsg")) {
        EV << "MECResponseCbrApp::handleSelfMessage - computeMsg" << endl;
        sendResponse();
    }
    else if (!strcmp(msg->getName(), "requestMsg")) {
        EV << "MECResponseCbrApp::handleSelfMessage - requestMsg" << endl;
        handleRequest(check_and_cast<cMessage *>(requestPktQueue_.pop()));
    }
}

void MECResponseCbrApp::handleRequest(cMessage *msg)
{
    EV << "MECResponseCbrApp::handleRequest" << endl;
    // this method pretends to perform some computation after having
    //.request some info to the RNI

    inet::Packet *packet = check_and_cast<inet::Packet *>(msg);

    if (!packet->peekAtFront<RequestResponseAppPacket>()->getRequestArrivedTimestamp().isZero()) {
        if (currentRequestMsg_ != nullptr) {
//            if (!requestPktQueue_.contains(msg))
//                requestPktQueue_.insert(msg);
//            return;
            throw cRuntimeError("MECResponseCbrApp::handleRequest - currentRequestMsg_ not null");

        }
        currentRequestMsg_ = msg;
        sendGetRequest();
        getRequestSent_ = simTime();
    }
    else {
        auto req = packet->removeAtFront<RequestResponseAppPacket>();
        req->setRequestArrivedTimestamp(simTime());
        packet->insertAtFront(req);
        requestPktQueue_.insert(check_and_cast<cMessage *>(packet));
        if (currentRequestMsg_ == nullptr && !requestPktQueue_.isEmpty() && !requestMsg_->isScheduled())
            scheduleAt(simTime(), requestMsg_);
    }
}

void MECResponseCbrApp::handleStopRequest(cMessage *msg)
{
    EV << "MECResponseCbrApp::handleStopRequest" << endl;
    mecServices[RNIS]->serviceSocket_->close();
}

void MECResponseCbrApp::sendResponse()
{
    EV << "MECResponseCbrApp::sendResponse()" << endl;
    if (currentRequestMsg_ == nullptr) {
        if(!requestMsg_->isScheduled())
            scheduleAt(simTime(), requestMsg_);
        return;
    }

    inet::Packet *packet = check_and_cast<inet::Packet *>(currentRequestMsg_);
    ueAppAddress_ = packet->getTag<L3AddressInd>()->getSrcAddress();
    ueAppPort_ = packet->getTag<L4PortInd>()->getSrcPort();

    auto req = packet->removeAtFront<RequestResponseAppPacket>();
    req->setType(MECAPP_RESPONSE);
    req->setServiceResponseTime(getRequestArrived_ - getRequestSent_);
    req->setResponseSentTimestamp(simTime());
    req->setProcessingTime(processingTime_);
    req->setChunkLength(packetSize_);
    inet::Packet *pkt = new inet::Packet("ResponseAppPacket");
    pkt->insertAtBack(req);

    ueAppSocket_.sendTo(pkt, ueAppAddress_, ueAppPort_);

    //clean current request
    delete packet;
    currentRequestMsg_ = nullptr;
    processingTime_ = 0;
    getRequestArrived_ = 0;
    getRequestSent_ = 0;
    if (currentRequestMsg_ == nullptr && !requestPktQueue_.isEmpty() && !requestMsg_->isScheduled()) {
        scheduleAt(simTime(), requestMsg_);
    }
}

void MECResponseCbrApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}

void MECResponseCbrApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[RNIS]->serviceSocket_->getUserData());
    HttpBaseMessage *httpMessage = check_and_cast<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());

    if (httpMessage->getType() == RESPONSE) {
        HttpResponseMessage *rspMsg = dynamic_cast<HttpResponseMessage *>(httpMessage);
        if (rspMsg->getCode() == 200) { // in response to a successful GET request
            EV << "MECResponseCbrApp::handleServiceMessage - response 200 from Socket with Id [" << connId << "]" << endl;
            getRequestArrived_ = simTime();
            EV << "response time " << getRequestArrived_ - getRequestSent_ << endl;
            doComputation();
        }
        // some error occured, show the HTTP code for now
        else {
            EV << "MECResponseCbrApp::handleServiceMessage - response with HTTP code:  " << rspMsg->getCode() << endl;
        }
    }
}

void MECResponseCbrApp::doComputation()
{
    processingTime_ = 0;    //exponential(0.010);
//    processingTime_ = vim->calculateProcessingTime(mecAppId, uniform(minInstructions_, maxInstructions_));
    EV << "processing time " << processingTime_ << endl;
    if (!processingTimer_->isScheduled())
        scheduleAt(simTime() + processingTime_, processingTimer_);
}

void MECResponseCbrApp::sendGetRequest()
{
    //check if the ueAppAddress is specified
    if (mecServices[RNIS]->serviceSocket_->getState() == inet::TcpSocket::CONNECTED) {
        EV << "MECResponseCbrApp::sendGetRequest(): send request to the RNI Service" << endl;
        std::stringstream uri;
        uri << "/example/rni/v2/queries/layer2_meas"; //TODO filter the request to get less data
        EV << "MECResponseCbrApp::requestLocation(): uri: " << uri.str() << endl;
        std::string host = mecServices[RNIS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[RNIS]->serviceSocket_->getRemotePort());
        Http::sendGetRequest(mecServices[RNIS]->serviceSocket_, host.c_str(), uri.str().c_str());
    }
    else {
        EV << "MECResponseCbrApp::sendGetRequest(): RNI Service not connected" << endl;
    }
}

void MECResponseCbrApp::established(int connId)
{
    EV << "MECResponseCbrApp::established - connId [" << connId << "]" << endl;

    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        EV << "MECResponseCbrApp::established - Mp1Socket" << endl;
        // once the connection with the Service Registry has been established, obtain the
        // endPoint (address+port) of the Location Service
        requiredSerName_ = "RNIService";
        MecAppBase::established(connId);

    }
    else if (mecServices[RNIS] != nullptr && connId == mecServices[RNIS]->serviceSocket_->getSocketId()) {

        EV << "MECResponseCbrApp::established - RNISSocket" << endl;
    }
    else {
        MecAppBase::established(connId);

    }
}

void MECResponseCbrApp::socketClosed(inet::TcpSocket *sock)
{
    EV << "MECResponseCbrApp::socketClosed" << endl;
    std::cout << "MECResponseCbrApp::socketClosed with sockId " << sock->getSocketId() << std::endl;
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

void MECResponseCbrApp::sendStopAck()
{
    inet::Packet *pkt = new inet::Packet("RequestResponseAppPacket");
    auto req = inet::makeShared<RequestResponseAppPacket>();
    req->setType(UEAPP_ACK_STOP);
    req->setChunkLength(packetSize_);
    pkt->insertAtBack(req);

    ueAppSocket_.sendTo(pkt, ueAppAddress_, ueAppPort_);
}

} //namespace

