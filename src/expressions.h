/**
 * @file
 * Representation of scalar SQL expressions.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <iostream>
#include <string>

#include "qlib/error.h"
#include "schema.h"
#include "values.h"
#include "RelationalContext.h"
#include "util/ResqlError.h"

/* forward declarations */
struct Expr;
typedef std::vector < Expr* > ExprVec;
void deriveExpressionTypes ( Expr* e, std::map <std::string, SqlType>& identTypes );


struct Expr {

    enum Tag {
        /* arithmetic */
        ADD,         
        SUB,
        MUL,
        DIV,
        /* bool */
        AND,
        OR,
        /* comparison */
        LT,
        LE,
        GT,
        GE,
        EQ,
        NEQ,
        LIKE,
        /* aggregate */
        SUM,
        COUNT,
        AVG,
        MIN,
        MAX,
        /* order by */
        ASC,
        DESC,
        /* case */
        CASE,
        WHENTHEN,
        /* other */
        ATTRIBUTE,
        TYPECAST,
        CONSTANT,
        AS,
        TYPE,
        TABLE,
        STAR,
        UNDEFINED
    };


    enum StructureTag {
       LITERAL,
       UNARY,
       BINARY,
       TERNARY,
       OTHER,
    };

    
    Tag          tag;
    StructureTag structureTag;
    std::string  symbol;

    // represent tree
    Expr*        next;
    Expr*        child;

    // Store sql type and value for the expression
    //  - the type is derived by the typesystem
    //  - the value not used by jit 
    SqlType      type;
    SqlValue     value;

    size_t       id;
};
    

const char* exprTagNames[] {
    /* arithmetic */
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    /* bool */
    "AND",
    "OR",
    /* comparison */
    "LT",
    "LE",
    "GT",
    "GE",
    "EQ",
    "NEQ",
    "LIKE",
    /* aggregate */
    "SUM",
    "COUNT",
    "AVG", 
    "MIN",
    "MAX",
    /* order by */
    "ASC",
    "DESC",
    /* case */
    "CASE",
    "WHENTHEN",
    /* other */
    "ATTRIBUTE",
    "TYPECAST",
    "CONSTANT",
    "AS",
    "TYPE",
    "TABLE",
    "STAR",
    "UNDEFINED"
};


bool exprEquals ( Expr* a, Expr* b ) {

    if ( a->tag != b->tag ) {
        return false;
    }
    if ( a->tag == Expr::ATTRIBUTE ) {
        if ( a->symbol.compare ( b->symbol ) != 0 ) {
            return false;
        }
    } 
    return true;
}


bool traceMatch ( Expr*     haystack, 
                  Expr*     needle ) {

    if ( haystack == nullptr ) {
        return ( needle == nullptr );
    }
    if ( needle == nullptr ) {
        return ( haystack == nullptr );
    }
    if ( ! exprEquals ( haystack, needle ) ) {
        return false;
    }

    bool childrenMatch = true;
    Expr* childHaystack = haystack->child;
    Expr* childNeedle   = needle->child;
    while ( childHaystack != nullptr && 
            childNeedle   != nullptr ) {

         childrenMatch &= traceMatch ( childHaystack, childNeedle );
         childHaystack = childHaystack->next;
         childNeedle   = childNeedle->next; 
    }
    /* both should be nullptr after the loop */
    childrenMatch &= ( childHaystack == childNeedle );
    return childrenMatch;
}


std::string serializeExpr ( Expr* e ) {
    std::stringstream ss;
    ss << "{" 
       << exprTagNames [e->tag] 
       << ","
       << serializeType ( e->type );
       //<< ","
       //<< e->symbol;

    switch ( e->tag ) { 
        case Expr::CONSTANT:
            ss << ","
               << serializeSqlValue ( e->value, e->type );
            break;
        default:
            // nothing to do
            break;
    }
  
    Expr* ch = e->child; 
    while ( ch != nullptr ) {
        ss << ","
           << serializeExpr ( ch );
        ch = ch->next;
    }  
    ss << "}"; 
    return ss.str();
}


std::string serializeExprList ( Expr* e ) {
    Expr* it = e;    
    std::stringstream ss;
    ss << serializeExpr ( it );
    it = it->next;
    while ( it != nullptr ) {
        ss << "," << serializeExpr ( it );
        it = it->next;
    }
    return ss.str();
}


std::string serializeExprVec ( std::vector < Expr* > vec ) {
    std::stringstream ss;
    for ( auto e : vec ) {
        ss << serializeExpr ( e ) << ",";
    }
    return ss.str();
}


