#ifndef SCANNER_HPP
#define SCANNER_HPP
#include <string>
#include <set>
#include <deque>
#include <iostream>
#include "tools.hpp"
using std::size_t;
using std::string;
using std::set;
using std::deque;
namespace msharp
{
    extern set<char> separator;
    extern set<char> identifier_dict;
    extern set<char> integer_dict;
    extern set<string> builtintype;
    extern set<string> keywordlist;
    extern set<string> operatorlist;
    extern set<string> reserved_word;
    extern deque<deque<string>> leveled_operator;
    enum TokenType
    {
        //types that don't need an automata:
        line_counter, comma,
        block_begin, block_end, expr_end,
        //types that need an automata
        op, id, keyword,
        integer, conststr,
        description,
        to_be_determined
    };
    extern size_t line_number;
    extern size_t pos;
    struct Token
    {
        size_t line, col;
        TokenType type;
        string value;
        Token()
            : line(size_t(line_number)), col(pos), type(to_be_determined), value() {}
        Token(TokenType _type, const string & _value)
            : line(size_t(line_number)), col(pos), type(_type), value(_value) {}
    };
    std::ostream & operator<<(std::ostream & fout, const Token & token);
    typedef deque<Token> List;
    class _Scanner
    {
    public:

        List lexemes;
        _Scanner()
            : automata(lexemes)
        {
            for (char i = 'a'; i <= 'z'; ++i) identifier_dict.insert(i);
            for (char i = '0'; i <= '9'; ++i) identifier_dict.insert(i), integer_dict.insert(i);
            for (char i = 'A'; i <= 'Z'; ++i) identifier_dict.insert(i);
            identifier_dict.insert('_');
            line_number = 1;
        }

        class _automata
        {
        public:
            deque<Token> & lexemes;
            _automata(deque<Token> & _lexemes)
                : lexemes(_lexemes) {}
            _automata & append(char c);
        };
        _automata automata;
        _Scanner & append(const string & src);
    };
}
#endif // MSHARP_UTILITY_HPP
