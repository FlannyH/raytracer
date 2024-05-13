#include "input.h"

namespace input {
	static bool keys_new[(size_t)Key::n_keys];
	static bool keys_curr[(size_t)Key::n_keys];
	static bool keys_prev[(size_t)Key::n_keys];
	static bool mouse_buttons_new[(size_t)MouseButton::n_buttons];
	static bool mouse_buttons_curr[(size_t)MouseButton::n_buttons];
	static bool mouse_buttons_prev[(size_t)MouseButton::n_buttons];
	static double mouse_x_new = 0.0;
	static double mouse_y_new = 0.0;
	static double mouse_x_curr = 0.0;
	static double mouse_y_curr = 0.0;
	static double mouse_x_prev = 0.0;
	static double mouse_y_prev = 0.0;
	static double mouse_scroll_x_new = 0.0;
	static double mouse_scroll_y_new = 0.0;
	static double mouse_scroll_x_curr = 0.0;
	static double mouse_scroll_y_curr = 0.0;
	static double mouse_scroll_x_prev = 0.0;
	static double mouse_scroll_y_prev = 0.0;

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

	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

	static void cursor_callback(GLFWwindow* window, double xpos, double ypos) {
		mouse_x_new = xpos;
		mouse_y_new = ypos;
	}

	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
		MouseButton mouse_button = MouseButton::left;
		switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT:
			mouse_button = MouseButton::left;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			mouse_button = MouseButton::right;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			mouse_button = MouseButton::middle;
			break;
		}

		switch (action) {
		case GLFW_PRESS:
			mouse_buttons_new[(size_t)mouse_button] = true;
			break;
		case GLFW_RELEASE:
			mouse_buttons_new[(size_t)mouse_button] = false;
			break;
		}
	}

	static void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
		mouse_scroll_x_new += xoffset;
		mouse_scroll_y_new += yoffset;
	}

	void init(GLFWwindow* window) {
		glfwSetKeyCallback(window, key_callback);
		glfwSetCursorPosCallback(window, cursor_callback);
		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetScrollCallback(window, mouse_scroll_callback);
	}

	void update() {
		// Copy new state to curr, and curr to prev - this way curr and prev are stable across the entire frame, while new is being updated by the callback
		memcpy(keys_prev, keys_curr, sizeof(keys_prev));
		memcpy(keys_curr, keys_new, sizeof(keys_prev));
		memcpy(mouse_buttons_prev, mouse_buttons_curr, sizeof(mouse_buttons_prev));
		memcpy(mouse_buttons_curr, mouse_buttons_new, sizeof(mouse_buttons_prev));
		mouse_x_prev = mouse_x_curr;
		mouse_x_curr = mouse_x_new;
		mouse_y_prev = mouse_y_curr;
		mouse_y_curr = mouse_y_new;
		mouse_scroll_x_prev = mouse_scroll_x_curr;
		mouse_scroll_x_curr = mouse_scroll_x_new;
		mouse_scroll_y_prev = mouse_scroll_y_curr;
		mouse_scroll_y_curr = mouse_scroll_y_new;
	}

	bool key_held(Key key) {
		return keys_curr[(size_t)key];
	}

	bool key_pressed(Key key) {
		auto curr = keys_curr[(size_t)key];
		auto prev = keys_prev[(size_t)key];
		return curr && !prev;
	}

	bool key_released(Key key) {
		auto curr = keys_curr[(size_t)key];
		auto prev = keys_prev[(size_t)key];
		return prev && !curr;
	}

	bool mouse_button(MouseButton mouse_button) {
		return mouse_buttons_curr[(size_t)mouse_button];
	}

	bool mouse_button_down(MouseButton mouse_button) {
		auto curr = mouse_buttons_curr[(size_t)mouse_button];
		auto prev = mouse_buttons_prev[(size_t)mouse_button];
		return curr && !prev;
	}

	bool mouse_button_up(MouseButton mouse_button) {
		auto curr = mouse_buttons_curr[(size_t)mouse_button];
		auto prev = mouse_buttons_prev[(size_t)mouse_button];
		return prev && !curr;
	}

	glm::vec2 mouse_scroll() {
		return glm::vec2(mouse_scroll_x_curr - mouse_scroll_x_prev, mouse_scroll_y_curr - mouse_scroll_y_prev);
	}

	glm::vec2 mouse_position() {
		return glm::vec2(mouse_x_curr, mouse_y_curr);
	}

	glm::vec2 mouse_movement() {
		return glm::vec2(mouse_x_curr - mouse_x_prev, mouse_y_curr - mouse_y_prev);
	}
}