void insertUnaryBetweenParentAndChild ( Expr* parent, Expr* child, Expr* insert ) {

    if ( parent->child == child ) {
        Expr* oldChild = parent->child;
        parent->child = insert;
        insert->child = oldChild;
        insert->next = oldChild->next;
        child->next = nullptr; // unary has only one child
    }
    else {
        Expr* prev = parent->child;
        while ( prev->next != child && prev->next != nullptr ) {
            prev = prev->next;
        }
        
        if ( prev->next != child ) {
            error_msg ( ELEMENT_NOT_FOUND, "Child in insertUnaryBetweenParentAndChild(..) "
                                           "not found." );
        }
        
        prev->next = insert;
        insert->next = child->next;
        child->next = nullptr; // unary has only one child
        insert->child = child;
        
    }
}


Expr* copyExpr ( Expr* expr ) {
    Expr* res = new Expr();
    memcpy ( res, expr, sizeof ( Expr ) );
    return res;
}



Expr* literalExpr ( Expr::Tag    tag, 
                    std::string  symbol ) {

    Expr* expr = new Expr ( { tag, 
                              Expr::LITERAL, 
                              symbol, 
                              nullptr, 
                              nullptr, 
                              emptySqlType, 
                              emptySqlValue, 
                              0 } ); 
    return expr;
}


Expr* unaryExpr ( Expr::Tag    tag, 
                  std::string  symbol,
                  Expr*        child ) {
    
    Expr* expr = new Expr ( { tag, 
                              Expr::UNARY,
                              symbol, 
                              nullptr, 
                              child, 
                              emptySqlType, 
                              emptySqlValue, 
                              0 } ); 
    return expr;
}


Expr* binaryExpr ( Expr::Tag    tag, 
                   std::string  symbol,
                   Expr*        left,
                   Expr*        right ) {

    left->next = right;
    Expr* expr = new Expr ( { tag,
                              Expr::BINARY, 
                              symbol, 
                              nullptr, 
                              left, 
                              emptySqlType, 
                              emptySqlValue, 
                              0 } ); 
    return expr;
}


Expr* otherExpr ( Expr::Tag    tag, 
                  std::string  symbol ) {

    Expr* expr = new Expr ( { tag,
                              Expr::OTHER,
                              symbol, 
                              nullptr, 
                              nullptr, 
                              emptySqlType, 
                              emptySqlValue, 
                              0 } ); 
    return expr;
}


Expr* ternaryExpr ( Expr::Tag    tag, 
                    std::string  symbol,
                    Expr*        left,
                    Expr*        middle,
                    Expr*        right ) {

    left->next = middle;
    middle->next = right;
    Expr* expr = new Expr ( { tag, 
                              Expr::TERNARY,
                              symbol, 
                              nullptr, 
                              left, 
                              emptySqlType, 
                              emptySqlValue, 
                              0 } ); 
    return expr;
}


void freeExprList ( Expr* e ) {
    if ( e == nullptr ) return;
    freeExprList ( e->child );
    freeExprList ( e->next );
    delete ( e );
}


void freeExpr ( Expr* e ) {
    if ( e == nullptr ) return;
    Expr* c = e->child;
    while ( c != nullptr ) {
        freeExpr ( c );
        c = c->next; 
    }
    delete ( e );
}


void parseBigintConstant ( Expr* e ) { 
    int32_t v = std::stoll ( e->symbol ); 
    e->value.bigintData = v;
    e->type = TypeInit::BIGINT ( );
}


void parseIntConstant ( Expr* e ) { 
    int32_t v = std::stoi ( e->symbol ); 
    e->value.intData = v;
    e->type = TypeInit::INT ( );
}


void parseBoolConstant ( Expr* e ) { 

    bool value = false;
    std::string buffer = e->symbol;
    if ( buffer.compare ( "true" ) == 0 ) {
        value = true;
    }
    else if ( buffer.compare ( "false" ) == 0 ) {
        value = false;
    }
    else {
        error_msg ( PARSE_ERROR, "Couldnt parse BOOL constant." );
    } 
    e->value.boolData = value;
    e->type = TypeInit::BOOL ( );
}


void parseCharConstant ( Expr* e ) { 
    e->value.charData = (char*) e->symbol.c_str();
    e->type = TypeInit::CHAR ( e->symbol.length() );
}


void parseVarcharConstant ( Expr* e ) { 
    e->value.varcharData = (char*) e->symbol.c_str();
    e->type = TypeInit::VARCHAR ( e->symbol.length() );
}


