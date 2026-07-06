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

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.common.L3Address;
import common.LteCommon;
cplusplus {{
#include "nodes/mec/MECPlatform/MECServices/RNIService/resources/AssociateId.h"
#include "nodes/mec/utils/MecCommon.h"
#include <vector>

typedef std::vector<std::string> AppInstanceIdVector;
}}

class AppInstanceIdVector { @existingClass; }

namespace simu5g;

class AssociateId
{
    @existingClass;
    @opaque;
}

struct SockAddr {
    @existingClass;
    @opaque;
}
    

message MECOrchestratorMessage {
    int contextId; // identifier of the ueMecApp 
    int requestId; // used to inform the UALCMP to which request the response refers or to which UeId in case of migration
    string type; // trigger of mecApp setup or termination 	 
    bool success;   
}

class MECAppMessage extends inet::FieldsChunk {
    chunkLength = inet::B(8);
        
    //type of message
    string type;
    
}

class CreateAppMessage extends MECAppMessage {

    unsigned int sno;
    simtime_t timestamp;
    
    //communication informations
    string sourceAddress;
    int	   sourcePort;   // added by Alessandro Noferi
    string destinationAddress;
    int	   destinationPort;   // added by Alessandro Noferi
    string destinationMecAppAddress;
    int	   destinationMecAppPort;   // added by Alessandro Noferi
    
    //instantiation informations
    string MEModuleType;			//path where to find the cModule of the MEApp to instantiate
    string MEModuleName;			//module class name of the MEApp to instantiate
    int contextId;
    
    //identification information
    int ueAppID;
    
    //resources required
    double requiredDisk;
    double requiredRam;
    double requiredCpu;
    
    //service required
    string requiredService;
    //service provded
    string providedService;
    
}

class DeleteAppMessage extends MECAppMessage {
   
    unsigned int sno;
    simtime_t timestamp;
    
    //communication informations
    string sourceAddress;
    int	   sourcePort;   // added by Alessandro Noferi
    string destinationAddress;
    int	   destinationPort;   // added by Alessandro Noferi
    string destinationMecAppAddress;
    int	   destinationMecAppPort;   // added by Alessandro Noferi
    
    
    //instantiation informations
    string MEModuleType;			//path where to find the cModule of the MEApp to instantiate
    string MEModuleName;			//module class name of the MEApp to instantiate
    
    //identification information
    int ueAppID;
    
    //resources required
    unsigned int requiredDisk;
    unsigned int requiredRam;
    double requiredCpu;
    
    //service required
    string requiredService;
}

class MigrateAppMessage extends MECAppMessage {
    
   	int mepmId;
   	string mepmAddress;
   
    unsigned int sno;
    simtime_t timestamp;

    int contextId;
    
}

class MigrateAppAckMessage extends MECAppMessage {
    
   	int mepmId;
   	string mepmAddress;
   	
    int contextId;
   
    unsigned int sno;
    simtime_t timestamp;
    
//    MecAppInstanceInfo appInfo;
    bool status;
    string instanceId;
    int moduleId;		// mec app instance omnet module id
    
    SockAddr endPoint;
    
}

class TriggerMigrationAppMessage extends MECAppMessage {
    // msg type: MIGRATE_MEAPP
   	
   	AssociateId associateId;	// type, value
   	AppInstanceIdVector appInstanceIds;	//mec app instance to migrate
   	
   	MacNodeId srcCellId;
   	MacNodeId trgCellId;
   	
   	
}

