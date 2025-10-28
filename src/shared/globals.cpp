#include "std_include.hpp"
#include <Psapi.h>

namespace shared::globals
{
	int game_version = -1;

	D3DXMATRIX IDENTITY =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	D3DXMATRIX TC_TRANSLATE_TO_CENTER =
	{
		 1.0f,  0.0f, 0.0f, 0.0f,	// identity
		 0.0f,  1.0f, 0.0f, 0.0f,	// identity
		 0.0f,  0.0f, 1.0f, 0.0f,	// identity
		-0.5f, -0.5f, 0.0f, 1.0f,	// translate to center
	};

	D3DXMATRIX TC_TRANSLATE_FROM_CENTER_TO_TOP_LEFT =
	{
		1.0f, 0.0f, 0.0f, 0.0f,	// identity
		0.0f, 1.0f, 0.0f, 0.0f,	// identity
		0.0f, 0.0f, 1.0f, 0.0f,	// identity
		0.5f, 0.5f, 0.0f, 1.0f,	// translate back to the top left corner
	};

	std::string root_path;
	HWND main_window = nullptr;

	HMODULE exe_hmodule;
	DWORD exe_module_addr;
	DWORD exe_size = 0u;

	void setup_exe_module()
	{
		exe_hmodule = GetModuleHandleA(nullptr);
		exe_module_addr = (DWORD)exe_hmodule;

		MODULEINFO moduleInfo;
		if (!GetModuleInformation(GetCurrentProcess(), exe_hmodule, &moduleInfo, sizeof(moduleInfo))) {
			std::cout << "[!] Failed to get exe module information. Error: " << GetLastError() << std::endl;
		} else {
			exe_size = moduleInfo.SizeOfImage;
		}
	}

	HMODULE dll_hmodule;
	DWORD dll_module_addr;
	void setup_dll_module(const HMODULE mod)
	{
		dll_hmodule = mod;
		dll_module_addr = (DWORD)dll_hmodule;
	}

	void setup_homepath()
	{	// init filepath var
		char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH);
		root_path = std::filesystem::path(path).parent_path().string();
	}

	IDirect3DDevice9* d3d_device = nullptr;
	IDirect3D9* d3d9_interface = nullptr;

	bool imgui_is_rendering = false;
	bool imgui_menu_open = false;
	bool imgui_allow_input_bypass = false;


	std::chrono::high_resolution_clock::time_point last_frame_time;
	float frame_time_ms = 0.0f;
}