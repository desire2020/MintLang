#ifndef PARSER_HPP
#define PARSER_HPP
#include <memory>
#include <iostream>
#include <string>
#include <stack>
#include <queue>
#include "scanner.hpp"
using namespace std;
namespace msharp
{
    class NameController;
    class ScopeTypeTable;
    enum SyntaxDesc
    {
        node_phrase,
            node_dual_def,
            node_var_def,
                node_initexpr,
                    node_name, node_op,
            node_constructor_def,
            node_fun_def,
                node_fun_signature,
                    node_sing_para,
            node_class_def,
                node_member_list,
            node_type,
            node_expr,
                node_closure,
                    node_keyword_flow,
                    node_expr_tree, node_expr_single_line,
                        node_const_int, node_const_str, node_func_call, node_ambiguity_handler, node_func_call_para, node_index_para,
        unknown_node
    };
    enum ControlFlow
    {
        __if,
        __for,
        __while,
        __continue,
        __break,
        __return,
        __new,
        __else,
        __unknown_flow
    };
    enum OperatorLabel
    {
        __assign,       //=
        __equal_to,     //==
        __not_equal_to, //!=
        __less,         //<
        __greater,      //>
        __less_equal,   //<=
        __greater_equal,//>=
        __logical_and,  //&&
        __logical_or,   //||
        __bit_xor,      //^
        __bit_and,      //&
        __bit_or,       //|
        __shr,          //>>
        __shl,          //<<
        __add,         //+
        __sub,        //-
        __modulus,      //%
        __multiplies,   //*
        __divides,      ///
        __bit_not,      //~
        __logical_not,  //!
        __inc,          //++
        __dec,          //--
        __index,        //[]
        __get_member,   //.
        __new_op,
        __unknown_op
    };

    extern map<SyntaxDesc, string> ASTNodeName;
    extern int lex_pos;
    class ASTNode;
    typedef deque<ASTNode> Leaves;
    extern stack<ASTNode *> inStack;
#define addSon(a) son.push_back(ASTNode(a, lex, (ASTNode *)this))
#define addKeyword(x) if (s == #x) return __##x
#define addRef(op, label) if (s == #op) return __##label
#define getRef(label) __##label
    class InferredType;
    class ASTNode
    {
        static ControlFlow control_flow_label(string s);
        static OperatorLabel operator_label(string s);
        //Dinner time!
        //Let the operator eat whatever it wants!
        void expression_construction(const string & opr, ASTNode * _father);
        friend class _Translator;

        InferredType __check_type(NameController & name_table);
    public:
        int start_idx;
        SyntaxDesc type;
        Leaves son;
        List & lex;
        ASTNode * father;
        deque<string> translation_tag;
        string description;
        shared_ptr<InferredType> inferred_type = nullptr;
        shared_ptr<ScopeTypeTable> scoped_namespace;

        //Optimization Flags
        bool is_linear_expr = false;
        unordered_map<string, long long> linear_coeff;
        void linear_expression_compression(NameController & name_table);
        void logical_expression_compression();
        void rebuild_tree(NameController &name_table);
        set<string> get_name();
        void loop_truncation(NameController & name_table);
        void prefix_reorder();
        void if_phrase_enlarge();
        static map<string, ASTNode> inlinable_function_table;
        void inline_func_extraction();
        ASTNode get_copy();
        deque<string> local_args_name;
        ASTNode inline_tree(deque<ASTNode> & args, int __start_idx);
        void do_inlination();
        void dead_code_elimination();
        void constant_fold();

        ASTNode(SyntaxDesc _type, deque<Token> & _lex, ASTNode * _father = NULL, string _des = "", int _idx = -1)
            : start_idx(_idx), type(_type), lex(_lex), father(_father), description(_des) {}
        ASTNode(const ASTNode & be_copied) = delete;
        ASTNode(ASTNode && be_moved) = default;
        ASTNode & operator=(ASTNode && be_moved);

