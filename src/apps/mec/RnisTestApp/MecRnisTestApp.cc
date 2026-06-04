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

#include "apps/mec/RnisTestApp/MecRnisTestApp.h"

#include <fstream>

#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/Packet_m.h>
#include <inet/networklayer/common/L3AddressTag_m.h>
#include <inet/transportlayer/common/L4PortTag_m.h>

#include "apps/mec/DeviceApp/DeviceAppMessages/DeviceAppPacket_Types.h"
#include "nodes/mec/MECPlatform/MECServices/packets/HttpResponseMessage/HttpResponseMessage.h"
#include "apps/mec/RnisTestApp/packets/RnisTestAppPacket_Types.h"
#include "apps/mec/RnisTestApp/packets/RnisTestAppPacket_m.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"
#include "nodes/mec/utils/httpUtils/json.hpp"

namespace simu5g {

Define_Module(MecRnisTestApp);

using namespace inet;
using namespace omnetpp;

void MecRnisTestApp::initialize(int stage)
{
    MecAppBase::initialize(stage);

    // avoid multiple initializations
    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;

    // set Udp Socket
    ueAppSocket_.setOutputGate(gate("socketOut"));

    localUePort_ = par("localUePort");
    ueAppSocket_.bind(localUePort_);

    //testing
    EV << "MecRnisTestApp::initialize - Mec application " << getClassName() << " with mecAppId[" << mecAppId << "] has started!" << endl;

}

void MecRnisTestApp::finish()
{
    MecAppBase::finish();
    EV << "MecRnisTestApp::finish()" << endl;

    if (gate("socketOut")->isConnected()) {
    }
}

void MecRnisTestApp::handleUeMessage(cMessage *msg)
{
    // determine its source address/port
    auto pk = check_and_cast<Packet *>(msg);
    ueAppAddress_ = pk->getTag<L3AddressInd>()->getSrcAddress();
    ueAppPort_ = pk->getTag<L4PortInd>()->getSrcPort();

    auto mecPk = pk->peekAtFront<RnisTestAppPacket>();

    if (strcmp(mecPk->getType(), START_QUERY_RNIS) == 0) {
        EV << "MecRnisTestApp::handleUeMessage - RnisTestAppStartPacket arrived" << endl;
        auto startMsg = dynamicPtrCast<const RnisTestAppStartPacket>(mecPk);
        if (startMsg == nullptr)
            throw cRuntimeError("MecRnisTestApp::handleUeMessage - RnisTestAppStartPacket is null");

        if (par("logger").boolValue()) {
            std::ofstream myfile;
            myfile.open("example.txt", std::ios::app);
            if (myfile.is_open()) {
                myfile << "[" << NOW << "] MecRnisTestApp - Received START message from UE, connecting to the RNIS\n";
                myfile.close();
            }
        }

        // get period
        rnisQueryingPeriod_ = startMsg->getPeriod();

        EV << "MecRnisTestApp::handleUeMessage - UE has asked the MEC app to connect to the RNIService" << endl;

        cMessage *m = new cMessage("connectService");
        scheduleAt(simTime() + 0.005, m);
    }
    else if (strcmp(mecPk->getType(), STOP_QUERY_RNIS) == 0) {
        EV << "MecRnisTestApp::handleUeMessage - RnisTestAppStopPacket arrived" << endl;
        auto stopMsg = dynamicPtrCast<const RnisTestAppStopPacket>(mecPk);
        if (stopMsg == nullptr)
            throw cRuntimeError("MecRnisTestApp::handleUeMessage - RnisTestAppStopPacket is null");

        // stop periodic timer
        cancelAndDelete(rnisQueryingTimer_);

        EV << "MecRnisTestApp::handleUeMessage - periodic querying to RNIS has been stopped" << endl;

        inet::Packet *packet = new inet::Packet("RnisTestAppAckPacket");
        auto ack = inet::makeShared<RnisTestAppStopPacket>();
        ack->setType(STOP_QUERY_RNIS_ACK);
        ack->setChunkLength(inet::B(2));
        packet->insertAtBack(ack);
        ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
        EV << "MecRnisTestApp::handleUeMessage - an ACK has been sent to the UE" << endl;

        if (par("logger").boolValue()) {
            std::ofstream myfile;
            myfile.open("example.txt", std::ios::app);
            if (myfile.is_open()) {
                myfile << "[" << NOW << "] MecRnisTestApp - Received STOP message from UE, stopping querying the RNIS\n";
                myfile.close();
            }
        }

        // TODO should we disconnect from the RNIS?
    }
    else {
        throw cRuntimeError("MecRnisTestApp::handleUeMessage - packet not recognized");
    }
}

void MecRnisTestApp::sendQuery(int cellId, std::string ueIpv4Address)
{
    std::string host = mecServices[RNIS]->serviceSocket_->getRemoteAddress().str() + ":" + std::to_string(mecServices[RNIS]->serviceSocket_->getRemotePort());

    std::string uri = "/example/rni/v2/queries/layer2_meas";

    unsigned short numPars = 0;
    std::string query_string = "";

    if (cellId != 0) {
        query_string += "cell_id=" + std::to_string(cellId);
        numPars++;
    }

    if (ueIpv4Address != "") {
        if (numPars > 0)
            query_string += "&";
        query_string += "ue_ipv4_address=" + ueIpv4Address;
    }

    uri += ("?" + query_string);

    if (par("logger").boolValue()) {
        std::ofstream myfile;
        myfile.open("example.txt", std::ios::app);
        if (myfile.is_open()) {
            myfile << "[" << NOW << "] MecRnisTestApp - Sent GET layer2_meas query to the Radio Network Information Service \n";
            myfile.close();
        }
    }

    EV << "MecRnisTestApp::sendQuery - GET request, Host[" << host << "] URI[" << uri << "]" << endl;

    Http::sendGetRequest(mecServices[RNIS]->serviceSocket_, host.c_str(), uri.c_str());
}

void MecRnisTestApp::established(int connId)
{
    if (connId == mp1Socket_->getSocketId()) {
        EV << "MecRnisTestApp::established - Mp1Socket" << endl;
        // get endPoint of the required service
        requiredSerName_ = "RNIService";
        MecAppBase::established(connId);
    }
    else if (connId == mecServices[RNIS]->serviceSocket_->getSocketId()) {
        EV << "MecRnisTestApp::established - serviceSocket" << endl;

        EV << "MecRnisTestApp::established - connection to the RNIService done... sending ACK to the UE" << endl;

        // the connectService message is scheduled after a start mec app from the UE app, so I can
        // respond to her here, once the socket is established
        auto ack = inet::makeShared<RnisTestAppPacket>();
        ack->setType(START_QUERY_RNIS_ACK);
        ack->setChunkLength(inet::B(2));
        inet::Packet *packet = new inet::Packet("RnisTestAppAckPacket");
        packet->insertAtBack(ack);
        ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);

        EV << "MecRnisTestApp::established - querying the RNIService" << endl;

        // send first request to the RNIS
        sendQuery(0, ueAppAddress_.toIpv4().str());

        // set periodic timer for next queries
        if (rnisQueryingPeriod_ > 0) {
            rnisQueryingTimer_ = new cMessage("rnisQueryingTimer");
            scheduleAfter(rnisQueryingPeriod_, rnisQueryingTimer_);
            EV << "MecRnisTestApp::established - next query to the RNIService in " << rnisQueryingPeriod_ << " seconds " << endl;
        }
    }
    else {
        MecAppBase::established(connId);
//        throw cRuntimeError("MecAppBase::socketEstablished - Socket %d not recognized", connId);
    }
}

