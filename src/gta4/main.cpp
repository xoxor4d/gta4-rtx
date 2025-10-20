#include "std_include.hpp"
#include <psapi.h>

//#include "backends/imgui_impl_opengl3_loader.h"
#include "shared/common/flags.hpp"
#include "modules/d3d9ex.hpp"
#include "modules/game_settings.hpp"
#include "modules/imgui.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

// Helper: Load PNG to GL texture
bool LoadTextureFromFile(const char* filename, GLuint* out_texture) {
	int width, height, channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);  // Force RGBA
	if (!data) {
		OutputDebugStringA("Failed to load texture: ");
		OutputDebugStringA(filename);
		OutputDebugStringA("\n");
		return false;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	//glGenerateMipmap(GL_TEXTURE_2D);


	stbi_image_free(data);
	*out_texture = tex;
	return true;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

// Map VK_xxx to ImGuiKey_xxx.
static ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_TAB: return ImGuiKey_Tab;
	case VK_LEFT: return ImGuiKey_LeftArrow;
	case VK_RIGHT: return ImGuiKey_RightArrow;
	case VK_UP: return ImGuiKey_UpArrow;
	case VK_DOWN: return ImGuiKey_DownArrow;
	case VK_PRIOR: return ImGuiKey_PageUp;
	case VK_NEXT: return ImGuiKey_PageDown;
	case VK_HOME: return ImGuiKey_Home;
	case VK_END: return ImGuiKey_End;
	case VK_INSERT: return ImGuiKey_Insert;
	case VK_DELETE: return ImGuiKey_Delete;
	case VK_BACK: return ImGuiKey_Backspace;
	case VK_SPACE: return ImGuiKey_Space;
	case VK_RETURN: return ImGuiKey_Enter;
	case VK_ESCAPE: return ImGuiKey_Escape;
	case VK_OEM_7: return ImGuiKey_Apostrophe;
	case VK_OEM_COMMA: return ImGuiKey_Comma;
	case VK_OEM_MINUS: return ImGuiKey_Minus;
	case VK_OEM_PERIOD: return ImGuiKey_Period;
	case VK_OEM_2: return ImGuiKey_Slash;
	case VK_OEM_1: return ImGuiKey_Semicolon;
	case VK_OEM_PLUS: return ImGuiKey_Equal;
	case VK_OEM_4: return ImGuiKey_LeftBracket;
	case VK_OEM_5: return ImGuiKey_Backslash;
	case VK_OEM_6: return ImGuiKey_RightBracket;
	case VK_OEM_3: return ImGuiKey_GraveAccent;
	case VK_CAPITAL: return ImGuiKey_CapsLock;
	case VK_SCROLL: return ImGuiKey_ScrollLock;
	case VK_NUMLOCK: return ImGuiKey_NumLock;
	case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
	case VK_PAUSE: return ImGuiKey_Pause;
	case VK_NUMPAD0: return ImGuiKey_Keypad0;
	case VK_NUMPAD1: return ImGuiKey_Keypad1;
	case VK_NUMPAD2: return ImGuiKey_Keypad2;
	case VK_NUMPAD3: return ImGuiKey_Keypad3;
	case VK_NUMPAD4: return ImGuiKey_Keypad4;
	case VK_NUMPAD5: return ImGuiKey_Keypad5;
	case VK_NUMPAD6: return ImGuiKey_Keypad6;
	case VK_NUMPAD7: return ImGuiKey_Keypad7;
	case VK_NUMPAD8: return ImGuiKey_Keypad8;
	case VK_NUMPAD9: return ImGuiKey_Keypad9;
	case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
	case VK_DIVIDE: return ImGuiKey_KeypadDivide;
	case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
	case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
	case VK_ADD: return ImGuiKey_KeypadAdd;
	case IM_VK_KEYPAD_ENTER: return ImGuiKey_KeypadEnter;
	case VK_LSHIFT: return ImGuiKey_LeftShift;
	case VK_LCONTROL: return ImGuiKey_LeftCtrl;
	case VK_LMENU: return ImGuiKey_LeftAlt;
	case VK_LWIN: return ImGuiKey_LeftSuper;
	case VK_RSHIFT: return ImGuiKey_RightShift;
	case VK_RCONTROL: return ImGuiKey_RightCtrl;
	case VK_RMENU: return ImGuiKey_RightAlt;
	case VK_RWIN: return ImGuiKey_RightSuper;
	case VK_APPS: return ImGuiKey_Menu;
	case '0': return ImGuiKey_0;
	case '1': return ImGuiKey_1;
	case '2': return ImGuiKey_2;
	case '3': return ImGuiKey_3;
	case '4': return ImGuiKey_4;
	case '5': return ImGuiKey_5;
	case '6': return ImGuiKey_6;
	case '7': return ImGuiKey_7;
	case '8': return ImGuiKey_8;
	case '9': return ImGuiKey_9;
	case 'A': return ImGuiKey_A;
	case 'B': return ImGuiKey_B;
	case 'C': return ImGuiKey_C;
	case 'D': return ImGuiKey_D;
	case 'E': return ImGuiKey_E;
	case 'F': return ImGuiKey_F;
	case 'G': return ImGuiKey_G;
	case 'H': return ImGuiKey_H;
	case 'I': return ImGuiKey_I;
	case 'J': return ImGuiKey_J;
	case 'K': return ImGuiKey_K;
	case 'L': return ImGuiKey_L;
	case 'M': return ImGuiKey_M;
	case 'N': return ImGuiKey_N;
	case 'O': return ImGuiKey_O;
	case 'P': return ImGuiKey_P;
	case 'Q': return ImGuiKey_Q;
	case 'R': return ImGuiKey_R;
	case 'S': return ImGuiKey_S;
	case 'T': return ImGuiKey_T;
	case 'U': return ImGuiKey_U;
	case 'V': return ImGuiKey_V;
	case 'W': return ImGuiKey_W;
	case 'X': return ImGuiKey_X;
	case 'Y': return ImGuiKey_Y;
	case 'Z': return ImGuiKey_Z;
	case VK_F1: return ImGuiKey_F1;
	case VK_F2: return ImGuiKey_F2;
	case VK_F3: return ImGuiKey_F3;
	case VK_F4: return ImGuiKey_F4;
	case VK_F5: return ImGuiKey_F5;
	case VK_F6: return ImGuiKey_F6;
	case VK_F7: return ImGuiKey_F7;
	case VK_F8: return ImGuiKey_F8;
	case VK_F9: return ImGuiKey_F9;
	case VK_F10: return ImGuiKey_F10;
	case VK_F11: return ImGuiKey_F11;
	case VK_F12: return ImGuiKey_F12;
	case VK_F13: return ImGuiKey_F13;
	case VK_F14: return ImGuiKey_F14;
	case VK_F15: return ImGuiKey_F15;
	case VK_F16: return ImGuiKey_F16;
	case VK_F17: return ImGuiKey_F17;
	case VK_F18: return ImGuiKey_F18;
	case VK_F19: return ImGuiKey_F19;
	case VK_F20: return ImGuiKey_F20;
	case VK_F21: return ImGuiKey_F21;
	case VK_F22: return ImGuiKey_F22;
	case VK_F23: return ImGuiKey_F23;
	case VK_F24: return ImGuiKey_F24;
	case VK_BROWSER_BACK: return ImGuiKey_AppBack;
	case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
	default: return ImGuiKey_None;
	}
}


