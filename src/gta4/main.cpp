#include "std_include.hpp"
#include <psapi.h>

#include "shared/common/flags.hpp"
#include "modules/d3d9ex.hpp"
#include "modules/game_settings.hpp"

namespace gta4
{
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

		gta4::main();
		return 0;
	}
}

RECT gRect = {};
BOOL WINAPI SetRect_hk(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
	if (*gta4::game::ms_bWindowed)
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
	if (*gta4::game::ms_bWindowed)
	{
		const auto res_setting = gta4::game_settings::get()->fix_windowed_hud_resolution.get_as<Vector2D*>();
		gWnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, 0, 0, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y), hWndParent, hMenu, hInstance, lpParam);

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