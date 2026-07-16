//
//                  Simu5G
// 

import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.common.L3Address;
import common.LteCommon;
import nodes.mec.MECOrchestrator.MECOMessages.MECOrchestratorMessages;


namespace simu5g;

class MecServiceMessage extends MECMessage {
//    chunkLength = inet::B(8);
//    string type;
    
    string mecHostName;
    inet::L3Address mepmAddress;
}

class RegisterMecHostMessage extends MecServiceMessage {
    MacNodeId cellIds[];	// MacNodeId of cells associated to the Mec host
}

class UpdateServingAreaMessage extends MecServiceMessage {
    MacNodeId srcCellId;	// id of the cell not covered anymore
    MacNodeId trgCellId;	// id of the new cell associated to Mec host
}

class UpdateServingAreaResponse extends MecServiceMessage {
    bool status;			// false if src and trg cell are in same mec host serving area (or on error)
    string response;		//
    
    MacNodeId srcCellId;	// id of the cell not covered anymore
    MacNodeId trgCellId;	// id of the new cell associated to Mec host
}



