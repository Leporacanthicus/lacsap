#include "binary.h"
#include "builtin.h"
#include "callgraph.h"
#include "constants.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "semantics.h"
#include "source.h"
#include "trace.h"
#include <iostream>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>

#include <llvm/IR/PassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

std::string   libpath;
llvm::Module* theModule;

int      verbosity;
bool     timetrace;
bool     disableMemcpyOpt;
OptLevel optimization = O1;
bool     rangeCheck;
bool     debugInfo;
bool     callGraph;
Model    model = m64;
bool     caseInsensitive = true;
EmitType emitType;
Standard standard = none;

// Command line option definitions.
static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional, llvm::cl::Required,
                                                llvm::cl::desc("<input file>"));

static llvm::cl::opt<int, true> Verbose("v", llvm::cl::desc("Enable verbose output"),
                                        llvm::cl::location(verbosity));

static llvm::cl::opt<OptLevel, true> OptimizationLevel(
    llvm::cl::desc("Choose optimization level:"),
    llvm::cl::values(clEnumVal(O0, "No optimizations"), clEnumVal(O1, "Enable trivial optimizations"),
                     clEnumVal(O2, "Enable more optimizations"),
                     clEnumVal(O3, "Enable extensive optimisations")),
    llvm::cl::location(optimization));

static llvm::cl::opt<EmitType, true> EmitSelection(
    "emit", llvm::cl::desc("Choose output:"),
    llvm::cl::values(clEnumValN(Exe, "exe", "Executable file"), clEnumValN(LlvmIr, "llvm", "LLVM IR file")),
    llvm::cl::location(emitType));

static llvm::cl::opt<bool, true> TimetraceEnable("tt", llvm::cl::desc("Enable timetrace"),
                                                 llvm::cl::location(timetrace));

static llvm::cl::opt<bool, true> DisableMemCpy("no-memcpy",
                                               llvm::cl::desc("Disable use of memcpy for larger structs"),
                                               llvm::cl::location(disableMemcpyOpt));

static llvm::cl::opt<bool, true> RangeCheck("Cr", llvm::cl::desc("Enable range checking"),
                                            llvm::cl::location(rangeCheck));

#if M32_DISABLE == 0
static llvm::cl::opt<Model, true> ModelSetting(llvm::cl::desc("Model:"),
                                               llvm::cl::values(clEnumVal(m32, "32-bit model"),
                                                                clEnumVal(m64, "64-bit model")),
                                               llvm::cl::location(model));
#endif
static llvm::cl::opt<bool, true> DebugInfo("g", llvm::cl::desc("Enable debug info"),
                                           llvm::cl::location(debugInfo));

static llvm::cl::opt<bool, true> CallGraphOpt("callgraph", llvm::cl::desc("Produce callgraph"),
                                              llvm::cl::location(callGraph));

static llvm::cl::opt<Standard, true> StandardOpt("std", llvm::cl::desc("ISO standard"),
                                                 llvm::cl::values(clEnumVal(none, "Allow all language forms"),
                                                                  clEnumVal(iso7185, "ISO-7185 mode"),
                                                                  clEnumVal(iso10206, "ISO-10206 mode")),
                                                 llvm::cl::location(standard));

static void RunOptimisationPasses(llvm::Module& theModule)
{
    llvm::OptimizationLevel opt;
    switch (OptimizationLevel)
    {
    case O0:
	opt = llvm::OptimizationLevel::O0;
	break;
    case O1:
	opt = llvm::OptimizationLevel::O1;
	break;
    case O2:
	opt = llvm::OptimizationLevel::O2;
	break;
    case O3:
	opt = llvm::OptimizationLevel::O3;
	break;
    default:
	std::cerr << "Unknown optimisaton level" << std::endl;
	std::exit(1);
	break;
    }

    if (opt != llvm::OptimizationLevel::O0)
    {
	llvm::PassBuilder pb;

	llvm::LoopAnalysisManager     lam;
	llvm::FunctionAnalysisManager fam;
	llvm::CGSCCAnalysisManager    cgam;
	llvm::ModuleAnalysisManager   mam;

	pb.registerModuleAnalyses(mam);
	pb.registerCGSCCAnalyses(cgam);
	pb.registerFunctionAnalyses(fam);
	pb.registerLoopAnalyses(lam);
	pb.crossRegisterProxies(lam, fam, cgam, mam);

	llvm::ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(opt);
	mpm.run(theModule, mam);
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
    ParserInterface& p = GetParser(source);

    ExprAST* ast = p.Parse(ParserType::Program);
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
	    ast->dump();
	    return 1;
	}
	BackPatch();
    }

#if !NDEBUG
    if (verbosity)
    {
	theModule->dump();
    }
#endif

    RunOptimisationPasses(*theModule);
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
