#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

std::string compilers[] = { "../lacsap", "fpc -Mdelphi" };
std::string compiler = compilers[0];

double totalTime;

// TODO: Move this to a "utility" library?
std::string replace_ext(const std::string& origName, const std::string& expectedExt,
                        const std::string& newExt)
{
    if (origName.substr(origName.size() - expectedExt.size()) != expectedExt)
    {
	assert(0 && "Could not find extension...");
    }
    return origName.substr(0, origName.size() - expectedExt.size()) + newExt;
}

int RunCmd(const std::string& cmd)
{
    std::cout << "Executing: " << std::left << std::setw(70) << cmd;
    std::cout.flush();
    auto   start = std::chrono::steady_clock::now();
    int    res = system(cmd.c_str());
    auto   end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
    std::cout << " Time: " << std::right << std::setw(6) << std::setprecision(3) << std::fixed << elapsed
              << std::endl;
    totalTime += elapsed;
    return res;
}

bool Diff(const std::string& args)
{
    return !RunCmd("diff " + args);
}

bool Check(const std::string& errFile, const std::string& tplFile)
{
    std::ifstream tp(tplFile);
    std::string   tpStr;
    bool          result = true;
    while (getline(tp, tpStr))
    {
	std::ifstream err(errFile);
	std::string   eStr;
	bool          found = false;
	while (getline(err, eStr))
	{
	    if (eStr == tpStr)
	    {
		found = true;
		break;
	    }
	}
	result &= found;
	if (!found)
	{
	    std::cout << "Failed to find " << tpStr << std::endl;
	}
    }
    return result;
}

class TestCase
{
public:
    TestCase(const std::string& nm, const std::string& src, const std::string& arg);
    // Compile, Run and Result functions return false on "failure", true on "good"
    virtual void        Clean();
    virtual bool        Compile(const std::string& options);
    virtual bool        Run();
    virtual bool        Result();
    virtual std::string Dir() { return "Basic"; }
    std::string         Name() const;
    virtual ~TestCase() {}

protected:
    std::string name;
    std::string source;
    std::string args;
};

TestCase::TestCase(const std::string& nm, const std::string& src, const std::string& arg)
    : name(nm), source(src), args(arg)
{
}

void TestCase::Clean()
{
    std::string resname = replace_ext(source, ".pas", ".res");
    remove(resname.c_str());
    std::string exename = replace_ext(source, ".pas", "");
    remove(exename.c_str());
    std::string objname = replace_ext(source, ".pas", ".o");
    remove(objname.c_str());
}

bool TestCase::Compile(const std::string& options)
{
    if (RunCmd(compiler + " " + options + " " + Dir() + "/" + source) == 0)
    {
	return true;
    }
    return false;
}

bool TestCase::Run()
{
    std::string exename = replace_ext(source, ".pas", "");
    std::string resname = replace_ext(source, ".pas", ".res");
    if (RunCmd("cd " + Dir() + "; ./" + exename + " " + args + " > " + resname))
    {
	return false;
    }
    return true;
}

bool TestCase::Result()
{
    std::string resname = Dir() + "/" + replace_ext(source, ".pas", ".res");
    std::string tplname = "expected/" + Dir() + "/" + replace_ext(source, ".pas", ".tpl");
    return Diff(resname + " " + tplname);
}

std::string TestCase::Name() const
{
    return name;
}

class FileTestCase : public TestCase
{
public:
    FileTestCase(const std::string& nm, const std::string& src, const std::string& arg);
    virtual bool        Result();
    virtual std::string Dir() { return "File"; }

private:
    std::string diffArgs;
};

FileTestCase::FileTestCase(const std::string& nm, const std::string& src, const std::string& arg)
    : TestCase(nm, src, ""), diffArgs(arg)
{
}

bool FileTestCase::Result()
{
    return Diff(diffArgs);
}

/* Class that confirms the compile-time is "fast enough" */
class TimeTestCase : public TestCase
{
public:
    TimeTestCase(const std::string& nm, const std::string& src, const std::string& arg);
    virtual bool        Result();
    virtual bool        Compile(const std::string& options);
    virtual std::string Dir() { return "Time"; }

private:
    long                                               maxTime; // In milliseconds.
    std::chrono::time_point<std::chrono::steady_clock> start, end;
};

TimeTestCase::TimeTestCase(const std::string& nm, const std::string& src, const std::string& arg)
    : TestCase(nm, src, ""), maxTime(std::stol(arg))
{
}

bool TimeTestCase::Compile(const std::string& options)
{
    start = std::chrono::steady_clock::now();

    bool res = TestCase::Compile(options);

    end = std::chrono::steady_clock::now();
    return res;
}