namespace gta4
{
	// Globals
	HANDLE g_hOverlayThread = NULL;
	HWND g_hOverlayWnd = NULL;
	HGLRC g_hGLRC = NULL;
	HDC g_hDC = NULL;
	//bool g_bMenuVisible = true;
	

	
	

	// Forward declarations
	LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void RenderImGui();


	void UpdateOverlayPosition(HWND hGameWnd)
	{
		if (!IsWindow(hGameWnd)) return;
		RECT rc;
		if (!GetWindowRect(hGameWnd, &rc)) return;
		SetWindowPos(g_hOverlayWnd, HWND_TOP, rc.left, rc.top,
			rc.right - rc.left, rc.bottom - rc.top,
			SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_SHOWWINDOW);  // No focus steal
	}



	static bool prevKeys[256] = { false };  // Track prev state for all VKeys
	static bool prevMouseButtons[5] = { false };  // 0=LMB,1=RMB,2=MMB,3/4=extra
	//static ImVec2 prevMousePos = ImVec2(-1, -1);

	// Overlay Thread Proc
	DWORD WINAPI OverlayThreadProc(LPVOID lpParam)
	{
		// Find game window
		HWND hGameWnd = shared::globals::main_window; //FindWindowA("GameClassName", NULL);  // Replace with actual

		// Register window class
		WNDCLASSEX wc = { sizeof(wc), CS_CLASSDC, OverlayWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"OverlayClass", NULL };
		RegisterClassEx(&wc);

		RECT rcInitial;
		GetWindowRect(hGameWnd, &rcInitial);  // Screen coords
		//g_hOverlayWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		//	L"OverlayClass", L"Overlay", WS_POPUP | WS_VISIBLE,
		//	rcInitial.left, rcInitial.top,
		//	rcInitial.right - rcInitial.left, rcInitial.bottom - rcInitial.top,
		//	NULL, NULL, wc.hInstance, NULL);  // No parent HWND

		g_hOverlayWnd = CreateWindowEx(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,  // Add WS_EX_TOOLWINDOW here
			L"OverlayClass", L"Overlay", WS_POPUP | WS_VISIBLE,
			rcInitial.left, rcInitial.top,
			rcInitial.right - rcInitial.left, rcInitial.bottom - rcInitial.top,
			NULL, NULL, wc.hInstance, NULL
		);

		SetLayeredWindowAttributes(g_hOverlayWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);  // Key black to transparent

		// Get DC and set up pixel format for OpenGL
		g_hDC = GetDC(g_hOverlayWnd);
		PIXELFORMATDESCRIPTOR pfd = {};  // Default-init all to zero
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cAlphaBits = 8;
		pfd.cDepthBits = 24;
		pfd.iLayerType = PFD_MAIN_PLANE;
		int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
		SetPixelFormat(g_hDC, pixelFormat, &pfd);
		g_hGLRC = wglCreateContext(g_hDC);
		wglMakeCurrent(g_hDC, g_hGLRC);


		// Init ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		imgui::get()->init_fonts();
		imgui::get()->style_xo();

		// Enable alpha blending and transparent clear
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent black

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear depth too if enabled
		glEnable(GL_BLEND);  // Ensure blending (state can reset)

		ImGui_ImplWin32_Init(g_hOverlayWnd);
		ImGui_ImplOpenGL3_Init("#version 130");  // Or "#version 120" for OpenGL2
		//ImGui::StyleColorsDark();  // Or your theme
		//ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 0.8f;  // Semi-transparent

		// Load
		if (!LoadTextureFromFile((shared::globals::root_path + "\\rtx_comp\\textures\\berry.png").c_str(), &tex::berry)) {
			std::cout << "Berry texture load failed!\n";
		}
		else {
			std::cout << "Berry texture loaded OK\n";
		}

		// Message loop with sync wait
		MSG msg = {};
		static bool lastF4State = false;
		while (true) 
		{
			// TEMP: Disable sync for testing—replace with your WaitForSingleObject if hook works
			// DWORD waitResult = WaitForSingleObject(g_hRenderEvent, 16);
			// if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_TIMEOUT) { ... }

			// Simple async loop for now
			while (PeekMessage(&msg, g_hOverlayWnd, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) goto Cleanup;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}


			// F4 Toggle Polling (global, works over game)
			bool currentF4State = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
			if (currentF4State && !lastF4State) {
				shared::globals::imgui_menu_open = !shared::globals::imgui_menu_open;
				LONG exStyle = GetWindowLong(g_hOverlayWnd, GWL_EXSTYLE);
				exStyle ^= shared::globals::imgui_menu_open ? WS_EX_TRANSPARENT : 0;
				SetWindowLong(g_hOverlayWnd, GWL_EXSTYLE, exStyle);
				OutputDebugStringA("F4 toggled!\n");
			}
			lastF4State = currentF4State;

			// Input Event Polling (only when visible)
			if (shared::globals::imgui_menu_open) {
				ImGuiIO& io = ImGui::GetIO();

				// Mouse Pos: Always push current (updates hover)
				POINT mousePos;
				GetCursorPos(&mousePos);
				ScreenToClient(g_hOverlayWnd, &mousePos);
				ImVec2 currMousePos((float)mousePos.x, (float)mousePos.y);
				io.AddMousePosEvent(currMousePos.x, currMousePos.y);

				// Mouse Buttons: Push events on press/release
				for (int btn = 0; btn < IM_ARRAYSIZE(prevMouseButtons); ++btn) {
					bool currDown = (GetAsyncKeyState(VK_LBUTTON + btn) & 0x8000) != 0;  // 0x01=RB, but approx for extras
					if (currDown != prevMouseButtons[btn]) {
						io.AddMouseButtonEvent(btn, currDown);
					}
					prevMouseButtons[btn] = currDown;
				}

				// Keys: Poll 0-255, push events on press/release
				for (int vk = 0; vk < 256; ++vk) {
					bool currDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
					if (currDown != prevKeys[vk]) {
						ImGuiKey imguiKey = ImGui_ImplWin32_VirtualKeyToImGuiKey(vk);  // Use ImGui's mapper
						if (imguiKey != ImGuiKey_None) {
							io.AddKeyEvent(imguiKey, currDown);
						}
					}
					prevKeys[vk] = currDown;
				}

				// Modifiers (ImGui maps these too, but explicit for reliability)
				io.AddKeyEvent(ImGuiKey_ModCtrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
				io.AddKeyEvent(ImGuiKey_ModShift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
				io.AddKeyEvent(ImGuiKey_ModAlt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
			}


			// Update position EVERY frame for responsiveness (resizes/alt-tab)
			UpdateOverlayPosition(hGameWnd);

			static bool lastGameForeground = true;

			// Visibility Check: Hide on minimize
			bool gameVisible = IsWindowVisible(hGameWnd) && !IsIconic(hGameWnd);
			if (!gameVisible) {
				ShowWindow(g_hOverlayWnd, SW_HIDE);
				lastGameForeground = false;  // Reset for re-show transition
			}
			else if (!lastGameForeground) {
				ShowWindow(g_hOverlayWnd, SW_SHOWNA);  // Show without stealing focus
			}

			// Z-Order Sync: Match game's foreground status
			
			bool currentGameForeground = (GetForegroundWindow() == hGameWnd);  // Or GetActiveWindow() if preferred
			if (currentGameForeground != lastGameForeground) {
				HWND zOrder = currentGameForeground ? HWND_TOPMOST : HWND_NOTOPMOST;
				SetWindowPos(g_hOverlayWnd, zOrder, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
				lastGameForeground = currentGameForeground;
				std::cout << (currentGameForeground ? "Overlay: TOPMOST\n" : "Overlay: NORMAL\n");
				//OutputDebugStringA(currentGameForeground ? "Overlay: TOPMOST\n" : "Overlay: NORMAL\n");
			}

			// Render block (now always, for testing)
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			static bool has_capture = false;
			// Dynamic capture: Only hold if ImGui wants it (prevents lock after click)
			ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureMouse && shared::globals::imgui_menu_open && !has_capture) {
				SetCapture(g_hOverlayWnd);
				has_capture = true;
			}
			else if (!shared::globals::imgui_menu_open && has_capture) {
				ReleaseCapture();
				has_capture = false;
			}

			// Clear to transparent (essential for non-black bg)
			glClear(GL_COLOR_BUFFER_BIT);

			if (shared::globals::imgui_menu_open) {

				//imgui::get()->devgui();

				/*ImGui::SetNextWindowPos(ImVec2(512, 256));
				ImGui::Begin("Dev Menu", &shared::globals::imgui_menu_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
				ImGui::Text("Overlay Test - Check positioning!");
				if (ImGui::Button("Close")) shared::globals::imgui_menu_open = false;

				ImGui::ShowDemoWindow();

				ImGui::End();*/
			}

			//imgui::get()->draw_debug();
			shared::globals::imgui_is_rendering = true; 
			ImGui::EndFrame();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			shared::globals::imgui_is_rendering = false;

			SwapBuffers(g_hDC);
			Sleep(8);  // Light throttle ~1000 FPS, keeps CPU low
		}

	Cleanup:
		if (tex::berry) {
			glDeleteTextures(1, &tex::berry);
			tex::berry = 0;
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		if (g_hGLRC) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_hGLRC); }
		if (g_hDC) ReleaseDC(g_hOverlayWnd, g_hDC);
		DestroyWindow(g_hOverlayWnd);
		CloseHandle(g_hRenderEvent);
		return 0;
	}


	LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (shared::globals::imgui_menu_open) {
			// Route to ImGui for any remaining input (e.g., non-polled edge cases)
			if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
				return true;
			}
		}

		// Fallback toggle if somehow focused (rare, but keeps it)
		if (msg == WM_KEYDOWN && wParam == VK_F4) {
			shared::globals::imgui_menu_open = !shared::globals::imgui_menu_open;
			LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
			exStyle ^= shared::globals::imgui_menu_open ? WS_EX_TRANSPARENT : 0;
			SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
			return 0;
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}


	std::unordered_set<HWND> wnd_class_list; // so we don't print the same window strings over and over again

	BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lParam)
	{
		DWORD window_pid, target_pid = static_cast<DWORD>(lParam);
		GetWindowThreadProcessId(hwnd, &window_pid);

		if (window_pid == target_pid && IsWindowVisible(hwnd))
		{
			char class_name[256];
			GetClassNameA(hwnd, class_name, sizeof(class_name));

			if (!wnd_class_list.contains(hwnd))
			{
				char debug_msg[256];
				wsprintfA(debug_msg, "|> HWND: %p, PID: %u, Class: %s, Visible: %d \n", hwnd, window_pid, class_name, IsWindowVisible(hwnd));
				std::cout << debug_msg;
				wnd_class_list.insert(hwnd);
			}

			if (std::string_view(class_name).contains("grcWindow"s))
			{
				shared::globals::main_window = hwnd;
				return FALSE;
			}
		}

		return TRUE;
	}

