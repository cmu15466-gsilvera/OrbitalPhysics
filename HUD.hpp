#pragma once

#include "GL.hpp"
#include "glm/glm.hpp"
#include <string>

struct HUD {
	struct Sprite {
		int width, height, nrChannels;
		unsigned int textureID;
	};

	public:
		static Sprite *loadSprite(std::string path);
		static void drawElement(glm::vec2 size, glm::vec2 pos, Sprite *sprite, float width, float height);
		static void init();

	private:
		static GLuint buffer;
		static GLuint vao;
		static bool initialized;
};
