#include "translator.hpp"
using std::stringstream;

namespace msharp
{
    string IR::repr_IRType(msharp::IRType p)
    {
        switch (p)
        {
        case IR__error:             return "error";
        case IR__arg:               return "arg";
        case IR__add:               return "add";
        case IR__land:              return "land";
        case IR__lor:               return "lor";
        case IR__band:              return "band";
        case IR__bor:               return "bor";
        case IR__assign:            return "mov";
        case IR__break:             return "break";
        case IR__call:              return "call";
        case IR__cond:              return "cond";
        case IR__cond_else:         return "cond_else";
        case IR__continue:          return "continue";
        case IR__get_size:          return "get_size";
        case IR__div:               return "div";
        case IR__mod:               return "mod";
        case IR__inc:               return "inc";
        case IR__dec:               return "dec";
        case IR__cond_end:          return "cond_end";
        case IR__malloc:            return "malloc";
        case IR__loop_start:        return "loop_start";
        case IR__loop_end:          return "loop_end";
        case IR__eq:                return "eq";
        case IR__func:              return "func";
        case IR__geq:               return "geq";
        case IR__greater:           return "greater";
        case IR__leq:               return "leq";
        case IR__less:              return "less";
        case IR__loop_body:         return "loop_body";
        case IR__loop_constraint:   return "loop_constraint";
        case IR__loop_iter:         return "loop_iter";
        case IR__mul:               return "mul";
        case IR__neq:               return "neq";
        case IR__not:               return "not";
        case IR__lnot:              return "lnot";
        case IR__ret:               return "ret";
        case IR__shl:               return "shl";
        case IR__shr:               return "shr";
        case IR__byte:              return "byte";
        case IR__word:              return "word";
        case IR__quad:              return "quad";
        case IR__sub:               return "sub";
        case IR__var:               return "var";           //var width, name;
        case IR__xor:               return "xor";
        }
        return "___something_has_been_fucked_up";
    }

    ostream &operator<<(ostream & fout, const IR & s)
    {
        static int indent = 0;
        if (s.oprand == IR__func && indent > 0) --indent;
        for (int i = 0; i < indent; ++i) fout << '\t';
        fout << IR::repr_IRType(s.oprand) << "\t" << join(", ", s.args);
        if (s.oprand == IR__func) ++indent;
        return fout;
    }

    shared_ptr<_Translator::StorageBase> _Translator::parse_symbol(const string & block_title, const string & s, const string & reg)
    {
        if (s.empty()) Error.compile_error("Invalid symbol");
        if (s[0] == '%' || s[0] == '#' || s[0] == '@') return storage_map[block_title][s];
        if ((s[0] <= '9' && s[0] >= '0') || s[0] == '-')
        {
            shared_ptr<StorageBase> ret(new RegisterModel(s));
            return ret;
        }
        if (s[0] == '+')
        {
            shared_ptr<StorageBase> ret(new RegisterModel(s.substr(1)));
            return ret;
        }
        if (s == "null") return nullptr;
        if (s.substr(0, 7) == "offset ")
        {
            stringstream sss;
            sss << s;
            string base_reg, offset;
            sss >> base_reg;
            sss >> base_reg >> offset;
            if (offset[0] <= '9' && offset[0] >= '0') offset = "+ " + offset;
            auto cons = parse_symbol(block_title, base_reg, reg);
            auto cons1 = cons -> name_as_src(reg);
            cons = parse_symbol(block_title, offset);
            if (cons1 != reg)
                *(StorageBase::sout) << "\tmov\t" << reg << ", " << cons1 << "\n";
            cons -> addonto(reg);
            shared_ptr<StorageBase> ret(new MemoryModel(reg, ""));
            return ret;
        }
        if (s.substr(0, 11) == "__conststr_")
        {
            shared_ptr<StorageBase> ret(new RegisterModel(s));
            return ret;
        }
        {
            if (!is_in(s, global_remap)) global_remap[s] = global_remap.size();
            shared_ptr<StorageBase> ret(new MemoryModel("rel ", "__msharp__global__" + s));
            return ret;
        }
    }

    _Translator::_Translator(_Parser & __Parser)
        : Parser(__Parser)
    {
    }

    void _Translator::proc_new(ASTNode & tree)
    {
        intermediate_repr.push_back(IR(IR__malloc));
        intermediate_repr.back().args = deque<string>(tree.son[1].translation_tag.begin() + 1, tree.son[1].translation_tag.end());
        intermediate_repr.back().args.push_front(tree.translation_tag.back());
    }

    void _Translator::add_member(ASTNode & tree, NameController & name_table)
    {
        if (tree.type == node_fun_def)
        {
            member_function_table[context_information].insert(tree.son[1].description);
            tree.son[1].description = "__" + context_information + "__" + tree.son[1].description + "__";
        } else if (tree.type == node_var_def) {
            ASTNode & type_node(tree.son.front());
            auto class_desc = name_table.fetch_class(type_node.description);
            class_member_offset[scope_title]["#sum#"] = 0;
            for (auto i = tree.son.begin() + 1; i != tree.son.end(); ++i)
            {
                class_member_offset[scope_title][i -> description] = class_offset_base;
                class_offset_base += class_desc.byte_num;
                class_member_offset[scope_title]["#sum#"] = class_offset_base;
            }
        } else {
            ASTNode & type_node(tree.son.front());
            need_construction.insert(type_node.description);
        }
    }

    void _Translator::type_reload(NameController & name_table)
    {
        for (ASTNode & tree : Parser.forest)
        {
            if (tree.type == node_class_def)
            {
                class_offset_base = 0;
                context_information = tree.son[0].description;
                scope_title = tree.son[0].description;
                for (ASTNode & ason : tree.son[1].son)
                {
                    add_member(ason, name_table);
                }
                context_information = "";
            }

        }
    }

