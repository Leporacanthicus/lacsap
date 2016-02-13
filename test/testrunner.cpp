#include <string> 
#include <iostream>
#include <vector>
#include <map>
#include <iomanip>
#include <cstdlib>
#include <chrono>
#include <cassert>

std::string compilers[] = {"../lacsap", "fpc -Mdelphi" };
std::string compiler = compilers[0];

// TODO: Move this to a "utility" library?
std::string replace_ext(const std::string &origName, const std::string& expectedExt, const std::string& newExt)
{
    if (origName.substr(origName.size() - expectedExt.size()) != expectedExt)
    {
	std::cerr << "Could not find extension..." << std::endl;
	exit(1);
	return "";
    }
    return origName.substr(0, origName.size() - expectedExt.size()) + newExt;
}

int runCmd(const std::string& cmd)
{
    std::cout << "Executing: " << cmd << std::endl;
    return system(cmd.c_str());
}

bool Diff(const std::string& args)
{
    if (runCmd("diff " + args))
    {
	return false;
    }
    return true;
}

class TestCase
{
public:
    TestCase(const std::string& nm, const std::string& src, const std::string& arg);
    // Compile, Run and Result functions return false on "failure", true on "good"
    virtual void Clean();
    virtual bool Compile(const std::string& options);
    virtual bool Run();
    virtual bool Result();
    std::string  Name() const;
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
}    

bool TestCase::Compile(const std::string& options)
{
    if (runCmd(compiler + " " + options + " " + source) == 0)
    {
	return true;
    }
    return false;
}

bool TestCase::Run()
{
    std::string exename = replace_ext(source, ".pas", "");
    std::string resname = replace_ext(source, ".pas", ".res");
    if (runCmd(std::string("./") + exename + " " + args + " > " + resname ))
    {
	return false;
    }
    return true;
}

bool TestCase::Result()
{
    std::string resname = replace_ext(source, ".pas", ".res");
    std::string tplname = replace_ext(source, ".pas", ".tpl");
    return Diff(resname + " expected/" + tplname);
}

std::string TestCase::Name() const
{
    return name;
}

class FileTestCase : public TestCase
{
public:
    FileTestCase(const std::string& nm, const std::string& src, const std::string& arg);
    virtual bool Result();
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
    virtual bool Result();
    virtual bool Compile(const std::string& options);
private:
    long maxTime;  // In milliseconds.
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
    long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    if (elapsed > maxTime)
    {
	std::cerr << "Took too long to compiel  " << source << " "
		  << std::fixed << std::setprecision(3) << elapsed << " ms" << std::endl;
	return false;
    }
    return true;
}


TestCase* TestCaseFactory(const std::string& type, 
			  const std::string& name,
			  const std::string& source,
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
	Failure(const std::string& nm, const std::string& st)
	    : name(nm), stage(st) { }
    };
    TestResult()
	: cases(0), pass(0), fail(0) { }
public:
    void RegisterFail(const TestCase* tc, const std::string& stage)
	{
	    failedTests.push_back(Failure(tc->Name(), stage));
	    failStageMap[stage]++;
	    fail++;
	}
    void RegisterCase(const TestCase* /* tc */)
	{
	    cases++;
	}

    void RegisterPass(const TestCase* /* tc */)
	{
	    pass++;
	}

    bool AnyFail() { return fail != 0; }

    void Report()
	{
	    std::cout << "Cases:  " << std::setw(5) << cases << std::endl;
	    std::cout << "Pass:   " << std::setw(5) << pass << std::endl;
	    std::cout << "Fail:   " << std::setw(5) << fail << std::endl;
	    
	    for(auto f : failStageMap)
	    {
		std::cout << f.first << " fail: " << std::setw(5) << f.second << std::endl;
	    }

	    bool b = true;
	    for(auto t : failedTests)
	    {
		if (b)
		{
		    std::cout << "The following tests failed:" << std::endl;
		}
		b = false;
		std::cout << std::setw(20) << t.name << " " << t.stage << std::endl;
	    }
	}
    
private:
    int cases;
    int pass;
    int fail;
    std::map<std::string, int> failStageMap;
    std::vector<Failure> failedTests;
};


enum TestFlags
{
    LACSAP_ONLY = 1 << 0,
};

