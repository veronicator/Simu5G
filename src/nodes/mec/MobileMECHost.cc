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

    // avoid multiple initializations
    if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        subscribe(NRPhyUe::handoverServingCellSignal_, this);
    }
}

void MobileMECHost::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *)
{
    std::cout << "receive signal mec host" << endl;
    if (hasPar("doMigration")) {
        doMigration = par("doMigration").boolValue();
    }
    MecPlatformManager *mepm = check_and_cast<MecPlatformManager *>(this->getSubmodule("mecPlatformManager"));
    if (doMigration && signalID == NRPhyUe::handoverServingCellSignal_) {
        std::cout << "receiveSignal servingCellSignal_: " << signalID << endl;
        mepm->migrateMEApps();
    }
}

} /* namespace simu5g */