bool TimeTestCase::Result()
{
    long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (elapsed > maxTime)
    {
	std::cerr << "Took too long to compile  " << source << " " << std::fixed << std::setprecision(3)
	          << elapsed << " ms" << std::endl;
	return false;
    }
    return true;
}

// Class to test compile detection of errors.
class CompileTimeError : public TestCase
{
public:
    CompileTimeError(const std::string& nm, const std::string& src, const std::string& arg);
    bool                Compile(const std::string& options);
    bool                Result();
    bool                Run();
    virtual std::string Dir() { return "CompErr"; }
};

CompileTimeError::CompileTimeError(const std::string& nm, const std::string& src, const std::string& arg)
    : TestCase(nm, src, arg)
{
}

bool CompileTimeError::Compile(const std::string& options)
{
    std::string errname = Dir() + "/" + replace_ext(source, ".pas", ".err");
    bool        res = TestCase::Compile(options + " 2> " + errname);
    return !res;
}

bool CompileTimeError::Run()
{
    // We don't run, just say "it's good".
    return true;
}

bool CompileTimeError::Result()
{
    std::string errname = Dir() + "/" + replace_ext(source, ".pas", ".err");
    std::string tplname = "expected/" + Dir() + "/" + replace_ext(source, ".pas", ".tpl");
    return Check(errname, tplname);
}

TestCase* TestCaseFactory(const std::string& type, const std::string& name, const std::string& source,
                          const std::string& args)
{
    if (type == "File")
    {
	return new FileTestCase(name, source, args);
    }

    if (type == "Time")
    {
	return new TimeTestCase(name, source, args);
    }

    if (type == "CompErr")
    {
	return new CompileTimeError(name, source, args);
    }

    assert(type == "Basic");
    return new TestCase(name, source, args);
}

class TestResult
{
public:
    struct Failure
    {
	std::string name;
	std::string stage;
	Failure(const std::string& nm, const std::string& st) : name(nm), stage(st) {}
    };
    TestResult() : cases(0), pass(0), fail(0) {}

public:
    void RegisterFail(const TestCase* tc, const std::string& stage)
    {
	failedTests.push_back(Failure(tc->Name(), stage));
	failStageMap[stage]++;
	fail++;
    }
    void RegisterCase(const TestCase* /* tc */) { cases++; }

    void RegisterPass(const TestCase* /* tc */) { pass++; }

    bool AnyFail() { return fail != 0; }

    void Report()
    {
	std::cout << "Cases:  " << std::setw(7) << cases << std::endl;
	std::cout << "Pass:   " << std::setw(7) << pass << std::endl;
	std::cout << "Fail:   " << std::setw(7) << fail << std::endl;
	std::cout << "Time:   " << std::setw(7) << totalTime << std::endl;

	for (auto f : failStageMap)
	{
	    std::cout << f.first << " fail: " << std::setw(5) << f.second << std::endl;
	}

	bool b = true;
	for (auto t : failedTests)
	{
	    if (b)
	    {
		std::cout << "The following tests failed:" << std::endl;
	    }
	    b = false;
	    std::cout << std::setw(20) << t.name << " " << t.stage << std::endl;
	}
	totalTime = 0;
    }

private:
    int                        cases;
    int                        pass;
    int                        fail;
    std::map<std::string, int> failStageMap;
    std::vector<Failure>       failedTests;
};

enum TestFlags
{
    LACSAP_ONLY = 1 << 0,
};

struct TestEntry
{
    int         flags;
    const char* type;
    const char* name;
    const char* source;
    const char* args;
};

