
/**
 * @brief Operator for projecting a relation.
 */
class ProjectionOp : public RelOperator {

public:

    ExprVec _expr;


    virtual std::string name() { return "Projection"; };

    
    ProjectionOp ( ExprVec expr, RelOperator* child ) 
                 : RelOperator ( RelOperator::PROJECTION ),
                   _expr ( expr ) {

        if ( child != nullptr ) 
            addChild ( child );
        else _child = nullptr;
    }


    virtual ~ProjectionOp() {}

    
    virtual size_t getSize () {
        if ( _child ) 
            return _child->getSize();
        else return 1;
    }
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {
        if ( _child != nullptr ) {
            //_child->defineExpressions ( identTypes );
        }
        deriveExpressionTypes ( _expr, identTypes );
    }
   
 
    virtual void produceFlounder ( JitContextFlounder& ctx, 
                                   SymbolSet           request ) {

        
        /* Projection with children */
        if ( _child != nullptr ) {
            SymbolSet req = extractRequiredAttributes ( _expr );
            _child->produceFlounder ( ctx, req );
        }
 
        /* Leaf projection                            *
         * Creates a single-line table from constants */
        else {
            if ( ctx.rel.innerScanCount == 0 ) {
                ctx.openPipeline();
            }
            consumeFlounder ( ctx );
            if ( ctx.rel.innerScanCount == 0 ) {
                ctx.closePipeline();
            }
        }
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {
        
        ctx.comment ( " --- Projection" );

        ValueSet projVals = evalExpressions ( _expr, ctx );
        Values::addSymbols ( ctx, projVals );
        _schema = Values::schema ( projVals, true );
        _parent->consumeFlounder ( ctx );
        Values::clear ( projVals, ctx );
    };

};
