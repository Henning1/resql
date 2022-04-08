


ir_node* extractValueAVX ( ir_node* out,
		           ir_node* ymm,
		           int idx ) {
    ir_node* c = irRoot();
    addChild ( c, vextractf128 ( xmm ( 15 ), ymm,        constInt32 ( idx / 2 ) ) );
    addChild ( c, vpextrq      ( out,        xmm ( 15 ), constInt32 ( idx % 2 ) ) );
    return c;
}


ir_node* extractValueSSE ( ir_node* out,
		           ir_node* ymm,
		           int idx ) {
    ir_node* c = irRoot();
    addChild ( c, pextrq ( out, ymm, constInt32 ( idx % 2 ) ) );
    return c;
}


ir_node* extractValueAVX512 ( ir_node* out,
		              ir_node* zmm,
		              int idx ) {
    ir_node* c = irRoot();
    addChild ( c, vextracti64x2 ( xmm ( 15 ), zmm,        constInt32 ( idx / 4 ) ) );
    addChild ( c, vpextrq       ( out,        xmm ( 15 ), constInt32 ( idx % 2 ) ) );
    return c;
}


typedef ir_node* (*SimdMov)     ( ir_node*, ir_node* );    
typedef ir_node* (*SimdReg)     ( int );    
typedef ir_node* (*SimdExtract) ( ir_node*, ir_node*, int );    


typedef struct SimdInstructionSet {

    // width in number of attributes (each 8 bytes)
    unsigned int width; 

    // instructions (sequence for extract) to add SIMD operations
    SimdMov     mov;
    SimdReg     reg;
    SimdExtract extract;

} SimdInstructionSet;


static void placeSimdMemoryAccess ( ir_node*                       code,
		                    CodeAnalysis&                  analysis,
                                    std::vector < MemAccessInfo >& loads,
                                    std::vector < MemAccessInfo >& stores,
		                    SimdInstructionSet&            instrSet,
                                    int                            simdRegId,
                                    bool                           delayLoads ) {

    // get insert positions and memory locations
    ir_node* loadBaseVreg = loads[0].baseVreg;
    ir_node* storeBaseVreg = stores[0].baseVreg;
    ir_node* loadIns = loads.back().line.node;

    // try to move loads to a later code position
    if ( delayLoads ) {
        loadIns = getEarliestAccess ( analysis, loads ).line.node->prev;
    }
    
    // simd load
    loadIns = insertAfterChild ( code, loadIns, 
        instrSet.mov ( 
            instrSet.reg(simdRegId), 
            memAt ( memAdd ( loadBaseVreg, constInt32 ( loads[0].offset ) ) ) 
        ) 
    );
    
    // extract attributes that are used individually to scalar registers
    for ( unsigned long i=0; i < loads.size(); i++) {
        auto& ld = loads[i];
        auto& st = stores[i];

	// check lines of use
	RegAccessInfo& firstUse = firstRead ( analysis, ld.movVreg );
	int materialize = st.line.num;

        // do extract value from simd to scalar if neceassry
        if ( firstUse.line.num < materialize ) {
	    //transferNodes ( code, firstUse.instr->prev, 
	    transferNodes ( code, loadIns, 
	        instrSet.extract ( ld.movVreg, instrSet.reg ( simdRegId) , i )
	    );
        }
    }        
    
    // simd store
    insertAfterChild ( code, stores[0].line.node->prev, 
        instrSet.mov ( 
            memAt ( memAdd ( storeBaseVreg, constInt32 ( stores[0].offset ) ) ), 
            instrSet.reg(simdRegId) 
        ) 
    );
    
    // remove scalar access
    for ( unsigned long i=0; i<stores.size(); i++) {
        removeChild ( code, stores[i].line.node );
    }
    for ( unsigned long i=0; i<loads.size(); i++) {
        removeChild ( code, loads[i].line.node );
    }
}



static void addSimdOptimizations ( ir_node*            baseNode,
                                   CodeAnalysis&       analysis,
				   SimdInstructionSet& instrSet,
	                           bool                delayLoads = true ) {
    
    std::vector < std::vector < MemAccessInfo > > loadSets;
    std::vector < MemAccessInfo > storeSet;

    for ( auto& it : analysis.orderedMemAccess ) {
        
        std::vector < MemAccessInfo >& set = it.second;

        // collect loads ..
        if ( (set.size() >= instrSet.width) && set[0].type == LOAD ) {
            loadSets.push_back ( set );
        }

        // .. until store
        if ( set[0].type == STORE ) {

            if ( loadSets.size() == 0 ) continue;

            std::vector < MemAccessInfo > matchedLoads;
            std::vector < MemAccessInfo > matchedStores;
            unsigned int simdRegId = 0;

            unsigned long loadSetId = loadSets.size()-1;
            std::vector < MemAccessInfo >::iterator storeIt = set.begin();
            std::vector < MemAccessInfo >::iterator loadIt = loadSets[loadSetId].begin();
           
            while ( storeIt < set.end() ) {

                if ( storeIt->movVreg->id == loadIt->movVreg->id ) {
                    matchedLoads.push_back ( *loadIt );
                    matchedStores.push_back ( *storeIt );
                    loadIt++;
                }

                // found matching load and store access with simd width
                //  -> replace scalar memory access simd memory access
                if ( matchedLoads.size() == instrSet.width ) {
                    placeSimdMemoryAccess ( 
                        baseNode, 
                        analysis, 
                        matchedLoads, 
                        matchedStores, 
                        instrSet,
                        simdRegId,
                        delayLoads 
                    );
                    simdRegId++;
                    matchedLoads.clear();
                    matchedStores.clear();
                    if ( simdRegId == 16 ) break;
                }

                // move store iterator and step through load sets
                storeIt++;
                if ( loadIt == loadSets[loadSetId].end() ) {
                    if ( loadSetId == 0 ) break;
                    loadSetId--;
                    loadIt = loadSets[loadSetId].begin();

                    // next loadSet has a different base address -> clear
                    matchedLoads.clear();
                    matchedStores.clear();
                }
            } 
            loadSets.clear();
        }
    }
}
