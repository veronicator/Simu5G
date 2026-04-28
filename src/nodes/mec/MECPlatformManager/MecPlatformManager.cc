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

#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"
#include "nodes/mec/MECOrchestrator/MecOrchestrator.h"


namespace simu5g {

Define_Module(MecPlatformManager);


void MecPlatformManager::initialize(int stage)
{
    EV << "VirtualisationInfrastructureManager::initialize - stage " << stage << endl;
    cSimpleModule::initialize(stage);
    // avoid multiple initializations
    if (stage == inet::INITSTAGE_LOCAL) {
        vim.reference(this, "vimModule", true);
        serviceRegistry.reference(this, "serviceRegistryModule", false);

        mecOrchestrator.reference(this, "mecOrchestrator", false);
        if (!mecOrchestrator) {
            EV << "MecPlatformManager::initialize - MECOrchestrator [" << par("mecOrchestrator").str() << "] not found" << endl;
        }
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        mepmAddress_ = inet::L3AddressResolver().addressOf(getContainingNode(this));

        const char *localAddressStr = par("localAddress");
        inet::L3Address localAddress = *localAddressStr ? inet::L3AddressResolver().resolve(localAddressStr) : inet::L3Address();

        // setup socket with the MECOrchestrator
        meoSocket_.setOutputGate(gate("socketOut"));
        meoSocket_.bind(localAddress, par("meoLocalPort").intValue());
        meoSocket_.setCallback(this);
        const char *meoAddress = par("meoAddress").stringValue();
        meoAddress_ = inet::L3AddressResolver().resolve(meoAddress);
        meoDestPort_ = par("meoDestPort");

        int timeToLive = par("timeToLive");
        if (timeToLive != -1) {
            meoSocket_.setTimeToLive(timeToLive);
        }

        int dscp = par("dscp");
        if (dscp != -1) {
            meoSocket_.setDscp(dscp);
        }

        int tos = par("tos");
        if (tos != -1) {
            meoSocket_.setTos(tos);
        }
        if (meoAddress_.isUnspecified()) {
            EV_ERROR << "Connecting to " << meoAddress_ << " port=" << meoDestPort_ << ": cannot resolve destination address\n";
            throw cRuntimeError("MecOrchestrator proxy address is unspecified!");
        }
        else {
            EV << "Connecting to " << meoAddress_ << " port=" << meoDestPort_ << endl;
            meoSocket_.connect(meoAddress_, meoDestPort_);
        }

    }
}


void MecPlatformManager::handleMessage(cMessage *msg)
{
    if (!msg->isSelfMessage()) {
        if (meoSocket_.belongsToSocket(msg)) {
            meoSocket_.processMessage(msg);
        }
        else {
            EV_ERROR << "message " << msg->getFullName() << "(" << msg->getClassName() << ") arrived for unknown socket \n";
            delete msg;
        }
    }
}

void MecPlatformManager::socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool urgent) {
    // socket messages from MEO
    if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        inet::Packet *packet = check_and_cast<inet::Packet *>(msg);
        int connId = socket->getSocketId(); //packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);

        while (queue.has<MECAppMessage>(b(-1))) {
            auto meoMsg = queue.pop<MECAppMessage>(b(-1));
//            auto meMsg = msg->peekAtFront<MECAppMessage>();
            if (!strcmp(meoMsg->getType(), MIGRATE_MEAPP))
                migrateMEApp(new Packet("CreateAppMessage", meoMsg));
            /*  // todo gestire casi start / stop
             * else if (!strcmp(meoMsg->getType(), STOP_MEAPP))
                handleMigrateAppAck(msg);
            */
            else
                EV << "MecPlatformManager::socketDataArrived - Unexpected type data: " << meoMsg->getType() << endl;
        }
    }
}


//void MecPlatformManager::socketPeerClosed(inet::TcpSocket *socket)
//{
//    EV << "MecPlatformManager::socketPeerClosed" << endl;
//    if (meoSocket_.getState() == inet::TcpSocket::PEER_CLOSED) {
//        EV_INFO << "remote TCP closed, closing here as well\n";
//        meoSocket_.close();
//    }
//}

