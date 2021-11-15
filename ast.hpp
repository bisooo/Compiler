#ifndef PJPPROJECT_AST_HPP
#define PJPPROJECT_AST_HPP

#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Error.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <memory>

using namespace llvm;
using namespace std;


class funcproto;

// LLVM " CONTEXT "
extern unique_ptr< LLVMContext > context;
// LLVM " BUILDER "
extern unique_ptr< IRBuilder<> > builder;
// LLVM " MODULE " - functions & global variables
extern unique_ptr< Module > module;

// " SCOPED " variables values
extern map< string, AllocaInst * > namedvalues;

// " FUNCTIONs " and their blueprints
extern map< string, unique_ptr< funcproto > > functions;

extern unique_ptr< legacy::FunctionPassManager > fpm;
extern ExitOnError exitonerr;
extern set<string> constants;

Function * getfunction( string name );
AllocaInst * createblockalloc( Function * func , StringRef var );

void writelnfunc();
void readlnfunc();


// " PROTOTYPE " class to encapsulate the blueprint of a function ( name, arguments, ... )
class funcproto
{
private:
  string name;
  vector< string > arguments;
  bool isoperator;
  unsigned precedence;

public:
  bool isprocedure;
  // " CONSTRUCTOR "
  funcproto( const string & n, vector<string> a, bool op = false, unsigned p = 0, bool isp = false ) :
  name(n), arguments(move(a)), isoperator(op), precedence(p), isprocedure(isp) {}

  // " CODEGEN "
  Function * codegen();

  // " GETTERS "
  const string & getname() const;
  bool isunary() const;
  bool isbinary() const;
  char operatorname() const;
  unsigned getprecedence() const;
};

class expression;

// " FUNCTION " represents a whole function definition which is made up of the blueprint (PROTOTYPE) & its body
class funct
{
private:
  unique_ptr< funcproto > proto;
  vector< unique_ptr< expression > > body;

public:
  bool isprocedure;
  // " CONSTRUCTOR "
  funct( unique_ptr<funcproto> p, vector<unique_ptr<expression>> b, bool isp = false ) :
  proto(move(p)), body(move(b)), isprocedure(isp) {}
  // " CODEGEN"
  Function * codegen();
};

// " BASE " class for all expressions
class expression
{
public:
  virtual ~expression() = default;
  virtual Value * codegen() = 0;
  virtual bool makeglobal();
  virtual const string getname() const;
};

// " NUMBER " expression ( 21 )
class numexpr : public expression
{
private:
  int value;

public:
  numexpr( int v ) : value( v ) {}
  Value * codegen() override;
};

// " VARIABLE " expression ( "ye" )
class variableexpr : public expression
{
private:
  string name;

public:
  variableexpr( string n ) : name(n) {}
  Value * codegen() override;
  const string getname() const override;
};

// " BINARY OPERATOR " expression ( & )
class binexpr : public expression
{
private:
  char op;
  unique_ptr< expression > lhs;
  unique_ptr< expression > rhs;

public:
  binexpr( char o, unique_ptr<expression> l, unique_ptr<expression> r ) : op(o), lhs(move(l)), rhs(move(r)) {}
  Value * codegen() override;
};

// " CALL FUNCTION " expression ( func(a,b) )
class callexpr : public expression
{
private:
  string caller;
  vector < unique_ptr< expression > > arguments;

public:
  callexpr( string & c, vector<unique_ptr<expression>> a ) : caller(c), arguments(move(a)) {}
  Value * codegen() override;
};

// " IF / ELSE " expression ( if / then / else )
class ifexpr : public expression
{
private:
  unique_ptr< expression > cond;
  vector < unique_ptr < expression > > then;
  unique_ptr< expression > Else;

public:
  bool iselse;
  // " CONSTRUCTOR " ( if / then / else )
  ifexpr( unique_ptr<expression> c, vector<unique_ptr<expression>> t, unique_ptr<expression> e, bool is ) :
  cond(move(c)), then(move(t)), Else(move(e)), iselse(is) {}
  // " CONSTRUCTOR " ( if / then )
  ifexpr( unique_ptr<expression> c, vector<unique_ptr<expression>> t ) :
  cond(move(c)), then(move(t)) {}
  Value * codegen() override;
};

// " FOR " expression ( for / in )
class forexpr : public expression
{
private:
  bool to;
  string name;
  unique_ptr< expression > start;
  unique_ptr< expression > step;
  unique_ptr< expression > end;
  vector< unique_ptr< expression > > body;

public:
  forexpr( const string & n, unique_ptr<expression> strt, unique_ptr<expression> stp,
           unique_ptr<expression> e, vector<unique_ptr<expression>> b, bool t ) :
  name(n), start(move(strt)), step(move(stp)), end(move(e)), body(move(b)), to(t) {}
  Value * codegen() override;
};

// " UNARY OPERATOR " expression ( ++ )
class unaryexpr : public expression
{
private:
  char opcode;
  unique_ptr< expression > operand;

public:
  unaryexpr( char opc, unique_ptr<expression> oper ) : opcode(opc), operand(move(oper)) {}
  Value * codegen() override;
};

// " VARIABLE " expression ( var / in )
class varexpr : public expression
{
private:
  vector< pair< string, unique_ptr<expression> > > variables;

public:
  varexpr( vector<pair<string, unique_ptr<expression>>> v ) : variables(move(v)) {}
  Value * codegen() override;
  bool makeglobal() override;
};

// " CONST " expression ( const )
class constantexpr : public expression
{
private:
  vector< pair< string, unique_ptr<expression> > > variables;

public:
  constantexpr( vector<pair<string, unique_ptr<expression>>> v ) : variables(move(v)) {}
  Value * codegen() override;
  bool makeglobal() override;
};

#endif //PJPPROJECT_AST_HPP
