#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "data_path.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <iostream>
#include <map>
#include <string>
#include <array>
#include <vector>

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
    float tx = 0.f; // index into the larger atlas texture if available

    // construct a character from a char request and typeface (glyph) collection
    static Character Load(hb_codepoint_t request, FT_Face typeface, bool bLoadTexture = true)
    {
        // taken almost verbatim from https://learnopengl.com/In-Practice/Text-Rendering
        if (FT_Load_Glyph(typeface, request, FT_LOAD_RENDER))
            throw std::runtime_error("Failed to load glyph: " + std::to_string(request));
        GLuint tex = 0;
        if (bLoadTexture) {
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

        }
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
    GLuint draw_text_program = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    
    // shader params
    GLint shader_tex;
    GLint shader_coord;
    GLint shader_proj;
    GLint shader_color;

    // text anchor
    enum AnchorType : uint8_t {
        LEFT=0,
        RIGHT,
        CENTER,
    };

    AnchorType anchor = AnchorType::LEFT;

    // for actual text content
    std::string text_content;

    struct Atlas {
        // attempting to "glom" all relevant textures to a single texture atlas
        // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02
        
        GLuint tex;
        unsigned int w, h;
        std::array<Character, 128> chars;

        Atlas(FT_Face face, int scale, glm::vec3 const &col) {
            // create a single large atlas texture for all the ASCII characters of size scale & color col

            FT_Set_Pixel_Sizes(face, 0, scale);
		    FT_GlyphSlot g = face->glyph;
            
            // find the size of the atlas texture (1 x n texture)
            w = 0; // width of all glyphs together
            h = 0; // height of single tallest glyph
            for (int i = 32; i < 128; i++) { // all ascii character
			    if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
                    throw std::runtime_error("Failed to load glyph: " + std::to_string(i));
                    continue;
                }

                w += g->bitmap.width + 1;
                h = std::max(h, g->bitmap.rows);
            }

            // create empty texture for atlas
            glActiveTexture(GL_TEXTURE0);
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
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
            for (int i = 32; i < 128; i++) {
                if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
                    fprintf(stderr, "Loading character %c failed!\n", i);
                    continue;
                }
                Character ch{0, glm::ivec2{g->bitmap.width, g->bitmap.rows}, glm::ivec2{g->bitmap_left, g->bitmap_top}, (unsigned int)(g->advance.x), g->bitmap.buffer};
                glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, ch.Size.x, ch.Size.y, GL_RED, GL_UNSIGNED_BYTE, ch.data);
                ch.tx = static_cast<float>(x) / this->w;
                chars[i] = ch;

                /// TODO: check if this should be +1 or not?
                x += ch.Size.x + 1; 
            }
            GL_ERRORS();
            std::cout << "Generated Atlas with dims: (" << w << " x " << h << ")" << std::endl;
        }

        ~Atlas() {
            if (tex != 0) {
                glDeleteTextures(1, &tex);
            }
            GL_ERRORS();
        }
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
        if (draw_text_program == 0) {
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
            draw_text_program = gl_compile_program(vertex_shader, fragment_shader);
        }

        // set initial harfbuzz buffer params/data
        {
            set_font_size(font_size, font_scale, true); // make sure to set the font size once upon initialization
            hb_typeface = hb_ft_font_create(typeface, nullptr);
            set_text("Initial text"); /// TODO: remove
        }


        // initialize openGL for rendering
        { // get shader params
            shader_tex = glGetUniformLocation(draw_text_program, "tex");          // the atlas texture
            shader_coord = glGetAttribLocation(draw_text_program, "coord");       // the texture coordinate
            shader_proj = glGetUniformLocation(draw_text_program, "projection");  // the projection to screen
            shader_color = glGetUniformLocation(draw_text_program, "textColor");  // the text color (RGB)
        }

        if (VAO == 0 || VBO == 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
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
        }
    }

    float calculate_start_anchor(Atlas *atlas, size_t &num_newlines, float left_pos, float ss_scale) {
        if (atlas == nullptr) {
            throw std::runtime_error("Atlas is null! cannot calculate start anchor!");
        }

        // calculate the final width of the text glyphs
        std::vector<float> line_widths;
        line_widths.push_back(0.f);
        for (char c : text_content) {
            const Character& ch = atlas->chars[c];

            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            if (c == '\n') {
                line_widths.push_back(0.f);
            }
            else {
                line_widths.back() += (ch.Advance >> 6) * ss_scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
        }
        float final_width = 0.f;
        for (float line_width : line_widths) {
            final_width = std::max(line_width, final_width);
        }
        num_newlines = line_widths.size();

        float anchor_x_start = left_pos; // default (left)
        if (anchor == Text::AnchorType::CENTER) {
            anchor_x_start -= final_width / 2.f; // to be horizontally centered
        }
        else if (anchor == Text::AnchorType::RIGHT) {
            anchor_x_start -= final_width; // all the way to the right
        }
        return anchor_x_start;
    }

    void draw(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, float ss_scale, glm::vec3 const &color) {
        ss_scale = 1.0f;
        // draw a text element using an atlas texture to draw it all at once

        /// TODO: support rebuilding the atlas on window resize/param change
        Atlas *atlas = nullptr;
        if (atlas == nullptr) {
            /// TODO: make atlas "content-aware" so to only allocate memory & load necessary glyphs
            atlas = new Atlas(typeface, scale, color);
        }
        
        size_t num_newlines;
        float anchor_x_start = calculate_start_anchor(atlas, num_newlines, pos.x, ss_scale);
        float char_x = anchor_x_start;
        float char_y = pos.y;

        // handle animation (only draw fraction of total depending on time)
        time += dt;
        float amnt = std::min(time / (anim_time * num_newlines), 1.f); // 1.f => 100% is drawn

        std::vector<float> render_data; // should be 6 * 4 * render_chars.size()
        for (size_t i = 0; i < static_cast<size_t>(amnt * text_content.size()); i++) {
            char char_req = text_content[i];
            // std::cout << char_req << std::endl;
            const Character& ch = atlas->chars[char_req];
            if (char_req != '\n') {
                float xpos = char_x + ch.Bearing.x * ss_scale;
                float ypos = -char_y - (ch.Size.y - ch.Bearing.y) * ss_scale;
                float w = ch.Size.x * ss_scale;
                float h = ch.Size.y * ss_scale;
                // if (!w || !h) // skip glyphs with no size
                //     continue;

                // each 4 tuple is a {x, y, s, t} to index into the vertex/texture coordinates/attribute
                render_data.insert(render_data.end(), {
                    xpos,     -ypos - h, ch.tx,                h / atlas->h,
                    xpos,     -ypos,     ch.tx,                0.0f,
                    xpos + w, -ypos,     ch.tx + w / atlas->w, 0.0f,
                    xpos,     -ypos - h, ch.tx,                h / atlas->h,
                    xpos + w, -ypos,     ch.tx + w / atlas->w, 0.0f,
                    xpos + w, -ypos - h, ch.tx + w / atlas->w, h / atlas->h
                });

                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                char_x += (ch.Advance >> 6) * ss_scale; // bitshift by 6 to get value in pixels (2^6 = 64)
            }
            else {
                char_x = anchor_x_start; // reset x
                char_y -= atlas->h; // increment Y
            }
        }

        // do opengl drawing!
        glUseProgram(draw_text_program);

        // get shader parameters
        auto projection = glm::ortho(0.0f, drawable_size.x, 0.0f, drawable_size.y);

        // assign some parameters
        glUniformMatrix4fv(shader_proj, 1, GL_FALSE, &projection[0][0]);
        glUniform3f(shader_color, color.x, color.y, color.z);
        
        // use the texture containing the atlas
        glBindTexture(GL_TEXTURE_2D, atlas->tex);
        glUniform1i(shader_tex, 0);
        GL_ERRORS();

        // set up the VBO for vertex data
        glBindVertexArray(VAO); // since glEnableVertexAttribArray sets state in the current VAO
        glEnableVertexAttribArray(shader_coord);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glVertexAttribPointer(shader_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
        GL_ERRORS();

        // draw triangles (this is the expensive part!)
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * render_data.size(), render_data.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, render_data.size());
        glDisableVertexAttribArray(shader_coord);
        GL_ERRORS();

        // reset openGL stuff
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader_tex, 0); // disable the texture from this shader until next call

        glUseProgram(0);

        GL_ERRORS();

        if (atlas != nullptr)
            delete atlas;
    }
};
