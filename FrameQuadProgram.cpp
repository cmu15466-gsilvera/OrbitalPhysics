#include "FrameQuadProgram.hpp"

#include "GL.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< FrameQuadProgram > frame_quad_program(LoadTagEarly);

FrameQuadProgram::FrameQuadProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"layout (location = 0) in vec3 Position;\n"
		"layout (location = 1) in vec2 InTexCoords;\n"
		"out vec2 TexCoords;\n"
		"void main() {\n"
		"	TexCoords = InTexCoords;\n"
		"	gl_Position = vec4(Position, 1.0);\n"
		"}",
		//fragment shader:
		"#version 330\n"
		"in vec2 TexCoords;\n"
		"uniform sampler2D scene;\n"
		"uniform sampler2D bloomBlur;\n"
		"uniform bool bloom;\n"
		"uniform float exposure;\n"

		"out vec4 FragColor;\n"
		"void main() {\n"
		"	const float gamma = 1.2;\n"
		"	vec3 mainColor = texture(scene, TexCoords).rgb;\n"      
		"	vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;\n"
		"	if(bloom)\n"
		"		mainColor += bloomColor; // additive blending\n"
		"	FragColor = vec4(mainColor, 1.0);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec3 = glGetAttribLocation(program, "Position");
	TexCoords_vec2 = glGetAttribLocation(program, "InTexCoords");

	BLOOM_bool = glGetUniformLocation(program, "bloom");
	EXPOSURE_float = glGetUniformLocation(program, "exposure");

	HDR_tex = glGetUniformLocation(program, "scene");
	BLOOM_tex  = glGetUniformLocation(program, "bloomBlur");
}

FrameQuadProgram::~FrameQuadProgram() {
	glDeleteProgram(program);
	program = 0;
}


