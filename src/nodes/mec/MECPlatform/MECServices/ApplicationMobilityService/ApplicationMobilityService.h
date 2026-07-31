//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __SIMU5G_APPLICATIONMOBILITYSERVICE_H_
#define __SIMU5G_APPLICATIONMOBILITYSERVICE_H_

#include <omnetpp.h>
#include <string>
#include "nodes/mec/MECPlatform/MECServices/MECServiceBase/MecServiceBase2.h"
#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"

// Resources needed by the service
#include "resources/ApplicationMobilityResource.h"
#include "resources/MobilityProcedureSubscription.h"
#include "resources/MobilityProcedureNotification.h"
#include "resources/FilterCriteria.h"
#include "nodes/mec/MECPlatform/MECServices/Resources/NotificationBase.h"
#include "nodes/mec/MECPlatform/ServiceRegistry/resources/ServiceInfo.h"


namespace simu5g {

using namespace omnetpp;

/**
 * Authors:
 * Alessandro Calvio
 * Angelo Feraudo
 *  
 * Currently this version of AMS support intra-host migration (vehicular computing)
 *
 *  */
struct SubscriptionLinkList
{
    LinkType self;
    std::vector<std::string> subscriptionType; // 0 = RESERVED, 1 = MOBILITY_PORCEDURE, 2 = ADJACENT_APPINDO
};

class ApplicationMobilityService : public MecServiceBase2
{
  std::string baseUriServiceRegistration_;
  std::string callbackUri_; // uri used to receive notification from RNI service
  int applicationServiceIds;
  ApplicationMobilityResource *registrationResources_;
  // id = id device
  // list of subscription for that device
  std::map<std::string, SubscriptionLinkList> subscriptionLinkList; // Not used so far..

  int migrationCounter_;
  simsignal_t totalMigrationsSignal_;

  // socket to communicate with RNIS
  inet::TcpSocket *rnisSocket_ = nullptr;

  // there could be more than one hms linked to a single ams
  std::map<int, std::string> hmsSockIdToMecHost;   // socketId-mecHostName
  std::map<std::string, int> hmsSubIds_;     // mapping bw mecHostName and subId of hostMobilitySubscription to hms on that mec host

  protected:
    std::map<int, inet::ChunkQueue> socketQueue;

  public:
    ApplicationMobilityService();
    ~ApplicationMobilityService();

  protected:
    void initialize(int stage) override;
    void finish() override;
    void handleMessage(cMessage *msg) override;

    void handleGETRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket) override;
    void handlePOSTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket)   override;
    void handlePUTRequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket)    override;
    void handleDELETERequest(const HttpRequestMessage *currentRequestMessageServed, inet::TcpSocket* socket) override;

    void socketDataArrived(inet::TcpSocket *socket, inet::Packet *packet, bool urgent) override;
    void socketEstablished(inet::TcpSocket *socket) override;

    void connectToHms(std::string mecHostName);
    void handleHmsMessage(cMessage *msg, inet::TcpSocket *socket);
    void handleHmsRequestMessage(const HttpRequestMessage *msg, inet::TcpSocket *socket);
    void handleHmsResponseMessage(const HttpResponseMessage *msg, inet::TcpSocket *socket);

    void handleRnisMessage(cMessage *msg);
    void handleRnisRequestMessage(const HttpRequestMessage *msg);
    void handleRnisResponseMessage(const HttpResponseMessage *msg);

    void handleSubscriptionRequest(SubscriptionBase *subscription, inet::TcpSocket* socket, const nlohmann::ordered_json& request);
    void handleCellChangeNotification(const nlohmann::ordered_json& request);
    void handleNotificationCallback(const nlohmann::ordered_json& request);

    void sendCellChangeSubscription(AssociateId associateId);
    void sendHostMobilitySubscription(inet::TcpSocket *socket, bool newReq = true);

    /*
     * This method is called for every element in the subscriptions_ queue.
     */
    bool manageSubscription() override;
  private:
    virtual void printAllSubscriptions();
};

}   // namespace

#endif
