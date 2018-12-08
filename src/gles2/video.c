/*
 * video.c - Raspberry Pi suport by djdron
 *
 * Copyright (c) 2013 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * This file is based on Portable ZX-Spectrum emulator.
 * Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
 *
 * C++ to C code conversion by Pate
 *
 * Atari800 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GLES2/gl2.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "screen.h"
#include "colours.h"

int op_filtering = 1;
float op_zoom = 1.0f;

#define	SHOW_ERROR		/*gles_show_error();*/

static const char* vertex_shader =
	"uniform mat4 u_vp_matrix;								\n"
	"attribute vec4 a_position;								\n"
	"attribute vec2 a_texcoord;								\n"
	"varying mediump vec2 v_texcoord;						\n"
	"void main()											\n"
	"{														\n"
	"	v_texcoord = a_texcoord;							\n"
	"	gl_Position = u_vp_matrix * a_position;				\n"
	"}														\n";

static const char* fragment_shader =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"uniform sampler2D u_palette;													\n"
	"void main()																	\n"
	"{																				\n"
	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" // use paletted texture
	"	vec4 c0 = texture2D(u_palette, vec2((255.0/256.0)*p0.r + 1.0/256.0*0.1, 0.5)); 		\n"
	"	gl_FragColor = c0;											 				\n"
	"}																				\n";

static const char* fragment_shader_filtering =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"uniform sampler2D u_palette;													\n"
	"void main()																	\n"
	"{																				\n"
	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" // manually linear filtering of paletted texture is awful
	"	vec4 p1 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 0)); 			\n"
	"	vec4 p2 = texture2D(u_texture, v_texcoord + vec2(0, 1.0/256.0)); 			\n"
	"	vec4 p3 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 1.0/256.0)); 	\n"
	"	vec4 c0 = texture2D(u_palette, vec2((255.0/256.0)*p0.r + 1.0/256.0*0.1, 0.5)); 			\n"
	"	vec4 c1 = texture2D(u_palette, vec2((255.0/256.0)*p1.r + 1.0/256.0*0.1, 0.5)); 			\n"
	"	vec4 c2 = texture2D(u_palette, vec2((255.0/256.0)*p2.r + 1.0/256.0*0.1, 0.5)); 			\n"
	"	vec4 c3 = texture2D(u_palette, vec2((255.0/256.0)*p3.r + 1.0/256.0*0.1, 0.5)); 			\n"
	"	vec2 l = vec2(fract(512.0*v_texcoord.x), fract(256.0*v_texcoord.y)); 		\n"
	"	gl_FragColor = mix(mix(c0, c1, l.x), mix(c2, c3, l.x), l.y); 				\n"
	"}																				\n";

static const GLfloat vertices[] =
{
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

#define	TEX_WIDTH	512
#define	TEX_HEIGHT	256
#define	WIDTH		Screen_WIDTH
#define	HEIGHT		Screen_HEIGHT

#define	min_u		24.0f/TEX_WIDTH
#define	max_u		(float)WIDTH/TEX_WIDTH - min_u
#define	min_v		0.0f
#define	max_v		(float)HEIGHT/TEX_HEIGHT

static const GLfloat uvs[] =
{
	min_u, min_v,
	max_u, min_v,
	max_u, max_v,
	min_u, max_v,
};

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

void gles_show_error()
{
	GLenum error = GL_NO_ERROR;
    error = glGetError();
    if (GL_NO_ERROR != error)
        printf("GL Error %x encountered!\n", error);
}

static GLuint CreateShader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);
	if(!shader)
		return 0;

	// Load and compile the shader source
	glShaderSource(shader, 1, &shader_src, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error compiling shader:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint CreateProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
	GLuint vertex_shader = CreateShader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
		return 0;
	GLuint fragment_shader = CreateShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program_object = glCreateProgram();
	if(!program_object)
		return 0;
	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	// Link the program
	glLinkProgram(program_object);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
	if(!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(info_len);
			glGetProgramInfoLog(program_object, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error linking program:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteProgram(program_object);
		return 0;
	}
	// Delete these here because they are attached to the program object.
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program_object;
}

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
		GLint u_palette;
} ShaderInfo;

static ShaderInfo shader;
static ShaderInfo shader_filtering;
static GLuint buffers[3];
static GLuint textures[2];
static char palette_changed;

