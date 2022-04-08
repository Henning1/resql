/**
 * @file
 * Database types, data access, data generation, etc.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include "schema.h"
#include "values.h"
#include "util/defs.h"
#include "util/Timer.h"
#include "util/ResqlError.h"


const size_t align = 64;


struct DataBlock {


    static const size_t Size = 2 << 20;

    
    size_t _contentSize = 0;


    std::unique_ptr < Data[] > _data;


    DataBlock() {
        _data = std::make_unique<Data[]> ( Size );
    }

 
    Data* begin() {
        return _data.get();
    }
    
 
    static Data* begin ( DataBlock* block ) {
        return block->begin();
    }
    

    Data* end() {
        return _data.get() + _contentSize;
    }
    

    static Data* end ( DataBlock* block ) {
        return block->end();
    }
    

    static Data* end2 ( DataBlock* block ) {
        return block->end();
    }
    

    Data* capacityEnd() {
        return _data.get() + DataBlock::Size;
    }
    

    static Data* capacityEnd ( DataBlock* block )  {
        return block->capacityEnd();
    }
    

    void updateContentSize ( size_t len ) {
        if ( len > Size ) {
            throw ResqlError ( "Len larger than block." );
        }
        _contentSize = len;
    }
    

    void updateContentSize ( Data* endWrite ) { 
        if ( endWrite > _data.get() + Size ) {
            throw ResqlError ( "Write after block end." );
        }
        _contentSize = endWrite - _data.get();
    }
   
 
    static void updateContentSize ( DataBlock* block, Data* endWrite ) {
        //std::cout << "update1" << std::endl;
        //std::cout << "block " << (void*)block << std::endl;
        block->updateContentSize ( endWrite );
    }
    
    static void updateContentSize2 ( DataBlock* block, Data* endWrite ) {
        //std::cout << "update2" << std::endl;
        block->updateContentSize ( endWrite );
    }

};


struct Relation {
    

    struct ReadIterator {
    

        /* Step size for adding tuples */
        size_t Step;
    
       
        /* Relation that we append to */ 
        Relation* rel;
    
    
        /* _blockNum indicates the index of the block *
         * that we currently append to.               */
        int _blockIndex = -1;


        Data* _pos = nullptr;
       

        bool _finished = false;


        std::mutex _blockMutex;
        

        ReadIterator() : Step ( 0 ), rel ( nullptr ) {}
 
    
        ReadIterator ( Relation* rel ) 
            : Step ( rel->_schema._tupSize ), rel ( rel ) {

            if ( rel->_schema._tupSize > DataBlock::Size ) {
                throw ResqlError ( "Tuple size larger than block size." );
            }

   
            if ( rel->_dataBlocks.size() == 0 ) {
                _finished = true;
            }
        }
        

        /* assignment */
        ReadIterator& operator= ( ReadIterator& other) = delete;


        ReadIterator& operator= ( ReadIterator&& other) noexcept {
            /* should not be performed during parallel iteration */
            if (this != &other) { 
                this->Step = other.Step;
                this->rel = other.rel;
                this->_blockIndex = other._blockIndex;
                this->_pos = other._pos;
                this->_finished = other._finished;
            }
            return *this; 
        }


        DataBlock* block() {
            return rel->_dataBlocks[_blockIndex].get();
        }
        

        DataBlock* getBlock() {
            const std::lock_guard<std::mutex> lock ( _blockMutex );
            _blockIndex++;
            if ( _blockIndex >= (int)rel->_dataBlocks.size() ) {
                _finished = true;
                return nullptr;
            }
            _pos = block()->begin();
            return block();
        }
        

        static DataBlock* getBlock ( ReadIterator* it )  {
            return it->getBlock();
        }
        
    
        Data* get() {
            if ( _finished ) {
                return nullptr;
            } 
            if ( _pos == nullptr || _pos >= block()->end() ) {
                getBlock();
                return get();
            }
            Data* result = _pos;
            _pos += Step;
            return result;
        }


        void refresh() {
            _blockIndex = -1;
            _pos = nullptr;
            _finished = false;
        }
        

        static void refresh ( ReadIterator* it ) {
            it->refresh();
        }

    };


    struct AppendIterator {
    

        /* Step size for adding tuples */
        size_t Step;
    
       
        /* Relation that we append to */ 
        Relation* rel;
    
    
        /* _blockNum indicates the index of the block *
         * that we currently append to.               */
        int _blockIndex;
        

        std::mutex _blockMutex;
        

        AppendIterator() : Step ( 0 ), rel ( nullptr ) {};
        
    
        AppendIterator ( Relation* rel ) 
            : Step ( rel->_schema._tupSize ), rel ( rel ) {
    
            if ( rel->_dataBlocks.size() == 0 ) {
                _blockIndex = -1;
            }
            else {
                _blockIndex = rel->_dataBlocks.size() - 1;
            }
        };

    
        AppendIterator& operator= ( AppendIterator& other) = delete;


        AppendIterator& operator= ( AppendIterator&& other) noexcept {
            /* should not be performed during parallel iteration */
            if (this != &other) { 
                this->Step = other.Step;
                this->rel = other.rel;
                this->_blockIndex = other._blockIndex;
            }
            return *this; 
        }


        DataBlock* block() {
            return rel->_dataBlocks[_blockIndex].get();
        }
        

        DataBlock* getBlock() {
            const std::lock_guard<std::mutex> lock ( _blockMutex );
            rel->addBlock();
            _blockIndex++;
            return block();
        }
        

        static DataBlock* getBlock ( Relation::AppendIterator* it ) {
            return it->getBlock();
        }
        
    
        Data* get() {
            if ( _blockIndex == -1 || block()->capacityEnd() < block()->end() + Step ) {
                getBlock();
            }
            Data* tupBegin = block()->end();
            block()->updateContentSize ( tupBegin + Step );
            return tupBegin;
        }
        

        static Data* get ( Relation::AppendIterator* it ) {
            return it->get();
        }

    };


    struct RandomAccessIterator {
    

        /* Step size for adding tuples */
        size_t Step;
        

        /* relation length in tuples */
        size_t _len;
    
       
        /* Relation that we append to */ 
        Relation* rel;


        /* inclusive prefix sum on block lengths */
        std::vector < size_t > _blockEnds;
        std::vector < size_t > _blockStarts;

   
        size_t numBlocks;
    
    
        RandomAccessIterator() : Step ( 0 ), rel ( nullptr ) {};
 
    
        RandomAccessIterator ( Relation* rel ) 
            : Step ( rel->_schema._tupSize ), rel ( rel ) {

            _blockEnds.resize ( rel->_dataBlocks.size() );
            _blockStarts.resize ( rel->_dataBlocks.size() );

            /* compute prefix sum */            
            size_t sum = 0;
            int i = 0;
            for ( auto& block : rel->_dataBlocks ) {
                _blockStarts[i] = sum;
                sum += block->_contentSize / rel->_schema._tupSize;
                _blockEnds[i] = sum - 1; /* last valid index in block is size-1 */
                i++;
            }

            _len = sum;
        };


        Data* get ( size_t index ) {
            
            /// get block index
            ///  - evaluate to which block index belongs
            auto end = std::lower_bound ( &_blockEnds[0], &_blockEnds[ _blockEnds.size()], index );
            size_t b = (size_t)(end - &_blockEnds[0]);
           
 
            // access block
            size_t block_offset = index - _blockStarts[b];
            return rel->_dataBlocks[b]->_data.get() + block_offset * rel->_schema._tupSize;
        }
    };


    /* constructors / destructor */
    Relation() = default;


    Relation ( Schema s ) : _schema(s) {
        if ( _schema._tupSize > DataBlock::Size ) {
            throw ResqlError ( "Tuple size larger than block size." );
        }
    }
    

    Relation ( const Relation& other ) = delete;
    

    Relation ( Relation&& other ) {
        this->_schema = other._schema;
        this->_dataBlocks = std::move ( other._dataBlocks );
    }

    /* assignment */
    Relation& operator= ( Relation& other) = delete;


    Relation& operator= ( Relation&& other) noexcept {
        if (this != &other) { 
            this->_schema = other._schema;
            this->_dataBlocks = std::move ( other._dataBlocks );
        }
        return *this; 
    }


    void addBlock ( ) {   
        _dataBlocks.emplace_back ( std::make_unique<DataBlock> () );
        //std::cout << "all block addresses:" << std::endl;
        //for ( auto& b : _dataBlocks ) {
        //    std::cout << (void*) b.get() << std::endl;
        //}
    }


    /* todo: check if correct */ 
    void applyLimit ( size_t limit ) {
        if ( _dataBlocks.size() == 0 ) return;
        size_t start = 0;
        size_t sum = 0;
        size_t i = 0;
        DataBlock* block;
        while ( i < _dataBlocks.size() && sum < limit ) {
           start = sum;
           sum += ( _dataBlocks[i]->_contentSize / _schema._tupSize );
           block = _dataBlocks[i].get(); 
           i++;
        }
        if ( i < _dataBlocks.size() ) {
            _dataBlocks.resize ( i );
        }
        if ( sum > limit ) {
            block->updateContentSize ( ( limit - start ) * _schema._tupSize );
        }
    }
    

    size_t tupleNum ( ) {
        size_t res = 0;
        for ( auto& d : _dataBlocks ) {
            res +=  ( d->_contentSize / _schema._tupSize );
        }
        return res;
    }


    /* attributes */
    Schema _schema;


    std::vector < std::unique_ptr < DataBlock > > _dataBlocks;

    
    /* serialization                                                         *
     * we have to split save and load here, because the size of _data cannot *
     * be determined by cereal. Therefore we pass it as binary data with     *
     * given size and have to perform the allocation when loading.           */
    template < class Archive >
    void save ( Archive& ar ) const {
        ar ( _schema );
        //ar ( cereal::binary_data(_data.get(), end() - begin()) ); 
    } 
    
    template < class Archive >
    void load ( Archive& ar ) {
        ar ( _schema );
        /* now capactiy is known and we can allocate */
        //_data = std::make_unique<Data[]> ( _capacity * _schema._tupSize );
        //ar ( cereal::binary_data ( _data.get(), end() - begin() ) );
    } 
};


