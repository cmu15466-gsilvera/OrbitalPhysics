#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "data_path.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <iostream>
#include <map>
#include <string>

// freetype
#include <ft2build.h>
#include FT_FREETYPE_H

// harfbuzz
#include <hb-ft.h>
#include <hb.h>

// referenced the following examples/documentation for inspiration
// https://learnopengl.com/In-Practice/Text-Rendering
// https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c

struct Character {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2 Size; // Size of glyph (glyph width, rows)
    glm::ivec2 Bearing; // Offset from baseline to left/top of glyph (glyph left,top)
    unsigned int Advance; // Offset to advance to next glyph
    GLvoid *data = nullptr;

    // construct a character from a char request and typeface (glyph) collection
    static Character Load(hb_codepoint_t request, FT_Face typeface)
    {
        // taken almost verbatim from https://learnopengl.com/In-Practice/Text-Rendering
        if (FT_Load_Glyph(typeface, request, FT_LOAD_RENDER))
            throw std::runtime_error("Failed to load glyph: " + std::to_string(request));
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            typeface->glyph->bitmap.width,
            typeface->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            typeface->glyph->bitmap.buffer);
        // set tex options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // not using mipmap
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // not using mipmap
        // also store the bitmap buffer
        size_t size = typeface->glyph->bitmap.width * typeface->glyph->bitmap.rows * sizeof(char);
        GLvoid *data = malloc(size);
        memcpy(data, typeface->glyph->bitmap.buffer, size);
        // now store character for later use
        return {
            tex,
            glm::ivec2(typeface->glyph->bitmap.width, typeface->glyph->bitmap.rows),
            glm::ivec2(typeface->glyph->bitmap_left, typeface->glyph->bitmap_top),
            (unsigned int)(typeface->glyph->advance.x), // since using ascii, advance.y => 0 anyways
            data,
        };
    }
};

struct Text {
    const std::string text_file = "assets/fonts/BungeeSpice-Regular.ttf";
    const std::string monotext_file = "assets/fonts/RobotoMono-Bold.ttf";
    float font_size = 64.f;
    float font_scale = 64.f; // number of units per pixel

    const float anim_time = 1.0f; // time (seconds) to complete animation
    float time = 0.f; // current time for new text

    // freetype typeface
    FT_Face typeface;

    // harfbuzz for shaping the text
    hb_buffer_t* hb_buffer = nullptr;
    hb_font_t* hb_typeface = nullptr;

    // for drawing
    inline static GLuint draw_text_program = 0;
    inline static GLuint VAO = 0;
    inline static GLuint VBO = 0;

    inline static GLuint VBO_deferred = 0;

    // text anchor
    enum AnchorType : uint8_t {
        LEFT=0,
        RIGHT,
        CENTER,
    };

    AnchorType anchor = AnchorType::LEFT;

    // for actual text content
    std::string text_content;

    // using hb_codepoint_t as the codepoint type from hb_buffer_get_glyph_positions
    std::map<hb_codepoint_t, Character> chars;


    struct RenderCharacter {
        RenderCharacter(const Character &c, glm::vec3 const &col, const float x, const float y, const float _w, const float _h) :
           ch(c), color(col), xpos(x), ypos(y), w(_w), h(_h) {};
        const Character ch;
        const glm::vec3 color;
        const float xpos, ypos;
        const float w, h;
        float tx = 0.f; // index into the larger atlas texture if available
    };
    inline static std::vector<RenderCharacter> render_chars;

    struct Atlas {
        
    };


    void init(AnchorType _anchor) {
        init(_anchor, false);
    }

