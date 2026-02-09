#include"ShaderClass.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

std::filesystem::path getExecutableDir()
{
	namespace fs = std::filesystem;
#ifdef _WIN32
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	return fs::path(path).parent_path();
#else
	char path[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len != -1)
	{
		path[len] = '\0';
		return fs::path(path).parent_path();
	}
	return fs::current_path();
#endif
}

std::string get_file_contents(const char* filename)
{
	namespace fs = std::filesystem;

	auto read_stream = [](std::ifstream& in) {
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(static_cast<size_t>(in.tellg()));
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return contents;
	};

	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		return read_stream(in);
	}

	fs::path exeDir = getExecutableDir();
	fs::path shadersPath = exeDir / "shaders" / filename;
	in.open(shadersPath, std::ios::binary);
	if (in)
	{
		return read_stream(in);
	}

#ifdef SHADER_DIR
	fs::path fallback = fs::path(SHADER_DIR) / filename;
	in.open(fallback, std::ios::binary);
	if (in)
	{
		return read_stream(in);
	}
#endif

	throw std::runtime_error("Failed to open shader file: " + fs::path(filename).string());
}

// Constructor that build the Shader Program from 2 different shaders
Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// Create Vertex Shader Object and get its reference
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	// Attach Vertex Shader source to the Vertex Shader Object
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(vertexShader);
	// Checks if Shader compiled succesfully
	compileErrors(vertexShader, "VERTEX");

	// Create Fragment Shader Object and get its reference
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	// Attach Fragment Shader source to the Fragment Shader Object
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	// Compile the Vertex Shader into machine code
	glCompileShader(fragmentShader);
	// Checks if Shader compiled succesfully
	compileErrors(fragmentShader, "FRAGMENT");

	// Create Shader Program Object and get its reference
	ID = glCreateProgram();
	// Attach the Vertex and Fragment Shaders to the Shader Program
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	// Wrap-up/Link all the shaders together into the Shader Program
	glLinkProgram(ID);
	// Checks if Shaders linked succesfully
	compileErrors(ID, "PROGRAM");

	// Delete the now useless Vertex and Fragment Shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

}

// Activates the Shader Program
void Shader::Activate()
{
	glUseProgram(ID);
}

// Deletes the Shader Program
void Shader::Delete()
{
	glDeleteProgram(ID);
}

// Checks if the different Shaders have compiled properly
void Shader::compileErrors(unsigned int shader, const char* type)
{
	// Stores status of compilation
	GLint hasCompiled;
	// Character array to store error message in
	char infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
}
