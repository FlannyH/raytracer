#pragma once
#include "glfw/glfw3.h"
#include <memory>
#include "device.h"

namespace input {
	enum class Key {
		a,b,c,d,e,f,g,
		h,i,j,k,l,m,n,o,p,
		q,r,s,t,u,v,
		w,x,y,z,

		space,
		escape,
		enter,
		tab,
		left_shift,
		left_control,
		left_alt,
		right_shift,
		right_control,
		right_alt,

		_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,
		up,down,left,right,

		f1,f2,f3,f4,
		f5,f6,f7,f8,
		f9,f10,f11,f12,

		n_keys
	};

	enum class MouseButton {
		left,
		right,
		middle
	};

	void init(GLFWwindow* window);
	void update();

	// Keyboard
	bool key_held(Key key);
	bool key_pressed(Key key);
	bool key_released(Key key);
}