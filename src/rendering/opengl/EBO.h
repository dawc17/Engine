#pragma once
#include "../../../libs/glad/include/glad/glad.h"

class EBO
{
public:
	GLuint ID;
	EBO(GLuint* indices, GLsizeiptr size);

	void Bind();
	void Unbind();
	void Delete();
};