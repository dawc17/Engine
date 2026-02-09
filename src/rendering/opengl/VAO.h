#pragma once
#include "../../../libs/glad/include/glad/glad.h"
#include "VBO.h"

class VAO
{
public:
	GLuint ID;
	VAO();

	void LinkAttrib(VBO& vbo, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);
	void LinkAttribInstanced(VBO& vbo, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset, GLuint divisor = 1);
	void Bind();
	void Unbind();
	void Delete();
};