#pragma once

#include "GL.hpp"
#include "Load.hpp"

//Shader program that draws transformed, colored vertices:
struct SkyboxProgram {
	SkyboxProgram();
	~SkyboxProgram();

	GLuint program = 0;
	//Attribute (per-vertex variable) locations:
	//Uniform (per-invocation variable) locations:
	GLuint PROJECTION_mat4 = -1U;
	GLuint VIEW_mat4 = -1U;
	//Textures:
	// none
};

extern Load< SkyboxProgram > skybox_program;
