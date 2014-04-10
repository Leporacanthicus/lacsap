#include "lexer.h"
#include "parser.h"
#include "binary.h"
#include "constants.h"
#include "options.h"
#include <iostream>
#include <fstream>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/CommandLine.h>


static std::string ErrStr;
llvm::FunctionPassManager* fpm;
llvm::Module* theModule = new llvm::Module("TheModule", llvm::getGlobalContext());
int verbosity;

// Command line option definitions.
static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::Required, 
					 llvm::cl::desc("<input file>"));
static llvm::cl::opt<int, true>         Verbose("v", llvm::cl::desc("Enable verbose output"), 
						llvm::cl::location(verbosity));

void DumpModule(llvm::Module* module)
{
    module->dump();
}

llvm::Module* CodeGen(std::vector<ExprAST*> ast)
{
    for(auto a : ast)
    {
	llvm::Value* v = a->CodeGen(); 
	if (!v)
	{
	    std::cerr << "Sorry, something went wrong here..." << std::endl;
	    a->Dump(std::cerr);
	    return 0;
	}
    }
    return theModule;
}

void OptimizerInit()
{
    fpm = new llvm::FunctionPassManager(theModule);

    llvm::InitializeNativeTarget();

    // Promote allocas to registers.
    fpm->add(llvm::createPromoteMemoryToRegisterPass());
    // Provide basic AliasAnalysis support for GVN.
    fpm->add(llvm::createBasicAliasAnalysisPass());
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    fpm->add(llvm::createInstructionCombiningPass());
    // Reassociate expressions.
    fpm->add(llvm::createReassociatePass());
    // Eliminate Common SubExpressions.
    fpm->add(llvm::createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    fpm->add(llvm::createCFGSimplificationPass());
}

std::string replace_ext(const std::string &origName, 
			const std::string& expectedExt, 
			const std::string& newExt)
{
    if (origName.substr(origName.size() - expectedExt.size()) != expectedExt)
    {
	std::cerr << "Could not find extension..." << std::endl;
	exit(1);
	return "";
    }
    return origName.substr(0, origName.size() - expectedExt.size()) + newExt;
}

static int Compile(const std::string& filename)
{
    std::vector<ExprAST*> ast;
    Lexer                 l(filename);
    if (!l.Good())
    {
	return 1;
    }
    Parser                p(l);

    OptimizerInit();

    ast = p.Parse();
    int e = p.GetErrors();
    if (e > 0)
    {
	std::cerr << "Errors in parsing: " << e << ". Exiting..." << std::endl;
	return 1;
    }
    llvm::Module* module = CodeGen(ast);
    if (!module)
    {
	std::cerr << "Code generation failed..." << std::endl;
	return 1;
    }
    if (verbosity)
    {
	DumpModule(module);
    }
    CreateBinary(module, replace_ext(filename, ".pas", ".o"), replace_ext(filename, ".pas", ""));
    return 0;
}

int main(int argc, char** argv)
{
    llvm::cl::ParseCommandLineOptions(argc, argv);
    int res = Compile(InputFilename);
    return res;
}
