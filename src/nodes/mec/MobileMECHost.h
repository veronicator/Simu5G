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

#ifndef __MOBILEMECHOST_H_
#define __MOBILEMECHOST_H_

#include <inet/common/INETDefs.h>
#include <inet/common/ModuleRefByPar.h>

#include "common/LteCommon.h"
#include "common/binder/Binder.h"
#include "nodes/mec/utils/MecCommon.h"

#include "stack/phy/LtePhyUe.h"
#include "stack/phy/NRPhyUe.h"

namespace simu5g {

using namespace omnetpp;

class MobileMECHost: public cModule, public cListener {

    friend class NRPhyUe;

    //------------------------------------
    //Binder module
    inet::ModuleRefByPar<Binder> binder_;
    //------------------------------------

    bool isMobilityAware = false;
public:

    int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;

//    MobileMECHost();
//    virtual ~MobileMECHost();
    // servingCellSignal_: signal emitted in NrPhyUe::doHandover()
    void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *) override;

    /*
     * handling migration of MEC applications after handover
     * send the stop message to mepm-meo
     */
    void handleMigration();

};

} /* namespace simu5g */

#endif /* __MOBILEMECHOST_H_ */
