#pragma once

#include <voxgl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>



class App
{
private:
	const int windowWidth = 1280, windowHeight = 720;
	GLFWwindow* window;

public:
	App();
	~App();

	void run();


private:
	void loadPTShader(GLuint& ptProgram);
	void loadRTShader(GLuint& rtProgram);
};

