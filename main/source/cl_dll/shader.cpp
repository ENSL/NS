#include "hud.h"
#include "cl_util.h"

//#include "PlatformHeaders.h"
#ifdef _WIN32
#include "windows.h"
#endif

#include <GL/glew.h>
//#include <gl/GL.h>
#include "util/ShaderUtil.h"

cvar_t* cl_postprocess = NULL;
cvar_t* cl_intensity = NULL;

GLuint screenTexture;
GLenum glew;
ShaderUtil shaderUtil;

void CPostProcessShader::Init()
{
	//glActiveTexture(GL_TEXTURE0);
	//glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTextAtInit);

	// Create the screen texture
	glGenTextures(1, &screenTexture);
	glBindTexture(GL_TEXTURE_2D, screenTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ScreenWidth(), ScreenHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	cl_postprocess = CVAR_CREATE("cl_postprocess", "1", FCVAR_ARCHIVE);
	cl_intensity = CVAR_CREATE("cl_intensity", "0.55", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_intensityalt", "1", FCVAR_ARCHIVE);

	// Store shaders in dll so swapping in different ones is harder.
	const std::string vertShader =
		"#version 330 core\n"

		"layout(location = 0) in vec2 vertexPosition;\n"
		"out vec2 texCoord;\n"

		"void main() {\n"
		// Convert vertex position to texture coordinates
		"texCoord = (vertexPosition + 1.0) * 0.5;\n"
		"gl_Position = vec4(vertexPosition, 0.0, 1.0);\n"
		"}\n"
		;

	const std::string fragShader =
		"#version 330 core\n"

		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"

		"uniform sampler2D textureSampler;\n"
		"uniform float colorMultiplier;\n"

		"void main() {\n"
		// Sample the texture
		"vec4 texColor = texture(textureSampler, texCoord);\n"
		// Multiply the color
		"fragColor = vec4(texColor.rgb * colorMultiplier, 1.0f);\n"
		"}\n"
		;

	glew = glewInit();

	if (GLEW_OK != glew)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		// fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		gEngfuncs.Con_DPrintf("[GLEW] Error: %s\n", glewGetErrorString(glew));
	}
	else
	{		
		gEngfuncs.Con_DPrintf("[GLEW] Initialize success!\n");
		//shaderUtil.LoadFromFile(std::string(gEngfuncs.pfnGetGameDirectory() + (std::string) "/shaders/vs.shaders"), std::string(gEngfuncs.pfnGetGameDirectory() + (std::string) "/shaders/fs.shaders"));
		shaderUtil.LoadFromString(vertShader, fragShader);
	}

	return;
}

void CPostProcessShader::ClearFrameBuffer()
{
	if (cl_postprocess->value <= 0 || cl_intensity->value <= 0)
		return;

	//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	//GLfloat clearColors[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	//GLint drawFboId;
	//GLint readFboId;
	//GLint FboId;
	//glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
	//glBindFramebuffer(GL_FRAMEBUFFER, drawFboId);
	//glClearBufferfv(GL_COLOR, 0, clearColors);

	//glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFboId);
	//glBindFramebuffer(GL_FRAMEBUFFER, readFboId);
	//glClearBufferfv(GL_COLOR, 0, clearColors);

	//glGetIntegerv(GL_FRAMEBUFFER_BINDING, &FboId);
	//glBindFramebuffer(GL_FRAMEBUFFER, FboId);
	//glClearBufferfv(GL_COLOR, 0, clearColors);
}

void CPostProcessShader::DrawShader()
{
	if (cl_postprocess->value <= 0 || cl_intensity->value <= 0)
		return;

	// TODO: Check NPOT texture support and shader support for older systems. HL might already check NPOT for FBO mode.

	const int error1 = glGetError();
	if (error1 != 0)
		gEngfuncs.Con_DPrintf("before shader error %d\n", error1);

	glPushAttrib(GL_TEXTURE_BIT);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	//// Might need some of these if things break.
	//glViewport(0, 0, ScreenWidth(), ScreenHeight());
	//glActiveTexture(GL_TEXTURE0);
	//glEnable(GL_TEXTURE_RECTANGE_NV);
	//glColor3f(1, 1, 1);
	//glDisable(GL_DEPTH_TEST);
	//glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
	//glLoadIdentity();
	//glMatrixMode(GL_PROJECTION);
	//glPushMatrix();
	//glLoadIdentity();
	//glOrtho(0, 1, 1, 0, 0.1, 100);
	//glReadBuffer(GL_COLOR_ATTACHMENT0);

	//glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, screenTexture);

	//// Solokiller's code to copy from MSAA/scaling framebuffer
	//glFinish(); // Significant performance hit with this and it seems to work without it.
	GLint drawFboId;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFboId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glBlitFramebuffer(0, 0, ScreenWidth(), ScreenHeight(), 0, 0, ScreenWidth(), ScreenHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);

	glReadBuffer(GL_BACK);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, ScreenWidth(), ScreenHeight(), 0);

	const int error2 = glGetError();
	if (error2 != 0)
		gEngfuncs.Con_DPrintf("framebuffer copy error %d\n", error2);


	float colorMultiplier = 1.0f;
	const float scalarCvar = min(2.0f, max( 0.0f, cl_intensity->value));
	//const float colorMultiplier = max( 1.0f, max(scalarCvar, 1.0f + (gHUD.GetGammaSlope() - 1.0f) * scalarCvar));

	// Scale the map's gamma value.
	if (gHUD.GetGammaSlope() > 1.0f)
	{
		colorMultiplier = 1.0f + (gHUD.GetGammaSlope() - 1.0f) * scalarCvar;
	}
	// If the map has no gamma value, override it with the cvar's value.
	else
	{
		colorMultiplier = max(1.0f, scalarCvar);
	}

	// Use shader.
	glUseProgram(shaderUtil.GetProgramID());
	//glUniform1f(glGetUniformLocation(shaderUtil.GetProgramID(), "textureMap"), 0);
	glUniform1f(glGetUniformLocation(shaderUtil.GetProgramID(), "colorMultiplier"), colorMultiplier);

	// Draw the quad.
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(-1, 1, -1);
	glTexCoord2f(0, 1);
	glVertex3f(-1, -1, -1);
	glTexCoord2f(1, 1);
	glVertex3f(1, -1, -1);
	glTexCoord2f(1, 0);
	glVertex3f(1, 1, -1);
	glEnd();

	// Restore state.
	glUseProgram(0);
	//glBindTexture(GL_TEXTURE_2D, 0);
	//glBindVertexArray(0);
	glPopAttrib();
	glPopClientAttrib();
	
	//glMatrixMode(GL_PROJECTION);
	//glPopMatrix();
	//glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();
	//glDisable(GL_TEXTURE_RECTANGE_NV); // Don't disable texture_2d or ui bugs out.
	//glEnable(GL_DEPTH_TEST);

	const int error3 = glGetError();
	if (error3 != 0)
		gEngfuncs.Con_DPrintf("shader error %d\n", error3);
}