TestEntry testCaseList[] = {
    { 0, "Basic", "Math", "mathtest.pas", "" },
    // Results differ due to different random number generator
    { LACSAP_ONLY, "Basic", "HungryMouse", "hungrymouse.pas", " < hungrymouse.txt" },
    { 0, "Basic", "Types", "type.pas", "" },
    { 0, "Basic", "WC", "wc.pas", "" },
    { 0, "Basic", "Histogram", "hist.pas", " < hist.pas" },
    // Case-statement using otherwise not available in FPC.
    // Alternative implementation in CaseCompat test.
    { LACSAP_ONLY, "Basic", "Case", "case.pas", "" },
    { 0, "Basic", "Case 2", "case2.pas", " < case2.in" },
    { 0, "Basic", "CaseCompat", "casecompat.pas", "" },
    { 0, "Basic", "TestSet", "testset.pas", "" },
    { 0, "Basic", "TestSet 2", "testset2.pas", "" },
    { LACSAP_ONLY, "Basic", "TestSet 3", "testset3.pas", "" },
    { 0, "Basic", "SetTest", "set_test.pas", "" },
    { 0, "Basic", "Record Pass", "recpass.pas", "" },
    // Random numbers are diferent
    { LACSAP_ONLY, "Basic", "Random Number", "randtest.pas", "" },
    { 0, "Basic", "Fact Bignum", "fact-bignum.pas", "" },
    { 0, "Basic", "Nested Funcs", "nestfunc.pas", "" },
    { 0, "Basic", "Nested Funcs2", "nestfunc2.pas", "" },
    { 0, "Basic", "Recursion", "recursion.pas", " < recursion.txt" },
    { 0, "Basic", "Test 04", "test04.pas", "" },
    { 0, "Basic", "Test 07", "test07.pas", "" },
    { 0, "Basic", "Test 08", "test08.pas", "" },
    { 0, "Basic", "Test 11", "test11.pas", "" },
    { 0, "Basic", "Test 12", "test12.pas", "" },
    { 0, "Basic", "Test 13", "test13.pas", "" },
    { 0, "Basic", "Test 14", "test14.pas", "" },
    { 0, "Basic", "Test 16", "test16.pas", "" },
    { 0, "Basic", "Test 17", "test17.pas", "" },
    { 0, "Basic", "Test 20", "test20.pas", "< test20.in" },
    { 0, "Basic", "Test 21", "test21.pas", "< test21.in" },
    { 0, "Basic", "Test 23", "test23.pas", "" },
    { 0, "Basic", "C func name", "cfuncname.pas", "" },
    { 0, "Basic", "MT 19937", "mt.pas", "" },
    { 0, "Basic", "String", "str.pas", "" },
    { 0, "Basic", "Linked List", "list.pas", "" },
    { 0, "Basic", "Whetstone", "whet.pas", "" },
    { 0, "Basic", "Variant Record", "variant.pas", "" },
    // Variant variable not supported.
    { LACSAP_ONLY, "Basic", "Variant Rec2", "variant2.pas", "" },
    { 0, "Basic", "Quicksort", "qsort.pas", "< numbers.txt" },
    { 0, "Basic", "Calc Words", "calcwords.pas", "< word-list.txt" },
    // Free pascal doesn't support __FILE__ and __LINE__
    { LACSAP_ONLY, "Basic", "Line & File", "linefile.pas", "" },
    { 0, "Basic", "Set Values", "set.pas", "" },
    { 0, "Basic", "Set Values 2", "set2.pas", "" },
    { 0, "Basic", "Set Values 3", "set3.pas", "" },
    { 0, "Basic", "Set Values 4", "set4.pas", "" },
    // Free Pascal doesn't support popcount!
    { LACSAP_ONLY, "Basic", "Pop Count", "popcnt.pas", "" },
    { 0, "Basic", "Sudoku", "sudoku.pas", "" },
    { 0, "Basic", "General", "general.pas", "< general.in" },
    { 0, "Basic", "Array", "arr.pas", "" },
    { 0, "Basic", "Array 2", "arr2.pas", "" },
    { 0, "Basic", "Array 3", "arr3.pas", "" },
    { LACSAP_ONLY, "Basic", "param", "param.pas", "1 fun \"quoted string\"" },
    { LACSAP_ONLY, "Basic", "pi", "pi.pas", "" },
    // Sizes of types are different.
    { LACSAP_ONLY, "Basic", "size", "size.pas", "" },
    { 0, "Basic", "minmax", "minmax.pas", "< minmax.in" },
    { 0, "Basic", "sign", "sign.pas", "< sign.in" },
    // fmod not supported by FPC.
    { LACSAP_ONLY, "Basic", "course", "course.pas", "< course.in" },
    { 0, "Basic", "loop", "loop.pas", "" },
    // Object handling is not yet compatible.
    { LACSAP_ONLY, "Basic", "obj", "obj.pas", "" },
    { LACSAP_ONLY, "Basic", "Virtuals", "virt.pas", "" },
    { 0, "Basic", "Static Fields", "sf.pas", "" },
    { 0, "Basic", "Dhrystone", "dhry.pas", "< dhry.in" },
    // Function arguments not the same yet.
    { LACSAP_ONLY, "Basic", "Function arg", "func.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg2", "func2.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg3", "func3.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg4", "func4.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg5", "func5.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg6", "func6.pas", "" },
    { LACSAP_ONLY, "Basic", "Function arg7", "func7.pas", "" },
    { 0, "Basic", "Multiple decl", "multidecl.pas", "" },
    { 0, "Basic", "Numeric", "numeric.pas", "" },
    { 0, "Basic", "Goto", "goto.pas", "" },
    { 0, "Basic", "GPC t03", "t03.pas", "" },
    { 0, "Basic", "GPC t04", "t04.pas", "" },
    { 0, "Basic", "GPC t09", "t09.pas", "" },
    { 0, "Basic", "GPC t12", "t12.pas", "" },
    { 0, "Basic", "GPC t14", "t14.pas", "" },
    /* Uses variant label not compatible with FPC */
    { LACSAP_ONLY, "Basic", "GPC varrec1", "varrec1.pas", "" },
    { 0, "Basic", "GPC Transpose", "transpose.pas", "" },
    { 0, "Basic", "Double Begin", "doublebegin.pas", "" },
    { 0, "Basic", "Simple unit", "unit_main.pas", "" },
    { 0, "Basic", "Simple unit2", "unit_main2.pas", "" },
    { LACSAP_ONLY, "Basic", "Pack & Unpack", "packunpack.pas", "" },
    { 0, "Basic", "With statement", "with.pas", "" },
    { LACSAP_ONLY, "Basic", "ISO 7185 PAT", "iso7185pat.pas", "" },
    { 0, "Basic", "Const Expr", "consts.pas", "" },
    { 0, "Basic", "Const Builtin", "consts2.pas", "" },
    { 0, "Basic", "Read char array", "readchars.pas", "< readchars.txt" },
    { 0, "Basic", "Game of life", "gol.pas", "< gol.txt" },
    { 0, "Basic", "Inline", "inline.pas", "" },
    { 0, "Basic", "Val", "val.pas", "" },
    { 0, "Basic", "Bool Ops", "boolops.pas", "" },
    { 0, "Basic", "Exponentiation", "pow.pas", "" },
    { 0, "Basic", "Case Expressions", "caseexpr.pas", "" },
    { 0, "Basic", "for in set", "forinset.pas", "" },
    { 0, "Basic", "New String funcs", "newstringfuncs.pas", "" },
    { 0, "Basic", "Base", "base.pas", "" },
    { 0, "Basic", "Time", "time.pas", "" },
    { 0, "Basic", "Pred & Succ w. 2 args", "predsucc.pas", "" },
    { 0, "Basic", "Type Of", "typeof.pas", "" },
    { 0, "Basic", "Caserange", "caserange.pas", "" },
    { 0, "Basic", "Caserange2", "caserange2.pas", "" },
    { 0, "Basic", "Bindable file", "bindable.pas", "" },
    { 0, "Basic", "Value initialization", "values.pas", "" },
    { 0, "Basic", "String Compare", "strcomp.pas", "" },
    { 0, "Basic", "String Size Expressions", "strsizeexpr.pas", "" },
    { 0, "Basic", "String Capacity", "cap.pas", "" },
    { 0, "Basic", "Type Value", "inittype.pas", "" },
    { 0, "Basic", "Complex Maths", "complex.pas", "" },
    { 0, "Basic", "Init Record", "initrecord.pas", "" },
    { 0, "Basic", "Init Record", "constrecord.pas", "" },
    { 0, "Basic", "Conformant Array", "confarray.pas", "" },
    { 0, "Basic", "Array Slice", "arrayslice.pas", "" },
    { 0, "Basic", "Dynamic Type", "dyntype.pas", "" },
    { 0, "Basic", "Init Pointer", "initptr.pas", "" },
    { 0, "Basic", "SubString", "substr.pas", "" },
    { 0, "Basic", "ResultNmae", "resultname.pas", "" },
    { 0, "Basic", "Dyn Alloc Virtual Object", "dynvirt.pas", "" },
    { 0, "Basic", "WriteStr", "wstr.pas", "" },
    { 0, "Basic", "ReadStr", "rstr.pas", "" },
    { 0, "Basic", "Default value", "default.pas", "" },
    { 0, "Basic", "Bind file", "bind.pas", "" },
    { 0, "Basic", "ISO const eclarations", "isoconst.pas", "" },

    { 0, "File", "CopyFile", "copyfile.pas", "File/infile.dat File/outfile.dat" },
    // get from files not supported.
    { LACSAP_ONLY, "File", "CopyFile2", "copyfile2.pas", "File/infile.dat File/outfile.dat" },
    { 0, "File", "File", "file.pas", "File/test1.txt expected/File/test1.txt" },

    // Check that compiler doesn't get too slow.
    { 0, "Time", "LongCompile", "longcompile.pas", "1000" },
};

