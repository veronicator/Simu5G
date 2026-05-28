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

#include "apps/mec/MecApps/MecAppBase.h"

#include <inet/common/ProtocolTag_m.h>
#include <inet/common/ProtocolGroup.h>
#include <inet/common/Protocol.h>

#include "apps/mec/MecApps/packets/ProcessingTimeMessage_m.h"
#include "nodes/mec/utils/MecCommon.h"
#include "nodes/mec/utils/httpUtils/httpUtils.h"

namespace simu5g {

using namespace omnetpp;
using namespace inet;


MecAppBase::~MecAppBase()
{
    std::cout << "MecAppBase::~MecAppBase()" << std::endl;
    cancelAndDelete(sendTimer);

    sockets_.deleteSockets();

    cancelAndDelete(processMessage_);
}

void MecAppBase::initialize(int stage)
{

    if (stage != inet::INITSTAGE_APPLICATION_LAYER)
        return;

    const char *mp1Ip = par("mp1Address");
    mp1Address = L3AddressResolver().resolve(mp1Ip);
    mp1Port = par("mp1Port");

    mecAppId = par("mecAppId"); // FIXME mecAppId is the deviceAppId (it does not change anything, though)
    requiredRam = par("requiredRam").doubleValue();
    requiredDisk = par("requiredDisk").doubleValue();
    requiredCpu = par("requiredCpu").doubleValue();

    vim.reference(this, "vimModule", true);
    mecPlatform.reference(this, "mecPlatformModule", true);

    serviceRegistry.reference(this, "serviceRegistryModule", true);

    mp1Socket_ = addNewSocket();


    isMobilityAware = par("isMobilityAware").boolValue();

    if (isMobilityAware) {
        mecServices[AMS] = new MecServiceSocketInfo;
        mecServices[AMS]->serviceSocket_ = addNewSocket();
    }

    // connect with the service registry
    cMessage *msg = new cMessage("connectMp1");
    scheduleAt(simTime() + 0, msg);

    processMessage_ = new cMessage("processedMessage");
}

void MecAppBase::connect(inet::TcpSocket *socket, const inet::L3Address& address, const int port)
{
    // we need a new connId if this is not the first connection
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket->setTimeToLive(timeToLive);

    int dscp = par("dscp");
    if (dscp != -1)
        socket->setDscp(dscp);

    int tos = par("tos");
    if (tos != -1)
        socket->setTos(tos);

    if (address.isUnspecified()) {
        EV_ERROR << "Connecting to " << address << " port=" << port << ": cannot resolve destination address\n";
    }
    else {
        EV_INFO << "Connecting to " << address << " port=" << port << endl;

        socket->connect(address, port);
    }
}

void MecAppBase::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (strcmp(msg->getName(), "connectMp1") == 0) {
            EV << "MecAppBase::handleMessage " << msg->getName() << endl;
            connect(mp1Socket_, mp1Address, mp1Port);
        }
        else if (strcmp(msg->getName(), "processedMessage") == 0) {
            EV << "MecAppBase::handleMessage() - processedMessage " << endl;
            handleProcessedMessage(check_and_cast<cMessage *>(packetQueue_.pop()));
            if (!packetQueue_.isEmpty()) {
                double processingTime = scheduleNextMsg(check_and_cast<cMessage *>(packetQueue_.front()));
                EV << "MecAppBase::scheduleNextMsg() - next msg is processed in " << processingTime << "s" << endl;
                scheduleAt(simTime() + processingTime, processMessage_);
            }
            else {
                EV << "MecAppBase::handleMessage - no more messages are present in the queue" << endl;
            }
        }
        else if (strcmp(msg->getName(), "processedHttpMsg") == 0) {
            EV << "MecAppBase::handleMessage(): processedHttpMsg " << endl;
            ProcessingTimeMessage *procMsg = check_and_cast<ProcessingTimeMessage *>(msg);
            int connId = procMsg->getSocketId();
            TcpSocket *sock = static_cast<TcpSocket *>(sockets_.getSocketById(connId));
            if (sock != nullptr) {
                HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(sock->getUserData());

                if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId())
                    MecAppBase::handleMp1Message(connId);
                else if (mecServices[AMS] != nullptr && connId == mecServices[AMS]->serviceSocket_->getSocketId())
                    MecAppBase::handleAmsMessage(connId);
                else
                    handleHttpMessage(connId);

                delete msgStatus->httpMessageQueue.pop();
                if (!msgStatus->httpMessageQueue.isEmpty()) {
                    EV << "MecAppBase::handleMessage(): processedHttpMsg - the httpMessageQueue is not empty, schedule next HTTP message" << endl;
                    double time = vim->calculateProcessingTime(mecAppId, 150);
                    scheduleAt(simTime() + time, msg);
                }
            }
        }
        else if (strcmp(msg->getName(), "amsConnectMessage") == 0) {
            EV << "MecAppBase::handleMessage(): amsConnectMessage " << endl;
            // todo check correctness
            if (strcmp(msg->getName(), "amsConnectMessage") == 0) {
                if (mecServices[AMS]->serviceAddress_.isUnspecified()) {
                    EV << "MECAppBase::handleSelfMessage - ams IP address is unspecified (maybe response from the service registry is arriving)" << endl;
                }
                else
                    switch (mecServices[AMS]->serviceSocket_->getState()) {
                        case inet::TcpSocket::PEER_CLOSED:
                        case inet::TcpSocket::LOCALLY_CLOSED:
                        case inet::TcpSocket::CLOSED:
                        case inet::TcpSocket::SOCKERROR:
                            mecServices[AMS]->serviceSocket_->renewSocket();
                            // nobreak;
                        case inet::TcpSocket::NOT_BOUND:
                        case inet::TcpSocket::BOUND:
                            connect(mecServices[AMS]->serviceSocket_, mecServices[AMS]->serviceAddress_, mecServices[AMS]->servicePort_);
                            break;
                        default:
                            EV << "MecAppBase::handleMessage - AMS socket state: " << (int)mecServices[AMS]->serviceSocket_->getState() << endl;
                    }
            }

        }
        else {
            handleSelfMessage(msg);
        }
    }
    else {
        if (!processMessage_->isScheduled() && packetQueue_.isEmpty()) {
            packetQueue_.insert(msg);
            double processingTime;
            if (strcmp(msg->getFullName(), "data") == 0)
                processingTime = MecAppBase::scheduleNextMsg(msg);
            else
                processingTime = scheduleNextMsg(msg);
            EV << "MecAppBase::scheduleNextMsg() - next msg is processed in " << processingTime << "s" << endl;
            scheduleAt(simTime() + processingTime, processMessage_);
        }
        else if (processMessage_->isScheduled() && !packetQueue_.isEmpty()) {
            packetQueue_.insert(msg);
        }
        else {
            throw cRuntimeError("MecAppBase::handleMessage - This situation is not possible");
        }
    }
}

