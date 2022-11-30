#include "FancyPlanet.hpp"
#include "GL.hpp"
#include <gl/GLU.h>
#include <minwindef.h>
#include <set>
#include <string>
#include "glm/gtc/type_ptr.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "TexturedPlanetProgram.hpp"
#include "data_path.hpp"
#include "stb_image.h"

struct LoadData {
    MeshBuffer buffer;
    GLuint vao;
    int count;
    LoadData(MeshBuffer buffer, GLuint vao, int count) : buffer(buffer), vao(vao), count(count) {}
};

Load<LoadData> planet_data(LoadTagDefault, []() ->  LoadData const * {
    MeshBuffer buffer(data_path("planet.pnct"));
	//create a new vertex array object:
	GLuint vao = 0;
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::u8vec4 Color;
		glm::vec3 TexCoord;
	};
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//Try to bind all attributes in this buffer:
	glBindBuffer(GL_ARRAY_BUFFER, buffer.buffer);
	auto bind_attribute = [&](char const *name, MeshBuffer::Attrib const &attrib) {
		if (attrib.size == 0) return; //don't bind empty attribs
		GLint location = glGetAttribLocation(textured_planet_program->program, name);
		if (location == -1) return; //can't bind missing attribs
		glVertexAttribPointer(location, attrib.size, attrib.type, attrib.normalized, attrib.stride, (GLbyte *)0 + attrib.offset);
		glEnableVertexAttribArray(location);
	};
    MeshBuffer::Attrib Position = Attrib(3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, Position));
    MeshBuffer::Attrib	Normal = Attrib(3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, Normal));
	MeshBuffer::Attrib Color = Attrib(4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), offsetof(Vertex, Color));
	MeshBuffer::Attrib TexCoord = Attrib(2, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, TexCoord));
        GLUquadricObj *sphere=NULL;
      sphere = gluNewQuadric();
      gluQuadricDrawStyle(sphere, GLU_FILL);
      gluQuadricTexture(sphere, TRUE);
      gluQuadricNormals(sphere, GLU_SMOOTH);
      //Making a display list
      mysphereID = glGenLists(1);
    glEnableVertexAttribArray(location);
	bind_attribute("Position", Position);
	bind_attribute("Normal", Normal);
	bind_attribute("Color", Color);
	bind_attribute("TexCoord", TexCoord);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

    LoadData const *data = new LoadData(buffer, vao, buffer.lookup("Planet").count);
	return data;
});

FancyPlanet::FancyPlanet(Scene::Transform *_transform){

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    std::string texture_pathes[6] = {
        "skyboxRT.png",
        "skyboxLF.png",
        "skyboxUP.png",
        "skyboxDN.png",
        "skyboxFT.png",
        "skyboxBK.png"
    };

    int width, height, nrChannels;
    unsigned char *data;  
    for(unsigned int i = 0; i < 6; i++)
    {
        data = stbi_load(
                data_path(texture_pathes[i].c_str()).c_str(),
                &width, 
                &height, 
                &nrChannels, 
                0);
        glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
                );
        stbi_image_free(data);
    }


    /* glGenTextures(1, &texture); */
	/* glActiveTexture(GL_TEXTURE0); */
    /* glBindTexture(GL_TEXTURE_2D, texture); */

    /* transform = _transform; */

    /* std::string texture_path = "seamless-noise.png"; */ 

    /* int width, height, nrChannels; */
    /* unsigned char *data; */  
    /* data = stbi_load( */
    /*         data_path(texture_path.c_str()).c_str(), */
    /*         &width, */ 
    /*         &height, */ 
    /*         &nrChannels, */ 
    /*         0); */
	/* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); */
    /* stbi_image_free(data); */


    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void FancyPlanet::draw(Scene::Camera *cam){
    glUseProgram(textured_planet_program->program);

	glm::mat4 world_to_clip = cam->make_projection() * glm::mat4(cam->transform->make_world_to_local());

    glm::mat4x3 object_to_world = transform->make_local_to_world();

    glm::mat4 object_to_clip = world_to_clip * glm::mat4(object_to_world);
    glUniformMatrix4fv(textured_planet_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));

    glBindVertexArray(planet_data->vao);
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glDrawArrays(GL_TRIANGLES, 0, planet_data->count);
	glUseProgram(0);
	glBindVertexArray(0);
}

