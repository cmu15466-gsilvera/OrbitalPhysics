#include "BloomBlurProgram.hpp"

#include "GL.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< BloomBlurProgram > bloom_blur_program(LoadTagEarly);

BloomBlurProgram::BloomBlurProgram() {
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
		"uniform sampler2D image;\n"
		"uniform bool horizontal;\n"
		"uniform float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);\n"

		"out vec4 FragColor;\n"
		"void main() {\n"
		"	vec2 tex_offset = 1.0 / textureSize(image, 0); // gets size of single texel\n"
			"vec3 result = texture(image, TexCoords).rgb * weight[0];\n"
			"if(horizontal){\n"
			"   for(int i = 1; i < 5; ++i)\n"
			"   {\n"
			"      result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];\n"
			"      result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];\n"
			"   }\n"
			"}\n"
			"else\n"
			"{\n"
			"    for(int i = 1; i < 5; ++i)\n"
			"    {\n"
			"        result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];\n"
			"        result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];\n"
			"    }\n"
			"}\n"
			"FragColor = vec4(result, 1.0);\n"
		"}\n"
	);
	//As you can see above, adjacent strings in C/C++ are concatenated.
	// this is very useful for writing long shader programs inline.

	//look up the locations of vertex attributes:
	Position_vec3 = glGetAttribLocation(program, "Position");
	TexCoords_vec2 = glGetAttribLocation(program, "InTexCoords");
	HORIZONTAL_bool = glGetUniformLocation(program, "horizontal");

}

BloomBlurProgram::~BloomBlurProgram() {
	glDeleteProgram(program);
	program = 0;
}


