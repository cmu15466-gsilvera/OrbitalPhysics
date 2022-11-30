#include "SkyboxProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< SkyboxProgram > skybox_program(LoadTagEarly);

SkyboxProgram::SkyboxProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"layout (location = 0) in vec3 aPos;\n"
		"out vec3 TexCoords;\n"
		"uniform mat4 projection;\n"
		"uniform mat4 view;\n"
		"void main() {\n"
		"	TexCoords = aPos;\n"
		"	vec4 pos = projection * view * vec4(aPos, 1.0);\n"
		"	gl_Position = pos.xyww;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"out vec4 FragColor;\n"
		"in vec3 TexCoords;\n"
		"uniform samplerCube skybox;\n"
		"void main() {\n"
		"	FragColor = texture(skybox, TexCoords);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:

	//look up the locations of uniforms:
	PROJECTION_mat4 = glGetUniformLocation(program, "projection");
	VIEW_mat4 = glGetUniformLocation(program, "view");
}

SkyboxProgram::~SkyboxProgram() {
	glDeleteProgram(program);
	program = 0;
}

