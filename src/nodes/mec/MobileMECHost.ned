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
import inet.node.ethernet.Eth10G;
import inet.mobility.contract.IMobility;
import simu5g.nodes.Upf;
import simu5g.nodes.mec.MECPlatform.MECPlatform;
import simu5g.nodes.mec.MECPlatformManager.MecPlatformManager;
import simu5g.nodes.mec.VirtualisationInfrastructure.VirtualisationInfrastructure;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.BackgroundApp;
import simu5g.nodes.mec.VirtualisationInfrastructureManager.VirtualisationInfrastructureManager;
import simu5g.stack.ICellularNic;


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
module MobileMECHost
{
    parameters:
        @networkNode();
        @display("i=device/server2;bgb=1000,500");

        //# Node specs
        string nodeType = "MECHost";						// DO NOT CHANGE
        string gateway = default("");

//        int masterId @mutable = default(0);
//        int macNodeId @mutable = default(0); // TODO: this is not a real parameter
//        int macCellId @mutable = default(0); // TODO: this is not a real parameter
//        
//        int nrMasterId @mutable = default(0);
//        int nrMacNodeId @mutable = default(0); // TODO: this is not a real parameter
//        int nrMacCellId @mutable = default(0); // TODO: this is not a real parameter

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

//		//# Network Layer specs
//        bool hasIpv4 = default(true);
//        bool hasIpv6 = default(false);
//        *.interfaceTableModule = default(absPath(".virtualisationInfrastructure.interfaceTable"));
//        *.routingTableModule = default("^.virtualisationInfrastructure.ipv4.routingTable");
        *.mobilityModule = default(absPath(".mobility"));

    gates:
		//to local radio networks
        inout ppp[];

        //to external networks (internet)
        inout pppIf[];

//        //cellularNic to VI dispatcher
//        input viIn;
//        output viOut;

        // input gate for cellularNic
        input nrRadioIn @directIn;     // connection to master

    submodules:

        virtualisationInfrastructure: VirtualisationInfrastructure {
            parameters:
                @display("p=500,400");
                gateway = parent.gateway;
        }

        mecPlatform: MECPlatform {
            parameters:
                @display("p=573,158");
        }

        mecPlatformManager: MecPlatformManager {
            parameters:
                @display("p=865,116;is=m");
        }

        vim: VirtualisationInfrastructureManager {
            parameters:
                @display("p=865,224;is=m");
        }

        // UPF module used to connect the MEC host to the 5G Core Network (either a BS or a UPF)
//        upf_mec: Upf {
//            parameters:
//                @display("p=150,400");
//                nodeType = "UPF_MEC";
//                gateway = parent.gateway;
//        }

        bgApp[numBGMecApp]: BackgroundApp {
            @display("p=865,326,row,140");
        }

        independentMecApp[numIndependentMecApp]: <> like IApp {
            @display("p=371,156,row,140");
        }

        mobility: <default("StationaryMobility")> like IMobility {
            @display("p=61,126;is=s");
        }

//        cellularNic: <default("NRNicUe")> like ICellularNic {
//            nodeType = parent.nodeType;
//            @display("p=193,399");
//        }

    connections allowunconnected:

//        for i=0..sizeof(ppp)-1 {
//            ppp++ <--> upf_mec.pppg++;
//        }

        for i=0..sizeof(pppIf)-1 {
            //#
            //# external connection to external networks
            //#
            virtualisationInfrastructure.ppp++ <--> pppIf++;
        }

        //#
        //# internal connection between GtpEndpoint and VirtualisationInfrastrcuture
        //#
//        virtualisationInfrastructure.ppp++ <--> Eth10G <--> upf_mec.filterGate;

        for i=0..numIndependentMecApp-1 {
            independentMecApp[i].socketIn <-- virtualisationInfrastructure.independentMecAppOut[i];
            independentMecApp[i].socketOut --> virtualisationInfrastructure.independentMecAppIn[i];
        }

        for i=0..mecPlatform.numMecServices {
            virtualisationInfrastructure.mecPlatform++ <--> mecPlatform.virtInfr++;
        }

//        cellularNic.upperLayerOut --> virtualisationInfrastructure.nicIn;
//        cellularNic.upperLayerIn <-- virtualisationInfrastructure.nicOut;
//
        virtualisationInfrastructure.nrRadioIn <-- nrRadioIn;
}
