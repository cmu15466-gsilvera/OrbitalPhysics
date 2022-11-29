#pragma once

#include "GL.hpp"
#include "Load.hpp"

//Shader program that draws transformed, colored vertices:
struct FrameQuadProgram {
	FrameQuadProgram();
	~FrameQuadProgram();

	GLuint program = 0;
	//Attribute (per-vertex variable) locations:
	GLuint Position_vec3 = -1U;
	GLuint TexCoords_vec2 = -1U;
	//Uniform (per-invocation variable) locations:
	GLuint BLOOM_bool = -1U;
	GLuint EXPOSURE_float = -1U;
	//Textures:
	GLuint BLOOM_tex = -1U;
	GLuint HDR_tex = -1U;
};

extern Load< FrameQuadProgram > frame_quad_program;