struct Database {
    std::map < std::string, Relation > relations;

    Relation& operator[] ( std::string name ) {
        return relations[name];
    }
};


SymbolSet relationSymbols ( Relation& rel ) {
    SymbolSet res;
    for ( auto& a : rel._schema._attribs ) {
        res.insert ( a.name );
    }
    return res;
}


class AttributeIterator {
public: 
    unsigned int offset; 
    Attribute    attribute;
  
    AttributeIterator ( Schema& s, std::string attributeName ) 
      :  offset ( s.getOffsetInTuple ( attributeName ) ), 
         attribute ( s.getAttributeByName ( attributeName ) ) { };

    Attribute getAttribute () { 
        return attribute; 
    };

    SqlValue getVal ( Data* addr ) { 
        return ValueMoves::fromAddress ( attribute.type, addr + offset ); 
    }; 

    Data* getPtr ( Data* t ) { 
        return t + offset; 
    }; 

    std::string serialize ( Data* addr ) { 
        return serializeSqlValue ( 
            ValueMoves::fromAddress ( attribute.type, addr + offset ), 
            attribute.type 
        );
    };
    
    static std::vector<AttributeIterator> getAll ( Schema& s ) {
        std::vector <AttributeIterator> res;
        for ( auto a : s.getAttributes() ) {
            res.push_back ( AttributeIterator ( s, a.name ) );
        } 
        return res;
    };
};


