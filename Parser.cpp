#include "Parser.hpp"

// " CONSTRUCTOR "
Parser::Parser() : MilaContext(), MilaBuilder(MilaContext), MilaModule("mila", MilaContext) {}

int currenttok;
map<char, int> BinopPrecedence;

int getNextToken()
{ return currenttok = gettok(); }

// Returns the precedence of the upcoming binary operator token
int GetTokPrecedence()
{
  // not an ASCII or a 2-charecter operator
  if( !isascii(currenttok) && currenttok != tok_notequal  && currenttok != tok_lessequal
                           && currenttok != tok_greaterequal  && currenttok != tok_assign
                           && currenttok != tok_or )
 { return -1; }
 // Ensure that the binary operator was declared and return its precedence
 int precedence = BinopPrecedence[currenttok];
 if( precedence <= 0 )
 {  return -1; }
 return precedence;
}

// Error logging
unique_ptr<expression> LogError(const char *str)
{
  fprintf(stderr, "ERROR: %s\n", str);
  return nullptr;
}
unique_ptr<funcproto> LogErrorP(const char *str)
{
  LogError(str);
  return nullptr;
}

bool Parser::Parse()
{
    getNextToken();
    return true;
}

const Module & Parser::Generate()
{
    // create writeln function
    {
      std::vector<Type*> Ints(1, Type::getInt32Ty(MilaContext));
      FunctionType * FT = FunctionType::get(Type::getInt32Ty(MilaContext), Ints, false);
      Function * F = Function::Create(FT, Function::ExternalLinkage, "writeln", MilaModule);
      for (auto & Arg : F->args())
          Arg.setName("x");
    }
    // create main function
    {
      FunctionType * FT = FunctionType::get(Type::getInt32Ty(MilaContext), false);
      Function * MainFunction = Function::Create(FT, Function::ExternalLinkage, "main", MilaModule);
      // block
      BasicBlock * BB = BasicBlock::Create(MilaContext, "entry", MainFunction);
      MilaBuilder.SetInsertPoint(BB);
      // call writeln with value from lexel
      MilaBuilder.CreateCall(MilaModule.getFunction("writeln"), {
        ConstantInt::get(MilaContext, APInt(32, value))
      });
      // return 0
      MilaBuilder.CreateRet(ConstantInt::get(Type::getInt32Ty(MilaContext), 0));
    }
    return this->MilaModule;
}

// EXPRESSION := UNARY BINOP(RHS)
unique_ptr<expression> ParseExpression()
{
  auto LHS = ParseUnary();
  if (!LHS)
  { return nullptr; }
  return ParseBinOpRHS(0, move(LHS));
}

// NUMBEREXPR := NUMBER
unique_ptr<expression> ParseNumberExpr()
{
  auto result = make_unique<numexpr>(value);
  getNextToken();
  return move(result);
}

// PARENTEXPRESSION := ( EXPRESSION )
unique_ptr<expression> ParseParentExpr()
{
  // EAT '('
  getNextToken();

  auto result = ParseExpression();
  if( !result )
  { return nullptr; }

  if( currenttok != ')' )
  { return LogError("CLOSING BRACKET ')' MISSING FOR THE EXPRESSION"); }
  // EAT ')'
  getNextToken();
  return result;
}

// IDENTIFIEREXPR := IDENTIFIER
// IDENTIFIEREXPR := IDENTIFIER ( EXPRESSION* )
unique_ptr<expression> ParseIdentifierExpr()
{
  string identname = idname;
  getNextToken();

  // If there's no preceding expression then its a variable reference
  if( currenttok != '(' )
  { return make_unique<variableexpr>(identname); }

  // Otherwise its a function call
  vector<unique_ptr<expression>> arguments;
  // EAT '('
  getNextToken();
  // Accumulate the arguments of the fucntion call
  if( currenttok != ')' )
  {
    while( true )
    {
      if( auto a = ParseExpression() )
      { arguments.push_back(move(a)); }
      else
      { return nullptr; }

      if( currenttok == ')' )
      { break; }
      if( currenttok != ',' )
      { return LogError("MISSING SEPERATOR ',' IN LIST OF ARGUMENTS"); }

      getNextToken();
    }
  }
  // EAT ')'
  getNextToken();
  // EAT ';'
  if( currenttok == ';' )
  { getNextToken(); }

  return make_unique<callexpr>(identname, move(arguments));
}

