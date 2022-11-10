#include "GL.hpp"
#include "glm/glm.hpp"

struct HUD {
	struct Sprite {
		int width, height, nrChannels;
		unsigned int textureID;
	};

	static Sprite *loadSprite(const char* path);
	static void drawElement(glm::vec2 tl, glm::vec2 tr, Sprite sprite);
	static void init();
	static GLuint buffer;
};