void parseDateConstant ( Expr* e ) { 

    bool success = false;
    std::string buffer = e->symbol;
    int year, day, month;

    if ( sscanf ( buffer.c_str(), "%4d-%2d-%2d", &year, &month, &day ) == 3 ) {
        success = true; 
    }
    if ( sscanf ( buffer.c_str(), "%4d/%2d/%2d", &year, &month, &day ) == 3 ) {
        success = true; 
    }

    if ( !success ) {
        sscanf ( buffer.c_str(), "%2d/%2d/%4d", &month, &day, &year );
    }

    if ( !success ) {
        error_msg ( PARSE_ERROR, 
            "Unsupported string type or unsupported date format "
            "(formats: \"yyyy/mm/dd\", \"mm/dd/yyyy\")" 
        );
    }

    unsigned int data = year * 10000 + month * 100 + day;
    e->value.dateData = data;
    e->type = TypeInit::DATE ( );
}


void parseDecimalConstant ( Expr* e ) {

    std::string sym = e->symbol;
    bool negative = false;
    unsigned int precision = 0;
    unsigned int scale = 0;

    if ( sym == "-" ) {
        negative = true;
        sym = sym.substr ( 1 );
    }

    size_t pos = sym.find ( "." );
    if ( pos != std::string::npos ) {
        scale = sym.length() - (pos+1);
        sym.replace ( pos, 1, "" );
    } 

    precision = sym.length ();
    long long int v = std::stoll ( sym ); 
    if ( negative ) v *= -1;

    e->value.decimalData = v;
    e->type = TypeInit::DECIMAL ( precision, scale );
}


void parseFloatConstant ( Expr* e ) {
    e->value.decimalData = std::stod ( e->symbol );
    e->type = TypeInit::FLOAT ();
}


void parseConstant ( Expr* e, SqlType::Tag typeCategory ) {

    switch ( typeCategory ) {

        case SqlType::DECIMAL:
            parseDecimalConstant ( e ); 
            break;
        
        case SqlType::FLOAT:
            parseFloatConstant ( e ); 
            break;

        case SqlType::DATE:
            parseDateConstant ( e ); 
            break;
        
       case SqlType::INT:
            parseIntConstant ( e ); 
            break;

        case SqlType::BIGINT:
            parseBigintConstant ( e ); 
            break;
        
        case SqlType::BOOL:
            parseBoolConstant ( e ); 
            break;

        case SqlType::CHAR:
            parseCharConstant ( e ); 
            break;
        
        case SqlType::VARCHAR:
            parseVarcharConstant ( e ); 
            break;

        default:
            error_msg ( NOT_IMPLEMENTED, "parseConstant(..) not implemented for type." );
    }
}


namespace ExprGen {

    Expr* constant ( std::string symbol, SqlType::Tag typeCategory ) {
        Expr* e = literalExpr ( Expr::CONSTANT, symbol );
        parseConstant ( e, typeCategory );
        return e;
    }
    
