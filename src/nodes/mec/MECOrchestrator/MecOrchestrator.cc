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

#include "nodes/mec/MECOrchestrator/MecOrchestrator.h"

//#include "nodes/mec/MobileMECHost.h"

#include "common/cellInfo/CellInfo.h"

#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"
#include "nodes/mec/VirtualisationInfrastructureManager/VirtualisationInfrastructureManager.h"

#include "nodes/mec/MECPlatform/ServiceRegistry/ServiceRegistry.h"
#include "apps/mec/MecApps/MultiUEMECApp.h"

#include "nodes/mec/UALCMP/UALCMPMessages/UALCMPMessages_m.h"
#include "nodes/mec/UALCMP/UALCMPMessages/UALCMPMessages_types.h"
#include "nodes/mec/UALCMP/UALCMPMessages/CreateContextAppMessage.h"
#include "nodes/mec/UALCMP/UALCMPMessages/CreateContextAppAckMessage.h"

#include "nodes/mec/MECOrchestrator/mecHostSelectionPolicies/MecServiceSelectionBased.h"
#include "nodes/mec/MECOrchestrator/mecHostSelectionPolicies/AvailableResourcesSelectionBased.h"
#include "nodes/mec/MECOrchestrator/mecHostSelectionPolicies/MecHostSelectionBased.h"
#include "nodes/mec/MECOrchestrator/mecHostSelectionPolicies/MigrationMecServiceSelectionBased.h"

// Emulation debug
#include <algorithm>
#include <iostream>

namespace simu5g {

Define_Module(MecOrchestrator);


void MecOrchestrator::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    // Avoid multiple initializations
    if (stage == inet::INITSTAGE_LOCAL) {
//        return;
        EV << "MecOrchestrator::initialize - stage " << stage << endl;

        binder_.reference(this, "binderModule", true);

        const char *selectionPolicyPar = par("selectionPolicy");
        if (!strcmp(selectionPolicyPar, "MecServiceBased"))
            mecHostSelectionPolicy_ = new MecServiceSelectionBased(this);
        else if (!strcmp(selectionPolicyPar, "AvailableResourcesBased"))
            mecHostSelectionPolicy_ = new AvailableResourcesSelectionBased(this);
        else if (!strcmp(selectionPolicyPar, "MecHostBased"))
            mecHostSelectionPolicy_ = new MecHostSelectionBased(this, par("mecHostIndex"));
        else if (!strcmp(selectionPolicyPar, "MigrationSelectionBased"))
            mecHostSelectionPolicy_ = new MigrationMecServiceSelectionBased(this);
        else
            throw cRuntimeError("MecOrchestrator::initialize - Selection policy '%s' not present!", selectionPolicyPar);

        onboardingTime = par("onboardingTime").doubleValue();
        instantiationTime = par("instantiationTime").doubleValue();
        terminationTime = par("terminationTime").doubleValue();
        removalWaitingTime = par("removalWaitingTime").doubleValue();

        getConnectedMecHosts();
        onboardApplicationPackages();
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        EV << "MecOrchestrator::initialize - stage " << stage << endl;

        getCellMecHostsConnections();

        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        EV << "Local Address: " << localAddress << " port: " << localPort << endl;
        inet::L3Address localAdd(inet::L3AddressResolver().resolve(localAddress));
        EV << "Local Address resolved: " << localAdd << endl;

        serverSocket.setOutputGate(gate("socketOut"));
        serverSocket.setCallback(this);
        serverSocket.bind(inet::L3Address(), localPort); // bind socket for any address

        int tos = par("tos");
        if (tos != -1)
            serverSocket.setTos(tos);

        serverSocket.listen();
    }
}

void MecOrchestrator::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (strcmp(msg->getName(), "DeferredMigrationRequest") == 0) {
            inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
            auto mepmMsg = pkt->peekAtFront<MECMessage>();
            if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPP))
                migrateMecApp(pkt);
            else if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPPS))
                migrateMecApps(pkt);
            else if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPPS_REQ))
                migrateAllMecApps(pkt);
            else
                throw cRuntimeError("MecOrchestrator::handleMessage - unexpected deferred migration type '%s'",
                        mepmMsg->getType());
        }
        else if (strcmp(msg->getName(), "MECOrchestratorMessage") == 0) {
            EV << "MecOrchestrator::handleMessage - " << msg->getName() << endl;
            MECOrchestratorMessage *meoMsg = check_and_cast<MECOrchestratorMessage *>(msg);
            if (strcmp(meoMsg->getType(), CREATE_CONTEXT_APP) == 0) {
                if (meoMsg->getSuccess())
                    sendCreateAppContextAck(true, meoMsg->getRequestId(), meoMsg->getContextId());
                else
                    sendCreateAppContextAck(false, meoMsg->getRequestId());
            }

            else if (strcmp(meoMsg->getType(), DELETE_CONTEXT_APP) == 0)
                sendDeleteAppContextAck(meoMsg->getSuccess(), meoMsg->getRequestId(), meoMsg->getContextId());

            else if (strcmp(meoMsg->getType(), MIGRATE_CONTEXT_APP) == 0)
                sendMigrateAppContext(meoMsg->getSuccess(), meoMsg->getRequestId(),
                        meoMsg->getContextId(), meoMsg->getMigrationId());

            else if (strcmp(meoMsg->getType(), STOP_MIGRATED_INSTANCE_APP) == 0) {
                stopMigratedMecApp(meoMsg->getSuccess(), meoMsg->getRequestId(),
                        meoMsg->getMigrationId());
            }
        }
    }
    // Handle message from the LCM proxy
    else if (msg->arrivedOn("fromUALCMP")) {
        EV << "MecOrchestrator::handleMessage - " << msg->getName() << endl;
        handleUALCMPMessage(msg);
    }
    else {
        EV << "MecOrchestrator::handleMessage from socket" << endl;
        inet::TcpSocket *socket = check_and_cast_nullable<inet::TcpSocket *>(sockets_.findSocketFor(msg));
        if (socket)
            socket->processMessage(msg);
        else if (serverSocket.belongsToSocket(msg))
            serverSocket.processMessage(msg);
        else {
            EV_ERROR << "message " << msg->getFullName() << "(" << msg->getClassName() << ") arrived for unknown socket \n";
            delete msg;
        }
    }
}

void MecOrchestrator::deferMigrationRequest(cMessage *msg)
{
    if (strcmp(msg->getName(), "DeferredMigrationRequest") != 0) {
        EV << "MecOrchestrator::deferMigrationRequest - delaying migration request until the in-flight migration completes"
           << endl;
        msg->setName("DeferredMigrationRequest");
    }
    scheduleAt(simTime() + 0.01, msg);
}

void MecOrchestrator::socketAvailable(inet::TcpSocket *socket, inet::TcpAvailableInfo *availableInfo)
{
    // new TCP connection -- create new socket object
    inet::TcpSocket *newSocket = new inet::TcpSocket(availableInfo);
    newSocket->setOutputGate(gate("socketOut"));

    newSocket->setCallback(this);

    sockets_.addSocket(newSocket);
    EV << "New socket added - [" << newSocket->getRemoteAddress() << ":" << newSocket->getRemotePort() << " connId: " << newSocket->getSocketId() << "]" << endl;

    socket->accept(availableInfo->getNewSocketId());
}

void MecOrchestrator::socketEstablished(inet::TcpSocket *socket) {
    inet::L3Address remoteAddress = socket->getRemoteAddress();     // get the remoteAddress used as key in the  map mepmSockets_
    mepmSockets_[remoteAddress] = socket->getSocketId();
}