void MecRnisTestApp::handleHttpMessage(int connId)
{
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
//        handleMp1Message(connId);
        return;
    }
    else {
        handleServiceMessage(connId);
    }
}


void MecRnisTestApp::handleServiceMessage(int connId)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mecServices[RNIS]->serviceSocket_->getUserData());
    serviceHttpMessage = check_and_cast_nullable<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());

    if (serviceHttpMessage->getType() == REQUEST) {
        // should never go here?
    }
    else if (serviceHttpMessage->getType() == RESPONSE) {
        HttpResponseMessage *rspMsg = dynamic_cast<HttpResponseMessage *>(serviceHttpMessage);

        if (rspMsg->getCode() == 200) { // in response to a GET
            // read values

            nlohmann::json jsonBody;
            EV << "MecRnisTestApp::handleServiceMessage - response 200 " << rspMsg->getBody() << endl;
            try {
                jsonBody = nlohmann::json::parse(rspMsg->getBody()); // get the JSON structure
            }
            catch (nlohmann::detail::parse_error e) {
                EV << e.what() << endl;
                // body is not correctly formatted in JSON, manage it
                return;
            }
            EV << "MecRnisTestApp::handleServiceMessage - received information from RNIS:" << endl;
            EV << jsonBody.dump(4) << endl;

            // TODO forward this message to the UE
            EV << "MecRnisTestApp::handleServiceMessage - sending RNIS info to the UE" << endl;

            inet::Packet *packet = new inet::Packet("RnisTestAppInfoPacket");

            auto rnisInfo = inet::makeShared<RnisTestAppInfoPacket>();
            rnisInfo->setType(RNIS_INFO);
            rnisInfo->setL2meas(jsonBody);
            rnisInfo->setChunkLength(inet::B(jsonBody.dump(4).length()));
            packet->insertAtBack(rnisInfo);
            ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
        }
    }
}

