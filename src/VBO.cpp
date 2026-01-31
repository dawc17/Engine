#include "VBO.h"

VBO::VBO(GLfloat* vertices, GLsizeiptr size, GLenum usage)
	: ID(0)
{
	glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, usage);
}

VBO::VBO()
	: ID(0)
{
}

void VBO::Init(GLsizeiptr size, GLenum usage)
{
	if (ID == 0)
		glGenBuffers(1, &ID);
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferData(GL_ARRAY_BUFFER, size, nullptr, usage);
}

void VBO::Update(const void* data, GLsizeiptr size, GLintptr offset)
{
	glBindBuffer(GL_ARRAY_BUFFER, ID);
	glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}

void VBO::Bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete()
{
	if (ID != 0)
	{
		glDeleteBuffers(1, &ID);
		ID = 0;
	}
}