#include "glm/glm.hpp"

struct HUD {
	struct Sprite {
		int width, height, nrChannels;
		unsigned int textureID;
	};

	Sprite loadSprite(const char* path);
	void drawElement(glm::vec2 tl, glm::vec2 tr, Sprite sprite);
};