// instancing the requested MECApp (called by socketDataArrived)
void MecPlatformManager::migrateMEApp(cMessage *msg)
{
    Enter_Method_Silent("MecPlatformManager::migrateMEApp");

    EV << "MEPM::migrateMECApp" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto meoMsg = pkt->removeAtFront<CreateAppMessage>();
    int contextId = meoMsg->getContextId();

    MecAppInstanceInfo *res = vim->instantiateMEApp(meoMsg.get());

    if (res->status) {
        auto migrateAckMsg = inet::makeShared<MigrateAppAckMessage>();
        migrateAckMsg->setType(ACK_MIGRATE_MEAPP);
        migrateAckMsg->setContextId(contextId);
        migrateAckMsg->setStatus(res->status);
        migrateAckMsg->setInstanceId(res->instanceId.c_str());
        migrateAckMsg->setEndPointAddr(res->endPoint.addr.str().c_str());
        migrateAckMsg->setEndPointPort(res->endPoint.port);
        migrateAckMsg->setModuleId(res->reference->getId());

        migrateAckMsg->setMepmId(getId());
        migrateAckMsg->setMepmAddress(mepmAddress_.str().c_str());

        inet::B msgSize = inet::B(50 + strlen(migrateAckMsg->getType()) + strlen(migrateAckMsg->getMepmAddress())
                + strlen(migrateAckMsg->getInstanceId()) + strlen(migrateAckMsg->getEndPointAddr()));

        migrateAckMsg->setChunkLength(msgSize);
        inet::Packet *newPkt = new inet::Packet("MigrateAppAckMessage");
        newPkt->insertAtBack(migrateAckMsg);
        meoSocket_.send(newPkt);
    }
    delete msg;
}


// instancing the requested MECApp (called by handleResource)
MecAppInstanceInfo *MecPlatformManager::instantiateMEApp(CreateAppMessage *msg)
{
    MecAppInstanceInfo *res = vim->instantiateMEApp(msg);
    delete msg;
    return res;
}

bool MecPlatformManager::instantiateEmulatedMEApp(CreateAppMessage *msg)
{
    bool res = vim->instantiateEmulatedMEApp(msg);
    delete msg;
    return res;
}

bool MecPlatformManager::terminateEmulatedMEApp(DeleteAppMessage *msg)
{
    bool res = vim->terminateEmulatedMEApp(msg);
    delete msg;
    return res;
}

// terminating the corresponding MECApp (called by handleResource)
bool MecPlatformManager::terminateMEApp(DeleteAppMessage *msg)
{
    bool res = vim->terminateMEApp(msg);
    delete msg;
    return res;
}

void MecPlatformManager::migrateMEAppsReq()
{
    Enter_Method_Silent("MecPlatformManager::migrateMEAppsReq");

    EV << "MEPM::migrateMEAppsReq" << endl;

    inet::Packet *newPkt = new inet::Packet("MigrateAppMessage");
    auto migrateMsgReq = inet::makeShared<MigrateAppMessage>();
    migrateMsgReq->setType(MIGRATE_MEAPP_REQ);
    migrateMsgReq->setMepmAddress(mepmAddress_.str().c_str());
    migrateMsgReq->setMepmId(getId());
    inet::B msgSize = inet::B(40 + strlen(migrateMsgReq->getType()) + strlen(migrateMsgReq->getMepmAddress()));
    migrateMsgReq->setChunkLength(msgSize);

    newPkt->insertAtBack(migrateMsgReq);
    meoSocket_.send(newPkt);

//    if (mecOrchestrator != nullptr) {
//        mecOrchestrator->migrateMECApps(this->getId());
////        vim->terminateAllMEApps();
//    }
}

const std::vector<ServiceInfo> *MecPlatformManager::getAvailableMecServices() const
{
    if (serviceRegistry == nullptr)
        return nullptr;
    else {
        return serviceRegistry->getAvailableMecServices();
    }
}

void MecPlatformManager::registerMecService(ServiceDescriptor& serviceDescriptor) const
{
    if (mecOrchestrator != nullptr)
        mecOrchestrator->registerMecService(serviceDescriptor);
}

} //namespace