void MecAppBase::handleMp1Message(int connId) {
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(mp1Socket_->getUserData());
    mp1HttpMessage = check_and_cast_nullable<HttpBaseMessage *>(msgStatus->httpMessageQueue.front());
    EV << "MecAppBase::handleMp1Message - payload: " << mp1HttpMessage->getBody() << endl;

    try {
        nlohmann::json jsonBodyTot = nlohmann::json::parse(mp1HttpMessage->getBody()); // get the JSON structure
        if (!jsonBodyTot.empty()) {
            nlohmann::json LSEndPoint = nullptr;
            nlohmann::json RNISEndPoint = nullptr;
            nlohmann::json AMSEndPoint = nullptr;
            bool localLS = false, localRNIS = false, localAMS = false;
            for (auto jsonBody: jsonBodyTot) {
                std::string serName = jsonBody["serName"];
                if(serName == "ApplicationMobilityService") {
                    std::string isLocal = jsonBody["isLocal"];
                    if (isLocal == "TRUE") {
                        localAMS = true;
                        if (jsonBody.contains("transportInfo")) {
                            AMSEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                        }
                    } else if (!localAMS) {
                        AMSEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                    }
                }
                else if (serName == "LocationService") {
                    std::string isLocal = jsonBody["isLocal"];
                    if (isLocal == "TRUE") {
                        localLS = true;
                        if (jsonBody.contains("transportInfo")) {
                            LSEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                        }
                    } else if (!localLS) {
                        LSEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                    }
                }
                else if (serName == "RNIService") {
                    std::string isLocal = jsonBody["isLocal"];
                    if (isLocal == "TRUE") {
                        localRNIS = true;
                        if (jsonBody.contains("transportInfo")) {
                            RNISEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                        }
                    } else if (!localRNIS) {
                        RNISEndPoint = jsonBody["transportInfo"]["endPoint"]["addresses"];
                    }
                }
            }   // end for

            if (AMSEndPoint != nullptr) {
                EV << "address: " << AMSEndPoint["host"] << " port: " << AMSEndPoint["port"] << endl;
                std::string address = AMSEndPoint["host"];
                mecServices[AMS]->serviceAddress_ = L3AddressResolver().resolve(address.c_str());
                mecServices[AMS]->servicePort_ = AMSEndPoint["port"];

                // connect to service
                cMessage *m = new cMessage("amsConnectMessage");
                scheduleAt(simTime()+0.005, m);
            }
            if (LSEndPoint != nullptr) {
                EV << "address: " << LSEndPoint["host"] << " port: " << LSEndPoint["port"] << endl;
                std::string address = LSEndPoint["host"];

                mecServices[LS] = new MecServiceSocketInfo;
                mecServices[LS]->serviceAddress_ = L3AddressResolver().resolve(address.c_str());
                mecServices[LS]->servicePort_ = LSEndPoint["port"];
                mecServices[LS]->serviceSocket_ = addNewSocket();
            }
            if (RNISEndPoint != nullptr) {
                EV << "address: " << RNISEndPoint["host"] << " port: " << RNISEndPoint["port"] << endl;
                std::string address = RNISEndPoint["host"];
                mecServices[RNIS] = new MecServiceSocketInfo;
                mecServices[RNIS]->serviceAddress_ = L3AddressResolver().resolve(address.c_str());
                mecServices[RNIS]->servicePort_ = RNISEndPoint["port"];
                mecServices[RNIS]->serviceSocket_ = addNewSocket();
                connect(mecServices[RNIS]->serviceSocket_, mecServices[RNIS]->serviceAddress_, mecServices[RNIS]->servicePort_);
            }
        }
    }
    catch (nlohmann::detail::parse_error e) {
        EV << e.what() << std::endl;
        // body is not correctly formatted in JSON, manage it
        return;
    }
}

