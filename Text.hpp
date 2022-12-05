#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "data_path.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <iostream>
#include <unordered_map>
#include <set>
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
    glm::vec2 t{0.f, 0.f}; // index into the larger atlas texture if available

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
    bool bIsStaticText = false; // whether or not to build a minimal Atlas (static text) or not (dynamic text)

    struct Atlas {
        // attempting to "glom" all relevant textures to a single texture atlas
        // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Text_Rendering_02

        // especially helpful reading:
        // https://gitlab.com/wikibooks-opengl/modern-tutorials/-/blob/master/text02_atlas/text.cpp#L226
        
        GLuint tex;
        unsigned int w, h;
        std::array<Character, 128> chars; // supports a variable number of characters
        // std::unordered_map<char, size_t> char_map; // mapping from char to char-index in atlas (for vector)
        static constexpr int MAXTEXWIDTH = 1024;
        int fontscale;

        Atlas(FT_Face face, int scale, std::string const &text = "") {
            // create a single large atlas texture for all the ASCII characters of size scale
            update(face, scale, text);
            std::cout << "Generated Atlas with dims: (" << w << " x " << h << ")"  << std::endl;
        }

        ~Atlas() {
            reset();
            GL_ERRORS();
        }

        void update(FT_Face face, int scale, std::string const &text = "") {
            fontscale = scale;
            
            FT_Set_Pixel_Sizes(face, 0, fontscale);
		    FT_GlyphSlot g = face->glyph;
            std::set<char> render_chars = {}; // don't care about duplicates
            if (text == "") {
                for (int i = 32; i < 128; i++) { // all drawable ascii characters
                    render_chars.insert(static_cast<char>(i));
                }
            } else {
                std::cout << "Generating Atlas texture for text: \"" << text << "\"" << std::endl;
                for (char c : text) {
                    render_chars.insert(c);
                }
            }


            // find the size of the atlas texture (big  width limited by MAXTEXWIDTH)
            unsigned int roww = 0, rowh = 0;
            w = 0; // width of all glyphs together
            h = 0; // height of single tallest glyph
            for (char c : render_chars) { // all ascii character
			    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    throw std::runtime_error("Failed to load glyph for char: \'" + std::to_string(c) + "\'");
                    continue;
                }

                if (roww + g->bitmap.width + 1 >= MAXTEXWIDTH) {
                    w = std::max(w, roww);
                    h += rowh;
                    // reset the dims to a new row
                    roww = 0;
                    rowh = 0;
                }

                roww += g->bitmap.width + 1;
                rowh = std::max(rowh, g->bitmap.rows);
            }
            w = std::max(w, roww);
            h += rowh;

            reset(); // kill existing atlas texture if exists

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
            glm::ivec2 offset{0, 0};
            rowh = 0;
            for (char c : render_chars) {
                if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    throw std::runtime_error("Failed to load glyph for char: \'" + std::to_string(c) + "\'");
                    continue;
                }
                if (offset.x + g->bitmap.width + 1 >= MAXTEXWIDTH) {
                    offset.y += rowh;
                    rowh = 0;
                    offset.x = 0;
                }
                glm::vec2 tex_char_offset{static_cast<float>(offset.x) / this->w, static_cast<float>(offset.y) / this->h};
                Character ch{0, // no texture
                    glm::ivec2{g->bitmap.width, g->bitmap.rows}, // size
                    glm::ivec2{g->bitmap_left, g->bitmap_top}, // bearing 
                    (unsigned int)(g->advance.x), // advance
                    g->bitmap.buffer, // buffer data
                    tex_char_offset,// texture
                };
                glTexSubImage2D(GL_TEXTURE_2D, 0, offset.x, offset.y, ch.Size.x, ch.Size.y, GL_RED, GL_UNSIGNED_BYTE, ch.data);
                chars[c] = ch;

                rowh = std::max(rowh, g->bitmap.rows);
                offset.x += ch.Size.x + 1;
            }
            GL_ERRORS();
        }

        void reset() {
            if (tex != 0) {
                glDeleteTextures(1, &tex);
            }
        }
    };
    Atlas *atlas = nullptr;    

    ~Text() {
        if (atlas != nullptr) {
            delete atlas;
        }
    }

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
            auto get_uniform = [this](std::string const &name){
                GLint param = glGetUniformLocation(this->draw_text_program, name.c_str());
                if (param == -1) {
                    throw std::runtime_error("Unable to read uniform param \""+name+"\"");
                }
                return param;
            };
            auto get_attrib = [this](std::string const &name){
                GLint param = glGetAttribLocation(this->draw_text_program, name.c_str());
                if (param == -1) {
                    throw std::runtime_error("Unable to read uniform param \""+name+"\"");
                }
                return param;
            };
            shader_tex = get_uniform("tex");          // the atlas texture
            shader_coord = get_attrib("coord");       // the texture coordinate
            shader_proj = get_uniform("projection");  // the projection to screen
            shader_color = get_uniform("textColor");  // the text color (RGB)
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

    void set_static_text(const std::string &new_text) {
        set_text(new_text);
        bIsStaticText = true;
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

    float calculate_start_anchor(Atlas *_atlas, size_t &num_newlines, int &line_height, float left_pos) {
        if (_atlas == nullptr) {
            throw std::runtime_error("Atlas is null! cannot calculate start anchor!");
        }

        // calculate the final width of the text glyphs
        std::vector<float> line_widths;
        line_widths.push_back(0.f);
        line_height = 0;
        for (char c : text_content) {
            const Character& ch = _atlas->chars[c];

            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            if (c == '\n') {
                line_widths.push_back(0.f);
            }
            else {
                line_widths.back() += (ch.Advance >> 6); // bitshift by 6 to get value in pixels (2^6 = 64)
            }
            line_height = std::max(line_height, ch.Size.y);
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

    void draw(float dt, const glm::vec2& drawable_size, float scale, const glm::vec2& pos, glm::vec3 const &color) {
        // overload to cast scale to int
        draw(dt, drawable_size, static_cast<int>(scale), pos, color);
    }

    void draw(float dt, const glm::vec2& drawable_size, int scale, const glm::vec2& pos, glm::vec3 const &color) {
        // draw a text element using an atlas texture to draw it all at once

        if (atlas == nullptr) {
            /// TODO: make atlas "content-aware" so to only allocate memory & load necessary glyphs
            atlas = new Atlas(typeface, scale, bIsStaticText ? text_content : "");
        }

        if (scale != atlas->fontscale) {
            atlas->update(typeface, scale, bIsStaticText ? text_content : ""); // resize
        }
        
        size_t num_newlines;
        int line_height;
        float anchor_x_start = calculate_start_anchor(atlas, num_newlines, line_height, pos.x);
        float char_x = anchor_x_start;
        float char_y = pos.y + line_height;

        // handle animation (only draw fraction of total depending on time)
        time += dt;
        float amnt = std::min(time / (anim_time * num_newlines), 1.f); // 1.f => 100% is drawn

        std::vector<float> render_data; // should be 6 * 4 * render_chars.size()
        const size_t num_render_chars = static_cast<size_t>(amnt * text_content.size());
        for (size_t i = 0; i < num_render_chars; i++) {
            char char_req = text_content[i];
            const Character& ch = atlas->chars[char_req];
            if (char_req != '\n') {
                float xpos = char_x + ch.Bearing.x;
                float ypos = -char_y - (ch.Size.y - ch.Bearing.y);
                float w = static_cast<float>(ch.Size.x);
                float h = static_cast<float>(ch.Size.y);
                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                char_x += (ch.Advance >> 6); // bitshift by 6 to get value in pixels (2^6 = 64)

                // if (!w || !h) // skip glyphs with no size
                //     continue;

                // each 4 tuple is a {x, y, s, t} to index into the vertex/texture coordinates/attribute
                render_data.insert(render_data.end(), {
                    xpos,     -ypos - h, ch.t.x,                ch.t.y + h / atlas->h,
                    xpos,     -ypos,     ch.t.x,                ch.t.y,
                    xpos + w, -ypos,     ch.t.x + w / atlas->w, ch.t.y,
                    xpos,     -ypos - h, ch.t.x,                ch.t.y + h / atlas->h,
                    xpos + w, -ypos,     ch.t.x + w / atlas->w, ch.t.y,
                    xpos + w, -ypos - h, ch.t.x + w / atlas->w, ch.t.y + h / atlas->h
                });
            }
            else {
                char_x = anchor_x_start; // reset x
                char_y -= line_height; // increment Y
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
        glBufferData(GL_ARRAY_BUFFER, num_render_chars * sizeof(float) * 6 * 4, render_data.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(num_render_chars * 6));
        glDisableVertexAttribArray(shader_coord);
        GL_ERRORS();

        // reset openGL stuff
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(shader_tex, 0); // disable the texture from this shader until next call

        glUseProgram(0);

        GL_ERRORS();
    }
};