// PRIMARY := BEGIN / END
// PRIMARY := IDENTIFIEREXPR
// PRIMARY := NUMBEREXPR
// PRIMARY := PARENTEXPR
// PRIMARY := IFEXPR
// PRIMARY := FOREXPR
// PRIMARY := VAREXPR
unique_ptr<expression> ParsePrimary()
{
  switch( currenttok )
  {
    default:
      if( currenttok == tok_begin || currenttok == tok_end )
      { return nullptr; }
      return LogError("EXPECTED EXPRESSION MISSING");
    case( tok_identifier ):
      return ParseIdentifierExpr();
    case( tok_number ):
      return ParseNumberExpr();
    case( '(' ):
      return ParseParentExpr();
    case( tok_if ):
      return ParseIfExpr();
    case( tok_for ):
      return ParseForExpr();
    case( tok_var ):
      return ParseVarExpr();
  }
}

// BINOPRHS := ( + PRIMARY )*
unique_ptr<expression> ParseBinOpRHS(int ExprPrec, unique_ptr<expression> LHS)
{
  while( true )
  {
    // get the operator's precedence
    int prec = GetTokPrecedence();
    // If the operator is a binary operator that dosen't bind at least as tightly as the current binop
    if( prec < ExprPrec )
    { return LHS; }

    // binary operator
    int binop = currenttok;
    getNextToken();

    // Parse the primary expression after the binary operator
    auto RHS = ParseUnary();
    if( !RHS )
    { return nullptr; }

    // If the binary operator binds less tightly with the RHS than the operator after the RHS
    // the pending operator takes the RHS as its LHS
    int nextprec = GetTokPrecedence();
    if( prec < nextprec )
    {
      RHS = ParseBinOpRHS(prec + 1, move(RHS));
      if( !RHS )
      { return nullptr; }
    }

    // Merge the LHS and RHS
    LHS = make_unique<binexpr>(binop, move(LHS), move(RHS));
  }
}

// PROTOTYPE := ID ( ID* )
unique_ptr<funcproto> ParsePrototype()
{
  bool isproc = false;
  if( currenttok == tok_procedure )
  {
    isproc = true;
    getNextToken();
  }

  if( currenttok != tok_identifier )
  { return LogErrorP("MISSING FUNCTION NAME"); }

  string funcname = idname;
  getNextToken();

  if( currenttok != '(' )
  { return LogErrorP("MISSING OPENING BRACKET '('"); }

  vector<string> arguments;
  while( currenttok != ')' && getNextToken() == tok_identifier )
  {
    arguments.push_back(idname);
    getNextToken();
    if( currenttok == ':' )
    { getNextToken(); }
    else
    { return LogErrorP("MISSING ':'"); }

    if( currenttok == tok_integer )
    { getNextToken(); }
    else
    { return LogErrorP("EXPECTED AN INTEGER"); }
  }
  if( currenttok != ')' )
  { return LogErrorP("MISSING CLOSING BRACKET ')'"); }
  getNextToken();

  if( currenttok == ':' )
  {
    if( currenttok == ':' )
    { getNextToken(); }
    else
    { return LogErrorP("MISSING ':'"); }

    if( currenttok == tok_integer )
    { getNextToken(); }
    else
    { return LogErrorP("EXPECTED AN INTEGER"); }
  }

  return make_unique<funcproto>(funcname, move(arguments), false, 0, isproc);
}