    Expr* add ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::ADD, "+", l, r );
        return e;
    }
    
    Expr* sub ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::SUB, "-", l, r );
        return e;
    }
        
    Expr* mul ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::MUL, "*", l, r );
        return e;
    }
    
    Expr* div ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::DIV, "/", l, r );
        return e;
    }
    
    Expr* and_ ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::AND, "and", l, r );
        return e;
    }
    
    Expr* or_ ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::OR, "or", l, r );
        return e;
    }
    
    Expr* lt ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::LT, "<", l, r );
        return e;
    }
    
    Expr* gt ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::GT, ">", l, r );
        return e;
    }
    
    Expr* le ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::LE, "<=", l, r );
        return e;
    }
    
    Expr* ge ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::GE, ">=", l, r );
        return e;
    }
    
    Expr* eq ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::EQ, ">=", l, r );
        return e;
    }
    
    Expr* neq ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::NEQ, ">=", l, r );
        return e;
    }
   
    Expr* like ( Expr* l, Expr* r ) {
        Expr* e = binaryExpr ( Expr::LIKE, "like", l, r );
        return e;
    }
    
    Expr* sum ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::SUM, "sum", child );
        return e;
    }
    
    Expr* count ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::COUNT, "count", child );
        return e;
    }
    
    Expr* avg ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::AVG, "avg", child );
        return e;
    }
    
    Expr* min ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::MIN, "min", child );
        return e;
    }
    
    Expr* max ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::MAX, "max", child );
        return e;
    }

    //Expr* asc (std::string&& symbol ) {
    //  return unaryExpr(Expr::ASC, std::move(symbol), nullptr);
    //}

    //Expr* desc ( std::string&& symbol ) {
    //  return unaryExpr(Expr::DESC, std::move(symbol), nullptr);
    //}

    Expr* asc ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::ASC, "asc", child );
        return e;
    }
    
    Expr* desc ( Expr* child ) {
        Expr* e = unaryExpr ( Expr::DESC, "desc", child );
        return e;
    }
    
    Expr* case_ ( Expr* whenThenElse ) {
        Expr* e = otherExpr ( Expr::CASE, "case" );
        e->child = whenThenElse;
        return e;
    }
    
    Expr* whenThen ( Expr* when, Expr* then ) {
        Expr* e = binaryExpr ( Expr::WHENTHEN, "whenThen", when, then );
        return e;
    }
    
    Expr* attr ( std::string symbol ) {
        Expr* e = literalExpr ( Expr::ATTRIBUTE, symbol );
        return e;
    }
    
    Expr* attr ( Attribute& a ) {
        Expr* e = literalExpr ( Expr::ATTRIBUTE, a.name );
	e->type = a.type;
        return e;
    }
    
    Expr* typecast ( SqlType type, Expr* child ) {
        Expr* e = unaryExpr ( Expr::TYPECAST, "typecast", child );
        e->type = type;
        return e;
    }
    
    Expr* as ( std::string symbol, Expr* child ) {
        Expr* e = unaryExpr ( Expr::AS, symbol, child );
        return e;
    }
    
    Expr* type ( SqlType type ) {
        Expr* e = literalExpr ( Expr::TYPE, "" );
        e->type = type;
        return e;
    }
    
    Expr* table ( std::string symbol ) {
        Expr* e = literalExpr ( Expr::TABLE, symbol );
        return e;
    }
    
    Expr* star ( ) {
        Expr* e = literalExpr ( Expr::STAR, "*" );
        return e;
    }
    
    Expr* undef ( std::string symbol ) {
        Expr* e = literalExpr ( Expr::UNDEFINED, symbol );
        return e;
    }
    
    Expr* copy ( Expr* expr ) {
        Expr* res = new Expr();
        res->tag = expr->tag;
        res->structureTag = expr->structureTag;
        res->symbol = expr->symbol;
        res->type = expr->type;
        res->value = expr->value;
        /* todo: fix */
        Expr* child = expr->child;
        while ( child != nullptr ) {
            res->child = copy ( child ); 
            child = child->next;
        }
        return res;
    }
    /* end added for parsing */
    
}



/*
Apply SQL Server type precedence:
    user defined type
    sql_variant
    xml
    datetimeoffset
    datetime2
    datetime
    smalldatetime
    date
    time
    float
    real
    decimal
    money
    smallmoney
    bigint
    int
    smallint
    tinyint
    bit
    ntext
    text
    image
    timestamp
    uniqueidentifier
    nvarchar
    nchar
    varchar
    char
    varbinary
    binary
*/
void insertTypecast ( Expr*    e, 
                      Expr*    child, 
                      SqlType  to ) {

    if ( to.tag == SqlType::CHAR ||
         to.tag == SqlType::VARCHAR ) {
        return;
    }

    if ( to.tag == SqlType::DECIMAL ) {
        /* todo: from float */
        to.decimalSpec().scale     =  0;
        to.decimalSpec().precision = 19;
    }

    insertUnaryBetweenParentAndChild ( e, child, ExprGen::typecast ( to, child ) );
}






void insertTypecastIfNeeded ( Expr* e,
                              Expr* child,
                              SqlType from,
                              SqlType to ) {

    if ( equalTypes ( from, to ) ) {
        return;
    }

    else {
        insertTypecast ( e, child, to );
    }

}


void applyPrecedence ( Expr* e, Expr* left, Expr* right ) {

    if ( left->type.tag != right->type.tag ) {

        // left child has higher precedence
        if ( left->type.tag > right->type.tag ) {
            insertTypecast ( e, right, left->type );
        }
        
        // right child has higher precedence
        else {
            insertTypecast ( e, left, right->type );
        }

    }
}


DecimalSpec getTypeOfDecimalArithmetic ( Expr::Tag op, DecimalSpec left, DecimalSpec right ) {

    DecimalSpec res = {0,0}; 

    switch ( op ) {
        case Expr::ADD:
        case Expr::SUB: {
            res.precision = std::max ( left.precision, right.precision ) + 1;
            res.scale     = left.scale; // scale on left and right is identical
            break;
        }
        case Expr::MUL:
            res.precision = left.precision + right.precision;
            res.scale     = left.scale     + right.scale;
            break;
        case Expr::DIV:
            error_msg ( NOT_IMPLEMENTED, "Decimal division not yet implemented" );
            break;
        default:
            error_msg ( NOT_IMPLEMENTED, "Invalid expression type or not implemented type "
                                         "in getTypeOfDecimalArithmetic(..)" );
    }

    if ( res.precision > 19 ) 
        res.precision = 19;

    return res;
}


