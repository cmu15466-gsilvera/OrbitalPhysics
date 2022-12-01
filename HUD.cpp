#include "HUD.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "ColorTextureProgram.hpp"
#include "GL.hpp"
#include "gl_errors.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION   // use of stb functions once and for all
#include "stb_image.h"

GLuint HUD::buffer;
GLuint HUD::vao;
bool HUD::initialized = false;

glm::uvec2 HUD::SCREEN_DIM = glm::vec2(0);
std::vector<HUD::Sprite*> HUD::sprites;

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

glm::vec2 HUD::fromAnchor(Anchor anchor, glm::vec2 offset){
    switch (anchor) {
        case TOPLEFT:
            return offset + glm::vec2(0, SCREEN_DIM.y);
        case TOPCENTER:
            return offset + glm::vec2(SCREEN_DIM.x / 2, SCREEN_DIM.y);
        case TOPRIGHT:
            return offset + glm::vec2(SCREEN_DIM.x, SCREEN_DIM.y);
        case CENTERRIGHT:
            return offset + glm::vec2(SCREEN_DIM.x, SCREEN_DIM.y / 2);
        case BOTTOMRIGHT:
            return offset + glm::vec2(SCREEN_DIM.x, 0);
        case BOTTOMCENTER:
            return offset + glm::vec2(SCREEN_DIM.x / 2, 0);
        case BOTTOMLEFT:
            return offset;
        case CENTERLEFT:
            return offset + glm::vec2(0, SCREEN_DIM.y / 2);
        default:
            return offset;
            break;
    }
}

void HUD::freeSprites(){
    for(auto it = sprites.begin(); it < sprites.end(); it++){
        free(*it);
    }
}

HUD::Sprite *HUD::loadSprite(std::string path){
	if(!HUD::initialized)
		init();

	HUD::Sprite *ret = new HUD::Sprite();
	unsigned char* data = stbi_load(path.c_str(), &ret->width, &ret->height, &ret->nrChannels, STBI_rgb_alpha);

	if(!data){
		std::cout << "Failed to load texture " << path << std::endl;
		return ret;
	}else{
		std::cout << "Loaded " << path << "with " << ret->nrChannels << std::endl;
	}

	glGenTextures(1, &ret->textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ret->textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ret->width, ret->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);

    sprites.push_back(ret);

	return ret;
}

void HUD::drawElement(glm::vec2 pos, HUD::Sprite *sprite, glm::u8vec4 const &color){
    drawElement(glm::vec2(static_cast<float>(sprite->width),static_cast<float>(sprite->height)), pos, sprite, color);
}

void HUD::drawElement(glm::vec2 size, glm::vec2 pos, HUD::Sprite *sprite, glm::u8vec4 const &color){
	glUseProgram(color_texture_program->program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sprite->textureID);
	glBindVertexArray(vao);

	glEnable(GL_BLEND);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(glm::ortho(0.0f, static_cast<float>(SCREEN_DIM.x), 0.f, static_cast<float>(SCREEN_DIM.y), 0.f, 1.0f)));
	glUniform4fv(color_texture_program->OFFSET_vec4, 1, glm::value_ptr(glm::vec4(size.x, -size.y, pos.x, pos.y)));
	glUniform4fv(color_texture_program->COLOR_vec4, 1, glm::value_ptr(glm::vec4(color) / 255.f));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glUseProgram(0);

	GL_ERRORS();
}
