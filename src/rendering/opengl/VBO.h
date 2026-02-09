#pragma once
#include "../../../libs/glad/include/glad/glad.h"

class VBO
{
public:
	GLuint ID;
	VBO(GLfloat* vertices, GLsizeiptr size, GLenum usage = GL_STATIC_DRAW);
	VBO();

	void Init(GLsizeiptr size, GLenum usage = GL_DYNAMIC_DRAW);
	void Update(const void* data, GLsizeiptr size, GLintptr offset = 0);
	void Bind();
	void Unbind();
	void Delete();
};