    void _Translator::translate(ASTNode & tree, NameController & name_table)
    {
        switch (tree.type)
        {
        case node_phrase: case node_dual_def: case node_op: case unknown_node: case node_index_para:
            Error.compile_error("fatal error : codegen failed.");
        break;
        case node_class_def:
            class_offset_base = 0;
            context_information = "in_class";
            context_detail = tree.son[0].description;
            for (ASTNode & ason : tree.son[1].son)
            {
                if (ason.type != node_var_def) translate(ason, name_table);
            }
            context_information = "";
            context_detail = "#noclass#";
            scope_title = tree.son[0].description;
        break;
        case node_initexpr:
            /*if (name_table.is_global())
                intermediate_repr.push_back(IR(IR__global_init));*/
            if (name_table.is_global())
                std::swap(intermediate_repr, init_repr);
#ifndef __infinity__reg__assumption
            calculation_cached_local_var_number = 0;
#endif
            for (ASTNode & ason : tree.son) translate(ason, name_table);
            if (name_table.is_global())
                std::swap(intermediate_repr, init_repr);
            /*if (name_table.is_global())
                intermediate_repr.push_back(IR(IR__global_init_end));*/
        break;
        case node_name: case node_type:
            if ((tree.inferred_type == nullptr) ? false : tree.inferred_type -> abstract_type._class)
                tree.translation_tag = {"type", tree.description};
            else if (tree.description == "null")
                tree.translation_tag.push_back("0");
            else if (tree.description == "true")
                tree.translation_tag.push_back("1");
            else if (tree.description == "false")
                tree.translation_tag.push_back("0");
            else if (tree.description == "this")
                tree.translation_tag.push_back("@0");
            else if (is_in(tree.description, argument_remap))
                tree.translation_tag.push_back(argument_remap[tree.description]);
            else if (context_detail != "#noclass#" && is_in(tree.description, member_function_table[context_detail]))
            {
                obj_title = "@0";
                tree.translation_tag = {"__" + context_detail + "__" + tree.description + "__"};
            }
            else if (local_remap[tree.description].empty())
                tree.translation_tag.push_back(tree.description);
            else
                tree.translation_tag.push_back("%" + Tool.to_string(local_remap[tree.description].top()));
        break;
        case node_fun_def:
        {
            if (context_information == "")
            {
                argument_remap.clear();
                ASTNode & name_node(tree.son[1]);
                intermediate_repr.push_back(IR(IR__func));
                intermediate_repr.back().args.push_back(name_node.description);
                intermediate_repr.back().args.push_back(Tool.to_string(tree.son[2].son.size()));
                if (name_node.description == "main")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {string("#0"), string("__init__")};
                }
                name_table.move_into_scope(tree.scoped_namespace.get());
                for (int i = 2; i < int(tree.son.size()); ++i) translate(tree.son[i], name_table);
                name_table.leave_scope();
            } else {
                ASTNode & name_node(tree.son[1]);
                member_function_table[context_detail].insert(name_node.description);
                context_information = "";
                argument_remap.clear();
                intermediate_repr.push_back(IR(IR__func));
                intermediate_repr.back().args.push_back(name_node.description);
                intermediate_repr.back().args.push_back(Tool.to_string(tree.son[2].son.size() + 1));
                intermediate_repr.push_back(IR(IR__arg));
                intermediate_repr.back().args.push_back(Tool.to_string(8));
                string name = "@" + Tool.to_string(argument_remap.size());
                intermediate_repr.back().args.push_back("this");
                intermediate_repr.back().args.push_back(name);
                argument_remap["this"] = name;
                name_table.move_into_scope(tree.scoped_namespace.get());
                translate(tree.son[2], name_table);
                for (auto & p : class_member_offset[context_detail])
                {
                    if (p.first == "#sum#") continue;
                    string new_title = "offset @0 " + Tool.to_string(p.second);
                    argument_remap[p.first] = new_title;
                    intermediate_repr.push_back(IR(IR__arg));
                    intermediate_repr.back().args.push_back(Tool.to_string(8));
                    intermediate_repr.back().args.push_back(p.first);
                    intermediate_repr.back().args.push_back(new_title);
                }
                translate(tree.son[3], name_table);
                name_table.leave_scope();
                context_information = "in_class";
            }
            intermediate_repr.push_back(IR(IR__ret));
            intermediate_repr.back().args = {"null"};
        }
        break;
        case node_expr:
        {
#ifndef __infinity__reg__assumption
            calculation_cached_local_var_number = 0;
#endif
            for (ASTNode & ason : tree.son) translate(ason, name_table);
        }
        break;
        case node_expr_single_line:
        {
            if (tree.son.empty())
            {
                tree.translation_tag = {"1"};
            } else {
                for (ASTNode & ason : tree.son) translate(ason, name_table);
                tree.translation_tag = tree.son.back().translation_tag;
            }
        }
        break;
        case node_member_list:
        {
            for (ASTNode & ason : tree.son) translate(ason, name_table);
        }
        break;
        case node_closure:
        {
            current_scope.push(deque<string>());
            for (ASTNode & ason : tree.son) translate(ason, name_table);
            for (const string & var_title : current_scope.top())
            {
                local_remap[var_title].pop();
                local_var_size.pop_back();
            }
            current_scope.pop();
        }
        break;
        case node_fun_signature:
        {
            for (ASTNode & ason : tree.son)
            {
                translate(ason, name_table);
            }
        }
        break;
        case node_constructor_def:
        {
            argument_remap.clear();
            ASTNode & name_node(tree.son[0]);
            special_construction.insert(name_node.description);
            intermediate_repr.push_back(IR(IR__func));
            intermediate_repr.back().args.push_back(name_node.description);
            intermediate_repr.back().args.push_back(Tool.to_string(tree.son[2].son.size() + 1));
            name_table.move_into_scope(tree.scoped_namespace.get());
            intermediate_repr.push_back(IR(IR__arg));
            intermediate_repr.back().args.push_back(Tool.to_string(8));
            string name = "@" + Tool.to_string(argument_remap.size());
            intermediate_repr.back().args.push_back("this");
            intermediate_repr.back().args.push_back(name);
            argument_remap["this"] = name;
            translate(tree.son[1], name_table);
            for (auto & p : class_member_offset[context_detail])
            {
                if (p.first == "#sum#") continue;
                string new_title = "offset @0 " + Tool.to_string(p.second);
                argument_remap[p.first] = new_title;
                intermediate_repr.push_back(IR(IR__arg));
                intermediate_repr.back().args.push_back(Tool.to_string(8));
                intermediate_repr.back().args.push_back(p.first);
                intermediate_repr.back().args.push_back(new_title);
            }
            intermediate_repr.push_back(IR(IR__malloc));
            intermediate_repr.back().args.push_back("@0");
            intermediate_repr.back().args.push_back(Tool.to_string(class_member_offset[context_detail]["#sum#"] / 8));
            translate(tree.son[2], name_table);
            intermediate_repr.push_back(IR(IR__ret));
            intermediate_repr.back().args.push_back("@0");
            name_table.leave_scope();
        }
        break;
        case node_sing_para:
        {
            ASTNode & type_node(tree.son.front());
            ASTNode & name_node(tree.son[1]);
            ClassDescriptor type_des = name_table.fetch_class(type_node.description);
            intermediate_repr.push_back(IR(IR__arg));
            intermediate_repr.back().args.push_back(Tool.to_string(type_des.byte_num));
            string name = "@" + Tool.to_string(argument_remap.size());
            intermediate_repr.back().args.push_back(name_node.description);
            intermediate_repr.back().args.push_back(name);
            argument_remap[name_node.description] = name;
        }
        break;
        case node_var_def:
        {
            ASTNode & type_node(tree.son.front());
            if (name_table.is_global())
            {
                IRType ir_type;
                /*if (type_node.description == "string")      ir_type = IR__quad;
                    else if (type_node.description == "bool")   ir_type = IR__byte;
                    else if (type_node.description == "int")    ir_type = IR__word;
                    else */ir_type = IR__quad;
                for (auto i = tree.son.begin() + 1; i != tree.son.end(); ++i)
                {
                    intermediate_repr.push_back(IR(ir_type));
                    intermediate_repr.back().args.push_back(i -> description);
                    translate(*i, name_table);
                }
            } else {
                auto class_desc = name_table.fetch_class(type_node.description);
                IRType ir_type = IR__var;
                for (auto i = tree.son.begin() + 1; i != tree.son.end(); ++i)
                {
                    intermediate_repr.push_back(IR(ir_type));
                    intermediate_repr.back().args.push_back(Tool.to_string(class_desc.byte_num));
                    current_scope.top().push_back(i -> description);
                    local_remap[i -> description].push(local_var_size.size());
                    local_var_size.push_back(class_desc.byte_num);
                    intermediate_repr.back().args.push_back("%" + Tool.to_string(local_remap[i -> description].top()));
                    translate(*i, name_table);
                }
            }
        }
        break;
        case node_expr_tree:
        {
            auto op_label = ASTNode::operator_label(tree.description);
            if (op_label != __assign && op_label != __get_member)
            {
                ++calculation_cached_local_var_number;
            }
            switch (op_label)
            {
            case __logical_and:
            case __logical_or:
            case __get_member:
                tree.translation_tag.push_back("#" + Tool.to_string(calculation_cached_local_var_number - 1));
                translate(tree.son[0], name_table);
            break;
            case __index:
            break;
            default:
                tree.translation_tag.push_back("#" + Tool.to_string(calculation_cached_local_var_number - 1));
                for (ASTNode & ason : tree.son) translate(ason, name_table);
            break;
            }
            switch (op_label)
            {
            case __assign:
                intermediate_repr.push_back(IR(IR__assign));
                tree.translation_tag.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __add:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__connect__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__add));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __sub:
                intermediate_repr.push_back(IR(IR__sub));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __multiplies:
                intermediate_repr.push_back(IR(IR__mul));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __bit_and:
                intermediate_repr.push_back(IR(IR__band));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __logical_or:
            {
                string label_idx(Tool.to_string(if_count));
                ++if_count;
                intermediate_repr.push_back(IR(IR__cond));
                intermediate_repr.back().args = {label_idx, tree.son[0].translation_tag.back()};
                intermediate_repr.push_back(IR(IR__assign));
                intermediate_repr.back().args = {tree.translation_tag.back(), "1"};
                intermediate_repr.push_back(IR(IR__cond_else));
                intermediate_repr.back().args = {label_idx};
                translate(tree.son[1], name_table);
                intermediate_repr.push_back(IR(IR__assign));
                intermediate_repr.back().args = {tree.translation_tag.back(), tree.son[1].translation_tag.back()};
                intermediate_repr.push_back(IR(IR__cond_end));
                intermediate_repr.back().args = {label_idx};
            }
            break;
            case __bit_or:
                intermediate_repr.push_back(IR(IR__bor));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __logical_and:
            {
                string label_idx(Tool.to_string(if_count));
                ++if_count;
                intermediate_repr.push_back(IR(IR__cond));
                intermediate_repr.back().args = {label_idx, tree.son[0].translation_tag.back()};
                translate(tree.son[1], name_table);
                intermediate_repr.push_back(IR(IR__assign));
                intermediate_repr.back().args = {tree.translation_tag.back(), tree.son[1].translation_tag.back()};
                intermediate_repr.push_back(IR(IR__cond_else));
                intermediate_repr.back().args = {label_idx};
                intermediate_repr.push_back(IR(IR__assign));
                intermediate_repr.back().args = {tree.translation_tag.back(), "0"};
                intermediate_repr.push_back(IR(IR__cond_end));
                intermediate_repr.back().args = {label_idx};
            }
            break;
            case __bit_xor:
                intermediate_repr.push_back(IR(IR__xor));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __logical_not:
                intermediate_repr.push_back(IR(IR__lnot));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __bit_not:
                 intermediate_repr.push_back(IR(IR__not));
                 intermediate_repr.back().args.push_back(tree.translation_tag.back());
                 intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
             break;
            case __equal_to:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__equals__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__eq));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __not_equal_to:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__neq__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__neq));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __less:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__less__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__less));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;

            case __less_equal:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__leq__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__leq));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __greater:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__greater__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__greater));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __greater_equal:
            {
                InferredType & info(*tree.son[1].inferred_type);
                if (info.basic_sign == "string")
                {
                    intermediate_repr.push_back(IR(IR__call));
                    intermediate_repr.back().args = {tree.translation_tag.back(), "__string__geq__", tree.son[0].translation_tag.back(), tree.son[1].translation_tag.back()};
                    return;
                }
                intermediate_repr.push_back(IR(IR__geq));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            }
            break;
            case __shr:
                intermediate_repr.push_back(IR(IR__shr));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __shl:
                intermediate_repr.push_back(IR(IR__shl));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __divides:
                intermediate_repr.push_back(IR(IR__div));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __modulus:
                intermediate_repr.push_back(IR(IR__mod));
                intermediate_repr.back().args.push_back(tree.translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
            break;
            case __inc:
                if (tree.son[0].translation_tag.back() == "null")
                {
                    tree.translation_tag.push_back(tree.son[1].translation_tag.back());
                    --calculation_cached_local_var_number;
                    intermediate_repr.push_back(IR(IR__inc));
                    intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
                } else {
                    intermediate_repr.push_back(IR(IR__assign));
                    intermediate_repr.back().args.push_back(tree.translation_tag.back());
                    intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                    intermediate_repr.push_back(IR(IR__inc));
                    intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                }
            break;
            case __dec:
                if (tree.son[0].translation_tag.back() == "null")
                {
                    tree.translation_tag.push_back(tree.son[1].translation_tag.back());
                    --calculation_cached_local_var_number;
                    intermediate_repr.push_back(IR(IR__dec));
                    intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
                } else {
                    intermediate_repr.push_back(IR(IR__assign));
                    intermediate_repr.back().args.push_back(tree.translation_tag.back());
                    intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                    intermediate_repr.push_back(IR(IR__dec));
                    intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                }
            break;
            case __index:
                translate(tree.son[0], name_table);
                if (tree.son[0].translation_tag.front() == "type")
                {
                    //--calculation_cached_local_var_number;
                    translate(tree.son[1], name_table);
                    deque<string> & args(tree.translation_tag);
                    tree.translation_tag = std::move(tree.son[0].translation_tag);
                    args.push_back(tree.son[1].translation_tag.back());
                } else {
                    if (tree.son[0].translation_tag.back().substr(0, 7) == "offset ")
                    {
                        ++calculation_cached_local_var_number;
                        tree.translation_tag.push_back("#" + Tool.to_string(calculation_cached_local_var_number - 1));
                        intermediate_repr.push_back(IR(IR__assign));
                        intermediate_repr.back().args.push_back(tree.translation_tag.back());
                        intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                        tree.son[0].translation_tag = {tree.translation_tag.back()};
                    }
                    translate(tree.son[1], name_table);
                    if (tree.son[1].translation_tag.back()[0] == '-' || (tree.son[1].translation_tag.back()[0] <= '9' && tree.son[1].translation_tag.back()[0] >= '0'))
                    {
                        stringstream sin;
                        sin << tree.son[1].translation_tag.back();
                        int s; sin >> s;
                        tree.son[1].translation_tag.back() = Tool.to_string(s * 8);
                    }
                    else // if (tree.son[1].translation_tag.back().substr(0, 7) == "offset ")
                    {
                        tree.translation_tag.push_back("#" + Tool.to_string(calculation_cached_local_var_number));
                        intermediate_repr.push_back(IR(IR__shl));
                        intermediate_repr.back().args.push_back(tree.translation_tag.back());
                        intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
                        intermediate_repr.back().args.push_back("3");
                        ++calculation_cached_local_var_number;
                        tree.son[1].translation_tag = {tree.translation_tag.back()};
                    }
                    {
                        tree.translation_tag = {"offset " + tree.son[0].translation_tag.back() + " " + tree.son[1].translation_tag.back()};
                    }
                }
            break;
            case __get_member:
            {
                InferredType & class_desc = *tree.son[0].inferred_type;
                if (class_desc.array_dimension > 0)
                {
                    tree.translation_tag = {string("__array__size__")};
                    obj_title = tree.son[0].translation_tag.back();
                    return;
                }
                string & member_title = tree.son[1].description;
                if (!is_in(member_title, class_member_offset[class_desc.basic_sign]))
                {
                    tree.translation_tag = {"__" + class_desc.basic_sign + "__" + member_title + "__"};
                    obj_title = tree.son[0].translation_tag.back();
                    return;
                }
                if (tree.son[0].translation_tag.back().substr(0, 6) == "offset")
                {
                    ++calculation_cached_local_var_number;
                    tree.translation_tag = {("#" + Tool.to_string(calculation_cached_local_var_number - 1))};
                    intermediate_repr.push_back(IR(IR__assign));
                    intermediate_repr.back().args.push_back(tree.translation_tag.back());
                    intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                    tree.translation_tag.back() = "offset " + tree.translation_tag.back() + " " + Tool.to_string(class_member_offset[class_desc.basic_sign][member_title]);
                } else {
                    tree.translation_tag.back() = "offset " + tree.son[0].translation_tag.back() + " " + Tool.to_string(class_member_offset[class_desc.basic_sign][member_title]);
                }
            }
            break;
            case __new_op:
            {
                if (tree.son[0].translation_tag.back() != "null" || tree.son[1].translation_tag.empty())
                    Error.compile_error("Invalid use of operator new.");
                string type_tag = tree.son[1].translation_tag[1];
                int dimension_cnter = tree.son[1].translation_tag.size() - 2;
                deque<string> and_no(tree.son[1].translation_tag.begin() + 1, tree.son[1].translation_tag.end());
                deque<string> sharp_no = {tree.translation_tag.back()};
                deque<string> off_no = {tree.translation_tag.back()};
                int n = dimension_cnter;
                while (tree.son[1].translation_tag[n + 1] == "null" && n > 0) --n;
                for (int i = 0; i < n + 1; ++i) sharp_no.push_back("#" + Tool.to_string(calculation_cached_local_var_number + i));
                for (int i = n + 1; i < n * 2 + 2; ++i) off_no.push_back("#" + Tool.to_string(calculation_cached_local_var_number + i));
                calculation_cached_local_var_number += dimension_cnter * 2 + 2;


                if (is_in(type_tag, set<string>({string("int"), string("bool"), string("string")})) || n != dimension_cnter)
                {
                    deque<IR> tmp_ir;
                    if (n == 0) return;
                    if (n == 1)
                    {
                        intermediate_repr.push_back(IR(IR__malloc));
                        intermediate_repr.back().args = {tree.translation_tag.back(), and_no[1]};
                        return;
                    }
                    {
                        tmp_ir.push_back(IR(IR__malloc));
                        tmp_ir.back().args = {"offset " + off_no[n] + " 0", and_no[n]};
                    }
                    for (int k = n - 1; k > 1; --k)
                    {
                        tmp_ir.push_front(IR(IR__add));
                        tmp_ir.front().args = {off_no[k + 1], off_no[k + 1], "offset " + off_no[k] + " 0"};
                        tmp_ir.push_front(IR(IR__shl));
                        tmp_ir.front().args = {off_no[k + 1], sharp_no[k], "3"};
                        tmp_ir.push_front(IR(IR__loop_body));
                        tmp_ir.front().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_front(IR(IR__loop_constraint));
                        tmp_ir.front().args = {Tool.to_string(loop_count), sharp_no[k + 1]};
                        tmp_ir.push_front(IR(IR__less));
                        tmp_ir.front().args = {sharp_no[k + 1], sharp_no[k], and_no[k]};
                        tmp_ir.push_front(IR(IR__loop_start));
                        tmp_ir.front().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_front(IR(IR__assign));
                        tmp_ir.front().args = {sharp_no[k], "0"};
                        tmp_ir.push_front(IR(IR__malloc));
                        tmp_ir.front().args = {"offset " + off_no[k] + " 0", and_no[k]};
                        tmp_ir.push_back(IR(IR__loop_iter));
                        tmp_ir.back().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_back(IR(IR__inc));
                        tmp_ir.back().args = {sharp_no[k]};
                        tmp_ir.push_back(IR(IR__loop_end));
                        tmp_ir.back().args = {Tool.to_string(loop_count)};
                        ++loop_count;
                    }
                    {
                        int k = 1;
                        tmp_ir.push_front(IR(IR__add));
                        tmp_ir.front().args = {off_no[k + 1], off_no[k + 1], off_no[k]};
                        tmp_ir.push_front(IR(IR__shl));
                        tmp_ir.front().args = {off_no[k + 1], sharp_no[k], "3"};
                        tmp_ir.push_front(IR(IR__loop_body));
                        tmp_ir.front().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_front(IR(IR__loop_constraint));
                        tmp_ir.front().args = {Tool.to_string(loop_count), sharp_no[k + 1]};
                        tmp_ir.push_front(IR(IR__less));
                        tmp_ir.front().args = {sharp_no[k + 1], sharp_no[k], and_no[k]};
                        tmp_ir.push_front(IR(IR__loop_start));
                        tmp_ir.front().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_front(IR(IR__assign));
                        tmp_ir.front().args = {sharp_no[k], "0"};
                        tmp_ir.push_front(IR(IR__malloc));
                        tmp_ir.front().args = {off_no[k], and_no[k]};
                        tmp_ir.push_back(IR(IR__loop_iter));
                        tmp_ir.back().args = {Tool.to_string(loop_count)};
                        tmp_ir.push_back(IR(IR__inc));
                        tmp_ir.back().args = {sharp_no[k]};
                        tmp_ir.push_back(IR(IR__loop_end));
                        tmp_ir.back().args = {Tool.to_string(loop_count)};
                        ++loop_count;
                    }
                    for (auto & i : tmp_ir) intermediate_repr.push_back(std::move(i));
                    intermediate_repr.push_back(IR(IR__assign));
                    intermediate_repr.back().args = {tree.translation_tag.back(), off_no[1]};
                } else {
                    deque<IR> tmp_ir;
                    if (dimension_cnter == 0)
                    {
                        if (is_in(type_tag, special_construction))
                        {
                            intermediate_repr.push_back(IR(IR__call));
                            intermediate_repr.back().args.push_back(tree.translation_tag.back());
                            intermediate_repr.back().args.push_back(type_tag);
                        } else {
                            intermediate_repr.push_back(IR(IR__malloc));
                            intermediate_repr.back().args.push_back(tree.translation_tag.back());
                            intermediate_repr.back().args.push_back(Tool.to_string(class_member_offset[type_tag]["#sum#"] / 8));
                        }
                    } else {
                            /*
                             * mov      #n, 0
                             * loop_start
                             * less     #(n + 1), #n, &n
                             * loop_constraint #(n + 1)
                             * loop_body
                             * call     offset %(n - 1) #n, T
                             * loop_iter
                             * inc      #n
                             * loop_end
                             */
                        if (n == 0) return;
                        if (n == 1)
                        {
                            deque<IR> & tmp_ir(intermediate_repr);
                            intermediate_repr.push_back(IR(IR__malloc));
                            intermediate_repr.back().args = {tree.translation_tag.back(), and_no[1]};
                            tmp_ir.push_back(IR(IR__assign));
                            tmp_ir.back().args = {off_no[n + 1], tree.translation_tag.back()};
                            tmp_ir.push_back(IR(IR__assign));
                            tmp_ir.back().args = {sharp_no[n], "0"};
                            tmp_ir.push_back(IR(IR__loop_start));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__shr));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "3"};
                            tmp_ir.push_back(IR(IR__less));
                            tmp_ir.back().args = {sharp_no[n + 1], sharp_no[n], and_no[n]};
                            tmp_ir.push_back(IR(IR__shl));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "3"};
                            tmp_ir.push_back(IR(IR__loop_constraint));
                            tmp_ir.back().args = {Tool.to_string(loop_count), sharp_no[n + 1]};
                            tmp_ir.push_back(IR(IR__loop_body));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            if (is_in(type_tag, special_construction))
                            {
                                tmp_ir.push_back(IR(IR__call));
                                tmp_ir.back().args = {"offset " + off_no[n + 1] + " " + sharp_no[n], type_tag};
                            } else {
                                tmp_ir.push_back(IR(IR__malloc));
                                tmp_ir.back().args.push_back("offset " + off_no[n + 1] + " " + sharp_no[n]);
                                tmp_ir.back().args.push_back(Tool.to_string(class_member_offset[type_tag]["#sum#"] / 8));
                            }
                            tmp_ir.push_back(IR(IR__loop_iter));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__add));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "8"};
                            tmp_ir.push_back(IR(IR__loop_end));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            ++loop_count;
                            return;
                        }
                        {
                            tmp_ir.push_back(IR(IR__malloc));
                            tmp_ir.back().args = {"offset " + off_no[n] + " 0", and_no[n]};
                            tmp_ir.push_back(IR(IR__assign));
                            tmp_ir.back().args = {off_no[n + 1], "offset " + off_no[n] + " 0"};
                            tmp_ir.push_back(IR(IR__assign));
                            tmp_ir.back().args = {sharp_no[n], "0"};
                            tmp_ir.push_back(IR(IR__loop_start));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__shr));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "3"};
                            tmp_ir.push_back(IR(IR__less));
                            tmp_ir.back().args = {sharp_no[n + 1], sharp_no[n], and_no[n]};
                            tmp_ir.push_back(IR(IR__shl));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "3"};
                            tmp_ir.push_back(IR(IR__loop_constraint));
                            tmp_ir.back().args = {Tool.to_string(loop_count), sharp_no[n + 1]};
                            tmp_ir.push_back(IR(IR__loop_body));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            if (is_in(type_tag, special_construction))
                            {
                                tmp_ir.push_back(IR(IR__call));
                                tmp_ir.back().args = {"offset " + off_no[n + 1] + " " + sharp_no[n], type_tag};
                            } else {
                                tmp_ir.push_back(IR(IR__malloc));
                                tmp_ir.back().args.push_back("offset " + off_no[n + 1] + " " + sharp_no[n]);
                                tmp_ir.back().args.push_back(Tool.to_string(class_member_offset[type_tag]["#sum#"] / 8));
                            }
                            tmp_ir.push_back(IR(IR__loop_iter));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__add));
                            tmp_ir.back().args = {sharp_no[n], sharp_no[n], "8"};
                            tmp_ir.push_back(IR(IR__loop_end));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            ++loop_count;
                        }
                        for (int k = n - 1; k > 1; --k)
                        {
                            tmp_ir.push_front(IR(IR__add));
                            tmp_ir.front().args = {off_no[k + 1], off_no[k + 1], "offset " + off_no[k] + " 0"};
                            tmp_ir.push_front(IR(IR__shl));
                            tmp_ir.front().args = {off_no[k + 1], sharp_no[k], "3"};
                            tmp_ir.push_front(IR(IR__loop_body));
                            tmp_ir.front().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_front(IR(IR__loop_constraint));
                            tmp_ir.front().args = {Tool.to_string(loop_count), sharp_no[k + 1]};
                            tmp_ir.push_front(IR(IR__less));
                            tmp_ir.front().args = {sharp_no[k + 1], sharp_no[k], and_no[k]};
                            tmp_ir.push_front(IR(IR__loop_start));
                            tmp_ir.front().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_front(IR(IR__assign));
                            tmp_ir.front().args = {sharp_no[k], "0"};
                            tmp_ir.push_front(IR(IR__malloc));
                            tmp_ir.front().args = {"offset " + off_no[k] + " 0", and_no[k]};
                            tmp_ir.push_back(IR(IR__loop_iter));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__inc));
                            tmp_ir.back().args = {sharp_no[k]};
                            tmp_ir.push_back(IR(IR__loop_end));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            ++loop_count;
                        }
                        {
                            int k = 1;
                            tmp_ir.push_front(IR(IR__add));
                            tmp_ir.front().args = {off_no[k + 1], off_no[k + 1], off_no[k]};
                            tmp_ir.push_front(IR(IR__shl));
                            tmp_ir.front().args = {off_no[k + 1], sharp_no[k], "3"};
                            tmp_ir.push_front(IR(IR__loop_body));
                            tmp_ir.front().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_front(IR(IR__loop_constraint));
                            tmp_ir.front().args = {Tool.to_string(loop_count), sharp_no[k + 1]};
                            tmp_ir.push_front(IR(IR__less));
                            tmp_ir.front().args = {sharp_no[k + 1], sharp_no[k], and_no[k]};
                            tmp_ir.push_front(IR(IR__loop_start));
                            tmp_ir.front().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_front(IR(IR__assign));
                            tmp_ir.front().args = {sharp_no[k], "0"};
                            tmp_ir.push_front(IR(IR__malloc));
                            tmp_ir.front().args = {off_no[k], and_no[k]};
                            tmp_ir.push_back(IR(IR__loop_iter));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            tmp_ir.push_back(IR(IR__inc));
                            tmp_ir.back().args = {sharp_no[k]};
                            tmp_ir.push_back(IR(IR__loop_end));
                            tmp_ir.back().args = {Tool.to_string(loop_count)};
                            ++loop_count;
                        }
                        for (auto & i : tmp_ir) intermediate_repr.push_back(std::move(i));
                        intermediate_repr.push_back(IR(IR__assign));
                        intermediate_repr.back().args = {tree.translation_tag.back(), off_no[1]};
                    }
                }
            }
            break;
            case __unknown_op:
                Error.compile_error("Translator failed.");
            break;
            }
        }
        break;
        case node_ambiguity_handler:
        {
            tree.translation_tag.push_back("null");
        }
        break;
        case node_const_int: case node_const_str:
        {
            tree.translation_tag.push_back(tree.description);
        }
        break;

        case node_func_call:
        {
            int answer_storage_idx = calculation_cached_local_var_number;
            ++calculation_cached_local_var_number;
            tree.translation_tag.push_back("#" + Tool.to_string(answer_storage_idx));
            for (ASTNode & ason : tree.son) translate(ason, name_table);
            intermediate_repr.push_back(IR(IR__call));
            intermediate_repr.back().args.push_back(tree.translation_tag.back());
            intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
            if (obj_title != "null")
            {
                intermediate_repr.back().args.push_back(obj_title);
                obj_title = "null";
            }
            for (ASTNode & ason : tree.son[1].son) intermediate_repr.back().args.push_back(ason.translation_tag.back());
            calculation_cached_local_var_number = answer_storage_idx + 1;
        }
        break;
        case node_func_call_para:
        {
            for (ASTNode & ason : tree.son) translate(ason, name_table);
        }
        break;
        case node_keyword_flow:
        {
            auto keyword_label = ASTNode :: control_flow_label(tree.description);
            switch (keyword_label)
            {
            case __if:
            {
                string label_idx(Tool.to_string(if_count));
                ++if_count;
                translate(tree.son[0], name_table);
                intermediate_repr.push_back(IR(IR__cond));
                intermediate_repr.back().args.push_back(label_idx);
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                translate(tree.son[1], name_table);
                intermediate_repr.push_back(IR(IR__cond_else));
                intermediate_repr.back().args.push_back(label_idx);
                if (tree.son.size() > 2)
                {
                    translate(tree.son[2], name_table);
                }
                intermediate_repr.push_back(IR(IR__cond_end));
                intermediate_repr.back().args.push_back(label_idx);
            }
            break;
            case __for:
            {
                current_scope.push(deque<string>());
                string label_idx(Tool.to_string(loop_count));
                ++loop_count;
                translate(tree.son[0], name_table);
                intermediate_repr.push_back(IR(IR__loop_start));
                intermediate_repr.back().args.push_back(label_idx);
                translate(tree.son[1], name_table);
                intermediate_repr.push_back(IR(IR__loop_constraint));
                intermediate_repr.back().args.push_back(label_idx);
                intermediate_repr.back().args.push_back(tree.son[1].translation_tag.back());
                deque<IR> tmp_ir;
                swap(intermediate_repr, tmp_ir);
                intermediate_repr.push_back(IR(IR__loop_iter));
                intermediate_repr.back().args.push_back(label_idx);
                translate(tree.son[2], name_table);
                swap(intermediate_repr, tmp_ir);
                intermediate_repr.push_back(IR(IR__loop_body));
                intermediate_repr.back().args.push_back(label_idx);
                translate(tree.son[3], name_table);
                for (auto & s : tmp_ir) intermediate_repr.push_back(std::move(s));
                tmp_ir.clear();
                intermediate_repr.push_back(IR(IR__loop_end));
                intermediate_repr.back().args.push_back(label_idx);
                for (const string & var_title : current_scope.top())
                {
                    local_remap[var_title].pop();
                    local_var_size.pop_back();
                }
                current_scope.pop();
            }
            break;
            case __while:
            {
                string label_idx(Tool.to_string(loop_count));
                ++loop_count;
                intermediate_repr.push_back(IR(IR__loop_start));
                intermediate_repr.back().args.push_back(label_idx);
                translate(tree.son[0], name_table);
                intermediate_repr.push_back(IR(IR__loop_constraint));
                intermediate_repr.back().args.push_back(label_idx);
                intermediate_repr.back().args.push_back(tree.son[0].translation_tag.back());
                intermediate_repr.push_back(IR(IR__loop_body));
                intermediate_repr.back().args.push_back(label_idx);
                translate(tree.son[1], name_table);
                intermediate_repr.push_back(IR(IR__loop_iter));
                intermediate_repr.back().args.push_back(label_idx);
                intermediate_repr.push_back(IR(IR__loop_end));
                intermediate_repr.back().args.push_back(label_idx);
            }
            break;
            case __return:
            {
                if (tree.son.empty())
                {
                    intermediate_repr.push_back(IR(IR__ret));
                    intermediate_repr.back().args.push_back("@0");
                    return;
                }
                for (ASTNode & ason : tree.son) translate(ason, name_table);
                intermediate_repr.push_back(IR(IR__ret));
                for (ASTNode & ason : tree.son) intermediate_repr.back().args.push_back(ason.translation_tag.back());
            }
            break;
            case __continue:
            {
                intermediate_repr.push_back(IR(IR__continue));
            }
            break;
            case __break:
            {
                intermediate_repr.push_back(IR(IR__break));
            }
            break;
            case __else: case __new: case __unknown_flow:
            {
                Error.compile_error("Translation failed.");
            }
            break;
            }
        }
        break;
        }
    }

    void _Translator::print(ostream & fout)
    {
        for (IR & thisir : intermediate_repr)
        {
            fout << thisir << endl;
        }
    }
