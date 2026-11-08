/*
 * Desktop-GL entry points referenced by DarkPlaces' USE_GLES2 path that
 * Android's GLES libraries do not export under these names.
 */
#include <GLES3/gl32.h>

void *glMapBuffer(GLenum target, GLenum access)
{
	GLint size = 0;
	(void)access; // DarkPlaces only maps PBOs read-only (GL_READ_ONLY)
	glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);
	if (size <= 0)
		return 0;
	return glMapBufferRange(target, 0, size, GL_MAP_READ_BIT);
}

typedef void (GL_APIENTRY *dp_debugproc)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *, const void *);

void glDebugMessageCallbackARB(dp_debugproc callback, const void *userParam)
{
	glDebugMessageCallback((GLDEBUGPROC)callback, userParam);
}

void glDebugMessageControlARB(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
	glDebugMessageControl(source, type, severity, count, ids, enabled);
}

void glDebugMessageInsertARB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
	glDebugMessageInsert(source, type, id, severity, length, buf);
}

GLuint glGetDebugMessageLogARB(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog)
{
	return glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}
