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

package simu5g.nodes.mec.MECOrchestrator;
import inet.common.MessageDispatcher;
import inet.linklayer.contract.IEthernetInterface;
import inet.linklayer.ppp.PppInterface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.contract.INetworkLayer;
import inet.transportlayer.tcp.Tcp;
import inet.transportlayer.udp.Udp;


module MecOrchestrator
{
    parameters:
        @display("i=device/mainframe;bgb=1006,692");
        @networkNode();

        string binderModule = default("binder");

        string selectionPolicy @enum(MecHostBased,MecServiceBased,AvailableResourcesBased, MigrationSelectionBased) = default("MecServiceBased"); // available policies: "MecHostBased", "MecServiceBased", "AvailableResourcesBased"
        int mecHostIndex = default(0); // to be used with the MecHostBased policybased
        object mecHostList = default([]);
        object mecApplicationPackageList = default([]);

        // times to simulate lifecycle operations
        double onboardingTime @unit(s) = default(50ms);
        double instantiationTime @unit(s) = default(50ms);
        double terminationTime @unit(s) = default(50ms);

        bool hasIpv4 = default(true);
        bool hasIpv6 = default(false);
        *.interfaceTableModule = default(absPath(".interfaceTable"));

//        int numPppInterfaces = default(1);

    gates:
        //to external networks (internet)
        inout ppp[];

        output toUALCMP;
        input fromUALCMP;
   	
   	submodules:
   		
   		interfaceTable: InterfaceTable {
            @display("p=61,74;is=s");
        }

        //#
        //# ppp modules
        //#
        pppIf: PppInterface {
            @display("p=446,506");
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
		// =============== UDP ============== //
        mecOrchestratorApp: MecOrchestratorApp {
            binderModule = parent.binderModule;

   		    selectionPolicy = parent.selectionPolicy;
   		    mecHostIndex = parent.mecHostIndex;
   		    mecHostList= parent.mecHostList.dup();
   		    mecApplicationPackageList = parent.mecApplicationPackageList.dup();
   		
   		    onboardingTime = parent.onboardingTime;
   		    instantiationTime = parent.instantiationTime;
   		    terminationTime = parent.terminationTime;

            @display("p=549,74,row");
        }

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
        //# MEO to transport layer connections
        //#
        mecOrchestratorApp.socketOut --> at.in++;
        mecOrchestratorApp.socketIn <-- at.out++;

        mecOrchestratorApp.toUALCMP --> toUALCMP;
        mecOrchestratorApp.fromUALCMP <-- fromUALCMP;

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

        ipv6.ifIn <-- nl.out++ if hasIpv6;
        ipv6.ifOut --> nl.in++ if hasIpv6;

        ipv6.transportIn <-- tn.out++ if hasIpv6;
        ipv6.transportOut --> tn.in++ if hasIpv6;

        pppIf.upperLayerOut --> nl.in++;
        pppIf.upperLayerIn <-- nl.out++;
        pppIf.phys <--> ppp++;

}
