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
    glm::ivec2 Size; // Size of glyph
    glm::ivec2 Bearing; // Offset from baseline to left/top of glyph
    unsigned int Advance; // Offset to advance to next glyph

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
        // now store character for later use
        return {
            tex,
            glm::ivec2(typeface->glyph->bitmap.width, typeface->glyph->bitmap.rows),
            glm::ivec2(typeface->glyph->bitmap_left, typeface->glyph->bitmap_top),
            (unsigned int)(typeface->glyph->advance.x)
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
    glm::mat4 projection;
    GLuint draw_text_program = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;

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
        {
            // https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text.vs
            const auto vertex_shader = "#version 330 core\n"
                                       "layout (location = 0) in vec4 vertex;\n"
                                       "out vec2 TexCoords;\n"
                                       "uniform mat4 projection;\n"
                                       "void main()\n"
                                       "{\n"
                                       "    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
                                       "    TexCoords = vertex.zw;\n"
                                       "}\n";

            // https://learnopengl.com/code_viewer_gh.php?code=src/7.in_practice/2.text_rendering/text.fs
            const auto fragment_shader = "#version 330 core\n"
                                         "in vec2 TexCoords;\n"
                                         "layout (location = 0) out vec4 color;\n"
                                         "layout (location = 1) out vec4 bright;\n"
                                         "uniform sampler2D text;\n"
                                         "uniform vec3 textColor;\n"
                                         "void main()\n"
                                         "{\n"
                                         "	vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"
                                         "   color = vec4(textColor, 1.0) * sampled;\n"
                                         "   bright = vec4(vec3(0.0), 1.0);\n"
                                         "}\n";
            draw_text_program = gl_compile_program(vertex_shader, fragment_shader);
        }

        // set initial harfbuzz buffer params/data
        {
            set_font_size(font_size, font_scale, true); // make sure to set the font size once upon initialization
            hb_typeface = hb_ft_font_create(typeface, nullptr);
            set_text("Initial text"); /// TODO: remove
        }

        // initialize openGL for rendering
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
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

    void draw(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, float ss_scale, glm::vec3 const &color)
    {
        // drawable_size - window size
        // width - how wide the displayed string gets to be
        // pos - position in screenspace where the text gets rendered
        // ss_scale - screenspace scale (lower quality but cheap)
        // color - rgb(1) colour

        float new_font_scale = scale; // scale font size off window height
        set_font_size(font_size, new_font_scale);

        projection = glm::ortho(0.0f, drawable_size.x, 0.0f, drawable_size.y);

        glUseProgram(draw_text_program);
        glUniform3f(glGetUniformLocation(draw_text_program, "textColor"), color.x, color.y, color.z);
        glUniformMatrix4fv(glGetUniformLocation(draw_text_program, "projection"), 1, GL_FALSE, &projection[0][0]);
        glActiveTexture(GL_TEXTURE0);

        glBindVertexArray(VAO);

        unsigned int num_chars = 0;
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

        float anchor_x_start = pos.x; // default (left)
        if (anchor == Text::AnchorType::CENTER) {
            anchor_x_start -= final_width / 2.f; // to be horizontally centered
        }
        else if (anchor == Text::AnchorType::RIGHT) {
            anchor_x_start -= final_width; // all the way to the right
        }

        float char_x = anchor_x_start;
        float char_y = pos.y;

        // handle animation (only draw fraction of total depending on time)
        time += dt;
        float amnt = std::min(time / (anim_time * line_widths.size()), 1.f);
        // std::cout << amnt << " | " << time << std::endl;

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
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
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
};
