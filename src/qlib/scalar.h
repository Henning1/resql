/**
 * @file
 * Hash table implementation.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#include "error.h"


#define DECIMAL_MAX  999999999999999999
#define DECIMAL_MIN -999999999999999999


// todo (?): Decide whether to check for precision overflows
//           when storage type of decimal has sufficient space.

static bool compareVarchar ( char* a, char* b ) {
    while ( *a != '\0' && *b != '\0' ) {
        if ( *a != *b ) return 0;
        a++; b++;
    }
    /* last position should equal too */ 
    if ( *a == *b ) return 1;
    else return 0;
}


static bool compareChar ( char* a, char* b ) {

    while ( *a != '\0' && *b != '\0' ) {
        if ( *a != *b ) return 0;
        a++; b++;
    }

    /* Handle trailing spaces for comparisons of char *
     * with different lengths.                        */
    while ( *a != '\0' ) {
        if ( *a != ' ' ) return 0;
        a++;
    }
    while ( *b != '\0' ) {
        if ( *b != ' ' ) return 0;
        b++;
    }

    return 1;
}


/* The LIKE operator was adapted from DogQC   */
static inline bool cmpLike ( const char c,
                             const char l ) {

    return ( c == l ) || ( l == '_' );
}


static bool stringLikeCheck ( char* string, 
                              char* like ) {

    /* Get last position of input strings.    *
     * In DogQC both start _and_ end pointers *
     * are available for strings.             */
    char* stringEnd = string; while ( *stringEnd != '\0' ) stringEnd++;
    char* likeEnd = like;     while ( *likeEnd   != '\0' ) likeEnd++;

    char *sPos, *lPos, *sTrace, *lTrace;
    char *lInStart = like;
    char *lInEnd   = likeEnd;
    char *sInStart = string;
    char *sInEnd   = stringEnd;

    // prefix 
    if ( *like != '%' ) { 
        sPos = string;
        lPos = like;
        for ( ; lPos < likeEnd && sPos < stringEnd && (*lPos) != '%'; ++lPos, ++sPos ) {
            if ( !cmpLike ( *sPos, *lPos ) )
                return false;
        }
        lInStart = lPos; 
        sInStart = sPos; 
    }
    if ( lInStart == likeEnd ) {
        // handle no-'%' likes
        return sInStart == stringEnd;
    }
    
    // suffix 
    if ( *(likeEnd-1) != '%' ) {
        sPos = stringEnd-1;
        lPos = likeEnd-1;
        for ( ; lPos >= like && sPos >= string && (*lPos) != '%'; --lPos, --sPos ) {
            if ( !cmpLike ( *sPos, *lPos ) )
                return false;
        }
        lInEnd = lPos;
        sInEnd = sPos+1; // first suffix char 
    }

    // infixes 
    if ( lInStart < lInEnd ) {
        lPos = lInStart+1; // skip '%'
        sPos = sInStart;
        while ( sPos < sInEnd && lPos < lInEnd ) { // loop 's' string
            lTrace = lPos;
            sTrace = sPos;
            while ( cmpLike ( *sTrace, *lTrace ) && sTrace < sInEnd ) { // loop infix matches
                ++lTrace;
                if ( *lTrace == '%' ) {
                    lPos = ++lTrace;
                    sPos = sTrace;
                    break;
                }
                ++sTrace; 
            }
            ++sPos;
        }
    }
    return lPos >= lInEnd;
}



// Add decimal numbers
// A type: DECIMAL ( a, b )
// B type: DECIMAL ( a, b )
// Result: DECIMAL ( a + delta, b ) 
static int64_t add_DECIMAL ( int64_t x, 
                             int64_t y ) {

    int64_t z;

    if ( __builtin_add_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
 
//    if ( z > DECIMAL_MAX || z < -DECIMAL_MIN ) {
//        query_error ( ARITHMETIC_OVERFLOW );
//    }
    return z;    
}


// Subtract decimal numbers
// A type: DECIMAL ( a, b )
// B type: DECIMAL ( a, b )
// Result: DECIMAL ( a + delta, b ) 
static int64_t sub_DECIMAL ( int64_t x, 
                             int64_t y ) {

    int64_t z;

    if ( __builtin_sub_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
 
//    if ( z > DECIMAL_MAX || z < -DECIMAL_MIN ) {
//        query_error ( ARITHMETIC_OVERFLOW );
//    }
    return z;    
}


// Multiply decimal numbers
// A type: DECIMAL ( a, b )
// B type: DECIMAL ( c, d )
// Result: DECIMAL ( a + c, c + d ) 
static int64_t mul_DECIMAL ( int64_t x, 
                             int64_t y ) { 
    int64_t z;

    //std::cout << " mul " << x << " and " << y << std::endl;

    if ( __builtin_mul_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
    
    //std::cout << " result " << z << std::endl;

    return z;
}


// Divide decimal numbers
// A type: DECIMAL ( a, b )
// B type: DECIMAL ( c, d )
// Result: DECIMAL ( todo ). 
// See: http://msdn.microsoft.com/en-us/library/ms190476(SQL.90).aspx 

static int64_t div_DECIMAL ( int64_t x, 
                             int64_t y ) { 

    query_error ( NOT_IMPLEMENTED );
    return 0;
}




// Add BIGINT numbers
static int64_t add_BIGINT  ( int64_t x, 
                             int64_t y ) {
    int64_t z;
    if ( __builtin_add_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
    return z;    
}


// Subtract BIGINT numbers
static int64_t sub_BIGINT  ( int64_t x, 
                             int64_t y ) {

    int64_t z;

    if ( __builtin_sub_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
 
    if ( z > DECIMAL_MAX || z < -DECIMAL_MIN ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
    return z;    
}


// Multiply BIGINT numbers
static int64_t mul_BIGINT  ( int64_t x, 
                             int64_t y ) { 
    int64_t z;

    std::cout << " mul " << x << " and " << y << std::endl;

    if ( __builtin_mul_overflow ( x, y, &z ) ) {
        query_error ( ARITHMETIC_OVERFLOW );
    }
    
    std::cout << " result " << z << std::endl;

    return z;
}


// Divide BIGINT numbers
static int64_t div_BIGINT  ( int64_t x, 
                             int64_t y ) { 
    
    if ( !y ) {
        query_error ( DIVISION_BY_ZERO );
    }
    
    if ( y == -1 ) {
        return sub_BIGINT ( 0, x );
    }

    return x / y;
}
