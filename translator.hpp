#ifndef TRANSLATOR_HPP
#define TRANSLATOR_HPP
#include "parser.hpp"
#include <stack>
#include <set>
#include <sstream>
#include <functional>
using std::stringstream;
using std::set;
using std::stack;
using std::map;
using std::multimap;
namespace msharp
{
    class _Translator;

    class VirtualRegisterRankMap
    {
        map<int, set<string>, std::greater<int>> ranker;
        map<string, int> weight;
#ifndef __infinity__reg__assumption
        const int RegisterLimit = 5;
#else
        const int RegisterLimit = 20000;
#endif
        shared_ptr<deque<string>> __topK;
    public:
        static deque<string> avaliable_reg()
        {
#ifndef __infinity__reg__assumption
            return {"rbx", "r12", "r13", "r14", "r15"};
#else
            static deque<string> __ret_v = __avaliable_reg();
            return __ret_v;
        }

        static deque<string> __avaliable_reg()
        {
            deque<string> r;
            for (int i = 0; i < 2000; ++i) r.push_back("v" + Tool.to_string(i));
            return r;
#endif
        }

        void promote(const string & title, int delta)
        {
            __topK = nullptr;
            ranker.operator[](weight[title]).erase(title);
            weight[title] += delta;
            ranker.operator[](weight[title]).insert(title);
        }
        deque<string> & topK()
        {
            if (__topK != nullptr)
                return *__topK;
            int K = RegisterLimit;
            deque<string> ret;
            K = min(K, RegisterLimit);
            for (auto & s : ranker)
                if (ret.size() < K)
                    for (auto & l : s.second)
                        if (ret.size() < K)
                            ret.push_back(l);
                        else
                            break;
                else
                    break;
            __topK = make_shared(new deque<string>(std::move(ret)));
            return *__topK;
            //Damn it!
        }
    };


    struct ASMInstr
    {
        string title;
        deque<string> args;
    };

    struct BasicBlock
    {
        string label;
        deque<ASMInstr> flow;
        string jmpsto;
        string alternative;
    };

    class Refiner
    {
        stringstream buffer;
        deque<shared_ptr<BasicBlock>> listBB;
        map<string, shared_ptr<BasicBlock>> BB;
    public:
        template <class T>
        stringstream & operator<<(const T & s)
        {
            buffer << s;
            return buffer;
        }
        void reallocate()
        {
            BasicBlock tmp;
            while (!buffer.eof())
            {
                string inst;
                getline(buffer, inst);
                if (inst.back() == ':')
                {
                    if (tmp.label != "")
                    {
                        tmp.jmpsto = inst.substr(0, inst.length() - 1);
                        listBB.push_back(make_shared(new BasicBlock(tmp)));
                        BB[inst.substr(0, inst.length() - 1)] = listBB.back();
                        tmp = BasicBlock();
                        tmp.label = inst.substr(0, inst.length() - 1);
                        continue;
                    } else {
                        tmp.label = inst.substr(0, inst.length() - 1);
                        continue;
                    }
                }
                string op;
                stringstream sin;
                sin << inst;
                sin >> op;
                getline(sin, inst);
                inst.erase(inst.begin(), inst.begin() + 1);
                deque<string> args;
            }
        }
        string str()
        {
            reallocate();
            return buffer.str();
        }
        void flush()
        {
            buffer.flush();
        }
    };


