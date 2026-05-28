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

#include "apps/mec/MecApps/MecRequestBackgroundApp/MecRequestBackgroundApp.h"

#include <string>

#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/chunk/BytesChunk.h>

#include "nodes/mec/MECPlatform/MECServices/packets/HttpRequestMessage/HttpRequestMessage.h"
#include "nodes/mec/MECPlatform/MECServices/packets/HttpResponseMessage/HttpResponseMessage.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"

namespace simu5g {

using namespace inet;
Define_Module(MecRequestBackgroundApp);

MecRequestBackgroundApp::~MecRequestBackgroundApp() {
    cancelAndDelete(sendBurst);
    cancelAndDelete(burstPeriod);
    cancelAndDelete(burstTimer);
}


void MecRequestBackgroundApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[LS]->serviceSocket_->getUserData());
    serviceHttpMessage = check_and_cast<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());
    EV << "payload: " << serviceHttpMessage->getBody() << endl;
}

void MecRequestBackgroundApp::initialize(int stage) {
    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;
    MecAppBase::initialize(stage);
    sendBurst = new cMessage("sendBurst");
    burstPeriod = new cMessage("burstPeriod");
    burstTimer = new cMessage("burstTimer");
    burstFlag = false;
    lambda = par("lambda").doubleValue();
    mecAppId = getId();
}

void MecRequestBackgroundApp::sendRequest() {

    /*
     * For a background mec application, I send the request as a bulk request.
     * This allows the service to only count the foreground requests, since they emit
     * the signal queuedRequests.
     *
     */
    std::string payload = "BulkRequest: 1";// + std::to_string(numberOfApplications_);
    Http::sendPacket(payload.c_str(), mecServices[LS]->serviceSocket_);
    EV << "sent 1 request to the server" << endl;
}

void MecRequestBackgroundApp::established(int connId)
{
    if (connId == mp1Socket_->getSocketId()) {
        EV << "MecRequestBackgroundApp::established - Mp1Socket" << endl;
        // get endPoint of the required service
        requiredSerName_ = "LocationService";
        MecAppBase::established(connId);
    }
    else if (connId == mecServices[LS]->serviceSocket_->getSocketId()) {
        EV << "MecRequestBackgroundApp::established - serviceSocket" << endl;
        scheduleAt(simTime() + exponential(lambda, 2), burstTimer);
    }
    else {
        throw cRuntimeError("MecRequestBackgroundApp::socketEstablished - Socket %d not recognized", connId);
    }
}

void MecRequestBackgroundApp::handleSelfMessage(cMessage *msg) {
    if (strcmp(msg->getName(), "burstTimer") == 0) {
        sendRequest();
        scheduleAt(simTime() + exponential(lambda, 2), burstTimer);
    }
    else if (strcmp(msg->getName(), "connectService") == 0) {
        EV << "MecAppBase::handleMessage- " << msg->getName() << endl;
        connect(mecServices[LS]->serviceSocket_, mp1Address, mp1Port);
        delete msg;
    }
    else {
        EV << "MecRequestBackgroundApp::handleSelfMessage - selfMessage not recognized" << endl;
        delete msg;
    }
}

void MecRequestBackgroundApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}

void MecRequestBackgroundApp::finish()
{
}

} //namespace

