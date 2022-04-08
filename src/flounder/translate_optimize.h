/**
 * @file
 * Apply optimization to Flounder IR i.e. SIMD load/store and delay loads. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <set>
#include <map>
#include <list>

#include "flounder.h"
#include "translate_analyze.h"
#include "translate_optimize_simd.h"



static void delayLoads ( ir_node* baseNode ) {
    
    std::cout << "delay loads.." << std::endl;

    CodeAnalysis analysis = analyzeCode ( baseNode );
   
    std::set < int > delayed;
    int lineNum = 0;
    ir_node* instr = baseNode->firstChild;

    while ( instr != NULL ) {

        // save prev instruction to allow deletion and skip inserts
        ir_node* next = instr->next;
        if ( instr->nodeType == MOV ) {
            ir_node* first = instr->firstChild;
            ir_node* second = instr->lastChild;
           
            // delay load
            if ( isVreg ( first ) && 
                 second->nodeType == MEM_AT && 
                 delayed.count ( first->id ) == 0 ) {

                if ( isRead ( analysis, first ) ) {
                    RegAccessInfo& fr = firstRead ( analysis, first );
                    
                    if ( analysis.vregWrites [ first->id ].size() == 1 ) {
                        delayed.insert ( first->id ); // delay only once to avoid loops
                        removeChild ( baseNode, instr );
                        insertAfterChild ( baseNode, fr.line.node->prev, instr );
                    }

                } 
            }
        }
        instr = next; 
        lineNum++;
    }
}




static void shrinkWrapUsageRanges ( ir_node* baseNode ) {

    std::cout << "shrink wrap.." << std::endl;

    CodeAnalysis analysis = analyzeCode ( baseNode );

    for ( const auto& vregReq : analysis.vregRequests ) {
   
        int id = vregReq.second.node->firstChild->id;
        LineInfo req   = vregReq.second;
        LineInfo clear = analysis.vregClears [ id ];

        auto itLowerLoops = analysis.loopOpenMarkers.lower_bound ( req.num );
        auto itUpperLoops = analysis.loopOpenMarkers.upper_bound ( clear.num );
        LineInfo requestBefore = firstWrite ( analysis, id ).line;
        if ( !isRead ( analysis, id ) ) {
            if ( requestBefore.node->nodeType != MANAGED_CALL ) {
                std::cout << call_emit ( requestBefore.node ) << " is never read!" << std::endl;
                removeChild ( baseNode, req.node );
                removeChild ( baseNode, requestBefore.node );
                removeChild ( baseNode, clear.node );
            }
            continue;
        }
        LineInfo clearAfter    = lastRead ( analysis, id ).line;
     
        /* Expand requestBefore and clearAfter such that all loops in the *
         * original validity range are fully contained.                   *
         */
        for ( auto itLoop = itLowerLoops; itLoop != itUpperLoops; itLoop++ ) {

            LineInfo open  = itLoop->second;
            LineInfo close = analysis.loopCloseMarkers [ open.node->id ];

            if ( open.num < requestBefore.num ) {
                
                /* skip if the whole loop closes before first use */
                if ( close.num < requestBefore.num ) {
                    continue;
                }
                requestBefore = open;
            }
            
            if ( close.num > clearAfter.num ) {
                clearAfter = close;
            }
        }

        removeChild ( baseNode, req.node );
        insertAfterChild ( baseNode, requestBefore.node->prev, req.node );

        if ( isRead ( analysis, id ) ) {
            
            removeChild ( baseNode, clear.node );
            insertAfterChild ( baseNode, clearAfter.node, clear.node );
        }
    } 
}





static void replaceAliasesDescend ( ir_node*               node,
                                    std::map < int, int >  aliasMap ) {
    
    ir_node* child = node->firstChild;
    while ( child != NULL ) {
        if ( isVreg ( child ) ) {
            if ( aliasMap.find ( child->id ) != aliasMap.end() ) {
                child->id = aliasMap [ child->id ];
                char* ident;
                asprintf ( &ident, "_(%i)", child->id );
                setIdent ( child, ident ); 
            }
        }
        replaceAliasesDescend ( child, aliasMap );

        if ( child->nodeType == MOV ) {
            if ( isVreg ( child->firstChild ) 
              && isVreg ( child->lastChild )
              && ( child->firstChild->id == child->lastChild->id ) ) {
      
                removeChild ( node, child );
            }
        }

        child = child->next;
    }
}