    void init(AnchorType _anchor, bool mono)
    {
        anchor = _anchor;
        // (try to) load freetype library and typeface
        {
            std::string const &file = mono ? monotext_file : text_file;
            FT_Library ftlibrary;
            if (FT_Init_FreeType(&ftlibrary))
                throw std::runtime_error("Failed to initialize FreeType library");
            if (FT_New_Face(ftlibrary, const_cast<char*>(data_path(file).c_str()), 0, &typeface))
                throw std::runtime_error("Failed to load font from typeface \"" + file + "\"");
        }

        // create shaders for rendering
        if (Text::draw_text_program == 0) {
            // https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text.vs
            const auto vertex_shader = "#version 330 core\n"
                                       "in vec4 coord;\n"
                                       "out vec2 texpos;\n"
                                       "uniform mat4 projection;\n"
                                       "void main()\n"
                                       "{\n"
                                       "    gl_Position = projection * vec4(coord.xy, 0.0, 1.0);\n"
                                       "    texpos = coord.zw;\n"
                                       "}\n";

            // https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text.fs
            const auto fragment_shader = "#version 330 core\n"
                                         "in vec2 texpos;\n"
                                         "layout (location = 0) out vec4 color;\n"
                                         "layout (location = 1) out vec4 bright;\n"
                                         "uniform sampler2D tex;\n"
                                         "uniform vec3 textColor;\n"
                                         "void main()\n"
                                         "{\n"
                                         "	 vec4 sampled = vec4(1.0, 1.0, 1.0, texture(tex, texpos).r);\n"
                                         "   color = vec4(textColor, 1.0) * sampled;\n"
                                         "   bright = vec4(vec3(0.0), 1.0);\n"
                                         "}\n";
            Text::draw_text_program = gl_compile_program(vertex_shader, fragment_shader);
        }

        // set initial harfbuzz buffer params/data
        {
            set_font_size(font_size, font_scale, true); // make sure to set the font size once upon initialization
            hb_typeface = hb_ft_font_create(typeface, nullptr);
            set_text("Initial text"); /// TODO: remove
        }

        // initialize openGL for rendering
        if (Text::VAO == 0 || Text::VBO == 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glGenVertexArrays(1, &Text::VAO);
            glGenBuffers(1, &Text::VBO);
            glBindVertexArray(Text::VAO);
            glBindBuffer(GL_ARRAY_BUFFER, Text::VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            GL_ERRORS();
        }
    }

    void update_buffer(const std::string& new_text)
    {
        /// TODO add more public setters
        if (hb_buffer != nullptr) {
            hb_buffer_destroy(hb_buffer);
        }
        hb_buffer = hb_buffer_create();
        hb_buffer_add_utf8(hb_buffer, new_text.c_str(), -1, 0, -1);
        hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR);
        hb_buffer_set_script(hb_buffer, HB_SCRIPT_LATIN);
        hb_buffer_set_language(hb_buffer, hb_language_from_string("en", -1));
        if (hb_typeface == nullptr) {
            throw std::runtime_error("Harfbuzz typeface is null!");
        }
        hb_shape(hb_typeface, hb_buffer, NULL, 0);
    }

    void set_text(const std::string& new_text)
    {
        text_content = new_text;
        update_buffer(text_content);
    }

    void reset_time()
    {
        time = 0.f;
    }

    void highlight()
    {
        // show some effect for highlighting
        update_buffer("**" + text_content + "**");
    }

    void set_font_size(float new_font_size, float new_font_scale, bool override = false)
    {
        if ((font_size != new_font_size || font_scale != new_font_scale) || override) {
            font_size = new_font_size;
            font_scale = new_font_scale;
            // std::cout << "setting font size to " << font_size << " and scale to " << font_scale << std::endl;
            FT_Set_Char_Size(typeface, 0, static_cast< unsigned int>(font_size * font_scale), 0, 0); // 64 units per pixel
            if (FT_Load_Char(typeface, 'X', FT_LOAD_RENDER))
                throw std::runtime_error("Failed to load char \"X\" from typeface!");
            // reset these characters to regenerate them with the new font size
            chars.clear();
        }
    }

    hb_glyph_info_t* prepare_draw(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, float ss_scale, glm::vec3 const &color, unsigned int &num_chars, float &amnt, float &char_x, float &char_y, float &anchor_x_start) {
        float new_font_scale = scale; // scale font size off window height
        set_font_size(font_size, new_font_scale);

        num_chars = 0;
        hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buffer, &num_chars);
        if (glyph_info == nullptr) {
            throw std::runtime_error("Unable to get glyph info from buffer!");
        }

        // calculate the final width of the text glyphs
        std::vector<float> line_widths;
        line_widths.push_back(0.f);
        for (unsigned int i = 0; i < num_chars; i++) {
            hb_codepoint_t char_req = glyph_info[i].codepoint;
            if (chars.find(char_req) == chars.end()) { // not already loaded
                Character ch = Character::Load(char_req, typeface);
                chars.insert(std::pair<hb_codepoint_t, Character>(char_req, ch));
            }

            const Character& ch = chars[char_req];

            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            if (char_req == hb_codepoint_t{'\0'}) {
                line_widths.push_back(0.f);
            }
            else {
                line_widths[line_widths.size() - 1] += (ch.Advance >> 6) * ss_scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
        }
        float final_width = 0.f;
        for (float line_width : line_widths) {
            final_width = std::max(line_width, final_width);
        }

        anchor_x_start = pos.x; // default (left)
        if (anchor == Text::AnchorType::CENTER) {
            anchor_x_start -= final_width / 2.f; // to be horizontally centered
        }
        else if (anchor == Text::AnchorType::RIGHT) {
            anchor_x_start -= final_width; // all the way to the right
        }

        char_x = anchor_x_start;
        char_y = pos.y;

        // handle animation (only draw fraction of total depending on time)
        time += dt;
        amnt = std::min(time / (anim_time * line_widths.size()), 1.f);

        return glyph_info;
    }

