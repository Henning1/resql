/**
 * @file
 * Translate scalar SQL expressions to Flounder IR.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include "expressions.h"
#include "qlib/qlib.h"
#include "JitContextFlounder.h"


//#define ARITHMETICS_VIA_CALLS
//#define DEBUG_EXPRESSIONS


// forward declarations
ir_node* emitExpression ( JitContextFlounder& ctx, Expr* expr );


struct ExpressionContext {

    /* all expressions from the query plan in execution order */
    std::vector<Expr*> _expressions;

    /* required table attributes */
    SymbolSet _required;

    
    void define ( Expr* e ) {
        _expressions.push_back ( e );
        SymbolSet req = extractRequiredAttributes ( e ); 
        _required = symbolSetUnion ( _required, req );
    }
    
    void define ( std::vector < Expr* > es ) {
        for ( auto& e : es ) { 
            define ( e );
	}
    }

    bool isRequired ( std::string ident ) {
        return _required.find ( ident ) != _required.end();
    }

    void deriveExpressionTypes( std::map <std::string, SqlType>&  identTypes ) {
        for ( auto& e : _expressions ) { 
            ::deriveExpressionTypes ( e, identTypes ); 
	}
    }
};



/***  Constant  ***/

ir_node* emitConstantDECIMAL ( JitContextFlounder&  ctx, 
                               SqlValue             val ) {

    ir_node* res = ctx.request ( vreg64 ( "decimal_constant" ) );
    ctx.yield ( mov ( res, constInt64 ( val.decimalData ) ) );
    return res;
}


ir_node* emitConstantFLOAT ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg64 ( "decimal_constant" ) );
    ctx.yield ( mov ( res, constDouble ( val.floatData ) ) );
    return res;
}


ir_node* emitConstantBIGINT ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg64 ( "bigint_constant" ) ); 
    ctx.yield ( mov ( res, constInt64 ( val.bigintData ) ) );
    return res;
}


ir_node* emitConstantINT ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg32 ( "int_constant" ) ); 
    ctx.yield ( mov ( res, constInt32 ( val.intData ) ) );
    return res;
}


ir_node* emitConstantDATE ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg32 ( "date_constant" ) );
    ctx.yield ( mov ( res, constInt32 ( val.dateData ) ) );
    return res;
}


ir_node* emitConstantBOOL ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg8 ( "bool_constant" ) );
    ctx.yield ( mov ( res, constInt8 ( val.boolData ) ) );
    return res;
}


ir_node* emitConstantCHAR ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg64 ( "char_constant" ) );
    ctx.yield ( mov ( res, constAddress ( val.charData ) ) );
    return res;
}


ir_node* emitConstantCHAR1 ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg8 ( "char1_constant" ) );
    ctx.yield ( mov ( res, memAt ( constLoad ( constAddress ( val.charData ) ) ) ) );
    return res;
}


ir_node* emitConstantVARCHAR ( JitContextFlounder& ctx, SqlValue val ) {
    ir_node* res = ctx.request ( vreg64 ( "varchar_constant" ) );
    ctx.yield ( mov ( res, constAddress ( val.varcharData ) ) );
    return res;
}


ir_node* emitConstant ( JitContextFlounder& ctx, Expr* expr ) {
    ir_node* res = nullptr;
    switch ( expr->type.tag ) {
        case SqlType::DECIMAL:
            res = emitConstantDECIMAL ( ctx, expr->value );
            break;
        case SqlType::FLOAT:
            res = emitConstantFLOAT ( ctx, expr->value );
            break;
        case SqlType::DATE:
            res = emitConstantDATE ( ctx, expr->value );
            break;
        case SqlType::BOOL:
            res = emitConstantBOOL ( ctx, expr->value );
            break;
        case SqlType::BIGINT:
            res = emitConstantBIGINT ( ctx, expr->value );
            break;
        case SqlType::INT:
            res = emitConstantINT ( ctx, expr->value );
            break;
        case SqlType::CHAR:
            if ( expr->type.charSpec().num > 1 ) {
                res = emitConstantCHAR ( ctx, expr->value );
            }
            else {
                res = emitConstantCHAR1 ( ctx, expr->value );
            }
            break;
        case SqlType::VARCHAR:
            res = emitConstantVARCHAR ( ctx, expr->value );
            break;
        default:
            throw ResqlError ( "Constant code generation not implemented for datatype" );
    }
    return res;
}



