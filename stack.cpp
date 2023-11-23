#include "stack.h"
#include "utils.h"

bool InterfaceList::Add(const NamedObject* obj)
{
    std::string name = obj->Name();
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