struct
{
    int flags;
    const char *type;
    const char *name;
    const char *source;
    const char *args;
} testCaseList[] = 
{
    { 0,           "Basic", "Math",          "mathtest.pas",    "" },
    // Results differ due to different random number generator
    { LACSAP_ONLY, "Basic", "HungryMouse",   "hungrymouse.pas", " < hungrymouse.txt" },
    { 0,           "Basic", "Types",         "type.pas",        "" },
    { 0,           "Basic", "WC",            "wc.pas",          "" },
    { 0,           "Basic", "Histogram",     "hist.pas",        " < hist.pas" },
    // Case-statement using otherwise not available in FPC.
    // Alternative implementation in CaseCompat test.
    { LACSAP_ONLY, "Basic", "Case",          "case.pas",        "" },
    { 0,           "Basic", "CaseCompat",    "casecompat.pas",  "" },
    { 0,           "Basic", "TestSet",       "testset.pas",     "" },
    { 0,           "Basic", "TestSet 2",     "testset2.pas",    "" },
    { LACSAP_ONLY, "Basic", "TestSet 3",     "testset3.pas",    "" },
    { 0,           "Basic", "SetTest",       "set_test.pas",    "" },
    { 0,           "Basic", "Record Pass",   "recpass.pas",     "" },
    // Random numbers are diferent
    { LACSAP_ONLY, "Basic", "Random Number", "randtest.pas",    "" },
    { 0,           "Basic", "Fact Bignum",   "fact-bignum.pas", "" },
    { 0,           "Basic", "Nested Funcs",  "nestfunc.pas",    "" },
    { 0,           "Basic", "Recursion",     "recursion.pas",   " < recursion.txt" },
    { 0,           "Basic", "Test 04",       "test04.pas",      "" },
    { 0,           "Basic", "Test 07",       "test07.pas",      "" },
    { 0,           "Basic", "Test 08",       "test08.pas",      "" },
    { 0,           "Basic", "Test 11",       "test11.pas",      "" },
    { 0,           "Basic", "Test 12",       "test12.pas",      "" },
    { 0,           "Basic", "Test 13",       "test13.pas",      "" },
    { 0,           "Basic", "Test 14",       "test14.pas",      "" },
    { 0,           "Basic", "Test 16",       "test16.pas",      "" },
    { 0,           "Basic", "Test 17",       "test17.pas",      "" },
    { 0,           "Basic", "Test 20",       "test20.pas",      "< test20.in" },
    { 0,           "Basic", "Test 21",       "test21.pas",      "< test21.in" },
    { 0,           "Basic", "Test 23",       "test23.pas",      "" },
    { 0,           "Basic", "C func name",   "cfuncname.pas",   "" },
    { 0,           "Basic", "MT 19937",      "mt.pas",          "" },
    { 0,           "Basic", "String",        "str.pas",         "" },
    { 0,           "Basic", "Linked List",   "list.pas",        "" },
    { 0,           "Basic", "Whetstone",     "whet.pas",        "" },
    { 0,           "Basic", "Variant Record","variant.pas",     "" },
    // Variant variable not supported.
    { LACSAP_ONLY, "Basic", "Variant Rec2",  "variant2.pas",    "" },
    { 0,           "Basic", "Quicksort",     "qsort.pas",       "< numbers.txt" },
    { 0,           "Basic", "Calc Words",    "calcwords.pas",   "< /usr/share/dict/words" },
    // Free pascal doesn't support __FILE__ and __LINE__
    { LACSAP_ONLY, "Basic", "Line & File",   "linefile.pas",    "" },
    { 0,           "Basic", "Set Values",    "set.pas",         "" },
    { 0,           "Basic", "Set Values 2",  "set2.pas",        "" },
    { 0,           "Basic", "Set Values 3",  "set3.pas",        "" },
    // Free Pascal doesn't support popcount!
    { LACSAP_ONLY, "Basic", "Pop Count",     "popcnt.pas",      "" },
    { 0,           "Basic", "Sudoku",        "sudoku.pas",      "" },
    { 0,           "Basic", "General",       "general.pas",     "< general.in" },
    { 0,           "Basic", "Array",         "arr.pas",         "" },
    { 0,           "Basic", "Array 2",       "arr2.pas",        "" },
    { LACSAP_ONLY, "Basic", "param",         "param.pas",       "1 fun \"quoted string\"" },
    { LACSAP_ONLY, "Basic", "pi",            "pi.pas",          "" },
    // Sizes of types are different.
    { LACSAP_ONLY, "Basic", "size",          "size.pas",        "" },
    { 0,           "Basic", "minmax",        "minmax.pas",      "< minmax.in" },
    { 0,           "Basic", "sign",          "sign.pas",        "< sign.in" },
    // fmod not supported by FPC.
    { LACSAP_ONLY, "Basic", "course",        "course.pas",      "< course.in" },
    { 0,           "Basic", "loop",          "loop.pas",        "" },
    // Object handling is not yet compatible.
    { LACSAP_ONLY, "Basic", "obj",           "obj.pas",         "" },
    { LACSAP_ONLY, "Basic", "Virtuals",      "virt.pas",        "" },
    { 0,           "Basic", "Static Fields", "sf.pas",          "" },
    { 0,           "Basic", "Dhrystone",     "dhry.pas",        "< dhry.in" },
    // Function arguments not the same yet.
    { LACSAP_ONLY, "Basic", "Function arg",  "func.pas",        "" },
    { LACSAP_ONLY, "Basic", "Function arg2", "func2.pas",       "" },
    { LACSAP_ONLY, "Basic", "Function arg3", "func3.pas",       "" },
    { LACSAP_ONLY, "Basic", "Function arg4", "func4.pas",       "" },
    { LACSAP_ONLY, "Basic", "Function arg5", "func5.pas",       "" },
    { 0,           "Basic", "Multiple decl", "multidecl.pas",   "" },
    { 0,           "Basic", "Numeric",       "numeric.pas",     "" },
    { 0,           "Basic", "Goto",          "goto.pas",        "" },
    { 0,           "Basic", "GPC t03",       "t03.pas",         "" },
    { 0,           "Basic", "GPC t04",       "t04.pas",         "" },
    { 0,           "Basic", "GPC t09",       "t09.pas",         "" },
    { 0,           "Basic", "GPC t12",       "t12.pas",         "" },
    { 0,           "Basic", "GPC t14",       "t14.pas",         "" },
    /* Uses variant label not compatible with FPC */
    { LACSAP_ONLY, "Basic", "GPC varrec1",   "varrec1.pas",     "" },
    { 0,           "Basic", "GPC Transpose", "transpose.pas",   "" },
    { 0,           "Basic", "Double Begin",  "doublebegin.pas", "" },
    { 0,           "Basic", "Simple unit",   "unit_main.pas",   "" },
    { 0,           "Basic", "Pack & Unpack", "packunpack.pas",  "" },
    { 0,           "Basic", "With statement","with.pas",        "" },
    { 0,           "Basic", "ISO 7185 PAT",  "iso7185pat.pas",  "" },

    { 0,           "File",  "CopyFile",      "copyfile.pas",    "infile.dat outfile.dat" },
    // get from files not supported.
    { LACSAP_ONLY, "File",  "CopyFile2",     "copyfile2.pas",   "infile.dat outfile.dat" },
    { 0,           "File",  "File",          "file.pas",        "test1.txt expected/test1.txt" },

    { 0,           "Time",  "LongCompile",   "longcompile.pas", "1000" },
};


