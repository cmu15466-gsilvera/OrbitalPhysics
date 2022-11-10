#include "HUD.hpp"
#include "ColorTextureProgram.hpp"
#include "stb_image.h"
#include <iostream>

void HUD::init(){
	glGenBuffers(1, &buffer);

	const float vertexData[] = {
		-1.0f, 1.0f, 0.0f, 1.0f,
		-1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f, 
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)96);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

HUD::Sprite *HUD::loadSprite(const char *path){
	HUD::Sprite *ret = new HUD::Sprite();
	unsigned char* data = stbi_load(path, &ret.width, &ret.height, &ret.nrChannels, 0);

	if(!data){
		std::cout << "Failed to load texture " << path << std::endl;
		return ret;
	}

	glGenTextures(1, &ret.textureID);

	glBindTexture(GL_TEXTURE_2D, ret.textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ret.width, ret.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);

	return ret;
}

void HUD::drawElement(glm::vec2 bl, glm::vec2 tr, HUD::Sprite sprite){
	glUseProgram(color_texture_program->program);

	glBindTexture(GL_TEXTURE_2D, sprite.textureID);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}
