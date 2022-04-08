/**
 * @file
 * Handle sets of SQL values, e.g. hash, compare, materialize.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "JitContextFlounder.h"
#include "ExpressionsJitFlounder.h"
#include "debug.h"


/* A named IR value with its type */
struct Value {
    ir_node*     node;
    SqlType      type;
    std::string  symbol;
};


typedef std::vector < Value > ValueSet;


namespace Values {

    
    Schema schema ( ValueSet&  vals,
                    bool       stringsByVal ) {

        std::vector < Attribute > atts;
        for  ( auto& v : vals ) {
            atts.push_back ( { v.symbol, v.type } );
        }
        return Schema ( atts, stringsByVal );
    }

    
    Schema schema ( ValueSet& vals, 
                    ValueSet& vals2,
                    bool      stringsByVal ) {
     
        Schema s = schema ( vals, stringsByVal );
        Schema s2 = schema ( vals2, stringsByVal );
        return s.join ( s2 );
    }

  
    size_t byteSize ( ValueSet&  vals,
                      bool       stringsByVal ) {

        Schema s = schema ( vals, stringsByVal );
        return s._tupSize;
    }
    

    void addSymbols ( JitContextFlounder& ctx, ValueSet& vals ) {
        for ( auto& v : vals ) {
            ctx.symbolTable [ v.symbol ] = v.node;
            ctx.rel.symbolTypes [ v.symbol ] = v.type;
        }
    }


    void hash ( Value&               val, 
                ir_node*             hashVreg, 
                JitContextFlounder&  ctx ) {

        switch ( val.type.tag ) {
            case SqlType::BIGINT:
            case SqlType::DECIMAL: {
                ir_node* hash = ctx.request ( vreg64 ( "hash" ) );
                ctx.yield ( mov ( hash, val.node ) );
                ctx.yield ( imul ( hash, constLoad ( constInt64 ( 1710227316115945415 ) ) ) );
                ctx.yield ( add ( hash, constLoad ( constInt64 ( 741332713408129251 ) ) ) );
                ctx.yield ( add ( hashVreg, hash ) );
                ctx.clear ( hash );
                break;
            }
            case SqlType::INT:
            case SqlType::DATE: {
                /* We use a mov with sign extension (movsxd)    *
                 * because extending the unsigned value to 64   *
                 * bits is done in x86 implicitly by switching  *
                 * to the full register from only the lower     *
                 * part (e.g. edx to rdx). This is currently    *
                 * not supported by flounder.                   * 
                 *                                              *
                 * movsxd (with 'd') is necessary because movsx *
                 * only works on 8/16 bit source registers.     *
                 * Using movsx here caused hard-to-track bugs   *
                 * because nasm adapts to movsxd automatically  *
                 * and asmjit doesn't.                          */
                ir_node* hash = ctx.request ( vreg64 ( "hash" ) );
                ctx.yield ( movsxd ( hash, val.node ) );
                ctx.yield ( add ( hash, constLoad ( constInt64 ( 741332713408129251 ) ) ) );
                ctx.yield ( imul ( hash, constLoad ( constInt64 ( 1710227316115945415 ) ) ) );
                ctx.yield ( add ( hashVreg, hash ) );
                ctx.clear ( hash );
                break;
            }
            case SqlType::BOOL: {
                ir_node* afterAdd = label ( "boolHash" );
                ctx.yield ( cmp ( val.node, constInt8 ( 0 ) ) );
                ctx.yield ( jne ( afterAdd ) );
                ctx.yield ( add ( hashVreg,  constLoad ( constInt64 ( 31636373 ) ) ) );
                ctx.yield ( placeLabel ( afterAdd ) );
                break;
            }
            case SqlType::CHAR: {
                ctx.comment ( "hash char" );
                size_t len = val.type.charSpec().num;
                if ( len > 1 ) {
                    ctx.yield ( mcall3 ( hashVreg, 
                                         (void*)&hashChar, 
                                         val.node, 
                                         hashVreg, 
                                         constInt64 ( len ) ) );
                }
                else {
                    ir_node* ext = ctx.request ( vreg64 ( "extend_char1" ) );
                    ctx.yield ( movzx ( ext, val.node ) );
                    ctx.yield ( add ( hashVreg, ext ) );
                    ctx.yield ( add ( hashVreg, hashVreg ) );
                    ctx.clear ( ext );
                }
                break;
            }
            case SqlType::VARCHAR: {
                ctx.comment ( "hash varchar" );
                size_t maxLen = val.type.varcharSpec().num;
                ctx.yield ( mcall3 ( hashVreg, 
                                     (void*)&hashVarchar, 
                                     val.node, 
                                     hashVreg, 
                                     constInt64 ( maxLen ) ) );
                break;
            }
            default:
                error_msg ( NOT_IMPLEMENTED, "Values::hash(..) not implemented for datatype" );
        }
    }
    

    void hash ( ValueSet&            vals,
                ir_node*             hashVreg,
                JitContextFlounder&  ctx ) {
  
        for ( auto& v : vals ) {
            hash ( v, hashVreg, ctx ); 
        }
    }


