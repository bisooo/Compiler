#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Parser.hpp"


int main (int argc, char *argv[])
{
  BinopPrecedence['='] = 2;
  BinopPrecedence['>'] = 10;
  BinopPrecedence['<'] = 10;
  BinopPrecedence[tok_greaterequal] = 10;
  BinopPrecedence[tok_lessequal] = 10;
  BinopPrecedence[tok_notequal] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  // PROGRAM
  getNextToken();
  // PROGRAM NAME
  getNextToken();
  string programName = idname;
  // ;
  getNextToken();

  InitializeModuleAndPassManager();
  // Create writeln and readln functions
  readlnfunc();
  writelnfunc();

  MainLoop();

  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();

  auto TargetTriple = sys::getDefaultTargetTriple();
  module->setTargetTriple(TargetTriple);

  string Error;
  auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

  // Print an error and exit if we couldn't find the requested target
  if( !Target )
  {
      errs() << "errors: " << Error;
      return 1;
  }

  // generic cpu without any additional features
  auto CPU = "generic";
  auto Features = "";

  Function * mainFunction = getfunction("main");
  builder->CreateRet(builder->getInt32(0));
  verifyFunction(*mainFunction);

  TargetOptions opt;
  auto RM = Optional<Reloc::Model>();
  auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

  module->setDataLayout(TheTargetMachine->createDataLayout());

  auto Filename = "ye.o";
  error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if( EC )
  {
      errs() << "Could not open file: " << EC.message();
      return 1;
  }

  legacy::PassManager pass;
  auto FileType = CGFT_ObjectFile;

  if( TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType) )
  {
      errs() << "TheTargetMachine can't emit a file of this type";
      return 1;
  }


  pass.add(createPromoteMemoryToRegisterPass());
  pass.add(createInstructionCombiningPass());
  pass.add(createReassociatePass());
  pass.add(createJumpThreadingPass());
  pass.add(createCFGSimplificationPass());
  module->print(errs(), nullptr);

  pass.run(*module);
  dest.flush();

  return 0;
}
