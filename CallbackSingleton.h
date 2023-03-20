#pragma once


class CallbackSingleton
{
private:
	App* app = nullptr;

public:
	static CallbackSingleton& getInstance(App* app) {
		static CallbackSingleton instance(app);
		return instance;
	}

	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
		getInstance(nullptr).scrollCallbackForward(window, xoffset, yoffset);
	}

	void scrollCallbackForward(GLFWwindow* window, double xoffset, double yoffset) {
		if (app != nullptr)
			app->scrollCallback(window, xoffset, yoffset);
	}

	CallbackSingleton(CallbackSingleton const&) = delete;
	void operator=(CallbackSingleton const&) = delete;

private:
	CallbackSingleton(App* app) {
		if (app != nullptr)
			this->app = app;
	}
};