#include "parser.hpp"
namespace msharp
{
    stack<ASTNode *> inStack;
    map<SyntaxDesc, string> ASTNodeName;
    int lex_pos = 0;
    bool _Parser::safe_to_be_wrapped = true;
    void ASTNode::accomplish_closure(NameController &name_table)
    {
        switch (type)
        {
        case node_class_def:
            scoped_namespace = make_shared(new ScopeTypeTable);
            name_table.move_into_scope(scoped_namespace.get());
            name_table.add_var(son.front().description, "this", "this");
            name_table.in_class(son.front().description);
        break;
        case node_keyword_flow:
            scoped_namespace = make_shared(new ScopeTypeTable);
            name_table.move_into_scope(scoped_namespace.get());
            name_table.set_flow(description);
        break;
        case node_closure:
            if (father -> type != node_fun_def)
            {
                scoped_namespace = make_shared(new ScopeTypeTable);
                name_table.move_into_scope(scoped_namespace.get());
            } else {
                scoped_namespace = father -> scoped_namespace;
                name_table.move_into_scope(scoped_namespace.get());
            }
        break;
        case node_constructor_def:
            name_table.set_flow("constructor");
            name_table.add_cons(*this);
            scoped_namespace = make_shared(new ScopeTypeTable);
            name_table.move_into_scope(scoped_namespace.get());
            if (son[0].description != name_table.get_class_title())
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : constructor name mismatch.");
        break;
        case node_fun_def:
        {
            name_table.set_flow("function");
            name_table.add_func(*this);
            scoped_namespace = make_shared(new ScopeTypeTable);
            name_table.move_into_scope(scoped_namespace.get());
            name_table.in_func(son[1].description);

        }
        break;
        case node_var_def: case node_sing_para:
        {
            if (father == NULL || father -> type != node_member_list)
                name_table.add_var(*this);
            else
                name_table.add_plain_var(*this);
        }
        break;

        default:
        break;
        }
        for (ASTNode & ason : son)
        {
            ason.accomplish_closure(name_table);
        }
        switch (type)
        {
        case node_fun_def:
        {
            deque<InferredType> son_type;
            for (int i = 0; i < int(son.size()); ++i)
            {
                ASTNode & ason = son[i];
                //cout << "validating " << ASTNodeName[type] << " -> " << ASTNodeName[ason.type] << endl;
                if (i > 2 && type == node_fun_def)
                {
                    name_table.add_func_type(son_type[1].basic_sign, son_type[0].signature());
                    if (!son_type[0].abstract_type._class)
                        Error.compile_error("Syntax Error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid return type.");
                    name_table.add_func_sign(son_type[1].basic_sign, son_type[2].basic_sign);
                    break;
                }
                son_type.push_back(ason.check_type(name_table));
            }
        }
        case node_closure:
        case node_class_def:
        case node_keyword_flow: case node_constructor_def:
            name_table.leave_scope();
        break;
        default:
        break;
        }
    }

