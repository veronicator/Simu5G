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

#include "nodes/mec/MECPlatform/MECServices/HostMobilityService/HostMobilityService.h"
#include "nodes/mec/MECPlatform/MECServices/ApplicationMobilityService/ApplicationMobilityService.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/RNIService.h"
#include "nodes/mec/MECPlatform/MECServices/LocationService/LocationService.h"
#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"
#include "nodes/mec/MECOrchestrator/MecOrchestrator.h"


namespace simu5g {

Define_Module(MecPlatformManager);


void MecPlatformManager::initialize(int stage)
{
    EV << "MecPlatformManager::initialize - stage " << stage << endl;
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

        while (queue.has<MECMessage>(b(-1))) {
            auto meoMsg = queue.pop<MECMessage>(b(-1));
//            auto meMsg = msg->peekAtFront<MECMessage>();
            if (!strcmp(meoMsg->getType(), MIGRATE_MEAPP))
                instantiateMigratingMecApp(new Packet("CreateAppMessage", meoMsg));
             else if (!strcmp(meoMsg->getType(), STOP_MIGRATED_MEAPP))
                stopMigratedMecApp(new Packet("DeleteAppMessage", meoMsg));
            /*  // todo gestire gli altri casi
            */
             else if (!strcmp(meoMsg->getType(), ACK_UPDATE_SERVING_AREA))
                 manageUpdateServingAreaResponse(new Packet("UpdateServingAreaResponse", meoMsg));
            else
                EV << "MecPlatformManager::socketDataArrived - Unexpected type data: " << meoMsg->getType() << endl;
        }
    }
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

void MecPlatformManager::triggerMecAppMigration(AssociateId associateId, std::vector<std::string> appInstanceIds, MacNodeId srcEcgi, MacNodeId trgEcgi) {

    Enter_Method_Silent("MecPlatformManager::triggerMecAppMigration");

    EV << "MecPlatformManager::triggerMecAppMigration" << endl;

    inet::Packet *newPkt = new inet::Packet("TriggerMigrateAppMessage");
    auto migrateMsg = inet::makeShared<TriggerMigrationAppMessage>();
    migrateMsg->setType(MIGRATE_MEAPP);
    migrateMsg->setAssociateId(associateId);
    migrateMsg->setAppInstanceIds(appInstanceIds);
    migrateMsg->setSrcCellId(srcEcgi);
    migrateMsg->setTrgCellId(trgEcgi);
    inet::B msgSize = inet::B(40 + strlen(migrateMsg->getType()) + strlen(associateId.getType().c_str()) + strlen(associateId.getValue().c_str())
            + sizeof(appInstanceIds));
    migrateMsg->setChunkLength(msgSize);

    newPkt->insertAtBack(migrateMsg);
    meoSocket_.send(newPkt);
}

void MecPlatformManager::triggerMecAppsMigration(std::vector<std::string> appInstanceIds, MacNodeId trgEcgi) {

    Enter_Method_Silent("MecPlatformManager::triggerMecAppsMigration");

    EV << "MecPlatformManager::triggerMecAppsMigration" << endl;

    inet::Packet *newPkt = new inet::Packet("TriggerMigrateAppMessage");
    auto migrateMsg = inet::makeShared<TriggerMigrationAppMessage>();
    migrateMsg->setType(MIGRATE_MEAPPS);
    migrateMsg->setAppInstanceIds(appInstanceIds);
    migrateMsg->setTrgCellId(trgEcgi);
    inet::B msgSize = inet::B(40 + strlen(migrateMsg->getType()) + sizeof(appInstanceIds));
    migrateMsg->setChunkLength(msgSize);

    newPkt->insertAtBack(migrateMsg);
    meoSocket_.send(newPkt);
}

/*
 * request migration of MecApps to another MecHost after source MEC host handover
 */
void MecPlatformManager::migrateMecAppsReq()
{
    Enter_Method_Silent("MecPlatformManager::migrateMEAppsReq");

    EV << "MecPlatformManager::migrateMEAppsReq" << endl;

    inet::Packet *newPkt = new inet::Packet("MigrateAppMessage");
    auto migrateMsgReq = inet::makeShared<MigrateAppMessage>();
    migrateMsgReq->setType(MIGRATE_MEAPPS_REQ);
    migrateMsgReq->setMepmAddress(mepmAddress_.str().c_str());
    migrateMsgReq->setMepmId(getId());
    inet::B msgSize = inet::B(40 + strlen(migrateMsgReq->getType()) + strlen(migrateMsgReq->getMepmAddress()));
    migrateMsgReq->setChunkLength(msgSize);

    newPkt->insertAtBack(migrateMsgReq);
    meoSocket_.send(newPkt);

}

// instancing the requested MECApp (called by socketDataArrived)
void MecPlatformManager::instantiateMigratingMecApp(cMessage *msg)
{
    Enter_Method_Silent("MecPlatformManager::migrateMEApp");

    EV << "MecPlatformManager::migrateMECApp" << endl;

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
        migrateAckMsg->setModuleId(res->reference->getId());
        migrateAckMsg->setEndPoint(res->endPoint);

        migrateAckMsg->setMepmId(getId());
        migrateAckMsg->setMepmAddress(mepmAddress_.str().c_str());

        inet::B msgSize = inet::B(50 + strlen(migrateAckMsg->getType()) + strlen(migrateAckMsg->getMepmAddress())
                + strlen(migrateAckMsg->getInstanceId()) + strlen(migrateAckMsg->getEndPoint().addr.str().c_str()));

        migrateAckMsg->setChunkLength(msgSize);
        inet::Packet *newPkt = new inet::Packet("MigrateAppAckMessage");
        newPkt->insertAtBack(migrateAckMsg);
        meoSocket_.send(newPkt);
    }
    else {
        // todo: handle error case
    }
    delete msg;
}