/***  Add  ***/

ir_node* emitAddDECIMALxBIGINT ( JitContextFlounder&  ctx, 
                                 ir_node*             left, 
                                 ir_node*             right ) {

    ir_node* res = ctx.request ( vreg64 ( "add_result" ) );

    #ifdef ARITHMETICS_VIA_CALLS
        ctx.yield ( mcall2 ( res, (void*)&add_DECIMAL, left, right ) );
    #else
        ctx.yield ( mov ( res, left ) );
        ctx.yield ( add ( res, right ) );
    #endif

    return res;
}


ir_node* emitAdd ( JitContextFlounder&  ctx, 
                     SqlType              type, 
                     ir_node*             left, 
                     ir_node*             right ) {

    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::BIGINT:
            res = emitAddDECIMALxBIGINT ( ctx, left, right );
            break;
        default:
            throw ResqlError ( "ADD code generation not implemented for datatype" );
    }
    return res; 
}



/***  Sub  ***/

ir_node* emitSubDECIMALxBIGINT ( JitContextFlounder&  ctx, 
                                 ir_node*             left, 
                                 ir_node*             right ) {

    ir_node* res = ctx.request ( vreg64 ( "sub_result" ) );

    #ifdef ARITHMETICS_VIA_CALLS
        ctx.yield ( mcall2 ( res, (void*)&sub_DECIMAL, left, right ) );
    #else
        ctx.yield ( mov ( res, left ) );
        ctx.yield ( sub ( res, right ) );
    #endif 
    return res;
}


ir_node* emitSub ( JitContextFlounder&  ctx, 
                   SqlType              type, 
                   ir_node*             left, 
                   ir_node*             right ) {

    ir_node* res = nullptr;
    switch (type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::BIGINT:
            res = emitSubDECIMALxBIGINT ( ctx, left, right );
            break;
        default:
            throw ResqlError ( "SUB code generation not implemented for datatype" );
    }
    return res;
}



/***  Mul  ***/

ir_node* emitMulDECIMALxBIGINT ( JitContextFlounder& ctx, ir_node* left, ir_node* right ) {
    ir_node* res = ctx.request ( vreg64 ( "mul_result" ) );
 
    #ifdef ARITHMETICS_VIA_CALLS 
        ctx.yield ( mcall2 ( res, (void*)&mul_DECIMAL, left, right ) );
    #else
        ctx.yield ( mov ( res, left ) );
        ctx.yield ( imul ( res, right ) );
    #endif
  
    return res;
}


ir_node* emitMul ( JitContextFlounder&  ctx, 
                   SqlType              type, 
                   ir_node*             left, 
                   ir_node*             right ) {

    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::BIGINT:
            res = emitMulDECIMALxBIGINT ( ctx, left, right );
            break;
        default:
            throw ResqlError ( "MUL code generation not implemented for datatype" );
    }
    return res;
}



/***  Div  ***/

ir_node* emitDivBIGINT ( JitContextFlounder&  ctx, 
                         ir_node*             left, 
                         ir_node*             right ) {

    // todo: handle div by zero
    ir_node* res = ctx.request ( vreg64 ( "div_result" ) );
    ctx.yield ( mov ( reg64 ( RAX ), left ) );
    ctx.yield ( mov ( reg64 ( RCX ), right ) );
    ctx.yield ( cqo ( ) );
    ctx.yield ( idiv ( reg64 ( RCX ) ) );
    ctx.yield ( mov ( res, reg64 ( RAX ) ) );
    return res;
}


ir_node* emitDiv ( JitContextFlounder&  ctx, 
                     SqlType              type,
                     ir_node*             left,
                     ir_node*             right ) {

    ir_node* res = nullptr; 
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::BIGINT: {
            res = emitDivBIGINT ( ctx, left, right );
            break;
        }
        default:
            throw ResqlError ( "DIV code generation not implemented for datatype" );
    }
    return res;

}



