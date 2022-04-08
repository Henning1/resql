

struct ScanLoop {
    std::size_t step;
    ir_node* tupleCursor;
    ir_node* relationEnd;
    ir_node* nextTuple;
    WhileLoop loop;
};


ScanLoop openScanLoop ( ir_node*             begin, 
                        ir_node*             end, 
                        std::size_t          step,
                        JitContextFlounder&  ctx ) {
    ScanLoop scan;  
    scan.step = step;

    // relation iterator 
    scan.tupleCursor = vreg64 ( "tupleCursor" ); 
    ctx.yield ( request ( scan.tupleCursor ) );
    ctx.yield ( mov ( scan.tupleCursor, begin ) );
    if ( end->nodeType == MEM_AT ) {
        scan.relationEnd = vreg64 ( "relEnd" ); 
        ctx.yield ( request ( scan.relationEnd ) );
        ctx.yield ( mov ( scan.relationEnd, end ) );
    }
    else {
        scan.relationEnd = end; 
    }
        
    // create label for being able to jump to the next tuple     
    scan.nextTuple = idLabel ( "nextTuple" );
    ctx.labelNextTuple = scan.nextTuple;

    // loop over in-memory relation 
    scan.loop = While ( isSmaller ( scan.tupleCursor, scan.relationEnd ), ctx.codeTree );
    return scan;
}


void closeScanLoop ( ScanLoop& scan, JitContextFlounder& ctx ) {

    ctx.comment ( " --- Scan loop tail" );
    ctx.yield ( placeLabel ( scan.nextTuple ) );

    // move pointer to next tuple
    ctx.yield ( add ( scan.tupleCursor, constInt64 ( scan.step ) ) );
    closeWhile ( scan.loop );

    ctx.yield ( clear ( scan.tupleCursor ) );
    if ( isVreg ( scan.relationEnd ) ) {
        ctx.yield ( clear ( scan.relationEnd ) );
    }
}


struct BlockScan {

    /* state */
    WhileLoop _loopBlocks;
    ScanLoop _loopScan;
    ir_node* _block;
    ir_node* _blockBegin;
    ir_node* _blockEnd;

    /* references */
    Relation::ReadIterator* readIt;
    JitContextFlounder& ctx;

    /* library function pointers */
    DataBlock* (*getBlockFunc) ( Relation::ReadIterator* ) = Relation::ReadIterator::getBlock;
    Data* (*blockBeginFunc) ( DataBlock* ) = DataBlock::begin;
    Data* (*blockEndFunc) ( DataBlock* ) = DataBlock::end;


    BlockScan ( Relation::ReadIterator* readIt, JitContextFlounder& ctx ) : 
        readIt ( readIt ), ctx ( ctx ) {

        _block = ctx.request ( vreg64 ( "inBlock" ) );
        ctx.yield (
            mcall1 ( _block,
                     (void*) getBlockFunc, 
                     constAddress ( readIt )
            )              
        ); 

        _loopBlocks = While ( isNotEqual ( _block, constAddress ( nullptr ) ), ctx.codeTree ); {
            _blockBegin = ctx.request ( vreg64 ( "inBlockBegin" ) );
            _blockEnd = ctx.request ( vreg64 ( "inBlockEnd" ) );
        
            ctx.yield (
                mcall1 ( _blockBegin,
                         (void*) blockBeginFunc, 
                         _block
                )              
            ); 
            
            ctx.yield (
                mcall1 ( _blockEnd,
                         (void*) blockEndFunc, 
                         _block
                )              
            ); 

            _loopScan = openScanLoop ( _blockBegin, _blockEnd, readIt->Step, ctx ); {
                 /*************
                  * loop body *
                  *************/
            }
        }
    }

    ~BlockScan() {
        {
            {
                 /*************
                  * loop body *
                  *************/
            } closeScanLoop ( _loopScan, ctx );

            ctx.yield (
                mcall1 ( _block,
                         (void*) getBlockFunc, 
                         constAddress ( readIt )
                )              
            ); 

        } closeWhile ( _loopBlocks );
            
        ctx.clear ( _block );
        ctx.clear ( _blockBegin );
        /* _blockEnd is currently cleared by ScanLoop */
    }

    ir_node* tupleCursor () {
        return _loopScan.tupleCursor;
    }
};


/**
 * @brief Operator for scanning a relation.
 */
class ScanOp : public RelOperator {

public:

    Relation* _rel;
    std::string relationName;

    Relation::ReadIterator _readIt;
    DataBlock* _currentBlock = nullptr;

    virtual std::string name() { 
        std::string name;
        if ( relationName.length() == 0 ) name = "Scan";
        else name = relationName;
        return  name + "(" + std::to_string ( _rel->tupleNum() ) + ")"; 
    }

    
    ScanOp ( Relation* r, std::string relationName="" )
         : RelOperator ( RelOperator::SCAN ),
           _rel ( r ), 
           _readIt ( Relation::ReadIterator ( r ) ) {

        if ( r == nullptr ) throw ResqlError ( "Scan relation is nullptr." );
 
        std::transform ( relationName.begin(), 
                         relationName.end(),
                         relationName.begin(), 
                         ::toupper );

        this->relationName=relationName;
    }


    virtual ~ScanOp() {}
    

    virtual void defineExpressions ( std::map <std::string, SqlType>&  identTypes ) {}

    
    virtual size_t getSize () {
        return _rel->tupleNum();
    }
   
 
    virtual void produceFlounder ( JitContextFlounder& ctx,
                                   SymbolSet           request ) {

        if ( ctx.requestAll ) request = {}; 
        
        ctx.comment ( " --- Scan " + relationName );
        
        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.openPipeline();
        } 


        // scan loop        
        { BlockScan scan ( &_readIt, ctx );

            // read tuple into registers
            auto scanVals = Values::dematerialize ( scan.tupleCursor(), 
                                                    _rel->_schema,
                                                    Values::relationMatConfig,
                                                    ctx,
                                                    request );

            _schema = Values::schema ( scanVals, true );
            
            Values::addSymbols ( ctx, scanVals ); 

            // parent operator code
            _parent->consumeFlounder ( ctx );

            Values::clear ( scanVals, ctx );
        }

    
        if ( ctx.rel.innerScanCount == 0 ) {
            ctx.closePipeline();
        }
    }


    virtual void consumeFlounder ( JitContextFlounder& ctx ) {};


    virtual RelOperator* copyImpl ( std::vector<RelOperator*> childCopies ) {
        return new ScanOp ( _rel );
    }

};
