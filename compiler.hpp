#ifndef COMPILER_HPP
#define COMPILER_HPP
#include "tools.hpp"
#include "scanner.hpp"
#include "parser.hpp"
#include "translator.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
using namespace std;
namespace msharp
{
    class _Compiler
    {
        _Scanner Scanner;
        _Parser Parser;
        _Translator Translator;
    public:
        void const_expr_wrap(int argc, string target_file);
        _Compiler(const string & src)
            : Parser(Scanner.lexemes), Translator(Parser)
        {
            ifstream fin(src);
            if (!fin.is_open())
                Error.compile_error("fatal : invalid file location.");
            while (!fin.eof())
            {
                string thisline;
                getline(fin, thisline);
                Scanner.append(thisline);
            }
            Parser.construct_CST();
            Parser.remap_constantstring();
            Parser.class_declare();
            Parser.check_conn();
            Parser.deep_declare();
            Parser.type_validation();
            for (ASTNode & tree : Parser.forest)
                tree.print_tree(std::cerr, 0);
            std::cerr <<  "\n\nAfter optimization:\n";
            Parser.do_AST_level_optimization();
            for (ASTNode & tree : Parser.forest)
                tree.print_tree(std::cerr, 0);
            std :: cerr << "\n";
            Parser.check_conn();
            Translator.type_reload(Parser.name_table);
            Translator.execute();
            ofstream fout("E:\\test.asm");
            Translator.to_amd64_linux_asm(fout);
            if (Parser.safe_to_be_wrapped)
                std::cerr << "WRAPPED!" << "\n";
            fout.close();
        }
        _Compiler(int argc, char *argv[])
            : Parser(Scanner.lexemes), Translator(Parser)
        {
            if (argc == 1)
            {
                Error.compile_error("fatal : no input files.");
            } else {
                string options;
                for (int i = 1; i < 2; ++i)
                    options = string(argv[i]);
                ifstream fin(options);
                if (!fin.is_open())
                    Error.compile_error("fatal : invalid file location.");
                while (!fin.eof())
                {
                    string thisline;
                    getline(fin, thisline);
                    Scanner.append(thisline);
                }
            }
            Parser.construct_CST();
            Parser.remap_constantstring();
            Parser.class_declare();
            Parser.check_conn();
            Parser.deep_declare();
            Parser.type_validation();
            Parser.do_AST_level_optimization();
            if (argc > 2)
            {
                string output(argv[2]);
                ofstream fout(output);
                Translator.type_reload(Parser.name_table);
                Translator.execute();
                Translator.to_amd64_linux_asm(fout);
                fout.close();
                if (Parser.safe_to_be_wrapped)
                    const_expr_wrap(2, output);
            }
        }
        int exit_on_success()
        {
            return 0;
        }
    };
}
#endif // COMPILER_HPP
