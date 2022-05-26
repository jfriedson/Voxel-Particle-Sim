#include "app.h"



float blockDist = 50.f;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	blockDist = glm::min(glm::max(blockDist + float(yoffset), 0.5f), 100.f);
}


App::App() : dimension(256) {
	std::srand(time(0));

	window = voxgl::createWindow("3D Powder Toy", windowWidth, windowHeight);

	glfwSetScrollCallback(window, scroll_callback);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

App::~App() {
	voxgl::destroyWindow(window);
}

void App::loadPTShader(GLuint& ptProgram) {
	GLuint ptCompShader = voxgl::createShader("./shaders/powdertoy.cs", GL_COMPUTE_SHADER);
	std::vector<GLuint> ptShaders;
	ptShaders.emplace_back(ptCompShader);
	ptProgram = voxgl::createProgram(ptShaders);
}

void App::loadRTShader(GLuint& rtProgram) {
	GLuint rtCompShader = voxgl::createShader("./shaders/dda.cs", GL_COMPUTE_SHADER);
	std::vector<GLuint> rtShaders;
	rtShaders.emplace_back(rtCompShader);
	rtProgram = voxgl::createProgram(rtShaders);
}

void App::run() {
	// create window objects
	int framebufferWidth, framebufferHeight;
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

	//framebufferWidth /= 2;
	//framebufferHeight /= 2;

	const GLuint work = 16;
	GLuint rtX = framebufferWidth / work + (framebufferWidth % work != 0),
		rtY = framebufferHeight / work + (framebufferHeight % work != 0);

	GLuint ptWorkGroupSize = dimension / work + (dimension % work != 0);


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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_FLOAT, NULL);
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

	GLuint resolutionUniform = glGetUniformLocation(rtProgram, "resolution");
	GLuint camPosUniform = glGetUniformLocation(rtProgram, "cameraPos");
	GLuint camMatUniform = glGetUniformLocation(rtProgram, "cameraMat");
	GLuint rtBlockDistUniform = glGetUniformLocation(rtProgram, "blockDist");
	GLuint rtPlaceBlock = glGetUniformLocation(rtProgram, "placeBlock");

	glUseProgram(rtProgram);
	glUniform2i(resolutionUniform, framebufferWidth, framebufferHeight);

	glUniform1f(rtBlockDistUniform, blockDist);
	glUniform1ui(rtPlaceBlock, placeBlock);



	// powder toy shader
	GLuint ptProgram;
	loadPTShader(ptProgram);

	GLuint ptBlockType = glGetUniformLocation(ptProgram, "blockType");
	GLuint ptPlaceBlock = glGetUniformLocation(ptProgram, "placeBlock");

	glUseProgram(ptProgram);

	glm::uint blockType = 1;
	glUniform1ui(ptBlockType, blockType);

	//bool blockPlaced = false;
	glUniform1ui(ptPlaceBlock, placeBlock);

	GLuint ptTimeUniform = glGetUniformLocation(ptProgram, "time");
	glUniform1ui(ptTimeUniform, 83U);

	//GLuint ptRandomUniform = glGetUniformLocation(ptProgram, "random");
	//glUniform1ui(ptRandomUniform, 383U);


	// block location for insertion/deletetion
	GLuint blockLocation;
	glGenBuffers(1, &blockLocation);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockLocation);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * 3U, NULL, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockLocation);


	// create player object
	Player player;
	player.position = glm::vec3(270.f, 270.f, 270.f);
	player.camera.direction = glm::vec2(-1.5f, -0.5f);


	// create grid texture
	std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();


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
	std::cout << "GL_MAX_3D_TEXTURE_SIZE is " << size << " bytes." << std::endl;*/


	//std::cout << "grid gen " << (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime)).count() << "ms" << std::endl;
	//startTime = std::chrono::system_clock::now();


	//bool dataLock = false;
	std::mutex dataLock;

	glfwMakeContextCurrent(NULL);
	std::thread render_thread([&]() {
		glfwMakeContextCurrent(window);

		Timer renderPacer(60);

		GLdouble last_refresh = glfwGetTime();
		//int frames = 0;

		std::vector<unsigned int> simTimes, renderTimes, drawTimes;

		while (!glfwWindowShouldClose(window)) {
			startTime = std::chrono::system_clock::now();

			// physics shader
			glUseProgram(ptProgram);

			double time = glfwGetTime();
			glUniform1ui(ptTimeUniform, *reinterpret_cast<unsigned long long*>(&time));

			dataLock.lock();
			glUniform1ui(ptBlockType, blockType);
			glUniform1ui(ptPlaceBlock, placeBlock);
			/*if (placeBlock)
			{
				blockPlaced = true;
				placeBlock = 0;
			}*/
			dataLock.unlock();


			glDispatchCompute(dimension, dimension, dimension);
			//glDispatchCompute(ptWorkGroupSize, ptWorkGroupSize, ptWorkGroupSize);
			//glDispatchCompute(1, 1, 1);
			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			simTimes.push_back((std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - startTime)).count());
			startTime = std::chrono::system_clock::now();


			// ray trace shader
			glUseProgram(rtProgram);

			dataLock.lock();
			glUniform3f(camPosUniform, player.camera.position.x, player.camera.position.y, player.camera.position.z);
			glUniformMatrix3fv(camMatUniform, 1, GL_FALSE, glm::value_ptr(glm::transpose(player.camera.getMatrix())));
			glUniform1f(rtBlockDistUniform, blockDist);
			glUniform1ui(rtPlaceBlock, placeBlock);
			dataLock.unlock();

			glDispatchCompute(rtX, rtY, 1);
			glMemoryBarrier(GL_ALL_BARRIER_BITS);


			renderTimes.push_back((std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - startTime)).count());
			startTime = std::chrono::system_clock::now();

			// draw ray trace texture to quad
			glClear(GL_COLOR_BUFFER_BIT);
			glUseProgram(quadProgram);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glfwSwapBuffers(window);

			drawTimes.push_back((std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - startTime)).count());

			//frames++;
			if (int(glfwGetTime() - last_refresh) > 0) {
				//std::cout << frames << "fps" << std::endl;
				//frames = 0;

				auto const simCount = static_cast<float>(simTimes.size());
				auto const renderCount = static_cast<float>(renderTimes.size());
				auto const drawCount = static_cast<float>(drawTimes.size());

				std::cout << "sim time: " << std::reduce(simTimes.begin(), simTimes.end()) / simCount << "us\t" << "render time: " << std::reduce(renderTimes.begin(), renderTimes.end()) / renderCount << "us\t" << "draw time: " << std::reduce(drawTimes.begin(), drawTimes.end()) / drawCount << "us" << std::endl;

				simTimes.clear();
				renderTimes.clear();
				drawTimes.clear();

				last_refresh = glfwGetTime();
			}

			if (glfwGetKey(window, GLFW_KEY_R)) {
				//loadPTShader(rtProgram);
				loadRTShader(rtProgram);

				resolutionUniform = glGetUniformLocation(rtProgram, "resolution");
				camPosUniform = glGetUniformLocation(rtProgram, "cameraPos");
				camMatUniform = glGetUniformLocation(rtProgram, "cameraMat");

				glUseProgram(rtProgram);
				glUniform2i(resolutionUniform, framebufferWidth, framebufferHeight);
			}

			renderPacer.tick();
		}
	});


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
			blockType = 1;

		else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
			blockType = 2;

		else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
			blockType = 3;


		// block type
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			if(!placeBlock)
				placeBlock = 1;
			/*if (!blockPlaced && (placeBlock == 0))
			{
				placeBlock = 1;
				blockPlaced = false;
			}*/
		}
		else {
			if (placeBlock)
				placeBlock = 0;
			//if (blockPlaced)
			//	blockPlaced = false;
		}

		dataLock.unlock();
	}

	render_thread.join();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &rtTexture);
	glBindTexture(GL_TEXTURE_3D, 0);
	glDeleteTextures(1, &gridTexture);

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &quadVAO);

	glDeleteProgram(quadProgram);
	glDeleteProgram(ptProgram);
	glDeleteProgram(rtProgram);
}
