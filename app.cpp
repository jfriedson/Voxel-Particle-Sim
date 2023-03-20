#include "App.h"
#include "CallbackSingleton.h"

#include <array>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>




App::App() {
	createWindow();

	setupShaders();
	
	createObjects();
}

App::~App() {
	glDeleteProgram(physShaderProperties.ptProgram);
	glDeleteProgram(renderShaderProperties.quadProgram);
	glDeleteProgram(renderShaderProperties.rtProgram);

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &renderShaderProperties.quadVAO);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &renderShaderProperties.rtTexture);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &sharedShaderProperties.gridTexture);
	//glDeleteTextures(1, &gridTexture1);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glDeleteBuffers(1, &blockPlacingProperties.blockLocation);
}


void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	blockPlacingProperties.blockSize = glm::min(glm::max(blockPlacingProperties.blockSize + float(yoffset), 1.f), 50.f);
}

void App::createWindow() {
	windowProperties.window = voxgl::createWindow("Voxel Particle Simulator" , windowProperties.windowWidth, windowProperties.windowHeight, false);

	CallbackSingleton& cbs = CallbackSingleton::getInstance(this);
	glfwSetScrollCallback(windowProperties.window.get(), cbs.scrollCallback);
}


void App::setupShaders() {
	setupRenderShader();
	setupPhysicsShader();


	// block location for insertion/deletetion
	blockPlacingProperties.blockLocation;
	glGenBuffers(1, &blockPlacingProperties.blockLocation);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockPlacingProperties.blockLocation);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * 3U, NULL, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockPlacingProperties.blockLocation);

	// create grid texture
	sharedShaderProperties.gridTexture;
	glGenTextures(1, &sharedShaderProperties.gridTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, sharedShaderProperties.gridTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, physProperties.dimension, physProperties.dimension, physProperties.dimension);
	glBindImageTexture(1, sharedShaderProperties.gridTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
}

