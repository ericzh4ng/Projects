#pragma once
#include <string>
#include <unordered_set>
#include <sstream>

class Emitter
{
public:
    //adds header line (#include etc.)
    void addHeader(const std::string& headerLine);

    //declare variables only once
    void ensureVar(const std::string& variableName);

    //add line
    void addLine(const std::string& codeLine);

    //final C code as a single string
    std::string getCode() const;

private:
    std::stringstream headers;
    std::stringstream declarations;
    std::stringstream body;
    std::unordered_set<std::string> declaredVars;
};