// DEFINITION := def "PROTOTYPE" FUNCTION
unique_ptr<funct> ParseDefinition()
{
  bool isproc = false;
  if( currenttok == tok_procedure )
  { isproc = true; }
  getNextToken();
  auto proto = ParsePrototype();
  if( !proto )
  { return nullptr; }

  if( currenttok == ':' )
  {
    if( currenttok == ':' )
    { getNextToken(); }
    else
    {
      LogErrorP("MISSING ':'");
      return nullptr;
    }
    if( currenttok == tok_integer )
    { getNextToken(); }
    else
    {
      LogErrorP("EXPECTED AN INTEGER");
      return nullptr;
    }
  }

  if( currenttok == ';' )
  { getNextToken(); }

  if( idname == "forward" )
  {
    getNextToken();
    if( currenttok == ';' )
    { getNextToken(); }
    return nullptr;
  }

  if( currenttok != tok_begin && currenttok != tok_var )
  { LogErrorP("EXPECTED 'begin'"); }


  vector<unique_ptr<expression>> astexpr;

  if(currenttok == tok_var)
  { astexpr.push_back(ParseExpression()); }

  if (currenttok == tok_begin)
  { getNextToken(); }

  while( true )
  {
    if( currenttok == tok_end )
    { break; }
    else if( auto e = ParseExpression() )
    {
      if( currenttok == ';' )
      { getNextToken(); }
      astexpr.push_back(move(e));
    }
    else
    {
      if (currenttok == tok_end)
      { break; }
      return nullptr;
    }
  }
  return make_unique<funct>(move(proto), move(astexpr), isproc);
}

//TOPLEVELEXPR := EXPRESSION
unique_ptr<funct> ParseTopLevelExpr()
{
  vector<unique_ptr<expression>> body;
  if( auto e = ParseExpression() )
  {
    // New function prototype declared
    auto proto = make_unique<funcproto>("main", vector<string>());
    body.push_back(move(e));
    return make_unique<funct>(move(proto), move(body), false);
  }
  return nullptr;
}

//EXTERNAL := FUNCTION PROTOTYPE ( USER FUNCTIONS )
unique_ptr<funcproto> ParseExternal()
{
  getNextToken();
  return ParsePrototype();
}


// IFEXPR := IF / THEN / ELSE EXPRESSIONS
unique_ptr<expression> ParseIfExpr()
{
  getNextToken();

  auto cond = ParseExpression();
  if( !cond )
  { return nullptr; }

  if( currenttok != tok_then )
  { return LogError("MISSING 'then'");}
  getNextToken();

  bool ifblock = false;
  if( currenttok == tok_begin )
  {
    ifblock = true;
    getNextToken();
  }

  vector<unique_ptr<expression>> thenblock;
  while( currenttok != tok_end )
  {
    if( auto e = ParseExpression() )
    {
      if( currenttok == ';' )
      { getNextToken(); }
      thenblock.push_back(move(e));

      if( !ifblock )
      { break; }
    }
    else
    {
      if( currenttok == tok_end )
      { break; }
      return nullptr;
    }
  }

  if( ifblock && currenttok != tok_end )
  { return LogError("MISSING 'end'"); }
  if( ifblock )
  { getNextToken(); }

  bool iselse = false;
  if( currenttok == tok_else )
  {
    iselse = true;
    getNextToken();
    auto elsee = ParseExpression();
    if( !elsee )
    { return nullptr; }

    return make_unique<ifexpr>(move(cond), move(thenblock), move(elsee), iselse);
  }

  return make_unique<ifexpr>(move(cond), move(thenblock));
}

// FOREXPR := FOR IDENT = EXPR , EXPR (, EXPR) ? IN EXPR
unique_ptr<expression> ParseForExpr()
{
  getNextToken();
  if (currenttok != tok_identifier)
  { return LogError("MISSING 'identifier' AFTER THE 'for'"); }

  string identname = idname;
  getNextToken();

  if( currenttok != tok_assign )
  { return LogError("MISSING ':=' AFTER THE 'for'"); }
  getNextToken();


  auto start = ParseExpression();
  if( !start )
  { return nullptr; }

  bool to;
  if( currenttok == tok_to )
  { to = true; }
  else if( currenttok == tok_downto )
  { to = false; }
  else
  { return LogError("MISSING 'to' OR 'downto' AFTER THE 'for'"); }
  getNextToken();

  auto end = ParseExpression();
  if( !end )
  { return nullptr; }

  unique_ptr<expression> step;
  if (currenttok == tok_do)
  { getNextToken(); }
  else
  { LogError("MISSING 'do' AFTER 'for'"); }
  if (currenttok == tok_begin)
  { getNextToken(); }
  else
  { LogError("MISSING 'begin' AFTER 'for'"); }

  vector<unique_ptr<expression>> body;
  while (currenttok != tok_end)
  {
    if( auto expr = ParseExpression() )
    {
      if( currenttok == ';' )
      { getNextToken(); }
      body.push_back(move(expr));
    }
    else
    {
      if( currenttok == tok_end )
      {
        getNextToken();
        return make_unique<forexpr>(identname, move(start), move(end), move(step), move(body), to);
      }
      return nullptr;
    }
  }
  if (currenttok == tok_end)
  { getNextToken(); }

  return make_unique<forexpr>(identname, move(start), move(end), move(step), move(body), to);
}