/*** And ***/

ir_node* emitAnd ( JitContextFlounder&  ctx, 
                   ir_node*             left,
                   ir_node*             right ) {

    ir_node* res = ctx.request ( vreg8 ( "and_result" ) );
    ctx.yield ( mov ( res, left ) );
    ctx.yield ( and_ ( res, right ) );
    return res;
}



/*** Or ***/

ir_node* emitOr ( JitContextFlounder&  ctx, 
                  ir_node*             left,
                  ir_node*             right ) {

    ir_node* res = ctx.request ( vreg8 ( "or_result" ) );
    ctx.yield ( mov ( res, left ) );
    ctx.yield ( or_ ( res, right ) );
    return res;
}



/*** Less Than ***/

ir_node* emitLessThanDECIMALxDATExBIGINT ( JitContextFlounder& ctx, 
                                           ir_node* left,
                                           ir_node* right ) {
    ir_node* res = ctx.request ( vreg8 ( "lt_result" ) );
    ir_node* lbl = idLabel ( "lt_false" );
    ctx.yield ( mov ( res, constInt8 ( 0 ) ) );
    ctx.yield ( cmp ( left, right ) ); 
    ctx.yield ( jge ( lbl ) ); 
    ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
    ctx.yield ( placeLabel ( lbl ) ); 
    return res;
}


ir_node* emitLessThan ( JitContextFlounder& ctx,
                          SqlType             type,
                          ir_node*            left,
                          ir_node*            right ) {

    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::DATE:
        case SqlType::BIGINT: {
            res = emitLessThanDECIMALxDATExBIGINT ( ctx, left, right );
            break;
        }
        default:
            throw ResqlError ( "LESS_THAN code generation not implemented for datatype" );
    }
    return res;
}



/*** Less Than or Equal ***/

ir_node* emitLessThanOrEqualDECIMALxDATExBIGINT ( JitContextFlounder& ctx, 
                                                ir_node* left, 
                                                ir_node* right ) {
    ir_node* res = ctx.request ( vreg8 ( "lt_result" ) );
    ir_node* lbl = idLabel ( "lt_false" );
    ctx.yield ( mov ( res, constInt8 ( 0 ) ) );
    ctx.yield ( cmp ( left, right ) ); 
    ctx.yield ( jg ( lbl ) ); 
    ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
    ctx.yield ( placeLabel ( lbl ) ); 
    return res;
}


ir_node* emitLessThanOrEqual ( JitContextFlounder& ctx,
                               SqlType             type,
                               ir_node*            left,
                               ir_node*            right ) {

    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::DATE:
        case SqlType::BIGINT:
            res = emitLessThanOrEqualDECIMALxDATExBIGINT ( ctx, left, right );
            break;

        default:
            std::cout << "datatype: " << serializeType ( type ) << std::endl;
            throw ResqlError ( "LE code generation not implemented for datatype" );
    }
    return res;
}



/*** Greater Than ***/

ir_node* emitGreaterThanDECIMALxDATExBIGINT ( JitContextFlounder& ctx, 
                                              ir_node* left,
                                              ir_node* right ) {
    ir_node* res = ctx.request ( vreg8 ( "gt_result" ) );
    ir_node* lbl = idLabel ( "gt_false" );
    ctx.yield ( mov ( res, constInt8 ( 0 ) ) );
    ctx.yield ( cmp ( left, right ) ); 
    ctx.yield ( jle ( lbl ) ); 
    ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
    ctx.yield ( placeLabel ( lbl ) ); 
    return res;
}


ir_node* emitGreaterThan ( JitContextFlounder& ctx,
                             SqlType             type,
                             ir_node*            left,
                             ir_node*            right ) {
    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::DATE:
        case SqlType::BIGINT: {
            res = emitGreaterThanDECIMALxDATExBIGINT ( ctx, left, right );
            break;
        }
        default:
            throw ResqlError ( "ADD code generation not implemented for datatype" );
    }
    return res;
}



