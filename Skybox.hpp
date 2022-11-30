#include "GL.hpp"
#include "Scene.hpp"

struct Skybox {
	GLuint texture, skyboxVAO, skyboxVBO;
	Skybox();
	void draw(Scene::Camera *cam);
};
