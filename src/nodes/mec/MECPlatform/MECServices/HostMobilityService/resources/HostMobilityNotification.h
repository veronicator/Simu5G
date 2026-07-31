//
//                  Simu5G
// 

#ifndef NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYNOTIFICATION_H_
#define NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYNOTIFICATION_H_

#include "nodes/mec/MECPlatform/MECServices/Resources/NotificationBase.h"

namespace simu5g {

class HostMobilityNotification: public NotificationBase {

    std::string mecHostName_;
    std::string mecHostAddress_;
    MacNodeId srcCellId_;
    MacNodeId trgCellId_;

public:
    HostMobilityNotification();
    virtual ~HostMobilityNotification();

    virtual nlohmann::ordered_json toJson() const override { return nullptr; }
    virtual bool fromJson(const nlohmann::ordered_json& json) override {return false; }
    virtual EventNotification* handleNotification(FilterCriteriaBase *filters, bool noCheck=false) override { return nullptr; }

//    void setSrcCellId(MacNodeId srcCellId) { srcCellId_ = srcCellId; }
//    void setTrgCellId(MacNodeId trgCellId) { trgCellId_ = trgCellId; }


    std::string getMecHostName() const { return mecHostName_; }
    std::string getMecHostAddress() const { return mecHostAddress_; }
    MacNodeId getSrcEcgi() const { return srcCellId_; }
    MacNodeId getTrgEcgi() const { return trgCellId_; }
};

} /* namespace simu5g */

#endif /* NODES_MEC_MECPLATFORM_MECSERVICES_HOSTMOBILITYSERVICE_RESOURCES_HOSTMOBILITYNOTIFICATION_H_ */
