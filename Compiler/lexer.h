#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "token.h"

class Lexer{
public:
    explicit Lexer(const std::string& src);

    //function to create tokens
    std::vector<Token> tokenize(); 

private:
    std::string src;
    size_t i; //index in src
    int line; //position in src
    int col;


    bool atEnd() const;
    char look() const;
    char lookNext() const;
    char advance();

    static bool isIdentifierStart(char c);
    static bool isIdentifier(char c);

    static const std::unordered_map<std::string, std::string> KEYWORDS;
    static const std::unordered_map<std::string, std::string> TWO_CHAR;
    static const std::unordered_map<char, std::string> ONE_CHAR;
};