void MecOrchestrator::socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool urgent) {
    // todo: gestire i diversi tipi di messaggi Create | Delete | Migrate + ack msg
    EV << "MecOrchestrator::socketDataArrived - received packet" << endl;

    if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        inet::Packet *packet = check_and_cast<inet::Packet *>(msg);
        int connId = socket->getSocketId(); //packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);

        while (queue.has<MECMessage>(b(-1))) {
            auto mepmMsg = queue.pop<MECMessage>(b(-1));
            // from mepm -> migrateMsg
            if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPPS_REQ)) {
                migrateAllMecApps(new Packet("MigrateAppMessage", mepmMsg));
            }

            else if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPP))
                migrateMecApp(new Packet("TriggerMigrateAppMessage", mepmMsg));

            else if (!strcmp(mepmMsg->getType(), MIGRATE_MEAPPS))
                migrateMecApps(new Packet("TriggerMigrateAppMessage", mepmMsg));

            else if (!strcmp(mepmMsg->getType(), ACK_MIGRATE_MEAPP))
                handleMigrateAppAck(new Packet("MigrateAppAckMessage", mepmMsg));

            else if (!strcmp(mepmMsg->getType(), UPDATE_SERVING_AREA))
                updateMecHostServingArea(new Packet("UpdateServingAreaMessage", mepmMsg));
            else
                EV << "MecOrchestrator::socketDataArrived - Unexpected type data: " << mepmMsg->getType() << endl;
        }
    }
}

void MecOrchestrator::handleUALCMPMessage(cMessage *msg)
{
    UALCMPMessage *lcmMsg = check_and_cast<UALCMPMessage *>(msg);

    // Handling CREATE_CONTEXT_APP
    if (!strcmp(lcmMsg->getType(), CREATE_CONTEXT_APP))
        startMECApp(lcmMsg);

    // Handling DELETE_CONTEXT_APP
    else if (!strcmp(lcmMsg->getType(), DELETE_CONTEXT_APP))
        stopMECApp(lcmMsg);
}

void MecOrchestrator::startMECApp(UALCMPMessage *msg)
{
    CreateContextAppMessage *contAppMsg = check_and_cast<CreateContextAppMessage *>(msg);

    EV << "MecOrchestrator::createMeApp - processing... request id: " << contAppMsg->getRequestId() << endl;

    // Retrieve UE App ID
    int ueAppID = atoi(contAppMsg->getDevAppId());

    /*
     * The MEC orchestrator has to decide where to deploy the MEC application.
     * - It checks if the MEC app has already been deployed
     * - It selects the most suitable MEC host
     */

    for (const auto& contextApp : meAppMap)
    {
        if (contextApp.second.lifecycle == MecAppLifecycle::ACTIVE &&
                contextApp.second.mecUeAppID == ueAppID &&
                contextApp.second.appDId.compare(contAppMsg->getAppDId()) == 0)
        {
            EV << "MecOrchestrator::startMECApp - \tWARNING: required MEC App instance ALREADY STARTED on MEC host: " << contextApp.second.mecHost->getName() << endl;
            EV << "MecOrchestrator::startMECApp  - sending ackMEAppPacket with " << ACK_CREATE_CONTEXT_APP << endl;
            sendCreateAppContextAck(true, contAppMsg->getRequestId(), contextApp.first);
            auto* existingMECApp = dynamic_cast<MultiUEMECApp*>(contextApp.second.reference);
            if (existingMECApp) {
                // if the app already exist and it is an app supporting multiple UEs, then notify the app about the new UE
                struct UE_MEC_CLIENT newUE;
                newUE.address = inet::L3Address(contAppMsg->getUeIpAddress());
                // the UE port is not known at this stage
                newUE.port = -1;
                existingMECApp->addNewUE(newUE);
            }
            else
                return;
        }
    }

    std::string appDid;
    double processingTime = 0.0;

    if (contAppMsg->getOnboarded() == false) {
        // Onboard app descriptor
        EV << "MecOrchestrator::startMECApp - onboarding appDescriptor from: " << contAppMsg->getAppPackagePath() << endl;
        const ApplicationDescriptor& appDesc = onboardApplicationPackage(contAppMsg->getAppPackagePath());
        appDid = appDesc.getAppDId();
        processingTime += onboardingTime;
    }
    else {
        appDid = contAppMsg->getAppDId();
    }

    auto it = mecApplicationDescriptors_.find(appDid);
    if (it == mecApplicationDescriptors_.end()) {
        EV << "MecOrchestrator::startMECApp - Application package with AppDId[" << contAppMsg->getAppDId() << "] not onboarded." << endl;
        sendCreateAppContextAck(false, contAppMsg->getRequestId());
    }

    const ApplicationDescriptor& desc = it->second;

    cModule *bestHost = mecHostSelectionPolicy_->findBestMecHost(desc);

    if (bestHost != nullptr) {
        CreateAppMessage *createAppMsg = new CreateAppMessage();
        createAppMsg->setMigrationId(-1);

        createAppMsg->setUeAppID(atoi(contAppMsg->getDevAppId()));
        createAppMsg->setMEModuleName(desc.getAppName().c_str());
        createAppMsg->setMEModuleType(desc.getAppProvider().c_str());

        createAppMsg->setRequiredCpu(desc.getVirtualResources().cpu);
        createAppMsg->setRequiredRam(desc.getVirtualResources().ram);
        createAppMsg->setRequiredDisk(desc.getVirtualResources().disk);

        // This field is useful for MEC services not ETSI MEC compliant (e.g. OMNeT++ like)
        // In such cases, the VIM must connect the gates between the MEC application and the service

        // Insert OMNeT like services, only one is supported for now
        if (!desc.getOmnetppServiceRequired().empty())
            createAppMsg->setRequiredService(desc.getOmnetppServiceRequired().c_str());
        else
            createAppMsg->setRequiredService("NULL");

        int contextId = contextIdCounter++;
        createAppMsg->setContextId(contextId);

        // Add the new MEC app in the map structure
        mecAppMapEntry newMecApp;
        newMecApp.appDId = appDid;
        newMecApp.mecUeAppID = ueAppID;
        newMecApp.mecHost = bestHost;
        newMecApp.ueAddress = inet::L3AddressResolver().resolve(contAppMsg->getUeIpAddress());
        newMecApp.vim = bestHost->getSubmodule("vim");
        newMecApp.mecpm = bestHost->getSubmodule("mecPlatformManager");

        newMecApp.mecAppName = desc.getAppName().c_str();
        MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(newMecApp.mecpm);

        /*
         * If the application descriptor refers to a simulated MEC app, the system eventually instantiates the MEC app object.
         * If the application descriptor refers to a MEC application running outside the simulator, i.e., emulation mode,
         * the system allocates the resources without instantiating any module.
         * The application descriptor contains the address and port information to communicate with the MEC application.
         */

        MecAppInstanceInfo *appInfo = nullptr;

        if (desc.isMecAppEmulated()) {
            EV << "MecOrchestrator::startMECApp - MEC app is emulated" << endl;
            bool result = mecpm->instantiateEmulatedMEApp(createAppMsg);
            appInfo = new MecAppInstanceInfo();
            appInfo->status = result;
            appInfo->endPoint.addr = inet::L3Address(desc.getExternalAddress().c_str());
            appInfo->endPoint.port = desc.getExternalPort();
            appInfo->instanceId = "emulated_" + desc.getAppName();
            newMecApp.isEmulated = true;

            bool isMobile = false;
            if (newMecApp.mecHost->hasPar("isMobile"))
                isMobile = newMecApp.mecHost->par("isMobile").boolValue();

            if (!isMobile) {
                // Register the address of the MEC app to the Binder, so the GTP knows the endpoint (iUpf) where to forward packets to
                const char* upfModule = newMecApp.mecHost->getSubmodule("vim")->par("upfModule").stringValue();
                inet::L3Address gtpAddress = inet::L3AddressResolver().resolve(getSimulation()->getModuleByPath(upfModule)->getFullPath().c_str());
                binder_->registerMecHostUpfAddress(appInfo->endPoint.addr, gtpAddress);
            }
        }
        else {
            appInfo = mecpm->instantiateMEApp(createAppMsg);
            newMecApp.isEmulated = false;
        }

        if (!appInfo->status) {
            EV << "MecOrchestrator::startMECApp - something went wrong during MEC app instantiation" << endl;
            MECOrchestratorMessage *msg = new MECOrchestratorMessage("MECOrchestratorMessage");
            msg->setType(CREATE_CONTEXT_APP);
            msg->setRequestId(contAppMsg->getRequestId());
            msg->setSuccess(false);
            processingTime += instantiationTime;
            scheduleAt(simTime() + processingTime, msg);
            return;
        }

        EV << "MecOrchestrator::startMECApp - new MEC application with name: " << appInfo->instanceId << " instantiated on MEC host []" << newMecApp.mecHost << " at " << appInfo->endPoint.addr.str() << ":" << appInfo->endPoint.port << endl;

        newMecApp.mecAppAddress = appInfo->endPoint.addr;
        newMecApp.mecAppPort = appInfo->endPoint.port;
        newMecApp.mecAppInstanceId = appInfo->instanceId;
        newMecApp.contextId = contextId;
        newMecApp.reference = appInfo->reference;
        meAppMap[contextId] = newMecApp;

        MECOrchestratorMessage *msg = new MECOrchestratorMessage("MECOrchestratorMessage");
        msg->setContextId(contextId);
        msg->setType(CREATE_CONTEXT_APP);
        msg->setRequestId(contAppMsg->getRequestId());
        msg->setSuccess(true);

        processingTime += instantiationTime;
        scheduleAt(simTime() + processingTime, msg);

        delete appInfo;
    }
    else {
        // throw cRuntimeError("MecOrchestrator::startMECApp - A suitable MEC host has not been selected");
        EV << "MecOrchestrator::startMECApp - A suitable MEC host has not been selected" << endl;
        MECOrchestratorMessage *msg = new MECOrchestratorMessage("MECOrchestratorMessage");
        msg->setType(CREATE_CONTEXT_APP);
        msg->setRequestId(contAppMsg->getRequestId());
        msg->setSuccess(false);
        processingTime += instantiationTime / 2;
        scheduleAt(simTime() + processingTime, msg);
    }
}