    ir_node* hash ( ValueSet&            vals,
                    JitContextFlounder&  ctx ) {
  
        ir_node* hashVreg = ctx.request ( vreg64 ( "hash" ) ); 
        ctx.yield ( mov ( hashVreg, constInt64 ( 0 ) ) );
        hash ( vals, hashVreg, ctx );
        return hashVreg;
    }
    


    
    ValueSet get ( Schema&              s,
                   JitContextFlounder&  ctx ) {
       
        ValueSet res; 
        for ( const Attribute& a : s._attribs ) {
            res.push_back ( {
                ctx.symbolTable [ a.name ], 
                ctx.rel.symbolTypes [ a.name ],
                a.name
            } );
        }
        return res;
    }


    struct MaterializeConfig {
        bool stringsByVal;
        bool explicit_;
    };
    
    
    /* First value: stringsByVal                    * 
     *    false:  strings in HT by reference, or    *
     *    true:   strings in HT by value.           *
     */
    MaterializeConfig htMatConfig = { false, true };

    /* First value _needs_ to be true to be able to read the result relation */
    MaterializeConfig relationMatConfig = { true, true };
   
 
    /* for memory to register reads, e.g. bigint */
    ir_node* offsetMemAt ( ir_node*  base, 
                           size_t    offset ) {
        ir_node* loc;
        if ( offset == 0 ) {
            loc = memAt ( base );
        }
        else {
            loc = memAtAdd ( base, constInt64 ( offset ) );
        }
        return loc;
    }

    
    /* for pointers e.g. varchar */
    ir_node* offsetVreg ( ir_node*             base, 
                          size_t               offset,
                          JitContextFlounder&  ctx ) {
    
        ir_node* loc = ctx.request ( vreg64 ( "loc" ) );
        ctx.yield ( mov ( loc, base ) );
        ctx.yield ( add ( loc, constInt64 ( offset ) ) );
        return loc;
    }
    

    /* for pointers e.g. varchar */
    void getOffset ( ir_node*             result,
                     ir_node*             base, 
                     size_t               offset,
                     JitContextFlounder&  ctx ) {
    
        ctx.yield ( mov ( result, base ) );
        ctx.yield ( add ( result, constInt64 ( offset ) ) );
    }

    
    ir_node* loadToReg ( SqlType              type, 
                         ir_node*             tupleAddress,
                         size_t               offset,
                         MaterializeConfig&   matConfig,
                         JitContextFlounder&  ctx ) {

        ir_node* res = ctx.vregForType ( type, matConfig.explicit_ ); 

        switch ( type.tag ) {
            
            case SqlType::INT:
                ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                break;

            case SqlType::BIGINT:
                ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                break;

            case SqlType::DECIMAL:
                ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                break;

            case SqlType::BOOL:
                ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                break;

            case SqlType::DATE:
                ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                break;
            
            case SqlType::CHAR: {
                if ( type.charSpec().num > 1 ) {
                    if ( matConfig.stringsByVal ) { 
                        /* assign address of attribute in data */
                        getOffset ( res, tupleAddress, offset, ctx );
                    }
                    else {
                        /* load address from data */
                        ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                    }
                }
                else {
                    ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                }
                break;
            }
            case SqlType::VARCHAR: {
                if ( matConfig.stringsByVal ) { 
                    /* assign address of attribute in data */
                    getOffset ( res, tupleAddress, offset, ctx );
                }
                else {
                    /* load address from data */
                    ctx.yield ( mov ( res, offsetMemAt ( tupleAddress, offset ) ) ); 
                }
                break;
            }

            default:
                error_msg ( NOT_IMPLEMENTED, "loadAttributeToReg(..) not implemented for datatype" );
        }
        return res;
    }


    void storeStringValueToMem ( ir_node*             attReg,
                                 ir_node*             tupleAddress,
                                 size_t               offset,
                                 size_t               maxNum,
                                 MaterializeConfig&   matConfig,
                                 JitContextFlounder&  ctx ) {
        
        if ( matConfig.stringsByVal ) {
            /* Materialize string characters */
            ir_node* loc = offsetVreg ( tupleAddress, offset, ctx );
            ctx.yield ( 
                mcall3 ( loc, 
                         (void*)&ValueMoves::writeString, 
                         attReg, 
                         loc, 
                         constInt64 ( maxNum ) )
            );
            ctx.clear ( loc );
        }
        else {
            /* Materialize string address */
            ctx.yield ( mov ( offsetMemAt ( tupleAddress, offset ), attReg ) ); 
        }
    }
                   

   
    void storeToMem ( SqlType              type, 
                      ir_node*             attReg,
                      ir_node*             tupleAddress,
                      size_t               offset,
                      MaterializeConfig&   matConfig,
                      JitContextFlounder&  ctx ) {
        
        switch ( type.tag ) {
            case SqlType::INT:
            case SqlType::BIGINT:
            case SqlType::DECIMAL:
            case SqlType::BOOL:
            case SqlType::DATE: {
                ctx.yield ( mov ( offsetMemAt ( tupleAddress, offset ), attReg ) ); 
                break;
            }
            case SqlType::CHAR: {
                if ( type.charSpec().num > 1 ) {
                    storeStringValueToMem ( attReg, 
                                            tupleAddress, 
                                            offset, 
                                            type.charSpec().num, 
                                            matConfig,
                                            ctx );
                }
                else {
                    ctx.yield ( mov ( offsetMemAt ( tupleAddress, offset ), attReg ) ); 
                }
                break;
            }
            case SqlType::VARCHAR: {
                storeStringValueToMem ( attReg, 
                                        tupleAddress, 
                                        offset, 
                                        type.varcharSpec().num, 
                                        matConfig,
                                        ctx );
                break;
            }
            default:
                error_msg ( NOT_IMPLEMENTED, "storeToMem(..) not implemented for datatype" );
        }
    }


