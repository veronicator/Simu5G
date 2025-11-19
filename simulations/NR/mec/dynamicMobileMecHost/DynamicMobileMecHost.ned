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

package simu5g.simulations.NR.mec.dynamicMobileMecHost;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import simu5g.common.binder.Binder;
import simu5g.common.carrierAggregation.CarrierAggregation;
import simu5g.nodes.NR.NRUe;
import simu5g.nodes.NR.gNodeB;
import simu5g.nodes.Upf;
import simu5g.nodes.backgroundCell.BackgroundCell;
import simu5g.nodes.mec.MECHost;
import simu5g.nodes.mec.MobileHostNode;
import simu5g.nodes.mec.MECOrchestrator.MecOrchestrator;
import simu5g.nodes.mec.UALCMP.UALCMP;
import simu5g.world.radio.LteChannelControl;


network DynamicMobileMecHost
{
    parameters:
        @display("i=block/network2;bgb=1500,1000;bgi=background/pisa");

        int numUes = default(1);
        int numBgCells = default(0);
        double routersDelay @unit(m) = default(0m);
//        volatile double routersDelay @unit(m) = default(normal(2m, 0.8m));

//        @display("i=block/network2;bgb=1588,1078;");
    submodules:
        channelControl: LteChannelControl {
            @display("p=50,25;is=s");
        }
        routingRecorder: RoutingTableRecorder {
            @display("p=50,75;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,125");
        }
        binder: Binder {
            @display("p=50,175;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=50,258;is=s");
        }
        upf: Upf {
            @display("p=685,116");
        }
        iUpf1: Upf {
            @display("p=495,238");
        }
        iUpf2: Upf {
            @display("p=685,238");
        }
        iUpf3: Upf {
            @display("p=891,238");
        }
        gnb1: gNodeB {
            @display("p=186,454;is=vl");
        }
        gnb2: gNodeB {
            @display("p=684,454;is=vl");
        }
        gnb3: gNodeB {
            @display("p=1201,454;is=vl");
        }
        bgCell[numBgCells]: BackgroundCell {
            @display("p=80,723;is=vl");
        }
        ue[numUes]: NRUe {
            @display("p=283,684");
        }

        //        //# MEC modules
                mecHost1: MECHost {
                    @display("p=343,238");
                }
        //        mecHost2: MECHost {
        //            @display("p=590,238");
        //        }
        //        mecHost3: MECHost {
        //            @display("p=793,238");
        //        }
        mobileMecHost: MobileHostNode {
            @display("p=253,589");
        }
        mecOrchestrator: MecOrchestrator {
            @display("p=835,35");
        }
        ualcmp: UALCMP {
            @display("p=685,35");
        }

    connections:

        //# 5G Core Network connections
        upf.pppg++ <--> Eth10G <--> iUpf1.pppg++;
        upf.pppg++ <--> Eth10G <--> iUpf2.pppg++;
        upf.pppg++ <--> Eth10G <--> iUpf3.pppg++;
        iUpf1.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf2.pppg++ <--> Eth10G <--> gnb2.ppp;
        iUpf3.pppg++ <--> Eth10G <--> gnb3.ppp;
//        iUpf2.pppg++ <--> Eth10G {  length = parent.routersDelay*2e5; } <--> iUpf1.pppg++;	// channel delay


        //# MEC-related connections
        ualcmp.ppp++ <--> Eth10G <--> upf.filterGate;
        ualcmp.toMecOrchestrator --> mecOrchestrator.fromUALCMP;
        ualcmp.fromMecOrchestrator <-- mecOrchestrator.toUALCMP;
        mecHost1.ppp++ <--> Eth10G <--> iUpf1.pppg++;
//        mecHost2.ppp++ <--> Eth10G <--> iUpf2.pppg++;
//        mecHost3.ppp++ <--> Eth10G <--> iUpf3.pppg++;

        //# X2 connections
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;
        gnb2.x2++ <--> Eth10G <--> gnb3.x2++;

}
