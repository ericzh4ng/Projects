#pragma once
#include  <string>

struct Token{
    std::string value; //token value
    std::string type; //token type
    int line; //line number in source
    int col; //column number in source
};
