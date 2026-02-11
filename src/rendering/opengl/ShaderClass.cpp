#include"ShaderClass.h"
#include "embedded_assets.h"
#include <cstring>

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
	struct Entry { const char* name; const unsigned char* data; unsigned int size; };
	static const Entry shaders[] = {
		{"default.vert", embed_default_vert_data, embed_default_vert_size},
		{"default.frag", embed_default_frag_data, embed_default_frag_size},
		{"selection.vert", embed_selection_vert_data, embed_selection_vert_size},
		{"selection.frag", embed_selection_frag_data, embed_selection_frag_size},
		{"destroy.vert", embed_destroy_vert_data, embed_destroy_vert_size},
		{"destroy.frag", embed_destroy_frag_data, embed_destroy_frag_size},
		{"water.vert", embed_water_vert_data, embed_water_vert_size},
		{"water.frag", embed_water_frag_data, embed_water_frag_size},
		{"particle.vert", embed_particle_vert_data, embed_particle_vert_size},
		{"particle.frag", embed_particle_frag_data, embed_particle_frag_size},
		{"item_model.vert", embed_item_model_vert_data, embed_item_model_vert_size},
		{"item_model.frag", embed_item_model_frag_data, embed_item_model_frag_size},
		{"tool_model.vert", embed_tool_model_vert_data, embed_tool_model_vert_size},
		{"tool_model.frag", embed_tool_model_frag_data, embed_tool_model_frag_size},
	};

	for (const auto& s : shaders)
	{
		if (std::strcmp(filename, s.name) == 0)
			return std::string(reinterpret_cast<const char*>(s.data), s.size);
	}

	throw std::runtime_error("Embedded shader not found: " + std::string(filename));
}

Shader::Shader(const char* vertexFile, const char* fragmentFile)
{
	std::string vertexCode = get_file_contents(vertexFile);
	std::string fragmentCode = get_file_contents(fragmentFile);

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	compileErrors(vertexShader, "VERTEX");

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	compileErrors(fragmentShader, "FRAGMENT");

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);
	compileErrors(ID, "PROGRAM");

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

}

void Shader::Activate()
{
	glUseProgram(ID);
}

void Shader::Delete()
{
	glDeleteProgram(ID);
}

void Shader::compileErrors(unsigned int shader, const char* type)
{
	GLint hasCompiled;
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
