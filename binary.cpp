#include "binary.h"
#include "options.h"
#include "trace.h"
#include "expr.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include <llvm/CodeGen/CommandFlags.def>
#pragma clang diagnostic pop
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <iostream>
#include <system_error>

static llvm::ToolOutputFile *GetOutputStream(const std::string& filename)
{
    // Open the file.
    std::error_code error;
    llvm::sys::fs::OpenFlags OpenFlags = llvm::sys::fs::F_None;
    llvm::ToolOutputFile *FDOut = new llvm::ToolOutputFile(filename, error, OpenFlags);
    if (error)
    {
	std::cerr << error << '\n';
	delete FDOut;
	return 0;
    }

    return FDOut;
}

static void CreateObject(llvm::Module *module, const std::string& objname)
{
    TIME_TRACE();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    std::string error;
    llvm::Triple triple = llvm::Triple(module->getTargetTriple());
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);

    if (!target)
    {
	std::cerr << "Error, could not find target: " << error << std::endl;
	return;
    }

    if (MCPU == "native")
    {
	MCPU = sys::getHostCPUName();
    }

    std::string FeaturesStr;
    if (MAttrs.size())
    {
	llvm::SubtargetFeatures Features;
	for (unsigned i = 0; i != MAttrs.size(); ++i)
	{
	    Features.AddFeature(MAttrs[i]);
	}
	FeaturesStr = Features.getString();
    }

    llvm::TargetOptions options;
    std::unique_ptr<llvm::TargetMachine> tm
	(target->createTargetMachine(triple.getTriple(), MCPU,
				     FeaturesStr, options, llvm::Reloc::Static));

    if (!tm)
    {
	std::cerr << "Error: Could not create targetmachine." << std::endl;
	return;
    }

    llvm::legacy::PassManager PM;
    llvm::TargetLibraryInfoWrapperPass *TLI =
	new llvm::TargetLibraryInfoWrapperPass(triple);
    PM.add(TLI);

    std::unique_ptr<llvm::ToolOutputFile> Out(GetOutputStream(objname));
    if (!Out)
    {
	std::cerr << "Could not open file ... " << std::endl;
	return;
    }

    llvm::raw_pwrite_stream *OS = &Out->os();

    if (tm->addPassesToEmitFile(PM, *OS, llvm::LLVMTargetMachine::CGFT_ObjectFile, false))
    {
	std::cerr << objname << ": target does not support generation of this"
	    " file type!\n";
	return;
    }
    PM.run(*module);
    Out->keep();
}

std::string replace_ext(const std::string &origName,
			const std::string& expectedExt,
			const std::string& newExt)
{
    if (origName.substr(origName.size() - expectedExt.size()) != expectedExt)
    {
	std::cerr << "Could not find extension..." << std::endl;
	exit(1);
    }
    return origName.substr(0, origName.size() - expectedExt.size()) + newExt;
}

bool CreateBinary(llvm::Module *module, const std::string& filename, EmitType emit)
{
    TIME_TRACE();
    if (emit == Exe)
    {
	std::string objname = replace_ext(filename, ".pas", ".o");
	std::string exename = replace_ext(filename, ".pas", "");
	std::string modelStr;

// Order matters here: clang, being gcc-compatible, will have __GNUC__ defined.
#ifdef __clang__
	std::string compiler = "clang";
#elif defined(__GNUC__)
	std::string compiler = "gcc";
#endif
	if (model == m32)
	{
	    modelStr = "-m32";
	}

	CreateObject(module, objname);
	std::string verboseflags;
	if (verbosity)
	{
	    verboseflags = " -v";
	}
	std::string debugFlag;
	if (debugInfo)
	{
	    debugFlag = " -g";
	}
	std::string cmd = compiler + " " + modelStr + verboseflags + " " + objname +
	    " -L\"" + libpath + "\" -lruntime" + modelStr + debugFlag + " -lm -o " + exename;
	if (verbosity)
	{
	    std::cerr << "Executing final link command: " << cmd << std::endl;
	}
	int res = system(cmd.c_str());
	if (res != 0)
	{
	    std::cerr << "Error: " << res << std::endl;
	    return false;
	}
	return true;
    }
    assert(emit == LlvmIr && "Expect LLVM IR here..");

    std::string irName = replace_ext(filename, ".pas", ".ll");
    std::unique_ptr<llvm::ToolOutputFile> Out(GetOutputStream(irName));
    llvm::formatted_raw_ostream FOS(Out->os());
    module->print(FOS, 0);
    Out->keep();
    return true;
}

llvm::Module* CreateModule()
{
    llvm::InitializeNativeTarget();

    llvm::Module* module = new llvm::Module("TheModule", theContext);

    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    if (model == m32)
    {
	triple = triple.get32BitArchVariant();
    }
    else
    {
	triple = triple.get64BitArchVariant();
    }
    module->setTargetTriple(triple.getTriple());
    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple.getTriple(), error);
    if (!target)
    {
	std::cerr << "Error, could not find target: " << error << std::endl;
	return 0;
    }

    std::string FeaturesStr;
    if (MAttrs.size())
    {
	llvm::SubtargetFeatures Features;
	for (unsigned i = 0; i != MAttrs.size(); ++i)
	{
	    Features.AddFeature(MAttrs[i]);
	}
	FeaturesStr = Features.getString();
    }

    llvm::TargetOptions options;
    std::unique_ptr<llvm::TargetMachine> tm(target->createTargetMachine(triple.getTriple(), MCPU,
									FeaturesStr, options, llvm::Reloc::Static));
    assert(tm && "Could not create TargetMachine");
    const llvm::DataLayout dl = tm->createDataLayout();
    module->setDataLayout(dl);
    return module;
}
