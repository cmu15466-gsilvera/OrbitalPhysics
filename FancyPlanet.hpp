#include "GL.hpp"
#include "Scene.hpp"

struct FancyPlanet {
    FancyPlanet(Scene::Transform *transform);
    void draw(Scene::Camera *cam);

    GLuint texture, vao, vbo;
    Scene::Transform *transform;
};
