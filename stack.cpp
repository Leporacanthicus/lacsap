#include "stack.h"
#include "utils.h"

bool InterfaceList::Add(std::string name, const NamedObject* obj) 
{
    if (caseInsensitive)
    {
	strlower(name);
    }
    auto it = list.find(name);
    if (it == list.end())
    {
	if (verbosity > 1)
	{
	    std::cerr << "Adding value: " << name << std::endl;
	}
	list[name] = obj;
	return true;
    }
    return false;
}