void MecPlatformManager::stopMigratedMecApp (cMessage *msg) {

    Enter_Method_Silent("MecPlatformManager::stopMecApp");

    EV << "MecPlatformManager::stopMigratedMecApp" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto meoMsg = pkt->removeAtFront<DeleteAppMessage>();

    vim->terminateMEApp(meoMsg.get());
    // send an ack to MEO? send a msg to AMS to update/delete old app registrationInfo?

    delete msg;
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

void MecPlatformManager::updateServigArea(std::string mecHostName, MacNodeId srcCell, MacNodeId trgCell) {
    Enter_Method_Silent("MecPlatformManager::updateServigArea");

    EV << "MecPlatformManager::updateServigArea" << endl;

    auto updateMsg = inet::makeShared<UpdateServingAreaMessage>();
    updateMsg->setType(UPDATE_SERVING_AREA);
    updateMsg->setMecHostName(mecHostName.c_str());
    updateMsg->setMepmAddress(mepmAddress_);
    updateMsg->setSrcCellId(srcCell);
    updateMsg->setTrgCellId(trgCell);
    inet::B msgSize = inet::B(40 + strlen(updateMsg->getType()) + strlen(mecHostName.c_str())
            + strlen(mepmAddress_.str().c_str()));
    updateMsg->setChunkLength(msgSize);

    inet::Packet *newPkt = new inet::Packet("UpdateServingAreaMessage");
    newPkt->insertAtBack(updateMsg);
    meoSocket_.send(newPkt);
}

void MecPlatformManager::registerMecServiceReference(MecServiceBase *mecService, std::string mecServiceName)
{
    EV << "MecPlatformManager::registerMecServiceReference" << endl;

    if (mecServiceName.compare("HostMobilityService") == 0)
        hms = check_and_cast<HostMobilityService *>(mecService);

    else if (mecServiceName.compare("ApplicationMobilityService") == 0)
        ams = check_and_cast<ApplicationMobilityService *>(mecService);

    else if (mecServiceName.compare("RNIService") == 0)
        rnis = check_and_cast<RNIService *>(mecService);

    else if (mecServiceName.compare("LocationService") == 0)
        ls = check_and_cast<LocationService *>(mecService);
}

void MecPlatformManager::manageUpdateServingAreaResponse(cMessage *msg) {
    Enter_Method_Silent("MecPlatformManager::manageUpdateResponse");

    EV << "MecPlatformManager::manageUpdateResponse" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto responseMsg = pkt->removeAtFront<UpdateServingAreaResponse>();

    EV << "MecPlatformManager::manageUpdateServingAreaResponse -> "  << responseMsg->getResponse() << endl;

    if(responseMsg->getStatus()) {
        // update serving area ok
        // notify the correct update to HMS
        if (hms != nullptr)
            hms->handleHostMobilityUpdate(responseMsg->getSrcCellId(), responseMsg->getTrgCellId());
    }
    // if status == false -> nothing happens
}

} //namespace

