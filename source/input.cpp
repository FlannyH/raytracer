#include "input.h"

namespace input {
	static bool keys_new[(size_t)Key::n_keys];
	static bool keys_curr[(size_t)Key::n_keys];
	static bool keys_prev[(size_t)Key::n_keys];

	Key glfw_to_key(int key) {
		// A to Z
		if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
			return (Key)(key - GLFW_KEY_A + (int)Key::a);
		}
		// 0 to 9
		if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
			return (Key)(key - GLFW_KEY_0 + (int)Key::_0);
		}
		// F1 to F12
		if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
			return (Key)(key - GLFW_KEY_F1 + (int)Key::f1);
		}
		// Misc
		if (key == GLFW_KEY_SPACE) return Key::space;
		if (key == GLFW_KEY_ESCAPE) return Key::escape;
		if (key == GLFW_KEY_ENTER) return Key::enter;
		if (key == GLFW_KEY_TAB) return Key::tab;
		if (key == GLFW_KEY_LEFT_SHIFT) return Key::left_shift;
		if (key == GLFW_KEY_LEFT_CONTROL) return Key::left_control;
		if (key == GLFW_KEY_LEFT_ALT) return Key::left_alt;
		if (key == GLFW_KEY_RIGHT_SHIFT) return Key::right_shift;
		if (key == GLFW_KEY_RIGHT_CONTROL) return Key::right_control;
		if (key == GLFW_KEY_RIGHT_ALT) return Key::right_alt;
		if (key == GLFW_KEY_UP) return Key::up;
		if (key == GLFW_KEY_DOWN) return Key::down;
		if (key == GLFW_KEY_LEFT) return Key::left;
		if (key == GLFW_KEY_RIGHT) return Key::right;
	}

	void input_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		switch (action) {
			case GLFW_PRESS:
				keys_new[(size_t)glfw_to_key(key)] = true;
				break;
			case GLFW_RELEASE:
				keys_new[(size_t)glfw_to_key(key)] = false;
				break;
			default:
				break;
		}
	}

	void init(GLFWwindow* window) {
		glfwSetKeyCallback(window, input_callback);
	}
	void update() {
		// Copy new state to curr, and curr to prev - this way curr and prev are stable across the entire frame, while new is being updated by the callback
		memcpy(keys_prev, keys_curr, sizeof(keys_prev));
		memcpy(keys_curr, keys_new, sizeof(keys_prev));
	}
	bool key_held(Key key)
	{
		return keys_curr[(size_t)key];
	}
	bool key_pressed(Key key)
	{
		auto curr = keys_curr[(size_t)key];
		auto prev = keys_prev[(size_t)key];
		return curr && !prev;
	}
	bool key_released(Key key)
	{
		auto curr = keys_curr[(size_t)key];
		auto prev = keys_prev[(size_t)key];
		return prev && !curr;
	}
}