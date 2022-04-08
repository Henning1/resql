


/**
 * @brief Operator for selection.
 */
class SelectionOp : public RelOperator {

public:

    Expr*     _condition;
    SymbolSet _request;


    virtual std::string name() { return "Selection"; };
     

    SelectionOp ( Expr* condition, 
                  RelOperator* child )
                 : RelOperator ( RelOperator::SELECTION ),
                   _condition ( condition ) {
        addChild ( child );
    }


    virtual ~SelectionOp() {};
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {
        deriveExpressionTypes ( _condition, identTypes );
    }

    
    virtual size_t getSize () {
        return children[0]->getSize() / 2;
    }

 
    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request ) {

        /* Save requested attributes and create own request for attributes *
         * from the selection condition                                    */
        _request = request;
        SymbolSet selectionReq = extractRequiredAttributes ( _condition ); 

        /* Request attributes from child */
        _child->produceFlounder ( ctx, symbolSetUnion ( _request, selectionReq ) );
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {
 
        /* Remove tuples that were only needed by the selection condition *
         * from the result.                                               */
        _schema = _child->_schema;
        if ( !ctx.requestAll ) {
            _schema = _schema.prune ( _request );
        }

        ctx.comment ( " --- Selection" );

        addExpressionIds ( _condition, &ctx.rel );
        ir_node* conditionResult = emitExpression ( ctx, _condition ); 
        ctx.yield ( cmp ( conditionResult, constInt8 ( 0 ) ) );
        ctx.yield ( je ( ctx.labelNextTuple ) ); 
        ctx.clear ( conditionResult );
    
        _parent->consumeFlounder ( ctx );
    }

};
