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


glm::float32 blockSize = 4.f;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	blockSize = glm::min(glm::max(blockSize + float(yoffset), 1.f), 50.f);
}


App::App() {
	window = voxgl::createWindow("Voxel Particle Simulator", windowWidth, windowHeight);

	glfwSetScrollCallback(window, scroll_callback);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

App::~App() {
	voxgl::destroyWindow(window);
}


void App::run() {
	// constants
	const int fps = 60;
	const glm::uint dimension = 256;
	/*GLint size;
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &size);
	std::cout << "GL_MAX_3D_TEXTURE_SIZE is " << size << " in each dimension." << std::endl;*/

	const glm::uint simLocalDim = 8;  // 8 is the max work group dimension on my gtx 1080
	const glm::uint maxVelocity = simLocalDim / 2;  // max velocity is limited by shader work group size
	const glm::uint simSpacing = 2 * simLocalDim;

	const int simIterations = 1;


	// variable uniforms shared between sim and renderer
	glm::uint currentFlag = 0;
	glm::uint placeBlock = 0;
	glm::uint blockType = 1;
	glm::uint drawLines = 0;
	glm::float32 blockDist = 50.f;
	glm::uint updateDist = 1;
	bool distUpdated = true;


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


	// ray trace shader
	GLuint rtProgram;
	loadRTShader(rtProgram);

	GLuint rtResolution = glGetUniformLocation(rtProgram, "resolution");
	GLuint rtCamPos = glGetUniformLocation(rtProgram, "cameraPos");
	GLuint rtCamMat = glGetUniformLocation(rtProgram, "cameraMat");
	GLuint rtDimension = glGetUniformLocation(rtProgram, "dimension");
	GLuint rtBlockDist = glGetUniformLocation(rtProgram, "blockDist");
	GLuint rtBlockSize = glGetUniformLocation(rtProgram, "blockSize");
	GLuint rtPlaceBlock = glGetUniformLocation(rtProgram, "placeBlock");
	GLuint rtUpdateDist = glGetUniformLocation(rtProgram, "updateDist");
	GLuint rtDrawLines = glGetUniformLocation(rtProgram, "drawLines");


	glUseProgram(rtProgram);

	glUniform2i(rtResolution, framebufferWidth, framebufferHeight);
	glUniform1ui(rtDimension, dimension);


	// particle sim shader
	GLuint ptProgram;
	loadPTShader(ptProgram);

	GLuint ptCurrentFlag = glGetUniformLocation(ptProgram, "currentFlag");
	GLuint ptPartition = glGetUniformLocation(ptProgram, "part");
	GLuint ptRNG = glGetUniformLocation(ptProgram, "rng");
	GLuint ptBlockType = glGetUniformLocation(ptProgram, "blockType");
	GLuint ptBlockSize = glGetUniformLocation(ptProgram, "blockSize");
	GLuint ptPlaceBlock = glGetUniformLocation(ptProgram, "placeBlock");
	GLuint ptDimension = glGetUniformLocation(ptProgram, "dimension");
	//GLuint ptMaxVelocity = glGetUniformLocation(ptProgram, "maxVelocity");

	glUseProgram(ptProgram);

	glUniform1ui(ptDimension, dimension);
	//glUniform1ui(ptMaxVelocity, maxVelocity);


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



	// mutex for shader uniform variables
	std::mutex dataLock;

	// Simulation and render thread
	glfwMakeContextCurrent(NULL);
	std::thread render_thread([&]() {
		glfwMakeContextCurrent(window);
		
		Timer renderPacer(fps);

		GLdouble last_refresh = glfwGetTime();
		int frames = 0;

		std::vector<unsigned int> simTimes, renderTimes, drawTimes;
		GLuint timerQueries[3];
		glGenQueries(3, timerQueries);

		auto rng = std::minstd_rand{};


		while (!glfwWindowShouldClose(window)) {
			// physics shader
			glUseProgram(ptProgram);

			dataLock.lock();
			glUniform1ui(ptBlockType, blockType);
			glUniform1f(ptBlockSize, blockSize);
			glUniform1ui(ptPlaceBlock, placeBlock);
			dataLock.unlock();

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[0]);
			for (int it = 0; it < simIterations; it++) {
				glUniform1ui(ptCurrentFlag, currentFlag = (!currentFlag) * 0x80);
				glUniform1f(ptRNG, float(rng())/ float(rng.max()));

				for (int part = 0; part < 8; part++) {
					glUniform3i(ptPartition, part % 2, (part / 2) % 2, (part / 4) % 2);
					glDispatchCompute(ptWorkGroups, ptWorkGroups, ptWorkGroups);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				}
			}
			glEndQuery(GL_TIME_ELAPSED);


			// ray trace shader
			glUseProgram(rtProgram);

			dataLock.lock();
			glUniform3f(rtCamPos, player.camera.position.x, player.camera.position.y, player.camera.position.z);
			glUniformMatrix3fv(rtCamMat, 1, GL_FALSE, glm::value_ptr(glm::transpose(player.camera.getMatrix())));
			glUniform1f(rtBlockDist, blockDist);
			glUniform1f(rtBlockSize, blockSize);
			glUniform1f(rtPlaceBlock, placeBlock);
			glUniform1ui(rtUpdateDist, updateDist);
			distUpdated = true;
			glUniform1ui(rtDrawLines, drawLines);
			dataLock.unlock();

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[1]);
			glDispatchCompute(rtX, rtY, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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


			// Recompile shaders with 'R' key
			if (glfwGetKey(window, GLFW_KEY_R)) {
				loadPTShader(ptProgram);

				ptCurrentFlag = glGetUniformLocation(ptProgram, "currentFlag");
				ptPartition = glGetUniformLocation(ptProgram, "part");
				ptRNG = glGetUniformLocation(ptProgram, "rng");
				ptBlockType = glGetUniformLocation(ptProgram, "blockType");
				ptBlockSize = glGetUniformLocation(ptProgram, "blockSize");
				ptPlaceBlock = glGetUniformLocation(ptProgram, "placeBlock");
				ptDimension = glGetUniformLocation(ptProgram, "dimension");

				glUseProgram(ptProgram);
				glUniform1ui(ptDimension, dimension);


				loadRTShader(rtProgram);

				rtResolution = glGetUniformLocation(rtProgram, "resolution");
				rtCamPos = glGetUniformLocation(rtProgram, "cameraPos");
				rtCamMat = glGetUniformLocation(rtProgram, "cameraMat");
				rtDimension = glGetUniformLocation(rtProgram, "dimension");
				rtBlockDist = glGetUniformLocation(rtProgram, "blockDist");
				rtBlockSize = glGetUniformLocation(rtProgram, "blockSize");
				rtPlaceBlock = glGetUniformLocation(rtProgram, "placeBlock");
				rtUpdateDist = glGetUniformLocation(rtProgram, "updateDist");
				rtDrawLines = glGetUniformLocation(rtProgram, "drawLines");

				glUseProgram(rtProgram);
				glUniform2i(rtResolution, framebufferWidth, framebufferHeight);
				glUniform1ui(rtDimension, dimension);
			}

			renderPacer.tick();
		}
	});


	// Input thread
	Timer inputPacer(500);
	unsigned int timeDelta;
	glm::vec3 prevFacingRay {0};
	bool prevQ = false;

	while (!glfwWindowShouldClose(window)) {
		timeDelta = inputPacer.tick();

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		bool cameraChanged = false;

		dataLock.lock();

		player.handleInputs(window);
		player.update(timeDelta);

		glm::vec3 facingRay = player.camera.facingRay();

		if (distUpdated) {
			if (facingRay != prevFacingRay  ||  glm::any(glm::greaterThan(glm::abs(player.velocity), glm::vec3(0)))) {
				updateDist = 1;
				distUpdated = false;
			}
			else
				updateDist = 0;
		}


		// block distance
		if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
			blockDist = glm::min(blockDist + 0.05f, 200.f);
			updateDist = 1;
			distUpdated = false;
		}
		else if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
			blockDist = glm::max(blockDist - 0.05f, blockSize);
			updateDist = 1;
			distUpdated = false;
		}


		// block type
		if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)		blockType = 0;
		else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)	blockType = 1;
		else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)	blockType = 2;
		else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)	blockType = 3;


		// place block
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			placeBlock = 1;
		else
			placeBlock = 0;

		// toggle draw lines
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
			if (!prevQ) {
				if(drawLines) drawLines = 0;
				else drawLines = 1;
			}
			prevQ = true;
		}
		else
			prevQ = false;

		dataLock.unlock();


		prevFacingRay = facingRay;
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
	//glDeleteTextures(1, &gridTexture1);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glDeleteBuffers(1, &blockLocation);
}


void App::loadPTShader(GLuint& ptProgram) {
	GLuint ptCompShader = voxgl::createShader("./shaders/particlesim.comp", GL_COMPUTE_SHADER);
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