void MecAppBase::handleAmsMessage(int connId) {
    // todo: implement management for REQUEST and RESPONSE type messages from AMS
    // registration/subscription response, event notifications, etc.
    EV << "MecAppBase::handleAmsMessage " << endl;
}

void MecAppBase::established(int connId) {
    if (mp1Socket_ != nullptr && connId == mp1Socket_->getSocketId()) {
        EV << "MECAppBase::established - Mp1Socket" << endl;
        // get endPoint of the required service
        std::string uri;
        if (isMobilityAware)
            uri = "/example/mec_service_mgmt/v1/services?ser_name=ApplicationMobilityService," + requiredSerName_;
        else
            uri = "/example/mec_service_mgmt/v1/services?ser_name=" + requiredSerName_;

        EV << "MECAppBase::established - URI: " << uri << endl;

        std::string host = mp1Socket_->getRemoteAddress().str() + ":" + std::to_string(mp1Socket_->getRemotePort());

        Http::sendGetRequest(mp1Socket_, host.c_str(), uri.c_str());
        return;
    }
    else if(mecServices[AMS] != nullptr && connId == mecServices[AMS]->serviceSocket_->getSocketId()) {
        EV << "MecAppBase::established - AMSSocket"<< endl;

        // Send registration

        return;
    } else {
        throw cRuntimeError("MecAppBase::socketEstablished - Socket %d not recognized", connId);
    }
}

