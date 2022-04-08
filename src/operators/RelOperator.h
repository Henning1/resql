/**
 * @file
 * Base class for relational operator implementations.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <vector>
#include <iostream>

#include "../dbdata.h"


class JitContextFlounder;


/**
 * @brief The operator base class
 * Super class for relational operators.
 */
class RelOperator {
public:

    enum OperatorTag {
        UNDEFINED,
        SCAN,
        PROJECTION,
        SELECTION,
        MATERIALIZE,
        NESTEDLOOPSJOIN,
        HASHJOIN,
        AGGREGATION,
        ORDERBY,    
    };

    /* pointer to parent operator */
    RelOperator* _parent = nullptr;

    /* all pointers to child operators */
    std::vector<RelOperator*> children;
    
    /* pointer to first child (if available) */
    RelOperator* _child;
    
    /* pointer to left and right child (if available) */
    RelOperator* _lChild;
    RelOperator* _rChild;

    /* type of operator */
    OperatorTag tag;

public:

    RelOperator ( OperatorTag tag ) : tag ( tag ) {};

    /* Result schema of this operator. Needs to be set before calling _parent->consume(). */
    Schema _schema;


    /* virtual destructor to be able to derive the actual type for deletePlan() */
    virtual ~RelOperator() {};


    /* add child operator */
    void addChild ( RelOperator* c ) {

        if ( c == nullptr ) {
            error_msg ( ELEMENT_NOT_FOUND, "The child operator that is added is NULL (" + name() + ")" );
        }

        c->_parent = this;
        this->children.push_back(c);
        if ( children.size() == 1 ) {
            _child = c;
        }
        if ( children.size() == 2 ) {
            _lChild = children[0];
            _rChild = children[1];
        }
    }
    

    /* replace child operator */
    void replaceChild ( RelOperator* old, RelOperator* new_ ) {
        bool found = false;
        std::vector < RelOperator* > back = children;
        children.clear();
        for ( RelOperator* c : back ) {
            if ( c == old ) {
                addChild ( new_ );
                found = true;
            }
            else {
                addChild ( c );
            }
        }
        if ( !found ) {
            error_msg ( ELEMENT_NOT_FOUND, "Child to replace not found in BaseOperator::replaceChild(..)" );
        }
    }


    /** 
     * @brief Overload new-operator for aligned allocation.
     * We use allocation of aligned memory to avoid false sharing.
     * Without aligned allocation false sharing may happen when operators,
     * that are executed on different cores share the same cache line.
     */
    static void* operator new ( size_t sz ) {

        /** The following commented line leads to usage of classical new
          * causing false sharing behaviour during parallel volcano execution.
          */
        //return ::operator new ( sz );        

        const size_t align = 64;

        void* ptr = aligned_alloc ( align, ( (sz / align) + 1) * align );
        if ( ptr == nullptr ) {
            std::cout << "allocation failed" << std::endl;
        }
        return ptr;

    }


    /** 
     * @brief We overload delete-operator for deallocation of aligned memory
     */
    static void operator delete ( void* ptr ) {
        
        /* The following line is classical delete with false sharing behaviour. */
        //::operator delete ( ptr );

        free ( ptr );
    }


    /** 
     * @brief Recursive deallocation and deletion of query plans.
     * Operators with varying numbers of children ( Join, Exchange Operator )
     * need to implement this method.
     */
    virtual void deletePlan () {
        for ( const auto& c : this->children ) {
            c->deletePlan();
        }
        //std::cout << "deleting " << this->name() << ", " << this << std::endl;
        delete ( this );
    }    


    /**
     * @brief Returns an estimate of the result size of the operator.
     * The estimate is based on the estimate of the child operators.
     * In the simplest case we pass relation sizes through the plan
     * for e.g. buffer allocation.
     */
    virtual size_t getSize ()   = 0;

    
    /*
     * Flounder JIT interface
     */
    virtual void defineExpressions ( std::map <std::string, SqlType>& identTypes ) = 0;
    virtual void produceFlounder ( JitContextFlounder& ctx, SymbolSet request ) = 0;
    virtual void consumeFlounder ( JitContextFlounder& ctx ) = 0;
    

    /** 
     * @brief Returns name of the relational operator.
     */
    virtual std::string name() = 0;
    
    
    virtual bool isMaterializedOperator() {
        return false;
    }


    virtual std::unique_ptr < Relation > retrieveResult() {
        throw ResqlError ( "Calling retrieveResult on non-materialized operator" );
    }
    

    virtual void addLimit ( size_t limit ) {
        throw ResqlError ( "Limit can only be set on materializing operators" );
    }
   

    void printPrefix ( std::string prefix, bool isLastChild, std::ostream& out ) {
        std::string dis = (isLastChild) ? "  └─" : "  ├─";
        out << prefix << dis << name () << std::endl;
        int num = children.size();
        for ( int i=num-1; i >= 0; i-- ) {
            std::string add = (isLastChild) ? "    " : "  │ ";
            children[i]->printPrefix ( prefix + add, i==0, out );
        }
    } 


    void defineExpressionsForPlan ( std::map <std::string, SqlType>&  identTypes ) {
        for ( auto& c : this->children ) {
            c->defineExpressionsForPlan ( identTypes );
        }
        this->defineExpressions ( identTypes );
    }
    
    
    void print ( std::ostream& out = std::cout) {
        printPrefix ( "", true, out );
    }

};