void runTestCases(const std::vector<TestCase*>& tc, TestResult& res, const std::string& options)
{
    for(auto t : tc)
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

int main(int argc, char **argv)
{
    std::vector<TestCase*> tc; 
    TestResult res;
    std::string mode = "full";
    std::vector<std::string> optimizations = { "", "-O0", "-O1", "-O2" };
    std::vector<std::string> models = { "", 
#if M32_DISABLE == 0
					"-m32", "-m64" 
#endif
    };
    std::vector<std::string> others = { "", "-Cr", "-g" };

    std::cout << "PATH=" << getenv("PATH") << std::endl;

    int flags = 0;

    for(int i = 1; i < argc; i++)
    {
	if (std::string(argv[i]) == "-F")
	{
	    compiler = compilers[1];
	    mode = "-O1";
	    flags |= LACSAP_ONLY;
	}
	else
	{
	    mode = argv[i];
	}
    }

    for(auto t : testCaseList)
    {
	if ((t.flags & flags) == 0)
	{
	    tc.push_back(TestCaseFactory(t.type, t.name, t.source, t.args));
	}
    }

    if (mode == "full")
    {
	for(auto opt : optimizations)
	{
	    for(auto model : models)
	    {
		for(auto other : others)
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

    for(auto i : tc)
    {
	delete i;
    }
    
    return static_cast<int>(anyFail);
}
