#include "ast.hpp"
#include "Parser.hpp"

// LLVM " CONTEXT "
unique_ptr< LLVMContext > context;
// LLVM " BUILDER "
unique_ptr< IRBuilder<> > builder;
// LLVM " MODULE " - functions & global variables
unique_ptr< Module > module;
// " SCOPED " variables values ( Symbol Table )
map< string, AllocaInst * > namedvalues;
// " FUNCTIONs " and their blueprints
map< string, unique_ptr< funcproto > > functions;
unique_ptr< legacy::FunctionPassManager > fpm;
ExitOnError exitonerr;
set<string> constants;

// Error logging
Value * LogErrorV( const char * message )
{
  LogError(message);
  return nullptr;
}

Function * getfunction( string name )
{
  // check if its already exists in the current LLVM " MODULE "
  auto * func = module->getFunction(name);
  if( func )
  { return func; }
  // else search for it in the map of function prototypes
  auto find = functions.find(name);
  if( find != functions.end() )
  { return find->second->codegen(); }
  return nullptr;
}

// Creates an alloca instruction in the entry block of the function, used for mutable variables
AllocaInst * createblockalloc( Function * func , StringRef var )
{
  IRBuilder<> tmp( &func->getEntryBlock(), func->getEntryBlock().begin() );
  return tmp.CreateAlloca(Type::getInt32Ty(*context), nullptr, var);
}

void writelnfunc()
{
  vector< Type * > Ints(1, Type::getInt32Ty(*context));
  FunctionType * functype = FunctionType::get(Type::getInt32Ty(*context), Ints, false);
  Function * func = Function::Create(functype, Function::ExternalLinkage, "writeln", module.get());
  for( auto & arg : func->args() )
  { arg.setName("x"); }
}

void readlnfunc()
{
    vector< Type * > Ints(1, Type::getInt32PtrTy(*context));
    FunctionType * functype = FunctionType::get(Type::getInt32Ty(*context), Ints, false);
    Function * func = Function::Create(functype, Function::ExternalLinkage, "readln", module.get());
    for( auto & arg : func->args() )
    { arg.setName("x"); }
}

// " EXPRESSION " BASE CLASS
bool expression::makeglobal()
{ return true; }
const string expression::getname() const
{ return "ERROR BASE CLASS NAME"; }

// " FUNCTION PROTOTYPE "
const string & funcproto::getname() const
{ return name; }
bool funcproto::isunary() const
{ return isoperator && arguments.size() == 1; }
bool funcproto::isbinary() const
{ return isoperator && arguments.size() == 2; }
char funcproto::operatorname() const
{ return name[name.size() - 1]; }
unsigned funcproto::getprecedence() const
{ return precedence; }
// " FUNCTION PROTOTYPE " CODEGEN
Function * funcproto::codegen()
{
  vector<Type*> integers(arguments.size(), Type::getInt32Ty(*context));

  // Function type ( RETURN ) either VOID or INTEGER
  FunctionType *functype = isprocedure ? functype =
                           FunctionType::get(Type::getVoidTy(*context), integers, false)
                           :
                           FunctionType::get(Type::getInt32Ty(*context), integers, false);

  Function * func = Function::Create(functype, Function::ExternalLinkage, name, module.get());

  // Set the names of all the functions arguments
  unsigned index = 0;
  for( auto & a : func->args() )
  { a.setName(arguments[index++]); }

  return func;
}

