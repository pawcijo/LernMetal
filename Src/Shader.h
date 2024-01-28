/*Hold shader in memory.*/


//TODO hold less momry. 

#include <string>

class Shader
{
    public:
    Shader(const char *shaderPath);
    std::string ShaderString();

    Shader() = delete;

private:
    std::string shaderString;
};