#ifndef PJPPROJECT_PARSER_HPP
#define PJPPROJECT_PARSER_HPP


#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "Lexer.hpp"
#include "ast.hpp"


using namespace llvm;
using namespace std;

class Parser
{
private:
  // value of the current token being parsed
  int currenttok;
  // LLVM " CONTEXT "
  LLVMContext MilaContext;
  // LLVM " BUILDER "
  IRBuilder<> MilaBuilder;
  // LLVM " MODULE "
  Module MilaModule;


public:

  Parser();
  ~Parser() = default;

  // " PARSE "
  bool Parse();
  // " GENERATE "
  const Module& Generate();
};

extern map<char, int> BinopPrecedence;
extern int currenttok;

// Returns the next token in the stdin into the ( current token ) variable
int getNextToken();
// Returns the current token's precedence
int GetTokPrecedence();

// Error logging (stderr)
unique_ptr<expression> LogError(const char *str);
unique_ptr<funcproto> LogErrorP(const char *str);

unique_ptr<expression> ParseExpression();
unique_ptr<expression> ParseNumberExpr();
unique_ptr<expression> ParseParentExpr();
unique_ptr<expression> ParseIdentifierExpr();
unique_ptr<expression> ParsePrimary();
unique_ptr<expression> ParseBinOpRHS(int ExprPrec, unique_ptr<expression> LHS);
unique_ptr<funcproto> ParsePrototype();
unique_ptr<funct> ParseDefinition();
unique_ptr<funct> ParseTopLevelExpr();
unique_ptr<funcproto> ParseExternal();
unique_ptr<expression> ParseIfExpr();
unique_ptr<expression> ParseForExpr();
unique_ptr<expression> ParseUnary();
unique_ptr<expression> ParseVarExpr();
unique_ptr<expression> ParseConstExpr();

void InitializeModuleAndPassManager();

void HandleDefinition();
void HandleTopLevelExpression();

void writelnfunc();
void readlnfunc();

void HandleVarGlobal();
void HandleConstVal();
void HandleForward();

void HandleSequenceVars( vector<pair<string, unique_ptr<expression>>> & varnames );
void HandleListVars( vector<pair<string, unique_ptr<expression>>> & varnames );

void MainLoop();

#endif //PJPPROJECT_PARSER_HPP
