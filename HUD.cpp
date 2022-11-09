#include "HUD.hpp"
#include "GL.hpp"
#include "stb_image.h"
#include <iostream>

HUD::Sprite HUD::loadSprite(const char *path){
	HUD::Sprite ret;
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
	float vertices[] = {

	};
}
