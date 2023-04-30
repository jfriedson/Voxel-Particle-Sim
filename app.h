#pragma once

#include <voxgl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/noise.hpp>

#include <mutex>

#include <player.h>


class App
{
public:
	static App& getInstance(void) {
		static App instance;
		return instance;
	}

	App(const App&) = delete;
	App& operator=(const App&) = delete;

	~App();

	void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	void run();


private:
	struct {
		const int windowWidth = 1280, windowHeight = 720;
		voxgl::GLFWwindow_ptr window;
	} windowProperties;

	struct {
		const int fps = 60;							// rendering frame rate
		glm::uint drawLines = 0;					// toggle to render cube grid with lines

		int framebufferWidth, framebufferHeight;	// screen render target dimensions
	} renderProperties;

	struct {
		const GLuint rtWork = 16;	// ray cast in groups of 16x16 pixels
		GLuint rtX, rtY;

		GLuint quadVAO;
		GLuint rtTexture;

		GLuint quadProgram;
		GLuint rtProgram;

		GLuint rtResolution;
		GLuint rtCamPos;
		GLuint rtCamMat;
		GLuint rtDimension;
		GLuint rtBlockDist;
		GLuint rtBlockSize;
		GLuint rtPlaceBlock;
		GLuint rtUpdateDist;
		GLuint rtDrawLines;
	} renderShaderProperties;


	struct {
		const glm::uint dimension = 256;	// edge dimension of world cube
		const int simIterations = 4;		// total world steps between renders
	} physProperties;

	struct {
		const glm::uint simLocalDim = 8;				// 8 is the maximum work group dimension for a gtx 1080
		const glm::uint maxVelocity = simLocalDim / 2;	// max velocity is limited by shader work group size
		const glm::uint simSpacing = 2 * simLocalDim;	// space between cubes acted on by shader workgroups - the gap prevents race conditions

		GLuint ptWorkGroups;	// work groups along each cube dimension

		GLuint ptProgram;

		GLuint ptCurrentFlag;
		GLuint ptPartition;
		GLuint ptRNG;
		GLuint ptBlockType;
		GLuint ptBlockSize;
		GLuint ptPlaceBlock;
		GLuint ptDimension;
		//GLuint ptMaxVelocity;
	} physShaderProperties;


	struct {
		glm::uint currentFlag = 0;		// used to update shaders on changed state
		glm::uint placeBlock = 0;		// blocks should be placed currently
		glm::uint blockType = 1;		// type of block to place
		glm::float32 blockSize = 4.f;	// radius filled by block placement
		glm::float32 blockDist = 50.f;	// distance from screen to place blocks
		glm::uint updateDist = 1;		// render shader calculates block placement, this signals whether it should
		bool distUpdated = true;

		GLuint blockLocation;
	} blockPlacingProperties;


	struct {
		GLuint gridTexture;
	} sharedShaderProperties;


	struct {
		Player player;
	} worldObjects;


	App();

	void createWindow();

	void setupShaders();
	void setupRenderShader();
	void setupPhysicsShader();

	void setupObjects();

	void loadPTShader();
	void loadRTShader();

	const std::thread logicThread(std::mutex& dataLock);
	void inputHandler(std::mutex& dataLock);
};