void configureBinaryArithmeticResultType ( Expr* e ) {

    switch ( e->type.tag ) {
     
        case SqlType::DECIMAL: {
            e->type.decimalSpec() = getTypeOfDecimalArithmetic ( 
                                      e->tag, 
                                      e->child->type.decimalSpec(), 
                                      e->child->next->type.decimalSpec() );
            break;
        }

        default:
            // nothing to do
            break;
    }
}


void configureAggregationResultType ( Expr* e ) {

    switch ( e->child->type.tag ) {
     
        case SqlType::DECIMAL: {
 
            const DecimalSpec& childSpec = e->child->type.decimalSpec();

            if ( e->tag == Expr::SUM ) {
                e->type.decimalSpec().scale     = childSpec.scale;
                e->type.decimalSpec().precision = 19;
            }
            if ( e->tag == Expr::AVG ) {
                e->type.decimalSpec().scale     = childSpec.scale + 2;
                e->type.decimalSpec().precision = std::min ( childSpec.precision + 2, 19 );
            }
            break;
        }
        default:
            // nothing to do
            break;
    }
}


DecimalSpec scaleToOther ( DecimalSpec spec, DecimalSpec other ) {
    int difference = other.scale - spec.scale;
    spec.scale += difference;
    spec.precision += difference;
    if ( spec.precision > 19 ) {
        spec.precision = 19; 
    }
    return spec;
}


void typecastDecimalsToSameScale ( Expr* e, Expr* left, Expr* right ) {
    
    const DecimalSpec& leftSpec = left->type.decimalSpec(); 
    const DecimalSpec& rightSpec = right->type.decimalSpec(); 

    if ( leftSpec.scale < rightSpec.scale ) {
        SqlType type = TypeInit::DECIMAL ( scaleToOther ( leftSpec, rightSpec ) );
        if ( left->tag == Expr::TYPECAST ) {
            left->type = type;
        }
        else {
            insertUnaryBetweenParentAndChild ( e, left, ExprGen::typecast ( type, left ) );
        }
    }
    else if ( leftSpec.scale > rightSpec.scale ) {
        SqlType type = TypeInit::DECIMAL ( scaleToOther ( rightSpec, leftSpec ) );
        if ( right->tag == Expr::TYPECAST ) {
            right->type = type;
        }
        else {
            insertUnaryBetweenParentAndChild ( e, right, ExprGen::typecast ( type, right ) );
        }
    }
}


void typecastDecimalInputs ( Expr* e, Expr* left, Expr* right ) {

    switch ( e->tag ) {
        case Expr::LT:
        case Expr::GT:
        case Expr::LE:
        case Expr::GE:
        case Expr::EQ:
        case Expr::NEQ:
        case Expr::ADD:
        case Expr::SUB: {
            // For ADD, SUB, LT, GT, LE, GE, EQ, NEQ decimals need same scale
            return typecastDecimalsToSameScale ( e, left, right );
        }
        case Expr::MUL:
            // No restriction for MUL
            break;
        case Expr::DIV: {
            error_msg ( NOT_IMPLEMENTED, "Decimal division not yet implemented" );
            break;
        }
        default:
            error_msg ( NOT_IMPLEMENTED, "Invalid expression type or type not implemented "
                                         "in typecastDecimalInputs(..)" );
    }
}


void typecastConfigurableInputTypes ( Expr* e ) {

    Expr* left = e->child;
    Expr* right = e->child->next;

    switch ( left->type.tag ) {
        case SqlType::DECIMAL: {
            typecastDecimalInputs ( e, left, right );
        }
        default:
            // nothing to do
            break;
    }
}
       
 
std::string getExpressionName ( Expr* expr ) {
    std::string name;
    if ( expr->tag == Expr::ATTRIBUTE ) {
        name = expr->symbol;
    }
    else if ( expr->tag == Expr::AS ) {
        name = expr->symbol;
    } 
    else {
        name = std::string ( "expr" ) + std::to_string ( expr->id );
    }
    return name;
}


/* expression utilities begin */
std::vector < Expr* > exprListToVector ( Expr* expr ) {
    std::vector < Expr* > res;
    while ( expr != nullptr ) {
        res.push_back ( expr );
        expr = expr->next;
    }
    return res;
}


