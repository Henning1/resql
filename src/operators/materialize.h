

/**
 * @brief Materialization operator for JIT-compilation.
 * This operator is responsible for writing the current tuple in JIT-code 
 * to the result relation. It is attached to plans as root to trigger materialization
 * of register-contents to memory. Other execution models materialize to memory without the operator.
 */
class MaterializeOp : public RelOperator {

public:
        
    int _nCall;

    /* limit */
    bool _hasLimitClause;
    size_t _limit;
    ir_node* _count;
    ir_node* _labelExit;

    std::unique_ptr<Relation> relOut;
    Relation::AppendIterator _appendIt;
    Relation::ReadIterator _readIt;
            
    void (*refreshFunc)( Relation::ReadIterator* ) = Relation::ReadIterator::refresh;

    virtual std::string name() { 
        return "Materialize"; 
    }


    MaterializeOp ( RelOperator*  child )
         :  RelOperator ( RelOperator::SELECTION ),
            _nCall ( 0 ), _hasLimitClause ( false ) {
        
        addChild ( child );
    };
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {};
    

    virtual bool isMaterializedOperator() {
        return true;
    }
    

    virtual std::unique_ptr < Relation > retrieveResult() {
        return std::move ( relOut );
    }
   

    void addLimit ( size_t limit ) {
        _hasLimitClause = true;
        _limit = limit;
    } 

    
    virtual size_t getSize () { 
        size_t size = _child->getSize(); 
        if ( _hasLimitClause ) {
            size = std::min ( _limit, size );
        }
        return size;
    } 


    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request = {} ) {

        if ( _child == nullptr ) throw ResqlError ( "Child of MaterializeOp is nullptr" );

        /* Request all attributes from _child */
        _child->produceFlounder ( ctx, request );
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {
            
        DataBlock* (*getBlockFunc)( Relation::AppendIterator* ) = Relation::AppendIterator::getBlock;
        void (*updateFunc)( DataBlock*, Data* ) = DataBlock::updateContentSize;
        void (*updateFunc2)( DataBlock*, Data* ) = DataBlock::updateContentSize2;
        Data* (*blockEndFunc)( DataBlock* ) = DataBlock::end2;
        Data* (*blockCapacityEndFunc)( DataBlock* ) = DataBlock::capacityEnd;
        
        _nCall++;

        if ( _nCall > 1 ) {
            error_msg ( CODEGEN_ERROR, "Double consumeFlounder(..) in MaterializeOp." );
        }

        ctx.comment ( " --- Materialize" );

        _schema = _child->_schema;
        relOut = std::make_unique < Relation > ( Relation ( _schema ) );
        _appendIt = Relation::AppendIterator ( relOut.get() );

        ir_node* outBlock = vreg64 ( "outBlock" );
        ctx.yieldPipeHead ( request ( outBlock ) );
        ctx.yieldPipeHead (
            mcall1 ( outBlock,
                     (void*) getBlockFunc, 
                     constAddress ( &_appendIt )
            )              
        ); 
        
        ir_node* outputCursor = vreg64 ( "outputCursor" );
        ctx.yieldPipeHead ( request ( outputCursor ) );
        ctx.yieldPipeHead (
            mcall1 ( outputCursor,
                     (void*) blockEndFunc, 
                     outBlock
            )              
        ); 
        
        ir_node* outBlockCapacityEnd = vreg64 ( "outBlockCapacityEnd" );
        ctx.yieldPipeHead ( request ( outBlockCapacityEnd ) );
        ctx.yieldPipeHead (
            mcall1 ( outBlockCapacityEnd,
                     (void*) blockCapacityEndFunc, 
                     outBlock
            )              
        ); 
        
        //ir_node* outputCursor = vreg64 ( "outputCursor" );
        //ctx.yield ( request ( outputCursor ) );
        //Data* (*getFunc)(Relation::AppendIterator* it) = Relation::AppendIterator::get;
        //ctx.yield (
        //    mcall1 ( outputCursor,
        //             (void*) getFunc,
        //             constAddress ( &_appendIt )
        //    )              
        //); 

        if ( _hasLimitClause ) {
            _count = vreg64 ( "count" );
            ctx.yieldPipeHead ( request ( _count ) );
            ctx.yieldPipeHead ( mov ( _count, constInt64 ( 0 ) ) );
            _labelExit = idLabel ( "exit" );
        }

        /* append block if necessary */
        ir_node* tupleEnd = vreg64 ( "tupleEnd" );
        ctx.yield ( request ( tupleEnd ) );
        ctx.yield ( mov ( tupleEnd, outputCursor ) );
        ctx.yield ( add ( tupleEnd, constInt64 ( _appendIt.Step ) ) ); 
        IfClause if_ = If ( isLargerEqual ( 
                               tupleEnd, 
                               outBlockCapacityEnd),
                           ctx.codeTree ); {

            ctx.clear ( tupleEnd );
        
            ir_node* foo = vreg64 ( "foo" );
            ctx.request ( foo );
            ctx.yield (
                mcall2 ( foo,
                         (void*) updateFunc, 
                         outBlock,
                         outputCursor
                )              
            ); 
            ctx.clear ( foo );
            
            ctx.yield (
                mcall1 ( outBlock,
                         (void*) getBlockFunc, 
                         constAddress ( &_appendIt )
                )              
            ); 
            
            ctx.yield (
                mcall1 ( outputCursor,
                         (void*) blockEndFunc, 
                         outBlock
                )              
            ); 
            
            ctx.yield (
                mcall1 ( outBlockCapacityEnd,
                         (void*) blockCapacityEndFunc, 
                         outBlock
                )              
            ); 
        } closeIf ( if_ );

        /* materialize tuple and step to next */
        ValueSet matValues = Values::get ( _schema, ctx ); 
        Values::materialize ( matValues, outputCursor, Values::relationMatConfig, ctx );
        ctx.yield ( add ( outputCursor, constInt64 ( _appendIt.Step ) ) ); 
        
        /* implement limit */
        if ( _hasLimitClause ) {
            ctx.yield ( inc ( _count ) ); 
            IfClause if_ = If ( isLargerEqual ( _count, constInt64 ( _limit ) ),
                               ctx.codeTree ); {

                ctx.yield ( jmp ( _labelExit ) );

            } closeIf ( if_ );
        }        

        if ( _hasLimitClause ) {
            ctx.yieldPipeFoot ( placeLabel ( _labelExit ) );
            ctx.yieldPipeFoot ( clear ( _count ) );
        }

        /* Retrieve materialization size */
        ctx.yieldPipeFoot (
            mcall2 ( outputCursor,
                     (void*) updateFunc2, 
                     outBlock,
                     outputCursor
            )              
        );

        ctx.yieldPipeFoot ( clear ( outputCursor ) );
        ctx.yieldPipeFoot ( clear ( outBlock ) );
        ctx.yieldPipeFoot ( clear ( outBlockCapacityEnd ) );
    }


    virtual void produceScanTable ( JitContextFlounder& ctx, bool refreshIterator = false ) {
    
        _readIt = Relation::ReadIterator ( relOut.get() );

        if ( refreshIterator ) {
            ir_node* foo = vreg64 ( "foo" );
            ctx.yield ( request ( foo ) );
            ctx.yield (
                mcall1 ( foo,
                         (void*) refreshFunc, 
                         constAddress ( &_readIt )
                )              
            ); 
            ctx.yield ( clear ( foo ) );
        }

        // scan loop        
        if ( _parent != nullptr ) {

            { BlockScan scan ( &_readIt, ctx );
                
                // read tuple into registers
                auto scanVals = Values::dematerialize ( scan.tupleCursor(), 
                                                        relOut->_schema,
                                                        Values::relationMatConfig,
                                                        ctx );
                
                // register symbols
                Values::addSymbols ( ctx, scanVals ); 

                // parent operator code
                _parent->consumeFlounder ( ctx );
 
                Values::clear ( scanVals, ctx );
            }
        }

    }
    
};