/**
  * @brief Generate relation with uniform distribution in memory mapped file.
  * Fills the fields of out with corresponding sizes and pointers.
  * The generated file may be deleted to overwrite/free.
  */
Relation genDataOneColumn ( Schema s, size_t len, size_t magn=100 );


Relation genDataTwoColumns ( Schema s, size_t len, int v=100 );


Relation genDataNColumns ( Schema s, size_t len, std::vector<int> ranges );


/**
  * @brief Load relation from memory mapped fill into out.
  * Returns whether loading and size verification was successful.
  */
bool loadData ( Schema s, Relation* out, const char* filepath, size_t len );


std::string repeat ( int n, std::string s) {
    std::ostringstream os;
    for(int i = 0; i < n; i++)
        os << s;
    return os.str();
}


void printTableLine ( std::ostream& out,
                      std::string   l, 
                      std::string   m, 
                      std::string   x, 
                      std::string   r, 
                      std::vector   < int > cw ) {

    out << l + repeat ( cw [0], m );
    for ( size_t i = 1; i < cw.size(); i++) {
        out << x + repeat ( cw [i], m );
    }    
    out << r << std::endl;
}


void printTableRow ( std::ostream&                 out,
                     std::vector < std::string >&  strings,
                     std::vector < int >&          cw, 
                     std::string                   sep,
                     int                           startIdx = 0 ) {
        
    for ( size_t i = startIdx; i < startIdx + cw.size(); i++ ) {
        out << sep << std::setw ( cw [ i % cw.size() ] )  
                         << " " + strings [ i ] + " "; 
    }
    out << sep << std::endl; 
}


