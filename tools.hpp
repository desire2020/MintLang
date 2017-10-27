#ifndef TOOLS_HPP
#define TOOLS_HPP
#include <string>
#include <set>
#include <iostream>
#include <unordered_map>
#include <map>
#include <functional>
#include <memory>
#include <sstream>

//Allocator Model Selection:
//#define __infinity__reg__assumption


using std::string;
using std::shared_ptr;
using std::unordered_map;
using std::stringstream;
namespace msharp
{
    template <class T>
    bool is_in(const T & a, const std::set<T> & the_set)
    {
        return the_set.find(a) != the_set.end();
    }
    template <class T, class T2>
    bool is_in(const T & a, const std::unordered_map<T, T2> & the_set)
    {
        return the_set.find(a) != the_set.end();
    }
    template <class T, class T2>
    bool is_in(const T & a, const std::map<T, T2> & the_set)
    {
        return the_set.find(a) != the_set.end();
    }
    template <class T>
    inline shared_ptr<T> make_shared(T * const _ptr)
    {
        return shared_ptr<T>(_ptr);
    }
    template <class T>
    string join(string sep, const T & li)
    {
        string ret;
        bool sp = false;
        for (auto x : li)
        {
            if (sp) ret += sep;
            sp = true;
            ret += x;
        }
        return ret;
    }

    class ToolObj
    {
    protected:
        ToolObj() {}
        ToolObj(const ToolObj & src) = default;
    public:
        static ToolObj & get_instance()
        {
            static ToolObj _Tool;
            return _Tool;
        }
        static char to_char(long long n)
        {
            return '0' + n;
        }
        static string to_string(long long n)
        {
            stringstream sin;
            sin << n;
            return sin.str();
        }
        static long long to_int(const string & src)
        {
            stringstream sin;
            sin << src;
            long long ret;
            sin >> ret;
            return ret;
        }
    };
    extern ToolObj & Tool;

    class _Error
    {
    public:
        static void compile_error(const string & p)
        {
            std::cerr << p << "\n";
            exit(201);
        }
        static string place(int l, int r)
        {
            return string("at line ") + Tool.to_string(l) + string(", col ") + Tool.to_string(r);
        }
    };
    extern _Error Error;
}
#endif // MSHARP_UTILITY_HPP