	DWORD WINAPI find_game_window_by_sha1([[maybe_unused]] LPVOID lpParam)
	{
		shared::common::console();
		std::uint32_t T = 0;

		char exe_path[MAX_PATH]; GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
		const std::string sha1 = shared::utils::hash_file_sha1(exe_path);

		std::cout << "[INIT] Waiting for window with classname containing 'grcWindow' ... \n";

		{
			while (!shared::globals::main_window)
			{
				EnumWindows(enum_windows_proc, static_cast<LPARAM>(GetCurrentProcessId()));
				if (!shared::globals::main_window) {
					Sleep(1u); T += 1u;
				}

				if (T >= 30000)
				{
					Beep(300, 100); Sleep(100); Beep(200, 100);
					shared::common::set_console_color_red(true);
					shared::common::console(); std::cout << "|> Could not find 'grcWindow' Window. Not loading RTX Compatibility Mod.\n";
					shared::common::set_console_color_default();
					return TRUE;
				}
			}
		}

		if (!shared::common::flags::has_flag("nobeep")) {
			Beep(523, 100);
		}

		//shared::utils::focus_and_lock_cursor_on_init();

		shared::common::loader::module_loader::register_module(std::make_unique<imgui>());
		g_hOverlayThread = CreateThread(NULL, 0, OverlayThreadProc, NULL, 0, NULL);
		//SetThreadPriority(g_hOverlayThread, THREAD_PRIORITY_LOWEST);  // Don't starve game

		gta4::main();
		return 0;
	}
}