void MecOrchestrator::stopMECApp(UALCMPMessage *msg) {
    EV << "MecOrchestrator::stopMECApp - processing..." << endl;

    DeleteContextAppMessage *contAppMsg = check_and_cast<DeleteContextAppMessage *>(msg);

    int contextId = contAppMsg->getContextId();
    EV << "MecOrchestrator::stopMECApp - processing contextId: " << contextId << endl;
    // Checking if ueAppIdToMeAppMapKey entry map does exist
    auto activeAppIt = meAppMap.find(contextId);
    if (activeAppIt == meAppMap.end()) {
        // Maybe it has already been deleted
        EV << "MecOrchestrator::stopMECApp - \tWARNING MEC Application context ["
           << contextId << "] not found!" << endl;
        sendDeleteAppContextAck(false, contAppMsg->getRequestId(), contextId);
        return;
    }
    if (activeAppIt->second.lifecycle != MecAppLifecycle::ACTIVE) {
        EV_ERROR << "MecOrchestrator::stopMECApp - context " << contextId
                 << " does not refer to an active MEC app" << endl;
        sendDeleteAppContextAck(false, contAppMsg->getRequestId(), contextId);
        return;
    }
    if (tmpMeAppMap.find(contextId) != tmpMeAppMap.end()) {
        EV_WARN << "MecOrchestrator::stopMECApp - migration is in progress for context "
                << contextId << "; retry deletion after migration completes" << endl;
        sendDeleteAppContextAck(false, contAppMsg->getRequestId(), contextId);
        return;
    }

    // Call the methods of resource manager and virtualization infrastructure of the selected MEC host to deallocate the resources

    MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(meAppMap[contextId].mecpm);
    DeleteAppMessage *deleteAppMsg = new DeleteAppMessage();
    deleteAppMsg->setUeAppID(meAppMap[contextId].mecUeAppID);
    deleteAppMsg->setMigrationId(meAppMap[contextId].migrationId);

    bool isTerminated;
    if (meAppMap[contextId].isEmulated) {
        isTerminated = mecpm->terminateEmulatedMEApp(deleteAppMsg);
        std::cout << "terminateEmulatedMEApp with result: " << isTerminated << std::endl;
    }
    else {
        isTerminated = mecpm->terminateMEApp(deleteAppMsg);
    }

    MECOrchestratorMessage *mecoMsg = new MECOrchestratorMessage("MECOrchestratorMessage");
    mecoMsg->setType(DELETE_CONTEXT_APP);
    mecoMsg->setRequestId(contAppMsg->getRequestId());
    mecoMsg->setContextId(contAppMsg->getContextId());
    if (isTerminated) {
        EV << "MecOrchestrator::stopMECApp - MEC Application [" << meAppMap[contextId].mecUeAppID << "] removed" << endl;
        meAppMap.erase(contextId);
        mecoMsg->setSuccess(true);
    }
    else {
        EV << "MecOrchestrator::stopMECApp - MEC Application [" << meAppMap[contextId].mecUeAppID << "] not removed" << endl;
        mecoMsg->setSuccess(false);
    }

    double processingTime = terminationTime;
    scheduleAt(simTime() + processingTime, mecoMsg);
}


void MecOrchestrator::stopMigratedMecApp(bool result, int ueAppId, int migrationId) {

    if (!result) {
        EV << "MecOrchestrator::stopMigratedMecApp - not success - this should not happen" << endl;
        return;
    }

    auto oldAppIt = pendingDeleteMeApps.find(migrationId);
    if (oldAppIt == pendingDeleteMeApps.end()) {
        EV << "MecOrchestrator::stopMigratedMecApp - no pending source instance for migrationId="
           << migrationId << endl;
        return;
    }
    const mecAppMapEntry oldMecApp = oldAppIt->second;
    EV << "MecOrchestrator::stopMigratedMecApp - stopping source instance for contextId="
       << oldMecApp.contextId << " migrationId=" << migrationId << " on MEC host "
       << (oldMecApp.mecHost != nullptr ? oldMecApp.mecHost->getFullPath() : "<null>")
       << " ueAppId=" << ueAppId << endl;
    if (oldMecApp.lifecycle != MecAppLifecycle::SOURCE_PENDING_DELETE) {
        throw cRuntimeError("MecOrchestrator::stopMigratedMecApp - migrationId %d is not pending deletion", migrationId);
    }

    // Send a message to MEPM of the selected (source) MEC host to deallocate the resources (source-mepm)

    auto deleteAppMsg = inet::makeShared<DeleteAppMessage>();
    deleteAppMsg->setType(STOP_MIGRATED_MEAPP);
    deleteAppMsg->setUeAppID(oldMecApp.mecUeAppID);
    deleteAppMsg->setMigrationId(oldMecApp.migrationId);

    inet::B msgSize = inet::B(84 + strlen(deleteAppMsg->getType()) + strlen(deleteAppMsg->getSourceAddress())
            + strlen(deleteAppMsg->getDestinationAddress()) + strlen(deleteAppMsg->getDestinationMecAppAddress()) + strlen(deleteAppMsg->getMEModuleType())
            + strlen(deleteAppMsg->getMEModuleName()) + strlen(deleteAppMsg->getRequiredService()));
    deleteAppMsg->setChunkLength(msgSize);

    // create pkt to send through socket to the mepm on the new mec host
    inet::Packet *newPkt = new inet::Packet("DeleteAppMessage");
    newPkt->insertAtBack(deleteAppMsg);

    MecPlatformManager *oldMecpm = check_and_cast<MecPlatformManager *>(oldMecApp.mecpm);
    inet::L3Address mepmAddress = oldMecpm->getMepmAddress();
    int sockId = mepmSockets_[mepmAddress];
    inet::TcpSocket *socket = check_and_cast<inet::TcpSocket *>(sockets_.getSocketById(sockId));

    socket->send(newPkt);
    pendingDeleteMeApps.erase(oldAppIt);
}

