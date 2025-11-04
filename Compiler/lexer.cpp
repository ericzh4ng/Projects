#include "lexer.h"
#include <cctype>
#include <stdexcept>

//define keywords
const std::unordered_map<std::string, std::string> Lexer::KEYWORDS = {
    {"print", "PRINT"}, {"if", "IF"}, {"then", "THEN"}, {"endif", "ENDIF"},
    {"let", "LET"}, {"input", "INPUT"}, {"while", "WHILE"}, {"repeat", "REPEAT"},
    {"endwhile", "ENDWHILE"}, {"goto", "GOTO"}, {"label", "LABEL"}
};
//define two character tokens
const std::unordered_map<std::string, std::string> Lexer::TWO_CHAR = {
    {"==", "COMP"}, {"!=", "COMP"}, {"<=", "COMP"}, {">=", "COMP"}
};

//define single character tokens
const std::unordered_map<char, std::string> Lexer::ONE_CHAR = {
    {'=', "ASSIGN"}, {'<', "COMP"}, {'>', "COMP"}, {'!', "NOT"},
    {'+', "PLUS"}, {'-', "MINUS"}, {'*', "TIMES"}, {'/', "DIVIDE"},
    {';', "SEMICOLON"}, {'(', "LPAREN"}, {')', "RPAREN"}
};

//constructor
Lexer::Lexer(const std::string& s) : src(s), i(0), line(1), col(1)
{}

bool Lexer::atEnd() const 
{
    if(i >= src.size())
    {
        return true;
    }
    else
    {
        return false;
    }
    
}
char Lexer::look() const 
{
    if(atEnd())
    {
        return '\0';
    }
    else
    {
        return src[i];
    }
}
char Lexer::lookNext() const 
{
    if (i + 1 < src.size()) {
        return src[i + 1];
    }
    else {
        return '\0';
    }
}

char Lexer::advance() 
{ 
    char curr = look();
    if (curr == '\n')
    {
        line ++;
        col = 1;
    }
    else 
    {
        col++; 
    }
    i++;
    return curr;
}

bool Lexer::isIdentifierStart(char c)
{
    if (std::isalpha(c)) 
    {
        return true;
    }
    if (c == '_')
    {
        return true;
    }
    return false;
}
bool Lexer::isIdentifier(char c) {
    if (std::isalnum(c)) {
        return true;
    }
    if (c == '_') {
        return true;
    }
    return false;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!atEnd()) {
        char currentChar = look();
        
        //skip whitespace
        if (std::isspace(currentChar))
        {
            advance(); 
            continue;
        }

        //skip comments (# is used in the teeny compiler)
        if (currentChar == '#') {
            while (!atEnd() && look() != '\n')
            {
                advance();
            }
            continue;
        }

        //strings
        if (currentChar == '"') {
            int start_line = line;
            int start_col = col;
            advance(); //skip initial "
            std::string value;
            while (!atEnd() && look() != '"') {
                char ch = advance();
                if (ch == '\\' && !atEnd()) { //handle escape sequences
                    char next = advance();
                    if (next == 'n')
                    {
                        value.push_back('\n');
                    }
                    else if (next == 't')
                    {
                        value.push_back('\t');
                    }
                    else if (next == '"')
                    {
                        value.push_back('"');
                    }
                    else
                    {
                        value.push_back(next);
                    }
                } else {
                    value.push_back(ch);
                }
            }
            if (atEnd())
            {
                throw std::runtime_error("Unterminated string at line " + std::to_string(start_line));
            }
            advance(); // closing "
            tokens.push_back({value, "STRING", start_line, start_col});
            continue;
        }

        //two-char operators
        std::string two = {currentChar, lookNext()};
        if (TWO_CHAR.count(two)) {
            tokens.push_back({two, TWO_CHAR.at(two), line, col});
            advance();
            advance();
            continue;
        }

        //one-char operators
        if (ONE_CHAR.count(currentChar)) {
            tokens.push_back({std::string(1, currentChar), ONE_CHAR.at(currentChar), line, col});
            advance();
            continue;
        }

        //nums
        if (std::isdigit(currentChar)) {
            int start_line = line;
            int start_col = col;
            std::string num;
            while (!atEnd() && std::isdigit(look()))
            {
                num.push_back(advance());
            }
            tokens.push_back({num, "INTEGER", start_line, start_col});
            continue;
        }

        //keywords/identifiers
        if (isIdentifierStart(currentChar)) {
            int start_line = line;
            int start_col = col;
            std::string id;
            while (!atEnd() && isIdentifier(look()))
            {
                id.push_back(advance());
            }

            std::string lower = id;
            for (char& ch : lower)
            {
                ch = std::tolower(ch);
            }

            if (KEYWORDS.count(lower))
            {
                tokens.push_back({lower, KEYWORDS.at(lower), start_line, start_col});
            }
            else
            {
                tokens.push_back({id, "IDENT", start_line, start_col});
            }
            continue;
        }

        throw std::runtime_error("Unknown character '" + std::string(1, currentChar) + "' at line " + std::to_string(line));
    }

    tokens.push_back({"", "EOF", line, col});
    return tokens;
}