// UNARY := PRIMARY
// UNARY := !PRIMARY
unique_ptr<expression> ParseUnary()
{
  // if the current token is not an operator its a primary expression
  if( !isascii(currenttok) || currenttok == '(' || currenttok == ',' )
  { return ParsePrimary(); }

  int op = currenttok;
  getNextToken();
  if( auto operand = ParseUnary() )
  { return make_unique<unaryexpr>(op, move(operand)); }
  return nullptr;
}

// VAREXPR := VAR IDENTIFIER ( = EXPRESSION )?
// VAREXPR := VAR (, IDENTIFIER ( = EXPRESSION )? )* IN EXPRESSION
unique_ptr<expression> ParseVarExpr()
{
  getNextToken();

  vector<pair<string, unique_ptr<expression>>> varnames;

  // At least one variable name
  if( currenttok != tok_identifier )
  {return LogError("EXPECTED AN 'identifier' AFTER 'var'");}

  string name = idname;
  getNextToken();

  unique_ptr<expression> init = nullptr;
  varnames.emplace_back(name, move(init));

  if( currenttok == ':' )
  { HandleSequenceVars(varnames); }
  else if( currenttok == ',' )
  { HandleListVars(varnames); }
  else
  { return LogError("MISSING ',' OR ':' AFTER THE 'identifier'"); }

  return make_unique<varexpr>(move(varnames));
}

// CONSTEXPR
unique_ptr<expression> ParseConstExpr()
{
  getNextToken();

  vector<pair<string, unique_ptr<expression>>> varnames;

  if (currenttok != tok_identifier)
  { return LogError("EXPECTING AN 'identifier' AFTER 'const'"); }

  while( true )
  {
    if( currenttok != tok_identifier )
    { break; }

    string name = idname;
    constants.insert(idname);
    getNextToken();

    unique_ptr<expression> init = nullptr;
    if (currenttok == '=')
    {
      getNextToken();
      init = ParseExpression();
      if (!init)
      { return nullptr; }
    }
    else
    { return LogError("MISSING INITIALIZATION FOR THE CONSTANT"); }

    varnames.emplace_back(name, move(init));
    if( currenttok != ';' )
    { break; }
    getNextToken();

    if( currenttok == tok_function || currenttok == tok_begin || currenttok == tok_var )
    { break; }
    if( currenttok != tok_identifier )
    { return LogError("MISSING 'identifier' list"); }
  }

  return make_unique<constantexpr>(move(varnames));
}


void InitializeModuleAndPassManager()
{
  // Create a new LLVM " MODULE "
  context = make_unique<LLVMContext>();
  module = make_unique<Module>("mila", *context);

  // Create a new LLVM " BUILDER " for the " MODULE "
  builder = make_unique<IRBuilder<>>(*context);

  // Create a new " FUNCTION PASS MANAGER " (FPM) attached to it
  fpm = make_unique<legacy::FunctionPassManager>(module.get());

  // Initialize the FPM
  fpm->add(createPromoteMemoryToRegisterPass());
  fpm->add(createInstructionCombiningPass());
  fpm->add(createReassociatePass());
  fpm->add(createGVNPass());
  fpm->add(createCFGSimplificationPass());
  fpm->doInitialization();
}

void HandleDefinition()
{
  if( auto func = ParseDefinition() )
  {
    auto *check = func->codegen();
    // if( auto *check = func->codegen() )
    // { fprintf(stderr, "FUNCTION DEFINITION ERROR\n"); }
  }
  else
  { getNextToken(); }
}

void HandleTopLevelExpression()
{
  if( auto func = ParseTopLevelExpr() )
  { func->codegen(); }
  else
  { getNextToken(); }
}