void MecOrchestrator::migrateMecApp(cMessage *msg) {
    Enter_Method_Silent("MecOrchestrator::migrateMecApp");

    EV << "MecOrchestrator::migrateMecApp" << endl;
    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto mepmMsg = pkt->peekAtFront<TriggerMigrationAppMessage>();
    AssociateId associateId = mepmMsg->getAssociateId();
    // note: only "UE_IPv4_ADDRESS" type is supported for now
    inet::L3Address ueAddress = inet::L3Address(associateId.getValue().c_str());
    std::vector<std::string> appInstanceIds = mepmMsg->getAppInstanceIds();
    const auto contextIds = mepmMsg->getContextIds();
    if (!contextIds.empty()) {
        appInstanceIds.clear();
        for (int contextId : contextIds) {
            auto activeIt = meAppMap.find(contextId);
            if (activeIt != meAppMap.end() &&
                    activeIt->second.lifecycle == MecAppLifecycle::ACTIVE)
                appInstanceIds.push_back(activeIt->second.mecAppInstanceId);
        }
    }
    MacNodeId srcCellId = mepmMsg->getSrcCellId();
    MacNodeId trgCellId = mepmMsg->getTrgCellId();

    EV << "MecOrchestrator::migrateMecApp - UE handover: associateId="
       << associateId.getType() << ":" << associateId.getValue()
       << " srcCellId=" << srcCellId << " trgCellId=" << trgCellId
       << " contextIds=" << contextIds.size()
       << " resolvedActiveInstances=" << appInstanceIds.size() << endl;
    for (const auto& appInstanceId : appInstanceIds)
        EV << "MecOrchestrator::migrateMecApp - requested appInstanceId="
           << appInstanceId << endl;

    auto targetHostsIt = cellToMecHosts.find(trgCellId);
    if (targetHostsIt == cellToMecHosts.end()) {
        EV << "MecOrchestrator::migrateMecApp - no MEC hosts are mapped to target cell "
           << trgCellId << endl;
    }
    else {
        for (auto mecHost : targetHostsIt->second)
            EV << "MecOrchestrator::migrateMecApp - target cell " << trgCellId
               << " is served by MEC host "
               << (mecHost != nullptr ? mecHost->getFullPath() : "<null>") << endl;
    }

    for (const auto& contextApp : meAppMap) {
        const auto& activeApp = contextApp.second;
        if (activeApp.lifecycle == MecAppLifecycle::ACTIVE &&
                activeApp.ueAddress == ueAddress &&
                std::find(appInstanceIds.begin(), appInstanceIds.end(), activeApp.mecAppInstanceId) != appInstanceIds.end() &&
                tmpMeAppMap.find(activeApp.contextId) != tmpMeAppMap.end()) {
            deferMigrationRequest(pkt);
            return;
        }
    }

    for (const auto& contextApp : meAppMap) {   // for loop on the mecAppMap
        auto oldMecApp = contextApp.second;
        EV << "MecOrchestrator::migrateMecApp - checking contextId=" << contextApp.first
           << " appInstanceId=" << oldMecApp.mecAppInstanceId
           << " appUE=" << oldMecApp.ueAddress.str()
           << " requestedUE=" << ueAddress.str()
           << " currentMecHost="
           << (oldMecApp.mecHost != nullptr ? oldMecApp.mecHost->getFullPath() : "<null>")
           << endl;
        if (oldMecApp.lifecycle == MecAppLifecycle::ACTIVE &&
                oldMecApp.ueAddress == ueAddress) {    // ue perfomed handover
            for (auto appInstanceId: appInstanceIds) {  // for loop on app instanceId of mobility-aware meApp found by ams
                EV << "MecOrchestrator::migrateMecApp - checking requested appInstanceId="
                   << appInstanceId << " against active instance="
                   << oldMecApp.mecAppInstanceId << " for contextId="
                   << oldMecApp.contextId << endl;
                bool sameCoverageArea = false;
                if (oldMecApp.lifecycle == MecAppLifecycle::ACTIVE &&
                        oldMecApp.mecAppInstanceId.compare(appInstanceId) == 0) {
                    EV << "MecOrchestrator::migrateMecApp - matched app instance "
                       << appInstanceId << " for contextId=" << oldMecApp.contextId << endl;
                    if (tmpMeAppMap.find(oldMecApp.contextId) != tmpMeAppMap.end()) {
                        deferMigrationRequest(pkt);
                        return;
                    }
                    for (auto mecHost: cellToMecHosts[trgCellId]) {
                        EV << "MecOrchestrator::migrateMecApp - comparing current MEC host "
                           << (oldMecApp.mecHost != nullptr ? oldMecApp.mecHost->getFullPath() : "<null>")
                           << " with target-cell MEC host "
                           << (mecHost != nullptr ? mecHost->getFullPath() : "<null>") << endl;
                        if (oldMecApp.mecHost == mecHost) { // check if the target cell is associated to the source mec host or not
                            EV << "MecOrchestrator::migrateMecApp - host match: "
                               << (mecHost != nullptr ? mecHost->getFullPath() : "<null>")
                               << " serves target cell " << trgCellId
                               << "; migration will be skipped" << endl;
                            sameCoverageArea = true;
                            break;
                        }
                    }
                    if (!sameCoverageArea) {
                        EV << "MecOrchestrator::migrateMecApp - current MEC host does not serve target cell "
                           << trgCellId << "; attempting app migration" << endl;
                        // if the target cell is not associated with the source mec host => do migration
                        int contextId = oldMecApp.contextId;
                        auto it = mecApplicationDescriptors_.find(oldMecApp.appDId);
                        if (it == mecApplicationDescriptors_.end()) {
                            // this should not happen bc the appDid is in the mecAppMap, so the mecApp is already instantiated at least once
                            EV << "MecOrchestrator::migrateMecApp - Application package with AppDId[" << oldMecApp.appDId << "] not onboarded." << endl;
                            continue;
                        }
                        const ApplicationDescriptor& desc = it->second;
                        // find new mec host among those associated with target cell
                        cModule *bestHost = mecHostSelectionPolicy_->findBestTargetMecHost(desc, cellToMecHosts[trgCellId]);

                        if (bestHost != nullptr) {
                            int migrationId = nextMigrationId_++;
                            EV << "MecOrchestrator::migrateMecApp - selected target MEC host "
                               << bestHost->getFullPath() << " for contextId=" << contextId
                               << " migrationId=" << migrationId << endl;
                            auto createAppMsg = inet::makeShared<CreateAppMessage>();
                            createAppMsg->setType(MIGRATE_MEAPP);
                            createAppMsg->setMigrationId(migrationId);

                            createAppMsg->setUeAppID(oldMecApp.mecUeAppID);
                            createAppMsg->setMEModuleName(desc.getAppName().c_str());
                            createAppMsg->setMEModuleType(desc.getAppProvider().c_str());

                            createAppMsg->setRequiredCpu(desc.getVirtualResources().cpu);
                            createAppMsg->setRequiredRam(desc.getVirtualResources().ram);
                            createAppMsg->setRequiredDisk(desc.getVirtualResources().disk);

                            // This field is useful for MEC services not ETSI MEC compliant (e.g. OMNeT++ like)
                            // In such cases, the VIM must connect the gates between the MEC application and the service

                            // Insert OMNeT like services, only one is supported for now
                            if (!desc.getOmnetppServiceRequired().empty())
                                createAppMsg->setRequiredService(desc.getOmnetppServiceRequired().c_str());
                            else
                                createAppMsg->setRequiredService("NULL");

                            createAppMsg->setContextId(contextId);

                            inet::B msgSize = inet::B(80 + strlen(createAppMsg->getType()) + strlen(createAppMsg->getSourceAddress())
                                    + strlen(createAppMsg->getDestinationAddress()) + strlen(createAppMsg->getDestinationMecAppAddress()) + strlen(createAppMsg->getMEModuleType())
                                    + strlen(createAppMsg->getMEModuleName()) + strlen(createAppMsg->getRequiredService()) + strlen(createAppMsg->getProvidedService())
                                    + sizeof(migrationId));
                            createAppMsg->setChunkLength(msgSize);

                            // create pkt to send through socket to the mepm on the new mec host
                            inet::Packet *newPkt = new inet::Packet("CreateAppMessage");   // new app creation
                            newPkt->insertAtBack(createAppMsg);


                            // Add the new MEC app in the map structure
                            mecAppMapEntry newMecApp;
                            newMecApp.appDId = oldMecApp.appDId;
                            newMecApp.mecUeAppID = oldMecApp.mecUeAppID;
                            newMecApp.mecHost = bestHost;
                            newMecApp.ueAddress = oldMecApp.ueAddress;
                            newMecApp.vim = bestHost->getSubmodule("vim");
                            newMecApp.mecpm = bestHost->getSubmodule("mecPlatformManager");

                            newMecApp.mecAppName = desc.getAppName().c_str();
                            newMecApp.startMigrationTime = mepmMsg->getTimestamp();
                            newMecApp.migrationId = migrationId;
                            newMecApp.lifecycle = MecAppLifecycle::MIGRATION_TARGET;

                            tmpMeAppMap[contextId] = newMecApp;

                            MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(newMecApp.mecpm);

                            inet::L3Address mepmAddress = mecpm->getMepmAddress();
                            int sockId = mepmSockets_[mepmAddress];
                            inet::TcpSocket *socket = check_and_cast_nullable<inet::TcpSocket *>(sockets_.getSocketById(sockId));

                            socket->send(newPkt);
                        }
                        else {
                            EV << "MecOrchestrator::migrateMecApp - no suitable MEC host selected for target cell "
                               << trgCellId << "; the MEC app cannot be migrated" << endl;
                        }
                    }
                    else {
                        EV << "MecOrchestrator::migrateMecApp - migration skipped: current MEC host "
                           << (oldMecApp.mecHost != nullptr ? oldMecApp.mecHost->getFullPath() : "<null>")
                           << " already serves target cell " << trgCellId << endl;
                    }
                }
                else {
                    EV << "MecOrchestrator::migrateMecApp - app instance mismatch: active="
                       << oldMecApp.mecAppInstanceId << " requested=" << appInstanceId << endl;
                }
            }   // end for - appInstanceId
            EV << "MecOrchestrator::migrateMecApp - finished checking requested app instances for contextId="
               << oldMecApp.contextId << " (count=" << appInstanceIds.size() << ")" << endl;
        }   // end if - ueAddress
        else {
            EV << "MecOrchestrator::migrateMecApp - UE address mismatch for contextId="
               << contextApp.first << endl;
        }
    }   // end for - mecAppMapEntry
    delete pkt;
}

