#include "compiler.hpp"

namespace msharp
{

    void _Compiler::const_expr_wrap(int argc, string target_file)
    {
        ios :: sync_with_stdio(false);
        int ret_v = system(("nasm -felf64 " + target_file + " -o tmp.o").c_str());
        ret_v = system(string("gcc tmp.o -o tmp").c_str());
        ret_v = system(string("./tmp >tmp.log").c_str());
        string output_s = "";
        ifstream fin("tmp.log");
        bool flag = false;
        while (!fin.eof())
        {
            string s;
            getline(fin, s);
            output_s += s;
            output_s += "\n";
        }
        fin.close();
        ofstream fout(target_file);
        fout << "default rel\n"
             << "extern puts\n"
             << "global main\n"
             << "SECTION .text\n"
             << "main:\n"
             << "\tmov\trdi, __conststr_0\n"
             << "\tcall\tputs\n"
             << "\tmov\trax, " << (ret_v >> 8) << "\n"
             << "\tret\n"
             << "SECTION .text\n"
             << "__conststr_0:\n"
             << "\tdb\t";
        for (int i = 0; i < output_s.length(); ++i)
        {
            fout << int(output_s[i]) << ", ";
        }
        fout << "0\n";
        fout.close();
    }

}
