#include <string>
#include "FancyPlanet.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "GL.hpp"
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
    GLuint vao = buffer.make_vao_for_program(textured_planet_program->program);
    LoadData const *data = new LoadData(buffer, vao, buffer.lookup("Planet").count);
	return data;
});

FancyPlanet::FancyPlanet(Scene::Transform *_transform){
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    transform = _transform;

    std::string texture_path = "noiseTexture.png"; 

    int width, height, nrChannels;
    unsigned char *data;  
    data = stbi_load(
            data_path(texture_path.c_str()).c_str(),
            &width, 
            &height, 
            &nrChannels, 
            0);
    glTexImage2D(
            GL_TEXTURE_2D, 
            0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
    stbi_image_free(data);

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
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, planet_data->count);
	glUseProgram(0);
	glBindVertexArray(0);
}