// app migration after mec host handover
void MecOrchestrator::migrateMecApps(cMessage *msg) {
    Enter_Method_Silent("MecOrchestrator::migrateMecApps");

    EV << "MecOrchestrator::migrateMecApps" << endl;
    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto mepmMsg = pkt->peekAtFront<TriggerMigrationAppMessage>();
    // note: only "UE_IPv4_ADDRESS" type is supported for now
    std::vector<std::string> appInstanceIds = mepmMsg->getAppInstanceIds();
    const auto contextIds = mepmMsg->getContextIds();
    if (!contextIds.empty()) {
        appInstanceIds.clear();
        for (int contextId : contextIds) {
            auto activeIt = meAppMap.find(contextId);
            if (activeIt != meAppMap.end() &&
                    activeIt->second.lifecycle == MecAppLifecycle::ACTIVE)
                appInstanceIds.push_back(activeIt->second.mecAppInstanceId);
        }
    }
    MacNodeId trgCellId = mepmMsg->getTrgCellId();

    for (const auto& contextApp : meAppMap) {
        const auto& activeApp = contextApp.second;
        if (activeApp.lifecycle == MecAppLifecycle::ACTIVE &&
                std::find(appInstanceIds.begin(), appInstanceIds.end(), activeApp.mecAppInstanceId) != appInstanceIds.end() &&
                tmpMeAppMap.find(activeApp.contextId) != tmpMeAppMap.end()) {
            deferMigrationRequest(pkt);
            return;
        }
    }

    for (const auto& contextApp : meAppMap) {   // for loop on the mecAppMap
        auto oldMecApp = contextApp.second;
        for (auto appInstanceId: appInstanceIds) {  // for loop on app instances of mobility-aware meApps found by ams
            bool sameCoverageArea = false;
            if (oldMecApp.lifecycle == MecAppLifecycle::ACTIVE &&
                    oldMecApp.mecAppInstanceId.compare(appInstanceId) == 0) {
                if (tmpMeAppMap.find(oldMecApp.contextId) != tmpMeAppMap.end()) {
                    deferMigrationRequest(pkt);
                    return;
                }

                for (auto mecHost: cellToMecHosts[trgCellId]) {
                    if (oldMecApp.mecHost == mecHost) { // check if the target cell is associated to the source mec host or not
                        sameCoverageArea = true;
                        break;
                    }
                }
                if (!sameCoverageArea) {
                    int contextId = oldMecApp.contextId;
                    auto it = mecApplicationDescriptors_.find(oldMecApp.appDId);
                    if (it == mecApplicationDescriptors_.end()) {
                        // this should not happen bc the appDid is in the mecAppMap, so the mecApp is already instantiated at least once
                        EV << "MecOrchestrator::migrateMecApps - Application package with AppDId[" << oldMecApp.appDId << "] not onboarded." << endl;
                        continue;
                    }
                    const ApplicationDescriptor& desc = it->second;
                    // find new mec host among those associated with target cell
                    cModule *bestHost = mecHostSelectionPolicy_->findBestTargetMecHost(desc, cellToMecHosts[trgCellId]);

                    if (bestHost != nullptr) {
                        int migrationId = nextMigrationId_++;
                        auto createAppMsg = inet::makeShared<CreateAppMessage>();
                        createAppMsg->setType(MIGRATE_MEAPP);
                        createAppMsg->setMigrationId(migrationId);

                        createAppMsg->setUeAppID(oldMecApp.mecUeAppID);
                        createAppMsg->setMEModuleName(desc.getAppName().c_str());
                        createAppMsg->setMEModuleType(desc.getAppProvider().c_str());

                        createAppMsg->setRequiredCpu(desc.getVirtualResources().cpu);
                        createAppMsg->setRequiredRam(desc.getVirtualResources().ram);
                        createAppMsg->setRequiredDisk(desc.getVirtualResources().disk);

                        // This field is useful for MEC services not ETSI MEC compliant (e.g. OMNeT++ like)
                        // In such cases, the VIM must connect the gates between the MEC application and the service

                        // Insert OMNeT like services, only one is supported for now
                        if (!desc.getOmnetppServiceRequired().empty())
                            createAppMsg->setRequiredService(desc.getOmnetppServiceRequired().c_str());
                        else
                            createAppMsg->setRequiredService("NULL");

                        createAppMsg->setContextId(contextId);

                        inet::B msgSize = inet::B(80 + strlen(createAppMsg->getType()) + strlen(createAppMsg->getSourceAddress())
                                + strlen(createAppMsg->getDestinationAddress()) + strlen(createAppMsg->getDestinationMecAppAddress()) + strlen(createAppMsg->getMEModuleType())
                                + strlen(createAppMsg->getMEModuleName()) + strlen(createAppMsg->getRequiredService()) + strlen(createAppMsg->getProvidedService())
                                + sizeof(migrationId));
                        createAppMsg->setChunkLength(msgSize);

                        // create pkt to send through socket to the mepm on the new mec host
                        inet::Packet *newPkt = new inet::Packet("CreateAppMessage");   // new app creation
                        newPkt->insertAtBack(createAppMsg);


                        // Add the new MEC app in the map structure
                        mecAppMapEntry newMecApp;
                        newMecApp.appDId = oldMecApp.appDId;
                        newMecApp.mecUeAppID = oldMecApp.mecUeAppID;
                        newMecApp.mecHost = bestHost;
                        newMecApp.ueAddress = oldMecApp.ueAddress;
                        newMecApp.vim = bestHost->getSubmodule("vim");
                        newMecApp.mecpm = bestHost->getSubmodule("mecPlatformManager");

                        newMecApp.mecAppName = desc.getAppName().c_str();

                        newMecApp.startMigrationTime = mepmMsg->getTimestamp();
                        newMecApp.migrationId = migrationId;
                        newMecApp.lifecycle = MecAppLifecycle::MIGRATION_TARGET;

                        tmpMeAppMap[contextId] = newMecApp;

                        MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(newMecApp.mecpm);

                        inet::L3Address mepmAddress = mecpm->getMepmAddress();
                        int sockId = mepmSockets_[mepmAddress];
                        inet::TcpSocket *socket = check_and_cast_nullable<inet::TcpSocket *>(sockets_.getSocketById(sockId));

                        socket->send(newPkt);
                    }
                    else {
                        EV << "MecOrchestrator::migrateMecApps - A suitable MEC host has not been selected, the MECApp cannot be migrated" << endl;
                    }
                }
            } // end if compare appInstanceId
        }   // end for - appInstanceId
    }   // end for - mecAppMapEntry
    delete pkt;
}

