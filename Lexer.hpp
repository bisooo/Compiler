#ifndef PJPPROJECT_LEXER_HPP
#define PJPPROJECT_LEXER_HPP

#include <iostream>
using namespace std;

// set if ( tok_identifier )
extern string idname;
// set if ( tok_number )
extern int value;

// returns the next token value from the stdin
int gettok();

// RETURNS ASCII value ( 0 - 255 ) for unknown chars, or ( -32, -1 ) for valid tokens
enum Token
{
    // " END OF FILE "
    tok_eof =           -1,
    // " IDENTIFIER "
    tok_identifier =    -2,
    // " NUMBER "
    tok_number =        -3,

    // " KEYWORDS "
    tok_begin =         -4,
    tok_end =           -5,
    tok_const =         -6,
    tok_procedure =     -7,
    tok_forward =       -8,
    tok_function =      -9,
    tok_if =            -10,
    tok_then =          -11,
    tok_else =          -12,
    tok_program =       -13,
    tok_while =         -14,
    tok_exit =          -15,
    tok_var =           -16,
    tok_integer =       -17,
    tok_for =           -18,
    tok_do =            -19,

    // " 2-char OPERATORS "
    tok_notequal =      -20,
    tok_lessequal =     -21,
    tok_greaterequal =  -22,
    tok_assign =        -23,
    tok_or =            -24,
    tok_eq =            -25,

    // " 3-char OPERATORS "
    tok_mod =           -26,
    tok_div =           -27,
    tok_not =           -28,
    tok_and =           -29,
    tok_xor =           -30,

    // " LOOP "
    tok_to =            -31,
    tok_downto =        -32
};

#endif //PJPPROJECT_LEXER_HPP
