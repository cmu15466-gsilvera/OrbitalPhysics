#pragma once

#include "GL.hpp"
#include "Text.hpp"
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

    struct ButtonSprite
	{
		ButtonSprite(ButtonSprite const &) = delete;
		~ButtonSprite()
		{
			if (sprite != nullptr)
				delete sprite;
			if (text != nullptr)
				delete text;
		}
		ButtonSprite(std::string const &sprite_path, glm::u8vec4 const &c0, glm::u8vec4 const &c1, glm::vec2 const &s0,
					 glm::vec2 const &s1, glm::vec2 const &l, std::string const &str = "")
			: color(c0), color_hover(c1), size(s0), size_hover(s1), loc(l)
		{
			sprite = HUD::loadSprite(sprite_path);
			text = new Text();
			text->init(Text::AnchorType::CENTER);
			text->set_static_text(str); // for now, buttons will always have static text
		}
		glm::u8vec4 color;
		glm::u8vec4 color_hover;
		/// NOTE: location/size is relative to CENTER of sprite
		glm::vec2 size;
		glm::vec2 size_hover;
		glm::vec2 loc;
		Text *text = nullptr;
		HUD::Sprite *sprite = nullptr;
		bool bIsHovered = false;
		bool is_hovered(glm::vec2 const &);
		void draw(glm::vec2 const &) const;
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