void MecOrchestrator::migrateAllMecApps(cMessage *msg) {
    Enter_Method_Silent("MecOrchestrator::migrateAllMecApps");

    EV << "MecOrchestrator::migrateAllMecApps" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto mepmMsg = pkt->peekAtFront<MigrateAppMessage>();
    int mepmId = mepmMsg->getMepmId();

    for (const auto& contextApp : meAppMap) {
        const auto& activeApp = contextApp.second;
        if (activeApp.lifecycle == MecAppLifecycle::ACTIVE &&
                activeApp.mecpm->getId() == mepmId &&
                tmpMeAppMap.find(activeApp.contextId) != tmpMeAppMap.end()) {
            deferMigrationRequest(pkt);
            return;
        }
    }

    MigrationMecServiceSelectionBased *migrationSelectionPolicy = dynamic_cast<MigrationMecServiceSelectionBased *> (mecHostSelectionPolicy_);
    for (const auto& contextApp : meAppMap) {
        if (contextApp.second.lifecycle == MecAppLifecycle::ACTIVE &&
                contextApp.second.mecpm->getId() == mepmId) {

            auto oldMecApp = contextApp.second;
            int contextId = oldMecApp.contextId;
            if (tmpMeAppMap.find(contextId) != tmpMeAppMap.end()) {
                deferMigrationRequest(pkt);
                return;
            }

            auto it = mecApplicationDescriptors_.find(oldMecApp.appDId);
            if (it == mecApplicationDescriptors_.end()) {
                // this should not happen bc the appDid is in the mecAppMap, so the mecApp is already instantiated at least once
                EV << "MecOrchestrator::migrateAllMecApps - Application package with AppDId[" << oldMecApp.appDId << "] not onboarded." << endl;
//                sendCreateAppContextAck(false, contAppMsg->getRequestId());
                continue;
            }

            const ApplicationDescriptor& desc = it->second;
            auto* mecHostName = oldMecApp.mecHost->getName();
            cModule *bestHost = migrationSelectionPolicy->findNewBestMecHost(desc, mecHostName);

            if (bestHost != nullptr) {
                int migrationId = nextMigrationId_++;
                auto createAppMsg = inet::makeShared<CreateAppMessage>();
                createAppMsg->setType(MIGRATE_MEAPP);
                createAppMsg->setMigrationId(migrationId);

                createAppMsg->setUeAppID(oldMecApp.mecUeAppID);
                createAppMsg->setMEModuleName(desc.getAppName().c_str());
                createAppMsg->setMEModuleType(desc.getAppProvider().c_str());

                createAppMsg->setRequiredCpu(desc.getVirtualResources().cpu);
                createAppMsg->setRequiredRam(desc.getVirtualResources().ram);
                createAppMsg->setRequiredDisk(desc.getVirtualResources().disk);

                // This field is useful for MEC services not ETSI MEC compliant (e.g. OMNeT++ like)
                // In such cases, the VIM must connect the gates between the MEC application and the service

                // Insert OMNeT like services, only one is supported for now
                if (!desc.getOmnetppServiceRequired().empty())
                    createAppMsg->setRequiredService(desc.getOmnetppServiceRequired().c_str());
                else
                    createAppMsg->setRequiredService("NULL");

                createAppMsg->setContextId(contextId);

                inet::B msgSize = inet::B(80 + strlen(createAppMsg->getType()) + strlen(createAppMsg->getSourceAddress())
                        + strlen(createAppMsg->getDestinationAddress()) + strlen(createAppMsg->getDestinationMecAppAddress()) + strlen(createAppMsg->getMEModuleType())
                        + strlen(createAppMsg->getMEModuleName()) + strlen(createAppMsg->getRequiredService()) + strlen(createAppMsg->getProvidedService())
                        + sizeof(migrationId));
                createAppMsg->setChunkLength(msgSize);

                // create pkt to send through socket to the mepm on the new mec host
                inet::Packet *newPkt = new inet::Packet("CreateAppMessage");   // new app creation
                newPkt->insertAtBack(createAppMsg);


                // Add the new MEC app in the map structure
                mecAppMapEntry newMecApp;
                newMecApp.appDId = oldMecApp.appDId;
                newMecApp.mecUeAppID = oldMecApp.mecUeAppID;
                newMecApp.mecHost = bestHost;
                newMecApp.ueAddress = oldMecApp.ueAddress;
                newMecApp.vim = bestHost->getSubmodule("vim");
                newMecApp.mecpm = bestHost->getSubmodule("mecPlatformManager");

                newMecApp.mecAppName = desc.getAppName().c_str();
                newMecApp.migrationId = migrationId;
                newMecApp.lifecycle = MecAppLifecycle::MIGRATION_TARGET;

                tmpMeAppMap[contextId] = newMecApp;

                MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(newMecApp.mecpm);

                inet::L3Address mepmAddress = mecpm->getMepmAddress();
                int sockId = mepmSockets_[mepmAddress];
                inet::TcpSocket *socket = check_and_cast_nullable<inet::TcpSocket *>(sockets_.getSocketById(sockId));

                socket->send(newPkt);
            }
            else {
                // throw cRuntimeError("MecOrchestrator::startMECApp - A suitable MEC host has not been selected");
                EV << "MecOrchestrator::migrateAllMecApps - A suitable MEC host has not been selected, the MECApp cannot be migrated" << endl;

            }
        }
    }
    delete pkt;
}

void MecOrchestrator::sendDeleteAppContextAck(bool result, unsigned int requestSno, int contextId)
{
    EV << "MecOrchestrator::sendDeleteAppContextAck - result: " << result << " reqSno: " << requestSno << " contextId: " << contextId << endl;
    DeleteContextAppAckMessage *ack = new DeleteContextAppAckMessage();
    ack->setType(ACK_DELETE_CONTEXT_APP);
    ack->setRequestId(requestSno);
    ack->setSuccess(result);

    send(ack, "toUALCMP");
}

void MecOrchestrator::sendCreateAppContextAck(bool result, unsigned int requestSno, int contextId)
{
    EV << "MecOrchestrator::sendCreateAppContextAck - result: " << result << " reqSno: " << requestSno << " contextId: " << contextId << endl;
    CreateContextAppAckMessage *ack = new CreateContextAppAckMessage();
    ack->setType(ACK_CREATE_CONTEXT_APP);

    if (result) {
        auto activeAppIt = meAppMap.find(contextId);
        if (activeAppIt == meAppMap.end() ||
                activeAppIt->second.lifecycle != MecAppLifecycle::ACTIVE) {
            EV << "MecOrchestrator::ackMEAppPacket - ERROR meApp[" << contextId << "] does not exist!" << endl;
            return;
        }

        mecAppMapEntry mecAppStatus = activeAppIt->second;

        ack->setSuccess(true);
        ack->setContextId(contextId);
        ack->setAppInstanceId(mecAppStatus.mecAppInstanceId.c_str());
        ack->setRequestId(requestSno);
        std::stringstream uri;

        uri << mecAppStatus.mecAppAddress.str() << ":" << mecAppStatus.mecAppPort;

        ack->setAppInstanceUri(uri.str().c_str());
    }
    else {
        ack->setRequestId(requestSno);
        ack->setSuccess(false);
    }
    send(ack, "toUALCMP");
}

