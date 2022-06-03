


struct AggregationState {
    
    SingleThreadGuard guard;

    AggregationState ( size_t numThreads ) : guard ( numThreads ) {} 

};



/**
 * @brief Aggregation operator.
 */
class AggregationOp : public RelOperator {

public: 

    /* Aggregation expressions from the query  *
     * e.g. min(d), avg(e)     */
    std::vector < Expr* > _aggExpr;


    /* Same as _aggExpr with averages split up *
     * into sum and count,                     *
     * e.g. min(d), sum (e), count(e)          */
    std::vector < Expr* > _splitAggExpr;
 
  
    /* Grouping expression,    *
     * e.g. group by a + b, c  */
    std::vector < Expr* > _groupExpr;     


    /* Aggregation hash table */
    HashTable*        _ht = nullptr;

    
    /* hash table entry schema */
    Schema _entrySchema;


    /* state that concerns the aggregation during execution */
    std::unique_ptr < AggregationState > _state;


    ir_node* endSingleThreadLabel;


    virtual std::string name() { return "Aggregation"; };


    AggregationOp ( std::vector < Expr* > aggExpr, 
                    std::vector < Expr* > groupExpr,
                    RelOperator* child )
                     :  RelOperator ( RelOperator::AGGREGATION ),
                        _aggExpr ( aggExpr ),
                        _groupExpr ( groupExpr) {

        addChild ( child );
    }

    
    virtual ~AggregationOp() {
        if (_ht != nullptr ) {
            freeHashTable ( _ht );
        }
    };
    

    void defineExpressions ( ExpressionContext& ctx ) {
        _splitAggExpr = splitAverages ( _aggExpr );
        ctx.define ( _groupExpr );
        ctx.define ( _splitAggExpr );
        ctx.define ( _aggExpr );
    }

    
    virtual size_t getSize () {
        if ( _groupExpr.size() == 0 ) {
            return 1;
        }
        else {
            int sizeReduction = 512;
            for ( size_t i=1; i<_groupExpr.size() && sizeReduction > 2; i++ ) {
                sizeReduction = sizeReduction / 2;
            }
            return _child->getSize ( ) / sizeReduction;
        }
    }