void printStringTable ( std::ostream&                out,
                        std::vector < std::string >  strings, 
                        int                          numColumns,
                        int                          numHeaderRows = 1,
                        std::string                  subTitle      = "",
                        int                          maxWidth      = 0,
                        bool                         openEnd       = false ) {
    
    std::vector < int > columnWidths ( numColumns, 0 );
    for ( unsigned int i = 0; i < strings.size(); i++ ) {
        int col = i % numColumns;
        std::string str = strings [ i ];
        if ( maxWidth != 0 ) {
            if ( str.length() > (size_t)maxWidth ) {
                str = str.substr ( 0, maxWidth ) + "..";
                strings [ i ] = str;
            }
        }
        columnWidths [ col ] = std::max ( (size_t)columnWidths [ col ], str.length()+2 ); 
    }

    printTableLine ( out,"┌", "─", "┬", "┐", columnWidths );

    size_t strIdx = 0;
    for ( int h = 0; h < numHeaderRows; h++ ) {
        printTableRow ( out, strings, columnWidths, "│", strIdx );
        strIdx += numColumns;
    }

    printTableLine ( out, "├", "─", "┼", "┤", columnWidths );
    
    while ( strIdx < strings.size() ) {
        printTableRow ( out, strings, columnWidths, "│", strIdx );
        strIdx += numColumns;
    }
    if ( openEnd ) {
        auto dots = std::vector < std::string > ( numColumns, "..." );
        printTableRow ( out, dots, columnWidths, "│" );
    }
    
    printTableLine ( out, "└", "─", "┴", "┘", columnWidths );

    if ( subTitle.length() > 0 ) { 
        int tableWidth = 0;
        for ( int i : columnWidths ) tableWidth += i;
        out << std::setw ( tableWidth + numColumns ) << subTitle << std::endl;   
    }
}



/**
  * @brief Print query result
  */
