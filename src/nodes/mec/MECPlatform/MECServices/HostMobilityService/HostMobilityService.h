//
//                  Simu5G
//

#ifndef NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_HOSTMOBILITYSERVICE_H_
#define NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_HOSTMOBILITYSERVICE_H_

#include "nodes/mec/MECPlatform/MECServices/MECServiceBase/MecServiceBase2.h"
#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"


namespace simu5g {

using namespace omnetpp;

/**
 * Host Mobility Service
 * This class inherits the MECServiceBase module interface for the implementation
 * of the HostMobilityService (HMS)
 * (new proposed MEC service, NOT defined in ETSI specification)
 */

class HostMobilityService : public MecServiceBase2
{

    std::string callbackRefUri_; // uri used to receive notification from RNI service
    // socket to communicate with RNIS
    inet::TcpSocket *rnisSocket_ = nullptr;

    int cellChangeSubId_;

  protected:
    std::map<int, inet::ChunkQueue> socketQueue;

  public:
    HostMobilityService();
    ~HostMobilityService();
    void handleHostMobilityUpdate();

  protected:

    void initialize(int stage) override;
    void finish() override {}
    void handleMessage(cMessage *msg) override;

    void handleGETRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) override {}
    void handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)   override {}
    void handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)    override {}
    void handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) override {}

    void socketDataArrived(inet::TcpSocket *socket, inet::Packet *packet, bool urgent) override;

    void handleRnisMessage(cMessage *msg);
    void handleRnisRequestMessage(const HttpRequestMessage *msg);
    void handleRnisResponseMessage(const HttpResponseMessage *msg);

    void handleCellChangeNotification(const nlohmann::ordered_json& request);
    void sendHostMobilityNotification();


    /*
    * This method is called for every element in the subscriptions_ queue.
    */
    bool manageSubscription() override;

};

} /* namespace simu5g */

#endif /* NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_HOSTMOBILITYSERVICE_H_ */
