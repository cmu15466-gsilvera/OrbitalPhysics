#pragma once

#include "GL.hpp"
#include "glm/glm.hpp"
#include <string>
#include <vector>

struct HUD {
	struct Sprite {
		int width, height, nrChannels;
		unsigned int textureID;
	};

    enum Anchor {
        TOPLEFT,
        TOPCENTER,
        TOPRIGHT,
        CENTERRIGHT,
        BOTTOMRIGHT,
        BOTTOMCENTER,
        BOTTOMLEFT,
        CENTERLEFT
    };



	public:
        static glm::uvec2 SCREEN_DIM;
		static Sprite *loadSprite(std::string path);
		static void drawElement(glm::vec2 size, glm::vec2 pos, Sprite *sprite, glm::u8vec4 const &color = glm::u8vec4{0xff});
		static void drawElement(glm::vec2 pos, Sprite *sprite, glm::u8vec4 const &color = glm::u8vec4{0xff});
		static void init();
        static glm::vec2 fromAnchor(Anchor anchor, glm::vec2 offset);
        static void freeSprites();

	private:
        static std::vector<Sprite*> sprites;
		static GLuint buffer;
		static GLuint vao;
		static bool initialized;
};
