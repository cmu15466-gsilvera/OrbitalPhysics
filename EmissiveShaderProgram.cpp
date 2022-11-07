#include "EmissiveShaderProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< EmissiveShaderProgram > background_program(LoadTagEarly);

Scene::Drawable::Pipeline emissive_program_pipeline;

Load< EmissiveShaderProgram > emissive_program(LoadTagEarly, []() -> EmissiveShaderProgram const * {
	EmissiveShaderProgram *ret = new EmissiveShaderProgram();

	//----- build the pipeline template -----
	emissive_program_pipeline.program = ret->program;

	emissive_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;

	return ret;
});


EmissiveShaderProgram::EmissiveShaderProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform vec4 Color;\n"
		"in vec4 Position;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	color = Color;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	COLOR_vec4 = glGetUniformLocation(program, "Color");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

EmissiveShaderProgram::~EmissiveShaderProgram() {
	glDeleteProgram(program);
	program = 0;
}