/*** Greater Than or Equal ***/

ir_node* emitGreaterThanOrEqualDECIMALxDATExBIGINT ( JitContextFlounder& ctx, 
                                                     ir_node* left, 
                                                     ir_node* right ) {
    ir_node* res = ctx.request ( vreg8 ( "gt_result" ) );
    ir_node* lbl = idLabel ( "gt_false" );
    ctx.yield ( mov ( res, constInt8 ( 0 ) ) );
    ctx.yield ( cmp ( left, right ) ); 
    ctx.yield ( jl ( lbl ) ); 
    ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
    ctx.yield ( placeLabel ( lbl ) ); 
    return res;
}


ir_node* emitGreaterThanOrEqual ( JitContextFlounder& ctx,
                                  SqlType             type,
                                  ir_node*            left,
                                  ir_node*            right ) {
    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::DATE:
        case SqlType::BIGINT: {
            res = emitGreaterThanOrEqualDECIMALxDATExBIGINT ( ctx, left, right );
            break;
        }
        default:
            throw ResqlError ( "GE code generation not implemented for datatype" );
    }
    return res;
}


/*** Equals ***/

ir_node* emitEqualsDECIMALxINTxBIGINTxDATExBOOL ( JitContextFlounder& ctx, 
                                                  ir_node*            left, 
                                                  ir_node*            right ) {

    ir_node* res = ctx.request ( vreg8 ( "eq_result" ) );
    ir_node* lbl = idLabel ( "eq_false" );
    ctx.yield ( mov ( res, constInt8 ( 0 ) ) );
    ctx.yield ( cmp ( left, right ) ); 
    ctx.yield ( jne ( lbl ) ); 
    ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
    ctx.yield ( placeLabel ( lbl ) ); 
    return res;
}


ir_node* emitEqualsVARCHAR ( JitContextFlounder& ctx, 
                             ir_node*            left, 
                             ir_node*            right ) {

    /* todo: support 1-8 byte call paremeters and return values */
    ir_node* res = ctx.request ( vreg8 ( "equals_varchar_result" ) );
    ctx.yield ( mcall2 ( res, (void*)&compareVarchar, left, right ) );
    return res;
}


ir_node* emitEqualsCHAR ( JitContextFlounder& ctx, 
                          ir_node*            left, 
                          ir_node*            right ) {

    /* todo: support 1-8 byte call paremeters and return values */
    ir_node* res = ctx.request ( vreg8 ( "equals_char_result" ) );
    ctx.yield ( mcall2 ( res, (void*)&compareChar, left, right ) );
    return res;
}


ir_node* emitEquals ( JitContextFlounder& ctx,  
                      SqlType             type,
                      ir_node*            left,
                      ir_node*            right ) {

    ir_node* res = nullptr;
    switch ( type.tag ) {
        case SqlType::DECIMAL:
        case SqlType::INT:
        case SqlType::BIGINT:
        case SqlType::BOOL:
        case SqlType::DATE: {
            res = emitEqualsDECIMALxINTxBIGINTxDATExBOOL ( ctx, left, right );
            break;
        }
        case SqlType::CHAR: {
            if ( type.charSpec().num > 1 ) {
                res = emitEqualsCHAR ( ctx, left, right );
            }
            else {
                res = emitEqualsDECIMALxINTxBIGINTxDATExBOOL ( ctx, left, right );
            }
            break;
        }
        case SqlType::VARCHAR: {
            res = emitEqualsVARCHAR ( ctx, left, right );
            break;
        }
        default:
            throw ResqlError ( "EQUALS code generation not implemented for datatype" );
    }
    return res;
}



/*** like ***/

ir_node* emitLike ( JitContextFlounder&  ctx, 
                    ir_node*             left, 
                    ir_node*             right ) {
    
    ir_node* res = ctx.request ( vreg8 ( "like_result" ) );
    ctx.yield ( mcall2 ( res, 
                         (void*) &stringLikeCheck, 
                         left,
                         right ) );
    return res;
}


/*** Count ***/

