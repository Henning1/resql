#include "qlib/sort.h"
#include "expressions.h"


class OrderByOp : public RelOperator {

public:

    
    /* Order by expressions */
    ExprVec _orderExpressions;


    /* Converted expressions */
    std::vector < OrderRequest > _orderRequests;

    
    /* limit for limit clause */
    size_t _limit;
    bool   _hasLimitClause;


    OrderByOp ( ExprVec&&     orderExpr, 
                RelOperator*  child ) 
        :  RelOperator ( RelOperator::ORDERBY ),
           _orderExpressions ( orderExpr ), _hasLimitClause ( false ) {
  
        addChild ( new MaterializeOp ( child ) );
    }
  
  
    ~OrderByOp() = default;
  
  
    std::string name() { return "OrderBy"; }
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {

        /* add ASC if unspecified */ 
        for ( size_t i=0; i < _orderExpressions.size(); i++) {
            Expr* e = _orderExpressions[i];
            if ( e->tag != Expr::ASC && 
                 e->tag != Expr::DESC ) {
                _orderExpressions[i] = ExprGen::asc ( e );
            }
        }
   
        for ( Expr* e : _orderExpressions ) {
            if ( e->child->tag != Expr::ATTRIBUTE ) {
                throw ResqlError ( "Order by only supports attribute expressions currently." + serializeExpr ( e->child)  );
            }
        }

        deriveExpressionTypes ( _orderExpressions, identTypes );
    }
  
  
    size_t getSize () {
         return _child->getSize(); 
    }
    

    virtual bool isMaterializedOperator() {
        return true;
    }
    

    virtual void addLimit ( size_t limit ) {
        _hasLimitClause = true;
        _limit = limit;
    }


    virtual std::unique_ptr < Relation > retrieveResult() {
        MaterializeOp* child = ( MaterializeOp* ) _child;
        if ( _hasLimitClause ) {
            child->relOut->applyLimit ( _limit );
        }
        return std::move ( child->relOut );
    }


    void produceFlounder ( JitContextFlounder&  ctx, 
                           SymbolSet            request ) {
  
        _child->produceFlounder ( ctx, request );

        /* Map orderby expressions to order requests */
        _schema = _child->_schema;
        _orderRequests.reserve ( _orderExpressions.size() );
        for ( Expr* e : _orderExpressions ) {
            if ( !_schema.contains ( e->child->symbol ) ) {
                throw ResqlError ( "Order By attribute not found." );
            }
            Attribute attr = _schema.getAttributeByName ( e->child->symbol );
            _orderRequests.push_back ( { 
                (size_t) _schema.getOffsetInTuple ( attr.name ), 
                attr.type, 
                e->tag != Expr::DESC
            } );
        }
 
        /* Produce sort functionality in flounder */ 
        ctx.comment ( " --- Sort " );
        MaterializeOp* child = ( MaterializeOp* ) _child;
        ir_node* foo = ctx.request ( vreg64 ( "unused_return" ) );

        /* call sort function */
        ctx.yield ( 
            mcall2 ( foo,
                     (void*) &sort,
                     constLoad ( constAddress ( child->relOut.get() ) ),
                     constLoad ( constAddress ( &_orderRequests ) )
            ) 
        );
        ctx.clear ( foo );
    }
 
 
    void consumeFlounder ( JitContextFlounder& ctx ) { }
  
};