    void draw(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, float ss_scale, glm::vec3 const &color) {
        // draw a text element at once the draw_all() call is made, deferred can improve performance

        unsigned int num_chars;
        float amnt, char_x, char_y, anchor_x_start;
        hb_glyph_info_t *glyph_info = prepare_draw(dt, drawable_size, scale, pos, ss_scale, color, num_chars, amnt, char_x, char_y, anchor_x_start);

        // this loop was taken almost verbatim from https://learnopengl.com/In-Practice/Text-Rendering
        for (unsigned int i = 0; i < static_cast< unsigned int >(amnt * num_chars); i++) {
            hb_codepoint_t char_req = glyph_info[i].codepoint;
            if (chars.find(char_req) == chars.end()) { // not already loaded
                Character ch = Character::Load(char_req, typeface);
                chars.insert(std::pair<hb_codepoint_t, Character>(char_req, ch));
            }

            const Character& ch = chars[char_req];
            if (char_req != hb_codepoint_t{'\0'}){
                float xpos = char_x + ch.Bearing.x * ss_scale;
                float ypos = -char_y - (ch.Size.y - ch.Bearing.y) * ss_scale;
                float w = ch.Size.x * ss_scale;
                float h = ch.Size.y * ss_scale;

                render_chars.emplace_back(ch, color, xpos, ypos, w, h);
                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                char_x += (ch.Advance >> 6) * ss_scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
            // / TODO: logic for newline? (reset x, increase y?)
            else {
                char_x = anchor_x_start; // reset x
                char_y -= ch.Size.y; // increment Y
            }
        }
    }


    void draw_immediate(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, float ss_scale, glm::vec3 const &color)
    {
        return;
        // draw a text element immediately! (this directly talks to the GPU)

        // drawable_size - window size
        // scale - how wide the displayed string gets to be
        // pos - position in screenspace where the text gets rendered
        // ss_scale - screenspace scale (lower quality but cheap)
        // color - rgb(1) colour

        unsigned int num_chars;
        float amnt, char_x, char_y, anchor_x_start;
        hb_glyph_info_t *glyph_info = prepare_draw(dt, drawable_size, scale, pos, ss_scale, color, num_chars, amnt, char_x, char_y, anchor_x_start);

        glUseProgram(Text::draw_text_program);

        auto projection = glm::ortho(0.0f, drawable_size.x, 0.0f, drawable_size.y);
        glUniformMatrix4fv(glGetUniformLocation(Text::draw_text_program, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniform3f(glGetUniformLocation(Text::draw_text_program, "textColor"), color.x, color.y, color.z);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(Text::VAO);

        // this loop was taken almost verbatim from https://learnopengl.com/In-Practice/Text-Rendering
        for (unsigned int i = 0; i < static_cast< unsigned int >(amnt * num_chars); i++) {
            hb_codepoint_t char_req = glyph_info[i].codepoint;
            if (chars.find(char_req) == chars.end()) { // not already loaded
                Character ch = Character::Load(char_req, typeface);
                chars.insert(std::pair<hb_codepoint_t, Character>(char_req, ch));
            }

            const Character& ch = chars[char_req];
            if (char_req != hb_codepoint_t{'\0'}) {
                float xpos = char_x + ch.Bearing.x * ss_scale;
                float ypos = char_y - (ch.Size.y - ch.Bearing.y) * ss_scale;
                float w = ch.Size.x * ss_scale;
                float h = ch.Size.y * ss_scale;

                // update VBO for each character
                float vertices[6][4] = {
                    { xpos, ypos + h, 0.0f, 0.0f },
                    { xpos, ypos, 0.0f, 1.0f },
                    { xpos + w, ypos, 1.0f, 1.0f },

                    { xpos, ypos + h, 0.0f, 0.0f },
                    { xpos + w, ypos, 1.0f, 1.0f },
                    { xpos + w, ypos + h, 1.0f, 0.0f }
                };

                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                glBindBuffer(GL_ARRAY_BUFFER, Text::VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                char_x += (ch.Advance >> 6) * ss_scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
            // / TODO: logic for newline? (reset x, increase y?)
            else {
                char_x = anchor_x_start; // reset x
                char_y -= ch.Size.y; // increment Y
            }
        }

        // reset openGL stuff
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        GL_ERRORS();
    }

    static void draw_all(const glm::vec2 &drawable_size) {
        // attempting to "glom" all relevant rextures (sizes, shapes, colors) to a single texture atlas
        // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02
        glUseProgram(Text::draw_text_program);

        // find the size of the atlas texture
        int atlas_width = 0; // width of all glyphs together
        int atlas_height = 0; // height of single tallest glyph
        for (const auto &rc : render_chars) {
            atlas_width += rc.ch.Size.x;
            atlas_height = std::max(atlas_height, rc.ch.Size.y);
        }
        // std::cout << "generated atlas with dims: " << atlas_width << " x " << atlas_height << std::endl;

        auto tex = glGetUniformLocation(Text::draw_text_program, "tex");
        auto attribute_coord = glGetAttribLocation(Text::draw_text_program, "coord");

        // create empty texture for atlas
        GLuint atlas_tex;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &atlas_tex);
        glBindTexture(GL_TEXTURE_2D, atlas_tex);
        glUniform1i(tex, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_width, atlas_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        GL_ERRORS();

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // 1 byte alignment
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // clamping to avoid artifacts
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // clamping to avoid artifacts
        GL_ERRORS();

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // linear filtering usually best for text
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // linear filtering usually best for text
        GL_ERRORS();


        // fill in the texture with all the glyphs
        size_t x = 0;
        for(auto &rc : render_chars) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, rc.ch.Size.x, rc.ch.Size.y, GL_RED, GL_UNSIGNED_BYTE, rc.ch.data);
            rc.tx = static_cast<float>(x) / atlas_width;
            x += rc.ch.Size.x;
        }
        GL_ERRORS();

        auto projection = glm::ortho(0.0f, drawable_size.x, 0.0f, drawable_size.y);
        glUniformMatrix4fv(glGetUniformLocation(Text::draw_text_program, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniform3f(glGetUniformLocation(Text::draw_text_program, "textColor"), 1.f, 1.f, 1.f);
        GL_ERRORS();

        // use the texture containing the atlas
        glBindTexture(GL_TEXTURE_2D, atlas_tex);
        glUniform1i(tex, 0);
        GL_ERRORS();

        // set up the VBO for vertex data
        if (Text::VBO_deferred == 0) {
            glGenBuffers(1, &Text::VBO_deferred);
        }
        glBindVertexArray(Text::VAO); // since glEnableVertexAttribArray sets state in the current VAO
        glEnableVertexAttribArray(attribute_coord);
        glBindBuffer(GL_ARRAY_BUFFER, Text::VBO_deferred);
        glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
        GL_ERRORS();

        // loop through all characters
        std::vector<float> render_data; // should be 6 * 4 * render_chars.size()
        for (size_t i = 0; i < render_chars.size(); i++) {
            const RenderCharacter &rc = render_chars[i];

            float bw = rc.w;
            float bh = rc.h;

            if (!bw || !bh) // glyphs with no pixels
                continue;

            // each 4 tuple is a {x, y, s, t} to index into the vertex/texture coordinates/attribute
            render_data.insert(render_data.end(), {
                rc.xpos,        -rc.ypos - rc.h, rc.tx,                    bh / atlas_height,
                rc.xpos,        -rc.ypos,        rc.tx,                    0.0f,
                rc.xpos + rc.w, -rc.ypos,        rc.tx + bw / atlas_width, 0.0f,
                rc.xpos,        -rc.ypos - rc.h, rc.tx,                    bh / atlas_height,
                rc.xpos + rc.w, -rc.ypos,        rc.tx + bw / atlas_width, 0.0f,
                rc.xpos + rc.w, -rc.ypos - rc.h, rc.tx + bw / atlas_width, bh / atlas_height
            });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * render_data.size(), render_data.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, render_data.size());
        glDisableVertexAttribArray(attribute_coord);
        GL_ERRORS();

        // reset openGL stuff
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // clean VBO
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // glDeleteBuffers(1, &VBO_deferred);

        glUseProgram(0);
        glDeleteTextures(1, &atlas_tex);

        GL_ERRORS();

        // it is very important to clear all the characters for the next frame else this will accumulate/leak!
        render_chars.clear();
    }
};