void App::setupRenderShader() {
	glfwGetFramebufferSize(windowProperties.window.get(), &renderProperties.framebufferWidth, &renderProperties.framebufferHeight);

	// increase performance for high resolution monitors
	if (windowProperties.windowWidth > 1920 || windowProperties.windowHeight > 1080) {
		renderProperties.framebufferWidth /= 2;
		renderProperties.framebufferHeight /= 2;
	}

	// workgroup size for rendering
	renderShaderProperties.rtX = renderProperties.framebufferWidth / renderShaderProperties.rtWork + (renderProperties.framebufferWidth % renderShaderProperties.rtWork != 0);
	renderShaderProperties.rtY = renderProperties.framebufferHeight / renderShaderProperties.rtWork + (renderProperties.framebufferHeight % renderShaderProperties.rtWork != 0);

	// quad to display texture
	glGenVertexArrays(1, &renderShaderProperties.quadVAO);
	glBindVertexArray(renderShaderProperties.quadVAO);

	// ray trace texture
	glGenTextures(1, &renderShaderProperties.rtTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderShaderProperties.rtTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderProperties.framebufferWidth, renderProperties.framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindImageTexture(0, renderShaderProperties.rtTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

	// load quad shaders
	GLuint quadVertShader = voxgl::createShader("./shaders/quad.vs", GL_VERTEX_SHADER);
	GLuint quadFragShader = voxgl::createShader("./shaders/quad.fs", GL_FRAGMENT_SHADER);
	std::vector<GLuint> quadShaders;
	quadShaders.emplace_back(quadVertShader);
	quadShaders.emplace_back(quadFragShader);
	renderShaderProperties.quadProgram = voxgl::createProgram(quadShaders);

	loadRTShader();
}

void App::setupPhysicsShader() {
	// find the maximum 3D texture dimensions supported by your system, considering future use
	/*GLint size;
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &size);
	std::cout << "GL_MAX_3D_TEXTURE_SIZE is " << size << " in each dimension." << std::endl;
	const glm::uint dimension = size;*/

	// The spacing between work groups depends on the max velocity of particles, and vice versa
	// It removes the possibility of work groups writing to the same voxel at any iteration
	physShaderProperties.ptWorkGroups = (physProperties.dimension - 1) / physShaderProperties.simSpacing + 1; // int division with ceiling rounding

	loadPTShader();
}


void App::createObjects() {
	worldObjects.player.position = glm::vec3{ static_cast<float>(physProperties.dimension + 30) };
	worldObjects.player.camera.direction = glm::vec2{ -.3f, 0.5f };
}


void App::loadPTShader() {
	GLuint ptCompShader = voxgl::createShader("./shaders/particlesim.comp", GL_COMPUTE_SHADER);
	std::vector<GLuint> ptShaders;
	ptShaders.emplace_back(ptCompShader);

	GLint shaderRef = voxgl::createProgram(ptShaders);
	if (shaderRef != -1) {
		physShaderProperties.ptProgram = shaderRef;


		physShaderProperties.ptCurrentFlag = glGetUniformLocation(physShaderProperties.ptProgram, "currentFlag");
		physShaderProperties.ptPartition = glGetUniformLocation(physShaderProperties.ptProgram, "part");
		physShaderProperties.ptRNG = glGetUniformLocation(physShaderProperties.ptProgram, "rng");
		physShaderProperties.ptBlockType = glGetUniformLocation(physShaderProperties.ptProgram, "blockType");
		physShaderProperties.ptBlockSize = glGetUniformLocation(physShaderProperties.ptProgram, "blockSize");
		physShaderProperties.ptPlaceBlock = glGetUniformLocation(physShaderProperties.ptProgram, "placeBlock");
		physShaderProperties.ptDimension = glGetUniformLocation(physShaderProperties.ptProgram, "dimension");
		//physShaderProperties.ptMaxVelocity = glGetUniformLocation(ptProgram, "maxVelocity");

		glUseProgram(physShaderProperties.ptProgram);
		glUniform1ui(physShaderProperties.ptDimension, physProperties.dimension);
		//glUniform1ui(ptMaxVelocity, maxVelocity);
	}
}

void App::loadRTShader() {
	GLuint rtCompShader = voxgl::createShader("./shaders/dda.comp", GL_COMPUTE_SHADER);
	std::vector<GLuint> rtShaders;
	rtShaders.emplace_back(rtCompShader);

	GLint shaderRef = voxgl::createProgram(rtShaders);
	if (shaderRef != -1) {
		renderShaderProperties.rtProgram = shaderRef;

		renderShaderProperties.rtResolution = glGetUniformLocation(renderShaderProperties.rtProgram, "resolution");
		renderShaderProperties.rtCamPos = glGetUniformLocation(renderShaderProperties.rtProgram, "cameraPos");
		renderShaderProperties.rtCamMat = glGetUniformLocation(renderShaderProperties.rtProgram, "cameraMat");
		renderShaderProperties.rtDimension = glGetUniformLocation(renderShaderProperties.rtProgram, "dimension");
		renderShaderProperties.rtBlockDist = glGetUniformLocation(renderShaderProperties.rtProgram, "blockDist");
		renderShaderProperties.rtBlockSize = glGetUniformLocation(renderShaderProperties.rtProgram, "blockSize");
		renderShaderProperties.rtPlaceBlock = glGetUniformLocation(renderShaderProperties.rtProgram, "placeBlock");
		renderShaderProperties.rtUpdateDist = glGetUniformLocation(renderShaderProperties.rtProgram, "updateDist");
		renderShaderProperties.rtDrawLines = glGetUniformLocation(renderShaderProperties.rtProgram, "drawLines");

		glUseProgram(renderShaderProperties.rtProgram);
		glUniform2i(renderShaderProperties.rtResolution, renderProperties.framebufferWidth, renderProperties.framebufferHeight);
		glUniform1ui(renderShaderProperties.rtDimension, physProperties.dimension);
	}
}


void App::run() {
	// mutex for shader uniform variables
	std::mutex dataLock;

	std::thread render_thread = logicThread(dataLock);
	inputHandler(dataLock);

	render_thread.join();
}

const std::thread App::logicThread(std::mutex &dataLock) {
	glfwMakeContextCurrent(NULL);
	std::thread logic_thread([&]() {
		glfwMakeContextCurrent(windowProperties.window.get());

		Timer renderPacer(renderProperties.fps);

		GLdouble last_refresh = glfwGetTime();
		int frames = 0;

		std::vector<unsigned int> simTimes, renderTimes, drawTimes;
		GLuint timerQueries[3];
		glGenQueries(3, timerQueries);

		auto rng = std::minstd_rand{};


		while (!glfwWindowShouldClose(windowProperties.window.get())) {
			// physics shader
			glUseProgram(physShaderProperties.ptProgram);

			dataLock.lock();
			glUniform1ui(physShaderProperties.ptBlockType, blockPlacingProperties.blockType);
			glUniform1f(physShaderProperties.ptBlockSize, blockPlacingProperties.blockSize);
			glUniform1ui(physShaderProperties.ptPlaceBlock, blockPlacingProperties.placeBlock);
			dataLock.unlock();

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[0]);
			for (int it = 0; it < physProperties.simIterations; it++) {
				glUniform1ui(physShaderProperties.ptCurrentFlag, blockPlacingProperties.currentFlag = (!blockPlacingProperties.currentFlag) * 0x80);
				glUniform1f(physShaderProperties.ptRNG, float(rng()) / float(rng.max()));

				for (int part = 0; part < 8; part++) {
					glUniform3i(physShaderProperties.ptPartition, part % 2, (part / 2) % 2, (part / 4) % 2);
					glDispatchCompute(physShaderProperties.ptWorkGroups, physShaderProperties.ptWorkGroups, physShaderProperties.ptWorkGroups);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				}
			}
			glEndQuery(GL_TIME_ELAPSED);


			// ray trace shader
			glUseProgram(renderShaderProperties.rtProgram);

			dataLock.lock();
			glUniform3f(renderShaderProperties.rtCamPos, worldObjects.player.camera.position.x, worldObjects.player.camera.position.y, worldObjects.player.camera.position.z);
			glUniformMatrix3fv(renderShaderProperties.rtCamMat, 1, GL_FALSE, glm::value_ptr(glm::transpose(worldObjects.player.camera.getMatrix())));
			glUniform1f(renderShaderProperties.rtBlockDist, blockPlacingProperties.blockDist);
			glUniform1f(renderShaderProperties.rtBlockSize, blockPlacingProperties.blockSize);
			glUniform1f(renderShaderProperties.rtPlaceBlock, blockPlacingProperties.placeBlock);
			glUniform1ui(renderShaderProperties.rtUpdateDist, blockPlacingProperties.updateDist);
			glUniform1ui(renderShaderProperties.rtDrawLines, renderProperties.drawLines);
			blockPlacingProperties.distUpdated = true;
			dataLock.unlock();

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[1]);
			glDispatchCompute(renderShaderProperties.rtX, renderShaderProperties.rtY, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glEndQuery(GL_TIME_ELAPSED);


			// draw ray trace texture to quad
			glUseProgram(renderShaderProperties.quadProgram);

			glBeginQuery(GL_TIME_ELAPSED, timerQueries[2]);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glEndQuery(GL_TIME_ELAPSED);

			glfwSwapBuffers(windowProperties.window.get());

			// get timer queries from opengl
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


			// output framerate and average times every second
			frames++;
			if (int(glfwGetTime() - last_refresh) > 0) {
				std::cout << frames << "fps" << std::endl;
				frames = 0;

				auto const simCount = static_cast<float>(simTimes.size());
				auto const renderCount = static_cast<float>(renderTimes.size());
				auto const drawCount = static_cast<float>(drawTimes.size());

				if (simCount > 0 && renderCount > 0 && drawCount > 0)
					std::cout << "sim time: " << std::reduce(simTimes.begin(), simTimes.end()) / simCount << "us\t" << "render time: " << std::reduce(renderTimes.begin(), renderTimes.end()) / renderCount << "us\t" << "draw time: " << std::reduce(drawTimes.begin(), drawTimes.end()) / drawCount << "us" << std::endl;

				simTimes.clear();
				renderTimes.clear();
				drawTimes.clear();

				last_refresh = glfwGetTime();
			}


			// Recompile shaders with 'R' key
			if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_R)) {
				loadPTShader();
				loadRTShader();
			}

			renderPacer.tick();
		}
	});

	return logic_thread;
}


