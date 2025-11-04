#pragma once
#include <vector>
#include <string>
#include "token.h"
#include "emitter.h"

class Parser
{
public:
    Parser(const std::vector<Token>& tokenList, Emitter& emitterInstance);

    //entry point
    void parseProgram();

private:
    //token stream
    std::vector<Token> tokens;
    size_t currentIndex;
    Emitter& emitter;

    //functions
    const Token& currentToken() const;
    const Token& previousToken() const;
    bool checkType(const std::string& type) const;
    bool checkValue(const std::string& value) const;
    void advance();
    void expectType(const std::string& type, const std::string& context);
    void error(const std::string& message) const;

    //grammar rules
    void statement();
    std::string comparison();
    std::string expression();
    std::string term();
    std::string unary();
    std::string primary();
};
