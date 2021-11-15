#include "Lexer.hpp"

 string idname;
 int value;

 // boolean helper function checks whether the charecter could be a two charecter operation ( := / <= / != )
 bool twocharop( char c )
 {
   // : < = > ! |
   if( c != 59 && ( ( c > 57 && c < 67 ) || c == 33 || c == 124 ) )
   {  return true; }
   else
   {  return false; }
 }

int gettok() {
    static int input = ' ';

    // skip whitespaces
    while( isspace(input) )
    { input = getchar(); }

    // " IDENTIFIERS "
    if( isalpha(input) )
    {
      idname = input;
      // keep reading and appending identifier name [A-Z][a-z][0-9]
      while( isalnum(input = getchar()) )
      { idname += input; }
      // check if the name matches a keyword identifier
      if( idname == "begin" )
      { return tok_begin; }
      if( idname == "end" )
      { return tok_end; }
      if( idname == "const" )
      { return tok_const; }
      if( idname == "procedure")
      { return tok_procedure; }
      if( idname == "forward" )
      { return tok_forward; }
      if( idname == "function" )
      { return tok_function; }
      if( idname == "if" )
      { return tok_if; }
      if( idname == "then" )
      { return tok_then; }
      if( idname == "else" )
      { return tok_else; }
      if( idname == "program" )
      { return tok_program; }
      if( idname == "while" )
      { return tok_while; }
      if( idname == "exit" )
      { return tok_exit; }
      if( idname == "var" )
      { return tok_var; }
      if( idname == "integer" )
      { return tok_integer; }
      if( idname == "for" )
      { return tok_for; }
      if( idname == "do" )
      { return tok_do; }
      if( idname == "to" )
      { return tok_to; }
      if( idname == "downto" )
      { return tok_downto; }
      // else its an identifier and its name is stored in ( idname )
      return tok_identifier;
    }

    // " DECIMAL " NOs [0 - 9]
    if( isdigit(input) )
    {
      // read the whole number into the string ( number )
      string number;
      do
      {
        number += input;
        input = getchar();
      } while( isdigit(input) );
      // convert the string into an integer of base 10
      value = (int) strtod(number.c_str(), nullptr);
      return tok_number;
    }

    // " OCTAL " NOs [0 - 7] (&)
    if( input == '&' )
    {
      input = getchar();
      string number;
      do
      {
        number += input;
        input = getchar();
      } while( isdigit(input) );
      // convert the string into an integer of base 8
      char * tmp;
      value = (int) strtol(number.c_str(), &tmp, 8);
      return tok_number;
    }

    // " HEXIDECIMAL " NOs [A-F][a-f][0-9] ($)
    if( input == '$' )
    {
      input = getchar();
      string number;
      do
      {
        number += input;
        input = getchar();
      } while( isdigit(input) || isalnum(input) );
      // convert the string into an integer of base 8
      char * tmp;
      value = (int) strtol(number.c_str(), &tmp, 16);
      return tok_number;
    }

    // " COMMENTS " (#)
    if( input == '#')
    {
      // loop until the end of the line / file is reached
      do
      { input = getchar(); }
      while( input != EOF && input != '\n' && input != '\r' );
      if( input != EOF )
      { return gettok(); }
    }

    // " OPERATORS "
    if( twocharop(input) )
    {
      string operation;
      operation = input;
      while( twocharop(cin.peek()) )
      {
        input = getchar();
        operation += input;
        if( operation == "!=" )
        {
          input = getchar();
          return tok_notequal;
        }
        if( operation == "<=" )
        {
          input = getchar();
          return tok_lessequal;
        }
        if( operation == ">=" )
        {
          input = getchar();
          return tok_greaterequal;
        }
        if( operation == ":=" )
        {
          input = getchar();
          return tok_assign;
        }
        if( operation == "||" )
        {
          input = getchar();
          return tok_or;
        }
        if( operation == "==" )
        {
          input = getchar();
          return tok_eq;
        }
      }
    }

    // " EOF "
    if( input == EOF )
    { return tok_eof; }

    // RETURN ASCII value
    int ascii = input;
    input = getchar();
    return ascii;
}