    enum IRType
    {
        IR__error,
        IR__loop_start,
        IR__loop_constraint,
        IR__loop_body,
        IR__loop_iter,
        IR__get_size,
        IR__loop_end,
        IR__func,
        IR__assign,
        IR__arg,
        IR__var,          //var   width, name
        IR__cond,         //cond  var
        IR__cond_else,
        IR__word,
        IR__byte,
        IR__quad,
        IR__cond_end,
        IR__continue,
        IR__break,
        IR__add,
        IR__land,
        IR__lor,
        IR__band,
        IR__bor,
        IR__xor,
        IR__not,
        IR__lnot,
        IR__mod,
        IR__sub,
        IR__mul,
        IR__div,
        IR__shl,
        IR__shr,
        IR__less,
        IR__leq,
        IR__inc,
        IR__dec,
        IR__eq,
        IR__neq,
        IR__greater,
        IR__geq,
        IR__ret,          //ret   value/<keyword null>
        IR__call,         //call  ret_v, func_name, arg0, arg1, arg2, ...
        IR__malloc         //malloc var, width  //Address is saved in rax
    };
    struct IR
    {
        IRType oprand;
        deque<string> args;
        IR(IRType _oprand = IR__error) : oprand(_oprand) {}
        void concrete_interprete();
        string get_mess();
        static string repr_IRType(IRType p);
        friend ostream & operator<<(ostream &, const IR & s);
    };
    ostream & operator<<(ostream &, const IR & s);
    class _Translator
    {
        //IR var
        friend class _Compiler;
        friend class _Optimizer;
        _Parser & Parser;
        deque<IR> intermediate_repr;
        deque<IR> init_repr;
        unordered_map<string, set<string>> member_function_table;
        int calculation_cached_local_var_number = 0;
        int class_offset_base;
        deque<int> local_var_size;
        unordered_map<string, stack<int>> local_remap;
        unordered_map<string, string> argument_remap;
        stack<deque<string>> current_scope;
        set<string> need_construction;
        unordered_map<string, unordered_map<string, int>> class_member_offset;
        string context_information;
        string context_detail;
        string scope_title;
        int if_count = 0;
        string obj_title = "null";
        int loop_count = 0;
        set<string> special_construction;
        //assembler var
    public:
        static string __sys_lib;
        class StorageBase
        {
        public:
#ifndef __infinity__reg__assumption
            static ostream * sout;
#else
            static Refiner * sout;
#endif
        public:
            template <class T>
            static void set_sout(T & fout)
            {
                sout = &fout;
            }
            virtual string name_as_src(string tmp_reg = "") = 0;
            virtual string name_as_tg() = 0;
            virtual void addonto(string tmp_reg) = 0;
            virtual ~StorageBase() {}
        };
        class RegisterModel : public StorageBase
        {
        public:
            string name;
            RegisterModel(const string & _name)
                : name(_name) {}
            string name_as_src(string) { return name; }
            string name_as_tg() { return name; }
            void addonto(string tmp_reg)
            {
#ifndef __infinity__reg__assumption
                ostream & fout(*(this -> sout));
#else
                Refiner & fout(*(this -> sout));
#endif
                fout << "\tadd\t" << tmp_reg << ", " << name_as_tg() << '\n';
            }
        };
        long long global_stack_offset = 0;
        class MemoryModel : public StorageBase
        {
        public:
            string base;
            string offset;
            MemoryModel(const string & _base, const string & _offset)
                : base(_base), offset(_offset) {}
            string name_as_src(string tmp_reg = "rax");
            string name_as_tg()
            {
                return "qword [" + base + " " + offset + "]";
            }
            void addonto(string tmp_reg)
            {
#ifndef __infinity__reg__assumption
                ostream & fout(*(this -> sout));
#else
                Refiner & fout(*(this -> sout));
#endif
                fout << "\tadd\t" << tmp_reg << ", " << name_as_tg() << '\n';
            }
        };
        unordered_map<string, unordered_map<string, shared_ptr<StorageBase>>> storage_map;
        unordered_map<string, int> global_remap;
        shared_ptr<StorageBase> parse_symbol(const string & block_title, const string & s, const string & reg = "r10");

    public:
        _Translator(_Parser & __Parser);
        void proc_new(ASTNode & tree);
        void add_member(ASTNode & tree, NameController & name_table);

        void type_reload(NameController & name_table);

        void translate(ASTNode & tree, NameController & name_table);
        void print(ostream & fout);

        void execute();
        static const string & get_library()
        {
            return __sys_lib;
        }
        void to_amd64_linux_asm(ostream & fout);
    };
}
#endif // TRANSLATOR_HPP
