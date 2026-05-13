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

#ifndef __MECPLATFORMMANAGER_H_
#define __MECPLATFORMMANAGER_H_

#include <omnetpp.h>
#include <inet/common/ModuleRefByPar.h>
#include <inet/common/packet/ChunkQueue.h>

#include "common/LteCommon.h"
#include "nodes/mec/utils/MecCommon.h"
#include "nodes/mec/MECOrchestrator/MECOMessages/MECOrchestratorMessages_m.h"
#include "nodes/mec/VirtualisationInfrastructureManager/VirtualisationInfrastructureManager.h"
#include "nodes/mec/MECPlatform/ServiceRegistry/ServiceRegistry.h"

namespace simu5g {

using namespace inet;
using namespace omnetpp;

//
// simple module implementing the MEC platform manager (MECPM) entity of a
// MEC system. It does not follow the ETSI specs, but acts only as a
// passthrough between the MEC orchestrator and the MEC host modules
//
// The mecOrchestrator module is used to link the MECPM with the MEC orchestrator

class ServiceRegistry;
class MecOrchestrator;

class MecPlatformManager : public cSimpleModule, public inet::TcpSocket::ICallback
{
  protected:
    inet::ModuleRefByPar<MecOrchestrator> mecOrchestrator;
    inet::ModuleRefByPar<VirtualisationInfrastructureManager> vim;
    inet::ModuleRefByPar<ServiceRegistry> serviceRegistry;

    inet::L3Address mepmAddress_;

    inet::L3Address meoAddress_;   // destination IP address to connect to the UALCMP
    int meoDestPort_;
    inet::TcpSocket meoSocket_;

    std::map<int, inet::ChunkQueue> socketQueue;

    /* inet::TcpSocket::CallbackInterface callback methods */
    void socketDataArrived(inet::TcpSocket *socket, inet::Packet *msg, bool urgent) override;
    void socketAvailable(inet::TcpSocket *socket, inet::TcpAvailableInfo *availableInfo) override { socket->accept(availableInfo->getNewSocketId()); }
    void socketEstablished(inet::TcpSocket *socket) override {}
    void socketPeerClosed(inet::TcpSocket *socket) override {}
    void socketClosed(inet::TcpSocket *socket) override {}
    void socketFailure(inet::TcpSocket *socket, int code) override {}
    void socketStatusArrived(inet::TcpSocket *socket, inet::TcpStatusInfo *status) override {}
    void socketDeleted(inet::TcpSocket *socket) override {}

  public:

    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(cMessage *msg) override;
    void finish() override {}

    /* instantiating the requested MECApp
     *
     * The argument is a message even if it is called as a direct method call from the
     * MEC orchestrator. It could be useful in the future if the MECPM were enriched
     * with gates and deeper functionalities.
     *
     * For the instantiateMEApp method:
     * @return MecAppInstanceInfo structure with the endpoint of the MEC app.
     *
     * For the instantiateEmulatedMEApp method only a bool is returned, since the endpoint
     * is known at the MEC orchestrator (in the appDescriptor)
     */
    // instantiating the MEC app
    MecAppInstanceInfo *instantiateMEApp(CreateAppMessage *msg);    // old version, to be removed after implementing everything
    bool instantiateEmulatedMEApp(CreateAppMessage *msg);
    // terminating the corresponding MEC app
    bool terminateMEApp(DeleteAppMessage *msg);
    bool terminateEmulatedMEApp(DeleteAppMessage *msg);

    void migrateMecAppsReq();
    void migrateMecApp(cMessage *msg);

    /* stop and remove MecApp instance from the MecHost after migration
     * triggered by a message from MEO*/
    void stopMigratedMecApp(cMessage *msg);

    const std::vector<ServiceInfo> *getAvailableMecServices() const;

    /*
     * method called by the MEC service to notify its presence to the MEC system
     */

    void registerMecService(ServiceDescriptor&) const;

    inet::L3Address getMepmAddress() {
        return mepmAddress_;
    }
};

} //namespace

#endif