        void print_tree(std::ostream & fout, int space = 0);
        ASTNode make_node(int idx);
        void accomplish();
        void remap_constant_string(NameController & name_table);
        void accomplish_closure(NameController & name_table);
        InferredType check_type(NameController & name_table);
    };
#define addName(x) ASTNodeName[x] = string(#x).substr(5)
    class TypeDescriptor
    {
    public:
        bool _ensembled = false,
             _class = false,
             _const = false,
             _constexpr = false,
             _callable = false,
             _var = false;
        int start_idx;
        static TypeDescriptor type_class(int idx = 0);
        static TypeDescriptor type_func(int idx = 0);
        static TypeDescriptor type_const_var(int idx = 0);
        static TypeDescriptor type_literal_var(int idx = 0);
        static TypeDescriptor type_var(int idx = 0);
        static TypeDescriptor type_built_in_func();

        static TypeDescriptor type_verified_ensembler(int idx = 0);
        static TypeDescriptor type_invalid();
        bool operator<=(const TypeDescriptor & rhs) const;
        bool operator==(const TypeDescriptor & rhs) const;
        bool invalid() const;
    };
    class ScopeTypeTable;
    class ClassDescriptor;
    class FunctionDescriptor
    {
    public:
        string name;
        ASTNode * ref_node = nullptr;
        string result_signature;
        static FunctionDescriptor global_function(ASTNode & node);
        static FunctionDescriptor constructor(ASTNode & node);
    };
    class ClassDescriptor
    {
    public:
        int byte_num;
        ASTNode * node_ref = NULL;
        static ClassDescriptor basic_type(int _byte_num);
        static ClassDescriptor actual_class(ASTNode & node);
    };
    class InferredType
    {
    public:
        TypeDescriptor abstract_type;
        string basic_sign;
        int array_dimension = 0;
        static InferredType unused_name();
        static InferredType lvalue(string _basic_sign, int _array_dimension = 0);
        static InferredType rvalue(string _basic_sign, int _array_dimension = 0);
        static InferredType built_in_func(string _basic_sign);
        static InferredType hash_para(const deque<InferredType> & para);

        static InferredType ensembler(string _description = "");
        static InferredType ambiguity_handler();
        bool ambi() const;
        InferredType array_wrap() const;
        InferredType de_array() const;
        bool is_indexable() const;
        bool basic_type() const;
        bool is_arith() const;
        bool operator<=(const InferredType & rhs) const;
        string signature() const;
        bool operator==(const InferredType & rhs) const;
    };

    class ScopeTypeTable
    {
    public:
        unordered_map<string, TypeDescriptor> _abstract_type;
        unordered_map<string, ClassDescriptor> _class_def;
        unordered_map<string, FunctionDescriptor> _func_def;
        unordered_map<string, InferredType> _var_def;
        unordered_map<string, string> _const_def;
        unordered_map<string, string> function_sign;
        unordered_map<string, string> function_ret;
        string control_flow = "";
        string func_title = "";
        string class_title = "";
        void clear();
        void in_func(string _title);
        void in_class(string _title);
        void add_class(string _name, int byte_num);
        void add_class(ASTNode & node);
        void add_func(ASTNode & node);
        void plainize();
        void add_cons(ASTNode & node);
        void add_const(string _name, string _value, InferredType type);
        void add_var(string _name, string _value, const InferredType & type);
        void add_var(ASTNode & node, const InferredType & type);
        void set_flow(string s);
        InferredType fetch(string _name, int start_idx = 0);
        ~ScopeTypeTable()
        {
        }
    };



