//#include <latch>



struct HashJoinState {


    /* synchronization point after build is finished */
    //std::latch _syncPointBuild;

  
    HashJoinState ( size_t numThreads ) /* : _syncPointBuild ( numThreads )*/ {} 


    static void syncBuild ( HashJoinState* state ) {
        //state->_syncPointBuild.arrive_and_wait();
    }

};


 
/**
 * @brief Hash Join operator.
 */
class HashJoinOp : public RelOperator {

public:

    /* join hash table and ir_node that holds the address of the hash table     *
     * data structure (jit constant).                                           */
    HashTable*  _ht = nullptr;
    ir_node*    _htAddr = nullptr;


    /* Remember schema of the build keys to access them later during probe.     */
    Schema      _schemaBuildKeys;


    /* Should join be able to produce multiple matches per probe key or not.    */
    bool        _singleMatch=false;

    
    /* Equality conditions for the hash join.                                   *
     *                                                                          *
     * Attributes from the left child operator side have to be on the left side *
     * of each equality. Attributes from the right child operator should be on  *
     * the right side of the equality.                                          * 
     * Otherwise the shape of the equalities is not restricted, e.g.:           * 
     *     eq (                                                                 *
     *         attr ( "a" ),                                                    *
     *         attr ( "b" )                                                     *
     *     )                                                                    */
    ExprVec     _equalities;


    /* Remember requested attributes to prune schema when the child operator's  *
     * schemas are available in consume.                                        */
    SymbolSet   _request;


    /* Number of times consumeFlounder(..) was called. Used to distinguish      *
     * between call from left child (first) and right child (second).           */
    int         _nCall = 0;


    /* state that concerns the hashjoin during execution */
    std::unique_ptr < HashJoinState > _state;

    

    virtual std::string name() { return "HashJoin"; };


