#include "stack.h"

bool InterfaceList::Add(std::string name, const NamedObject* obj) 
{
    if (caseInsensitive)
    {
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
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
