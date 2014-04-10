#include "binary.h"
#include "options.h"
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include <iostream>

static llvm::tool_output_file *GetOutputStream(const std::string& filename) 
{
    // Open the file.
    std::string error;
    llvm::sys::fs::OpenFlags OpenFlags = llvm::sys::fs::F_None;
    llvm::tool_output_file *FDOut = new llvm::tool_output_file(filename.c_str(), error,
							       OpenFlags);
    if (!error.empty()) 
    {
	std::cerr << error << '\n';
	delete FDOut;
	return 0;
    }
    
    return FDOut;
}


static void CreateObject(llvm::Module *module, const std::string& objname)
{
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());

    module->setTargetTriple(triple.getTriple());

    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget("", triple, error);
    
    if (!target)
    {
	std::cerr << "Error, could not find target: " << error << std::endl;
	return;
    }

    llvm::TargetOptions options;
    llvm::TargetMachine* tm = target->createTargetMachine(triple.getTriple(), "", "", options);

    if (!tm)
    {
	std::cerr << "Error: Could not create targetmachine." << std::endl;
	return;
    }

    llvm::PassManager PM;
    llvm::TargetLibraryInfo *TLI = new llvm::TargetLibraryInfo(triple);
    PM.add(TLI);
    tm->setAsmVerbosityDefault(true);

    std::unique_ptr<llvm::tool_output_file> Out(GetOutputStream(objname));
    if (!Out) 
    {
	std::cerr << "Could not open file ... " << std::endl;
	return;
    }

    llvm::formatted_raw_ostream FOS(Out->os());

    llvm::AnalysisID StartAfterID = 0;
    llvm::AnalysisID StopAfterID = 0;
    if (tm->addPassesToEmitFile(PM, FOS, llvm::LLVMTargetMachine::CGFT_ObjectFile, false,
                                   StartAfterID, StopAfterID)) 
    {
	std::cerr << objname << ": target does not support generation of this"
	       << " file type!\n";
	return;
    }
    PM.run(*module);
    Out->keep();
}


void CreateBinary(llvm::Module *module, const std::string& objname, const std::string& exename)
{
    CreateObject(module, objname);
    std::string cmd = std::string("clang ") + objname + " runtime.o -lm -o " + exename; 
    if (verbosity)
    {
	std::cerr << "Executing final link command: " << cmd << std::endl;
    }
    system(cmd.c_str());
}