void MecOrchestrator::handleMigrateAppAck(cMessage *msg)
{
    EV << "MecOrchestrator::handleMigrateAppAck" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto mepmMsg = pkt->peekAtFront<MigrateAppAckMessage>();
    int contextId = mepmMsg->getContextId();
    int migrationId = mepmMsg->getMigrationId();
    EV << "MecOrchestrator::handleMigrateAppAck - processing contextId: " << contextId
       << " migrationId: " << migrationId << endl;
    auto tmpIt = tmpMeAppMap.find(contextId);
    if (tmpIt == tmpMeAppMap.end()) {
        EV_WARN << "MecOrchestrator::handleMigrateAppAck - no migration target found for contextId="
                << contextId << " migrationId=" << migrationId << endl;
        delete pkt;
        return;
    }
    if (tmpIt->second.migrationId != migrationId) {
        EV_WARN << "MecOrchestrator::handleMigrateAppAck - stale ACK: expected migrationId="
                << tmpIt->second.migrationId << " but received " << migrationId << endl;
        delete pkt;
        return;
    }

    auto tmpMecApp = tmpIt->second;
    double processingTime = 0.0;

    if (!mepmMsg->getStatus()) {
        EV_ERROR << "MecOrchestrator::handleMigrateAppAck - target instantiation failed for contextId="
                 << contextId << " migrationId=" << migrationId << endl;
        MECOrchestratorMessage *meoMsg = new MECOrchestratorMessage("MECOrchestratorMessage");
        meoMsg->setType(MIGRATE_CONTEXT_APP);
        meoMsg->setContextId(contextId);
        meoMsg->setMigrationId(migrationId);
        meoMsg->setRequestId(tmpMecApp.mecUeAppID);
        meoMsg->setSuccess(false);
        scheduleAt(simTime() + instantiationTime, meoMsg);
        delete pkt;
        return;
    }

    auto it = mecApplicationDescriptors_.find(tmpMecApp.appDId);
    if (it == mecApplicationDescriptors_.end()) {
        // this should not happen bc the appDid is in the mecAppMap, so the mecApp is already instantiated at least once
        EV_ERROR << "MecOrchestrator::handleMigrateAppAck - Application package with AppDId["
                 << tmpMecApp.appDId << "] not onboarded." << endl;
        delete pkt;
        return;
    }

    const ApplicationDescriptor& desc = it->second;
    /*
      * If the application descriptor refers to a simulated MEC app, the system eventually instantiates the MEC app object.
      * If the application descriptor refers to a MEC application running outside the simulator, i.e., emulation mode,
      * the system allocates the resources without instantiating any module.
      * The application descriptor contains the address and port information to communicate with the MEC application.
      */

     MecAppInstanceInfo *appInfo = new MecAppInstanceInfo();
     appInfo->status = mepmMsg->getStatus();

     if (desc.isMecAppEmulated()) {
         EV << "MecOrchestrator::handleMigrateAppAck - MEC app is emulated" << endl;
         appInfo->endPoint.addr = inet::L3Address(desc.getExternalAddress().c_str());
         appInfo->endPoint.port = desc.getExternalPort();
         appInfo->instanceId = "emulated_" + desc.getAppName();
         tmpMecApp.isEmulated = true;
     }
     else {
         appInfo->endPoint = mepmMsg->getEndPoint();
         appInfo->instanceId = mepmMsg->getInstanceId();
         appInfo->reference = getSimulation()->getModule(mepmMsg->getModuleId());
         tmpMecApp.isEmulated = false;
     }

     delete pkt;

     EV << "MecOrchestrator::handleMigrateAppAck - new MEC application with name: " << appInfo->instanceId << " instantiated on MEC host [" << tmpMecApp.mecHost << "] at " << appInfo->endPoint.addr.str() << ":" << appInfo->endPoint.port << endl;

     tmpMecApp.mecAppAddress = appInfo->endPoint.addr;
     tmpMecApp.mecAppPort = appInfo->endPoint.port;
     tmpMecApp.mecAppInstanceId = appInfo->instanceId;
     tmpMecApp.contextId = contextId;
     tmpMecApp.reference = appInfo->reference;
     tmpIt->second = tmpMecApp;

     MECOrchestratorMessage *meoMsg = new MECOrchestratorMessage("MECOrchestratorMessage");
     meoMsg->setContextId(contextId);
     meoMsg->setMigrationId(migrationId);
     meoMsg->setType(MIGRATE_CONTEXT_APP);
     // requestId field used to send the ueAppID of interest to UALCMP
     meoMsg->setRequestId(tmpMecApp.mecUeAppID);
     meoMsg->setSuccess(true);

     processingTime += instantiationTime;
     scheduleAt(simTime() + processingTime, meoMsg);

     delete appInfo;

}

void MecOrchestrator::sendMigrateAppContext(bool result, int ueAppId, int contextId, int migrationId) {
    EV << "MecOrchestrator::sendMigrateAppContext - result: " << result << " reqSno: "
       << ueAppId << " contextId: " << contextId << " migrationId: " << migrationId << endl;
    auto targetIt = tmpMeAppMap.find(contextId);
    if (targetIt == tmpMeAppMap.end() || targetIt->second.migrationId != migrationId) {
        EV_ERROR << "MecOrchestrator::sendMigrateAppContext - no matching staged target for contextId="
                 << contextId << " migrationId=" << migrationId << endl;
        return;
    }

    if (!result) {
        EV << "MecOrchestrator::sendMigrateAppContext - App NOT migrated" << endl;
        tmpMeAppMap.erase(targetIt);
        return;
    }

    auto sourceIt = meAppMap.find(contextId);
    if (sourceIt == meAppMap.end() ||
            sourceIt->second.lifecycle != MecAppLifecycle::ACTIVE) {
        EV_ERROR << "MecOrchestrator::sendMigrateAppContext - no active source instance for contextId="
                 << contextId << "; cannot promote migration target" << endl;
        return;
    }
    if (pendingDeleteMeApps.find(migrationId) != pendingDeleteMeApps.end()) {
        throw cRuntimeError("MecOrchestrator::sendMigrateAppContext - duplicate migrationId %d", migrationId);
    }

    mecAppMapEntry oldMecApp = sourceIt->second;
    oldMecApp.lifecycle = MecAppLifecycle::SOURCE_PENDING_DELETE;
    pendingDeleteMeApps.emplace(migrationId, oldMecApp);

    mecAppMapEntry newMecApp = targetIt->second;
    newMecApp.lifecycle = MecAppLifecycle::ACTIVE;
    meAppMap[contextId] = newMecApp;
    tmpMeAppMap.erase(targetIt);

    MigrateContextAppMessage *migrateMsg = new MigrateContextAppMessage();
    migrateMsg->setType(MIGRATE_CONTEXT_APP);
    mecAppMapEntry mecAppStatus = newMecApp;

    migrateMsg->setStartMigrationTime(mecAppStatus.startMigrationTime);
    migrateMsg->setStopMigrationTime(simTime());
    migrateMsg->setContextId(contextId);
    migrateMsg->setAppInstanceId(mecAppStatus.mecAppInstanceId.c_str());
    migrateMsg->setRequestId(ueAppId);
    std::stringstream uri;

    uri << mecAppStatus.mecAppAddress.str() << ":" << mecAppStatus.mecAppPort;

    migrateMsg->setAppInstanceUri(uri.str().c_str());

    send(migrateMsg, "toUALCMP");

    // todo: future work: manage the case of multi-ue for a single mec app instance -> remove old mec app instance only if not used anymore
    MECOrchestratorMessage *deleteAppMsg = new MECOrchestratorMessage("MECOrchestratorMessage");
    deleteAppMsg->setType(STOP_MIGRATED_INSTANCE_APP);
    deleteAppMsg->setContextId(contextId);
    deleteAppMsg->setMigrationId(migrationId);
    deleteAppMsg->setRequestId(oldMecApp.mecUeAppID);
    deleteAppMsg->setSuccess(true);

    double processingTime = removalWaitingTime + terminationTime;
    scheduleAt(simTime() + processingTime, deleteAppMsg);

}


cModule *MecOrchestrator::findBestMecHost(const ApplicationDescriptor& appDesc)
{
    EV << "MecOrchestrator::findBestMecHost - finding best MEC host..." << endl;
    cModule *bestHost = nullptr;

    for (auto mecHost : mecHosts) {
        VirtualisationInfrastructureManager *vim = check_and_cast<VirtualisationInfrastructureManager *>(mecHost->getSubmodule("vim"));
        ResourceDescriptor resources = appDesc.getVirtualResources();
        bool res = vim->isAllocable(resources.ram, resources.disk, resources.cpu);
        if (!res) {
            EV << "MecOrchestrator::findBestMecHost - MEC host [" << mecHost->getName() << "] has not got enough resources. Searching again..." << endl;
            continue;
        }

        // Temporarily select this MEC host as the best
        EV << "MecOrchestrator::findBestMecHost - MEC host [" << mecHost->getName() << "] temporarily chosen as best MEC host, checking for the required MEC services.." << endl;
        bestHost = mecHost;

        MecPlatformManager *mecpm = check_and_cast<MecPlatformManager *>(mecHost->getSubmodule("mecPlatformManager"));
        auto mecServices = mecpm->getAvailableMecServices();
        std::string serviceName;

        // I assume the app requires only one MEC service
        if (appDesc.getAppServicesRequired().size() > 0) {
            serviceName = appDesc.getAppServicesRequired()[0];
        }
        else {
            break;
        }
        for (const auto& service : *mecServices) {
            if (serviceName == service.getName()) {
                bestHost = mecHost;
                break;
            }
        }
    }
    if (bestHost != nullptr)
        EV << "MecOrchestrator::findBestMecHost - best MEC host: " << bestHost->getName() << endl;
    else
        EV << "MecOrchestrator::findBestMecHost - no MEC host found" << endl;

    return bestHost;
}

