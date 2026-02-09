#pragma once
#include "../../../libs/glad/include/glad/glad.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cerrno>

std::string get_file_contents(const char* filename);
std::filesystem::path getExecutableDir();

class Shader
{
public:
	GLuint ID;
	Shader(const char* vertexFile, const char* fragmentFile);

	void Activate();
	void Delete();
private:
	void compileErrors(unsigned int shader, const char* type);
};