void writeln()
{
  vector<Type*> Ints(1, Type::getInt32Ty(*context));
  FunctionType * functype = FunctionType::get(Type::getInt32Ty(*context), Ints, false);
  Function * func = Function::Create(functype, Function::ExternalLinkage, "writeln", module.get());
  for (auto & Arg : func->args())
  { Arg.setName("x"); }
}
void readln()
{
  std::vector<Type *> Ints(1, Type::getInt32PtrTy(*context));
  FunctionType * functype = FunctionType::get(Type::getInt32Ty(*context), Ints, false);
  Function * func = Function::Create(functype, Function::ExternalLinkage, "readln", module.get());
  for (auto & Arg : func->args())
  { Arg.setName("x"); }
}

void HandleVarGlobal()
{
  if( auto func = ParseVarExpr() )
  {
    auto check = func->makeglobal();
    // if( auto check = func->makeglobal())
    // { fprintf(stderr, "VARIABLE DEFINITION ERROR\n"); }
  }
  else
  { getNextToken(); }
}

void HandleConstVal()
{
  if( auto func = ParseConstExpr() )
  {
    auto check = func->makeglobal();
    // if( auto check = func->makeglobal())
    // { fprintf(stderr, "CONSTANT DEFINITION ERROR\n"); }
  }
  else
  { getNextToken(); }
}

void HandleForward()
{
  if( auto funcproto = ParseExternal() )
  {
    auto * check = funcproto->codegen();
    // if( auto * check = funcproto->codegen() )
    // { fprintf(stderr, "EXTERN DEFINITION ERROR\n"); }
  }
  else
  { getNextToken(); }
}

// VAR X : INTEGER; Y:INTEGER ;
void HandleSequenceVars( vector<pair<string, unique_ptr<expression>>> & varnames )
{
  bool flag = true;
  while( true )
  {

    if( currenttok != tok_identifier && !flag )
    { break; }

    if( !flag )
    {
      string name = idname;
      getNextToken();

      unique_ptr<expression> init = nullptr;
      varnames.emplace_back(name, move(init));
    }

    if( currenttok == ':' )
    {
      getNextToken();

      if( currenttok != tok_integer )
      {
        LogErrorP("EXPECTED AN 'integer' after ':'");
        return;
      }
      getNextToken();
      if( currenttok != ';' )
      {
        LogErrorP("MISSING ';' AFTER THE 'integer'");
        return;
      }
      getNextToken();
    }
    else
    {
      LogErrorP("MISSING ':' OR ',' AFTER THE 'var'");
      return;
    }
    flag = false;
  }
}

// VAR X,Y : INTEGER;
void HandleListVars( vector<pair<string, unique_ptr<expression>>> & varnames )
{
  bool flag = true;
  while( true )
  {

    if( currenttok != tok_identifier && !flag )
    { break; }

    if( !flag )
    {
      string name = idname;
      getNextToken();

      unique_ptr<expression> init = nullptr;
      varnames.emplace_back(name, move(init));
    }

    // END OF THE VAR LIST
    if( currenttok != ',')
    { break; }
    getNextToken();

    if( currenttok != tok_identifier )
    {
      LogErrorP("MISSING 'identifier' LIST AFTER 'var'");
      return;
    }
    flag = false;
  }
  if( currenttok != ':' )
  {
    LogErrorP("MISSING ':' AFTER 'var'");
    return;
  }
  getNextToken();

  if( currenttok != tok_integer )
  {
    LogErrorP("EXPECTED AN 'integer' AFTER 'var'");
    return;
  }
  getNextToken();

  if( currenttok != ';' )
  {
    LogErrorP("MISSING ';' AFTER 'var'");
    return;
  }
  getNextToken();
}

// PARSER " DRIVER "
void MainLoop()
{
  while( true )
  {
    switch( currenttok )
    {
      case tok_eof:
        return;
      // IGNORE ';' AT TOP LEVEL PARSING
      case ';':
        getNextToken();
        break;
      case tok_function:
        HandleDefinition();
        break;
      case tok_procedure:
        HandleDefinition();
        break;
      case tok_forward:
        HandleForward();
        break;
      case tok_var:
        HandleVarGlobal();
        break;
      // END OF THE PROGRAM
      case '.':
        return;
      case tok_end:
        getNextToken();
        break;
      case tok_const:
        HandleConstVal();
        break;
      default:
        HandleTopLevelExpression();
        break;
    }
  }
}