RECT gRect = {};
BOOL WINAPI SetRect_hk(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
	if (gta4::game_settings::get()->fix_windowed_hud.get_as<bool>())
	{
		const auto res_setting = gta4::game_settings::get()->fix_windowed_hud_resolution.get_as<Vector2D*>();
		gRect = { xLeft, yTop, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y) };
	}
	else
	{
		gRect = { xLeft, yTop, xRight, yBottom };
	}
	
	return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

HWND gWnd;
HWND WINAPI CreateWindowExA_hk(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	if (gta4::game_settings::get()->fix_windowed_hud.get_as<bool>())
	{
		gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, 0, 0, 1920, 1080, hWndParent, hMenu, hInstance, lpParam);

		const auto res_setting = gta4::game_settings::get()->fix_windowed_hud_resolution.get_as<Vector2D*>();

		*reinterpret_cast<int*>(gta4::game::systemMetrics_xRight) = static_cast<int>(res_setting->x); // xRight - GetSystemMetrics(0) .. another but unused: 0x17ED8CC
		*reinterpret_cast<int*>(gta4::game::systemMetrics_yBottom) = static_cast<int>(res_setting->y); // xBottom - GetSystemMetrics(1) .. ^ 0x17ED8D4
	}
	else {
		gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	}

	return gWnd;
}

