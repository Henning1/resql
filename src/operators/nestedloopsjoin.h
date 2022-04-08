
/**
 * @brief Join operator.
 */
class NestedLoopsJoinOp : public RelOperator {
    
public:

    Expr* _condition;
    bool _materializeInner;

    // Number of times consumeFlounder(..) was called.    
    int _nCall;


    virtual std::string name() { return "NestedLoopsJoin"; };


    NestedLoopsJoinOp ( Expr* condition, 
                        RelOperator* leftChild, 
                        RelOperator* rightChild )
        : RelOperator ( RelOperator::NESTEDLOOPSJOIN ),
          _condition ( condition ),
          _nCall ( 0 ) {

        addChild ( new MaterializeOp ( leftChild ) );
        addChild ( new MaterializeOp ( rightChild ) );
    }

    
    virtual ~NestedLoopsJoinOp() {}
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {
        if ( _condition != nullptr ) {
            deriveExpressionTypes ( _condition, identTypes );
        }
    }

    
    virtual size_t getSize () {
        size_t lSz = _lChild->getSize();
        size_t rSz = _rChild->getSize();

        if ( ( lSz + rSz ) <= 10000 ) {
            return lSz * rSz; 
        }
        else {
            return ( lSz + rSz )  * 2;
        }
    }


    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request ) {

        _lChild->produceFlounder ( ctx, request );
        _rChild->produceFlounder ( ctx, request );

        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.openPipeline();
        } 

        ((MaterializeOp*)_rChild)->produceScanTable ( ctx );
        
        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.closePipeline();
        } 
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {

        _nCall++; 

        if ( _nCall == 1 ) {
            ctx.rel.innerScanCount++;
            ((MaterializeOp*)_lChild)->produceScanTable ( ctx, true );
            ctx.rel.innerScanCount--;
        }
        else if ( _nCall == 2 ) {
            _schema = _lChild->_schema.join ( _rChild->_schema );
            if ( _condition != nullptr ) {
                addExpressionIds ( _condition, &ctx.rel );
                ir_node* conditionResult = emitExpression ( ctx, _condition ); 
                ctx.yield ( cmp ( conditionResult, constInt8 ( 0 ) ) );
                ctx.yield ( je ( ctx.labelNextTuple ) );        
                ctx.clear ( conditionResult );
            }
            _parent->consumeFlounder ( ctx );
        }
    }
};

