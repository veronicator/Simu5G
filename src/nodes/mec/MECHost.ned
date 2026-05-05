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

package simu5g.nodes.mec;

import inet.applications.contract.IApp;
import inet.common.MessageDispatcher;
import inet.linklayer.contract.IEthernetInterface;
import inet.linklayer.contract.ILoopbackInterface;
import inet.linklayer.ethernet.contract.IEthernetLayer;
import inet.linklayer.ppp.PppInterface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.contract.INetworkLayer;
import inet.transportlayer.tcp.Tcp;
import inet.transportlayer.udp.Udp;
import inet.node.ethernet.Eth10G;
import simu5g.nodes.Upf;
import simu5g.nodes.mec.MECPlatform.MECPlatform;
import simu5g.nodes.mec.MECPlatformManager.MecPlatformManager;
import simu5g.nodes.mec.VirtualisationInfrastructure.VirtualisationInfrastructure;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.BackgroundApp;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.VirtualisationInfrastructureManager;


//
// This module represents a model of a Multi-access Edge Computing (MEC) host
// belonging to a MEC system, as specified by the ETSI GS MEC 003 specifications. It runs
// MEC applications within its ~VirtualisationInfrastructure and can provide a number of
// MEC services via the MEC platform. MEC applications can be created either dynamically by
// the MEC orchestrator or statically at the initialization of the simulation. The MEC host
// possesses a configurable number of computing resources (such as CPU speed, RAM, and storage)
// and keeps track of the utilization of these resources by the MEC applications via the
// ~VirtualisationInfrastructureManager. Moreover, this module includes a User Plane
// Function (UPF) that allows it to be connected with the 5G core network.
//
module MECHost
{
    parameters:
        @networkNode();
        @display("i=device/server2;bgb=1121,750");
        @figure[applicationLayer](type=rectangle; pos=250,6; size=600,130; lineColor=#808080; cornerRadius=5; fillColor=#ffff00; fillOpacity=0.1);
        @figure[applicationLayer.title](type=text; pos=845,11; anchor=ne; text="application layer");
        @figure[transportLayer](type=rectangle; pos=250,156; size=600,130; fillColor=#ff0000; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[transportLayer.title](type=text; pos=845,161; anchor=ne; text="transport layer");
        @figure[networkLayer](type=rectangle; pos=250,306; size=600,130; fillColor=#00ff00; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[networkLayer.title](type=text; pos=845,311; anchor=ne; text="network layer");
        @figure[linkLayer](type=rectangle; pos=250,456; size=600,130; fillColor=#0000ff; lineColor=#808080; cornerRadius=5; fillOpacity=0.1);
        @figure[linkLayer.title](type=text; pos=845,461; anchor=ne; text="link layer");
        @figure[submodules];

        //# Network Layer specs
        bool hasIpv4 = default(true);
        bool hasIpv6 = default(false);
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        *.routingTableModule = default("^.ipv4.routingTable");


        //# Node specs
        string nodeType = "MECHost";						// DO NOT CHANGE
        string gateway = default("upf");

        //#	ME Host Resources
        int maxMECApps = default(0);
        double maxRam @unit("B") = default(32GB);
        double maxDisk @unit("B") = default(100TB);
        double maxCpuSpeed = default(300000); 	 // CPU's clock speed in Million Instruction per Seconds (MIPS)

        int numBGMecApp = default(0);
        int numIndependentMecApp = default(0);
        int numExtEthInterfaces = default(0);

        //# List of Base Stations associated to the MEC host
        // This is a string of comma-separated values
        object bsList = default([]);

    gates:
		//to local radio networks
        inout ppp[];		// connection with GtpEndpoint and local/external networks

        output independentMecAppOut[numIndependentMecApp];			// connection to the ME App input gate
        input independentMecAppIn[numIndependentMecApp];			// connection to the ME App output gate

        output meAppOut[];			// connection to the ME App input gate
        input meAppIn[];			// connection to the ME App output gate


    submodules:

        mecPlatform: MECPlatform {
            parameters:
                @display("p=686,73");
        }

        mecPlatformManager: MecPlatformManager {
            parameters:
                @display("p=865,63;is=m");
        }

        vim: VirtualisationInfrastructureManager {
            parameters:
                @display("p=965,216;is=m");
        }

        bgApp[numBGMecApp]: BackgroundApp {
            @display("p=964,340,row,140");
        }

        independentMecApp[numIndependentMecApp]: <> like IApp {
            @display("p=325,83,row,140");
        }


        interfaceTable: InterfaceTable {
            @display("p=61,74;is=s");
        }

        //#
        //# link-layer modules
        //#
        pppIf[sizeof(ppp)]: PppInterface {
            @display("p=734,639");
        }

        // ethernet interface used for communicating with external applications in emulation mode
        extEth[numExtEthInterfaces]: <default("ExtLowerEthernetInterface")> like IEthernetInterface {
            parameters:
                @display("p=324,639,row,150;q=txQueue");
        }
        encap: <default("EthernetEncapsulation")> like IEthernetLayer if typename != "" {
            parameters:
                registerProtocol = true;
                @display("p=326,511");
        }

        lo: <default("LoopbackInterface")> like ILoopbackInterface {
            parameters:
                @display("p=550,639,row,150");
        }

        //#
        //# Network layer module
        //#
        ipv4: <default("Ipv4NetworkLayer")> like INetworkLayer if hasIpv4 {
            parameters:
                @display("p=420,372;q=queue");
        }
        ipv6: <default("Ipv6NetworkLayer")> like INetworkLayer if hasIpv6 {
            parameters:
                @display("p=657,374;q=queue");
        }

        //#
        //# Transport layer modules and corresponding application modules
        //#
        udp: Udp {
            @display("p=460,217");
        }

        tcp: Tcp {
            @display("p=627,217");
        }

        // message dispatcher for SAP between application and transport layer
        at: MessageDispatcher {
            parameters:
                @display("p=550,146;b=600,5,,,,1");
        }
        // message dispatcher for SAP between transport and network layer
        tn: MessageDispatcher {
            parameters:
                @display("p=550,300;b=600,5,,,,1");
        }
        // message dispatcher for SAP to link layer
        nl: MessageDispatcher {
            parameters:
                @display("p=550,446;b=600,5,,,,1");
        }

    connections allowunconnected:

        //#
        //# internal connection between GtpEndpoint and VirtualisationInfrastrcuture
        //#

        //MEC host internal connection
        for i=0..numIndependentMecApp-1 {
            independentMecApp[i].socketIn <-- at.out++;
            independentMecApp[i].socketOut --> at.in++;
        }

        for i=0..mecPlatform.numMecServices {
            at.out++ --> mecPlatform.virtInfr$i++;
            at.in++ <-- mecPlatform.virtInfr$o++;
        }

        mecPlatformManager.socketOut --> at.in++;
        mecPlatformManager.socketIn <-- at.out++;

		//#
        //# App layer to transport layer connections
        //#
        at.out++ --> udp.appIn;
        at.in++ <-- udp.appOut;

        at.out++ --> tcp.appIn;
        at.in++ <-- tcp.appOut;

        //#
        //# Transport layer to network layer connections
        //#
        udp.ipOut --> tn.in++;
        udp.ipIn <-- tn.out++;

        tcp.ipOut --> tn.in++;
        tcp.ipIn <-- tn.out++;


        ipv4.ifIn <-- nl.out++ if hasIpv4;
        ipv4.ifOut --> nl.in++ if hasIpv4;

        ipv4.transportIn <-- tn.out++ if hasIpv4;
        ipv4.transportOut --> tn.in++ if hasIpv4;

        //#
        //# Network layer to link layer connections
        //#
        ipv6.ifIn <-- nl.out++ if hasIpv6;
        ipv6.ifOut --> nl.in++ if hasIpv6;

        ipv6.transportIn <-- tn.out++ if hasIpv6;
        ipv6.transportOut --> tn.in++ if hasIpv6;

        for i=0..sizeof(ppp)-1 {
            pppIf[i].upperLayerOut --> nl.in++;
            pppIf[i].upperLayerIn <-- nl.out++;
            pppIf[i].phys <--> ppp++;				//  Eth10G <-->upf_mec.filterGate;
        }

        for i=0..numExtEthInterfaces-1 {
            encap.lowerLayerOut --> extEth[i].upperLayerIn;
            encap.lowerLayerIn <-- extEth[i].upperLayerOut;
        }
        encap.upperLayerOut --> nl.in++ if numExtEthInterfaces != 0;
        encap.upperLayerIn <-- nl.out++ if numExtEthInterfaces != 0;

        nl.out++ --> lo.upperLayerIn;
        lo.upperLayerOut --> nl.in++;

}
