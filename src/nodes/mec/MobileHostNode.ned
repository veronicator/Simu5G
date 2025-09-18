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

package simu5g.nodes.mec;

import inet.applications.contract.IApp;
import inet.common.MessageDispatcher;
import inet.common.packet.recorder.PcapRecorder;
import inet.linklayer.contract.IEthernetInterface;
import inet.linklayer.contract.ILoopbackInterface;
import inet.linklayer.ethernet.contract.IEthernetLayer;
import inet.mobility.contract.IMobility;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.contract.INetworkLayer;
import inet.transportlayer.contract.ISctp;
import inet.transportlayer.contract.ITcp;
import inet.transportlayer.contract.IUdp;
import simu5g.corenetwork.statsCollector.NRueStatsCollector;
import simu5g.corenetwork.statsCollector.UeStatsCollector;
import simu5g.stack.ICellularNic;
import simu5g.nodes.mec.MECPlatform.MECPlatform;
import simu5g.nodes.mec.MECPlatformManager.MecPlatformManager;
import simu5g.nodes.mec.VirtualisationInfrastructure.VirtualisationInfrastructure;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.BackgroundApp;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.VirtualisationInfrastructureManager;


module MobileHostNode
{
    parameters:
        @networkNode();
        @display("i=device/pocketpc;bgb=1219,700");
        @figure[applicationLayer](type=rectangle; pos=250,6; size=600,130; lineColor=#808080; cornerRadius=5; fillColor=#ffff00; fillOpacity=0.1);
        @figure[applicationLayer.title](type=text; pos=845,11; anchor=ne; text="application layer");
        @figure[transportLayer](type=rectangle; pos=250,156; size=600,130; fillColor=#ff0000; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[transportLayer.title](type=text; pos=845,161; anchor=ne; text="transport layer");
        @figure[networkLayer](type=rectangle; pos=250,306; size=600,130; fillColor=#00ff00; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[networkLayer.title](type=text; pos=845,311; anchor=ne; text="network layer");
        @figure[linkLayer](type=rectangle; pos=250,456; size=600,130; fillColor=#0000ff; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[linkLayer.title](type=text; pos=845,461; anchor=ne; text="link layer");
        @figure[submodules];

		bool isMobile = default(false);
		
        //# Apps
        int numApps = default(0);   // no of apps.
                                    // Specify the app types in INI file with app[0..1].typename="UdpVideoStreamClient" syntax

        //# Transport layer
        bool hasUdp = default(firstAvailableOrEmpty("Udp") != "");
        bool hasTcp = default(firstAvailableOrEmpty("Tcp", "TcpLwip", "TcpNsc") != "");
        bool hasSctp = default(false);

        //# Node specs
        string nodeType = "UE";  // DO NOT CHANGE
        int masterId @mutable = default(0);
        int macNodeId @mutable = default(0); // TODO: this is not a real parameter
        int macCellId @mutable = default(0); // TODO: this is not a real parameter

        int nrMasterId @mutable = default(0);
        int nrMacNodeId @mutable = default(0); // TODO: this is not a real parameter
        int nrMacCellId @mutable = default(0); // TODO: this is not a real parameter


        string gateway = default("");

         //#	ME Host Resources
        int maxMECApps = default(0);
        double maxRam @unit("B") = default(32GB);
        double maxDisk @unit("B") = default(100TB);
        double maxCpuSpeed = default(300000); 	 // CPU's clock speed in Million Instruction per Seconds (MIPS)

        int numBGMecApp = default(0);
        int numIndependentMecApp = default(0);

        //# List of Base Stations associated to the MEC host
        // This is a string of comma-separated values
        object bsList = default([]);

        //# Network Layer specs
        bool hasIpv4 = default(true);
        bool hasIpv6 = default(false);
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        *.routingTableModule = default("^.ipv4.routingTable");
        *.mobilityModule = default(absPath(".mobility"));

        //# External Ethernet interfaces
        int numEthInterfaces = default(0);
        string extHostAddress = default("");

        //# Loopback interfaces
        int numLoInterfaces = default(1);

        //# Num of PcapRecorders
        int numPcapRecorders = default(0); // no of PcapRecorders

        bool hasRNISupport = default(false);
        cellularNic.mac.collectorModule = hasRNISupport ? "^.^.ueCollector" : "";
        cellularNic.phy.hasCollector = hasRNISupport;

        cellularNic.nrMac.collectorModule = hasRNISupport ? "^.^.NRueCollector" : "";
        cellularNic.nrPhy.hasCollector = hasRNISupport;

    gates:
        input radioIn @directIn;     // connection to master
        input nrRadioIn @directIn;

		 //from MecHost
//        //to local radio networks
//        inout ppp[];
//
//        //to external networks (internet)
//        inout pppIf[];

    submodules:
        interfaceTable: InterfaceTable {
            @display("p=127,257;is=s");
        }
        mobility: <default("StationaryMobility")> like IMobility {
            @display("p=127,172;is=s");
        }

		//*
//        virtualisationInfrastructure: VirtualisationInfrastructure {
//            parameters:
//                @display("p=980,213");
//                gateway = parent.gateway;
//        }

        mecPlatform: MECPlatform {
            @display("p=981,63");
        }
        mecPlatformManager: MecPlatformManager {
            parameters:
                @display("p=1139,96;is=m");
        }

        vim: VirtualisationInfrastructureManager {
            parameters:
                @display("p=1097,446;is=m");
        }

        bgApp[numBGMecApp]: BackgroundApp {
            @display("p=1097,301,row,140");
        }

        independentMecApp[numIndependentMecApp]: <> like IApp {
            @display("p=696,41,row,140");
        }
        //*//


        //# layer submodules (must be ordered from lower to upper layers)

        //# link layer
        lo[numLoInterfaces]: <default("LoopbackInterface")> like ILoopbackInterface {
            parameters:
                @display("p=503,590,row,150");
        }
        cellularNic: <default("NRNicUe")> like ICellularNic {
            nodeType = parent.nodeType;
            @display("p=355,589");
        }

        //# ethernet interface used for communicating with external applications in emulation mode
        eth[numEthInterfaces]: <default("ExtLowerEthernetInterface")> like IEthernetInterface {
            parameters:
                @display("p=674,590,row,150;q=txQueue");
        }
        encap: <default("EthernetEncapsulation")> like IEthernetLayer if typename != "" {
            parameters:
                registerProtocol = true;
                @display("p=674,500");
        }

        //# network layer
        ipv4: <default("Ipv4NetworkLayer")> like INetworkLayer if hasIpv4 {
            parameters:
                @display("p=375,376;q=queue");
        }
        ipv6: <default("Ipv6NetworkLayer")> like INetworkLayer if hasIpv6 {
            parameters:
                @display("p=525,376;q=queue");
        }

        //# transport layer
        udp: <default(firstAvailableOrEmpty("Udp"))> like IUdp if hasUdp {
            parameters:
                @display("p=375,226");
        }
        tcp: <default(firstAvailableOrEmpty("Tcp", "TcpLwip", "TcpNsc"))> like ITcp if hasTcp {
            parameters:
                @display("p=525,226");
        }
        sctp: <default(firstAvailableOrEmpty("Sctp"))> like ISctp if hasSctp {
            parameters:
                @display("p=675,226");
        }

        //# app layer
        app[numApps]: <> like IApp {
            @display("p=374,72,row,140");
        }

        //# message dispatcher for SAP between application and transport layer
        at: MessageDispatcher {
            parameters:
                @display("p=550,146;b=600,5,,,,1");
        }
        //# message dispatcher for SAP between transport and network layer
        tn: MessageDispatcher {
            parameters:
                @display("p=550,300;b=600,5,,,,1");
        }
        //# message dispatcher for SAP to link layer
        nl: MessageDispatcher {
            parameters:
                @display("p=550,446;b=600,5,,,,1");
        }
        //# PcapRecorders
        pcapRecorder[numPcapRecorders]: PcapRecorder {
            @display("p=126,346,r,10");
        }

        //# UeStatsCollector - for MEC
        ueCollector: UeStatsCollector if hasRNISupport {
            @display("p=126,588;is=s");
        }
        //# NR UeStatsCollector - for MEC
        NRueCollector: NRueStatsCollector if hasRNISupport {
            @display("p=127,506;is=s");
        }

    connections allowunconnected:

        //#
        //# Internal Tcp/Udp applications connections with transport layer
        //#

        for i=0..numApps-1 {
            app[i].socketOut --> at.in++;
            app[i].socketIn <-- at.out++;
        }

        at.out++ --> udp.appIn if hasUdp;
        at.in++ <-- udp.appOut if hasUdp;

        at.out++ --> tcp.appIn if hasTcp;
        at.in++ <-- tcp.appOut if hasTcp;

        at.out++ --> sctp.appIn if hasSctp;
        at.in++ <-- sctp.appOut if hasSctp;

        //MEC host internal connection
        for i=0..numIndependentMecApp-1 {
            at.out++ --> independentMecApp[i].socketIn;
            at.in++ <-- independentMecApp[i].socketOut;
        }


        for i=0..mecPlatform.numMecServices {
            at.out++ --> mecPlatform.virtInfr$i++;
            at.in++ <-- mecPlatform.virtInfr$o++;
        }

        //#
		//# Internal transport layer connections to network layer
		//#

        udp.ipOut --> tn.in++ if hasUdp;
        udp.ipIn <-- tn.out++ if hasUdp;

        tcp.ipOut --> tn.in++ if hasTcp;
        tcp.ipIn <-- tn.out++ if hasTcp;

        sctp.ipOut --> tn.in++ if hasSctp;
        tn.out++ --> sctp.ipIn if hasSctp;

        ipv4.ifIn <-- nl.out++ if hasIpv4;
        ipv4.ifOut --> nl.in++ if hasIpv4;

        ipv4.transportIn <-- tn.out++ if hasIpv4;
        ipv4.transportOut --> tn.in++ if hasIpv4;

        ipv6.ifIn <-- nl.out++ if hasIpv6;
        ipv6.ifOut --> nl.in++ if hasIpv6;

        ipv6.transportIn <-- tn.out++ if hasIpv6;
        ipv6.transportOut --> tn.in++ if hasIpv6;

        tn.out++ --> nl.in++;
        tn.in++ <-- nl.out++;

        //#
        //# Internal link layer connections to network layer
        //#

        cellularNic.upperLayerOut --> nl.in++;
        cellularNic.upperLayerIn <-- nl.out++;

        for i=0..numEthInterfaces-1 {
            encap.lowerLayerOut --> eth[i].upperLayerIn;
            encap.lowerLayerIn <-- eth[i].upperLayerOut;
        }
        encap.upperLayerOut --> nl.in++ if numEthInterfaces != 0;
        encap.upperLayerIn <-- nl.out++ if numEthInterfaces != 0;

        for i=0..numLoInterfaces-1 {
            nl.out++ --> lo[i].upperLayerIn;
            lo[i].upperLayerOut --> nl.in++;
        }

        //#
        //# Connections between LTE/NR NIC and the radio medium
        //#
        cellularNic.nrRadioIn <-- nrRadioIn;
        cellularNic.radioIn <-- radioIn;
}
