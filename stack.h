#ifndef STACK_H
#define STACK_H

#include "options.h"
#include "namedobject.h"
#include "utils.h"
#include <deque>
#include <map>
#include <string>
#include <iostream>
#include <vector>

template <typename T>
class Stack
{
public:
    // Expose this so we can use it as InterfaceList for example.
    typedef std::map<std::string, T> MapType;
private:
    typedef typename MapType::const_iterator MapIter;
    typedef std::deque<MapType> StackType;
    typedef typename StackType::const_reverse_iterator StackRIter;
public:
    Stack() { NewLevel(); }
    void NewLevel() 
    { 
	stack.push_back(MapType()); 
    }

    size_t MaxLevel() const
    {
	return stack.size()-1;
    }

    std::vector<T> GetLevel()
    {
	std::vector<T> v;
	for(auto i : stack.back())
	{
	    v.push_back(i.second);
	}
	return v;
    }

    std::vector<T> GetLevel(int n)
    {
	std::vector<T> v;
	assert(n < stack.size() && "Requests for getlevel should be in bounds...");
	for(auto i : stack[n])
	{
	    v.push_back(i.second);
	}
	return v;
    }

    void DropLevel() 
    { 
	stack.pop_back(); 
    }

    /* Returns false on failure */
    bool Add(std::string name, const T v) 
    {
	if (caseInsensitive)
	{
	    strlower(name);
	}
	MapIter it = stack.back().find(name);
	if (it == stack.back().end())
	{
	    if (verbosity > 1)
	    {
		std::cerr << "Adding value: " << name << std::endl;
	    }
	    stack.back()[name] = v;
	    return true;
	}
	return false;
    }

    T Find(std::string name, size_t& level) const
    {
	int lvl = MaxLevel();
	if (verbosity > 1)
	{
	    std::cerr << "Finding value: " << name << std::endl;
	}
	if (caseInsensitive)
	{
	    strlower(name);
	}
	for(StackRIter s = stack.rbegin(); s != stack.rend(); s++, lvl--)
	{
	    MapIter it = s->find(name);
	    if (it != s->end())
	    {
		level = lvl;
		if (verbosity > 1)
		{
		    std::cerr << "Found at lvl " << lvl << std::endl;
		}
		return it->second;
	    }
	}
	if (verbosity > 1)
	{
	    std::cerr << "Not found" << std::endl;
	    dump();
	}
	return 0;
    }

    T Find(const std::string& name) 
    {
	size_t dummy;
	return Find(name, dummy);
    }

    T FindTopLevel(std::string name)
    {
	if (caseInsensitive)
	{
	    strlower(name);
	}
	MapIter it = stack.back().find(name);
	if (it != stack.back().end())
	{
	    return it->second;
	}
	return 0;
    }

    T FindBottomLevel(std::string name)
    {
	if (caseInsensitive)
	{
	    strlower(name);
	}
	MapIter it = stack.front().find(name);
	if (it != stack.front().end())
	{
	    return it->second;
	}
	return 0;
    }

    void dump(std::ostream& out) const;
    void dump() const { dump(std::cerr); }
private:
    StackType stack;
};

template <typename T>
class StackWrapper
{
public:
    StackWrapper(Stack<T> &v) : stack(v)
    {
	stack.NewLevel();
    }
    ~StackWrapper()
    {
	stack.DropLevel();
    }
private:
    Stack<T>& stack;
};

template <typename T>
void Stack<T>::dump(std::ostream& out) const
{
    int n = 0;
    for(auto s : stack)
    {
	out << "Level " << n << std::endl;
	n++;
	for(auto v : s)
	{
	    out << v.first << ": ";
	    v.second->dump(out);
	    out << std::endl;
	}
    }
}

class InterfaceList
{
public:
    InterfaceList() {};
    bool Add(std::string name, const NamedObject* obj);
    const Stack<const NamedObject*>::MapType& List() const { return list; }
private:
    Stack<const NamedObject*>::MapType list;
};

#endif