ir_node* emitCount ( JitContextFlounder&  ctx, 
                     ir_node*             child ) {

    /* todo: count only when child != NULL */
    SqlValue val;
    val.bigintData = 1;
    return emitConstantBIGINT ( ctx, val );
}


ir_node* emitCase ( JitContextFlounder&  ctx,
                    Expr*                expr ) {
  
    ir_node* res = ctx.vregForType ( expr->type );
    ir_node* afterCase = idLabel ( "afterCase" );
    Expr* child = expr->child;

    /* Handle WHEN _ THEN _ combinations */
    while ( child != nullptr &&
            child->tag == Expr::WHENTHEN ) {

        Expr* when = child->child;
        Expr* then = child->child->next;
        ir_node* nextWhen = idLabel ( "nextWhen" );
        ir_node* whenRes  = emitExpression ( ctx, when );
        ctx.yield ( cmp ( whenRes, constInt8 ( 0 ) ) ); 
        ctx.yield ( je ( nextWhen ) ); 
        ir_node* thenRes = emitExpression ( ctx, then );
        ctx.yield ( mov ( res, thenRes ) );
        ctx.yield ( jmp ( afterCase ) ); 
        ctx.clear ( whenRes );
        ctx.clear ( thenRes );
        ctx.yield ( placeLabel ( nextWhen ) );
        child = child->next;
    }

    /* Handle ELSE _ */
    if ( child != nullptr ) {
        ir_node* elseResult = emitExpression ( ctx, child );
        ctx.yield ( mov ( res, elseResult ) );
        ctx.clear ( elseResult );
    }
    ctx.yield ( placeLabel ( afterCase ) );
    return res;
}



/*** Typecast ***/
    
const unsigned int factorsDECIMAL[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 }; 


ir_node* emitTypecastDECIMALToDECIMAL ( JitContextFlounder&  ctx, 
                                        const DecimalSpec    fromSpec,
                                        const DecimalSpec    toSpec,
                                        ir_node*             child ) {

    if ( toSpec.scale == fromSpec.scale ) {
        std::cerr << "Warning: Unnecessary decimal typecast." << std::endl;
        return child;
    }
    
    ir_node* res;
    if ( toSpec.scale >= fromSpec.scale ) {
        int64_t factor = factorsDECIMAL [ toSpec.scale - fromSpec.scale ];
        res = emitMulDECIMALxBIGINT ( ctx, child, constInt64 ( factor ) );
    }
    else {
        int divisor = factorsDECIMAL [ fromSpec.scale - toSpec.scale ];
        res = emitDivBIGINT ( ctx, child, constInt64 ( divisor ) );
    }
    return res;
}


ir_node* emitTypecastBIGINTToDECIMAL ( JitContextFlounder&  ctx, 
                                       DecimalSpec          decSpec,
                                       ir_node*             child ) {

    int64_t factor = factorsDECIMAL [ decSpec.scale ];
    return emitMulDECIMALxBIGINT ( ctx, child, constInt64 ( factor ) );
}


ir_node* emitTypecastToDECIMAL ( JitContextFlounder&  ctx, 
                                 SqlType              fromType,
                                 DecimalSpec          toSpec,
                                 ir_node*             child ) {

    ir_node* res = nullptr;
    switch ( fromType.tag ) {

        case SqlType::DECIMAL:
            res = emitTypecastDECIMALToDECIMAL ( ctx, fromType.decimalSpec(), toSpec, child );
            break;

        case SqlType::BIGINT:
            res = emitTypecastBIGINTToDECIMAL ( ctx, toSpec, child );
            break;

        default:
            throw ResqlError ( "emitTypecastToDECIMAL(..) code generation not implemented for datatype" );
    }
    return res;
}


ir_node* emitTypecastINTToBIGINT ( JitContextFlounder&  ctx, 
                                   ir_node*             child ) {

    ir_node* res = ctx.request ( vreg64 ( "typecast_bigint" ) );
    ctx.yield ( movsx ( res, child ) );
    return res;
}


