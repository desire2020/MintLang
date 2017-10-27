//#define __DEBUG__
#include <iostream>
#include <memory>
#include "scanner.hpp"
#include "compiler.hpp"
using namespace std;
int main(int argc, char *argv[])
{
    ios :: sync_with_stdio(false);
    shared_ptr<msharp::_Compiler> Compiler;
    if (argc > 1)
        Compiler = make_shared(new msharp::_Compiler(argc, argv));
    else
        Compiler = make_shared(new msharp::_Compiler(string("E:\\test.c")));
    return Compiler -> exit_on_success();
}
