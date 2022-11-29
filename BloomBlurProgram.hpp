#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//Shader program that draws transformed, vertices tinted with vertex colors:
struct BloomBlurProgram {
	BloomBlurProgram();
	~BloomBlurProgram();

	GLuint program = 0;
	//Attribute (per-vertex variable) locations:
	GLuint Position_vec3 = -1U;
	GLuint TexCoords_vec2 = -1U;
	//Uniform (per-invocation variable) locations:
	GLuint HORIZONTAL_bool = -1U;
	//Textures:
	// none
};

extern Load< BloomBlurProgram > bloom_blur_program;