bool isAggregationExpr ( Expr* expr ) {
    switch ( expr->tag ) {
        case Expr::SUM:
        case Expr::MIN:
        case Expr::MAX:
        case Expr::AVG:
        case Expr::COUNT: {
            return true;
        }
        default:
            return false;
    }
}


std::vector < Expr* > equalitiesLeftSide ( std::vector < Expr* > eqs ) {

    std::vector < Expr* > res;
    for ( auto expr : eqs ) {
        if ( expr->tag != Expr::EQ ) {
            error_msg ( WRONG_TAG, "The elements of the expression list passed to "
                                   "equalitiesLeftSide(..) need the tag Expr::EQ" );
        }
        res.push_back ( expr->child ); 
    }
    return res;
}


std::vector < Expr* > equalitiesRightSide ( std::vector < Expr* > eqs ) {

    std::vector < Expr* > res;
    for ( auto expr : eqs ) {
        if ( expr->tag != Expr::EQ ) {
            error_msg ( WRONG_TAG, "The elements of the expression list passed to "
                                   "equalitiesRightSide(..) need the tag Expr::EQ" ); 
        }
        res.push_back ( expr->child->next ); 
    }
    return res;
}
/* expression utilities end */


void requireBoolType ( std::string name,
                       Expr*       expr ) {
    
    if ( expr->type.tag != SqlType::BOOL ) { 
        throw ResqlError ( 
            "Incompatible types: " + name +
            " expression requires bool operand at " +
            serializeExpr ( expr ) 
        );
    }

}

void requireBoolType ( Expr::Tag  tag,
                       Expr*      expr ) {

    requireBoolType ( exprTagNames [ tag ], expr ); 
}


void requireStringType ( Expr::Tag  tag,
                         Expr*      expr ) {

    if ( expr->type.tag != SqlType::CHAR &&
         expr->type.tag != SqlType::VARCHAR ) { 

        throw ResqlError ( 
            "Incompatible types: " +
            std::string ( exprTagNames [ tag ] ) + 
            " expression requires a char or varchar operand at " + 
            serializeExpr ( expr ) 
        );
    }
}


void requireNumericType ( Expr::Tag  tag,
                          Expr*      expr ) {


    if ( expr->type.tag != SqlType::DECIMAL &&
         expr->type.tag != SqlType::BIGINT  &&
         expr->type.tag != SqlType::INT     &&
         expr->type.tag != SqlType::FLOAT ) { 
        
        throw ResqlError ( 
            "Incompatible types: " +
            std::string ( exprTagNames [ tag ] ) + 
            " expression requires a numeric operand at " + 
            serializeExpr ( expr ) 
        );
    }
}


void requireOrderedType ( Expr::Tag  tag,
                          Expr*      expr ) {
    
    if ( expr->type.tag != SqlType::DECIMAL &&
         expr->type.tag != SqlType::BIGINT  &&
         expr->type.tag != SqlType::INT     &&
         expr->type.tag != SqlType::FLOAT   &&
         expr->type.tag != SqlType::DATE ) { 
        
        throw ResqlError ( 
            "Incompatible types: " +
            std::string ( exprTagNames [ tag ] ) + 
            " expression requires an ordered operand type at " + 
            serializeExpr ( expr ) 
        );
    }
}


SqlType commonSuperType ( SqlType& a, SqlType& b ) {

    if ( a.tag == b.tag ) {
        if ( a.tag == SqlType::DECIMAL ) {
            return TypeInit::DECIMAL (
                std::max ( a.decimalSpec().precision, b.decimalSpec().precision ),
                std::max ( a.decimalSpec().scale,     b.decimalSpec().scale )
            );
        }
        else if ( a.tag == SqlType::VARCHAR ) {
            return TypeInit::VARCHAR (
                std::max ( a.varcharSpec().num, b.varcharSpec().num )
            );
        }
        else if ( a.tag == SqlType::CHAR ) {
            return TypeInit::CHAR (
                std::max ( a.charSpec().num, b.charSpec().num )
            );
        }
        else {
            return a;
        }
    }
    else if ( a.tag == SqlType::BIGINT || a.tag == SqlType::INT ) {
        if ( b.tag == SqlType::DECIMAL ) {
            return b;
        }
        else {
           throw ResqlError ( 
               "Incompatible or unimplemented type combination in getCommonSuperType(..):" +
               serializeType ( a ) + " and " + serializeType ( b ) );
        }
    }
    else if ( b.tag == SqlType::BIGINT || b.tag == SqlType::INT ) {
        if ( a.tag == SqlType::DECIMAL ) {
            return b;
        }
        else {
           throw ResqlError ( 
               "Incompatible or unimplemented type combination in getCommonSuperType(..):" +
               serializeType ( a ) + " and " + serializeType ( b ) );
        }
    }
    throw ResqlError ( 
        "Incompatible or unimplemented type combination in getCommonSuperType(..):" +
        serializeType ( a ) + " and " + serializeType ( b ) );
}