    InferredType ASTNode::check_type(NameController & name_table)
    {
        if (inferred_type == nullptr)
        {
            inferred_type = make_shared(new InferredType(__check_type(name_table)));
            return *inferred_type;
        } else {
            return *inferred_type;
        }
    }
    InferredType ASTNode::__check_type(NameController & name_table)
    {
        switch (type)
        {
        case node_class_def:
        case node_closure: case node_keyword_flow:
        case node_fun_def:
            name_table.move_into_scope(scoped_namespace.get());
        break;
        default:
        break;
        }
        deque<InferredType> son_type;
        if (type == node_expr_tree && description == ".")
        {
            if (son[1].type != node_name)
            {
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of operator. .");
            }
            InferredType first_son = son.front().check_type(name_table);
            if (first_son.array_dimension > 0)
            {
                if (son[1].description != "size")
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : array object doesn't have member " + son[1].description + ".");
                return InferredType::built_in_func("__array__size__");
            }
            if (first_son.basic_sign == "string")
            {
                if (son[1].description == "length" ||
                    son[1].description == "substring" ||
                    son[1].description == "parseInt" ||
                    son[1].description == "ord")
                    return InferredType::built_in_func(string("__string__") + son[1].description + "__");
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : string object doesn't have member " + son[1].description + ".");
            }

            name_table.move_into_scope(name_table.fetch_class(first_son.basic_sign).node_ref -> scoped_namespace.get());
            name_table.plainize_current_scope();
            InferredType r = son[1].check_type(name_table);

            r.abstract_type._const = false;
            r.abstract_type._constexpr = false;
            r.abstract_type._var = true;
            if (r.abstract_type._callable)
                r.basic_sign = first_son.basic_sign + " " + r.basic_sign;
            name_table.leave_scope();
            return r;
        } else
        for (int i = 0; i < int(son.size()); ++i)
        {
            ASTNode & ason = son[i];
            //cout << "validating " << ASTNodeName[type] << " -> " << ASTNodeName[ason.type] << endl;
           /* if (i > 2 && type == node_fun_def)
            {
                name_table.add_func_type(son_type[1].basic_sign, son_type[0].signature());
                if (!son_type[0].abstract_type._class)
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid return type.");
                name_table.add_func_sign(son_type[1].basic_sign, son_type[2].basic_sign);
            }*/
            son_type.push_back(ason.check_type(name_table));
        }
        if (son_type.empty())
            son_type.push_back(InferredType::ensembler());
        switch (type)
        {
        case node_const_int:
        {
            InferredType ret;
            ret.abstract_type._const = true;
            ret.abstract_type._constexpr = true;
            ret.abstract_type._var = true;
            ret.basic_sign = "int";
            return ret;
        }
        break;
        case node_const_str:
        {
            InferredType ret;
            ret.abstract_type._const = true;
            ret.abstract_type._constexpr = true;
            ret.abstract_type._var = true;
            ret.basic_sign = "string";
            return ret;
        }
        break;
        case node_type:
        {
            auto r = name_table.fun_type_fetch(description);
            return r;
        }
        break;
        case node_var_def: case node_expr_single_line: case node_sing_para: case node_expr:
            return son_type[0];
        break;
        case node_initexpr: case node_member_list:
            return InferredType::ensembler();
        break;
        case node_fun_def:
        break;
        case node_constructor_def:
            name_table.add_func_sign(son_type[0].basic_sign, son_type[1].basic_sign);
        break;
        case node_class_def:
            name_table.add_func_type(son_type[0].basic_sign, son_type[0].basic_sign);
        break;
        case node_closure:
        break;
        case node_func_call:
        {
            InferredType & func = son_type[0];
            if (!func.abstract_type._callable)
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid call attempt to a non-callable object.");
            if (!func.abstract_type._var)
            {
                if (!name_table.check_sign(func.basic_sign, son_type[1].basic_sign))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid call to a callable object : invalid parameter list.");
                if (func.basic_sign == "getInt" || func.basic_sign == "getString") _Parser::safe_to_be_wrapped = false;
                return InferredType::rvalue(name_table.get_fun_ret(func.basic_sign));
            } else {
                int s;
                for (s = 0; func.basic_sign[s] != ' '; ++s);
                string class_name = func.basic_sign.substr(0, s);
                string func_name = func.basic_sign.substr(s + 1);
                name_table.move_into_scope(name_table.fetch_class(class_name).node_ref -> scoped_namespace.get());
                if (!name_table.check_sign(func_name, son_type[1].basic_sign))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid call to a member callable object : invalid parameter list.");
                auto r = InferredType::rvalue(name_table.get_fun_ret(func_name));
                name_table.leave_scope();
                return r;
            }
        }
        break;
        case node_func_call_para: case node_fun_signature:
            return InferredType::hash_para(son_type);
        break;
        case node_keyword_flow:
        {

            switch (control_flow_label(description))
            {
            case __else: case __new: case __unknown_flow:
                Error.compile_error("Fatal error : parser fucked up.");
            break;
            case __continue: case __break:
                if (!(name_table.in_flow("for") || name_table.in_flow("while")))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : control flow redirector occurs before any loop occurs.");
            break;
            case __return:
            {
                if (!(name_table.in_flow("function")))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : control flow redirector occurs before any func call already exists.");
                string func_re = name_table.get_func_title();
                func_re = name_table.get_fun_ret(func_re);
                if (func_re == "void")
                {
                    if (son.size() != 0)
                        Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : procedure doesn't need return values.");
                    name_table.leave_scope();
                    return InferredType::ensembler();
                }
                if (name_table.match(func_re, son_type[0].signature()))
                {
                    name_table.leave_scope();
                    return InferredType::ensembler();
                }
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : return value type check failed.");
            }
            break;
            case __for:
            {
                InferredType & second_son = son_type[1];
                if (second_son.basic_sign != "bool" || second_son.array_dimension != 0)
                {
                    if (!son[1].son.empty())
                        Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : the constraint of for-loop must be a bool expression.");
                }
            }
            break;
            case __while: case __if:
            {
                InferredType & first_son = son_type[0];
                if (first_son.basic_sign != "bool" || first_son.array_dimension != 0)
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : the constraint must be a bool expression.");
            }
            break;/*
            default:
                return InferredType::ensembler();*/
            }
            name_table.leave_scope();
            return InferredType::ensembler();
        }
        break;
        case node_ambiguity_handler:
        {
            return InferredType::ambiguity_handler();
        }
        break;
        case node_name:
        {
            auto r = name_table.fetch(description, start_idx);
            //r.basic_sign = description;
            if (r.abstract_type.invalid())
            {
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : undefined name \"" + description + "\" occurs.");
            }
            return r;
        }
        break;
        case node_expr_tree:
        {
            InferredType & first_son = son_type[0];
            InferredType & second_son = son_type[1];
            switch (operator_label(description))
            {
            case __assign:
            case __equal_to:
            case __not_equal_to:
            case __less:
            case __greater:
            case __less_equal:
            case __greater_equal:
            case __logical_and:
            case __logical_or:
            case __bit_and:
            case __bit_or:
            case __bit_xor:
            case __shr:
            case __shl:
            case __modulus:
            case __multiplies:
            case __divides:
            case __get_member:
                if (first_son.ambi() || second_son.ambi())
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : operator" + description + " needs 2 parameters.");
            break;
            case __inc: case __dec:
                if (!(first_son.ambi() ^ second_son.ambi()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : operator" + description + " needs exactly one parameter.");
            break;
            case __index:
                if ((first_son.ambi() && second_son.ambi()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : operator" + description + " needs exactly one parameter.");
            break;
            case __logical_not: case __bit_not: case __add: case __sub: case __new_op:
                if (second_son.ambi())
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : operator" + description + " needs an appendix parameter.");
            break;
            case __unknown_op:
                Error.compile_error("Fatal error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : operator" + description + " parser must've fucked up.");
            break;
            }
            switch (operator_label(description))
            {
            case __assign:
                if (first_son <= second_son)
                    return first_son;
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails when assigning.");
                return InferredType::unused_name();
            break;
            case __logical_and: case __logical_or:
                if ((!(first_son.basic_type() && first_son.basic_sign == "bool")) || (!(second_son.basic_type() && second_son.basic_sign == "bool")))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : unimplemented operator for non-bool type.");
                if (first_son == second_son)
                    return InferredType::rvalue("bool");
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails when trying to do comparing.");
                return InferredType::unused_name();
            break;
            case __new_op:
                return InferredType::rvalue(second_son.signature());
            break;
            case __less: case __greater: case __greater_equal: case __less_equal:
                if ((!first_son.basic_type()) || (!second_son.basic_type()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : unimplemented operator for non-basic type.");
                if (first_son == second_son)
                    return InferredType::rvalue("bool");
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails when trying to do comparing.");
                return InferredType::unused_name();
            break;
            case __bit_and: case __bit_xor: case __bit_or:
                if ((!first_son.basic_type()) || (!second_son.basic_type()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : unimplemented operator for non-basic type.");
                if (first_son == second_son)
                    return InferredType::rvalue(first_son.signature());
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails when trying to do comparing.");
                return InferredType::unused_name();
            break;
            case __shr: case __shl: case __modulus: case __multiplies: case __divides:
                if ((!first_son.basic_type()) || (!second_son.basic_type()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : unimplemented operator for non-basic type.");
                if (first_son == second_son)
                    return InferredType::rvalue("int");
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails when trying to do comparing.");
                return InferredType::unused_name();
            break;
            case __equal_to: case __not_equal_to:
                if (first_son == second_son)
                    return InferredType::rvalue("bool");
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid comparison between these expressions.");
                return InferredType::unused_name();
            break;
            case __add: case __sub:
                if (first_son.ambi())
                {
                    if (second_son.is_arith())
                        return InferredType::rvalue(second_son.signature());
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of prefix operator.");
                    return InferredType::unused_name();
                }
                if ((!first_son.basic_type()) || (!second_son.basic_type()))
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : unimplemented operator for non-basic type.");
                if (first_son == second_son)
                    return InferredType::rvalue(first_son.signature());
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : type validation fails.");
                return InferredType::unused_name();
            break;
            case __logical_not:
                if (second_son.basic_type() && second_son.basic_sign == "bool")
                    return InferredType::rvalue(second_son.basic_sign);
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of prefix operator!.");
                return InferredType::unused_name();
            break;
            case __bit_not:
                if (second_son.basic_type())
                    return InferredType::rvalue(second_son.basic_sign);
                return InferredType::rvalue(second_son.basic_sign);
            break;
            case __inc: case __dec:
                if (first_son.ambi())
                {
                    if (second_son.is_arith() && !second_son.abstract_type._const)
                        return InferredType::lvalue(second_son.basic_sign);
                } else {
                    if (second_son.ambi() && first_son.is_arith() && !first_son.abstract_type._const)
                        return InferredType::rvalue(first_son.basic_sign);
                }
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of prefix & appendix operator ++/--.");
                return InferredType::unused_name();
            break;
            case __get_member:
            {
                Error.compile_error("Fatal error : parser fucked up when parsing operator get-member.");
                return InferredType::unused_name();
            }
            break;
            case __unknown_op:
            {
                Error.compile_error("Fatal error : parser fucked up.");
                return InferredType::unused_name();
            }
            break;
            case __index:
            {
                if (!first_son.is_indexable())
                    Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of index operator[].");
                if (first_son.abstract_type._class)
                {
                    if (second_son.is_arith() || second_son.ambi())
                    {
                        return first_son.array_wrap();
                    }
                } else {
                    return first_son.de_array();
                }
            }
            }
        }
        break;
        case node_phrase: case node_dual_def: case node_op: case unknown_node: case node_index_para:
            Error.compile_error("Fatal error : parse fucked up when dealing " + ASTNodeName[type] + ".");
            return InferredType::unused_name();
        break;
        }
        switch (type)
        {
        case node_class_def: case node_closure: case node_fun_def: case node_keyword_flow:
            name_table.leave_scope();
        break;
        default:
        break;
        }
        return InferredType::ensembler();
    }
    void ASTNode::rebuild_tree(NameController & name_table)
    {
        son.clear();
        for (auto & s : linear_coeff)
        {
            if (s.first == "#constant#")
            {
                son.push_back(ASTNode(node_const_int, lex, (ASTNode *)this, Tool.to_string(s.second)));
            }
            else if (s.second == 0)
                continue;
            else if (s.second == 1)
            {
                son.push_back(ASTNode(node_name, lex, &son.back(), s.first, start_idx));
            }
            else if (s.second == -1)
            {
                son.push_back(ASTNode(node_expr_tree, lex, (ASTNode *)this, "-"));
                son.back().son.push_back(ASTNode(node_const_int, lex, &son.back(), "0"));
                son.back().son.push_back(ASTNode(node_name, lex, &son.back(), s.first, start_idx));
            }
            else
            {
                son.push_back(ASTNode(node_expr_tree, lex, (ASTNode *)this, "*"));
                son.back().son.push_back(ASTNode(node_name, lex, &son.back(), s.first, start_idx));
                son.back().son.push_back(ASTNode(node_const_int, lex, &son.back(), Tool.to_string(s.second)));
            }
        }
        while (son.size() > 2)
        {
            son.push_back(ASTNode(node_expr_tree, lex, (ASTNode *)this, "+"));
            son.back().son.push_back(std::move(son[0]));
            son.back().son.push_back(std::move(son[1]));
            son.pop_front(); son.pop_front();
        }
        _Parser::check(*this, father);
        for (ASTNode & tree : son) tree.check_type(name_table);
        if (son.size() == 1)
        {
            ASTNode tmp = std::move(son[0]);
            operator=(std::move(tmp));
        }
        while (((son.size() > 0 && son[0].description == "0") || (son.size() > 1 && son[1].description == "0")) && (description == "+" || description == "-"))
        {
            while (son.size() > 0 && son[0].description == "0")
            {
                ASTNode tmp = std::move(son[1]);
                operator=(std::move(tmp));
            }
            while (son.size() > 1 && son[1].description == "0")
            {
                ASTNode tmp = std::move(son[0]);
                operator=(std::move(tmp));
            }
        }
    }

    set<string> ASTNode::get_name()
    {
        set<string> ret;
        if (type == node_name)
        {
            ret = {description};
            return ret;
        }
        for (ASTNode & ason : son)
        {
            set<string> dict = ason.get_name();
            for (const string & _name : dict)
                ret.insert(_name);
        }
        return ret;
    }

    void ASTNode::loop_truncation(NameController & name_table)
    {
        for (ASTNode & ason : son) ason.loop_truncation(name_table);
        switch (type)
        {
        case node_keyword_flow:
        {
            auto keyword_label = ASTNode :: control_flow_label(description);
            switch (keyword_label)
            {
            case __for:
            {
                if (son[3].son.empty()) return;
                if (son[3].son.front().type == node_closure)
                {
                    if (son[3].son.front().son.size() != 1) return;
                    ASTNode & body(son[3].son.front().son.front());
                    if (body.son.empty()) return;
                    if (body.son.front().type == node_keyword_flow && body.son.front().description == "if")
                    {
                        if (body.son.front().son.size() == 2)
                        {
                            set<string> static_required_name = body.son.front().son.front().get_name();
                            set<string> changed_name = son[1].son.front().get_name();
                            for (const auto & s : static_required_name)
                            {
                                if (s == "true" || s == "false" || s == "null") continue;
                                if (changed_name.count(s) != 0) return;
                            }
                            if (!son[1].son.empty())
                            {
                                ASTNode loop_constraint = std::move(son[1].son.front());
                                son[1].son.pop_front();
                                ASTNode constraint = std::move(body.son.front().son.front());
                                body.son.front().son.pop_front();
                                body.son.front().son.push_front(ASTNode(node_name, lex, constraint.father, "true", constraint.start_idx));
                                ASTNode new_loop_constraint(node_expr_tree, lex, &(son[1]), "&&", this -> start_idx);
//                                std::cerr << "\n";
//                                loop_constraint.print_tree(std::cerr, 0);
//                                std::cerr << "\n";
//                                constraint.print_tree(std::cerr, 0);
//                                std::cerr << "\n";
                                new_loop_constraint.son.push_back(std::move(constraint));
                                new_loop_constraint.son.push_back(std::move(loop_constraint));
//                                new_loop_constraint.print_tree(std::cerr, 0);
//                                Error.compile_error("\ndetected!");
                                son[1].son.emplace_front(std::move(new_loop_constraint));
                            }
                        }
                    }

                }
            }
            break;
            default:

            break;
            }
        }
        break;
        default:
        break;
        }
    }
    map<string, ASTNode> ASTNode :: inlinable_function_table;
    void ASTNode::prefix_reorder()
    {
        for (ASTNode & ason : son) ason.prefix_reorder();
        if (type == node_expr_tree)
        {
            if ((father -> type) == node_expr_single_line || ((father -> type) == node_keyword_flow && (father -> description == "for")))
            if (description == "++" || description == "--")
            {
                if (son[1].type == node_ambiguity_handler)
                    swap(son[0], son[1]);
            }
        }
    }

    void ASTNode::if_phrase_enlarge()
    {
        for (ASTNode & ason : son) ason.if_phrase_enlarge();
        if (type == node_keyword_flow)
        {
            if (description == "if")
            {
                while (son[0].description == "&&" && son.size() == 2)
                {
                    ASTNode & cons = son[0];
                    ASTNode left = std::move(cons.son[0]);
                    ASTNode right = std::move(cons.son[1]);
                    son[0] = std::move(left);
                    ASTNode branch = std::move(son[1].son[0]);
                    ASTNode new_phrase = ASTNode(node_keyword_flow, lex, nullptr, "if", start_idx);
                    ASTNode new_branch = ASTNode(node_expr, lex, nullptr, "", start_idx);
                    new_branch.son.push_back(std::move(branch));
                    new_phrase.son.push_back(std::move(right));
                    new_phrase.son.push_back(std::move(new_branch));
                    son[1].son[0] = std::move(new_phrase);
                }
            }
        }
    }

    void ASTNode::inline_func_extraction()
    {
        for (ASTNode & ason : son) ason.inline_func_extraction();
        if (type == node_fun_def)
        {
            if (son[3].son.empty()) return;
            if (son[3].son.size() != 1) return;
            if (son[3].son[0].son.size() != 1) return;
            ASTNode & target_phrase = son[3].son[0].son[0];
            if (target_phrase.type != node_keyword_flow) return;
            if (target_phrase.description != "return") return;
            for (ASTNode & paran : son[2].son)
            {
                for (const string & s : paran.son[1].get_name())
                    local_args_name.push_back(s);
            }
            inlinable_function_table.emplace(std::make_pair(son[1].description, get_copy()));
        }
    }

    ASTNode ASTNode::get_copy()
    {
        ASTNode ret(type, lex, nullptr, description, start_idx);
        ret.description = description;
        ret.inferred_type = inferred_type;
        ret.is_linear_expr = is_linear_expr;
        ret.linear_coeff = linear_coeff;
        ret.local_args_name = local_args_name;
        for (ASTNode & ason : son) ret.son.emplace_back(ason.get_copy());
        return std::move(ret);
    }
    void inline_replace(ASTNode & tg, deque<ASTNode> & args, map<string, int> & dict, int __start_idx)
    {
        if (tg.type == node_name)
        {
            if (is_in(tg.description, dict))
            {
                ASTNode tmpnode = std::move(args[dict[tg.description]].get_copy());
                tmpnode.start_idx = __start_idx;
                tg = std::move(tmpnode);
            }
        } else {
            for (ASTNode & ason : tg.son) inline_replace(ason, args, dict, __start_idx);
        }
    }

    ASTNode ASTNode::inline_tree(deque<ASTNode> & args, int __start_idx)
    {
        ASTNode ret_v = (son[3].son[0].son[0].son[0].son[0]).get_copy();
        map<string, int> replace_tag;
        for (int i = 0; i < local_args_name.size(); ++i)
            replace_tag[local_args_name[i]] = i;
        inline_replace(ret_v, args, replace_tag, __start_idx);
        ret_v.start_idx = __start_idx;
        return std::move(ret_v);
    }
    void resetidx(ASTNode & node, int start_idx)
    {
        node.start_idx = start_idx;
        for (ASTNode & tree : node.son) resetidx(tree, start_idx);
    }

    void ASTNode::do_inlination()
    {
        for (ASTNode & ason : son) ason.do_inlination();
        if (type == node_func_call)
        {
            if (son[0].type != node_name) return;
            if (!is_in(son[0].description, inlinable_function_table)) return;
            string title = son[0].description;
            deque<ASTNode> args;
            for (ASTNode & ason : son[1].son) args.emplace_back(std::move(ason));
            int copy_idx = start_idx;
            operator=(std::move(inlinable_function_table.at(title).inline_tree(args, start_idx)));
            resetidx(*this, copy_idx);
        }
    }
    void ASTNode::dead_code_elimination()
    {
        for (ASTNode & ason : son) ason.dead_code_elimination();
        if (type == node_expr_tree)
        {
            if (description == "=")
            {
                ASTNode & left = son[0];
                ASTNode & right = son[1];
                if (left.type == node_name && right.type == node_name)
                {
                    if (left.description == right.description)
                    {
                        ASTNode new_expr = std::move(left);
                        operator=(std::move(new_expr));
                    }
                }
            }
        }
        if (type == node_expr_single_line && father -> type == node_expr)
        {
            if (son.size() == 1 && (son[0].type == node_name || son[0].type == node_const_str || son[0].type == node_const_int))
            {
                son[0].type = node_const_int;
                son[0].description = "0";
            }
        }

    }

    void ASTNode::constant_fold()
    {
        for (ASTNode & ason : son) ason.constant_fold();
        if (type != node_expr_tree) return;
        if(son.size() > 1 && son[0].type == node_const_int && son[1].type == node_const_int)
        {
            long long lhs = Tool.to_int(son[0].description);
            long long rhs = Tool.to_int(son[1].description);
            auto p = operator_label(description);
            switch (p)
            {
            case __add:
                type = node_const_int;
                description = Tool.to_string(lhs + rhs);
            break;
            case __sub:
                type = node_const_int;
                description = Tool.to_string(lhs - rhs);
            break;
            case __multiplies:
                type = node_const_int;
                description = Tool.to_string(lhs * rhs);
            break;
            case __divides:
                type = node_const_int;
                description = Tool.to_string(lhs / rhs);
            break;
            case __modulus:
                type = node_const_int;
                description = Tool.to_string(lhs % rhs);
            break;
            case __shl:
                type = node_const_int;
                description = Tool.to_string(lhs << rhs);
            break;
            case __shr:
                type = node_const_int;
                description = Tool.to_string(lhs >> rhs);
            break;
            case __bit_and:
                type = node_const_int;
                description = Tool.to_string(lhs & rhs);
            break;
            case __bit_or:
                type = node_const_int;
                description = Tool.to_string(lhs | rhs);
            break;
            case __bit_xor:
                type = node_const_int;
                description = Tool.to_string(lhs ^ rhs);
            break;
            }
        }
    }

    void ASTNode::linear_expression_compression(NameController & name_table)
    {
        if (type == node_const_int)
        {
            is_linear_expr = true;
            linear_coeff["#constant#"] = Tool.to_int(description);
            return;
        }
        if (type == node_name)
        {
            is_linear_expr = true;
            linear_coeff[description] = 1;
            return;
        }
        if (type == node_ambiguity_handler)
        {
            is_linear_expr = true;
            return;
        }
        switch (type)
        {
        case node_expr_tree:
        break;
        case node_class_def:
        case node_closure: case node_keyword_flow:
        case node_fun_def:
            name_table.move_into_scope(scoped_namespace.get());
        default:
            for (ASTNode & tree : son) tree.linear_expression_compression(name_table);
        break;
        }

        if (type == node_expr_tree && (!is_linear_expr))
        {
            if (inferred_type -> basic_sign == "int" && inferred_type -> array_dimension == 0)
            {
                switch (operator_label(description))
                {
                case __assign:
                {
                    for (ASTNode & tree : son) tree.linear_expression_compression(name_table);
                }
                break;
                case __add:
                {
                    for (ASTNode & tree : son) tree.linear_expression_compression(name_table);
                    if (son[0].is_linear_expr && son[1].is_linear_expr)
                    {
                        for (auto & s : son[0].linear_coeff)
                            linear_coeff[s.first] += s.second;
                        for (auto & s : son[1].linear_coeff)
                            linear_coeff[s.first] += s.second;
                        rebuild_tree(name_table);
                        is_linear_expr = true;
                    }
                }
                break;
                case __sub:
                {
                    for (ASTNode & tree : son) tree.linear_expression_compression(name_table);
                    if (son[0].is_linear_expr && son[1].is_linear_expr)
                    {
                        description = "+";
                        for (auto & s : son[0].linear_coeff)
                            linear_coeff[s.first] += s.second;
                        for (auto & s : son[1].linear_coeff)
                            linear_coeff[s.first] -= s.second;
                        rebuild_tree(name_table);
                        is_linear_expr = true;
                    }
                }
                break;
                case __multiplies:
                {
                    for (ASTNode & tree : son) tree.linear_expression_compression(name_table);
                    if (son[0].is_linear_expr && son[1].type == node_const_int)
                    {
                        description = "+";
                        long long multiplier = Tool.to_int(son[1].description);
                        for (auto & s : son[0].linear_coeff)
                            linear_coeff[s.first] = s.second * multiplier;
                        rebuild_tree(name_table);
                        is_linear_expr = true;
                    } else {
                        if (son[1].is_linear_expr && son[0].type == node_const_int)
                        {
                            description = "+";
                            long long multiplier = Tool.to_int(son[0].description);
                            for (auto & s : son[1].linear_coeff)
                                linear_coeff[s.first] = s.second * multiplier;
                            rebuild_tree(name_table);
                            is_linear_expr = true;
                        }
                    }
                }
                break;
                default:
                break;
                }
            }
        }

        switch (type)
        {
        case node_class_def: case node_closure: case node_fun_def: case node_keyword_flow:
            name_table.leave_scope();
        break;
        default:
        break;
        }
    }

    void ASTNode::logical_expression_compression()
    {
        for (ASTNode & ason : son) ason.logical_expression_compression();
        if (type == node_expr_tree)
        {
            if (description == "&&")
            {
                if (son[0].description == "true")
                {
                    ASTNode right = std::move(son[1]);
                    operator=(std::move(right));
                } else {
                    if (son[1].description == "true")
                    {
                        ASTNode left = std::move(son[0]);
                        operator=(std::move(left));
                    }
                }
            }
            if (description == "||")
            {
                if (son[0].description == "false")
                {
                    ASTNode right = std::move(son[1]);
                    operator=(std::move(right));
                } else {
                    if (son[1].description == "false")
                    {
                        ASTNode left = std::move(son[0]);
                        operator=(std::move(left));
                    }
                }
            }
            if (description == "==")
            {
                if (son[0].type == node_name && son[1].type== node_name)
                {
                    if (son[0].description == son[1].description)
                    {
                        type = node_name;
                        description = "true";
                    }
                }
                if (son[0].type == node_const_int && son[1].type == node_const_int)
                {
                    if (son[0].description == son[1].description)
                    {
                        type = node_name;
                        description = (Tool.to_int(son[0].description) == Tool.to_int(son[1].description)) ? "true" : "false";
                    }
                }
                if (son[0].type == node_const_str && son[1].type == node_const_str)
                {
                    if (son[0].description == son[1].description)
                    {
                        type = node_name;
                        description = (son[0].description == son[1].description) ? "true" : "false";
                    }
                }
            }
        }
        if (type == node_keyword_flow)
        {
            if (description == "if")
            {
                if (son[0].description == "true")
                {
                    ASTNode leftbranch = std::move(son[1]);
                    operator=(std::move(leftbranch));
                } else {
                    if (son[0].description == "false")
                    {
                        if (son.size() > 2)
                        {
                            ASTNode rightbranch = std::move(son[2]);
                            operator=(std::move(rightbranch));
                        } else {
                            type = node_const_int;
                            description = "0";
                        }
                    }
                }
            }
        }
    }

    void ASTNode::accomplish()
    {
        if (start_idx == -1)
            start_idx = lex_pos;
        if (lex_pos >= int(lex.size()))
            Error.compile_error("Fatal error : incomplete syntax phrase.");
        switch (type)
        {
        case node_phrase:
        {
            Token & head(lex[lex_pos]);
            lex_pos++;
            switch (head.type)
            {
            case keyword:
                if (head.value != "class")
                {
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : invalid keyword " + head.value + " occurs, keyword \"class\" expected.");
                }
                type = node_class_def;
                addSon(node_name);
                addSon(node_member_list);
                for (int i = 0; i < int(son.size()); ++i) inStack.push(&son[son.size() - 1 - i]);
                return;
            break;
            case id:
                type = node_dual_def;
                --lex_pos;
                inStack.push(this);
                return;
            break;
            case expr_end:
                Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : ISO Molang doesn't allow \";\" as an empty phrase.");
                inStack.push(this);
                return;
            break;
            default:
                Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : unexpected token " + head.value + " occurs.");
            break;
            }
        }
        break;
        case node_dual_def:
        {
            if (son.empty())
            {
                {
                    if (lex_pos + 1 >= int(lex.size()))
                        Error.compile_error("Fatal error : incomplete syntax phrase.");
                    if (lex[lex_pos + 1].type == op && lex[lex_pos + 1].value == "(")
                    {
                        type = node_constructor_def;
                        inStack.push(this);
                        return;
                    }
                }
                addSon(node_type);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            int separator_pos = lex_pos + 1;
            if (separator_pos >= int(lex.size()))
                Error.compile_error("Fatal error : incomplete syntax phrase.");
            if (lex[lex_pos].type != id)
                Error.compile_error("Syntax error :" + Error.place(lex[lex_pos].line, lex[lex_pos].col) + ": expect an identifier, got token \"" + lex[lex_pos].value + "\"");
            switch (lex[separator_pos].type)
            {
            case comma: case expr_end:
                type = node_var_def;
                inStack.push(this);
                return;
            break;
            case op:
                if (lex[separator_pos].type == op && lex[separator_pos].value == "(")
                {
                    type = node_fun_def;
                    inStack.push(this);
                    return;
                }
                if (lex[separator_pos].type == op && lex[separator_pos].value == "=")
                {
                    type = node_var_def;
                    inStack.push(this);
                    return;
                }
                Error.compile_error("Syntax error :" + Error.place(lex[separator_pos].line, lex[separator_pos].col) + "expect a valid separator, got operator \"" + lex[separator_pos].value + "\".");
                return;
            break;
            default:
                Error.compile_error("Syntax error : unexpected token \"" + lex[separator_pos].value + "\"");
                return;
            }
        }
        break;
        case node_var_def:
        {
            Token & head = lex[lex_pos];
            if (head.type == expr_end)
            {
                ++lex_pos;
                return;
            }
            addSon(node_initexpr);
            inStack.push(this);
            inStack.push(&son.back());
            return;
        }
        break;
        case node_initexpr:
        {
            if (son.empty())
            {
                addSon(node_name);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            description = son.front().description;
            Token & head = lex[lex_pos];
            lex_pos++;
            if (head.type == expr_end)
                --lex_pos;
            if (head.type == comma || head.type == expr_end)
                return;
            if (head.type == op)
            {
                if (head.value != "=")
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : invalid operator \"" + head.value + "\" found.");
                lex_pos -= 2;
                addSon(node_expr_tree);
                inStack.push(&son.back());
                return;
            }
        }
        break;
        case node_func_call:
        {
            addSon(node_name);
            addSon(node_func_call_para);
            for (int i = son.size() - 1; son.size() - i <= 2; --i) inStack.push(&son[i]);
            return;
        }
        break;
        case node_type:
        {
            {
                Token & head = lex[lex_pos++];
                if (head.type != id)
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect an identifier, got token \"" + head.value + "\".");
                description = head.value;
            }
            while (lex[lex_pos].type == op)
            {
                if (lex[lex_pos].value != "[]")
                    Error.compile_error("Syntax error : " + Error.place(lex[lex_pos].line, lex[lex_pos].col) + " : expect an array declarer [], got operator \"" + lex[lex_pos].value + "\".");
                description += "*";
                lex_pos++;
            }
            return;
        }
        break;
        case node_name:
        {
            Token & head = lex[lex_pos++];
            if (head.type != id)
                Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect an identifier, got token \"" + head.value + "\".");
            description = head.value;
            return;
        }
        break;
        case node_constructor_def:
        {
            addSon(node_name);
            addSon(node_fun_signature);
            addSon(node_closure);
            for (int i = son.size() - 1; son.size() - i <= 3; --i) inStack.push(&son[i]);
            return;

        }
        break;
        case node_fun_def:
        {
            addSon(node_name);
            addSon(node_fun_signature);
            addSon(node_closure);
            for (int i = son.size() - 1; son.size() - i <= 3; --i) inStack.push(&son[i]);
            return;
        }
        break;
        case node_func_call_para:
        {
            if (son.empty())
            {
                Token & head = lex[lex_pos++];
                if (!(head.type == op && head.value == "("))
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect a bracket (, got token \"" + head.value + "\".");
            }
            Token & peek = lex[lex_pos];
            if (!((peek.type == op && peek.value == ")") || peek.type == comma || peek.type == expr_end))
            {
                addSon(node_expr_tree);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            if (peek.type != expr_end)
                ++lex_pos;
            return;
        }
        break;
        case node_index_para:
        {
            if (son.empty())
            {
                Token & head = lex[lex_pos++];
                if (!(head.type == op && head.value == "["))
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect a bracket [, got token \"" + head.value + "\".");
            }
            Token & peek = lex[lex_pos];
            if (!((peek.type == op && peek.value == "]") || peek.type == comma || peek.type == expr_end))
            {
                addSon(node_expr_tree);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            if (son.empty())
                son.push_back(ASTNode(node_ambiguity_handler, lex, this, "ambiguity_handler"));
            if (peek.type != expr_end)
                ++lex_pos;
            return;
        }
        break;
        case node_fun_signature:
        {
            if (son.empty())
            {
                Token & head = lex[lex_pos++];
                if (!(head.type == op && head.value == "("))
                    Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect a bracket (, got token \"" + head.value + "\".");
            }
            Token & peek = lex[lex_pos];
            if (!(peek.type == op && peek.value == ")"))
            {
                addSon(node_sing_para);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            description = "";
            ++lex_pos;
            return;
        }
        break;
        case node_sing_para:
        {
            if (son.empty())
            {
                addSon(node_type);
                addSon(node_name);
                inStack.push(this);
                inStack.push(&son.back());
                inStack.push(&son.front());
                return;
            }
            Token & head(lex[lex_pos]);
            description = son.front().description;
            if (head.type == comma)
                ++lex_pos;
            if ((head.type != comma) && (!(head.type == op && head.value == ")")))
                Error.compile_error("Syntax error : " + Error.place(head.line, head.col) + " : expect a bracket ) or comma \",\" , got token \"" + head.value + "\".");
            return;
        }
        break;
        case node_expr:
        {
            Token & peek = lex[lex_pos];
            switch (peek.type)
            {
            case block_begin:
                addSon(node_closure);
                inStack.push(&son.back());
                return;
            break;
            case id: case integer: case conststr:
            {
                if (lex_pos + 1 >= int(lex.size()))
                    Error.compile_error("Fatal error : incomplete syntax phrase.");
                Token & peeknext = lex[lex_pos + 1];
                switch (peeknext.type)
                {
                case op:
                    if (peeknext.value == "[]")
                    {
                        addSon(node_dual_def);
                        inStack.push(&son.back());
                        return;
                    }
                    addSon(node_expr_single_line);
                    inStack.push(&son.back());
                    return;
                break;
                case id:
                    addSon(node_dual_def);
                    inStack.push(&son.back());
                    return;
                break;
                case expr_end:
                    addSon(node_expr_single_line);
                    inStack.push(&son.back());
                    return;
                break;
                default:
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect an expression, got token \"" + peek.value + "\".");
                }
            }
            break;
            case op:
                addSon(node_expr_single_line);
                inStack.push(&son.back());
                return;
            break;
            case expr_end:
                ++lex_pos;
                return;
            break;
            case keyword:
                addSon(node_keyword_flow);
                inStack.push(&son.back());
                return;
            break;
            default:
                Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect an expression, got token \"" + peek.value + "\".");
            }
        }
        break;
        case node_class_def:
            addSon(node_name);
            addSon(node_member_list);
            for (int i = son.size() - 1; i >= 0; --i) inStack.push(&son[i]);
            return;
        break;
        case node_member_list:
        {
            if (son.empty())
            {
                Token & peek(lex[lex_pos]);
                if (peek.type != block_begin)
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a member list, got token \"" + peek.value + "\".");
                ++lex_pos;
            }
            while (lex[lex_pos].type == expr_end) ++lex_pos;
            if (lex[lex_pos].type != block_end)
            {
                addSon(node_dual_def);
                inStack.push(this);
                inStack.push(&son.back());
            } else ++lex_pos;
        }
        break;
        case node_closure:
        {
            if (son.empty())
            {
                Token & peek(lex[lex_pos]);
                if (peek.type != block_begin)
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a code block, got token \"" + peek.value + "\".");
                ++lex_pos;
            }
            if (lex[lex_pos].type != block_end)
            {
                addSon(node_expr);
                inStack.push(this);
                inStack.push(&son.back());
            } else
            ++lex_pos;
        }
        break;
        case node_keyword_flow:
        {
            Token & peek(lex[lex_pos]);
            if (son.empty())
            {
                lex_pos++;
                if (peek.type != keyword)
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a keyword, got token \"" + peek.value + "\".");
                description = peek.value;
            }
            switch(control_flow_label(description))
            {
            case __if:
            {
                Token & peek = lex[lex_pos];
                switch (son.size())
                {
                case 0:
                {
                    if (!(peek.type == op && peek.value == "("))
                        Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a left bracket (, got token \"" + peek.value + "\".");
                    ++lex_pos;
                    addSon(node_expr_tree);
                    addSon(node_expr);
                    inStack.push(this);
                    for (int i = son.size() - 1; i >= 0; --i) inStack.push(&son[i]);
                    return;
                }
                break;
                case 2:
                {
                    if (peek.type != keyword || peek.value != "else") return;
                    ++lex_pos;
                    addSon(node_expr);
                    inStack.push(&son.back());
                    return;
                }
                break;
                default:
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : unknown error.");
                }
            }
            break;
            case __while:
            {
                Token & peek = lex[lex_pos];
                switch (son.size())
                {
                case 0:
                {
                    if (!(peek.type == op && peek.value == "("))
                        Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a left bracket (, got token \"" + peek.value + "\".");
                    ++lex_pos;
                    addSon(node_expr_tree);
                    addSon(node_expr);
                    for (int i = son.size() - 1; i >= 0; --i) inStack.push(&son[i]);
                    return;
                }
                break;
                default:
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : unknown error.");
                }
            }
            break;
            case __for:
            {
                Token & peek = lex[lex_pos];
                switch (son.size())
                {
                case 0:
                {
                    if (!(peek.type == op && peek.value == "("))
                        Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a left bracket (, got token \"" + peek.value + "\".");
                    ++lex_pos;
                    addSon(node_expr);
                    addSon(node_expr_single_line);
                    addSon(node_expr_tree);
                    addSon(node_expr);
                    for (int i = son.size() - 1; i >= 0; --i) inStack.push(&son[i]);
                    return;
                }
                break;
                default:
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : unknown error.");
                }
            }
            break;
            case __break: case __continue:
                ++lex_pos;
                return;
            break;
            case __else:
                Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : got keyword \"" + peek.value + "\" without a prerequisite \"if\" phrase.");
            break;
            case __new:
                type = node_expr_single_line;
                inStack.push(this);
                return;
            break;
            case __return:
            {
                Token & peek(lex[lex_pos]);
                if (peek.type != expr_end)
                {
                    addSon(node_expr_single_line);
                    inStack.push(&son.back());
                }
                return;
            }
            break;
            default:
                Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : expect a control flow redirector, got token \"" + peek.value + "\".");
            }
        }
        break;
        case node_expr_single_line:
        {
            Token & peek(lex[lex_pos]);
            if (peek.type == expr_end)
            {
                ++lex_pos;
                return;
            }
            if (son.empty())
            {
                addSon(node_expr_tree);
                inStack.push(this);
                inStack.push(&son.back());
                return;
            }
            if (peek.type != expr_end)
            {
                Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : missing phrase eliminator \";\".");
            }
            ++lex_pos;
        }
        break;
        case node_expr_tree:
        {
            //when description is empty or "func_call_seeking", it still absorbs tokens.
            //otherwise it just simplifies itself.
            //ASTNode & thisnode(*this);
            Token & peek(lex[lex_pos]);
            if (lex[lex_pos].type == keyword && lex[lex_pos].value == "new")
            {
                if (description != "__disable_new__")
                {
                    ++lex_pos;
                    description = "__disable_new__";
                    inStack.push(this);
                    return;
                } else {
                    Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : duplicate new found.");
                }
            }
            if (description == "__disable_new__") description = "";
            if (description == "" || description == "func_call_seeking")
            {
                if (peek.type == expr_end)
                {
                    if (father -> type != node_initexpr && father -> type != node_expr_single_line && father -> type != node_expr_tree)
                        ++lex_pos;
                    description = "complete";
                    inStack.push(this);
                    return;
                }
                if (peek.type == op && peek.value == ")")
                {
                    if (father -> type != node_func_call_para)
                        ++lex_pos;
                    description = "complete";
                    inStack.push(this);
                    return;
                }
                if (peek.type == comma)
                {
                    if (father -> type != node_expr_tree && father -> type != node_expr_single_line)
                    {
                        ++lex_pos;
                        description = "complete";
                        inStack.push(this);
                        return;
                    } else {
                        Error.compile_error("Syntax error : " + Error.place(peek.line, peek.col) + " : no comma expression supported.");
                    }
                }
                if (peek.type == op && peek.value == "]")
                {
                    if (father -> type != node_index_para)
                        ++lex_pos;
                    description = "complete";
                    inStack.push(this);
                    return;
                }
                if (peek.type == op && peek.value == "(")
                {
                    if (description == "func_call_seeking")
                    {
                        addSon(node_func_call_para);
                        inStack.push(this);
                        inStack.push(&son.back());
                        description = "";
                        return;
                    } else {
                        ++lex_pos;
                        addSon(node_expr_tree);
                        inStack.push(this);
                        inStack.push(&son.back());
                        return;
                    }
                }
                if (peek.type == id)
                    description = "func_call_seeking";
                else
                    description = "";
                if (peek.type == op && peek.value == "[")
                {
                    addSon(node_index_para);
                    inStack.push(this);
                    inStack.push(&son.back());
                    return;
                }
                son.push_back(make_node(lex_pos));
                ++lex_pos;
                inStack.push(this);
                return;
            } else {
                for (int i = leveled_operator.size() - 1; i >= 0; --i)
                {
                    for (string & op : leveled_operator[i])
                    {
                        expression_construction(op, this);
                    }
                }
                if (son.size() == 1)
                {
                    ASTNode tmp(std::move(son[0]));
                    operator=(std::move(tmp));
                }
            }
        }
        break;
        default:
        break;
        }
    }

    void ASTNode::remap_constant_string(NameController & name_table)
    {
        if (type == node_const_str)
        {
            description = "__conststr_" + Tool.to_string(name_table.get_constant_string_idx(description));
        }
        for (ASTNode & tree : son)
        {
            tree.remap_constant_string(name_table);
        }
    }
    ScopeTypeTable NameController::global_table;
    ControlFlow ASTNode::control_flow_label(string s)
    {
        addKeyword(if);
        addKeyword(for);
        addKeyword(continue);
        addKeyword(while);
        addKeyword(break);
        addKeyword(return);
        addKeyword(else);
        addKeyword(new);
        return __unknown_flow;
    }

    OperatorLabel ASTNode::operator_label(string s)
    {
        addRef(=, assign);
        addRef(==, equal_to);
        addRef(!=, not_equal_to);
        addRef(<, less);
        addRef(>, greater);
        addRef(<=, less_equal);
        addRef(>=, greater_equal);
        addRef(&&, logical_and);
        addRef(||, logical_or);
        addRef(&, bit_and);
        addRef(|, bit_or);
        addRef(^, bit_xor);
        addRef(>>, shr);
        addRef(<<, shl);
        addRef(+, add);
        addRef(-, sub);
        addRef(%, modulus);
        addRef(*, multiplies);
        addRef(/, divides);
        addRef(~, bit_not);
        addRef(!, logical_not);
        addRef(++, inc);
        addRef(--, dec);
        addRef([], index);
        addRef(., get_member);
        addRef(new, new_op);
        return __unknown_op;
    }

    void ASTNode::expression_construction(const string & opr, ASTNode * _father)
    {
        switch (operator_label(opr))
        {
        case __inc: case __dec:
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = son.size() - 1; i >= 0; --i)
                {
                    if (son[i].type == node_op && son[i].description == opr)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            continue;
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = 0; i < int(son.size()); ++i)
                {
                    if (i + 1 < int(son.size()) && son[i + 1].type == node_func_call_para)
                    {
                        if (i < 0 || (son[i].type != node_expr_tree && son[i].type != node_name))
                            Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of calling functions, that is not a callable object.");
                        ASTNode tmp_node(node_func_call, lex, _father, "", son[i].start_idx);
                        tmp_node.son.push_back(std::move(son[i]));
                        tmp_node.son.push_back(std::move(son[i + 1]));
                        son[i] = std::move(tmp_node);
                        son.erase(son.begin() + i + 1, son.begin() + i + 2);
                    }
                    if (son[i].type == node_op && son[i].description == opr)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            tmp_node.start_idx = son[i - 1].start_idx;
                            tmp_node.son.push_back(std::move(son[i - 1]));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i - 1], "ambiguity_handler"));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.description = son[i].description;
                            son[i - 1] = std::move(tmp_node);
                            son.erase(son.begin() + i, son.begin() + i + 1);
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
        break;
        default: break;
        }

