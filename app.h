#pragma once

#include <voxgl.h>

#include <HashGrid.h>
#include <SVO.h>
#include <DAG.h>
#include <player.h>

#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>



class App
{
private:
	const int windowWidth = 1280, windowHeight = 720;
	GLFWwindow* window;

	const unsigned int dimension;

public:
	App();
	~App();

	void run();


private:
	void loadPTShader(GLuint& ptProgram);
	void loadRTShader(GLuint& rtProgram);
};