double MecAppBase::scheduleNextMsg(cMessage *msg)
{
    double processingTime = vim->calculateProcessingTime(mecAppId, 20);
    return processingTime;
}

void MecAppBase::handleProcessedMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else {
        ISocket *sock = sockets_.findSocketFor(msg);
        if (sock != nullptr) {
            EV << "MecAppBase::handleProcessedMessage(): message for socket with ID: " << sock->getSocketId() << endl;
            sock->processMessage(msg);
        }
        else {
            delete msg;
        }
    }
}

void MecAppBase::socketEstablished(TcpSocket *socket)
{
    established(socket->getSocketId());
}

void MecAppBase::socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool)
{
    EV << "MecAppBase::socketDataArrived" << endl;

    std::vector<uint8_t> bytes = msg->peekDataAsBytes()->getBytes();
    std::string packet(bytes.begin(), bytes.end());
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(socket->getUserData());

    bool res = Http::parseReceivedMsg(socket->getSocketId(), packet, msgStatus->httpMessageQueue, msgStatus->bufferedData, msgStatus->currentMessage);
    if (res) {
        if (vim == nullptr)
            throw cRuntimeError("MecAppBase::socketDataArrived - vim is null!");
        double time = vim->calculateProcessingTime(mecAppId, 150);
        if (!msgStatus->processMsgTimer->isScheduled())
            scheduleAt(simTime() + time, msgStatus->processMsgTimer);
    }

    delete msg;
}

void MecAppBase::socketPeerClosed(TcpSocket *socket_)
{
    EV << "MecAppBase::socketPeerClosed" << endl;
    socket_->close();
}

void MecAppBase::socketClosed(TcpSocket *socket)
{
    EV_INFO << "MecAppBase::socketClosed" << endl;
}

void MecAppBase::socketFailure(TcpSocket *sock, int code)
{
    // subclasses may override this function, and add code try to reconnect after a delay.
}

TcpSocket *MecAppBase::addNewSocket()
{
    TcpSocket *newSocket = new TcpSocket();
    newSocket->setOutputGate(gate("socketOut"));
    newSocket->setCallback(this);
    HttpMessageStatus *msg = new HttpMessageStatus;
    msg->processMsgTimer = new ProcessingTimeMessage("processedHttpMsg");
    msg->processMsgTimer->setSocketId(newSocket->getSocketId());
    newSocket->setUserData(msg);

    sockets_.addSocket(newSocket);
    EV << "MecAppBase::addNewSocket(): added socket with ID: " << newSocket->getSocketId() << endl;
    return newSocket;
}

void MecAppBase::removeSocket(inet::TcpSocket *tcpSock)
{
    HttpMessageStatus *msgStatus = static_cast<HttpMessageStatus *>(tcpSock->getUserData());
    std::cout << "Deleting httpMessages in socket with sockId " << tcpSock->getSocketId() << std::endl;
    while (!msgStatus->httpMessageQueue.isEmpty()) {
        std::cout << "Deleting httpMessages message" << std::endl;
        delete msgStatus->httpMessageQueue.pop();
    }
    if (msgStatus->currentMessage != nullptr)
        delete msgStatus->currentMessage;
    if (msgStatus->processMsgTimer != nullptr) {
        cancelAndDelete(msgStatus->processMsgTimer);
    }
    delete sockets_.removeSocket(tcpSock);
}

void MecAppBase::finish()
{
    EV << "MecAppBase::finish()" << endl;
}

} //namespace