#ifndef __infinity__reg__assumption
    std::ostream * _Translator::StorageBase::sout = nullptr;
#else
    Refiner * _Translator::StorageBase::sout = nullptr;
#endif
    void _Translator::execute()
    {
        for (ASTNode & tree : Parser.forest)
        {
            translate(tree, Parser.name_table);
        }
        intermediate_repr.push_front(IR(IR__ret));
        intermediate_repr.front().args = {"null"};

        for (auto i = init_repr.rbegin(); i != init_repr.rend(); ++i)
        {
            auto & t = *i;
            intermediate_repr.push_front(t);
        }

        intermediate_repr.push_front(IR(IR__func));
        intermediate_repr.front().args.push_back("__init__");
        intermediate_repr.front().args.push_back("0");
    }
    //string _Translator::__sys_lib = "default rel\nglobal __msharp__malloc__\nglobal __string__connect__\nglobal __string__length__\nglobal __string__ord__\nglobal __string__parseInt__\nglobal __array__size__\nglobal getString\nglobal getInt\nglobal print\nglobal println\nglobal toString\nglobal __string__substring__\nglobal __string__less__\nglobal __string__greater__\nglobal __string__equals__\nglobal __string__neq__\nglobal main\nextern puts\nextern _IO_putc\nextern stdout\nextern _IO_getc\nextern stdin\nextern gets\nextern malloc\nSECTION .text\n__msharp__malloc__:\n        push    rbx\n        mov     rbx, rdi\n        lea     rdi, [rdi*8+8H]\n        call    malloc\n        mov     qword [rax], rbx\n        add     rax, 8\n        pop     rbx\n        ret\nALIGN   8\n__string__connect__:\n        push    r14\n        push    r13\n        mov     r13, rsi\n        push    r12\n        mov     r12, rdi\n        push    rbp\n        push    rbx\n        mov     rbx, qword [rdi-8H]\n        mov     rbp, qword [rsi-8H]\n        lea     r14, [rbx+rbp]\n        lea     rdi, [r14+9H]\n        call    malloc\n        test    rbx, rbx\n        mov     rcx, rax\n        mov     qword [rax], r14\n        lea     rax, [rax+8H]\n        jle     L_002\n        lea     rdx, [rcx+18H]\n        cmp     r12, rdx\n        lea     rdx, [r12+10H]\n        setae   sil\n        cmp     rax, rdx\n        setae   dl\n        or      sil, dl\n        je      L_007\n        cmp     rbx, 15\n        jbe     L_007\n        mov     rdi, rbx\n        xor     esi, esi\n        xor     r8d, r8d\n        shr     rdi, 4\n        mov     rdx, rdi\n        shl     rdx, 4\nL_001:  movdqu  xmm0, oword [r12+rsi]\n        add     r8, 1\n        movdqu  oword [rcx+rsi+8H], xmm0\n        add     rsi, 16\n        cmp     rdi, r8\n        ja      L_001\n        cmp     rbx, rdx\n        je      L_002\n        movzx   esi, byte [r12+rdx]\n        mov     byte [rcx+rdx+8H], sil\n        lea     rsi, [rdx+1H]\n        cmp     rbx, rsi\n        jle     L_002\n        movzx   edi, byte [r12+rdx+1H]\n        lea     rsi, [rdx+2H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+9H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+2H]\n        lea     rsi, [rdx+3H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0AH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+3H]\n        lea     rsi, [rdx+4H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0BH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+4H]\n        lea     rsi, [rdx+5H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0CH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+5H]\n        lea     rsi, [rdx+6H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0DH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+6H]\n        lea     rsi, [rdx+7H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0EH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+7H]\n        lea     rsi, [rdx+8H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+0FH], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+8H]\n        lea     rsi, [rdx+9H]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+10H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+9H]\n        lea     rsi, [rdx+0AH]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+11H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+0AH]\n        lea     rsi, [rdx+0BH]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+12H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+0BH]\n        lea     rsi, [rdx+0CH]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+13H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+0CH]\n        lea     rsi, [rdx+0DH]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+14H], dil\n        jle     L_002\n        movzx   edi, byte [r12+rdx+0DH]\n        lea     rsi, [rdx+0EH]\n        cmp     rbx, rsi\n        mov     byte [rcx+rdx+15H], dil\n        jle     L_002\n        movzx   edx, byte [r12+rdx+0EH]\n        mov     byte [rcx+rsi+8H], dl\nL_002:  test    rbp, rbp\n        jle     L_004\n        lea     rsi, [rcx+rbx]\n        lea     r8, [r13+10H]\n        lea     rdx, [rsi+18H]\n        cmp     r13, rdx\n        lea     rdx, [rsi+8H]\n        setae   dil\n        cmp     r8, rdx\n        setbe   dl\n        or      dil, dl\n        je      L_005\n        cmp     rbp, 15\n        jbe     L_005\n        mov     r8, rbp\n        xor     edi, edi\n        xor     r9d, r9d\n        shr     r8, 4\n        mov     rdx, r8\n        shl     rdx, 4\nL_003:  movdqu  xmm0, oword [r13+rdi]\n        add     r9, 1\n        movdqu  oword [rsi+rdi+8H], xmm0\n        add     rdi, 16\n        cmp     r8, r9\n        ja      L_003\n        cmp     rbp, rdx\n        je      L_004\n        movzx   esi, byte [r13+rdx]\n        add     rbx, rax\n        mov     byte [rbx+rdx], sil\n        lea     rsi, [rdx+1H]\n        cmp     rbp, rsi\n        jle     L_004\n        movzx   edi, byte [r13+rdx+1H]\n        lea     rsi, [rdx+2H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+1H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+2H]\n        lea     rsi, [rdx+3H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+2H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+3H]\n        lea     rsi, [rdx+4H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+3H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+4H]\n        lea     rsi, [rdx+5H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+4H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+5H]\n        lea     rsi, [rdx+6H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+5H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+6H]\n        lea     rsi, [rdx+7H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+6H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+7H]\n        lea     rsi, [rdx+8H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+7H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+8H]\n        lea     rsi, [rdx+9H]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+8H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+9H]\n        lea     rsi, [rdx+0AH]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+9H], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+0AH]\n        lea     rsi, [rdx+0BH]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+0AH], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+0BH]\n        lea     rsi, [rdx+0CH]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+0BH], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+0CH]\n        lea     rsi, [rdx+0DH]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+0CH], dil\n        jle     L_004\n        movzx   edi, byte [r13+rdx+0DH]\n        lea     rsi, [rdx+0EH]\n        cmp     rbp, rsi\n        mov     byte [rdx+rbx+0DH], dil\n        jle     L_004\n        movzx   edx, byte [r13+rdx+0EH]\n        mov     byte [rbx+rsi], dl\nL_004:  mov     byte [rcx+r14+8H], 0\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        pop     r14\n        ret\nALIGN   16\nL_005:  xor     edx, edx\nALIGN   8\nL_006:  movzx   edi, byte [r13+rdx]\n        mov     byte [rsi+rdx+8H], dil\n        add     rdx, 1\n        cmp     rdx, rbp\n        jnz     L_006\n        mov     byte [rcx+r14+8H], 0\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        pop     r14\n        ret\nALIGN   8\nL_007:  xor     edx, edx\nALIGN   8\nL_008:  movzx   esi, byte [r12+rdx]\n        mov     byte [rcx+rdx+8H], sil\n        add     rdx, 1\n        cmp     rdx, rbx\n        jnz     L_008\n        jmp     L_002\n__string__length__:\n        mov     rax, qword [rdi-8H]\n        ret\nALIGN   16\n__string__ord__:\n        movsx   rax, byte [rdi+rsi]\n        ret\nALIGN   16\n__string__parseInt__:\n        movsx   ecx, byte [rdi]\n        mov     rax, qword [rdi-8H]\n        cmp     cl, 45\n        jz      L_013\n        test    rax, rax\n        jle     L_011\n        lea     edx, [rcx-30H]\n        cmp     dl, 9\n        ja      L_011\n        lea     rdx, [rdi+1H]\n        add     rdi, rax\n        xor     eax, eax\n        jmp     L_010\nALIGN   8\nL_009:  movsx   ecx, byte [rdx]\n        add     rdx, 1\n        lea     esi, [rcx-30H]\n        cmp     sil, 9\n        ja      L_012\nL_010:  sub     ecx, 48\n        lea     rax, [rax+rax*4]\n        cmp     rdx, rdi\n        movsxd  rcx, ecx\n        lea     rax, [rcx+rax*2]\n        jnz     L_009\n        ret\nL_011:  xor     eax, eax\n        nop\nL_012:\n        ret\nALIGN   8\nL_013:  cmp     rax, 1\n        jle     L_011\n        movsx   ecx, byte [rdi+1H]\n        lea     edx, [rcx-30H]\n        cmp     dl, 9\n        ja      L_011\n        lea     rdx, [rdi+2H]\n        add     rdi, rax\n        xor     eax, eax\n        jmp     L_015\nALIGN   8\nL_014:  movsx   ecx, byte [rdx]\n        add     rdx, 1\n        lea     esi, [rcx-30H]\n        cmp     sil, 9\n        ja      L_012\nL_015:  lea     rax, [rax+rax*4]\n        sub     ecx, 48\n        movsxd  rcx, ecx\n        add     rax, rax\n        sub     rax, rcx\n        cmp     rdx, rdi\n        jnz     L_014\n        ret\n        nop\nALIGN   16\n__array__size__:\n        mov     rax, qword [rdi-8H]\n        ret\nALIGN   16\ngetString:\n        sub     rsp, 8\n        mov     edi, 508\n        call    malloc\n        lea     rdi, [rax+8H]\n        call    gets\n        xor     edx, edx\n        cmp     byte [rax], 0\n        mov     qword [rax-8H], 0\n        jz      L_017\nALIGN   16\nL_016:  add     rdx, 1\n        mov     qword [rax-8H], rdx\n        cmp     byte [rax+rdx], 0\n        jnz     L_016\nL_017:  add     rsp, 8\n        ret\nALIGN   16\ngetInt:\n        mov     rdi, qword [rel stdin]\n        push    rbx\n        call    _IO_getc\n        cmp     al, 45\n        movsx   edx, al\n        jz      L_020\n        sub     eax, 48\n        xor     ebx, ebx\n        cmp     al, 9\n        ja      L_019\nALIGN   8\nL_018:  mov     rdi, qword [rel stdin]\n        lea     rax, [rbx+rbx*4]\n        sub     edx, 48\n        movsxd  rdx, edx\n        lea     rbx, [rdx+rax*2]\n        call    _IO_getc\n        movsx   edx, al\n        sub     eax, 48\n        cmp     al, 9\n        jbe     L_018\nL_019:  mov     rax, rbx\n        pop     rbx\n        ret\nALIGN   8\nL_020:  xor     ebx, ebx\n        jmp     L_022\nALIGN   8\nL_021:  lea     rbx, [rbx+rbx*4]\n        movsx   eax, al\n        sub     eax, 48\n        add     rbx, rbx\n        cdqe\n        sub     rbx, rax\nL_022:  mov     rdi, qword [rel stdin]\n        call    _IO_getc\n        lea     edx, [rax-30H]\n        cmp     dl, 9\n        jbe     L_021\n        mov     rax, rbx\n        pop     rbx\n        ret\nALIGN   16\nprint:\n        push    rbx\n        mov     rbx, rdi\n        movsx   edi, byte [rdi]\n        test    dil, dil\n        jz      L_024\nALIGN   8\nL_023:  mov     rsi, qword [rel stdout]\n        add     rbx, 1\n        call    _IO_putc\n        movsx   edi, byte [rbx]\n        test    dil, dil\n        jnz     L_023\nL_024:  pop     rbx\n        ret\nALIGN   8\nprintln:\n        jmp     puts\nALIGN   16\ntoString:\n        push    r13\n        push    r12\n        push    rbp\n        push    rbx\n        mov     rbx, rdi\n        sub     rsp, 8\n        test    rdi, rdi\n        je      L_031\n        js      L_030\n        mov     edi, 10\n        xor     ebp, ebp\n        mov     r12d, 1\nL_025:  cmp     rbx, 9\n        jle     L_032\n        mov     rcx, rbx\n        mov     rsi, qword 6666666666666667H\nL_026:  mov     rax, rcx\n        sar     rcx, 63\n        add     r12, 1\n        imul    rsi\n        sar     rdx, 2\n        sub     rdx, rcx\n        cmp     rdx, 9\n        mov     rcx, rdx\n        jg      L_026\n        lea     rdi, [r12+9H]\n        lea     r13, [r12-1H]\nL_027:  call    malloc\n        lea     r9, [rax+8H]\n        lea     rcx, [rax+r13]\n        mov     r8, rax\n        mov     qword [rax], r12\n        mov     rdi, qword 6666666666666667H\nALIGN   8\nL_028:  mov     rax, rbx\n        mov     rsi, rbx\n        sub     rcx, 1\n        imul    rdi\n        sar     rsi, 63\n        sar     rdx, 2\n        sub     rdx, rsi\n        lea     rsi, [rdx+rdx*4]\n        add     rsi, rsi\n        sub     rbx, rsi\n        add     ebx, 48\n        mov     byte [rcx+9H], bl\n        test    rdx, rdx\n        mov     rbx, rdx\n        jnz     L_028\n        cmp     rbp, 1\n        mov     rax, r9\n        jz      L_029\n        add     rsp, 8\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        ret\nALIGN   8\nL_029:  mov     byte [r8+8H], 45\n        add     rsp, 8\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        ret\nL_030:  neg     rbx\n        mov     edi, 11\n        mov     ebp, 1\n        mov     r12d, 2\n        jmp     L_025\nALIGN   16\nL_031:  mov     edi, 10\n        call    malloc\n        mov     qword [rax], 1\n        mov     byte [rax+8H], 48\n        add     rax, 8\n        mov     byte [rax+1H], 0\n        add     rsp, 8\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        ret\nL_032:  mov     r13, rbp\n        jmp     L_027\n__string__substring__:\n        push    r13\n        mov     r13, rdx\n        sub     r13, rsi\n        push    r12\n        mov     r12, rdx\n        push    rbp\n        mov     rbp, rdi\n        lea     rdi, [r13+0AH]\n        push    rbx\n        mov     rbx, rsi\n        sub     rsp, 8\n        call    malloc\n        lea     rdx, [r13+1H]\n        cmp     r12, rbx\n        mov     rsi, rax\n        lea     rax, [rax+8H]\n        mov     qword [rax-8H], rdx\n        jl      L_034\n        lea     rcx, [rbp+rbx]\n        lea     rdi, [rsi+18H]\n        cmp     rcx, rdi\n        lea     rdi, [rcx+10H]\n        setae   r8b\n        cmp     rax, rdi\n        setae   dil\n        or      r8b, dil\n        je      L_035\n        cmp     rdx, 15\n        jbe     L_035\n        mov     r9, rdx\n        xor     r8d, r8d\n        xor     r10d, r10d\n        shr     r9, 4\n        mov     rdi, r9\n        shl     rdi, 4\nL_033:  movdqu  xmm0, oword [rcx+r8]\n        add     r10, 1\n        movdqu  oword [rsi+r8+8H], xmm0\n        add     r8, 16\n        cmp     r9, r10\n        ja      L_033\n        cmp     rdx, rdi\n        lea     rcx, [rbx+rdi]\n        je      L_034\n        movzx   edx, byte [rbp+rcx]\n        mov     byte [rsi+rdi+8H], dl\n        lea     rdx, [rcx+1H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+1H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+2H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+2H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+3H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+3H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+4H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+4H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+5H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+5H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+6H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+6H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+7H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+7H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+8H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+8H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+9H]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+9H]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+0AH]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+0AH]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+0BH]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+0BH]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+0CH]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+0CH]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+0DH]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   edi, byte [rbp+rcx+0DH]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], dil\n        lea     rdx, [rcx+0EH]\n        cmp     r12, rdx\n        jl      L_034\n        movzx   ecx, byte [rbp+rcx+0EH]\n        sub     rdx, rbx\n        mov     byte [rsi+rdx+8H], cl\nL_034:  mov     byte [rax+r13+1H], 0\n        add     rsp, 8\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        ret\nL_035:  add     r12, 1\n        sub     rsi, rbx\nALIGN   16\nL_036:  movzx   edx, byte [rbp+rbx]\n        mov     byte [rsi+rbx+8H], dl\n        add     rbx, 1\n        cmp     rbx, r12\n        jnz     L_036\n        jmp     L_034\nALIGN   16\n__string__less__:\n        cmp     byte [rdi], 0\n        jnz     L_038\n        test    rsi, rsi\n        jnz     L_038\n        movzx   eax, byte [abs 0H]\n        test    al, al\n        jg      L_039\nALIGN   16\nL_037:  test    al, al\n        jz      L_037\nL_038:  xor     eax, eax\n        ret\nL_039:  mov     eax, 1\n        ret\nALIGN   8\n__string__greater__:\n        cmp     byte [rdi], 0\n        jnz     L_041\n        test    rsi, rsi\n        jnz     L_041\n        movzx   eax, byte [abs 0H]\n        test    al, al\n        jg      L_041\nALIGN   16\nL_040:  test    al, al\n        jz      L_040\n        mov     eax, 1\n        ret\nL_041:  xor     eax, eax\n        ret\nALIGN   8\n__string__equals__:\n        cmp     byte [rdi], 0\n        jnz     L_044\n        test    rsi, rsi\n        jnz     L_044\n        cmp     byte [abs 0H], 0\n        jnz     L_043\nL_042:  jmp     L_042\nALIGN   16\nL_043:  xor     eax, eax\n        ret\nALIGN   8\nL_044:  mov     eax, 1\nALIGN   8\n        ret\nALIGN   16\n__string__neq__:\n        cmp     byte [rdi], 0\n        jnz     L_047\n        test    rsi, rsi\n        jnz     L_047\n        cmp     byte [abs 0H], 0\n        jnz     L_046\nL_045:  jmp     L_045\nALIGN   16\nL_046:  mov     eax, 1\n        ret\nALIGN   16\nL_047:  xor     eax, eax\n        ret\n";
    string _Translator::__sys_lib = "default rel\nglobal __msharp__malloc__\nglobal __string__connect__\nglobal __string__length__\nglobal __string__ord__\nglobal __string__parseInt__\nglobal __array__size__\nglobal getString\nglobal getInt\nglobal print\nglobal println\nglobal toString\nglobal __string__substring__\nglobal __string__less__\nglobal __string__greater__\nglobal __string__equals__\nglobal __string__neq__\nglobal main\nextern puts\nextern _IO_putc\nextern stdout\nextern _IO_getc\nextern stdin\nextern gets\nextern malloc\nSECTION .text \n__msharp__malloc__:\n        push    rbx\n        mov     rbx, rdi\n        lea     rdi, [rdi*8+8H]\n        call    malloc\n        mov     qword [rax], rbx\n        add     rax, 8\n        pop     rbx\n        ret\nALIGN   8\n__string__connect__:\n        push    r14\n        push    r13\n        mov     r13, rsi\n        push    r12\n        mov     r12, rdi\n        push    rbp\n        push    rbx\n        mov     rbx, qword [rdi-8H]\n        mov     rbp, qword [rsi-8H]\n        lea     r14, [rbx+rbp]\n        lea     rdi, [r14+9H]\n        call    malloc\n        xor     edx, edx\n        test    rbx, rbx\n        mov     r8, rax\n        mov     qword [rax], r14\n        lea     rax, [rax+8H]\n        jle     L_002\nALIGN   8\nL_001:  movzx   ecx, byte [r12+rdx]\n        mov     byte [r8+rdx+8H], cl\n        add     rdx, 1\n        cmp     rdx, rbx\n        jnz     L_001\nL_002:  xor     edx, edx\n        test    rbp, rbp\n        lea     rsi, [r8+rbx]\n        jle     L_004\nALIGN   16\nL_003:  movzx   ecx, byte [r13+rdx]\n        mov     byte [rsi+rdx+8H], cl\n        add     rdx, 1\n        cmp     rdx, rbp\n        jnz     L_003\nL_004:  mov     byte [r8+r14+8H], 0\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        pop     r14\n        ret\nALIGN   16\n__string__length__:\n        mov     rax, qword [rdi-8H]\n        ret\nALIGN   16\n__string__ord__:\n        movsx   rax, byte [rdi+rsi]\n        ret\nALIGN   16\n__string__parseInt__:\n        movsx   ecx, byte [rdi]\n        mov     rax, qword [rdi-8H]\n        cmp     cl, 45\n        jz      L_009\n        test    rax, rax\n        jle     L_007\n        lea     edx, [rcx-30H]\n        cmp     dl, 9\n        ja      L_007\n        lea     rdx, [rdi+1H]\n        add     rdi, rax\n        xor     eax, eax\n        jmp     L_006\nALIGN   8\nL_005:  movsx   ecx, byte [rdx]\n        add     rdx, 1\n        lea     esi, [rcx-30H]\n        cmp     sil, 9\n        ja      L_008\nL_006:  sub     ecx, 48\n        lea     rax, [rax+rax*4]\n        cmp     rdx, rdi\n        movsxd  rcx, ecx\n        lea     rax, [rcx+rax*2]\n        jnz     L_005\n        ret\nL_007:  xor     eax, eax\n        nop\nL_008:\n        ret\nALIGN   8\nL_009:  cmp     rax, 1\n        jle     L_007\n        movsx   ecx, byte [rdi+1H]\n        lea     edx, [rcx-30H]\n        cmp     dl, 9\n        ja      L_007\n        lea     rdx, [rdi+2H]\n        add     rdi, rax\n        xor     eax, eax\n        jmp     L_011\nALIGN   8\nL_010:  movsx   ecx, byte [rdx]\n        add     rdx, 1\n        lea     esi, [rcx-30H]\n        cmp     sil, 9\n        ja      L_008\nL_011:  lea     rax, [rax+rax*4]\n        sub     ecx, 48\n        movsxd  rcx, ecx\n        add     rax, rax\n        sub     rax, rcx\n        cmp     rdx, rdi\n        jnz     L_010\n        ret\n        nop\nALIGN   16\n__array__size__:\n        mov     rax, qword [rdi-8H]\n        ret\nALIGN   16\ngetString:\n        sub     rsp, 8\n        mov     edi, 508\n        call    malloc\n        lea     rdi, [rax+8H]\n        call    gets\n        xor     edx, edx\n        cmp     byte [rax], 0\n        mov     qword [rax-8H], 0\n        jz      L_013\nALIGN   16\nL_012:  add     rdx, 1\n        mov     qword [rax-8H], rdx\n        cmp     byte [rax+rdx], 0\n        jnz     L_012\nL_013:  add     rsp, 8\n        ret\nALIGN   16\ngetInt:\n        mov     rdi, qword [rel stdin]\n        push    rbx\n        call    _IO_getc\n        cmp     al, 45\n        movsx   edx, al\n        jz      L_016\n        sub     eax, 48\n        xor     ebx, ebx\n        cmp     al, 9\n        ja      L_015\nALIGN   8\nL_014:  mov     rdi, qword [rel stdin]\n        lea     rax, [rbx+rbx*4]\n        sub     edx, 48\n        movsxd  rdx, edx\n        lea     rbx, [rdx+rax*2]\n        call    _IO_getc\n        movsx   edx, al\n        sub     eax, 48\n        cmp     al, 9\n        jbe     L_014\nL_015:  mov     rax, rbx\n        pop     rbx\n        ret\nALIGN   8\nL_016:  xor     ebx, ebx\n        jmp     L_018\nALIGN   8\nL_017:  lea     rbx, [rbx+rbx*4]\n        movsx   eax, al\n        sub     eax, 48\n        add     rbx, rbx\n        cdqe\n        sub     rbx, rax\nL_018:  mov     rdi, qword [rel stdin]\n        call    _IO_getc\n        lea     edx, [rax-30H]\n        cmp     dl, 9\n        jbe     L_017\n        mov     rax, rbx\n        pop     rbx\n        ret\nALIGN   16\nprint:\n        push    rbx\n        mov     rbx, rdi\n        movsx   edi, byte [rdi]\n        test    dil, dil\n        jz      L_020\nALIGN   8\nL_019:  mov     rsi, qword [rel stdout]\n        add     rbx, 1\n        call    _IO_putc\n        movsx   edi, byte [rbx]\n        test    dil, dil\n        jnz     L_019\nL_020:  pop     rbx\n        ret\nALIGN   8\nprintln:\n        jmp     puts\nALIGN   16\ntoString:\n        push    r12\n        test    rdi, rdi\n        push    rbp\n        push    rbx\n        mov     rbx, rdi\n        je      L_027\n        mov     r12d, 0\n        mov     ebp, 1\n        js      L_026\nL_021:  cmp     rbx, 9\n        jle     L_023\n        mov     rcx, rbx\n        mov     rsi, qword 6666666666666667H\nALIGN   8\nL_022:  mov     rax, rcx\n        sar     rcx, 63\n        add     rbp, 1\n        imul    rsi\n        sar     rdx, 2\n        sub     rdx, rcx\n        cmp     rdx, 9\n        mov     rcx, rdx\n        jg      L_022\nL_023:  lea     rdi, [rbp+9H]\n        call    malloc\n        lea     r9, [rax+8H]\n        lea     rcx, [rax+rbp]\n        mov     r8, rax\n        mov     qword [rax], rbp\n        mov     rdi, qword 6666666666666667H\nALIGN   16\nL_024:  mov     rax, rbx\n        mov     rsi, rbx\n        sub     rcx, 1\n        imul    rdi\n        sar     rsi, 63\n        sar     rdx, 2\n        sub     rdx, rsi\n        lea     rsi, [rdx+rdx*4]\n        add     rsi, rsi\n        sub     rbx, rsi\n        add     ebx, 48\n        mov     byte [rcx+8H], bl\n        test    rdx, rdx\n        mov     rbx, rdx\n        jnz     L_024\n        cmp     r12, 1\n        mov     rax, r9\n        jz      L_025\n        pop     rbx\n        pop     rbp\n        pop     r12\n        ret\nALIGN   8\nL_025:  mov     byte [r8+8H], 45\n        pop     rbx\n        pop     rbp\n        pop     r12\n        ret\nALIGN   8\nL_026:  neg     rbx\n        mov     r12b, 1\n        mov     bpl, 2\n        jmp     L_021\nALIGN   8\nL_027:  mov     edi, 10\n        call    malloc\n        mov     qword [rax], 1\n        mov     byte [rax+8H], 48\n        add     rax, 8\n        mov     byte [rax+1H], 0\n        pop     rbx\n        pop     rbp\n        pop     r12\n        ret\nALIGN   16\n__string__substring__:\n        push    r13\n        mov     r13, rsi\n        push    r12\n        mov     r12, rdx\n        push    rbp\n        mov     rbp, rdx\n        sub     rbp, rsi\n        push    rbx\n        mov     rbx, rdi\n        lea     rdi, [rbp+0AH]\n        sub     rsp, 8\n        call    malloc\n        mov     rcx, rax\n        lea     rax, [rbp+1H]\n        lea     rdi, [r12+1H]\n        mov     rsi, r13\n        mov     qword [rcx], rax\n        lea     rax, [rcx+8H]\n        sub     rcx, r13\n        cmp     r12, r13\n        jl      L_029\nL_028:  movzx   edx, byte [rbx+rsi]\n        mov     byte [rcx+rsi+8H], dl\n        add     rsi, 1\n        cmp     rsi, rdi\n        jnz     L_028\nL_029:  mov     byte [rax+rbp+1H], 0\n        add     rsp, 8\n        pop     rbx\n        pop     rbp\n        pop     r12\n        pop     r13\n        ret\nALIGN   16\n__string__less__:\n        cmp     byte [rdi], 0\n        jnz     L_032\n        test    rsi, rsi\n        jz      L_031\n        jmp     L_032\nALIGN   8\nL_030:  jnz     L_032\nL_031:\n        cmp     byte [abs 0H], 0\n        jle     L_030\n        mov     eax, 1\n        ret\nALIGN   8\nL_032:  xor     eax, eax\n        ret\nALIGN   8\n__string__greater__:\n        cmp     byte [rdi], 0\n        jnz     L_035\n        test    rsi, rsi\n        jz      L_034\n        jmp     L_035\nALIGN   8\nL_033:  jnz     L_036\nL_034:\n        cmp     byte [abs 0H], 0\n        jle     L_033\nL_035:  xor     eax, eax\n        ret\nALIGN   8\nL_036:  mov     eax, 1\n        ret\nALIGN   16\n__string__equals__:\n        cmp     byte [rdi], 0\n        jnz     L_038\n        test    rsi, rsi\n        jnz     L_038\nL_037:\n        cmp     byte [abs 0H], 0\n        jz      L_037\n        xor     eax, eax\n        ret\nALIGN   16\nL_038:  mov     eax, 1\n        ret\nALIGN   16\n__string__neq__:\n        cmp     byte [rdi], 0\n        jnz     L_040\n        test    rsi, rsi\n        jnz     L_040\nL_039:\n        cmp     byte [abs 0H], 0\n        jz      L_039\n        mov     eax, 1\n        ret\nALIGN   8\nL_040:  xor     eax, eax\n        ret\n";
    void _Translator::to_amd64_linux_asm(ostream & fout)
    {
#ifndef __infinity__reg__assumption
        stringstream text_section;
#else
        Refiner text_section;
#endif
        stringstream data_section;
        //set<string> virtual_registers;
        //first pass : register analyze & bss section

        fout << get_library() << endl;
        string function_title;
        stack<string> current_loop_title;
        unordered_map<string, int> stack_size;
        set<string> calling_others;
        set<string> local_temp_var;
        unordered_map<string, VirtualRegisterRankMap> rank_map;
        //calculation_cached_local_var_number = 20000;
        int call_extra = 0;
        int weight_coeff = 1;
        for (auto & s : intermediate_repr)
        {
            if (s.oprand == IR__func)
            {
                global_stack_offset = 0;
                local_temp_var.clear();
                call_extra = 0;
                function_title = s.args.front();
            }
            if (s.oprand == IR__loop_start)
            {
                weight_coeff *= 5;
                continue;
            }
            if (s.oprand == IR__loop_end)
            {
                weight_coeff /= 5;
                continue;
            }
            for (auto & sf : s.args)
            {
                if (sf[0] == '@' || sf[0] == '#' || sf[0] == '%')
                {
                    VirtualRegisterRankMap & rank_register_rank_map = rank_map[function_title];
                    rank_register_rank_map.promote(sf, weight_coeff);
                }
            }
        }
        for (int i = 0; i < int(intermediate_repr.size()); ++i)
        {
            if (intermediate_repr[i].oprand == IR__func)
            {
                global_stack_offset = 0;
                local_temp_var.clear();
                call_extra = 0;
                function_title = intermediate_repr[i].args.front();
                deque<string> left_over_reg = VirtualRegisterRankMap::avaliable_reg();
                for (auto & s : rank_map[function_title].topK())
                {
                    storage_map[function_title][s] = shared_ptr<StorageBase>(new RegisterModel(left_over_reg.front()));
                    left_over_reg.pop_front();
                }
            }
            while (intermediate_repr[i].oprand == IR__ret)
            {
                if (i + 1 < int(intermediate_repr.size()))
                {
                    if (intermediate_repr[i + 1].oprand == IR__ret)
                        intermediate_repr.erase(intermediate_repr.begin() + (i + 1));
                    else
                        break;
                } else {
                    break;
                }
            }
            /*if (intermediate_repr[i].oprand == IR__quad)
            {
                data_section << intermediate_repr[i].args.back() << ":\n\t\tresq\t1\n";
            }*/
            if (intermediate_repr[i].oprand == IR__call)
            {
                calling_others.insert(function_title);
                call_extra = max(call_extra, int(intermediate_repr[i].args.size() - 8));
            }

            for (auto & s : intermediate_repr[i].args)
            {
                if (s[0] == '@' || s[0] == '#' || s[0] == '%')
                {
                    if (!is_in(s, storage_map[function_title]))
                    {
                        local_temp_var.insert(s);
                        global_stack_offset += 8;
                        storage_map[function_title][s] = shared_ptr<StorageBase>(new MemoryModel("rbp", "- " + Tool.to_string(global_stack_offset)));
                    }
                }
            }
            stack_size[function_title] = (((storage_map[function_title].size()/* - rank_map[function_title].topK(5).size()*/ + call_extra) * 8 + 8) / 16) * 16;
        }

        print(cerr);
        cerr << endl;
        //second pass : really generate the code
        StorageBase::set_sout(text_section);
        deque<string> arg_tgt = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        int line_no = 0;
        for (IR & this_line : intermediate_repr)
        {
            //std::cerr << "processing IR record no." << line_no << endl;
            ++line_no;
            text_section.flush();
            switch (this_line.oprand)
            {
            case IR__error: case IR__get_size: case IR__word: case IR__byte:
                Error.compile_error("Fatal error : compiler failed at codegen");
            break;
            case IR__func:
            {
                function_title = this_line.args.front();
                arg_tgt = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
                text_section << function_title << ":\n";
                //if (is_in(function_title, calling_others))
                deque<string> left_over_reg = VirtualRegisterRankMap::avaliable_reg();
                text_section << "\tpush\trbp\n";
#ifndef __infinity__reg__assumption
                for (auto & s : rank_map[function_title].topK())
                {
                    text_section << "\tpush\t" << left_over_reg.front() << "\n";
                    left_over_reg.pop_front();
                }
#endif
                text_section << "\tmov\trbp, rsp\n" << "\tsub\trsp, " << stack_size[function_title] << endl;
            }
            break;
            case IR__assign:
            {
#ifndef __infinity__reg__assumption
                auto cons_var = parse_symbol(function_title, this_line.args.front(), "rdx");
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args[1], "rax");
                auto cons1 = cons_var -> name_as_src("rax");
                text_section << "\tmov\t" << cons0 << ", " << cons1 << "\n";
#else
                auto cons_var = parse_symbol(function_title, this_line.args.front(), "v" + Tool.to_string(calculation_cached_local_var_number++));
                auto cons0 = cons_var -> name_as_tg();
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                cons_var = parse_symbol(function_title, this_line.args[1], rax);
                auto cons1 = cons_var -> name_as_src(rax);
                text_section << "\tmov\t" << cons0 << ", " << cons1 << "\n";
#endif
            }
            break;
            case IR__break:
            {
                text_section << "\tjmp\t__loop__end__" << current_loop_title.top() << "\n";
            }
            break;
            case IR__continue:
            {
                text_section << "\tjmp\t__loop__start__" << current_loop_title.top() << "\n";
            }
            break;
            case IR__ret:
            {
                deque<string> left_over_reg = VirtualRegisterRankMap::avaliable_reg();
                if (this_line.args.back() == "null")
                {
                    //if (is_in(function_title, calling_others))
                        text_section << "\tmov\trsp, rbp\n";
#ifndef __infinity__reg__assumption
                        for (int i = rank_map[function_title].topK().size() - 1; i >= 0; --i)
                        {
                            text_section << "\tpop\t" << left_over_reg[i] << "\n";
                        }
#endif
                        text_section << "\tpop\trbp\n";
                    text_section << "\tret\n";
                    break;
                }
                auto cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons = cons_var -> name_as_tg();
                text_section << "\tmov\trax, " << cons << "\n";
                //if (is_in(function_title, calling_others))
                    text_section << "\tmov\trsp, rbp\n";
#ifndef __infinity__reg__assumption
                    for (int i = rank_map[function_title].topK().size() - 1; i >= 0; --i)
                    {
                        text_section << "\tpop\t" << left_over_reg[i] << "\n";
                    }
#endif
                    text_section << "\tpop\trbp\n";
                text_section << "\tret\n";
            }
            break;
            case IR__quad:
                //data_section << this_line.args.back() << ":\n\tresq\t1\n";
            break;
            case IR__arg:
            {
                if (this_line.args.back()[0] != '@') break;
                auto cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons = cons_var -> name_as_tg();
                int idx = 0;
                {
                    stringstream sin;
                    sin << this_line.args.back().substr(1);
                    sin >> idx;
                }
                if (idx < 6) text_section << "\tmov\t" << cons << ", " << arg_tgt[idx] << "\n";
                else
                {
                    //if (is_in(function_title, calling_others))
                    {
                        text_section << "\tmov\trax, qword [rbp+"
                                     << (16
                 #ifndef __infinity__reg__assumption
                                         + rank_map[function_title].topK().size() * 8
                 #else
                                         + /*should add some base stack size here*/
                 #endif
                                         + (idx - 6) * 8) << "]\n"
                                     << "\tmov\t" << cons << ", rax\n";
                    } /*else {
                        text_section << "\tmov\trax, qword [rsp+" << (8 + (idx - 6) * 8) << "]\n"
                                     << "\tmov\t" << cons << ", rax\n";
                    }*/
                }
            }
            break;
            case IR__var:
            break;
            case IR__shl:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args[1]);
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                auto cons1 = cons_var -> name_as_src(rax);
                if (cons1 != rax)
                {
                    text_section << "\tmov\t"+ rax + ", " << cons1 << "\n";
                    cons1 = rax;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src("rcx");
                if (cons2 == "rcx") cons2 = "cl";
                else if (cons2[0] > '9' || cons2[0] < '0')
                {
                    text_section << "\tmov\trcx, " << cons2 << "\n";
                    cons2 = "cl";
                }
                text_section << "\tshl\t" << cons1 << ", " << cons2 << "\n\tmov\t" << cons0 << ", " << cons1 << "\n";
            }
            break;
            case IR__shr:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args[1]);
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                auto cons1 = cons_var -> name_as_src(rax);
                if (cons1 != rax)
                {
                    text_section << "\tmov\t" + rax + ", " << cons1 << "\n";
                    cons1 = rax;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src("rcx");
                if (cons2 == "rcx") cons2 = "cl";
                else if (cons2[0] > '9' || cons2[0] < '0')
                {
                    text_section << "\tmov\trcx, " << cons2 << "\n";
                    cons2 = "cl";
                }
                text_section << "\tsar\t" << cons1 << ", " << cons2 << "\n\tmov\t" << cons0 << ", " << cons1 << "\n";
            }
            break;
            case IR__less:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsetl\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__geq:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsetge\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__greater:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsetg\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__eq:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsete\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__neq:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsetne\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__leq:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                if (cons1 != rcx)
                {
                    text_section << "\tmov\t" + rcx + ", " << cons1 << "\n";
                    cons1 = rcx;
                }
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tcmp\t" << cons1 << ", " << cons2 << "\n"
                             << "\tsetle\tal" << "\n"
                             << "\tmovzx\teax, al\n"
                             << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__add:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = string("0");
                if (cons_var != nullptr)
                    cons1 = cons_var -> name_as_src(rcx);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tmov\trax, " << cons1 << "\n";
                text_section << "\tadd\trax, " << cons2 << "\n";
                if (cons0 != "rax") text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__sub:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = string("0");
                if (cons_var != nullptr)
                    cons1 = cons_var -> name_as_src(rcx);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                text_section << "\tmov\trax, " << cons1 << "\n"
                             << "\tsub\trax, " << cons2 << "\n";
                if (cons0 != "rax") text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__mul:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rcx);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                //if (cons1 != "rcx")
                {
                    text_section << "\tmov\trax, " << cons1 << "\n";
                }
                text_section << "\timul\trax, " << cons2 << "\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__not:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons1 = cons_var -> name_as_tg();
                if (cons1 != "rax")
                    text_section << "\tmov\trax, " << cons1 << "\n";
                text_section << "\tnot\trax" << "\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__lnot:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons1 = cons_var -> name_as_tg();
                if (cons1 != "rax")
                    text_section << "\tmov\trax, " << cons1 << "\n";
                text_section << "\tcmp\trax, 0" << "\n";
                text_section << "\tsetz\tal" << "\n";
                text_section << "\tmovzx\teax, al\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__xor:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rax);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                if (cons1 != rax)
                {
                    text_section << "\tmov\t" + rax + ", " << cons1 << "\n";
                }
                text_section << "\txor\t" + rax + ", " << cons2 << "\n";
                if (cons0 != rax)
                    text_section << "\tmov\t" << cons0 << ", " + rax + "\n";
            }
            break;
            case IR__land:
            case IR__band:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rax);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                if (cons1 != rax)
                {
                    text_section << "\tmov\t" + rax + ", " << cons1 << "\n";
                }
                text_section << "\tand\t" + rax + ", " << cons2 << "\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", " + rax + "\n";
            }
            break;
            case IR__bor: case IR__lor:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
