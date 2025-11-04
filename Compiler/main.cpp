#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "lexer.h"
#include "parser.h"
#include "emitter.h"

//helper func to see if ends with given suffix
bool hasSuffix(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length())
    {
        return false;
    }

    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <file.basic>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];

    if (!hasSuffix(inputPath, ".basic"))
    {
        std::cerr << "Error: Input file must have a .basic extension." << std::endl;
        return 1;
    }

    //read into string
    std::ifstream inputFile(inputPath);

    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open input file: " << inputPath << std::endl;
        return 1;
    }

    std::string sourceCode(
        (std::istreambuf_iterator<char>(inputFile)),
        std::istreambuf_iterator<char>()
    );

    inputFile.close();

    //run the other files i made
    try
    {
        Lexer lexer(sourceCode);
        std::vector<Token> tokens = lexer.tokenize();

        Emitter emitter;
        Parser parser(tokens, emitter);

        parser.parseProgram();

        //transpile into c
        std::string outputPath = inputPath + ".c";

        std::ofstream outputFile(outputPath);

        if (!outputFile.is_open())
        {
            std::cerr << "Error: Could not write to output file: " << outputPath << std::endl;
            return 1;
        }

        outputFile << emitter.getCode();
        outputFile.close();

        std::cout << "Successfully transpiled to: " << outputPath << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Compilation error: " << ex.what() << std::endl;
        return 1;
    }
}
