/**
 * @file
 * Analyze vreg and memoiry usage in IR. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


struct LineInfo {
    int       num;
    ir_node*  node;
};


struct RegAccessInfo {
    LineInfo  line;
    ir_node*  reg;
};


enum MemAccessType {
    LOAD,
    STORE
};


struct MemAccessInfo {
    LineInfo       line;
    ir_node*       baseVreg;
    int            offset;
    ir_node*       movVreg;
    MemAccessType  type;
};



struct CodeAnalysis {
 
    // analyze virtual register usage
    // - key (int): id of vreg that was used
    // - value:     sequence of accesses
    std::map < int, std::vector < RegAccessInfo > > vregReads;
    std::map < int, std::vector < RegAccessInfo > > vregWrites;
    

    // virtual register ranges
    // - key (int): id of vreg that was used
    // - value:     line with marker
    std::map < int, LineInfo > vregRequests;
    std::map < int, LineInfo > vregClears;
    

    // loop open markers ordered by line number
    //  map: line number -> loop open LineInfo
    std::map < int, LineInfo > loopOpenMarkers;
    
    // loop close markers ordered by loop id
    //  map: loop id -> loop close LineInfo
    std::map < int, LineInfo > loopCloseMarkers;


    // analyze memory access 
    // - key (int): virtual register used as base address for multiple reads/writes
    // - value:     sequence of loads / stores
    std::map < int, std::vector < MemAccessInfo > > memReads;
    std::map < int, std::vector < MemAccessInfo > > memWrites;


    // - key is the line number of the instruction
    std::map < int, std::vector < MemAccessInfo > > orderedMemAccess;
};







bool isRead ( CodeAnalysis&  analysis, 
              int            vregId ) {

    return analysis.vregReads.count ( vregId ) > 0;
}


bool isRead ( CodeAnalysis& analysis, ir_node* vreg ) {
    return isRead ( analysis, vreg->id );
}


RegAccessInfo& firstRead ( CodeAnalysis& analysis, int vregId ) {
    M_Assert ( isRead ( analysis, vregId ), "Should only be called if there was access." );
    return analysis.vregReads [ vregId ].front();
}


RegAccessInfo& firstRead ( CodeAnalysis& analysis, ir_node* vreg ) {
    return firstRead ( analysis, vreg->id );
}


RegAccessInfo& lastRead ( CodeAnalysis& analysis, int vregId ) {
    M_Assert ( isRead ( analysis, vregId ), "Should only be called if there was access." );
    return analysis.vregReads [ vregId ].back();
}


RegAccessInfo& lastRead ( CodeAnalysis& analysis, ir_node* vreg ) {
    return lastRead ( analysis, vreg->id );
}


RegAccessInfo& firstWrite ( CodeAnalysis& analysis, int vregId ) {
    return analysis.vregWrites [ vregId ].front();
}


RegAccessInfo& firstWrite ( CodeAnalysis&  analysis, 
                            ir_node*       vreg ) {

    return firstWrite ( analysis, vreg->id );
}


static void recordMarkers ( ir_node*       node, 
                            int            lineNum, 
	                    CodeAnalysis&  analysis ) {

    LineInfo info = { lineNum, node };

    if ( node->nodeType == REQ_VREG ) {
        int vregId = node->firstChild->id; 
        analysis.vregRequests [ vregId ] = info; 
    }    

    else if ( node->nodeType == CLEAR_VREG ) {
        int vregId = node->firstChild->id; 
        analysis.vregClears [ vregId ] = info; 
    }

    else if ( node->nodeType == OPEN_LOOP ) {
        analysis.loopOpenMarkers [ lineNum ] = info;
    }

    else if ( node->nodeType == CLOSE_LOOP ) {
        analysis.loopCloseMarkers [ node->id ] = info;
    }

}


static void setReadWriteDescend ( ir_node*      instr,
                                  ir_node*      node, 
                                  int           lineNum, 
	                          CodeAnalysis& analysis ) {
    
    int i=0;
    ir_node* child = node->firstChild;
    while ( child != NULL ) {

        // child is virtual register
        if ( isVreg ( child ) ) {

	    int vid = child->id;

            if ( checkInstrRead ( node, i ) ) {

		analysis.vregReads [ vid ].push_back (
		    { { lineNum, instr }, child }
	        );

            }
            if ( checkInstrWrite ( node, i ) ) {
		
		analysis.vregWrites [ vid ].push_back (
		    { { lineNum, instr }, child }
	        );
            }
        }

        setReadWriteDescend ( instr, child, lineNum, analysis );

        child = child->next;
        i++;
    }
}


bool decodeMemoryLocation ( ir_node* memNode, ir_node*& locVreg, int& offset ) {

    ir_node* child = memNode->firstChild;

    if ( isVreg ( child ) ) {
        locVreg = child;
        offset = 0;
        return true;
    } 
    
    if ( child->nodeType == MEM_ADD ) {
        ir_node* base = child->firstChild;
        ir_node* offs = child->lastChild;
        if ( ! isVreg ( base ) ) return false;
        if ( offs->nodeType != CONSTANT ) return false;
        locVreg = base;
        offset  = atoi ( offs->ident );
        return true;
    } 

    return false;
}


static void recordMemoryAccess ( ir_node*      instr,
                                 CodeAnalysis& analysis, 
                                 int           lineNum ) {

    if ( instr->nodeType == MOV ) {

        M_Assert ( instr->nChildren == 2, "Invalid mov instruction during analysis." );
   
        ir_node* locVreg;
        int offset; 

        ir_node* first = instr->firstChild;
        ir_node* second = instr->lastChild;
    
        // write 
        if ( first->nodeType == MEM_AT ) {
            if ( decodeMemoryLocation ( first, locVreg, offset ) ) {
                analysis.memWrites[locVreg->id].push_back ( { { lineNum, instr }, locVreg, offset, second, STORE } );
            } 

        } 
       
        // read
        if ( second->nodeType == MEM_AT ) {
            if ( decodeMemoryLocation ( second, locVreg, offset ) ) {
                analysis.memReads[locVreg->id].push_back ( { { lineNum, instr }, locVreg, offset, first, LOAD } );
            } 
        } 
    }
} 


static CodeAnalysis analyzeCode ( ir_node*      baseNode, 
                                  bool          showAnalysis = false ) {
   
    CodeAnalysis analysis;
     
    int lineNum = 0;
    ir_node* line = baseNode->firstChild;
    while ( line != NULL ) {
        
        // save prev instruction to allow deletion and skip inserts
        ir_node* next = line->next;

        setReadWriteDescend ( line, line, lineNum, analysis );

        recordMarkers ( line, lineNum, analysis );

        recordMemoryAccess ( line, analysis, lineNum );
        
        line = next; 
        lineNum++;
    }

    // order by instruction number of first element in group
    for ( auto& it: analysis.memReads ) {
        analysis.orderedMemAccess [ it.second.front().line.num ] = it.second;
    }
    for ( auto& it: analysis.memWrites ) {
        analysis.orderedMemAccess [ it.second.front().line.num ] = it.second;
    }

    return analysis;
}


static RegAccessInfo getEarliestAccess ( CodeAnalysis&                  analysis,
                                         std::vector < MemAccessInfo >& group ) {
        
    RegAccessInfo firstReadAcross = firstRead ( analysis, group[0].movVreg );
    for ( auto l : group ) {
        RegAccessInfo& rd = firstRead ( analysis, l.movVreg ); 
        if ( rd.line.num < firstReadAcross.line.num ) {
            firstReadAcross = rd;
        }
    }
    return firstReadAcross;
}






