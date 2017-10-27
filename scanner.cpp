#include "scanner.hpp"

namespace msharp
{
    set<char> separator = {' ', '\t', '\n', '\r'};
    set<char> identifier_dict;
    set<char> integer_dict;
    size_t line_number = 1;
    size_t pos = 0;
    deque<deque<string>> leveled_operator
                                = {//0
                                   {"="},
                                   //1
                                   {"&&"},
                                   {"||"},
                                   //2
                                   {"==", "!=", "<", ">", ">=", "<="},
                                   //3 -> 0, actually 3
                                   {"|"},
                                   {"^"},
                                   {"&"},
                                   //3 -> 1, actually 4
                                   {">>", "<<"},
                                   //4 -> 0, actually 5, can be used as prefices
                                   {"-", "+"},
                                   //4 -> 1, actually 6
                                   //4 -> 2, actually 7
                                   {"/", "*", "%"},
                                   //pref & appendix, 8
                                   {".", "new", "~", "!", "++", "--"},
                                   //5, actually 9
                                   {"[]"},
                                   //6, actually 10
                                   {"."}
                                   };
    set<string> builtintype = {"bool", "int", "string", "void"};
    set<string> builtinconst = {"null", "true", "false", "this"};
    set<string> keywordlist = {"if", "for", "while", "break", "continue", "return", "class", "else"};
    set<string> reserved_word = {"bool", "int", "string", "void", "this",
                                 "null", "true", "false",
                                 "if", "for", "while", "break", "continue", "return", "new", "class", "else"};
    set<string> operatorlist = {"+", "-", "*", "/", "%", "<", ">", "==", "!=", "<=", ">=", "&&", "[]", "||", "!", "<<", ">>", "~", "|", "^", "&", "=", "++", "--", ".", "[", "]", "(", ")"};
    std::ostream & operator<<(std::ostream & fout, const Token & token)
    {
        fout << "<at line: " << token.line << ", col: " << token.col << ">";
        switch (token.type)
        {
        case line_counter:
            fout << "\nLine " << token.value << ":\n";
        break;
        case block_begin:
            fout << "<code block begin>";
        break;
        case block_end:
            fout << "<code block end>";
        break;
        case op:
            fout << "<operator " << token.value << ">";
        break;
        case comma:
            fout << "<comma>";
        break;
        case id:
            fout << "<identifier " << token.value << ">";
        break;
        case keyword:
            fout << "<keyword " << token.value << ">";
        break;
        case integer:
            fout << "<literal integer: " << token.value << ">";
        break;
        case conststr:
            fout << "<literal string: \"" << token.value << "\">";
        break;
        case description:
            fout << "<description: \"" << token.value << "\">";
        break;
        case expr_end:
            fout << "<expression end>";
        break;
        case to_be_determined:
            if (token.value != "")
                fout << "<LEXER_ERROR!!! : incomplete scanning element \"" << token.value << "\">";
        break;
        }
        return fout;
    }

    _Scanner::_automata &_Scanner::_automata::append(char c)
    {
        static Token tmp;
        switch (tmp.type)
        {
        case to_be_determined:
        {
            if (is_in(c, separator))
                return *this;
            //determine whether it is a literal integer
            if (c == '{')
            {
                lexemes.push_back(Token(block_begin, "{"));
                tmp = Token();
                return *this;
            }
            if (c == '}')
            {
                lexemes.push_back(Token(block_end, "}"));
                tmp = Token();
                return *this;
            }
            if (c == ';')
            {
                lexemes.push_back(Token(expr_end, ";"));
                tmp = Token();
                return *this;
            }
            if (c == ',')
            {
                lexemes.push_back(Token(comma, ","));
                tmp = Token();
                return *this;
            }
            if (c >= '0' && c <= '9')
            {
                tmp = Token(integer, string() + c);
            } else {
                //determine whether it is a literal string
                if (c == '\"')
                {
                    tmp = Token(conststr, "");
                } else {
                    tmp.value = string() + c;
                    //determine whether it is a probable operator
                    if (is_in(tmp.value, operatorlist))
                        tmp = Token(op, tmp.value);
                    else
                        tmp = Token(id, tmp.value);
                }
            }
        }
        break;
        case op:
        {
            if (is_in(c, separator))
            {
                lexemes.push_back(tmp);
                tmp = Token();
                return *this;
            }
            if (tmp.value == "/" && c == '/')
            {
                tmp = Token(description, "");
                return *this;
            }
            if (!is_in(tmp.value + c, operatorlist))
            {
                lexemes.push_back(tmp);
                tmp = Token();
                return append(c);
            }
            tmp.value += c;
        }
        break;
        case id:
        {
            if (is_in(c, identifier_dict))
            {
                tmp.value += c;
                return *this;
            }
            if (is_in(tmp.value, keywordlist))
                tmp.type = keyword;
            if (tmp.value == "new") tmp.type = op;
            lexemes.push_back(tmp);
            tmp = Token();
            return append(c);
        }
        break;
        case integer:
        {
            if (is_in(c, identifier_dict))
            {
                tmp.value += c;
                return *this;
            }
            lexemes.push_back(tmp);
            tmp = Token();
            return append(c);
        }
        break;
        case conststr:
        {
            static bool flag_esc = false;
            if (flag_esc)
            {
                flag_esc = false;
                switch (c)
                {
                case 't':
                    c = '\t';
                break;
                case 'n':
                    c = '\n';
                break;
                }
                tmp.value += c;
                return *this;
            }
            if (c == '\"')
            {
                lexemes.push_back(tmp);
                tmp = Token();
                return *this;
            }
            if (c == '\\')
            {
                flag_esc = true;
                return *this;
            }
            tmp.value += c;
            return *this;
        }
        break;
        case description:
        {
            if (c == '\n')
            {
                //lexemes.push_back(tmp);
                //std::cout << tmp << std::endl;
                tmp = Token();
                return *this;
            }
            tmp.value += c;
            return *this;
        }
        break;
        default:
        break;
        }
        return *this;
    }

    _Scanner &_Scanner::append(const string & src)
    {
        string tmp;
        pos = 0;
        int old_pos = lexemes.size();
        while (pos < src.size())
        {
            automata.append(src[pos]);
            ++pos;
        }
        automata.append('\n');
        for (old_pos = std::max(1, old_pos); old_pos < int(lexemes.size()); ++old_pos)
        {
            if (lexemes[old_pos].type == op && lexemes[old_pos].value == "]")
            {
                if (lexemes[old_pos - 1].type == op  && lexemes[old_pos - 1].value == "[")
                {
                    lexemes[old_pos - 1].value = "[]";
                    lexemes.erase(lexemes.begin() + old_pos, lexemes.begin() + old_pos + 1);
                }
            }
        }
        line_number++;
        return *this;
    }

}