    void materialize ( ValueSet&            vals, 
                       ir_node*             addr,
                       MaterializeConfig&   matConfig,
                       JitContextFlounder&  ctx ) {

        Schema schm = schema ( vals, matConfig.stringsByVal );
        for ( auto& val : vals )
        {
            size_t offset = schm.getOffsetInTuple ( val.symbol );
            storeToMem ( val.type, val.node, addr, offset, matConfig, ctx );
        }
    }

    
    ValueSet dematerialize ( ir_node*             addr, 
                             Schema&              schema, 
                             MaterializeConfig&   matConfig,
                             JitContextFlounder&  ctx,
                             SymbolSet            required=SymbolSet() ) {

        Schema s = Schema ( schema._attribs, matConfig.stringsByVal );

        ValueSet res;
        for ( const Attribute& a : s._attribs ) {
            if ( required.size() == 0 || ( required.find ( a.name ) != required.end() ) ) {
                size_t offset = s.getOffsetInTuple ( a.name );
                ir_node* valreg = loadToReg ( a.type, addr, offset, matConfig, ctx );
                res.push_back ( { valreg, a.type, a.name } );
            }
        }
        return res;
    }

    void clear ( ValueSet& set, JitContextFlounder& ctx ) {
        for ( auto& v : set ) {
            ctx.yield ( clear ( v.node ) );
        }
    }

    
    ValueSet dematerialize ( ir_node*             addr, 
                             ValueSet&            template_,
                             MaterializeConfig&   matConfig,
                             JitContextFlounder&  ctx ) {

        Schema s = schema ( template_, matConfig.stringsByVal );
        return dematerialize ( addr, s, matConfig, ctx );
    }

    
    void checkEqualityJump ( ValueSet&           a, 
                             ValueSet&           b, 
                             ir_node*            jumpLabelIfNot,
                             JitContextFlounder& ctx ) {

        for ( size_t i = 0; i < a.size(); i++ ) {
            ir_node* eqResult = emitEquals ( ctx, a[i].type, a[i].node, b[i].node ); 
            ctx.yield ( cmp ( eqResult, constInt8 ( 0 ) ) );
            ctx.yield ( je ( jumpLabelIfNot ) );
            ctx.clear ( eqResult );        
        }
    }
    

    void checkEqualityJumpIfTrue ( ValueSet&           a, 
                                   ValueSet&           b, 
                                   ir_node*            jumpLabelIf,
                                   JitContextFlounder& ctx ) {

        for ( size_t i = 0; i < a.size(); i++ ) {
            ir_node* eqResult = emitEquals ( ctx, a[i].type, a[i].node, b[i].node ); 
            ctx.yield ( cmp ( eqResult, constInt8 ( 1 ) ) );
            ctx.yield ( je ( jumpLabelIf ) );
            ctx.clear ( eqResult );        
        }
    }
   
 
    void checkEqualityBool ( ValueSet&           a, 
                             ValueSet&           b, 
                             ir_node*            flagVreg,
                             JitContextFlounder& ctx ) {

        ir_node* notEqual = idLabel ( "ValueSetsNotEqual" );
        ctx.yield ( mov ( flagVreg, constInt8 ( 0 ) ) );
        for ( size_t i = 0; i < a.size(); i++ ) {
            ir_node* eqResult = emitEquals ( ctx, a[i].type, a[i].node, b[i].node ); 
            ctx.yield ( cmp ( eqResult, constInt8 ( 0 ) ) );
            ctx.yield ( je ( notEqual ) );
            ctx.clear ( eqResult );        
        }
        ctx.yield ( mov ( flagVreg, constInt8 ( 1 ) ) );
        ctx.yield ( placeLabel ( notEqual ) ); 
    }


};


ValueSet evalExpressions ( std::vector < Expr* >  expressions, 
                           JitContextFlounder&    ctx ) {

    ValueSet res;
    for ( auto expr : expressions ){
        addExpressionIds ( expr, &ctx.rel );
        ir_node* node      = emitExpression ( ctx, expr ); 
        std::string name   = getExpressionName ( expr );
        res.push_back ( { node, expr->type, name } );
    }
    return res;
}

        
ValueSet evalExpressionList ( Expr*                expr, 
                              JitContextFlounder&  ctx ) {

    std::vector < Expr* > expressions;
    while ( expr != nullptr ) {
        expressions.push_back ( expr );
        expr = expr->next;
    }
    return evalExpressions ( expressions, ctx );
}
