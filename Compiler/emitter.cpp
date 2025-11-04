#include "emitter.h"

//add header like #include
void Emitter::addHeader(const std::string& headerLine)
{
    headers << headerLine << "\n";
}

//declare the variable
void Emitter::ensureVar(const std::string& variableName)
{
    if (declaredVars.find(variableName) == declaredVars.end())
    {
        declarations << "int " << variableName << " = 0;\n";
        declaredVars.insert(variableName);
    }
}

//add line of code toto body
void Emitter::addLine(const std::string& codeLine)
{
    body << codeLine << "\n";
}

//combine all into full code
std::string Emitter::getCode() const
{
    std::string result;

    result += headers.str();
    result += "\nint main()\n{\n";
    result += declarations.str();
    result += body.str();
    result += "    return 0;\n}\n";

    return result;
}