void gles2_create()
{
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = CreateProgram(vertex_shader, fragment_shader);
	if(shader.program)
	{
		shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
		shader.u_palette	= glGetUniformLocation(shader.program,	"u_palette");
	}
	memset(&shader_filtering, 0, sizeof(ShaderInfo));
	shader_filtering.program = CreateProgram(vertex_shader, fragment_shader_filtering);
	if(shader_filtering.program)
	{
		shader_filtering.a_position	= glGetAttribLocation(shader_filtering.program,	"a_position");
		shader_filtering.a_texcoord	= glGetAttribLocation(shader_filtering.program,	"a_texcoord");
		shader_filtering.u_vp_matrix	= glGetUniformLocation(shader_filtering.program,	"u_vp_matrix");
		shader_filtering.u_texture	= glGetUniformLocation(shader_filtering.program,	"u_texture");
		shader_filtering.u_palette	= glGetUniformLocation(shader_filtering.program,	"u_palette");
	}
	palette_changed = 1;
	if(!shader.program || !shader_filtering.program)
		return;

	glGenTextures(2, textures);	SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEX_WIDTH, TEX_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[1]); SHOW_ERROR	// color palette
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL); SHOW_ERROR

	glGenBuffers(3, buffers); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); SHOW_ERROR
	glDisable(GL_DEPTH_TEST); SHOW_ERROR
	glDisable(GL_DITHER); SHOW_ERROR
}

void gles2_destroy()
{
	if(shader.program)
		glDeleteProgram(shader.program);
	if(shader_filtering.program)
		glDeleteProgram(shader_filtering.program);
	if(!shader.program || !shader_filtering.program)
		return;
	glDeleteBuffers(3, buffers); SHOW_ERROR
	glDeleteTextures(2, textures); SHOW_ERROR
}

static void SetOrtho(float m[4][4], float left, float right, float bottom, float top, float near, float far, float scale_x, float scale_y)
{
	memset(m, 0, 4*4*sizeof(float));
	m[0][0] = 2.0f/(right - left)*scale_x;
	m[1][1] = 2.0f/(top - bottom)*scale_y;
	m[2][2] = -2.0f/(far - near);
	m[3][0] = -(right + left)/(right - left);
	m[3][1] = -(top + bottom)/(top - bottom);
	m[3][2] = -(far + near)/(far - near);
	m[3][3] = 1;
}

static void gles2_DrawQuad(const ShaderInfo *sh, GLuint textures[2])
{
	glUniform1i(sh->u_texture, 0); SHOW_ERROR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR //@note : GL_LINEAR must be implemented in shader because of palette indexes in texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR

	glActiveTexture(GL_TEXTURE1); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[1]); SHOW_ERROR
	glUniform1i(sh->u_palette, 1); SHOW_ERROR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_position); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_texcoord); SHOW_ERROR

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0); SHOW_ERROR
}

inline unsigned short BGR565(unsigned char r, unsigned char g, unsigned char b) { return ((r&~7) << 8)|((g&~3) << 3)|(b >> 3); }

void gles2_draw(int _w, int _h)
{
	if(!shader.program || !shader_filtering.program)
		return;

	float sx = 1.0f, sy = 1.0f;

	// Screen aspect ratio adjustment
	float a = (float)_w/_h;
	float a0 = 336.0f/240.0f;
	if(a > a0)
		sx = a0/a;
	else
		sy = a/a0;

	glClear(GL_COLOR_BUFFER_BIT); SHOW_ERROR
	glViewport(0, 0, _w, _h); SHOW_ERROR

	ShaderInfo* sh = op_filtering ? &shader_filtering : &shader;

	float proj[4][4];
	glDisable(GL_BLEND); SHOW_ERROR
	glUseProgram(sh->program); SHOW_ERROR
	SetOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f, sx*op_zoom, sy*op_zoom);
	glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, &proj[0][0]); SHOW_ERROR

	if(palette_changed)
	{
		int i;
		palette_changed = 0;
		unsigned short palette[256];
		for(i = 0; i < 256; ++i)
		{
			palette[i] = BGR565(Colours_GetR(i), Colours_GetG(i), Colours_GetB(i));
		}
		glActiveTexture(GL_TEXTURE1); SHOW_ERROR
	    glBindTexture(GL_TEXTURE_2D, textures[1]); SHOW_ERROR
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, palette); SHOW_ERROR
	}

	glActiveTexture(GL_TEXTURE0); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, textures[0]); SHOW_ERROR
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, Screen_atari); SHOW_ERROR
	gles2_DrawQuad(sh, textures);

	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR
//	glFlush();
}

void gles2_palette_changed()
{
	palette_changed = 1;
}