void printRelation ( std::ostream& out, Relation& rel, bool onlyTupleCount=false ) {

    const unsigned int lim = 18;
    const unsigned int maxWidth = 25;

    if ( onlyTupleCount ) {
        out << "Relation has " << rel.tupleNum() << " tuples" << std::endl; 
        return;
    }

    auto attrIts = AttributeIterator::getAll ( rel._schema );
    int nAtts = attrIts.size();
    if ( nAtts == 0) {
        out << "Relation with no attributes." << std::endl; 
        return;
    }
    std::vector < std::string > table;

    /* Attribute names */
    for ( int i=0; i < nAtts; i++) {
        table.push_back ( attrIts[i].attribute.name ); 
    }    
    
    /* Attribute types */
    for ( int i=0; i < nAtts; i++) {
        table.push_back ( serializeType ( attrIts[i].attribute.type ) ); 
    }    

    /* Attribute values */
    size_t n=0; 
    auto readIt = Relation::ReadIterator ( &rel );
    Data* tuple = readIt.get();
    while ( tuple != nullptr ) {
        for ( int i=0; i<nAtts; i++) {
            if ( attrIts[i].attribute.type.tag == SqlType::CHAR ) {   
                std::string str = ( attrIts[i].serialize(tuple) ); 
            }
            table.push_back ( attrIts[i].serialize(tuple) );
        }
        tuple = readIt.get();
        n++;
        if ( n >= lim ) break;
    }

    std::string subTitle = std::to_string ( rel.tupleNum() ) + " tuples";

    printStringTable ( out, table, nAtts, 2, subTitle, maxWidth, rel.tupleNum() > lim );
    
    out << "Relation has " << rel._dataBlocks.size() << " blocks." << std::endl;

}


/**
  * @brief Print query result
  */
void serializeRelation ( Relation& rel, std::ostream& os, std::string separator="|" ) {
    Relation::ReadIterator readIt = Relation::ReadIterator ( &rel );
    std::vector <AttributeIterator> atts = AttributeIterator::getAll ( rel._schema );
    Data* t = readIt.get();
    while ( t != nullptr ) {
        for ( auto& a : atts ) {
            SqlValue val = ValueMoves::fromAddress ( a.attribute.type, t + a.offset );
            os << serializeSqlValue ( val, a.attribute.type );
            os << separator;
        }
        os << std::endl;
        t = readIt.get();
    }
}


Relation genDataTypeMix ( size_t len, std::string prefix="" ) {

    Schema schema = Schema ( { 
        { prefix + "key",        TypeInit::BIGINT() }, 
        { prefix + "quantity",   TypeInit::BIGINT() }, 
        { prefix + "date",       TypeInit::DATE() },
        { prefix + "salesvalue", TypeInit::DECIMAL(6,1) },
        { prefix + "ratio",      TypeInit::DECIMAL(3,2) },
        { prefix + "isvalid",    TypeInit::BOOL() }
    } );
    
    Relation rel ( schema ); 
    std::vector<AttributeIterator> atts = AttributeIterator::getAll ( rel._schema );

    Relation::AppendIterator appendIt ( &rel );    
    for ( size_t i=0; i<len; i++) {
  
        Data* t = appendIt.get();
 
        int a = 0;

        // BIGINT
        int64_t* p1 = (int64_t*) atts[a++].getPtr ( t );
        *p1 = i++;
        
        // BIGINT
        int64_t* p12 = (int64_t*) atts[a++].getPtr ( t );
        *p12 = rand()%10+1;
        
        // DATE
        uint32_t* p2 = (uint32_t*) atts[a++].getPtr ( t );
        *p2 =     10000 * ( 1996 + (rand() % 24) ) +
                    100 * (    1 + (rand() % 12) ) +
                      1 * (    1 + (rand() % 31) );

        // DECIMAL(12,1)
        int64_t* p3 = (int64_t*) atts[a++].getPtr ( t );
        *p3 = rand() % 20000;

        // DECIMAL(12,2)
        int64_t* p4 = (int64_t*) (atts[a++].getPtr ( t ));
        *p4 = rand() % 101 - 50;
        
        // BOOL
        uint8_t* p5 = (uint8_t*) (atts[a++].getPtr ( t ));
        *p5 = rand() % 2;
    }
    return rel;
}