void App::inputHandler(std::mutex& dataLock) {
	// Input handler
	Timer inputPacer(500);
	unsigned int timeDelta;
	glm::vec3 prevFacingRay{ 0 };
	bool prevQ = false;

	while (!glfwWindowShouldClose(windowProperties.window.get())) {
		timeDelta = inputPacer.tick();

		glfwPollEvents();
		if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(windowProperties.window.get(), GLFW_TRUE);

		bool cameraChanged = false;

		dataLock.lock();

		worldObjects.player.handleInputs(windowProperties.window);
		worldObjects.player.update(timeDelta);

		glm::vec3 facingRay = worldObjects.player.camera.facingRay();

		if (blockPlacingProperties.distUpdated) {
			if (facingRay != prevFacingRay || glm::any(glm::greaterThan(glm::abs(worldObjects.player.velocity), glm::vec3(0)))) {
				blockPlacingProperties.updateDist = 1;
				blockPlacingProperties.distUpdated = false;
			}
			else
				blockPlacingProperties.updateDist = 0;
		}


		// block distance
		if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_EQUAL) == GLFW_PRESS) {
			blockPlacingProperties.blockDist = glm::min(blockPlacingProperties.blockDist + 0.05f, 200.f);
			blockPlacingProperties.updateDist = 1;
			blockPlacingProperties.distUpdated = false;
		}
		else if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_MINUS) == GLFW_PRESS) {
			blockPlacingProperties.blockDist = glm::max(blockPlacingProperties.blockDist - 0.05f, blockPlacingProperties.blockSize);
			blockPlacingProperties.updateDist = 1;
			blockPlacingProperties.distUpdated = false;
		}


		// block type
		if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_1) == GLFW_PRESS)		blockPlacingProperties.blockType = 0;
		else if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_2) == GLFW_PRESS)	blockPlacingProperties.blockType = 1;
		else if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_3) == GLFW_PRESS)	blockPlacingProperties.blockType = 2;
		else if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_4) == GLFW_PRESS)	blockPlacingProperties.blockType = 3;


		// place block
		if (glfwGetMouseButton(windowProperties.window.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			blockPlacingProperties.placeBlock = 1;
		else
			blockPlacingProperties.placeBlock = 0;

		// toggle draw lines
		if (glfwGetKey(windowProperties.window.get(), GLFW_KEY_Q) == GLFW_PRESS) {
			if (!prevQ)
				renderProperties.drawLines = renderProperties.drawLines ? 0 : 1;

			prevQ = true;
		}
		else
			prevQ = false;

		dataLock.unlock();


		prevFacingRay = facingRay;
	}
}
