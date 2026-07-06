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

#ifndef _RNISERVICE_H
#define _RNISERVICE_H

#include "nodes/mec/MECPlatform/MECServices/MECServiceBase/MecServiceBase2.h"
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/L2Meas.h"

namespace simu5g {

/**
 * Radio Network Information Service (RNI)
 * This class inherits the MECServiceBase module interface for the implementation
 * of the RNI Service defined in ETSI GS MEC 012 RNI API.
 * The currently available functionalities are related to the L2 measurements information resource
 */

class RNIService : public MecServiceBase2
{
  private:

    L2Meas L2MeasResource_;

  public:
    RNIService();
    void receiveHandoverSignal(MacNodeId nodeId, MacNodeId srcCellId, MacNodeId trgCellIds);

  protected:

    void initialize(int stage) override;
    void finish() override;
    void handleMessage(cMessage *msg) override;

    void handleGETRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) override;
    void handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)   override;
    void handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket)    override;
    void handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket *socket) override;

    /*
     * This method is called for every element in the subscriptions_ queue.
     */
    bool manageSubscription() override;

//    // subscription related methods
    void handleSubscriptionRequest(SubscriptionBase *subscription, inet::TcpSocket* socket, const nlohmann::ordered_json& request);
//
//    ~RNIService();
  private:
    void printAllSubscriptions();
};

} //namespace

#endif // ifndef _RNISERVICE_H

