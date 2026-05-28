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

#include "apps/mec/MecApps/MecRequestForegroundApp/MecRequestForegroundApp.h"

#include <string>

#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/chunk/BytesChunk.h>

#include "nodes/mec/MECPlatform/MECServices/packets/HttpRequestMessage/HttpRequestMessage.h"
#include "nodes/mec/MECPlatform/MECServices/packets/HttpResponseMessage/HttpResponseMessage.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"

namespace simu5g {

using namespace inet;
Define_Module(MecRequestForegroundApp);

MecRequestForegroundApp::~MecRequestForegroundApp() {
    cancelAndDelete(sendFGRequest);
}

void MecRequestForegroundApp::initialize(int stage) {
    MecAppBase::initialize(stage);
    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        mecAppId = getId();
        sendFGRequest = new cMessage("sendFGRequest");
    }
}

void MecRequestForegroundApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[LS]->serviceSocket_->getUserData());
    serviceHttpMessage = check_and_cast<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());
    EV << "payload: " << serviceHttpMessage->getBody() << endl;
    scheduleAt(simTime() + par("meanRequestTime"), sendFGRequest);
}

void MecRequestForegroundApp::handleSelfMessage(cMessage *msg) {
    if (strcmp(msg->getName(), "sendFGRequest") == 0) {
        sendRequest();
        //scheduleAt(simTime() + exponential(lambda, 2), sendFGRequest);
    }
    else {
        EV << "MecRequestBackgroundApp::handleSelfMessage - selfMessage not recognized" << endl;
        delete msg;
    }
}

void MecRequestForegroundApp::established(int connId)
{
    if (connId == mp1Socket_->getSocketId()) {
        EV << "MecRequestBackgroundApp::established - Mp1Socket" << endl;
        // get endPoint of the required service
        requiredSerName_ = "LocationService";
        MecAppBase::established(connId);
    }
    else if (connId == mecServices[LS]->serviceSocket_->getSocketId()) {
        EV << "MecRequestBackgroundApp::established - serviceSocket" << endl;
        if (par("exponentialDist").boolValue())
            scheduleAt(simTime() + exponential(par("meanRequestTime").doubleValue()), sendFGRequest);
        else
            scheduleAt(simTime() + par("meanRequestTime"), sendFGRequest);

    }
    else {
        throw cRuntimeError("MecRequestBackgroundApp::socketEstablished - Socket %d not recognized", connId);
    }
}

void MecRequestForegroundApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}

void MecRequestForegroundApp::sendRequest() {
    const char *uri = "/example/location/v2/queries/users";
    std::string host = mecServices[LS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[LS]->serviceSocket_->getRemotePort());

    Http::sendGetRequest(mecServices[LS]->serviceSocket_, host.c_str(), uri);
}

} //namespace