        switch (operator_label(opr))
        {
        case __assign:
        case __logical_not:
        case __bit_not:
        case __inc:
        case __dec:
        case __new_op:
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = son.size() - 1; i >= 0; --i)
                {
                    if (son[i].type == node_op && son[i].description == opr)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            tmp_node.start_idx = son[i - 1].start_idx;
                            tmp_node.son.push_back(std::move(son[i - 1]));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i - 1], "ambiguity_handler"));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.description = son[i].description;
                            son[i - 1] = std::move(tmp_node);
                            son.erase(son.begin() + i, son.begin() + i + 1);
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
        break;
        case __equal_to:
        case __not_equal_to:
        case __less:
        case __greater:
        case __less_equal:
        case __greater_equal:
        case __logical_and:
        case __logical_or:
        case __bit_and:
        case __bit_or:
        case __bit_xor:
        case __shr:
        case __shl:
        case __add:
        case __sub:
        case __index:
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = 0; i < int(son.size()); ++i)
                {
                    if (i + 1 < int(son.size()) && son[i + 1].type == node_func_call_para)
                    {
                        if (i < 0 || (son[i].type != node_expr_tree && son[i].type != node_name))
                            Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of calling functions, that is not a callable object.");
                        ASTNode tmp_node(node_func_call, lex, _father, "", son[i].start_idx);
                        tmp_node.son.push_back(std::move(son[i]));
                        tmp_node.son.push_back(std::move(son[i + 1]));
                        son[i] = std::move(tmp_node);
                        son.erase(son.begin() + i + 1, son.begin() + i + 2);
                    }
                    if (son[i].type == node_op && son[i].description == opr)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            tmp_node.start_idx = son[i - 1].start_idx;
                            tmp_node.son.push_back(std::move(son[i - 1]));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i - 1], "ambiguity_handler"));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.description = son[i].description;
                            son[i - 1] = std::move(tmp_node);
                            son.erase(son.begin() + i, son.begin() + i + 1);
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
        break;
        case __multiplies:
        case __divides:
        case __modulus:
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = 0; i < int(son.size()); ++i)
                {
                    if (i + 1 < int(son.size()) && son[i + 1].type == node_func_call_para)
                    {
                        if (i < 0 || (son[i].type != node_expr_tree && son[i].type != node_name))
                            Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of calling functions, that is not a callable object.");
                        ASTNode tmp_node(node_func_call, lex, _father, "", son[i].start_idx);
                        tmp_node.son.push_back(std::move(son[i]));
                        tmp_node.son.push_back(std::move(son[i + 1]));
                        son[i] = std::move(tmp_node);
                        son.erase(son.begin() + i + 1, son.begin() + i + 2);
                    }
                    if (son[i].type == node_op && is_in(son[i].description, set<string>({"*", "/", "%"})))
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            tmp_node.start_idx = son[i - 1].start_idx;
                            tmp_node.son.push_back(std::move(son[i - 1]));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i - 1], "ambiguity_handler"));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.description = son[i].description;
                            son[i - 1] = std::move(tmp_node);
                            son.erase(son.begin() + i, son.begin() + i + 1);
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
        break;
        case __get_member:
            for(bool flag = true; flag;)
            {
                flag = false;
                for (int i = 0; i < int(son.size()); ++i)
                {
                    if (i + 1 < int(son.size()) && son[i + 1].type == node_func_call_para)
                    {
                        if (i < 0 || (son[i].type != node_expr_tree && son[i].type != node_name))
                            Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of calling functions, that is not a callable object.");
                        ASTNode tmp_node(node_func_call, lex, _father, "", son[i].start_idx);
                        tmp_node.son.push_back(std::move(son[i]));
                        tmp_node.son.push_back(std::move(son[i + 1]));
                        son[i] = std::move(tmp_node);
                        son.erase(son.begin() + i + 1, son.begin() + i + 2);
                    }
                    while (i + 1 < int(son.size()) && son[i + 1].type == node_index_para)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father, "[]", son[i].start_idx);
                        tmp_node.son.push_back(std::move(son[i]));
                        tmp_node.son.push_back(std::move(son[i + 1].son[0]));
                        son[i] = std::move(tmp_node);
                        son.erase(son.begin() + i + 1, son.begin() + i + 2);
                    }
                    if (son[i].type == node_op && son[i].description == opr)
                    {
                        ASTNode tmp_node(node_expr_tree, lex, _father);
                        if (i + 1 >= int(son.size()) || son[i + 1].type == node_op)
                        {
                            tmp_node.start_idx = son[i - 1].start_idx;
                            tmp_node.son.push_back(std::move(son[i - 1]));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i - 1], "ambiguity_handler"));
                            tmp_node.son.back().father = &son[i - 1];
                            tmp_node.description = son[i].description;
                            son[i - 1] = std::move(tmp_node);
                            son.erase(son.begin() + i, son.begin() + i + 1);
                        } else {
                            if (i - 1 < 0 || son[i - 1].type == node_op)
                            {
                                tmp_node.start_idx = son[i].start_idx;
                                tmp_node.son.push_back(ASTNode(node_ambiguity_handler, lex, &son[i], "ambiguity_handler"));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i];
                                tmp_node.description = son[i].description;
                                son[i] = std::move(tmp_node);
                                son.erase(son.begin() + i + 1, son.begin() + i + 2);
                            } else {
                                tmp_node.start_idx = son[i - 1].start_idx;
                                tmp_node.son.push_back(std::move(son[i - 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.son.push_back(std::move(son[i + 1]));
                                tmp_node.son.back().father = &son[i - 1];
                                tmp_node.description = son[i].description;
                                son[i - 1] = std::move(tmp_node);
                                son.erase(son.begin() + i, son.begin() + i + 2);
                            }
                        }
                        flag = true;
                        break;
                    }
                }
            }
        break;
        case __unknown_op:
            Error.compile_error("fatal error : scanner has failed.");
        break;
        }
        switch (operator_label(opr))
        {
        case __index:
            for (int i = son.size() - 1; i >= 0; --i)
            {
                if (son[i].type == node_func_call_para)
                {
                    if (i - 1 < 0 || (son[i - 1].type != node_expr_tree && son[i - 1].type != node_name))
                        Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : invalid use of calling functions, that is not a callable object.");
                    ASTNode tmp_node(node_func_call, lex, _father, "", son[i - 1].start_idx);
                    tmp_node.son.push_back(std::move(son[i - 1]));
                    tmp_node.son.push_back(std::move(son[i]));
                    son[i - 1] = std::move(tmp_node);
                    son.erase(son.begin() + i, son.begin() + i + 1);
                }
            }
        break;
        case __assign:
            if (son.size() == 0)
                son.push_back(ASTNode(node_ambiguity_handler, lex, this, "ambiguity_handler"));
            if (son.size() != 1)
                Error.compile_error("Syntax error : " + Error.place(lex[start_idx].line, lex[start_idx].col) + " : expression does not express a tree, a forest instead.");
        break;
        default:
        break;
        }
    }

    ASTNode & ASTNode::operator=(ASTNode && be_moved)
    {
        start_idx = be_moved.start_idx;
        type = be_moved.type;
        description = be_moved.description;
        son = std::move(be_moved.son);
        return *this;
    }

    void ASTNode::print_tree(ostream & fout, int space)
    {
        for (int i = 0; i < space; ++i) fout << ' ';
        if (type == node_const_int || type == node_name || type == node_type)
        {
            fout << description;
            return;
        }
        if (type == node_const_str)
        {
            fout << "\"" << description << "\"";
            return;
        }
        if (type == node_expr_tree || type == node_func_call || type == node_func_call_para)
        {
            fout << '(' << description;
            for (ASTNode & ason : son)
            {
                fout << ' ';
                ason.print_tree(fout, 0);
            }
            fout << ')';
            return;
        }
        else if (type == node_op || type == node_type || type == node_name || type == node_keyword_flow || type == node_expr_tree)
            fout << '(' << ASTNodeName[type] << ":" << description;
        else
            fout << '(' << ASTNodeName[type];
        for (ASTNode & ason : son)
        {
            fout << '\n';
            ason.print_tree(fout, space + 4);
        }
        fout << ')';
    }

    ASTNode ASTNode::make_node(int idx)
    {
        Token & token(lex[idx]);
        switch (token.type)
        {
        case integer:
        return ASTNode(node_const_int, lex, this, token.value, idx);
        case conststr:
        return ASTNode(node_const_str, lex, this, token.value, idx);
        case id:
        return ASTNode(node_name, lex, this, token.value, idx);
        case op:
        return ASTNode(node_op, lex, this, token.value, idx);
        default:
            Error.compile_error("Expression parsing error : " + Error.place(token.line, token.col) + " : no keyword expected.");
        return ASTNode(unknown_node, lex, this);
        }
    }

    void _Parser::check_bracket_pair()
    {
        int small_bra = 0;
        int sq_bra = 0;
        int big_bra = 0;
        for (Token & t : lexemes)
        {
            if (t.type == block_begin)
            {
                ++big_bra;
            }
            if (t.type == block_end)
            {
                --big_bra;
            }
            if (t.type == op && t.value == "(")
            {
                ++small_bra;
            }
            if (t.type == op && t.value == ")")
            {
                --small_bra;
            }
            if (t.type == op && t.value == "[")
            {
                ++sq_bra;
            }
            if (t.type == op && t.value == "]")
            {
                --sq_bra;
            }
            if (big_bra < 0)
            {
                Error.compile_error("Syntax error : " + Error.place(t.line, t.col) + " : unpaired block bracket found.");
            }
            if (sq_bra < 0)
            {
                Error.compile_error("Syntax error : " + Error.place(t.line, t.col) + " : unpaired index bracket found.");
            }
            if (small_bra < 0)
            {
                Error.compile_error("Syntax error : " + Error.place(t.line, t.col) + " : unpaired bracket found.");
            }
        }
        if (big_bra > 0)
        {
            Error.compile_error("Syntax error : unpaired block bracket found.");
        }
        if (sq_bra > 0)
        {
            Error.compile_error("Syntax error : unpaired index bracket found.");
        }
        if (small_bra > 0)
        {
            Error.compile_error("Syntax error : unpaired bracket found.");
        }
    }

    void _Parser::construct_CST()
    {
        check_bracket_pair();
        while (lex_pos < int(lexemes.size()))
        {
            forest.push_back(ASTNode(node_phrase, lexemes));
            inStack.push(&forest.back());
            while (!inStack.empty())
            {
                auto k = inStack.top();
                inStack.pop();
                k -> accomplish();
            }
        }
    }

    void _Parser::class_declare()
    {
        for (ASTNode & tree : forest)
        {
            if (tree.type == node_class_def)
            {
                name_table.add_class(tree);
            }
        }
    }

    void _Parser::deep_declare()
    {
        for (ASTNode & tree : forest)
        {
            tree.accomplish_closure(name_table);
        }
    }

    void _Parser::remap_constantstring()
    {
        for (ASTNode & tree : forest)
        {
            tree.remap_constant_string(name_table);
        }
        //name_table.print_constant_table(std::cout);
    }

    void _Parser::type_validation()
    {
        //int i = 0;

        for (ASTNode & tree : forest)
        {
            //std::cout << "validating no." << i << endl;
            tree.check_type(name_table);
            //std::cout << "tree no." << i++ << " alive." << std::endl;
        }
    }

    void _Parser::do_AST_level_optimization()
    {
        for (ASTNode & tree : forest)
            tree.inline_func_extraction();
        check_conn();
        for (auto & s : ASTNode::inlinable_function_table)
            std::cerr << "YES :: " << s.first << "\n";
        for (ASTNode & tree : forest)
            tree.do_inlination();
        check_conn();
        for (ASTNode & tree : forest)
            tree.linear_expression_compression(name_table);
        check_conn();
        for (ASTNode & tree : forest)
            tree.constant_fold();
        check_conn();
        for (ASTNode & tree : forest)
            tree.if_phrase_enlarge();
        check_conn();
        for (ASTNode & tree : forest)
            tree.loop_truncation(name_table);
        check_conn();
        for (ASTNode & tree : forest)
            tree.if_phrase_enlarge();
        check_conn();
        for (ASTNode & tree : forest)
            tree.loop_truncation(name_table);
        check_conn();
        for (ASTNode & tree : forest)
            tree.prefix_reorder();
        check_conn();
        for (ASTNode & tree : forest)
            tree.dead_code_elimination();
        check_conn();
        for (ASTNode & tree : forest)
            tree.logical_expression_compression();
        check_conn();
    }

    void _Parser::check_conn()
    {
        for (int i = 0; i < int(forest.size()); ++i)
        {
            check(forest[i], NULL);
        }
    }

    bool _Parser::check(ASTNode & in, ASTNode * fat)
    {
        bool r = true;
        for (ASTNode & subtree : in.son)
        {
            if (!check(subtree, &in))
            {
                r = false;
            }
        }
        r = r && (in.father == fat);
        if (!r)
        {
            in.father = fat;
            //std::cout << "checking a " << ASTNodeName[in.type] << " fails, repaired." << std::endl;
        }
        return r;
    }

    void NameController::add_var(ASTNode & node)
    {
        scope.back() -> add_var(node, var_type_fetch(node.start_idx, node.son.front().description, false));
    }

    void NameController::add_func(ASTNode & node)
    {
        if (is_in(node.son[1].description, reserved_word))
        {
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse reserved name.");
        }
        scope.back() -> add_func(node);
    }

    void NameController::add_cons(ASTNode & node)
    {
        if (is_in(node.son[1].description, reserved_word))
        {
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse reserved name.");
        }
        if (scope.size() != 2) Error.compile_error("Constructor error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : no surrounding class found.");
        global_table.add_cons(node);
    }

    void NameController::add_class(ASTNode & node)
    {
        if (is_in(node.son.front().description, reserved_word))
        {
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse reserved name.");
        }
        if (scope.size() != 1) Error.compile_error("Class error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : no embedded class permitted.");
        scope.back() -> add_class(node);
    }

    InferredType NameController::var_type_fetch(int idx, string _name, bool plain)
    {
        InferredType ret;

        ret.abstract_type = TypeDescriptor::type_var(plain ? 0 : idx);
        for (int i = _name.length() - 1; _name[i] == '*'; --i, ++ret.array_dimension);
        ret.basic_sign = _name.substr(0, _name.length() - ret.array_dimension);
        if (!is_in(ret.basic_sign, global_table._class_def))
        {
            Error.compile_error("Type error : unknown type \"" + ret.basic_sign + "\"");
        }
        if (ret.basic_sign == "void")
        {
            Error.compile_error("Type error : type void is only used when you declare a procedure, whose return value cannot be used as lvalue or rvalue.");
        }
        return ret;
    }

    InferredType NameController::fun_type_fetch(string _name)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_class(0);
        for (int i = _name.length() - 1; _name[i] == '*'; --i, ++ret.array_dimension);
        ret.basic_sign = _name.substr(0, _name.length() - ret.array_dimension);
        if (!is_in(ret.basic_sign, global_table._class_def))
        {
            Error.compile_error("Type error : unknown type \"" + ret.basic_sign + "\"");
        }
        if (ret.basic_sign == "void" && ret.array_dimension > 0)
        {
            Error.compile_error("Type error : type void is only used when you declare a procedure, whose return value cannot be used as lvalue or rvalue.");
        }
        return ret;
    }

    void NameController::in_func(string title)
    {
        scope.back() -> in_func(title);
    }

    void NameController::in_class(string title)
    {
        scope.back() -> in_class(title);
    }

    void NameController::plainize_current_scope()
    {
        scope.back() -> plainize();
    }

    string NameController::get_func_title()
    {
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            if (scope[i] -> func_title != "")
                return scope[i] -> func_title;
        }
        return "__constructor__";
    }

    string NameController::get_class_title()
    {
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            if (scope[i] -> class_title != "")
                return scope[i] -> class_title;
        }
        Error.compile_error("Error : no class fetched.");
        return "__no_class__";
    }

    int NameController::get_constant_string_idx(const string & ss)
    {
        if (is_in(ss, constant_string_remapping))
            return constant_string_remapping[ss];
        constant_string_remapping[ss] = constant_string.size();
        constant_string.push_back(ss);
        return constant_string.size() - 1;
    }

    const string &NameController::get_constant_string(int idx) const
    {
        return constant_string[idx];
    }

    bool NameController::is_global() const
    {
        return scope.size() == 1;
    }

    bool NameController::match(const string & s1, const string & s2)
    {
        if (s2 == "__null")
        {
            if (s1 == "int" || s1 == "bool" || s1 == "string")
                return false;
            return true;
        }
        if (s1 != s2) return false;
        return true;
    }

    string NameController::fetch_function_sign(string func)
    {
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            if (is_in(func, scope[i] -> function_sign))
            {
                return scope[i] -> function_sign[func];
            }
        }
        Error.compile_error("FATAL!! NO CORRECT FUNCTION FOUND");
        return "";
    }

    void NameController::print_constant_table(ostream & fout)
    {
        for (int i = 0; i < int(constant_string.size()); ++i)
        {
            fout << "Constant string no." << i << " : " << constant_string[i] << endl;
        }
    }

    bool NameController::check_actual_sign(string std_sign, string _sign)
    {
        if (_sign == " ") _sign = "";
        int paran1 = 0, paran2 = 0;
        for (char i : std_sign) if (i == ' ') ++paran1;
        for (char i : _sign) if (i == ' ') ++paran2;
        if (paran1 != paran2) return false;
        size_t st1 = 0, st2 = 0;
        string s1, s2;
        while (st1 < std_sign.size() && st2 < _sign.size())
        {
            int l1 = 0, l2 = 0;
            while (std_sign[st1 + l1] != ' ') ++l1;
            while (_sign[st2 + l2] != ' ') ++l2;
            s1 = std_sign.substr(st1, l1);
            s2 = _sign.substr(st2, l2);
            if (!match(s1, s2))
                return false;
            st1 += l1 + 1;
            st2 += l2 + 1;
        }
        return true;
    }

    bool NameController::check_sign(string func, string _sign)
    {
        string std_sign = fetch_function_sign(func);
        if (_sign == " ") _sign = "";
        int paran1 = 0, paran2 = 0;
        for (char i : std_sign) if (i == ' ') ++paran1;
        for (char i : _sign) if (i == ' ') ++paran2;
        if (paran1 != paran2) return false;
        size_t st1 = 0, st2 = 0;
        string s1, s2;
        while (st1 < std_sign.size() && st2 < _sign.size())
        {
            int l1 = 0, l2 = 0;
            while (std_sign[st1 + l1] != ' ') ++l1;
            while (_sign[st2 + l2] != ' ') ++l2;
            s1 = std_sign.substr(st1, l1);
            s2 = _sign.substr(st2, l2);
            if (!match(s1, s2))
                return false;
            st1 += l1 + 1;
            st2 += l2 + 1;
        }
        return true;
    }

    void NameController::add_func_type(string name, string type_sign)
    {
        if (scope.size() == 1)
            scope.back() -> function_ret[name] = type_sign;
        else
            scope[scope.size() - 2] -> function_ret[name] = type_sign;
    }

    void NameController::add_func_sign(string name, string sign)
    {
        if (sign == " ") sign = "";
        if (scope.size() == 1)
            scope.back() -> function_sign[name] = sign;
        else
            scope[scope.size() - 2] -> function_sign[name] = sign;

    }

    bool NameController::in_flow(string s)
    {
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            if (scope[i] -> control_flow == s)
                return true;
        }
        return false;
    }

    string NameController::get_fun_ret(string s)
    {
        if (s == "__constructor__")
            return "void";
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            if (is_in(s, scope[i] -> function_ret))
                return scope[i] -> function_ret[s];
        }
        Error.compile_error("FATAL ERROR : return value not found.");
        return "";
    }

    void NameController::set_flow(string s)
    {
        scope.back() -> set_flow(s);
    }

    void NameController::add_class(string _name, int byte_num)
    {
        scope.back() -> add_class(_name, byte_num);
    }

    InferredType NameController::fetch_in_current_scope(string _name)
    {
        return scope.back() -> fetch(_name);
    }

    InferredType NameController::fetch(string _name, int idx)
    {
        if (_name == "true")
        {
            int p = 2;
            p += 2;
        }
        for (int i = scope.size() - 1; i >= 0; --i)
        {
            auto p = scope[i] -> fetch(_name, idx);
            if (p.abstract_type.invalid())
                continue;
            return p;
        }
        return InferredType::unused_name();
    }

    void NameController::move_into_scope(ScopeTypeTable * new_scope)
    {
        scope.push_back(new_scope);
    }

    ClassDescriptor NameController::fetch_class(string _name)
    {
        if (is_in(_name, global_table._class_def))
            return global_table._class_def[_name];
        if (_name.back() == '*')
            return ClassDescriptor::basic_type(8);
        return ClassDescriptor::basic_type(0);
    }

    void NameController::leave_scope()
    {
        scope.pop_back();
    }

    void NameController::add_const(string _type, string _name, string _value)
    {
        /*if (is_in(_name, reserved_word))
            {
                Error.compile_error("Name error : trying to reuse reserved name to define a const variable.");
            }*/
        scope.back() -> add_const(_name, _value, var_type_fetch(0, _type, true));
    }

    void NameController::add_var(string _type, string _name, string _value)
    {
        /*if (is_in(_name, reserved_word))
            {
                Error.compile_error("Name error : trying to reuse reserved name to define a const variable.");
            }*/
        scope.back() -> add_var(_name, _value, var_type_fetch(0, _type, true));
    }

    void NameController::add_plain_var(ASTNode & node)
    {
        scope.back() -> add_var(node, var_type_fetch(node.start_idx, node.son.front().description, true));

    }

    void ScopeTypeTable::add_var(ASTNode & node, const InferredType & type)
    {
        for (auto i = node.son.begin() + 1; i != node.son.end(); ++i)
        {
            string & var_name = i -> description;
            if (is_in(var_name, reserved_word))
            {
                Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse reserved name.");
            }
            if (!is_in(var_name, _abstract_type))
            {
                _abstract_type[i -> description] = TypeDescriptor::type_var();
                _var_def[i -> description] = type;
            }
            else
                Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse a local name.");
        }
    }

    void ScopeTypeTable::add_cons(ASTNode & node)
    {
        if (is_in(node.son[0].description, _abstract_type) && _abstract_type[node.son[0].description]._class)
        {
            _func_def[node.son[0].description] = FunctionDescriptor::constructor(node);
        }
        else
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to a invalid name \"" + node.son[0].description + "\" to define a constructor.");
    }

    void ScopeTypeTable::add_func(ASTNode & node)
    {
        if (!is_in(node.son[1].description, _abstract_type))
        {
            _abstract_type[node.son[1].description] = TypeDescriptor::type_func();
            _func_def[node.son[1].description] = FunctionDescriptor::global_function(node);
        }
        else
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse a local name \"" + node.son[1].description + "\".");
    }

    void ScopeTypeTable::add_class(ASTNode & node)
    {
        if (!is_in(node.son.front().description, _abstract_type))
        {
            _abstract_type[node.son.front().description] = TypeDescriptor::type_class();
            _class_def[node.son.front().description] = ClassDescriptor::actual_class(node);
        }
        else
            Error.compile_error("Name error : " + Error.place(node.lex[node.start_idx].line, node.lex[node.start_idx].col) + " : trying to reuse a local name \"" + node.son.front().description + "\".");
    }

    void ScopeTypeTable::clear()
    {
        _abstract_type.clear();
        _class_def.clear();
        _func_def.clear();
        _var_def.clear();
        _const_def.clear();
    }

    void ScopeTypeTable::in_func(string _title)
    {
        func_title = _title;
    }

    void ScopeTypeTable::in_class(string _title)
    {
        class_title = _title;
    }

    void ScopeTypeTable::add_class(string _name, int byte_num)
    {
        if (!is_in(_name, _abstract_type))
        {
            _abstract_type[_name] = TypeDescriptor::type_class();
            _class_def[_name] = ClassDescriptor::basic_type(byte_num);
            if (_name == "void" || _name == "__null") _abstract_type[_name]._callable = false;
        }
        else
            Error.compile_error(string("fatal error : symbol ") + _name  + " redefined");
    }

    void ScopeTypeTable::plainize()
    {
        for (auto i = _var_def.begin(); i != _var_def.end(); ++i)
            i -> second.abstract_type.start_idx = 0;
    }

    void ScopeTypeTable::add_const(string _name, string _value, InferredType type)
    {
        type.abstract_type._const = true;
        type.abstract_type._constexpr = true;
        if (!is_in(_name, _abstract_type))
        {
            _abstract_type[_name] = TypeDescriptor::type_const_var();
            _var_def[_name] = type;
            _const_def[_name] = _value;
        }
        else
            Error.compile_error("Name error : trying to reuse a local name as a const symbol.");
    }

    void ScopeTypeTable::add_var(string _name, string _value, const InferredType & type)
    {
        if (!is_in(_name, _abstract_type))
        {
            _abstract_type[_name] = TypeDescriptor::type_var();
            _var_def[_name] = type;
            _const_def[_name] = _value;
        }
        else
            Error.compile_error("Name error : trying to reuse a local name as a const symbol.");
    }

    void ScopeTypeTable::set_flow(string s)
    {
        control_flow = s;
    }

    InferredType ScopeTypeTable::fetch(string _name, int start_idx)
    {

        InferredType type_des;
        type_des.abstract_type = _abstract_type[_name];
        type_des.basic_sign = _name;
        for (int i = _name.length() - 1; _name[i] == '*'; --i, ++type_des.array_dimension);
        type_des.basic_sign = _name.substr(0, _name.length() - type_des.array_dimension);
        if (!is_in(type_des.basic_sign, _abstract_type))
            return InferredType::unused_name();
        if (!type_des.abstract_type._var)
            return type_des;
        else
        {
            if (start_idx >= _var_def[_name].abstract_type.start_idx)
                return _var_def[_name];
            else
                return InferredType::unused_name();
        }
    }

    InferredType InferredType::unused_name()
    {
        return InferredType();
    }

    InferredType InferredType::lvalue(string _basic_sign, int _array_dimension)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_var();
        ret.basic_sign = _basic_sign;
        ret.array_dimension = _array_dimension;
        int i = ret.basic_sign.length() - 1;
        while (i >= 0 && ret.basic_sign[i] == '*') --i, ++ret.array_dimension;
        if (i != int(ret.basic_sign.length() - 1))
            ret.basic_sign = ret.basic_sign.substr(0, i + 1);
        return ret;
    }

    InferredType InferredType::rvalue(string _basic_sign, int _array_dimension)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_const_var();
        ret.basic_sign = _basic_sign;
        ret.array_dimension = _array_dimension;
        int i = ret.basic_sign.length() - 1;
        while (i >= 0 && ret.basic_sign[i] == '*') --i, ++ret.array_dimension;
        if (i != int(ret.basic_sign.length() - 1))
            ret.basic_sign = ret.basic_sign.substr(0, i + 1);
        return ret;
    }

    InferredType InferredType::built_in_func(string _basic_sign)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_built_in_func();
        ret.basic_sign = _basic_sign;
        return ret;
    }

    InferredType InferredType::hash_para(const deque<InferredType> & para)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_verified_ensembler();
        ret.basic_sign = "";
        for (const InferredType & ty : para)
        {
            ret.basic_sign += ty.basic_sign;
            for (int i = 0; i < ty.array_dimension; ++i) ret.basic_sign += "*";
            ret.basic_sign += " ";
        }
        return ret;
    }

    InferredType InferredType::ensembler(string _description)
    {
        InferredType ret;
        ret.abstract_type = TypeDescriptor::type_verified_ensembler();
        ret.basic_sign = _description;
        return ret;
    }

    InferredType InferredType::ambiguity_handler()
    {
        InferredType ret;
        ret.basic_sign = "ambiguity_handler";
        return ret;
    }

    bool InferredType::ambi() const
    {
        return basic_sign == "ambiguity_handler" && abstract_type.invalid();
    }

    InferredType InferredType::array_wrap() const
    {
        InferredType r(*this);
        ++r.array_dimension;
        return r;
    }

    InferredType InferredType::de_array() const
    {
        InferredType r(*this);
        --r.array_dimension;
        return r;
    }

    bool InferredType::is_indexable() const
    {
        if (array_dimension > 0) return true;
        if (abstract_type._class) return true;
        return false;
    }

    bool InferredType::basic_type() const
    {
        return is_in(basic_sign, builtintype) && array_dimension == 0;
    }

    bool InferredType::is_arith() const
    {
        return array_dimension == 0 && (basic_sign == "int");
    }

    bool InferredType::operator<=(const InferredType & rhs) const
    {
        if (abstract_type._const || abstract_type._constexpr || abstract_type._ensembled || abstract_type._callable || abstract_type._class || (!abstract_type._var))
            return false;
        if (rhs.basic_sign == "__null")
        {
            if (array_dimension == 0 && (is_in(basic_sign, builtintype)))
                return false;
            return true;
        }
        return basic_sign == rhs.basic_sign && array_dimension == rhs.array_dimension;
    }

    string InferredType::signature() const
    {
        string base = basic_sign;
        for (int i = 0; i < array_dimension; ++i) base += "*";
        return base;
    }

    bool InferredType::operator==(const InferredType & rhs) const
    {
        if (rhs.basic_sign == "__null")
        {
            if (array_dimension == 0 && (is_in(basic_sign, builtintype)))
                return false;
            return true;
        }
        return basic_sign == rhs.basic_sign && array_dimension == rhs.array_dimension;
    }

    ClassDescriptor ClassDescriptor::actual_class(ASTNode & node)
    {
        ClassDescriptor ret;
        ret.byte_num = 8;
        ret.node_ref = &node;
        return ret;
    }

    ClassDescriptor ClassDescriptor::basic_type(int _byte_num)
    {
        ClassDescriptor ret;
        ret.byte_num = _byte_num;
        ret.node_ref = NULL;
        return ret;
    }

    FunctionDescriptor FunctionDescriptor::constructor(ASTNode & node)
    {
        FunctionDescriptor ret;
        ret.name = node.son.front().description;
        ret.ref_node = &node;
        ret.result_signature = node.son.front().description;
        return ret;
    }

    FunctionDescriptor FunctionDescriptor::global_function(ASTNode & node)
    {
        FunctionDescriptor ret;
        ret.name = node.son[1].description;
        ret.ref_node = &node;
        ret.result_signature = node.son.front().description;
        return ret;
    }

    TypeDescriptor TypeDescriptor::type_class(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._class = true;
        ret_v._const = false;
        ret_v._callable = true;
        ret_v._var = false;
        ret_v.start_idx = idx;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_func(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._callable = true;
        ret_v.start_idx = idx;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_const_var(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._const = true;
        ret_v.start_idx = idx;
        ret_v._var = true;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_literal_var(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._const = true;
        ret_v._constexpr = true;
        ret_v.start_idx = idx;
        ret_v._var = true;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_var(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._var = true;
        ret_v.start_idx = idx;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_built_in_func()
    {
        TypeDescriptor ret_v;
        ret_v._callable = true;
        ret_v._ensembled = true;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_verified_ensembler(int idx)
    {
        TypeDescriptor ret_v;
        ret_v._ensembled = true;
        ret_v.start_idx = idx;
        return ret_v;
    }

    TypeDescriptor TypeDescriptor::type_invalid()
    {
        return TypeDescriptor();
    }

    bool TypeDescriptor::operator<=(const TypeDescriptor & rhs) const
    {
        return
                (!invalid()) &&
                _class == rhs._class &&
                _const == false &&
                _constexpr == false &&
                _callable == rhs._callable &&
                _ensembled == rhs._ensembled &&
                _var == rhs._var;
    }

    bool TypeDescriptor::operator==(const TypeDescriptor & rhs) const
    {
        return
                (!invalid()) &&
                _class == rhs._class &&
                _callable == rhs._callable &&
                _ensembled == rhs._ensembled &&
                _var == rhs._var;
    }

    bool TypeDescriptor::invalid() const
    {
        return
                _class == false &&
                _const == false &&
                _constexpr == false &&
                _callable == false &&
                _ensembled == false &&
                _var == false;
    }





}