// Keep "negative" tests in a separate category
TestEntry negativeCaseList[] = {
    { 0, "CompErr", "Goto err", "goto.pas", "" },
    { 0, "CompErr", "Goto err2", "goto2.pas", "" },
    { 0, "CompErr", "Goto err3", "goto3.pas", "" },
    { 0, "CompErr", "For w. real", "forreal.pas", "" },
    { 0, "CompErr", "For w. ptr", "forptr.pas", "" },
    { 0, "CompErr", "For w. rec", "forrec.pas", "" },
    { 0, "CompErr", "For w. type", "fortype.pas", "" },
    { 0, "CompErr", "Arr Index", "arr.pas", "" },
    { 0, "CompErr", "Binary Ops", "binops.pas", "" },
    { 0, "CompErr", "Boolean Ops", "boolop.pas", "" },
    { 0, "CompErr", "Case Else", "caseelse.pas", "" },
    { 0, "CompErr", "Const", "const.pas", "" },
    { 0, "CompErr", "Const 2", "const2.pas", "" },
    { 0, "CompErr", "Const 3", "const3.pas", "" },
    { 0, "CompErr", "Crazy", "crazy.pas", "" },
    { 0, "CompErr", "Crazy 2", "crazy2.pas", "" },
    { 0, "CompErr", "DivMod", "divmod.pas", "" },
    { 0, "CompErr", "Duplicate Case Labels", "dupcaselabel.pas", "" },
    { 0, "CompErr", "Packed", "packed.pas", "" },
    { 0, "CompErr", "Wrong args", "wrongargs.pas", "" },
    { 0, "CompErr", "Wrong args 2", "wrongargs2.pas", "" },
    { 0, "CompErr", "Wrong args 3", "wrongargs3.pas", "" },
    { 0, "CompErr", "Wrong args 4", "wrongargs4.pas", "" },
    { LACSAP_ONLY, "CompErr", "Assign to constant", "constassign.pas", "" },
    { LACSAP_ONLY, "CompErr", "Downcast class", "downcast.pas", "" },
    { LACSAP_ONLY, "CompErr", "Duplicate member function Definition", "dupmembfunc.pas", "" },
    { 0, "CompErr", "Not Char", "notchar.pas", "" },
    { 0, "CompErr", "Not Real", "notreal.pas", "" },
    { 0, "CompErr", "Bad negate", "badneg.pas", "" },
};

