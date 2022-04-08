/**
 * @file
 * Base functionality for tree based IR representations. 
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <cstring>
#include "util/assertion.h"


enum BaseNodeTypes {
    UNDEFINED         =  0,
    ROOT              =  1
};


typedef struct ir_node ir_node;


typedef struct ir_node {

    /* pointer to code generation function */
    char* (*emit_fun)( struct ir_node* );  

    /* language tokens */
    char* ident;                           
    char* ident2;                           

    /* values for constants */
    union {
        int64_t  int64Data;
        int32_t  int32Data;
        int8_t   int8Data;
        void*    addressData;
        double   doubleData;
    };

    /* enum in target IR */
    int nodeType;

    /* id to identify resources that consist of multiple entities
     * e.g. machine registers 0 to 15 or virtual registers 0 to n.
     * Value is 0 when unapplicable.
     */
    int id;

    /* an ir node 
     * _has_ a linked list of descendants (nChildren, firstChild, lastChild )
     * _is_ a linked list element (next, prev)
     */     
    int nChildren;
    ir_node* firstChild;
    ir_node* lastChild;
    ir_node* next;
    ir_node* prev;

} ir_node;


/* global allocation of nodes */
static ir_node* allNodes = NULL;
static int nextNodeIdx = 0;
static int numAllocated = 1000000;


static void allocateAllNodes () {
    allNodes = (ir_node*) malloc ( sizeof ( ir_node ) * numAllocated );
    for ( int i=0; i<nextNodeIdx; i++ ) {
        allNodes[i].ident = nullptr;
        allNodes[i].ident2 = nullptr;
    }
}


static ir_node* getNode () {
    M_Assert ( nextNodeIdx < numAllocated, "More nodes than allocated." );
    ir_node* node = &allNodes[nextNodeIdx++];
    node->ident = NULL;
    node->ident2 = NULL;
    node->emit_fun = NULL;
    node->nodeType = 0;
    node->id = 0;

    node->nChildren = 0;
    node->firstChild = NULL;
    node->lastChild = NULL;
    node->next = NULL;
    node->prev = NULL;
    return node;
}


static void setIdent ( ir_node* n, char* id ) {
    asprintf ( &(n->ident), "%s", id );
}


static void setIdent2 ( ir_node* n, char* id ) {
    asprintf ( &(n->ident2), "%s", id );
}


static ir_node* copyNode ( ir_node* n) {
    ir_node* res = getNode();
    memcpy ( res, n, sizeof ( ir_node ) );
    if ( n->ident != NULL ) 
        setIdent ( res, n->ident );
    if ( n->ident2 != NULL ) 
        setIdent2 ( res, n->ident2 );
    return res;
}


static char* call_emit ( ir_node* node ) {
    if ( node->emit_fun == NULL ) {
        fprintf ( stderr, "Error. No emit function in node type %i\n", node->nodeType );
    }
    M_Assert ( node->emit_fun != NULL, "Cannot call emit_fun(..)" );
    char* res = node->emit_fun ( node );
    return res;
}


static void freeNode ( ir_node* n ) {
    M_Assert ( n != NULL, "Node to free is NULL" );
    if ( n == NULL ) return;
    if ( n->ident != NULL ) {
        free ( n->ident );
    }
    if ( n->ident2 != NULL ) {
        free ( n->ident2 );
    }
}


static void freeAllNodes () {
    for ( int i=0; i<nextNodeIdx; i++ ) {
        freeNode ( &allNodes[i] );
    }
    free ( allNodes );
}


static char* emitIrRoot ( ir_node* node ) {
    char* res = "";
    ir_node* chd = node->firstChild;
    int i = 0;
    while ( chd != NULL ) {
        char* code = call_emit ( chd ); 
        asprintf ( &res, "%s%s", res, code );
        chd = chd->next;
        i++;
    }
    return res;
}


static ir_node* irRoot () {
    ir_node* res = getNode ();
    res->emit_fun = &(emitIrRoot);
    res->nodeType = ROOT;
    return res;
}