static void mergeVregRanges ( ir_node*       baseNode,
                              CodeAnalysis&  analysis,
                              int            id1,
                              int            id2 ) {


        LineInfo req        = analysis.vregRequests [ id1 ];
        LineInfo clear      = analysis.vregClears   [ id1 ];
        LineInfo aliasReq   = analysis.vregRequests [ id2 ];
        LineInfo aliasClear = analysis.vregClears   [ id2 ];

        LineInfo mergeReq;
        LineInfo mergeClear;
        if ( aliasReq.num < req.num ) {
            mergeReq = aliasReq;
            removeChild ( baseNode, req.node );
        }
        else {
            mergeReq = req;
            removeChild ( baseNode, aliasReq.node );
        }
        if ( aliasClear.num > clear.num ) {
            mergeClear = aliasClear;
            removeChild ( baseNode, clear.node );
        }
        else {
            mergeClear = clear;
            removeChild ( baseNode, aliasClear.node );
        }
        mergeReq.node->firstChild->id = id2;
        mergeClear.node->firstChild->id = id2;
       
        analysis.vregRequests [ id1 ] = mergeReq; 
        analysis.vregRequests [ id2 ] = mergeReq; 
        analysis.vregClears   [ id1 ] = mergeClear; 
        analysis.vregClears   [ id2 ] = mergeClear; 
}
                          



static void aliasing ( ir_node* baseNode ) {

    std::cout << "aliasing.." << std::endl;
    
    std::map < int, int > aliasMap;

    CodeAnalysis analysis = analyzeCode ( baseNode );
    for ( const auto& vregReq : analysis.vregRequests ) {

        int id = vregReq.second.node->firstChild->id;
       
        std::map < int, std::vector < RegAccessInfo > > vregWrites;
        
        if ( analysis.vregWrites [ id ].size() > 1 ) {
            continue;
        }
        ir_node* write = analysis.vregWrites [ id ] [ 0 ].line.node;
        if ( write->nodeType != MOV ) {
           continue;
        }
        if ( !isVreg ( write->firstChild->next ) ) {
           continue;
        }

        int aliasId = write->firstChild->next->id;
        if ( analysis.vregWrites [ aliasId ].size() > 1 ) {
            continue;
        }
        if ( aliasMap.find ( aliasId ) != aliasMap.end() ) {
            aliasId = aliasMap [ aliasId ];
        }

        mergeVregRanges ( baseNode, analysis, id, aliasId );

/*
        LineInfo aliasReq   = analysis.vregRequests [ aliasId ];
        LineInfo aliasClear = analysis.vregClears [ aliasId ];

        LineInfo mergeReq;
        LineInfo mergeClear;
        if ( aliasReq.num < req.num ) {
            mergeReq = aliasReq;
            removeChild ( baseNode, req.node );
        }
        else {
            mergeReq = req;
            removeChild ( baseNode, aliasReq.node );
        }
        if ( aliasClear.num > clear.num ) {
            mergeClear = aliasClear;
            removeChild ( baseNode, clear.node );
        }
        else {
            mergeClear = clear;
            removeChild ( baseNode, aliasClear.node );
        }
        mergeReq.node->firstChild->id = aliasId;
        mergeClear.node->firstChild->id = aliasId;
       
        analysis.vregRequests [ id ] = mergeReq; 
        analysis.vregRequests [ aliasId ] = mergeReq; 
        analysis.vregClears [ id ] = mergeClear; 
        analysis.vregClears [ aliasId ] = mergeClear; 
*/


        aliasMap [ id ] = aliasId;
    }

    replaceAliasesDescend ( baseNode, aliasMap );
}

static void combining ( ir_node* baseNode ) {
    std::cout << "combining.." << std::endl;
    CodeAnalysis analysis = analyzeCode ( baseNode );
    std::map < int, int > aliasMap;
    ir_node* instr = baseNode->firstChild;
    while ( instr != NULL && instr->next != NULL ) {
        if ( instr->nodeType == MOV ) {
            if ( ( isVreg ( instr->firstChild ) ) &&
                 ( isVreg ( instr->lastChild  ) ) && 
                 ( instr->next->nodeType == CLEAR_VREG ) &&
                 ( instr->next->firstChild->id == instr->lastChild->id ) )  
            {
                int idFirst  = instr->lastChild->id;
                int idSecond = instr->firstChild->id;
                if ( aliasMap.find ( idFirst ) != aliasMap.end() ) {
                    idFirst = aliasMap [ idFirst ];
                } 
                mergeVregRanges ( baseNode, analysis, idFirst, idSecond );
                aliasMap [ idSecond ] = idFirst;
            }
        } 
        instr = instr->next;
    }
    replaceAliasesDescend ( baseNode, aliasMap );
}


static void optimize ( ir_node* baseNode ) { 
    
    aliasing ( baseNode );

    delayLoads ( baseNode );

    shrinkWrapUsageRanges ( baseNode );
    
    combining ( baseNode );
}




