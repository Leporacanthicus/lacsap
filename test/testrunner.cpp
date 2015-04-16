#include <string> 
#include <iostream>
#include <vector>
#include <map>
#include <iomanip>
#include <cstdlib>
#include <chrono>
#include <cassert>

std::string compiler = "../lacsap";

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
    TestResult():
	cases(0), pass(0), fail(0)
	{
	}
public:
    void RegisterFail(const TestCase* tc, const std::string& stage)
	{
	    failedTests.push_back(tc->Name());
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
		std::cout << t << std::endl;
	    }
	}
    
private:
    int cases;
    int pass;
    int fail;
    std::map<std::string, int> failStageMap;
    std::vector<std::string> failedTests;
};    

struct
{
    const char *type;
    const char *name;
    const char *source;
    const char *args;
} testCaseList[] = 
{
    { "Basic", "Math",          "math.pas",        "" },
    { "Basic", "HungryMouse",   "hungrymouse.pas", " < hungrymouse.txt" },
    { "Basic", "Types",         "type.pas",        "" },
    { "Basic", "WC",            "wc.pas",          "" },
    { "Basic", "Histogram",     "hist.pas",        " < hist.pas" },
    { "Basic", "Case",          "case.pas",        "" },
    { "Basic", "TestSet",       "testset.pas",     "" },
    { "Basic", "TestSet 2",     "testset2.pas",    "" },
    { "Basic", "SetTest",       "set_test.pas",    "" },
    { "Basic", "Record Pass",   "recpass.pas",     "" },
    { "Basic", "Random Number", "randtest.pas",    "" },
    { "Basic", "Fact Bignum",   "fact-bignum.pas", "" },
    { "Basic", "Nested Funcs",  "nestfunc.pas",    "" },
    { "Basic", "Recursion",     "recursion.pas",   " < recursion.txt" },
    { "Basic", "Test 04",       "test04.pas",      "" },
    { "Basic", "Test 07",       "test07.pas",      "" },
    { "Basic", "Test 08",       "test08.pas",      "" },
    { "Basic", "Test 11",       "test11.pas",      "" },
    { "Basic", "Test 12",       "test12.pas",      "" },
    { "Basic", "Test 13",       "test13.pas",      "" },
    { "Basic", "Test 14",       "test14.pas",      "" },
    { "Basic", "Test 16",       "test16.pas",      "" },
    { "Basic", "Test 17",       "test17.pas",      "" },
    { "Basic", "Test 20",       "test20.pas",      "< test20.in" },
    { "Basic", "Test 21",       "test21.pas",      "< test21.in" },
    { "Basic", "Test 23",       "test23.pas",      "" },
    { "Basic", "C func name",   "cfuncname.pas",   "" },
    { "Basic", "MT 19937",      "mt.pas",          "" },
    { "Basic", "String",        "str.pas",         "" },
    { "Basic", "Linked List",   "list.pas",        "" },
    { "Basic", "Whetstone",     "whet.pas",        "" },
    { "Basic", "Variant Record","variant.pas",     "" },
    { "Basic", "Variant Rec2",  "variant2.pas",    "" },
    { "Basic", "Quicksort",     "qsort.pas",       "< numbers.txt" },
    { "Basic", "Calc Words",    "calcwords.pas",   "< /usr/share/dict/words" },
    { "Basic", "Line & File",   "linefile.pas",    "" },
    { "Basic", "Set Values",    "set.pas",         "" },
    { "Basic", "Pop Count",     "popcnt.pas",      "" },
    { "Basic", "Sudoku",        "sudoku.pas",      "" },
    { "Basic", "General",       "general.pas",     "< general.in" },
    { "Basic", "Array",         "arr.pas",         "" },
    { "Basic", "param",         "param.pas",       "1 fun \"quoted string\"" },
    { "Basic", "pi",            "pi.pas",          "" },
    { "Basic", "size",          "size.pas",        "" },
    { "Basic", "minmax",        "minmax.pas",      "< minmax.in" },
    { "Basic", "sign",          "sign.pas",        "< sign.in" },
    { "Basic", "course",        "course.pas",      "< course.in" },
    { "Basic", "loop",          "loop.pas",        "" },
    { "Basic", "obj",           "obj.pas",         "" },
    { "Basic", "Static Fields", "sf.pas",          "" },

    { "File",  "CopyFile",      "copyfile.pas",    "infile.dat outfile.dat" },
    { "File",  "CopyFile2",     "copyfile2.pas",   "infile.dat outfile.dat" },
    { "File",  "File",          "file.pas",        "test1.txt expected/test1.txt" },

    { "Time",  "LongCompile",   "longcompile.pas", "1000" },
};


void runTestCases(const std::vector<TestCase*>& tc, 
		  TestResult& res,
		  const std::string& options)
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
    bool runSomeTests = false;
    std::string mode = "full";

    if (argc >= 2)
    {
	mode = argv[1];
    }

    for(auto t : testCaseList)
    {
	tc.push_back(TestCaseFactory(t.type, t.name, t.source, t.args));
    }

    if (mode == "full" || mode == "-O0")
    {
	runTestCases(tc, res, "-O0");
	runSomeTests = true;
    }
    if (mode == "full" || mode == "-O1")
    {
	runTestCases(tc, res, "-O1");
	runSomeTests = true;
    }
    if (mode == "full" || mode == "-O1 -Cr")
    {
	runTestCases(tc, res, "-O1 -Cr");
	runSomeTests = true;
    }
    if (mode == "full" || mode == "-O2")
    {
	runTestCases(tc, res, "-O2");
	runSomeTests = true;
    }
    if (!runSomeTests)
    {
	runTestCases(tc, res, mode);
    }
    res.Report();

    for(auto i : tc)
    {
	delete i;
    }
}

