#include "ColorTextureProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Scene::Drawable::Pipeline color_texture_program_pipeline;

Load< ColorTextureProgram > color_texture_program(LoadTagEarly, []() -> ColorTextureProgram const * {
	ColorTextureProgram *ret = new ColorTextureProgram();

	//----- build the pipeline template -----
	color_texture_program_pipeline.program = ret->program;

	color_texture_program_pipeline.OBJECT_TO_CLIP_mat4 = ret->OBJECT_TO_CLIP_mat4;

	return ret;
});


ColorTextureProgram::ColorTextureProgram() {
	//Compile vertex and fragment shaders using the convenient 'gl_compile_program' helper function:
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"uniform vec4 OFFSET;\n"
		"in vec4 Position;\n"
		"in vec2 TexCoord;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"   vec4 newPosition = Position;\n"
		"   newPosition.x *= OFFSET.x;\n"
		"   newPosition.y *= OFFSET.y;\n"
		"   newPosition.x += OFFSET.z;\n"
		"   newPosition.y += OFFSET.w;\n"
		"	gl_Position = OBJECT_TO_CLIP * newPosition;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"uniform vec4 COLOR;\n"
		"in vec2 texCoord;\n"
		"layout (location = 0) out vec4 fragColor;\n"
		"layout (location = 1) out vec4 brightColor;\n"
		"void main() {\n"
		"	fragColor = texture(TEX, texCoord) * COLOR;\n"
		"	brightColor = vec4(0, 0, 0, 1.0);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(program, "OBJECT_TO_CLIP");
	OFFSET_vec4 = glGetUniformLocation(program, "OFFSET");
	COLOR_vec4 = glGetUniformLocation(program, "COLOR");
	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

ColorTextureProgram::~ColorTextureProgram() {
	glDeleteProgram(program);
	program = 0;
}