// " FUNCTION " CODEGEN
Function * funct::codegen()
{
  auto & p = *proto;
  functions[proto->getname()] = move(proto);

  Function * thefunc = getfunction(p.getname());
  if( !thefunc )
  { return nullptr; }

  if( p.isbinary() )
  { BinopPrecedence[p.operatorname()] = p.getprecedence(); }

  // BASIC BLOCK to create an entry point for insertion
  BasicBlock * bb;
  if( thefunc->begin() == thefunc->end() )
  {
    bb = BasicBlock::Create(*context, "entry", thefunc);
    builder->SetInsertPoint(bb);
  }
  else
  { builder->SetInsertPoint(&*prev(thefunc->end())); }

  // Allocate and add the functions arguments into the symbol table
  namedvalues.clear();
  for( auto & a : thefunc->args() )
  {
    AllocaInst *alloc = createblockalloc(thefunc, a.getName());
    builder->CreateStore(&a, alloc);
    namedvalues[string(a.getName())] = alloc;
  }

  for( int i = 0; i < body.size() ; i++ )
  {
    if( Value * returnval = body[i]->codegen() )
    {
      if( (p.getname() != "main") && (i == body.size() - 1) )
      {
        if( isprocedure )
        { builder->CreateRet(nullptr); }
        else if( !isprocedure )
        { builder->CreateRet(returnval); }
      }
      // Validate the generated code for consistency
      verifyFunction(*thefunc);
    }
    else
    {
      thefunc->eraseFromParent();
      if( p.isbinary() )
      { BinopPrecedence.erase(p.operatorname()); }
      return nullptr;
    }
  }
  return thefunc;
}

// " NUMBER EXPRESSION " CODEGEN
Value * numexpr::codegen()
{ return ConstantInt::get(*context, APInt(32, value, true)); }

// " VARIABLE EXPRESSION "
const string variableexpr::getname() const
{ return name; }
// " VARIABLE EXPRESSION " CODEGEN
Value * variableexpr::codegen()
{
  // Look up the variable name in the symbol table
  Value * v = namedvalues[name];
  if( !v )
  {
    v = module->getNamedGlobal(name);
    if( !v )
    { return LogErrorV("UNDECLARED VARIABLE NAME"); }
  }
  return builder->CreateLoad(v, name.c_str());
}