ir_node* emitTypecastDECIMALToBIGINT ( JitContextFlounder&  ctx, 
                                       DecimalSpec          decSpec,
                                       ir_node*             child ) {

    int divisor = factorsDECIMAL [ decSpec.scale ];
    return emitDivBIGINT ( ctx, child, constInt64 ( divisor ) );
}


ir_node* emitTypecastToBIGINT ( JitContextFlounder&  ctx, 
                                SqlType              fromType,
                                ir_node*             child ) {

    ir_node* res = nullptr;
    switch ( fromType.tag ) {
        
        case SqlType::INT:
            res = emitTypecastINTToBIGINT ( ctx, child );
            break;

        case SqlType::DECIMAL:
            res = emitTypecastDECIMALToBIGINT ( ctx, fromType.decimalSpec(), child );
            break;

        case SqlType::BIGINT:
            std::cerr << "Warning: Typecast from BIGINT to BIGINT." << std::endl;
            res = child;
            break;

        default:
            throw ResqlError ( "emitTypecastToBIGINT(..) code generation not implemented for datatype" );
    }
    return res;
}


ir_node* emitTypecast ( JitContextFlounder&  ctx, 
                        SqlType              fromType,
                        SqlType              toType,
                        ir_node*             child ) {

    ir_node* res = nullptr;
    switch ( toType.tag ) {
        case SqlType::DECIMAL:
            res = emitTypecastToDECIMAL ( ctx, fromType, toType.decimalSpec(), child );
            break;

        case SqlType::BIGINT:
            res = emitTypecastToBIGINT ( ctx, fromType, child );
            break;

        default:
            throw ResqlError ( "emitTypecast(..) code generation not implemented for datatype" );
    }
    return res;
}



/*** Attribute ***/

ir_node* emitAttribute ( JitContextFlounder& ctx, Expr* expr ) {
    ir_node* res = ctx.vregForType ( expr->type );
    ctx.yield ( mov ( res, ctx.symbolTable [ expr->symbol ] ) );
    return res;
}




/*** Expression ***/


ir_node* emitExpressionLiteral ( JitContextFlounder&  ctx, 
                                 Expr*                expr ) {
    
    #ifdef DEBUG_EXPRESSIONS
    ctx.comment ( exprTagNames [ expr->tag ] );
    #endif

    switch ( expr->tag ) {

        case Expr::ATTRIBUTE:
            return emitAttribute ( ctx, expr );

        case Expr::CONSTANT:
            return emitConstant ( ctx, expr );
        
        case Expr::STAR:
            return nullptr;

        default:
            throw ResqlError ( "emitExpressionLiteral(..) not implemented for expression type" + 
                std::string ( exprTagNames [ expr->tag ] ) );
    }
}


ir_node* emitExpressionUnary ( JitContextFlounder&  ctx,
                               Expr*                expr ) {

    /* Evaluate child expression */
    ir_node* child = emitExpression ( ctx, expr->child );
    
    #ifdef DEBUG_EXPRESSIONS
    ctx.comment ( exprTagNames [ expr->tag ] );
    #endif

    ir_node* res      = nullptr;
    SqlType exprType  = expr->type;
    SqlType childType = expr->child->type;

    switch ( expr->tag ) {

        case Expr::SUM:
        case Expr::AVG:
        case Expr::MIN:
        case Expr::MAX:
            res = ctx.vregForType ( expr->child->type );
            ctx.yield ( mov ( res, child ) );
            break;
        
        case Expr::AS:
            res = ctx.vregForType ( expr->type );
            ctx.yield ( mov ( res, child ) );
            break;

        case Expr::COUNT:
            res = emitCount ( ctx, child );
            break;

        case Expr::TYPECAST:
            res = emitTypecast ( ctx, childType, exprType, child );
            break;

        default:
            throw ResqlError ( "emitExpression(..) not implemented for expression type" + 
                std::string ( exprTagNames [ expr->tag ] ) );
    }
    /* free vregs allocated by child expressions */
    if ( child != nullptr && res != child ) {
        ctx.clear ( child );
    }
    return res;
}


