#include "Shader.h"
#include <iostream>
#include <fstream>

Shader::Shader(const char *shaderPath)
{
    shaderString = std::string();
    // Open the file
    std::ifstream inputFile(shaderPath,std::ios::in);

    // Check if the file is open
    if (inputFile.is_open())
    {
            
        // Read and append each line from the file to the string
        std::string line;
        while (getline(inputFile, line))
        {
            shaderString += line + '\n'; // Add newline for each line
        }
        // Close the file when done
        inputFile.close();
        // Now 'fileContent' holds the content of the file
    }
    else
    {
        // Print an error message if the file couldn't be opened
        std::cerr << "Unable to open the file: " << shaderPath << std::endl;
    }

}

std::string Shader::ShaderString()
{
    return shaderString;
}