// " BINARY EXPRESSION " CODEGEN
Value * binexpr::codegen()
{
  // Edge case as the LHS is an identifier
  if( op == '=' )
  {
    variableexpr * lhsvar = static_cast<variableexpr*>( lhs.get() );
    if( !lhsvar )
    { return LogErrorV("EQUAL OPERATOR MUST BE ASSIGNED TO A VARIABLE"); }

    Value * val = rhs->codegen();
    if( !val )
    { return nullptr; }

    if( constants.find(lhsvar->getname()) != constants.end() )
    { return LogErrorV("UNDECLARED CONSTANT NAME"); }

    Value * var = namedvalues[lhsvar->getname()];
    if (!var)
    {
      var = module->getNamedGlobal(lhsvar->getname());
      if( !var )
      { return LogErrorV("UNDECLARED VARIABLE NAME"); }
    }
    builder->CreateStore(val, var);
    module->print(errs(), nullptr);

    return val;
  }


  Value * l = lhs->codegen();
  Value * r = rhs->codegen();
  if( !l || !r )
  { return nullptr; }

  switch (op)
  {
    case '+':
        return builder->CreateAdd(l, r, "addtmp");
    case '-':
        return builder->CreateSub(l, r, "subtmp");
    case '*':
        return builder->CreateMul(l, r, "multmp");
    case '/':
        return builder->CreateSDiv(l, r, "divtmp");
    case '<':
        l = builder->CreateICmpSLT(l, r, "lecmptmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    case tok_lessequal:
        l = builder->CreateICmpSLE(l, r, "lecmptmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    case '>':
        l = builder->CreateICmpSGT(l, r, "gecmptmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    case tok_greaterequal:
        l = builder->CreateICmpSGE(l, r, "gecmptmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    case tok_eq:
        l = builder->CreateICmpEQ(l, r, "lttmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    case tok_notequal:
        l = builder->CreateICmpNE(l, r, "lttmp");
        return builder->CreateIntCast(l, Type::getInt32Ty(*context), true, "booltmp");
    default:
        break;
  }

  Function * func = getfunction( string("binary") + op );
  Value * ops[2] = { l, r };
  return builder->CreateCall(func, ops, "binop");
}

// " FUNCTION CALL " CODEGEN
Value * callexpr::codegen()
{
  // Look up the function name in the global module table
  Function * called = getfunction(caller);
  if( !called )
  { return LogErrorV("UNDECLARED FUNCTION REFRENCE"); }

  // Argument mismatch error
  if( called->arg_size() != arguments.size() )
  { return LogErrorV("INVALID NO. OF ARGUMENTS PASSED"); }

  vector<Value *> argsval;
  for( unsigned i = 0, e = arguments.size(); i != e; ++i )
  {
    if( called->getName() == "readln" )
    {
      Value * val = namedvalues[arguments[i]->getname()];
      if( !val )
      {
        val = module->getNamedGlobal(arguments[i]->getname());
        if( !val )
        { return LogErrorV("UNDECLARED VARIABLE NAME"); }
      }
      builder->CreateLoad(builder->CreateIntToPtr(val, Type::getInt32PtrTy(*context)), "ptr");
      argsval.push_back(builder->CreateIntToPtr(val, Type::getInt32PtrTy(*context)));
      }
      else
      { argsval.push_back(arguments[i]->codegen()); }

      if( !argsval.back() )
      { return nullptr; }
  }

  if( called->getReturnType()->isVoidTy() )
  { return builder->CreateCall(called, argsval); }

  return builder->CreateCall(called, argsval, "calltmp");
}

// " IF EXPRESSION " CODEGEN
Value * ifexpr::codegen()
{
  Value *condv = cond->codegen();
  if( !condv )
  { return nullptr; }

  // Convert the condition into a bool by comparing it in non equality to 0
  condv = builder->CreateICmpNE(condv, ConstantInt::get(*context, APInt(32, 0, true)), "ifcond");

  Function *thefunc = builder->GetInsertBlock()->getParent();
  // Create Basic Blocks for the then and else cases, insert the then block at the end of the function
  BasicBlock *thenbb = BasicBlock::Create(*context, "then", thefunc);
  BasicBlock *elsebb;
  if( iselse )
  { elsebb = BasicBlock::Create(*context, "else"); }
  BasicBlock *mergebb = BasicBlock::Create(*context, "ifcont");
  if( !iselse )
  { builder->CreateCondBr(condv, thenbb, mergebb); }
  builder->CreateCondBr(condv, thenbb, elsebb);

  builder->SetInsertPoint(thenbb);
  Value *thenv;
  for( const auto & th: then )
  {
    thenv = th->codegen();
    if( !thenv )
    { return nullptr; }
  }
  builder->CreateBr(mergebb);
  thenbb = builder->GetInsertBlock();

  if( iselse )
  {
    thefunc->getBasicBlockList().push_back(elsebb);
    builder->SetInsertPoint(elsebb);
  }
  Value *elsev;
  if( iselse )
  {
    elsev = Else->codegen();
    if( !elsev )
    { return nullptr; }
  }

  if( iselse )
  { builder->CreateBr(mergebb); }
  elsebb = builder->GetInsertBlock();

  thefunc->getBasicBlockList().push_back(mergebb);
  builder->SetInsertPoint(mergebb);

  PHINode *node = builder->CreatePHI(Type::getInt32Ty(*context), 2, "iftmp");
  node->addIncoming(thenv, thenbb);
  if (iselse)
  { node->addIncoming(elsev, elsebb); }

  return node;
}

// " FOR EXPRESSION " CODEGEN
Value * forexpr::codegen()
{
  Function *thefunc = builder->GetInsertBlock()->getParent();
  AllocaInst *alloc = createblockalloc(thefunc, name);

  Value *startv = start->codegen();
  if( !startv )
  { return nullptr; }
  builder->CreateStore(startv, alloc);

  BasicBlock *loopbb = BasicBlock::Create(*context, "loop", thefunc);
  builder->CreateBr(loopbb);
  builder->SetInsertPoint(loopbb);

  AllocaInst * oldval = namedvalues[name];
  namedvalues[name] = alloc;
  for( const auto & b: body )
  {
    if( !b->codegen() )
    { return nullptr; }
  }

  Value * stepv = nullptr;
  if( step )
  {
    stepv = step->codegen();
    if( !stepv )
    { return nullptr; }
  }
  else
  { stepv = ConstantInt::get(*context, APInt(32, 1, true)); }

  Value * endcond = end->codegen();
  if( !endcond )
  { return nullptr; }

  Value * currvar = builder->CreateLoad(alloc, name.c_str());
  Value * nextvar;
  if( to )
  { nextvar = builder->CreateAdd(currvar, stepv, "nextvar"); }
  else
  { nextvar = builder->CreateSub(currvar, stepv, "nextvar"); }

  builder->CreateStore(nextvar, alloc);
  endcond = builder->CreateICmpNE(endcond, currvar, "loopcond");
  BasicBlock * afterbb = BasicBlock::Create(*context, "afterloop", thefunc);
  builder->CreateCondBr(endcond, loopbb, afterbb);
  builder->SetInsertPoint(afterbb);

  if( oldval )
  { namedvalues[name] = oldval; }
  else
  { namedvalues.erase(name); }

  return Constant::getNullValue(Type::getInt32Ty(*context));
}

// " UNARY EXPRESSION " CODEGEN
Value * unaryexpr::codegen()
{
  Value *operandv = operand->codegen();
  if( !operandv )
  { return nullptr; }

  Function *func = getfunction(string("unary") + opcode);
  if( !func )
  { return LogErrorV("UNKNOWN UNARY OPERATOR"); }

  return builder->CreateCall(func, operandv, "unop");
}

// " VARIABLES " CODEGEN
Value * varexpr::codegen()
{
  vector<AllocaInst*> oldbindings;

  Function *thefunc = builder->GetInsertBlock()->getParent();

  // Register all the variables and emit their initializer
  for( unsigned i = 0, e = variables.size(); i != e; ++i )
  {
    const string & varname = variables[i].first;
    expression *init = variables[i].second.get();

    Value *initval;
    if( init )
    {
      initval = init->codegen();
      if( !initval )
      { return nullptr; }
    }
    else
    { initval = ConstantInt::get(*context, APInt(32, 0, true)); }

    AllocaInst *alloc = createblockalloc(thefunc, varname);
    builder->CreateStore(initval, alloc);

    oldbindings.push_back(namedvalues[varname]);
    namedvalues[varname] = alloc;
  }

  return thefunc;
}
bool varexpr::makeglobal()
{
  for( const auto & v : variables )
  {
    module->getOrInsertGlobal(v.first, builder->getInt32Ty());
    GlobalVariable *gvar = module->getNamedGlobal(v.first);
    gvar->setLinkage(GlobalValue::ExternalLinkage);
    gvar->setInitializer(ConstantInt::get(*context, APInt(32, 0, true)));
  }
  return true;
}

// " CONSTANTS " CODEGEN
Value * constantexpr::codegen()
{
  vector<AllocaInst*> oldbindings;
  Function *thefunc = builder->GetInsertBlock()->getParent();

  // Register all the variables and emit their initializer
  for( unsigned i = 0, e = variables.size(); i != e; ++i )
  {
    const string & varname = variables[i].first;
    expression *init = variables[i].second.get();

    Value *initval;
    if( init )
    {
      initval = init->codegen();
      if( !initval )
      { return nullptr; }
    }
    else
    { initval = ConstantInt::get(*context, APInt(32, 0, true)); }

    AllocaInst *alloc = createblockalloc(thefunc, varname);
    builder->CreateStore(initval, alloc);

    oldbindings.push_back(namedvalues[varname]);
    namedvalues[varname] = alloc;
  }

  return thefunc;
}
bool constantexpr::makeglobal()
{
  int varno = 0;
  for( const auto & v : variables )
  {
    module->getOrInsertGlobal(v.first, builder->getInt32Ty());
    GlobalVariable *gvar = module->getNamedGlobal(v.first);
    gvar->setLinkage(GlobalValue::ExternalLinkage);

    expression *init = variables[varno].second.get();
    if( init )
    {
      if( auto initval = init->codegen() )
      { gvar->setInitializer(dyn_cast<llvm::ConstantInt>(initval)); }
      else
      { return false; }
    }

    varno++;
  }

  return true;
}
