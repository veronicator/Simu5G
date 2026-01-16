//
//                  Simu5G
//

package simu5g.simulations.NR.mec.appMigration_mobileMECHost;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.networklayer.ipv4.RoutingTableRecorder;
import inet.node.ethernet.Eth10G;
import inet.node.internetcloud.InternetCloud;
import ned.DatarateChannel;
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


network appMigration_mobileMECHost
{
    parameters:
        @display("i=block/network2;bgb=1500,1000;bgi=background/pisa");

        int numUes = default(1);
        int numBgCells = default(0);

    types:
        channel C extends DatarateChannel
        {
            delay = 10ms;
            datarate = 5Mbps;
        }
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
            @display("p=556,347");
        }
        iUpf2: Upf {
            @display("p=853,349");
        }
        gnb1: gNodeB {
            @display("p=450,500;is=vl");
        }
        gnb2: gNodeB {
            @display("p=950,500;is=vl");
        }
        bgCell[numBgCells]: BackgroundCell {
            @display("p=80,723;is=vl");
        }
        ue[numUes]: NRUe {
            @display("p=195,522");
        }

        // MEC modules
        mobileMecHost1: MobileHostNode {
            @display("p=315,695");
        }
        mobileMecHost2: MobileHostNode {
            @display("p=287,351");
        }
        mecOrchestrator: MecOrchestrator {
            @display("p=835,35");
        }
        ualcmp: UALCMP {
            @display("p=685,35");
        }

        internet: InternetCloud {
            @display("p=715,203;is=vl");
        }

    connections:

        //# 5G Core Network connections
        upf.pppg++ <--> C <--> internet.pppg++;
        iUpf1.pppg++ <--> C <--> internet.pppg++;
        iUpf2.pppg++ <--> C <--> internet.pppg++;

//        upf.pppg++ <--> Eth10G <--> iUpf1.pppg++;
//        upf.pppg++ <--> Eth10G <--> iUpf2.pppg++;

        iUpf1.pppg++ <--> Eth10G <--> gnb1.ppp;
        iUpf2.pppg++ <--> Eth10G <--> gnb2.ppp;
//        iUpf2.pppg++ <--> Eth10G {  length = parent.routersDelay*2e5; } <--> iUpf1.pppg++;	// channel delay


        //# MEC-related connections
        ualcmp.ppp++ <--> Eth10G <--> upf.filterGate;
        ualcmp.toMecOrchestrator --> mecOrchestrator.fromUALCMP;
        ualcmp.fromMecOrchestrator <-- mecOrchestrator.toUALCMP;

        //# X2 connections
        gnb1.x2++ <--> Eth10G <--> gnb2.x2++;

}