#ifndef __infinity__reg__assumption
                string rax = "rax";
                string rcx = "rcx";
                string rdx = "rdx";
#else
                string rax = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rcx = "v" + Tool.to_string(calculation_cached_local_var_number++);
                string rdx = "v" + Tool.to_string(calculation_cached_local_var_number++);
#endif
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src(rax);
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src(rdx);
                if (cons1 != rax)
                {
                    text_section << "\tmov\t" + rax + ", " << cons1 << "\n";
                }
                text_section << "\tor\t" + rax + ", " << cons2 << "\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", " + rax + "\n";
            }
            break;
            case IR__div:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src("rdx");
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src("rcx");
                //if (cons1 != "rcx")
                {
                    text_section << "\tmov\trax, " << cons1 << "\n";
                }

                if (cons2 != "rcx")
                {
                    text_section << "\tmov\trcx, " << cons2 << "\n";
                }
                text_section << "\tcdq\n\tidiv\trcx\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", rax\n";
            }
            break;
            case IR__mod:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.front());
                auto cons0 = cons_var -> name_as_tg();
                cons_var = parse_symbol(function_title, this_line.args[1]);
                auto cons1 = cons_var -> name_as_src("rdx");
                cons_var = parse_symbol(function_title, this_line.args.back());
                auto cons2 = cons_var -> name_as_src("rcx");
                //if (cons1 != "rcx")
                {
                    text_section << "\tmov\trax, " << cons1 << "\n";
                }
                if (cons2 != "rcx")
                {
                    text_section << "\tmov\trcx, " << cons2 << "\n";
                }
                text_section << "\tcdq\n\tidiv\trcx\n";
                if (cons0 != "rax")
                    text_section << "\tmov\t" << cons0 << ", rdx\n";
            }
            break;
            case IR__call:
            {
                if (this_line.args.size() > 8)
                {
                    for (int i = 2; i < 8; ++i)
                    {
                        auto cons = parse_symbol(function_title, this_line.args[i]);
                        auto arg_src = cons -> name_as_src(arg_tgt[i - 2]);
                        if (arg_src != arg_tgt[i - 2])
                            text_section << "\tmov\t" << arg_tgt[i - 2] << ", " << arg_src << "\n";
                    }
                    for (int i = 8; i < int(this_line.args.size()); ++i)
                    {
                        auto cons = parse_symbol(function_title, this_line.args[i]);
                        auto arg_src = cons -> name_as_src("rax");
                        text_section << "\tmov\tqword [rsp+" << (i - 8) * 8 << "], " << arg_src << "\n";
                    }
                } else {
                    for (int i = 2; i < int(this_line.args.size()); ++i)
                    {
                        auto cons = parse_symbol(function_title, this_line.args[i]);
                        auto arg_src = cons -> name_as_src(arg_tgt[i - 2]);
                        if (arg_src != arg_tgt[i - 2])
                            text_section << "\tmov\t" << arg_tgt[i - 2] << ", " << arg_src << "\n";
                    }
                }
                text_section << "\tcall\t" << this_line.args[1] << "\n";
                auto cons = parse_symbol(function_title, this_line.args[0]);
                auto arg_src = cons -> name_as_tg();
                if (arg_src != "rax")
                    text_section << "\tmov\t" << arg_src << ", rax\n";
            }
            break;
            case IR__loop_start:
                current_loop_title.push(this_line.args.back());
                text_section << "__loop__start__" << this_line.args.back() << ": \n";
            break;
            case IR__loop_constraint:
                if (this_line.args.back() == "0")
                    text_section << "__loop__constraint__" << this_line.args.front() << ":\n"
                                 << "\tjmp\t__loop__end__" << this_line.args.front() << "\n";
                else if (this_line.args.back() == "1")
                    text_section << "__loop_constraint__" << this_line.args.front() << ":\n";
                else
                {
                    auto cons_var = parse_symbol(function_title, this_line.args.back());
                    auto cons = cons_var -> name_as_tg();
                    text_section << "__loop__constraint__" << this_line.args.front() << ":\n"
                                 << "\tcmp\t" << cons << ", 0\n"
                                 << "\tjz\t__loop__end__" << this_line.args.front() << "\n";
                }
            break;
            case IR__loop_body:
            {
                text_section << "__loop__body__" << this_line.args.front() << ":\n";
            }
            break;
            case IR__loop_iter:
            {
                text_section << "__loop__iter__" << this_line.args.front() << ":\n";
            }
            break;
            case IR__loop_end:
            {
                current_loop_title.pop();
                text_section << "\tjmp\t__loop__start__" << this_line.args.front() << "\n";
                text_section << "__loop__end__" << this_line.args.front() << ":\n";
            }
            break;
            case IR__cond:
            {
                if (this_line.args.back() == "0")
                    text_section << "\tjmp\t__cond__else__" << this_line.args.front() << "\n"
                                 << "__cond__" << this_line.args.front() << ":\n";
                else if (this_line.args.back() == "1")
                    text_section << "__cond__" << this_line.args.front() << ":\n";
                else
                {
                    auto cons_var = parse_symbol(function_title, this_line.args.back());
                    auto cons = cons_var -> name_as_tg();
                    text_section << "\tcmp\t" << cons << ", 0\n"
                                 << "\tjz\t__cond__else__" << this_line.args.front() << "\n"
                                 << "__cond__" << this_line.args.front() << ":\n";
                }
            }
            break;
            case IR__cond_else:
            {
                text_section << "\tjmp\t__cond__end__" << this_line.args.front() << "\n"
                             << "__cond__else__" << this_line.args.front() << ":\n";
            }
            break;
            case IR__cond_end:
            {
                text_section << "__cond__end__" << this_line.args.front() << ":\n";
            }
            break;
            case IR__inc:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.back(), "rax");
                auto cons1 = cons_var -> name_as_tg();
                text_section << "\tinc\t" << cons1 << "\n";
            }
            break;
            case IR__dec:
            {
                auto cons_var = parse_symbol(function_title, this_line.args.back(), "rax");
                auto cons1 = cons_var -> name_as_tg();
                text_section << "\tdec\t" << cons1 << "\n";
            }
            break;
            case IR__malloc:
            {
                string & arg1 = this_line.args.front(), arg2 = this_line.args.back();
                auto cons_var = parse_symbol(function_title, arg2, "rax");
                auto cons1 = cons_var -> name_as_tg();
                if (cons1 != "rdi")
                    text_section << "\tmov\trdi, " << cons1 << "\n";
                text_section << "\tcall\t__msharp__malloc__\n";
                cons_var = parse_symbol(function_title, arg1, "rcx");
                auto cons2 = cons_var -> name_as_tg();
                text_section << "\tmov\t" << cons2 << ", rax\n";
            }
            break;
           /* default:
                Error.compile_error("not implemented:" + IR::repr_IRType(this_line.oprand));
            break;*/
            }
        }
        fout << text_section.str() << "\n";
        fout << "SECTION .data\n";
        deque<string> & const_str(Parser.name_table.constant_string);
        for (auto & p : global_remap)
        {
            fout << "__msharp__global__" << p.first << ":\n\tdq\t0\n";
        }
        fout << "SECTION .text\n";
        for (int i = 0; i < int(const_str.size()); ++i)
        {
            string & current(const_str[i]);
            data_section << "\tdq\t" << current.length() << "\n"
                         << "__conststr_" << Tool.to_string(i) << ":\n";
            data_section << "\tdb\t";
            for (int i = 0; i < int(current.size()); ++i) data_section << int(current[i]) << ", ";
            for (int i = 0; (current.size() + 1 + i) % 8 != 0; ++i) data_section << "0, ";
            data_section << "0\n";
        }
        fout << data_section.str() << endl;
    }

    string _Translator::MemoryModel::name_as_src(string tmp_reg)
    {
#ifndef __infinity__reg__assumption
        ostream & fout(*(this -> sout));
#else
        Refiner & fout(*(this -> sout));
#endif
        fout << "\tmov\t" << tmp_reg << ", " << name_as_tg() << '\n';
        return tmp_reg;
    }

    }