void runTestCases(const std::vector<TestCase*>& tc, TestResult& res, const std::string& options)
{
    for (auto t : tc)
    {
	res.RegisterCase(t);
	t->Clean();
	if (!t->Compile(options))
	{
	    res.RegisterFail(t, "compile");
	}
	else
	{
	    if (!t->Run())
	    {
		res.RegisterFail(t, "run");
	    }
	    else
	    {
		if (!t->Result())
		{
		    res.RegisterFail(t, "result");
		}
		else
		{
		    res.RegisterPass(t);
		}
	    }
	}
    }
}

int main(int argc, char** argv)
{
    std::vector<TestCase*>   tc;
    TestResult               res;
    std::string              mode = "full";
    std::vector<std::string> optimizations = { "", "-O0", "-O1", "-O2" };
    std::vector<std::string> models = { "",
#if M32_DISABLE == 0
					"-m32", "-m64"
#endif
    };
    std::vector<std::string> others = { "", "-Cr", "-g" };
    int                      flags = 0;
    int                      negative = false;

    for (int i = 1; i < argc; i++)
    {
	if (std::string(argv[i]) == "-F")
	{
	    compiler = compilers[1];
	    mode = "-O1";
	    flags |= LACSAP_ONLY;
	}
	else if (std::string(argv[i]) == "-N")
	{
	    negative = true;
	}
	else
	{
	    mode = argv[i];
	}
    }

    if (mode == "full" || !negative)
    {
	for (auto t : testCaseList)
	{
	    if ((t.flags & flags) == 0)
	    {
		tc.push_back(TestCaseFactory(t.type, t.name, t.source, t.args));
	    }
	}
    }

    if (mode == "full" || negative)
    {
	for (auto t : negativeCaseList)
	{
	    if ((t.flags & flags) == 0)
	    {
		tc.push_back(TestCaseFactory(t.type, t.name, t.source, t.args));
	    }
	}
    }

    if (mode == "full")
    {
	for (auto opt : optimizations)
	{
	    for (auto model : models)
	    {
		for (auto other : others)
		{
		    runTestCases(tc, res, opt + " " + model + " " + other);
		}
	    }
	}
    }
    else
    {
	runTestCases(tc, res, mode);
    }

    res.Report();
    bool anyFail = res.AnyFail();

    for (auto i : tc)
    {
	delete i;
    }

    return static_cast<int>(anyFail);
}