    void updateAggregates ( ValueSet&               valuesTable, 
                            std::vector < Expr*  >  aggExpr, 
                            ValueSet&               aggVals,
                            JitContextFlounder&     ctx ) {

        for ( size_t i = 0; i < valuesTable.size(); i++ ) {
            Value& accumulator = valuesTable[i];
            Value& increment   = aggVals[i];

            switch ( aggExpr[i]->tag ) {

                case Expr::COUNT: {
                    ctx.yield ( inc ( accumulator.node ) );
                    break;
                }

                case Expr::SUM: {
                    ir_node* addRes = emitAdd ( ctx, 
                                                accumulator.type, 
                                                accumulator.node, 
                                                increment.node ); 
                    ctx.yield ( mov ( accumulator.node, addRes ) );
                    ctx.clear ( addRes );
                    break;
                }

                case Expr::MIN: {
                    ir_node* isLT = emitLessThan ( ctx, 
                                                   increment.type, 
                                                   increment.node, 
                                                   accumulator.node );
                    
                    IfClause if_ = If ( isEqual ( isLT, constInt8 ( 1 ) ), ctx.codeTree ); {
                        ctx.yield ( mov ( accumulator.node, increment.node ) ); 
                    } closeIf ( if_ );
                    ctx.clear ( isLT );
                    break;
                }

                case Expr::MAX: {
                    ir_node* isGT = emitGreaterThan ( ctx, 
                                                      increment.type, 
                                                      increment.node, 
                                                      accumulator.node );
                    
                    IfClause if_ = If ( isEqual ( isGT, constInt8 ( 1 ) ), ctx.codeTree ); {
                        ctx.yield ( mov ( accumulator.node, increment.node ) ); 
                    } closeIf ( if_ );
                    ctx.clear ( isGT );
                    break;
                }

                default:
                    error_msg ( NOT_IMPLEMENTED, "Aggregation type not "
                        "implemented in updateAggregates(..)." );
            }
        }
    }

    
    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request ) {
        
        _state = std::make_unique < AggregationState > ( ctx.numThreads() );

        SymbolSet aggReq   = extractRequiredAttributes ( _aggExpr ); 
        SymbolSet groupReq = extractRequiredAttributes ( _groupExpr ); 
        _child->produceFlounder ( ctx, symbolSetUnion ( aggReq, groupReq ) );
        this->consumeAggregateFlounder ( ctx );
    }


    static ExprVec splitAverages ( ExprVec& aggs ) {
        ExprVec res;
        for ( auto& e : aggs ) {
            if ( e->tag == Expr::AVG ) {
                res.push_back ( ExprGen::sum ( e->child ) ); 
                res.push_back ( ExprGen::count ( e->child ) ); 
            }
            else {
                res.push_back ( e );
            }
        }
        return res;
    }


    static ir_node* getAvgFromSumAndCount ( Value&               sum, 
                                            Value&               count, 
                                            JitContextFlounder&  ctx ) {

        ir_node* res = nullptr;
        switch ( sum.type.tag ) {
            case SqlType::BIGINT: {
                ir_node* scaled = emitMulDECIMALxBIGINT ( ctx, sum.node, constInt64 ( 100 ) ); 
                res = emitDivBIGINT ( ctx, scaled, count.node ); 
                ctx.clear ( scaled );
                break;
            }
            case SqlType::DECIMAL: {
                ir_node* scaled = emitMulDECIMALxBIGINT ( ctx, sum.node, constInt64 ( 100 ) ); 
                res = emitDivBIGINT ( ctx, scaled, count.node );
                ctx.clear ( scaled );
                break;
            }
            default:
                error_msg ( NOT_IMPLEMENTED, "getAvgFromSumAndCount(..) not supported for datatype" );
        }
        return res;
    }
    

    static ValueSet mergeAverages ( ExprVec&             aggs, 
                                    ValueSet&            vals,
                                    size_t               firstAggIdx,
                                    JitContextFlounder&  ctx ) {
        ValueSet result;
        size_t aggIdx = 0;
        for ( size_t i = 0; i < vals.size(); i++ ) {
     
            if ( i < firstAggIdx ) {
                result.push_back ( vals [ i ] );
                continue;
            }
    
            if ( aggs [ aggIdx ]->tag == Expr::AVG ) {
                /* do merge */
                Expr*  avgExpr = aggs [ aggIdx ];
                addExpressionIds ( avgExpr, &ctx.rel );
                Value& sum   = vals[i++];
                Value& count = vals[i];
                ir_node* avg = getAvgFromSumAndCount ( sum, count, ctx );
                ctx.clear ( sum.node );
                ctx.clear ( count.node );
                result.push_back ( { avg, avgExpr->type, getExpressionName ( avgExpr ) } );
            } 
            else {
                /* keep */
                result.push_back ( vals [ i ] );
            }
            aggIdx++;
        }
        return result;
    }

    virtual void consumeFlounder ( JitContextFlounder& ctx ) {
        
        ctx.comment ( " --- Hash aggregation" );

        _state->guard.open ( ctx.pipeHeader );

        ValueSet groupVals = evalExpressions ( _groupExpr, ctx );
        ValueSet aggVals   = evalExpressions ( _splitAggExpr, ctx );

        _entrySchema = Values::schema ( groupVals, aggVals, Values::htMatConfig.stringsByVal ); 
        size_t groupOffset = Values::byteSize ( groupVals, Values::htMatConfig.stringsByVal );
        ir_node* groupHash = Values::hash ( groupVals, ctx ); 

        _ht = allocateHashTable ( getSize(), _entrySchema._tupSize );

        ir_node* htPtr = constLoad ( constAddress ( _ht ) );
        ir_node* htEntry = ctx.request ( vreg64 ( "htEntry" ) );
        ctx.yield ( mov ( htEntry, constAddress ( nullptr ) ) );
        ir_node* entryFound = ctx.request ( vreg8 ( "entryFound" ) );
        ctx.yield ( mov ( entryFound, constInt8 ( 0 ) ) );

        /* check hash table entry */
        WhileLoop whileLoop = While ( isNotEqual ( entryFound, constInt8 ( 1 ) ), ctx.codeTree ); {
            ctx.yield ( mcall3 ( htEntry, (void*) &ht_get, htPtr, groupHash, htEntry ) );
            breakWhile ( whileLoop, isEqual ( htEntry, constAddress ( nullptr ) ) ); 
            ValueSet groupValsProbe = Values::dematerialize ( htEntry, groupVals, Values::htMatConfig, ctx );
            Values::checkEqualityBool ( groupVals, groupValsProbe, entryFound, ctx );
            Values::clear ( groupValsProbe, ctx );
        } closeWhile ( whileLoop );

        /* new group - insert */
        ctx.comment ( "Materialize aggregation HT entry." );
        IfClause if1 = If ( isEqual ( entryFound, constInt8 ( 0 ) ), ctx.codeTree ); {
            ctx.yield ( mcall2 ( htEntry, (void*) &ht_put, htPtr, groupHash ) );
            Values::materialize ( groupVals, htEntry, Values::htMatConfig, ctx );
            Values::clear ( groupVals, ctx );
            ctx.yield ( add ( htEntry, constInt64 ( groupOffset ) ) );
            Values::MaterializeConfig mConf = { Values::htMatConfig.stringsByVal, false };
            Values::materialize ( aggVals, htEntry, mConf, ctx );
        } closeIf ( if1 );

        ctx.clear ( groupHash );
        
        /* existing group - update */
        IfClause if2 = If ( isEqual ( entryFound, constInt8 ( 1 ) ), ctx.codeTree ); {
            ctx.clear ( entryFound );
            ctx.yield ( add ( htEntry, constInt64 ( groupOffset ) ) );
            ValueSet valuesTable = Values::dematerialize ( htEntry, aggVals, Values::htMatConfig, ctx );
            updateAggregates ( valuesTable, _splitAggExpr, aggVals, ctx );
            Values::materialize ( valuesTable, htEntry, Values::htMatConfig, ctx );
            Values::clear ( valuesTable, ctx );
        } closeIf ( if2 );

        ctx.clear ( htEntry );
        Values::clear ( aggVals, ctx ); 
    }


    virtual void consumeAggregateFlounder ( JitContextFlounder& ctx ) {

        ctx.comment ( " --- Scan aggregation hash table" );
        
        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.openPipeline();
        } 

        /* Scan hash table */
        ScanLoop scan = openScanLoop ( memAt ( constLoad ( constAddress ( &_ht->entries ) ) ), 
                                       memAt ( constLoad ( constAddress ( &_ht->entriesEnd ) ) ), 
                                       _ht->fullEntrySize, 
                                       ctx ); {
            
            /* Check hash to skip empty buckets */
            ir_node* entryStatus = ctx.request ( vreg8 ( "htEntryStatus" ) );
            ctx.yield ( mov ( entryStatus, memAt ( scan.tupleCursor ) ) );
            ctx.yield ( cmp ( entryStatus, constInt8 ( 0 ) ) );
            ctx.yield ( je ( scan.nextTuple ) );
            ctx.clear ( entryStatus );
            
            /* Dematerialize groups and aggregates */
            ir_node* tupleAddr = ctx.request ( vreg64 ( "tupleAddr" ) );
            ctx.yield ( mov ( tupleAddr, constInt64 ( sizeof ( Entry ) ) ) );
            ctx.yield ( add ( tupleAddr, scan.tupleCursor ) ); 
            ValueSet tableValues = Values::dematerialize ( tupleAddr, _entrySchema, Values::htMatConfig, ctx );
            //ctx.clear ( tupleAddr );
            ValueSet groupsAndAggregates = mergeAverages ( _aggExpr, tableValues, _groupExpr.size(), ctx );
            _schema = Values::schema ( groupsAndAggregates, true );

            Values::addSymbols ( ctx, groupsAndAggregates );
 
            _parent->consumeFlounder ( ctx );

            Values::clear ( groupsAndAggregates, ctx );
            ctx.clear ( tupleAddr );
        
        } closeScanLoop ( scan, ctx );

        _state->guard.close ( ctx.pipeFooter );

        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.closePipeline();
        }     

    }
        
};


