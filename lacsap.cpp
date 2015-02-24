#include "lexer.h"
#include "parser.h"
#include "binary.h"
#include "constants.h"
#include "semantics.h"
#include "options.h"
#include "trace.h"
#include <iostream>
#include <fstream>
#include <llvm/PassManager.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Support/CommandLine.h>

llvm::FunctionPassManager* fpm;
llvm::PassManager* mpm;
llvm::Module* theModule = new llvm::Module("TheModule", llvm::getGlobalContext());
int verbosity;
bool timetrace;
bool disableMemcpyOpt;
OptLevel optimization;

// Command line option definitions.
static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::Required, 
						llvm::cl::desc("<input file>"));
static llvm::cl::opt<int, true>   Verbose("v", llvm::cl::desc("Enable verbose output"), 
						llvm::cl::location(verbosity));
static llvm::cl::opt<OptLevel, true>    OptimizationLevel(llvm::cl::desc("Choose optimization level:"),
						    llvm::cl::values(
							clEnumVal(O0, "No optimizations"),
							clEnumVal(O1, "Enable trivial optimizations"),
							clEnumVal(O2, "Enable more optimizations"),
							clEnumValEnd),
						    llvm::cl::location(optimization));
static llvm::cl::opt<EmitType>    EmitSelection("emit", llvm::cl::desc("Choose output:"),
						llvm::cl::values(
						    clEnumValN(Exe, "exe", "Executable file"),
						    clEnumValN(LlvmIr, "llvm", "LLVM IR file"),
						    clEnumValEnd));

static llvm::cl::opt<bool, true>  TimetraceEnable("tt", llvm::cl::desc("Enable timetrace"),
						   llvm::cl::location(timetrace));


static llvm::cl::opt<bool, true>  DisableMemCpyGen("no-memcpy",
						   llvm::cl::desc("Disable use of memcpy for large data structs"),
						   llvm::cl::location(disableMemcpyOpt));

void DumpModule(llvm::Module* module)
{
    module->dump(); 
}

bool CodeGen(std::vector<ExprAST*> ast)
{
    TIME_TRACE();
    for(auto a : ast)
    {
	llvm::Value* v = a->CodeGen(); 
	if (!v)
	{
	    std::cerr << "Sorry, something went wrong here..." << std::endl;
	    a->dump(std::cerr);
	    return false;
	}
    }
    return true;
}

void OptimizerInit()
{
    fpm = new llvm::FunctionPassManager(theModule);
    mpm = new llvm::PassManager();

    llvm::InitializeNativeTarget();

    if (OptimizationLevel > O0)
    {
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
        // Memory copying opts. 
	fpm->add(llvm::createMemCpyOptPass());
	// Merge constants.
	mpm->add(llvm::createConstantMergePass());
	// dead code removal:
	fpm->add(llvm::createDeadCodeEliminationPass());
	if (OptimizationLevel > O1)
	{
	    // Inline functions. 
	    mpm->add(llvm::createFunctionInliningPass());
	    // Thread jumps.
	    fpm->add(llvm::createJumpThreadingPass());
	    // Loop strength reduce.
	    fpm->add(llvm::createLoopStrengthReducePass());
	}
    }
}

static int Compile(const std::string& filename)
{
    TIME_TRACE();
    std::vector<ExprAST*> ast;
    Lexer                 lex(filename);
    if (!lex.Good())
    {
	return 1;
    }
    Parser p(lex);

    OptimizerInit();

    ast = p.Parse();
    int e = p.GetErrors();
    if (e > 0)
    {
	std::cerr << "Errors in parsing: " << e << ". Exiting..." << std::endl;
	return 1;
    }

    Semantics sema;
    sema.Analyse(ast);

    e = sema.GetErrors();
    if (e > 0)
    {
	std::cerr << "Errors in analysis: " << e << ". Exiting..." << std::endl;
	return 1;
    }

    if (!CodeGen(ast))
    {
	std::cerr << "Code generation failed..." << std::endl;
	return 1;
    }
    mpm->run(*theModule);
    if (verbosity)
    {
	DumpModule(theModule);
    }
    CreateBinary(theModule, filename, EmitSelection);
    return 0;
}

int main(int argc, char** argv)
{
    llvm::cl::ParseCommandLineOptions(argc, argv);
    int res = Compile(InputFilename);
    return res;
}