void on_create_game_window_hk()
{
	// delayed hooking because of FusionFix
	shared::utils::hook::set(gta4::game::import_addr__SetRect, SetRect_hk);
	shared::utils::hook::set(gta4::game::import_addr__CreateWindowExA, CreateWindowExA_hk);
}

__declspec (naked) void on_create_game_window_stub()
{
	__asm
	{
		pushad;
		call	on_create_game_window_hk;
		popad;

		push    0;
		cmovnz  ebx, edi;
		jmp		gta4::game::retn_addr__on_create_game_window_hk;
	}
}







BOOL APIENTRY DllMain(HMODULE hmodule, const DWORD ul_reason_for_call, LPVOID)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) 
	{
		shared::common::console();
		shared::globals::setup_dll_module(hmodule);
		shared::globals::setup_exe_module();
		shared::globals::setup_homepath();

		std::cout << "Launching GTAIV RTX Remix Compatiblity Mod Version [" << COMP_MOD_VERSION_MAJOR << "." << COMP_MOD_VERSION_MINOR << "." << COMP_MOD_VERSION_PATCH << "]\n";
		gta4::game::init_game_addresses();

		if (const auto MH_INIT_STATUS = MH_Initialize(); MH_INIT_STATUS != MH_STATUS::MH_OK)
		{
			shared::common::set_console_color_red(true);
			std::cout << "[!][INIT] MinHook failed to initialize with code: " << MH_INIT_STATUS << "\n";
			shared::common::set_console_color_default();
			return TRUE;
		}

		shared::common::loader::module_loader::register_module(std::make_unique<gta4::d3d9ex>());
		shared::common::loader::module_loader::register_module(std::make_unique<gta4::game_settings>());

		// we have to hook SetRect and CreateWindowExA after FusionFix (disables FusionFix' hooks)
		shared::utils::hook(gta4::game::hk_addr__on_create_game_window_hk, on_create_game_window_stub, HOOK_JUMP).install()->quick();

		// allow actual commandline args + commandline.txt
		shared::utils::hook::nop(gta4::game::nop_addr__allow_commandline01, 6);
		shared::utils::hook::conditional_jump_to_jmp(gta4::game::jmp_addr__allow_commandline02);

		if (const auto t = CreateThread(nullptr, 0, gta4::find_game_window_by_sha1, nullptr, 0, nullptr); t) {
			CloseHandle(t);
		}
	}

	return TRUE;
}