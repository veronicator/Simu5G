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

package simu5g.nodes.mec.MECPlatformManager;

//
//# MECPlatformManager module
//
// This module implements the MEC platform manager (MECPM) entity within the ~MECHost.
// It serves as an interface between the MEC orchestrator and the MEC host itself. Specifically,
// it manages interactions with the ~VirtualisationInfrastructureManager to instantiate
// and terminate MEC applications upon requests from the MEC orchestrator.
//
simple MecPlatformManager
{
    parameters:
        string meoAddress = default("mecOrchestrator");  // MECOrchestrator connect address to connect to, may be left empty ("")
        int meoDestPort = default(1000);    // MECOrchestrator port number to connect to
        int meoLocalPort = default(5000); // MECOrchestrator local port number

        string localAddress = default(""); // local address: may be left empty ("")
        int localPort = default(-1); // localPort number to listen on
        int timeToLive = default(-1); // if not -1, set the TTL (IPv4) or Hop Limit (IPv6) field of sent packets to this value
        int dscp = default(-1); // if not -1, set the DSCP (IPv4/IPv6) field of sent packets to this value
        int tos = default(-1); // if not -1, set the Type Of Service (IPv4) / Traffic Class (IPv6) field of sent packets to this value

        string mecOrchestrator;
        string serviceRegistryModule = default("^.mecPlatform.serviceRegistry");
        string vimModule = default("^.vim");
        @display("i=block/control");

    gates:
        output socketOut;
        input socketIn;
}