static void addChild ( ir_node* node, ir_node* child ) {
    if ( child == nullptr ) return;
    ir_node* added = copyNode ( child );
    if ( node->firstChild == nullptr ) {
        node->firstChild = added;
        node->lastChild = added;
        node->nChildren = 1;
    } else {
        added->prev = node->lastChild;
        node->lastChild->next = added;
        node->lastChild = added;
        node->nChildren++;
    }
}


static ir_node* insertBeforeChild ( ir_node* baseNode, ir_node* child, ir_node* insert ) {
    if ( baseNode->firstChild == child ) {
        baseNode->firstChild = insert;
    }
    else {
        child->prev->next = insert;
    }
    insert->next = child;
    child->prev  = insert;
    insert->prev = child->prev;
    baseNode->nChildren++;
    return insert;
}



static ir_node* insertAfterChild ( ir_node* baseNode, ir_node* child, ir_node* insert ) {
    insert->next = child->next;
    insert->next->prev = insert;
    insert->prev = child;
    child->next  = insert;
    if ( baseNode->lastChild == child ) {
        baseNode->lastChild = insert;
    }
    baseNode->nChildren++;
    return insert;
}


static ir_node* transferNodes ( ir_node* baseNode, ir_node* childPos, ir_node* insertBase ) {
    ir_node* ret = insertBase->lastChild;
    if ( insertBase->nChildren == 0 ) {
        return nullptr;
    }
    // prepend
    if ( childPos == nullptr ) {
        baseNode->firstChild->prev = insertBase->lastChild;
        insertBase->lastChild->next = baseNode->firstChild;
        baseNode->firstChild = insertBase->firstChild;
    } 
    // append
    else if ( childPos == baseNode->lastChild ) {
        childPos->next = insertBase->firstChild;
        childPos->next->prev = childPos;
        baseNode->lastChild = insertBase->lastChild;
    } 
    // insert
    else {
        insertBase->firstChild->prev = childPos;
        insertBase->lastChild->next = childPos->next;
        childPos->next->prev = insertBase->lastChild;
        childPos->next = insertBase->firstChild;
        baseNode->nChildren += insertBase->nChildren;
    }

    // clear insertBase children
    insertBase->firstChild = nullptr;
    insertBase->lastChild = nullptr;
    insertBase->nChildren = 0;
    return ret;
}


static ir_node* removeChild ( ir_node* baseNode, ir_node* child ) {

    if ( child == baseNode->firstChild ) {
        baseNode->firstChild = child->next;
    } 
    if ( child == baseNode->lastChild ) {
        baseNode->lastChild = child->prev;
    } 
    if ( child->prev != NULL ) {
        child->prev->next = child->next;
    }
    if ( child->next != NULL ) {
        child->next->prev = child->prev;
    }
    baseNode->nChildren--;
    return child->prev;
}


static ir_node* replaceChild ( ir_node* baseNode, ir_node* old, ir_node* replacement ) {
    if ( baseNode->firstChild == old ) {
        baseNode->firstChild = replacement;
    }
    if ( baseNode->lastChild == old ) {
        baseNode->lastChild = replacement;
    }
    if ( old->prev != NULL ) {
        old->prev->next = replacement;
    }
    if ( old->next != NULL ) {
        old->next->prev = replacement;
    }
    replacement->next = old->next;
    replacement->prev = old->prev;
    return baseNode;
}


static char* emitLiteral ( ir_node* node ) {
    char* res;
    asprintf ( &res, "%s", node->ident );
    return res;
}

static ir_node* literal ( char* ident, int type ) {
    ir_node* res = getNode();
    setIdent ( res, ident );
    res->emit_fun = &(emitLiteral);
    res->nodeType = type;
    return res;
}


static char* emitUnaryInstr ( ir_node* node ) {
    M_Assert ( node->firstChild != NULL, "Unary instruction without child." );
    char* op1 = call_emit ( node->firstChild ); 
    char* res;
    asprintf ( &res, "%-14s %-20s\n", node->ident, op1 );
    free ( op1 );
    return res;
}