    class NameController
    {    
        deque<ScopeTypeTable *> scope;
        unordered_map<string, int> constant_string_remapping;
        deque<string> constant_string;
        friend class _Translator;
    public:
        static ScopeTypeTable global_table;
        static InferredType var_type_fetch(int idx, string _name, bool plain = false);
        static InferredType fun_type_fetch(string _name);
#define addBuiltInFunc(type, name, sign) global_table._abstract_type[#name] = TypeDescriptor::type_func(); add_func_sign(#name, sign); add_func_type(#name, #type)
#define addBuiltInMemb(father, type, name, sign) add_func_sign(string("__") + #father + string("__") + #name + string("__"), sign); add_func_type(string("__") + #father + string("__") + #name + string("__"), #type)
        NameController()
        {
            scope.push_back(&global_table);
            global_table.clear();
            global_table.add_class("int", 8);
            global_table.add_class("bool", 8);
            global_table.add_class("string", 8);
            global_table.add_class("void", 0);
            global_table.add_class("__null", 8);
            add_const("bool", "true", "1");
            add_const("bool", "false", "0");
            add_const("__null", "null", "0");
            addBuiltInMemb(array, int, size, "");
            addBuiltInMemb(string, int, ord, "int ");
            addBuiltInMemb(string, int, length, "");
            addBuiltInMemb(string, int, parseInt, "");
            addBuiltInMemb(string, string, substring, "int int ");
            addBuiltInFunc(void, print, "string ");
            addBuiltInFunc(void, println, "string ");
            addBuiltInFunc(string, getString, "");
            addBuiltInFunc(int, getInt, "");
            addBuiltInFunc(string, toString, "int ");
        }
        ~NameController()
        {
            if (!is_in(string("main"), global_table.function_ret))
                Error.compile_error("Fatal error : function main not found.");
            if (is_in(string("main"), global_table.function_ret) && global_table.function_ret["main"] != "int")
                Error.compile_error("Fatal error : function main should return an integer.");
        }
        void in_func(string title);
        void in_class(string title);
        void plainize_current_scope();
        string get_func_title();
        string get_class_title();
        int get_constant_string_idx(const string & ss);
        const string & get_constant_string(int idx) const;
        bool is_global() const;
        static bool match(const string & s1, const string & s2);
        string fetch_function_sign(string func);
        void print_constant_table(ostream & fout);
        bool check_actual_sign(string std_sign, string _sign);
        bool check_sign(string func, string _sign);
        void add_func_type(string name, string type_sign);
        void add_func_sign(string name, string sign);
        bool in_flow(string s);
        string get_fun_ret(string s);
        void set_flow(string s);
        void add_class(string _name, int byte_num);
        InferredType fetch_in_current_scope(string _name);
        InferredType fetch(string _name, int idx = 0);
        void move_into_scope(ScopeTypeTable * new_scope);
        ClassDescriptor fetch_class(string _name);
        void leave_scope();
        void add_class(ASTNode & node);
        void add_cons(ASTNode & node);
        void add_func(ASTNode & node);
        void add_const(string _type, string _name, string _value);
        void add_var(string _type, string _name, string _value);
        void add_var(ASTNode & node);
        void add_plain_var(ASTNode & node);
    };

#define addName(x) ASTNodeName[x] = string(#x).substr(5)
    class _Parser
    {
        friend class _Compiler;
        friend class _Translator_M2M;
        friend class _Optimizer;
        List & lexemes;

    public:
        deque<ASTNode> forest;
        NameController name_table;
        static bool safe_to_be_wrapped;
        _Parser(List & _lex)
            : lexemes(_lex)
        {
            addName(node_phrase);
            addName(node_dual_def);
            addName(node_var_def);
            addName(node_initexpr);
            addName(node_name);
            addName(node_fun_def);
            addName(node_keyword_flow);
            addName(node_func_call_para);
            addName(node_fun_signature);
            addName(node_sing_para);
            addName(node_constructor_def);
            addName(node_expr_single_line);
            addName(node_class_def);
            addName(node_member_list);
            addName(node_op);
            addName(node_type);
            addName(node_ambiguity_handler);
            addName(node_expr);
            addName(node_closure);
            addName(node_expr_tree);
            addName(node_const_int);
            addName(node_const_str);
            addName(node_func_call);
        }
        void check_bracket_pair();

        void construct_CST();

        void class_declare();
        void deep_declare();
        void remap_constantstring();
        static bool check(ASTNode & in, ASTNode * fat);
        void type_validation();
        void do_AST_level_optimization();
        void check_conn();

        //AST level optimizer
        void LinearExprFolder(ASTNode & tree);
    };
}
#undef addName
#endif // PARSER_HPP