void MecOrchestrator::getConnectedMecHosts()
{
    EV << "MecOrchestrator::getConnectedMecHosts - mecHostList: " << par("mecHostList").str() << endl;

    // Getting the list of MEC hosts associated with this MEC system from parameter
    auto mecHostList = check_and_cast<cValueArray *>(par("mecHostList").objectValue());
    if (mecHostList->size() > 0) {
        for (int i = 0; i < mecHostList->size(); i++) {
            const char *token = mecHostList->get(i).stringValue();
            EV << "MecOrchestrator::getConnectedMecHosts - mec host (from par): " << token << endl;
            cModule *mecHostModule = getSimulation()->getModuleByPath(token);
            mecHosts.push_back(mecHostModule);
        }
    }
    else {
        EV << "MecOrchestrator::getConnectedMecHosts - No mecHostList found" << endl;
    }
}

void MecOrchestrator::getCellMecHostsConnections() {
    EV << "MecOrchestrator::getCellMecHostsConnections " << endl;

    for (auto mecHostModule: mecHosts) {
        auto bsList = check_and_cast<cValueArray *>(mecHostModule->par("bsList").objectValue());
        for (int i = 0; i < bsList->size(); i++) {
            const char *token = bsList->get(i).stringValue();
            cModule *bsModule = getSimulation()->getModuleByPath(token);
            CellInfo *cellInfo = check_and_cast<CellInfo *>(bsModule->getSubmodule("cellInfo"));
            // insert the mec host in the vector of each corresponding cell -> for each cell there'll be a vector of all associate mec hosts
            cellToMecHosts[cellInfo->getMacCellId()].push_back(mecHostModule);
            EV_DEBUG << "MecOrchestrator::getCellMecHostsConnections - cellToMecHosts - cell: " << cellInfo->getMacCellId() << " - " << binder_->getModuleByMacNodeId(cellInfo->getMacCellId())->getName() << endl;
        }
    }

    // debug
    for (auto cell: cellToMecHosts) {
        for (auto mecHost: cell.second) {
            EV_DEBUG << "MecOrchestrator::getCellMecHostsConnections - cellToMecHosts - cell: " << cell.first << " - mecHost: " << mecHost->getFullName() << endl;
        }
    }
}

bool MecOrchestrator::isMecHostServingCell(const std::string& mecHostName, MacNodeId cellId) const
{
    Enter_Method_Silent("MecOrchestrator::isMecHostServingCell");

    auto cellIt = cellToMecHosts.find(cellId);
    if (cellIt == cellToMecHosts.end())
        return false;

    for (const cModule *host : cellIt->second) {
        if (host != nullptr &&
                (mecHostName == host->getFullPath() || mecHostName == host->getName()))
            return true;
    }

    return false;
}

const ApplicationDescriptor& MecOrchestrator::onboardApplicationPackage(const char *fileName)
{
    EV << "MecOrchestrator::onBoardApplicationPackages - onboarding application package (from request): " << fileName << endl;
    ApplicationDescriptor appDesc(fileName);
    if (mecApplicationDescriptors_.find(appDesc.getAppDId()) != mecApplicationDescriptors_.end()) {
        EV << "MecOrchestrator::onboardApplicationPackages() - Application descriptor with appName [" << fileName << "] is already present.\n" << endl;
    }
    else {
        mecApplicationDescriptors_[appDesc.getAppDId()] = appDesc; // add to the mecApplicationDescriptors_
    }

    return mecApplicationDescriptors_[appDesc.getAppDId()];
}

void MecOrchestrator::registerMecService(ServiceDescriptor& serviceDescriptor) const
{
    EV << "MecOrchestrator::registerMecService - Registering MEC service [" << serviceDescriptor.name << "]" << endl;
    for (auto mecHost : mecHosts) {
        cModule *module = mecHost->getSubmodule("mecPlatform")->getSubmodule("serviceRegistry");
        if (module != nullptr) {
            EV << "MecOrchestrator::registerMecService - Registering MEC service [" << serviceDescriptor.name << "] in MEC host [" << mecHost->getName() << "]" << endl;
            ServiceRegistry *serviceRegistry = check_and_cast<ServiceRegistry *>(module);
            serviceRegistry->registerMecService(serviceDescriptor);
        }
    }
}

void MecOrchestrator::updateMecHostServingArea(cMessage *msg) {
    EV << "MecOrchestrator::updateMecHostServingArea" << endl;

    inet::Packet *pkt = check_and_cast<inet::Packet *>(msg);
    auto mepmMsg = pkt->peekAtFront<UpdateServingAreaMessage>();

    const char *mecHostName = mepmMsg->getMecHostName();
    MacNodeId srcCell = mepmMsg->getSrcCellId();
    MacNodeId trgCell = mepmMsg->getTrgCellId();

    cModule *mecHostModule = getSimulation()->getModuleByPath(mecHostName);

    // remove mecHost module reference from source cell mecHost list
    auto it = std::find(cellToMecHosts[srcCell].begin(), cellToMecHosts[srcCell].end(), opp_component_ptr<cModule>(mecHostModule));
    if (it != cellToMecHosts[srcCell].end())
        cellToMecHosts[srcCell].erase(it);

    // create response message
    auto responseMsg = inet::makeShared<UpdateServingAreaResponse>();
    responseMsg->setType(ACK_UPDATE_SERVING_AREA);
    responseMsg->setMecHostName(mecHostName);
    responseMsg->setMepmAddress(mepmMsg->getMepmAddress());
    responseMsg->setSrcCellId(srcCell);
    responseMsg->setTrgCellId(trgCell);

    auto itTrg = std::find(cellToMecHosts[trgCell].begin(), cellToMecHosts[trgCell].end(), opp_component_ptr<cModule>(mecHostModule));
    if (itTrg != cellToMecHosts[trgCell].end()) {
        // target cell coverage area is already associated to this mecHost
        // -> same serving area, no update
        // NOTE: this should not happen because the HMS/mecHost checks his serving area before request the update
        responseMsg->setStatus(false);
        responseMsg->setResponse("The MEC host already serves the target cell coverage area");
    }
    else {
        // add mecHost module reference to target cell mecHost list
        cellToMecHosts[trgCell].push_back(mecHostModule);

        responseMsg->setStatus(true);
        responseMsg->setResponse("The MecHost-cells mapping has been updated");
    }

    // send response msg to mepm
    inet::B msgSize = inet::B(40 + strlen(responseMsg->getType()) + strlen(mecHostName)
           + strlen(mepmMsg->getMepmAddress().str().c_str()) + strlen(responseMsg->getResponse()));
    responseMsg->setChunkLength(msgSize);

    inet::Packet *newPkt = new inet::Packet("UpdateServingAreaResponse");
    newPkt->insertAtBack(responseMsg);

    inet::L3Address mepmAddress = mepmMsg->getMepmAddress();
    int sockId = mepmSockets_[mepmAddress];
    inet::TcpSocket *socket = check_and_cast_nullable<inet::TcpSocket *>(sockets_.getSocketById(sockId));

    socket->send(newPkt);
}

void MecOrchestrator::onboardApplicationPackages()
{
    // Getting the list of MEC hosts associated with this MEC system from parameter
    auto mecApplicationPackageList = check_and_cast<cValueArray *>(par("mecApplicationPackageList").objectValue());
    if (mecApplicationPackageList->size() > 0) {
        for (int i = 0; i < mecApplicationPackageList->size(); i++) {
            const char *token = mecApplicationPackageList->get(i).stringValue();
            std::string buf = std::string("ApplicationDescriptors/") + token + ".json";
            onboardApplicationPackage(buf.c_str());
        }
    }
    else {
        EV << "MecOrchestrator::onboardApplicationPackages - No mecApplicationPackageList found" << endl;
    }
}

const ApplicationDescriptor *MecOrchestrator::getApplicationDescriptorByAppName(const std::string& appName) const
{
    for (const auto& appDesc : mecApplicationDescriptors_) {
        if (appDesc.second.getAppName() == appName)
            return &(appDesc.second);
    }

    return nullptr;
}

} //namespace
