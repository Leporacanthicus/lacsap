#include <string> 
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdlib>

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
    return system(cmd.c_str());
}

class TestCase
{
public:
    TestCase(const std::string& nm, const std::string& src, const std::string& arg);
    // Compile, Run and Result functions return false on "failure", true on "good"
    virtual bool Compile();
    virtual bool Run();
    virtual bool Result();
private:
    std::string name;
    std::string source;
    std::string args;
};

TestCase::TestCase(const std::string& nm, const std::string& src, const std::string& arg)
    : name(nm), source(src), args(arg)
{
}

bool TestCase::Compile()
{
    if (runCmd(compiler + " " + source) == 0)
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
    if (runCmd("diff " + resname + " expected/" + tplname))
    {
	return false;
    }
    return true;
}

std::vector<TestCase> tc; 

struct
{
    const char *name;
    const char *source;
    const char *args;
} testCaseList[] = 
{
    { "Math",          "math.pas",        "" },
    { "HungryMouse",   "hungrymouse.pas", " < hungrymouse.txt" },
    { "Types",         "type.pas",        "" },
    { "WC",            "wc.pas",          "" },
    { "Case",          "case.pas",        "" },
    { "Set",           "testset.pas",     "" },
    { "Set 2",         "testset2.pas",    "" },
    { "Record Pass",   "recpass.pas",     "" },
    { "Random Number", "randtest.pas",    "" },
};
	

int main()
{
    int compileFail = 0;
    int runFail     = 0;
    int resultFail  = 0;
    int pass        = 0;
    int cases       = 0;
    int fail        = 0;

    for(auto t : testCaseList)
    {
	tc.push_back(TestCase(t.name, t.source, t.args));
    }

    for(auto t : tc)
    {
	cases ++;
	if (!t.Compile())
	{
	    fail++;
	    compileFail++;
	}
	else
	{
	    if (!t.Run())
	    {
		fail++;
		runFail++;
	    }
	    else
	    {
		if (!t.Result())
		{
		    fail++;
		    resultFail++;
		}
		else
		{
		    pass++;
		}
	    }
	}
    }
    std::cout << "Cases:  " << std::setw(5) << cases << std::endl;
    std::cout << "Pass:   " << std::setw(5) << cases << std::endl;
    std::cout << "Fail:   " << std::setw(5) << fail << std::endl;

    if (fail)
    {
	std::cout << "Compile fail: " << std::setw(5) << compileFail << std::endl;
	std::cout << "Run fail:     " << std::setw(5) << runFail << std::endl;
	std::cout << "Result fail:  " << std::setw(5) << resultFail << std::endl;
    }
}