void deriveCaseExpressionTypes ( Expr*                             e, 
                                 std::map <std::string, SqlType>&  identTypes ) {

    /* first pass over children to find mutual *
     * type for all 'then'-expressions and the *
     * 'else'-expression.                      */
    Expr* child = e->child;
    deriveExpressionTypes ( child, identTypes );
    SqlType thenType = child->type;
    child = child->next;
    while ( child != nullptr &&
            child->tag == Expr::WHENTHEN ) {
        Expr* when = child->child;
        Expr* then = when->next;
        deriveExpressionTypes ( when, identTypes );
        deriveExpressionTypes ( then, identTypes );
        thenType = commonSuperType ( thenType, then->type );
        child = child->next;
    }
    if ( child != nullptr ) {
        /* we have an else part */
        deriveExpressionTypes ( child, identTypes );
        thenType = commonSuperType ( thenType, child->type );
    }
    
    /* second pass to typecast to mutual type */
    child = e->child;
    while ( child != nullptr &&
            child->tag == Expr::WHENTHEN ) {
        Expr* when = child->child;
        Expr* then = when->next;
        insertTypecastIfNeeded ( child, then, then->type, thenType );
        deriveExpressionTypes ( child, identTypes );
        child = child->next;
    }
    if ( child != nullptr ) {
        /* handle else part */
        insertTypecastIfNeeded ( e, child, child->type, thenType );
    }
    e->type = thenType;
}
    

void deriveExpressionTypesOther ( Expr*                             e, 
                                  std::map <std::string, SqlType>&  identTypes ) {
    switch ( e->tag ) {

        case Expr::CASE:
            deriveCaseExpressionTypes ( e, identTypes );
            break;

        default:
            throw ResqlError ( "deriveExpressionTypesOther(..) not implemented for " + serializeExpr ( e ) );    
    }
}


void deriveExpressionTypesLiteral ( Expr*                             e, 
                                    std::map <std::string, SqlType>&  identTypes ) {

    /* some methods of operators create new expressions *
     * these need to call deriveExpressionTypes(..) but *
     * dont have the identTypes types available.        *
     *                                                  *
     * We add this to allow calling safely.             */

    if ( e->type.tag != SqlType::NT ) {
	if ( e->tag == Expr::ATTRIBUTE ) {
	    identTypes[e->symbol] = e->type;
	}
	return;
    }

    switch ( e->tag ) {

        case Expr::ATTRIBUTE:
            if ( identTypes.count ( e->symbol ) == 0 ) {
                throw ResqlError ( "Attribute " + e->symbol + " not found." ); 
            }
            e->type = identTypes [ e->symbol ]; 
            break;
        
        case Expr::CONSTANT:
            /* nothing to do here, type already defined by parser */
            break;

        case Expr::STAR:
            /* for count(*) and select * also gets BIGINT but is never used */
            e->type = TypeInit::BIGINT();
            break; 

        default:
            throw ResqlError ( "deriveExpressionTypesLiteral(..) not implemented for " + serializeExpr ( e ) );    
    }
}


void deriveExpressionTypesUnary ( Expr*                             e, 
                                  std::map <std::string, SqlType>&  identTypes ) {
    Expr* child = e->child;
    deriveExpressionTypes ( child, identTypes );

    switch ( e->tag ) {

        case Expr::TYPECAST:
            break;

        case Expr::AS:
            /* register identifier */
            identTypes[e->symbol] = child->type;
            e->type = child->type;
            break;

        case Expr::COUNT:
            e->type = TypeInit::BIGINT();
            break; 

        case Expr::SUM:
            requireNumericType ( e->tag, child );
            e->type = child->type;
            configureAggregationResultType ( e );
            break;

        case Expr::AVG:
            requireNumericType ( e->tag, child );
            e->type = TypeInit::DECIMAL ( 19, 2 );
            configureAggregationResultType ( e );
            break;

        case Expr::MAX:
        case Expr::MIN:
            requireOrderedType ( e->tag, child );
            e->type = child->type;
            break;

        case Expr::DESC:
        case Expr::ASC:
            e->type = child->type;
            break;
        
        default:
            throw ResqlError ( "deriveExpressionTypesUnary(..) not implemented for " + serializeExpr ( e ) );    
    }
}


