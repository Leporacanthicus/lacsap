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
    virtual bool Compile();
    virtual bool Run();
    virtual bool Result();
protected:
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
    return Diff(resname + " expected/" + tplname);
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


TestCase* TestCaseFactory(const std::string& type, 
			  const std::string& name,
			  const std::string& source,
			  const std::string& args)
{
    if (type == "File")
    {
	return new FileTestCase(name, source, args);
    }

    return new TestCase(name, source, args);
}

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
    { "Basic", "Case",          "case.pas",        "" },
    { "Basic", "Set",           "testset.pas",     "" },
    { "Basic", "Set 2",         "testset2.pas",    "" },
    { "Basic", "Record Pass",   "recpass.pas",     "" },
    { "Basic", "Random Number", "randtest.pas",    "" },

    { "File",  "CopyFile",      "copyfile.pas",    "infile.dat outfile.dat" },
    { "File",  "CopyFile2",     "copyfile2.pas",   "infile.dat outfile.dat" },
    { "File",  "File",          "file.pas",        "test1.txt expected/test1.txt" },
};
	

int main()
{
    int compileFail = 0;
    int runFail     = 0;
    int resultFail  = 0;
    int pass        = 0;
    int cases       = 0;
    int fail        = 0;

    std::vector<TestCase*> tc; 

    for(auto t : testCaseList)
    {
	tc.push_back(TestCaseFactory(t.type, t.name, t.source, t.args));
    }

    for(auto t : tc)
    {
	cases ++;
	if (!t->Compile())
	{
	    fail++;
	    compileFail++;
	}
	else
	{
	    if (!t->Run())
	    {
		fail++;
		runFail++;
	    }
	    else
	    {
		if (!t->Result())
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

