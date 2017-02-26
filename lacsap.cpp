#include "source.h"
#include "lexer.h"
#include "parser.h"
#include "binary.h"
#include "constants.h"
#include "semantics.h"
#include "options.h"
#include "trace.h"
#include "builtin.h"
#include "callgraph.h"
#include <iostream>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Scalar/GVN.h>

llvm::legacy::PassManager* mpm;
llvm::Module* theModule;
std::string libpath;

int      verbosity;
bool     timetrace;
bool     disableMemcpyOpt;
OptLevel optimization;
bool     rangeCheck;
bool     debugInfo;
bool     callGraph;
Model    model = m64;
bool     caseInsensitive = true;
EmitType emitType;
Standard standard = none;

// Command line option definitions.
static llvm::cl::opt<std::string>    InputFilename(llvm::cl::Positional, llvm::cl::Required, 
						llvm::cl::desc("<input file>"));

static llvm::cl::opt<int, true>      Verbose("v", llvm::cl::desc("Enable verbose output"), 
					  llvm::cl::location(verbosity));

static llvm::cl::opt<OptLevel, true> OptimizationLevel(llvm::cl::desc("Choose optimization level:"),
						    llvm::cl::values(
							clEnumVal(O0, "No optimizations"),
							clEnumVal(O1, "Enable trivial optimizations"),
							clEnumVal(O2, "Enable more optimizations")),
						    llvm::cl::location(optimization));

static llvm::cl::opt<EmitType,true>       EmitSelection("emit", llvm::cl::desc("Choose output:"),
						   llvm::cl::values(
						       clEnumValN(Exe, "exe", "Executable file"),
						       clEnumValN(LlvmIr, "llvm", "LLVM IR file")),
						   llvm::cl::location(emitType));

static llvm::cl::opt<bool, true>     TimetraceEnable("tt", llvm::cl::desc("Enable timetrace"),
						     llvm::cl::location(timetrace));

static llvm::cl::opt<bool, true>     DisableMemCpy("no-memcpy",
						   llvm::cl::desc("Disable use of memcpy for larger structs"),
						   llvm::cl::location(disableMemcpyOpt));

static llvm::cl::opt<bool, true>     RangeCheck("Cr",
						llvm::cl::desc("Enable range checking"),
						llvm::cl::location(rangeCheck));

#if M32_DISABLE==0
static llvm::cl::opt<Model, true> ModelSetting(llvm::cl::desc("Model:"),
					       llvm::cl::values(
								clEnumVal(m32, "32-bit model"),
								clEnumVal(m64, "64-bit model")),
					       llvm::cl::location(model));
#endif
static llvm::cl::opt<bool, true>     DebugInfo("g",
					       llvm::cl::desc("Enable debug info"),
					       llvm::cl::location(debugInfo));

static llvm::cl::opt<bool, true>     CallGraphOpt("callgraph",
						  llvm::cl::desc("Produce callgraph"),
						  llvm::cl::location(callGraph));

static llvm::cl::opt<Standard, true>     StandardOpt("std",
						     llvm::cl::desc("ISO standard"),
						     llvm::cl::values(
							 clEnumVal(none, "Allow all language forms"),
							 clEnumVal(iso7185, "ISO-7185 mode"),
							 clEnumVal(iso10206, "ISO-10206 mode")),
						     llvm::cl::location(standard));


void OptimizerInit()
{
    mpm = new llvm::legacy::PassManager();

    if (OptimizationLevel > O0)
    {
	// Promote allocas to registers.
	mpm->add(llvm::createPromoteMemoryToRegisterPass());
	// Provide basic AliasAnalysis support for GVN.
//	mpm->add(llvm::createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	mpm->add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	mpm->add(llvm::createReassociatePass());
	// Eliminate Common SubExpressions.
	mpm->add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	mpm->add(llvm::createCFGSimplificationPass());
        // Memory copying opts. 
	mpm->add(llvm::createMemCpyOptPass());
	// Merge constants.
	mpm->add(llvm::createConstantMergePass());
	// dead code removal:
	mpm->add(llvm::createDeadCodeEliminationPass());
	if (OptimizationLevel > O1)
	{
	    // Inline functions. 
	    mpm->add(llvm::createFunctionInliningPass());
	    // Thread jumps.
	    mpm->add(llvm::createJumpThreadingPass());
	    // Loop strength reduce.
	    mpm->add(llvm::createLoopStrengthReducePass());
	}
    }
}

static int Compile(const std::string& fileName)
{
    TIME_TRACE();
    theModule = CreateModule();
    Builtin::InitBuiltins();
    FileSource source(fileName);
    if (!source)
    {
	std::cerr << "Could not open " << fileName << std::endl;
	return 1;
    }
    Parser p(source);

    OptimizerInit();

    ExprAST* ast = p.Parse(Parser::Program);
    if (int e = p.GetErrors())
    {
	std::cerr << "Errors in parsing: " << e << ".\nExiting..." << std::endl;
	return 1;
    }

    BuildClosures(ast);

    Semantics sema;
    sema.Analyse(ast);

    if (int e = sema.GetErrors())
    {
	std::cerr << "Errors in analysis: " << e << ".\nExiting..." << std::endl;
	return 1;
    }

    if (callGraph)
    {
	CallGraphPrinter p;
	CallGraph(ast, p);
    }

    {
	TIME_TRACE();
	if (!ast->CodeGen())
	{
	    std::cerr << "Sorry, something went wrong here..." << std::endl;
	    ast->dump(std::cerr);
	    return 1;
	}
	BackPatch();
    }

    if (verbosity)
    {
	theModule->dump();
    }
    mpm->run(*theModule);
    if (!CreateBinary(theModule, fileName, EmitSelection))
    {
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    libpath = GetPath(argv[0]);
    llvm::cl::ParseCommandLineOptions(argc, argv);
    int res = Compile(InputFilename);
    return res;
}
