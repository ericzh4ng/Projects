#include "parser.h"
#include <stdexcept>
#include <iostream>

// ---------------------------------------------
// Constructor
// ---------------------------------------------
Parser::Parser(const std::vector<Token>& tokenList, Emitter& emitterInstance)
    : tokens(tokenList), currentIndex(0), emitter(emitterInstance)
{
}

// ---------------------------------------------
// Utility helper functions
// ---------------------------------------------
const Token& Parser::currentToken() const
{
    return tokens[currentIndex];
}

const Token& Parser::previousToken() const
{
    return tokens[currentIndex - 1];
}

bool Parser::checkType(const std::string& type) const
{
    return currentToken().type == type;
}

bool Parser::checkValue(const std::string& value) const
{
    return currentToken().value == value;
}

void Parser::advance()
{
    if (currentIndex < tokens.size() - 1)
    {
        currentIndex++;
    }
}

void Parser::expectType(const std::string& type, const std::string& context)
{
    if (!checkType(type))
    {
        error(context + " — expected '" + type + "', got '" + currentToken().type + "'");
    }
    advance();
}

void Parser::error(const std::string& message) const
{
    const Token& t = currentToken();
    throw std::runtime_error(
        "Parser error at line " + std::to_string(t.line) + ", column " + std::to_string(t.col) +
        ": " + message
    );
}

// ---------------------------------------------
// Main parsing entry point
// ---------------------------------------------
void Parser::parseProgram()
{
    emitter.addHeader("#include <stdio.h>");
    emitter.addHeader("#include <stdlib.h>");

    while (!checkType("EOF"))
    {
        statement();
    }
}

// ---------------------------------------------
// Grammar rule: statement
// ---------------------------------------------
void Parser::statement()
{
    // print "string";
    if (checkType("PRINT"))
    {
        advance();

        if (checkType("STRING"))
        {
            std::string text = currentToken().value;
            advance();

            // Escape quotes and percent symbols
            std::string escaped;
            for (char c : text)
            {
                if (c == '%')
                {
                    escaped += "%%";
                }
                else if (c == '\\')
                {
                    escaped += "\\\\";
                }
                else if (c == '\"')
                {
                    escaped += "\\\"";
                }
                else if (c == '\n')
                {
                    escaped += "\\n";
                }
                else
                {
                    escaped.push_back(c);
                }
            }

            emitter.addLine("printf(\"%s\\n\", \"" + escaped + "\");");
        }
        else
        {
            std::string expr = comparison();
            emitter.addLine("printf(\"%d\\n\", (" + expr + "));");
        }

        expectType("SEMICOLON", "after print statement");
        return;
    }

    // input variable;
    if (checkType("INPUT"))
    {
        advance();

        if (!checkType("IDENT"))
        {
            error("Expected identifier after 'input'");
        }

        std::string name = currentToken().value;
        emitter.ensureVar(name);
        advance();

        emitter.addLine("{ if (scanf(\"%d\", &" + name +
                        ") != 1) { fprintf(stderr, \"Input error\\n\"); exit(1); } }");

        expectType("SEMICOLON", "after input statement");
        return;
    }

    // let variable = expression;
    if (checkType("LET"))
    {
        advance();

        if (!checkType("IDENT"))
        {
            error("Expected identifier after 'let'");
        }

        std::string name = currentToken().value;
        emitter.ensureVar(name);
        advance();

        expectType("ASSIGN", "assignment");
        std::string expr = comparison();

        emitter.addLine(name + " = (" + expr + ");");
        expectType("SEMICOLON", "after assignment");
        return;
    }

    // if comparison then ... endif
    if (checkType("IF"))
    {
        advance();
        std::string condition = comparison();
        expectType("THEN", "after if condition");
        emitter.addLine("if (" + condition + ") {");

        while (!checkType("ENDIF"))
        {
            if (checkType("EOF"))
            {
                error("Unclosed 'if' statement");
            }
            statement();
        }

        advance(); // consume ENDIF
        emitter.addLine("}");
        return;
    }

    // while comparison repeat ... endwhile
    if (checkType("WHILE"))
    {
        advance();
        std::string condition = comparison();
        expectType("REPEAT", "after while condition");
        emitter.addLine("while (" + condition + ") {");

        while (!checkType("ENDWHILE"))
        {
            if (checkType("EOF"))
            {
                error("Unclosed 'while' loop");
            }
            statement();
        }

        advance(); // consume ENDWHILE
        emitter.addLine("}");
        return;
    }

    // label name;
    if (checkType("LABEL"))
    {
        advance();

        if (!checkType("IDENT"))
        {
            error("Expected label name");
        }

        std::string label = currentToken().value;
        advance();

        emitter.addLine(label + ": ;");
        expectType("SEMICOLON", "after label");
        return;
    }

    // goto name;
    if (checkType("GOTO"))
    {
        advance();

        if (!checkType("IDENT"))
        {
            error("Expected label name after 'goto'");
        }

        std::string label = currentToken().value;
        advance();

        emitter.addLine("goto " + label + ";");
        expectType("SEMICOLON", "after goto");
        return;
    }

    // Unknown statement
    error("Unexpected token: " + currentToken().type);
}

// ---------------------------------------------
// Grammar rules for expressions
// ---------------------------------------------
std::string Parser::comparison()
{
    std::string left = expression();

    while (checkType("COMP"))
    {
        std::string op = currentToken().value;
        advance();
        std::string right = expression();
        left = "(" + left + " " + op + " " + right + ")";
    }

    return left;
}

std::string Parser::expression()
{
    std::string left = term();

    while (checkType("PLUS") || checkType("MINUS"))
    {
        std::string op = currentToken().type == "PLUS" ? "+" : "-";
        advance();
        std::string right = term();
        left = "(" + left + " " + op + " " + right + ")";
    }

    return left;
}

std::string Parser::term()
{
    std::string left = unary();

    while (checkType("TIMES") || checkType("DIVIDE"))
    {
        std::string op = currentToken().type == "TIMES" ? "*" : "/";
        advance();
        std::string right = unary();
        left = "(" + left + " " + op + " " + right + ")";
    }

    return left;
}

std::string Parser::unary()
{
    if (checkType("PLUS"))
    {
        advance();
        return unary();
    }

    if (checkType("MINUS"))
    {
        advance();
        return "(-" + unary() + ")";
    }

    if (checkType("NOT"))
    {
        advance();
        return "(!" + unary() + ")";
    }

    return primary();
}

std::string Parser::primary()
{
    if (checkType("INTEGER"))
    {
        std::string value = currentToken().value;
        advance();
        return value;
    }

    if (checkType("IDENT"))
    {
        std::string name = currentToken().value;
        emitter.ensureVar(name);
        advance();
        return name;
    }

    if (checkType("LPAREN"))
    {
        advance();
        std::string inside = comparison();
        expectType("RPAREN", "closing parenthesis");
        return "(" + inside + ")";
    }

    error("Expected expression");
    return "";
}