ir_node* emitExpressionBinary ( JitContextFlounder&  ctx,
                                Expr*                expr ) {
   
    /* Evaluate child expressions and retrieve vregs allocated by *
     * their emit functions.                                      */
    ir_node* left  = emitExpression ( ctx, expr->child );
    ir_node* right = emitExpression ( ctx, expr->child->next );

    #ifdef DEBUG_EXPRESSIONS
    ctx.comment ( exprTagNames [ expr->tag ] );
    #endif

    ir_node* res          = nullptr;
    SqlType type          = expr->type;
    SqlType operationType = expr->child->type;
 
    switch ( expr->tag ) {

        case Expr::ADD:
            res = emitAdd ( ctx, type, left, right );
            break;

        case Expr::SUB:
            res = emitSub ( ctx, type, left, right );
            break;

        case Expr::MUL:
            res = emitMul ( ctx, type, left, right );
            break;

        case Expr::DIV:
            res = emitDiv ( ctx, type, left, right );
            break;

        case Expr::AND:
            res = emitAnd ( ctx, left, right );
            break;

        case Expr::OR:
            res = emitOr ( ctx, left, right );
            break;

        case Expr::LT:
            res = emitLessThan ( ctx, operationType, left, right );
            break;

        case Expr::LE:
            res = emitLessThanOrEqual ( ctx, operationType, left, right );
            break;

        case Expr::GT:
            res = emitGreaterThan ( ctx, operationType, left, right );
            break;

        case Expr::GE:
            res = emitGreaterThanOrEqual ( ctx, operationType, left, right );
            break;

        case Expr::EQ:
            res = emitEquals ( ctx, operationType, left, right );
            break;

        case Expr::NEQ: {
            res = ctx.request ( vreg8 ( "neqResult" ) );
            ctx.yield ( mov ( res, constInt8 ( 1 ) ) );
            ir_node* equals = emitEquals ( ctx, operationType, left, right );
            ctx.yield ( sub ( res, equals ) );
            ctx.clear ( equals );
            break;
        }

        case Expr::LIKE:
            res = emitLike ( ctx, left, right );
            break;

        default:
            throw ResqlError ( "emitExpressionBinary(..) not implemented for expression type" + 
                std::string ( exprTagNames [ expr->tag ] ) );
    }

    /* free vregs allocated by child expressions and preserve res */
    if ( res != left )   ctx.clear ( left );
    if ( res != right )  ctx.clear ( right );
    return res;
}


ir_node* emitExpressionOther ( JitContextFlounder&  ctx,
                               Expr*                expr ) {
    
    #ifdef DEBUG_EXPRESSIONS
    ctx.comment ( exprTagNames [ expr->tag ] );
    #endif

    switch ( expr->tag ) {

        case Expr::CASE:
            return emitCase ( ctx, expr );
        
        default:
            throw ResqlError ( "emitExpressionOther(..) not implemented for expression type" + 
                std::string ( exprTagNames [ expr->tag ] ) );
    }
}


ir_node* emitExpression ( JitContextFlounder&  ctx, 
                          Expr*                expr ) {
   
    if ( expr->type.tag == SqlType::NT ) {        
        error_msg ( ELEMENT_NOT_FOUND, "Expression type undefined in emitExpression(..)."
                                       " Have you derived the expression types?" );
    }

    /* Use value from symbol table if already stored there. */
    std::string exprName = getExpressionName ( expr );
    if ( ctx.symbolTable.count ( exprName ) > 0 ) {
        ir_node* res = ctx.vregForType ( expr->type );
        ctx.yield ( mov ( res, ctx.symbolTable [ exprName ] ) );
        return res;
    }

    switch ( expr->structureTag ) {
  
        case Expr::LITERAL:
            return emitExpressionLiteral ( ctx, expr );

        case Expr::UNARY:
            return emitExpressionUnary ( ctx, expr );
        
        case Expr::BINARY:
            return emitExpressionBinary ( ctx, expr );

        case Expr::OTHER:
            return emitExpressionOther ( ctx, expr );

        default:
            error_msg ( NOT_IMPLEMENTED, "emitExpression(..)" );   
            return nullptr;
    }
}

