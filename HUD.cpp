#include "HUD.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "ColorTextureProgram.hpp"
#include "GL.hpp"
#include "gl_errors.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION   // use of stb functions once and for all
#include "stb_image.h"

GLuint HUD::buffer;
GLuint HUD::vao;
bool HUD::initialized = false;

void HUD::init(){
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &buffer);

	const float vertexData[] = {
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
	};

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)96);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	initialized = true;
	
	GL_ERRORS();
}

HUD::Sprite *HUD::loadSprite(std::string path){
	if(!HUD::initialized)
		init();

	HUD::Sprite *ret = new HUD::Sprite();
	unsigned char* data = stbi_load(path.c_str(), &ret->width, &ret->height, &ret->nrChannels, STBI_rgb_alpha);

	if(!data){
		std::cout << "Failed to load texture " << path << std::endl;
		return ret;
	}

	glGenTextures(1, &ret->textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ret->textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ret->width, ret->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);

	return ret;
}

void HUD::drawElement(glm::vec2 size, glm::vec2 pos, HUD::Sprite *sprite, float width, float height){
	glUseProgram(color_texture_program->program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sprite->textureID);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(glm::ortho(0.0f, width, 0.f, height, 0.f, 1.0f)));
	glUniform4fv(color_texture_program->OFFSET_vec4, 1, glm::value_ptr(glm::vec4(size.x, -size.y, pos.x, pos.y)));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glUseProgram(0);

	GL_ERRORS();
}
