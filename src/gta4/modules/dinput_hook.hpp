#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace gta4
{
	class dinput final : public shared::common::loader::component_module
	{
	public:
		dinput();

		static inline dinput* p_this = nullptr;
		static dinput* get() { return p_this; }

		struct WndMsg
		{
			HWND hWnd;
			uint32_t msg;
			uint32_t wParam;
			uint32_t lParam;
		};

		void updateWindowSize();

		template<typename T>
		void updateMouseState(const T* state, bool isAbsoluteAxis);
		void updateKeyState(LPBYTE KS);

		IDirectInputDevice8* mouse_device = nullptr;
		IDirectInputDevice8* keyboard_device = nullptr;
		bool KeyboardDeviceStateUsed = false;
		DWORD MouseAxisMode = DIPROPAXISMODE_REL;

		LONG s_windowWidth = 3840;
		LONG s_windowHeight = 2160;

	private:
		// Last known key state
		BYTE s_KS[256] = { 0 };

		// Last known mouse state
		BYTE s_mouseButtons[8] = { 0 };
		LONG s_mouseX = 0;
		LONG s_mouseY = 0;

		// Last sent messages
		WndMsg s_mouseMove = { 0 };
		WndMsg s_mouseLButton = { 0 };
		WndMsg s_mouseRButton = { 0 };
		WndMsg s_mouseWheel = { 0 };

		bool m_initialized = false;
	};
}
