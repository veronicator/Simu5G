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

package simu5g.simulations.NR.mec.cbrApp_standardMecHost;

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
import simu5g.nodes.mec.MECOrchestrator.MecOrchestrator;
import simu5g.nodes.mec.UALCMP.UALCMP;
import simu5g.world.radio.LteChannelControl;


network cbrApp_standardMecHost
{
    parameters:
        @display("i=block/network2;bgb=1500,1000;bgi=background/pisa");

        int numUes = default(1);
        int numBgCells = default(0);

        double playgroundSizeX @unit(m); // x size of the area the nodes are in (in meters)
        double playgroundSizeY @unit(m); // y size of the area the nodes are in (in meters)
        double playgroundSizeZ @unit(m); // z size of the area the nodes are in (in meters)

    submodules:

        routingRecorder: RoutingTableRecorder {
            @display("p=137,149;is=s");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=137,255;is=s");
        }
        channelControl: LteChannelControl {
            @display("p=135,58;is=s");
        }
        binder: Binder {
            @display("p=137,359;is=s");
        }
        carrierAggregation: CarrierAggregation {
            @display("p=137,446;is=s");
        }
        upf: Upf {
            @display("p=924,462");
        }
        iUpf: Upf {
            @display("p=741,462");
        }
        gNodeB1: gNodeB {
            @display("p=500,462;is=vl");
        }
        bgCell[numBgCells]: BackgroundCell {
            @display("p=265,695;is=vl");
        }
        ue[numUes]: NRUe {
            @display("p=341,357");
        }
        //# MEC module
        mecHost: MECHost {
            @display("p=741,303;is=l");
        }

        mecOrchestrator: MecOrchestrator {
            @display("p=924,114;is=l");
        }

        ualcmp: UALCMP {
            @display("p=924,295;i=device/smallrouter");
        }


    connections allowunconnected:

        upf.pppg++ <--> Eth10G <--> iUpf.pppg++;
        iUpf.pppg++ <--> Eth10G <--> gNodeB1.ppp;
        ualcmp.ppp++ <--> Eth10G <--> upf.filterGate;

        ualcmp.toMecOrchestrator --> mecOrchestrator.fromUALCMP;
        ualcmp.fromMecOrchestrator <-- mecOrchestrator.toUALCMP;

        mecHost.ppp++ <--> Eth10G <--> iUpf.pppg++;
}
