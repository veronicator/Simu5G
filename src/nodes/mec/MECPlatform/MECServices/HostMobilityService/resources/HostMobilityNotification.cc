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

#include "HostMobilityNotification.h"

namespace simu5g {

HostMobilityNotification::HostMobilityNotification() {
    notificationType_ = "HostMobilityNotification";

}

HostMobilityNotification::~HostMobilityNotification() {
    // TODO Auto-generated destructor stub
}

nlohmann::ordered_json HostMobilityNotification::toJson() const {
    nlohmann::ordered_json val;
    val["notificationType"] = notificationType_;

    val["timeStamp"] = timestamp_.toJson();
    val["mecHostName"] = mecHostName_;
    val["mecHostAddress"] = mecHostAddress_;
    val["srcEcgi"]["cellId"] = srcCellId_;
    val["trgEcgi"]["cellId"] = trgCellId_;

    val["_links"]["href"] = links_;
    return val;
}

bool HostMobilityNotification::fromJson(const nlohmann::ordered_json& json) {
    EV << "HostMobilityNotification::from json" << endl;
        bool result = NotificationBase::fromJson(json);

        if(notificationType_ != json["notificationType"])
        {
            EV << "HostMobilityNotification::fromJson - notificationType mismatch" << endl;
            return false;
        }

        if (!json.contains("srcEcgi") || !json.contains("trgEcgi") || !json.contains("mecHostName") || !json.contains("mecHostAddress"))
        {
            EV << "HostMobilityNotification::fromJson - missing mandatory fields" << endl;
            return false;
        }

        mecHostName_ = json["mecHostName"];
        mecHostAddress_ = json["mecHostAddress"];
        srcCellId_ = json["srcEcgi"]["cellId"];
        trgCellId_ = json["trgEcgi"]["cellId"];

        return result;
}


} /* namespace simu5g */
