/**
 * @file
 * Represent database schemas and their attributes.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include <set>
#include <vector>
#include "util/assertion.h"
#include "qlib/error.h"
#include "types.h"



typedef std::set < std::string > SymbolSet;


SymbolSet symbolSetUnion ( SymbolSet& a, SymbolSet& b ) {
    SymbolSet res;
    res.insert ( a.begin(), a.end() );
    res.insert ( b.begin(), b.end() );
    return res;
}


struct Attribute {

    std::string  name;
    SqlType      type;
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( name, type );
    }
};



/**
 * @brief Simple schema representation based on the attribute names and types. 
 */
class Schema {

public:
    /** 
     * @brief The attribute names of the schema.
     */
    std::vector < Attribute  > _attribs;
    
    /** 
     * @brief Number of attributes
     */
    size_t _nElems;
    
    /** 
     * @brief todo
     */
    size_t _stringsByVal;
    
    /** 
     * @brief Width of a tuple in bytes
     */
    size_t _tupSize;

    /** 
     * @brief Default constructor.
     */
    Schema () : _nElems ( 0 ), _tupSize ( 0 ) {};

    /** 
     * @brief Constructor from list of attribute names.
     */
    Schema ( std::vector < Attribute >  attributes,
             bool                       stringsByVal = true )
              
        : _attribs ( attributes ), 
          _nElems ( attributes.size() ), 
          _stringsByVal ( stringsByVal ) 
    {

        _tupSize = 0;
        for ( auto const& att : _attribs ) {
            _tupSize += getSizeInTuple ( att.type, _stringsByVal );
        }
    };

    /**
     * @brief Returns the offset of a specific attribute within a tuple.
     * This method is for use during plan creation (not during processing).
     */
    int getOffsetInTuple ( const std::string&  attributeName ) {

        size_t offset = 0;
        for ( auto const& att : _attribs ) {
            if ( att.name.compare ( attributeName ) == 0)
                break;
            offset += getSizeInTuple ( att.type, _stringsByVal );
        }
        if ( offset >= _tupSize )
            error_msg ( ELEMENT_NOT_FOUND, "The attribute " + attributeName + " was not"
                                           " found in the schema " + getSchemaString() );
        return offset;   
    }
    
    /**
     * @brief Returns the attribute names
     */
    std::vector < Attribute > getAttributes ( ) {
        return _attribs;
    }
    

    /**
     * @brief Check if symbol is there
     */
    bool contains ( std::string symbol ) {
        for ( auto a : _attribs ) {
            if ( symbol.compare ( a.name ) == 0 ) {
                return true;
            }
        }
        return false;
    }

    Schema prune ( SymbolSet required ) {
        std::vector < Attribute > res;
        for ( auto a : _attribs ) {
            if ( required.find ( a.name ) != required.end() ) {
                res.push_back ( a );
            }
        }
        return res;
    }
    

    /**
     * @brief Returns the attribute with given name
     */
    Attribute getAttributeByName ( std::string name ) {
        for ( auto const& att : _attribs ) {
            if ( att.name.compare ( name ) == 0)
                return att;
        }
        std::cerr << "Error: Attribute not found" << std::endl;
        return { "empty", emptySqlType };
    }
    
    /**
     * @brief Returns the attribute with given name
     */
    SqlType getTypeByName ( std::string name ) {
        return getAttributeByName ( name ).type;
    }

    /**
     * @brief Returns a new schema with the attributes concatenated from this and s2.
     */
    Schema join ( Schema& s2 ) {
        Schema res;
        res._attribs.insert ( res._attribs.end(), _attribs.begin(), _attribs.end() );
        res._attribs.insert ( res._attribs.end(), s2._attribs.begin(), s2._attribs.end() );
        res._nElems  = _nElems + s2._nElems;
        res._tupSize = _tupSize + s2._tupSize;
        res._stringsByVal = _stringsByVal;
        return res;
    }
    
    /**
     * @brief Prints a schema with its attribute names.
     */
    std::string getSchemaString () {
        std::string res = "["; 
        for ( auto const& a : _attribs ) {
            res = res + a.name + " ";
        }
        res = res +  "]";
        return res;
    }

    /**
     * @ brief Compares the types and names of two schema objects.
     */
    bool compare ( Schema& other, bool compareNames = true ) const {
        if ( _nElems != other._nElems ) {
            return false;
        }
        for ( size_t i = 0; i < _nElems; i++ ) {
            std::string type = serializeType ( _attribs[i].type );
            std::string otherType = serializeType ( other._attribs[i].type );
            if ( type.compare ( otherType ) != 0 ) {
                return false;
            }
            if ( compareNames ) {
                std::string name = _attribs[i].name;
                std::string otherName = other._attribs[i].name;
                if ( name.compare ( otherName ) != 0 ) {
                    return false;
                }
            }
        } 
        return true;
    }
    
    /**
     * @brief Prints a schema with its attribute names.
     */
    void print () {
        std::cout << "Schema object (" << this << ")" << " with attributes " << getSchemaString() << std::endl;
    }

    //Schema& operator=(const Schema&) = default;
    
    /* serialization */
    template < class Archive >
    void serialize ( Archive& ar ) {
        ar ( _attribs, _nElems, _stringsByVal, _tupSize );
    }
};