void deriveExpressionTypesBinary ( Expr*                             e, 
                                   std::map <std::string, SqlType>&  identTypes ) {
            
    Expr* left = e->child;
    Expr* right = e->child->next;
    deriveExpressionTypes ( left, identTypes );
    deriveExpressionTypes ( right, identTypes );

    switch ( e->tag ) {
        case Expr::ADD:
        case Expr::SUB:
        case Expr::MUL:
        case Expr::DIV:
            requireNumericType ( e->tag, left );
            requireNumericType ( e->tag, right );
            /* Apply precedence rules such that both children have the same type category */
            applyPrecedence ( e, left, right );
            e->type = e->child->type;
            /* Typecast type parameters, e.g. same scale for decimal add. */
            typecastConfigurableInputTypes ( e );
            configureBinaryArithmeticResultType ( e );
            break;
        
        case Expr::LT:
        case Expr::LE:
        case Expr::GT:
        case Expr::GE:
            requireOrderedType ( e->tag, left );
            requireOrderedType ( e->tag, right );
        case Expr::EQ:
        case Expr::NEQ:
            applyPrecedence ( e, left, right );
            /* Typecast type parameters, e.g. same scale for comparisons. */
            typecastConfigurableInputTypes ( e );
            e->type = TypeInit::BOOL ( );
            break;

        case Expr::OR:
        case Expr::AND:
            requireBoolType ( e->tag, left );
            requireBoolType ( e->tag, right );
            e->type = TypeInit::BOOL ( );
            break;

        case Expr::LIKE:
            requireStringType ( e->tag, left );
            requireStringType ( e->tag, right );
            e->type = TypeInit::BOOL ( );
            break;
        
        case Expr::WHENTHEN:
            requireBoolType ( e->tag, left );
            e->type = right->type;
            break;

        default:
            throw ResqlError ( "deriveExpressionTypesBinary(..) not implemented for " + serializeExpr ( e ) );    
    }
}


void addExpressionIds ( Expr* e, RelationalContext* rctx ) {
    if ( e->id == 0 ) {
        e->id = ++(rctx->exprIdGen);
    }
}


/*
 * @brief Derive the type of expressions recursively.
 *
 * Parameter ctx holds the types of symbols. It can be left empty
 * when expressions contain no attributes.
 */
void deriveExpressionTypes ( Expr* e, std::map <std::string, SqlType>& identTypes ) {


    switch ( e->structureTag ) {
  
        case Expr::LITERAL:
            deriveExpressionTypesLiteral ( e, identTypes );
            break;

        case Expr::UNARY:
            deriveExpressionTypesUnary ( e, identTypes );
            break;
        
        case Expr::BINARY:
            deriveExpressionTypesBinary ( e, identTypes );
            break;
        
        case Expr::OTHER:
            deriveExpressionTypesOther ( e, identTypes );
            break;

        default:
            error_msg ( NOT_IMPLEMENTED, "deriveExpressionTypes(..)" );   
    }

}


void deriveExpressionTypes ( Expr*  e ) {
    std::map <std::string, SqlType> emptyIdentifiers;
    deriveExpressionTypes ( e, emptyIdentifiers );
}


void deriveExpressionTypes ( ExprVec&                          expressions, 
                             std::map <std::string, SqlType>&  identTypes ) {

    for ( auto expr : expressions ){
        deriveExpressionTypes ( expr, identTypes );
    }
}


void deriveExpressionTypes ( ExprVec&  expressions ) {
    std::map <std::string, SqlType> emptyIdentifiers;
    deriveExpressionTypes ( expressions, emptyIdentifiers );
}


SymbolSet extractRequiredAttributes ( Expr* expr ) {

    if ( expr == nullptr ) return {};

    SymbolSet res;
    if ( expr->tag == Expr::ATTRIBUTE ) {
        res.insert ( { getExpressionName ( expr ) } );
    }
    Expr* child = expr->child;
    while ( child != nullptr ) {
        SymbolSet chRes = extractRequiredAttributes ( child );
        res.insert ( chRes.begin(), chRes.end() );
        child = child->next;
    }
    return res; 
}


SymbolSet extractRequiredAttributes ( ExprVec&  exprVec ) {
 
    SymbolSet res;
    for ( auto& e : exprVec ) {
        SymbolSet eRes = extractRequiredAttributes ( e );
        res.insert ( eRes.begin(), eRes.end() );
    }
    return res;
}