void MecRnisTestApp::handleSelfMessage(cMessage *msg)
{
        if (strcmp(msg->getName(), "connectService") == 0) {
        EV << "MecAppBase::handleMessage- " << msg->getName() << endl;
        if (!mecServices[RNIS]->serviceAddress_.isUnspecified() && mecServices[RNIS]->serviceSocket_->getState() != inet::TcpSocket::CONNECTED) {
            EV << "MecRnisTestApp::handleSelfMessage - socket has already been created... now connecting to the RNIService" << endl;
            connect(mecServices[RNIS]->serviceSocket_, mecServices[RNIS]->serviceAddress_, mecServices[RNIS]->servicePort_);
        }
        else {
            if (mecServices[RNIS]->serviceAddress_.isUnspecified())
                EV << "MecRnisTestApp::handleSelfMessage - service IP address is unspecified (maybe response from the service registry is arriving)" << endl;
            else if (mecServices[RNIS]->serviceSocket_->getState() == inet::TcpSocket::CONNECTED)
                EV << "MecRnisTestApp::handleSelfMessage - service socket is already connected" << endl;

            EV << "MecRnisTestApp::handleSelfMessage - socket has not been created for some reason... cannot connect to the RNIService, now sending NACK to UE" << endl;

            inet::Packet *packet = new inet::Packet("RnisTestAppNackPacket");
            auto nack = inet::makeShared<RnisTestAppStartPacket>();

            // the connectService message is scheduled after a start mec app from the UE app, so I can
            // respond to her here
            nack->setType(START_QUERY_RNIS_NACK);
            nack->setChunkLength(inet::B(2));
            packet->insertAtBack(nack);
            ueAppSocket_.sendTo(packet, ueAppAddress_, ueAppPort_);
        }
    }
    else if (strcmp(msg->getName(), "rnisQueryingTimer") == 0) {
        EV << "MecRnisTestApp::handleSelfMessage - rnisQueryingTimer expired: now querying the RNIService" << endl;

        // send request to the RNIS
        sendQuery(0, ueAppAddress_.toIpv4().str());

        scheduleAfter(rnisQueryingPeriod_, rnisQueryingTimer_);
        EV << "MecRnisTestApp::handleSelfMessage - next query to the RNIService in " << rnisQueryingPeriod_ << " seconds " << endl;
        return;
    }

    delete msg;
}

void MecRnisTestApp::handleProcessedMessage(cMessage *msg)
{
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