static ir_node* unaryInstr ( char* mnemonic, ir_node* op, int type ) {
    M_Assert ( op != NULL, "Child node that was passed is NULL." );
    ir_node* res = getNode();
    setIdent ( res, mnemonic );
    addChild ( res, op );
    res->emit_fun = &(emitUnaryInstr);
    res->nodeType = type;
    return res;
}


static char* emitUnarySub ( ir_node* node ) {
    M_Assert ( node->firstChild != NULL, "Unary sub without child." );
    char* op1 = call_emit ( node->firstChild ); 
    char* res;
    asprintf ( &res, "%s(%s)", node->ident, op1 );
    free ( op1 );
    return res;
}


static ir_node* unarySub ( char* mnemonic, ir_node* op, int type ) {
    ir_node* res = unaryInstr ( mnemonic, op, type );
    res->emit_fun = &(emitUnarySub);
    return res;
}


static char* emitBinaryInstr ( ir_node* node ) {
    M_Assert ( node->firstChild != NULL, "Binary instruction without first child." );
    M_Assert ( node->firstChild->next != NULL, "Binary instruction without second child." );
    char* op1 = call_emit ( node->firstChild ); 
    char* op2 = call_emit ( node->firstChild->next ); 
    char* res;
    char* op1comma;
    asprintf ( &op1comma, "%s,", op1 );
    asprintf ( &res, "%-14s %-32s%-26s\n", node->ident, op1comma, op2 );
    free ( op1 );
    free ( op2 );
    return res;
}


static ir_node* binaryInstr ( char* mnemonic, ir_node* op1, ir_node* op2, int type ) {
    M_Assert ( op1 != NULL, "Requires first child." );
    M_Assert ( op2 != NULL, "Requires second child." );
    ir_node* res = getNode();
    setIdent ( res, mnemonic );
    addChild ( res, op1 );
    addChild ( res, op2 );
    res->emit_fun = &(emitBinaryInstr);
    res->nodeType = type;
    return res;
}

static char* sep ( char* str ) {
    char* res;
    asprintf ( &res, "%s,", str );
    free ( str );
    return res;
}

static char* emitTernaryInstr ( ir_node* node ) {
    M_Assert ( node->firstChild != NULL, "Ternary instruction without first child." );
    M_Assert ( node->firstChild->next != NULL, "Ternary instruction without second child." );
    M_Assert ( node->firstChild->next->next != NULL, "Ternary instruction without third child." );
    char* op1 = call_emit ( node->firstChild ); 
    char* op2 = call_emit ( node->firstChild->next ); 
    char* op3 = call_emit ( node->firstChild->next->next ); 
    char* res;
    asprintf ( &res, "%-14s %-20s,%-20s,%-20s\n", node->ident, op1, op2, op3 );
    free ( op1 );
    free ( op2 );
    free ( op3 );
    return res;
}


static ir_node* ternaryInstr ( char* mnemonic, ir_node* op1, ir_node* op2, ir_node* op3, int type ) {
    M_Assert ( op1 != NULL, "Requires first child." );
    M_Assert ( op2 != NULL, "Requires second child." );
    M_Assert ( op3 != NULL, "Requires third child." );
    ir_node* res = getNode();
    setIdent ( res, mnemonic );
    addChild ( res, op1 );
    addChild ( res, op2 );
    addChild ( res, op3 );
    res->emit_fun = &(emitTernaryInstr);
    res->nodeType = type;
    return res;
}


static char* emitBracketingNode ( ir_node* node ) {
    M_Assert ( node->firstChild != NULL, "Bracketing instruction without child." );
    char* op = call_emit ( node->firstChild ); 
    char* res;
    asprintf ( &res, "%s%s%s", node->ident, op, node->ident2 );
    free ( op );
    return res;
}

static ir_node* bracketingNode ( char* tokenOpen, char* tokenClose, ir_node* child, int type ) {
    M_Assert ( child != NULL, "Child node that was passed is NULL." );
    ir_node* res = getNode();
    setIdent ( res, tokenOpen );
    setIdent2 ( res, tokenClose );
    addChild ( res, child );
    res->emit_fun = &(emitBracketingNode);
    res->nodeType = type;
    return res;
}