    HashJoinOp ( std::vector < Expr* > equalities,
                 RelOperator* leftChild, 
                 RelOperator* rightChild )
         :  RelOperator ( RelOperator::HASHJOIN ),
           _equalities ( equalities )
    {
        addChild ( leftChild );
        addChild ( rightChild );

    }

    
    virtual ~HashJoinOp() {
        freeHashTable ( _ht );
    }
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {
        deriveExpressionTypes ( _equalities, identTypes );
    }

    
    virtual size_t getSize () {
        return _lChild->getSize() + _rChild->getSize() / 2;
    }
    
    
    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request ) {
         
        _request = request;

        _state = std::make_unique < HashJoinState > ( ctx.numThreads() );

        SymbolSet joinReq = extractRequiredAttributes ( _equalities ); 
        SymbolSet allReq  = symbolSetUnion ( _request, joinReq );

        _lChild->produceFlounder ( ctx, allReq ); 

        ir_node* foo = vreg64 ( "foo_sync" );
        ctx.request ( foo );
        ctx.yield ( mcall1 ( foo, (void*) &_state->syncBuild, constAddress ( _state.get() ) ) );
        ctx.clear ( foo );

        _rChild->produceFlounder ( ctx, allReq );
    }

    virtual void consumeMultiMatchProbe ( ir_node*             probeHash,
                                          ValueSet&            probeKeys,
                                          JitContextFlounder&  ctx ) {
        
        /* Prepare entry pointer for probe results  *
         * and preserve it too                      */ 
        ir_node* htProbeEntry = ctx.request ( vreg64 ( "htProbeEntry" ) );
        ctx.yield ( mov ( htProbeEntry, constAddress ( nullptr ) ) );

        /* Loop over ht entries with matching hash values */
        WhileLoop whileLoop = WhileTrue ( ctx.codeTree ); {

            /* update jump label for succeeding operators *
             * Todo: test selection after hash join       */
            ctx.labelNextTuple = whileLoop.headLabel;

            /* Probe hash table;       *
             * exit loop when no match */
            ctx.yield ( mcall3 ( htProbeEntry, (void*) &ht_get, _htAddr, probeHash, htProbeEntry ) );
            breakWhile ( whileLoop, isEqual ( htProbeEntry, constAddress ( nullptr ) ) ); 

            /* Dematerialize keys from entry and  *
             * check match with probe keys        */
            ValueSet entryKeys = Values::dematerialize ( htProbeEntry, _schemaBuildKeys, Values::htMatConfig, ctx );
            Values::checkEqualityJump ( probeKeys, entryKeys, whileLoop.headLabel, ctx );
            Values::clear ( entryKeys, ctx );

            /* Compute address the values in the hash table entry.       *
             * Then dematerialize the values and register their symbols. */
            ir_node* valueLoc = ctx.request ( vreg64 ( "buildValueLoc" ) );
            ctx.yield ( mov ( valueLoc, htProbeEntry ) );
            ctx.yield ( add ( valueLoc, constInt64 ( Values::byteSize ( entryKeys, false ) ) ) );
            ValueSet entryValues = Values::dematerialize ( valueLoc, _lChild->_schema, Values::htMatConfig, ctx );
            Values::addSymbols ( ctx, entryValues );

            /* Code from parent operator */
            _parent->consumeFlounder ( ctx );

            Values::clear ( entryValues, ctx );
            ctx.clear ( valueLoc );

        } closeWhile ( whileLoop );

        /* Clear explicit vregs */
        ctx.clear ( htProbeEntry );
        ctx.clear ( probeHash );
        Values::clear ( probeKeys, ctx );
    }
    

    virtual void consumeSingleMatchProbe ( ir_node*             probeHash,
                                           ValueSet&            probeKeys,
                                           JitContextFlounder&  ctx ) {
        
        /* Prepare entry pointer for probe results, destination label *
         * for matching probes, and size of the keys in ht entries.   */
        ir_node* htProbeEntry = ctx.request ( vreg64 ( "htProbeEntry" ) );
        ctx.yield ( mov ( htProbeEntry, constAddress ( nullptr ) ) );
        ir_node* foundMatch = idLabel ( "foundMatch" );
	size_t entryKeysByteSize = 0;

        /* Loop over ht entries with matching hash values */
        WhileLoop whileLoop = WhileTrue ( ctx.codeTree ); {

            /* Probe hash table and go to next tuple when no match */
            ctx.yield ( mcall3 ( htProbeEntry, (void*) &ht_get, _htAddr, probeHash, htProbeEntry ) );
            ctx.yield ( cmp ( htProbeEntry, constAddress ( nullptr ) ) );
            ctx.yield ( je ( ctx.labelNextTuple ) );        

            /* Dematerialize keys from entry and  *
             * check match with probe keys        */
            ValueSet entryKeys = Values::dematerialize ( htProbeEntry, _schemaBuildKeys, Values::htMatConfig, ctx );
            entryKeysByteSize = Values::byteSize ( entryKeys, Values::htMatConfig.stringsByVal );
            Values::checkEqualityJumpIfTrue ( probeKeys, entryKeys, foundMatch, ctx );
            Values::clear ( entryKeys, ctx );
        
        } closeWhile ( whileLoop );
        ctx.clear ( probeHash );
        Values::clear ( probeKeys, ctx );
        
        ctx.yield ( placeLabel ( foundMatch ) );

        /* Compute address the values in the hash table entry.       *
         * Then dematerialize the values and register their symbols. */
        ir_node* valueLoc = ctx.request ( vreg64 ( "buildValueLoc" ) );
        ctx.yield ( mov ( valueLoc, htProbeEntry ) );
        ctx.clear ( htProbeEntry );
        ctx.yield ( add ( valueLoc, constInt64 ( entryKeysByteSize ) ) );
        ValueSet entryValues = Values::dematerialize ( valueLoc, _lChild->_schema, Values::htMatConfig, ctx );
        Values::addSymbols ( ctx, entryValues );

        /* Code from parent operator */
        _parent->consumeFlounder ( ctx );

        Values::clear ( entryValues, ctx );
        ctx.clear ( valueLoc );
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {

        _nCall++;

        if ( _nCall > 2 ) {
            error_msg ( CODEGEN_ERROR, "HashJoin::consumeFlounder(..) called"
                                       " more than 2 times." );
        }
            
        if ( _nCall == 1 ) {
            ctx.comment ( " --- Hash join build" );

            /* prepare hash build keys */
            auto left = equalitiesLeftSide ( _equalities );
            ValueSet buildKeys = evalExpressions ( left, ctx ); 
            _schemaBuildKeys = Values::schema ( buildKeys, Values::htMatConfig.stringsByVal );
            ValueSet buildVals = Values::get ( _lChild->_schema, ctx ); 

            /* allocate hash table */
            size_t entrySize = Values::schema ( buildKeys, buildVals, Values::htMatConfig.stringsByVal )._tupSize;
            _ht = allocateHashTable ( _lChild->getSize() * 5 / 3, entrySize ); 
            _htAddr = constAddress ( _ht );

            /* hash build keys and insert the hash */
            ir_node* buildHash = Values::hash ( buildKeys, ctx ); 
            ir_node* htEntry = ctx.request ( vreg64 ( "htEntry" ) );
            ctx.yield ( mcall2 ( htEntry, (void*)&ht_put, _htAddr, buildHash ) );
            ctx.clear ( buildHash );

            /* Materialize build keys into hash table *
             * and move the pointer behind the keys.  */
            Values::materialize ( buildKeys, htEntry, Values::htMatConfig, ctx );
            Values::clear ( buildKeys, ctx );
            ctx.yield ( add ( htEntry, constInt64 ( Values::byteSize ( buildKeys, false ) ) ) );
 
            /* Materialize values behind the keys */
            Values::materialize ( buildVals, htEntry, Values::htMatConfig, ctx );
            Values::clear ( buildVals, ctx );
            ctx.clear ( htEntry );
        }

        else if ( _nCall == 2 ) {

            ctx.comment ( " --- Hash join probe" );
            _schema = _lChild->_schema.join ( _rChild->_schema );
            if ( ! ctx.requestAll ) {
                _schema = _schema.prune ( _request );
            }
 
            /* Evaluate hash probe keys..    */
            ExprVec right = equalitiesRightSide ( _equalities );
            ValueSet probeKeys = evalExpressions ( right, ctx ); 

            /* ..and hash them.              */
            ir_node* probeHash = Values::hash ( probeKeys, ctx );

            if ( !_singleMatch ) {
                consumeMultiMatchProbe ( probeHash, probeKeys, ctx );
            }
            else {
                consumeSingleMatchProbe ( probeHash, probeKeys, ctx );
            }
        }
    }
};

