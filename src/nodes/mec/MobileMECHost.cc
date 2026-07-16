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

#include "nodes/mec/MobileMECHost.h"

#include "nodes/mec/MECPlatformManager/MecPlatformManager.h"
#include "nodes/mec/VirtualisationInfrastructureManager/VirtualisationInfrastructureManager.h"

namespace simu5g {

Define_Module(MobileMECHost);


void MobileMECHost::initialize(int stage)
{
    cModule::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL)
        binder_.reference(this, "binderModule", true);

    // avoid multiple initializations
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        subscribe(NRPhyUe::handoverServingCellSignal_, this);
    }
}

void MobileMECHost::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    if (hasPar("isMobilityAware")) {
        isMobilityAware = par("isMobilityAware").boolValue();
    }

    MecPlatformManager *mepm = check_and_cast<MecPlatformManager *>(this->getSubmodule("mecPlatformManager"));
    if (isMobilityAware && signalID == NRPhyUe::handoverServingCellSignal_) {
        binder_->setStartMECHostHandover(simTime());
        mepm->migrateMecAppsReq();
    }
}

} /* namespace simu5g */
