#include "app.h"

#include <array>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <thread>
#include <vector>


#include <player.h>


glm::uint blockType = 1;
float blockDist = 50.f;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	blockDist = glm::min(glm::max(blockDist + float(yoffset), 0.5f), 100.f);
}


App::App() {
	window = voxgl::createWindow("3D Powder Toy", windowWidth, windowHeight);

	glfwSetScrollCallback(window, scroll_callback);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

App::~App() {
	voxgl::destroyWindow(window);
}

void App::loadPTShader(GLuint& ptProgram) {
	GLuint ptCompShader = voxgl::createShader("./shaders/powdertoy.comp", GL_COMPUTE_SHADER);
	std::vector<GLuint> ptShaders;
	ptShaders.emplace_back(ptCompShader);
	ptProgram = voxgl::createProgram(ptShaders);
}

void App::loadRTShader(GLuint& rtProgram) {
	GLuint rtCompShader = voxgl::createShader("./shaders/dda.comp", GL_COMPUTE_SHADER);
	std::vector<GLuint> rtShaders;
	rtShaders.emplace_back(rtCompShader);
	rtProgram = voxgl::createProgram(rtShaders);
}

void App::run() {
	// constants
	const glm::uint dimension = 256;

	const glm::uint maxVelocity = 1;
	const GLuint simSpacing = 2 * maxVelocity + 1;
	const int simCubeSize = simSpacing * simSpacing * simSpacing;

	const int simIterations = 1;
	const int simPartialIter = 27;

	// shader related stuff
	int framebufferWidth, framebufferHeight;
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

	//framebufferWidth /= 2;
	//framebufferHeight /= 2;

	// Ray cast in groups of 16x16 pixels
	const GLuint rtWork = 16;
	const GLuint rtX = framebufferWidth / rtWork + (framebufferWidth % rtWork != 0),
					rtY = framebufferHeight / rtWork + (framebufferHeight % rtWork != 0);

	// The spacing between work groups depends on the max velocity of particles.
	// It prevents all work groups from being able to write to the same voxel at any iteration.
	const GLuint ptWorkGroups = (dimension + simSpacing - 1) / simSpacing; // int division, round up


	// quad to display texture
	GLuint quadVAO;
	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);

	// ray trace texture
	GLuint rtTexture;
	glGenTextures(1, &rtTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rtTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, rtTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);


	// quad shader
	GLuint quadVertShader = voxgl::createShader("./shaders/quad.vs", GL_VERTEX_SHADER);
	GLuint quadFragShader = voxgl::createShader("./shaders/quad.fs", GL_FRAGMENT_SHADER);
	std::vector<GLuint> quadShaders;
	quadShaders.emplace_back(quadVertShader);
	quadShaders.emplace_back(quadFragShader);
	GLuint quadProgram = voxgl::createProgram(quadShaders);


	// variables shared by sim and ray trace
	glm::uint placeBlock = 0;

	// ray trace shader
	GLuint rtProgram;
	loadRTShader(rtProgram);

	GLuint rtResolutionUniform = glGetUniformLocation(rtProgram, "resolution");
	GLuint rtCamPosUniform = glGetUniformLocation(rtProgram, "cameraPos");
	GLuint rtCamMatUniform = glGetUniformLocation(rtProgram, "cameraMat");
	GLuint rtBlockDistUniform = glGetUniformLocation(rtProgram, "blockDist");
	GLuint rtPlaceBlock = glGetUniformLocation(rtProgram, "placeBlock");
	GLuint rtDimension = glGetUniformLocation(rtProgram, "dimension");

	glUseProgram(rtProgram);

	glUniform2i(rtResolutionUniform, framebufferWidth, framebufferHeight);
	glUniform1ui(rtDimension, dimension);



	// powder toy shader
	GLuint ptProgram;
	loadPTShader(ptProgram);

	GLuint ptDimension = glGetUniformLocation(ptProgram, "dimension");
	GLuint ptMaxVelocity = glGetUniformLocation(ptProgram, "maxVelocity");
	GLuint ptBlockType = glGetUniformLocation(ptProgram, "blockType");
	GLuint ptPlaceBlock = glGetUniformLocation(ptProgram, "placeBlock");
	GLuint ptTimeUniform = glGetUniformLocation(ptProgram, "time");

	glUseProgram(ptProgram);

	glUniform1ui(ptDimension, dimension);
	glUniform1ui(ptMaxVelocity, maxVelocity);


	// block location for insertion/deletetion
	GLuint blockLocation;
	glGenBuffers(1, &blockLocation);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockLocation);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * 3U, NULL, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockLocation);


	// create player object
	Player player;
	player.position = glm::vec3(dimension + 30);
	player.camera.direction = glm::vec2(-1.6f, -0.5f);


	// create grid texture
	GLuint gridTexture;
	glGenTextures(1, &gridTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, gridTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, dimension, dimension, dimension);
	glBindImageTexture(1, gridTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);


	/*GLint size;
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &size);
	std::cout << "GL_MAX_3D_TEXTURE_SIZE is " << size << std::endl;*/

	// mutex for shader uniform variables
	std::mutex dataLock;

	// Simulation and render thread
	glfwMakeContextCurrent(NULL);
	std::thread render_thread([&]() {
		glfwMakeContextCurrent(window);

		Timer renderPacer(60);

		GLdouble last_refresh = glfwGetTime();
		int frames = 0;

		std::vector<unsigned int> simTimes, renderTimes, drawTimes;
		GLuint timerQueries[3];
		glGenQueries(3, timerQueries);


		auto rng = std::minstd_rand{};

		std::vector<glm::int32> voxels;
		for (int i = 0; i < simCubeSize; i++)
			voxels.emplace_back(i);


		while (!glfwWindowShouldClose(window)) {
			// physics shader
			glUseProgram(ptProgram);

			dataLock.lock();
			glUniform1ui(ptBlockType, blockType);
			glUniform1ui(ptPlaceBlock, placeBlock);
			dataLock.unlock();


			glBeginQuery(GL_TIME_ELAPSED, timerQueries[0]);
			// This must be done here because shaders only guarantee
			// memory coherency between local invocations.
			for (int j = 0; j < simIterations; j++) {
				std::shuffle(voxels.begin(), voxels.end(), rng);
				for (int i = 0; i < simPartialIter; i++) {
					glUniform1i(ptTimeUniform, voxels[i]);
					glDispatchCompute(ptWorkGroups, ptWorkGroups, ptWorkGroups);
					glMemoryBarrier(GL_ALL_BARRIER_BITS);
				}
			}
			glEndQuery(GL_TIME_ELAPSED);


			// ray trace shader
			glUseProgram(rtProgram);

			dataLock.lock();
			glUniform3f(rtCamPosUniform, player.camera.position.x, player.camera.position.y, player.camera.position.z);
			glUniformMatrix3fv(rtCamMatUniform, 1, GL_FALSE, glm::value_ptr(glm::transpose(player.camera.getMatrix())));
			glUniform1f(rtBlockDistUniform, blockDist);
			glUniform1ui(rtPlaceBlock, placeBlock);
			dataLock.unlock();

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[1]);
			glDispatchCompute(rtX, rtY, 1);
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			glEndQuery(GL_TIME_ELAPSED);


			// draw ray trace texture to quad
			glUseProgram(quadProgram);

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[2]);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glEndQuery(GL_TIME_ELAPSED);

			glfwSwapBuffers(window);


			GLint queryAvailable = 0;
			GLuint elapsed_time;

			while (!queryAvailable) {
				glGetQueryObjectiv(timerQueries[0],
					GL_QUERY_RESULT_AVAILABLE,
					&queryAvailable);
			}
			glGetQueryObjectuiv(timerQueries[0], GL_QUERY_RESULT, &elapsed_time);
			simTimes.push_back(elapsed_time / 1000);

			queryAvailable = 0;
			while (!queryAvailable) {
				glGetQueryObjectiv(timerQueries[1],
					GL_QUERY_RESULT_AVAILABLE,
					&queryAvailable);
			}
			glGetQueryObjectuiv(timerQueries[1], GL_QUERY_RESULT, &elapsed_time);
			renderTimes.push_back(elapsed_time / 1000);
			
			queryAvailable = 0;
			while (!queryAvailable) {
				glGetQueryObjectiv(timerQueries[2],
					GL_QUERY_RESULT_AVAILABLE,
					&queryAvailable);
			}
			glGetQueryObjectuiv(timerQueries[2], GL_QUERY_RESULT, &elapsed_time);
			drawTimes.push_back(elapsed_time / 1000);
			

			frames++;
			if (int(glfwGetTime() - last_refresh) > 0) {
				std::cout << frames << "fps" << std::endl;
				frames = 0;
				
				auto const simCount = static_cast<float>(simTimes.size());
				auto const renderCount = static_cast<float>(renderTimes.size());
				auto const drawCount = static_cast<float>(drawTimes.size());

				if(simCount > 0 && renderCount > 0 && drawCount > 0)
					std::cout << "sim time: " << std::reduce(simTimes.begin(), simTimes.end()) / simCount << "us\t" << "render time: " << std::reduce(renderTimes.begin(), renderTimes.end()) / renderCount << "us\t" << "draw time: " << std::reduce(drawTimes.begin(), drawTimes.end()) / drawCount << "us" << std::endl;

				simTimes.clear();
				renderTimes.clear();
				drawTimes.clear();

				last_refresh = glfwGetTime();
			}

			// Needs to be fixed
			/*/ Recompile shaders with 'R' key
			if (glfwGetKey(window, GLFW_KEY_R)) {
				loadPTShader(ptProgram);

				ptDimension = glGetUniformLocation(ptProgram, "dimension");
				ptMaxVelocity = glGetUniformLocation(ptProgram, "maxVelocity");
				ptSpacing = glGetUniformLocation(ptProgram, "spacing");
				ptBlockType = glGetUniformLocation(ptProgram, "blockType");
				ptPlaceBlock = glGetUniformLocation(ptProgram, "placeBlock");
				ptTimeUniform = glGetUniformLocation(ptProgram, "time");

				glUseProgram(ptProgram);
				glUniform1ui(ptDimension, dimension);
				glUniform1ui(ptMaxVelocity, maxVelocity);
				glUniform1ui(ptSpacing, spacing);


				loadRTShader(rtProgram);

				rtResolutionUniform = glGetUniformLocation(rtProgram, "resolution");
				rtCamPosUniform = glGetUniformLocation(rtProgram, "cameraPos");
				rtCamMatUniform = glGetUniformLocation(rtProgram, "cameraMat");
				rtBlockDistUniform = glGetUniformLocation(rtProgram, "blockDist");
				rtPlaceBlock = glGetUniformLocation(rtProgram, "placeBlock");
				rtDimension = glGetUniformLocation(rtProgram, "dimension");

				glUseProgram(rtProgram);
				glUniform2i(rtResolutionUniform, framebufferWidth, framebufferHeight);
				glUniform1ui(ptDimension, dimension);
			}*/

			renderPacer.tick();
		}
	});


	// Input thread
	Timer inputPacer(500);
	unsigned int timeDelta;

	while (!glfwWindowShouldClose(window)) {
		timeDelta = inputPacer.tick();

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		dataLock.lock();

		player.handleInputs(window);
		player.update(timeDelta);

		// block distance
		if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
			blockDist = glm::min(blockDist + 0.05f, 100.f);

		else if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
			blockDist = glm::max(blockDist - 0.05f, 0.5f);


		// block type
		if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
			blockType = 0;

		if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
			blockType = 1;

		else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
			blockType = 2;

		else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
			blockType = 3;


		// place block
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			if(!placeBlock)
				placeBlock = 1;
		}
		else {
			if (placeBlock)
				placeBlock = 0;
		}

		dataLock.unlock();
	}

	render_thread.join();

	glDeleteProgram(ptProgram);
	glDeleteProgram(quadProgram);
	glDeleteProgram(rtProgram);

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &quadVAO);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &rtTexture);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &gridTexture);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glDeleteBuffers(1, &blockLocation);
}
