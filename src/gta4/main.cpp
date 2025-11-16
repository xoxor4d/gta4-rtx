#include "std_include.hpp"
#include <psapi.h>

#include "shared/common/flags.hpp"
#include "modules/d3d9ex.hpp"
#include "modules/game_settings.hpp"
#include "modules/renderer.hpp"

//#define BLOCK_DISCORDHOOK // also hooks d3d

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
				wsprintfA(debug_msg, "> HWND: %p, PID: %u, Class: %s, Visible: %d \n", hwnd, window_pid, class_name, IsWindowVisible(hwnd));
				shared::common::log("Main", debug_msg, shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
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

		shared::common::log("Main", "Waiting for window with classname containing 'grcWindow'...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
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
					shared::common::log("Main", "Could not find 'grcWindow' Window. Not loading RTX Compatibility Mod.", shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
					return TRUE;
				}
			}
		}

		if (!shared::common::flags::has_flag("nobeep")) {
			Beep(523, 100);
		}

		// Wait until the first prim was rendered

		T = 0u;
		while (true)
		{
			if (g_rendered_first_primitive) {
				break;
			}

			Sleep(1u); T += 1u;
			if (T >= 6000)
			{
				shared::common::log("Main", "Drawing of first Primitive takes longer then expected.", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true, true);
				shared::common::log("Main", "Proceeding to initiate the Compatibility Mod ...\n", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
				break;
			}
		}

		gta4::main();
		return 0;
	}
}


bool g_populated_res_table = false;

BOOL WINAPI SetRect_hk(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
	RECT rect = {};

	// if manual override via game setting
	if (gta4::game_settings::get()->manual_game_resolution_enabled.get_as<bool>())
	{
		const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
		rect = { xLeft, yTop, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y) };
	}
	else
	{
		if (!g_populated_res_table)
		{
			gta4::game::PopulateAvailResolutionsArray(*gta4::game::d3d9_adapter_index); // 0x17ED930
			g_populated_res_table = true;
		}

		if (auto modes_ptr = gta4::game::avail_game_resolutions; modes_ptr->modes) // 0x1168BB0
		{
			const auto res = modes_ptr->modes[gta4::game::loaded_settings_cfg->resolution_index]; // 0x1160E80
			rect = { xLeft, yTop, (LONG)res.width, (LONG)res.height };
		}
		else
		{
			if (*gta4::game::ms_bWindowed)
			{
				// fallback
				const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
				rect = { xLeft, yTop, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y) };
			}
			else {
				rect = { xLeft, yTop, xRight, yBottom };
			}
		}
	}

	// send rect to FF
	if (gta4::game::hmodule_fusionfix)
	{
		using FusionFix_SetRect_RemixFn = void(__cdecl*)(RECT rect);
		if (const auto pFusionFix_SetRect = reinterpret_cast<FusionFix_SetRect_RemixFn>(GetProcAddress(gta4::game::hmodule_fusionfix, "SetRect_Remix"));
			pFusionFix_SetRect) 
		{
			pFusionFix_SetRect(rect);
		}
	}
	

	return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

HWND WINAPI CreateWindowExA_hk(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	gta4::game::loaded_settings_cfg->nightshadow_quality = 0u;
	gta4::game::loaded_settings_cfg->reflection_quality = 0u;
	gta4::game::loaded_settings_cfg->shadow_quality = 0u;
	gta4::game::loaded_settings_cfg->water_quality = 0u;
	gta4::game::loaded_settings_cfg->sharpness = 0u; // fix cutscene crashing issue on amd cards

	HWND wnd;

	// if manual override via game setting
	if (gta4::game_settings::get()->manual_game_resolution_enabled.get_as<bool>())
	{
		const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
		*reinterpret_cast<int*>(gta4::game::systemMetrics_xRight) = static_cast<int>(res_setting->x); // xRight - GetSystemMetrics(0)
		*reinterpret_cast<int*>(gta4::game::systemMetrics_yBottom) = static_cast<int>(res_setting->y); // xBottom - GetSystemMetrics(1)
		wnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y), hWndParent, hMenu, hInstance, lpParam);
	}
	else
	{
		// load game settings to populate modes
		if (!g_populated_res_table)
		{
			gta4::game::PopulateAvailResolutionsArray(*gta4::game::d3d9_adapter_index); // 0x17ED930
			g_populated_res_table = true;
		}

		if (auto modes_ptr = gta4::game::avail_game_resolutions; modes_ptr->modes) // 0x1168BB0
		{
			const auto res = modes_ptr->modes[gta4::game::loaded_settings_cfg->resolution_index]; // 0x1160E80
			wnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, 0, 0, static_cast<int>(res.width), static_cast<int>(res.height), hWndParent, hMenu, hInstance, lpParam);
			*reinterpret_cast<int*>(gta4::game::systemMetrics_xRight) = static_cast<int>(res.width); // xRight - GetSystemMetrics(0) .. another but unused: 0x17ED8CC
			*reinterpret_cast<int*>(gta4::game::systemMetrics_yBottom) = static_cast<int>(res.height); // xBottom - GetSystemMetrics(1) .. ^ 0x17ED8D4
		}
		else
		{
			if (*gta4::game::ms_bWindowed)
			{
				// fallback
				const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
				wnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, 0, 0, static_cast<int>(res_setting->x), static_cast<int>(res_setting->y), hWndParent, hMenu, hInstance, lpParam);

				*reinterpret_cast<int*>(gta4::game::systemMetrics_xRight) = static_cast<int>(res_setting->x); // xRight - GetSystemMetrics(0) .. another but unused: 0x17ED8CC
				*reinterpret_cast<int*>(gta4::game::systemMetrics_yBottom) = static_cast<int>(res_setting->y); // xBottom - GetSystemMetrics(1) .. ^ 0x17ED8D4
			}
			else {
				wnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
			}
		}
	}

	// send hwnd to FF
	if (gta4::game::hmodule_fusionfix)
	{
		using FusionFix_CreateWindowExA_RemixFn = void(__cdecl*)(HWND hwnd);
		if (const auto FusionFix_CreateWindowExA_Remix = reinterpret_cast<FusionFix_CreateWindowExA_RemixFn>(GetProcAddress(gta4::game::hmodule_fusionfix, "CreateWindowExA_Remix"));
			FusionFix_CreateWindowExA_Remix)
		{
			DWORD hash = (DWORD)(uintptr_t)wnd;
			shared::common::log("Main", std::format("Sending HWND to FusionFix (0x{:X})", hash));
			FusionFix_CreateWindowExA_Remix(wnd);
		}
	}

	return wnd;
}

// detoured cmdline get int func to be able to set width and height via game setting override
BOOL __fastcall commandlinearg_get_int_hk(gta4::game::cmdarg* arg, [[maybe_unused]] void* fastcall, int* ret)
{
	if (arg)
	{
		if (gta4::game_settings::get()->manual_game_resolution_enabled.get_as<bool>())
		{
			// is game checking for width but val is null?
			if (std::string_view(arg->arg_name) == "width" && (!arg->arg_val || !*arg->arg_val))
			{
				const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
				*ret = static_cast<int>(res_setting->x);
				return 1;
			}

			if (std::string_view(arg->arg_name) == "height" && (!arg->arg_val || !*arg->arg_val))
			{
				const auto res_setting = gta4::game_settings::get()->manual_game_resolution.get_as<Vector2D*>();
				*ret = static_cast<int>(res_setting->y);
				return 1;
			}
		}
		
		// original code
		if (!arg->arg_val || !*arg->arg_val) {
			return 0;
		}

		*ret = strtol(arg->arg_val, nullptr, 0);
		return 1;
	}

	return 0;
}

// delayed hook to override potential FF hooks
void on_create_game_window_hk()
{
	if (gta4::game::hmodule_fusionfix = GetModuleHandleA("GTAIV.EFLC.FusionFix.asi"); gta4::game::hmodule_fusionfix) {
		shared::common::log("Main", "Detected FusionFix", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
	}

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

#ifdef BLOCK_DISCORDHOOK

bool is_discord_dll(const char* name)
{
	if (strstr(name, "DiscordHook.dll"))
	{
		shared::common::log("Main", "Prevented 'DiscordHook.dll' from loading ...", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
		return true;
	}

	return false;
}

bool is_discord_dll_wide(const wchar_t* name)
{
	char buf[1024];
	if (const int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, buf, sizeof(buf), nullptr, nullptr); len > 0) {
		buf[len - 1] = 0;
	}

	if (strstr(buf, "DiscordHook.dll"))
	{
		shared::common::log("Main", "Prevented 'DiscordHook.dll' from loading ...", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
		return true;
	}

	return false;
}

typedef HMODULE(WINAPI* pLoadLibraryA)(LPCSTR);
pLoadLibraryA LoadLibraryA_og = LoadLibraryA;

HMODULE WINAPI LoadLibraryA_hk(LPCSTR lpFileName)
{
	if (is_discord_dll(lpFileName)) {
		return nullptr;
	}

	return LoadLibraryA_og(lpFileName);
}

typedef HMODULE(WINAPI* pLoadLibraryW)(LPCWSTR);
pLoadLibraryW LoadLibraryW_og = LoadLibraryW;

HMODULE WINAPI LoadLibraryW_hk(LPCWSTR lpFileName)
{
	if (is_discord_dll_wide(lpFileName)) {
		return nullptr;
	}

	return LoadLibraryW_og(lpFileName);
}
#endif

BOOL APIENTRY DllMain(HMODULE hmodule, const DWORD ul_reason_for_call, LPVOID)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) 
	{
		shared::common::console();
		shared::globals::setup_dll_module(hmodule);
		shared::globals::setup_exe_module();
		shared::globals::setup_homepath();

		shared::common::set_console_color_blue(true);
		std::cout << "Launching GTAIV RTX Remix Compatiblity Mod Version [" << COMP_MOD_VERSION_MAJOR << "." << COMP_MOD_VERSION_MINOR << "." << COMP_MOD_VERSION_PATCH << "]\n";
		std::cout << "> Compiled On : " + std::string(__DATE__) + " " + std::string(__TIME__) + "\n";
		std::cout << "> https://github.com/xoxor4d/gta4-rtx\n\n";
		shared::common::set_console_color_default();

		if (const auto MH_INIT_STATUS = MH_Initialize(); MH_INIT_STATUS != MH_STATUS::MH_OK)
		{
			shared::common::log("Main", std::format("MinHook failed to initialize with code: {:d}", static_cast<int>(MH_INIT_STATUS)), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return TRUE;
		}

#ifdef BLOCK_DISCORDHOOK
		MH_CreateHook(&LoadLibraryA, &LoadLibraryA_hk, reinterpret_cast<LPVOID*>(&LoadLibraryA_og));
		MH_CreateHook(&LoadLibraryW, &LoadLibraryW_hk, reinterpret_cast<LPVOID*>(&LoadLibraryW_og));
		MH_EnableHook(MH_ALL_HOOKS);
#endif

		gta4::game::init_game_addresses();

		shared::common::loader::module_loader::register_module(std::make_unique<gta4::d3d9ex>());
		shared::common::loader::module_loader::register_module(std::make_unique<gta4::game_settings>());

		// we have to hook SetRect and CreateWindowExA after FusionFix (disables FusionFix' hooks)
		shared::utils::hook(gta4::game::hk_addr__on_create_game_window_hk, on_create_game_window_stub, HOOK_JUMP).install()->quick();

		// allow actual commandline args + commandline.txt
		shared::utils::hook::nop(gta4::game::nop_addr__allow_commandline01, 6);
		shared::utils::hook::conditional_jump_to_jmp(gta4::game::jmp_addr__allow_commandline02);

		// detour cmd line get int func
		shared::utils::hook::detour(gta4::game::hk_addr__commandlinearg_get_int, &commandlinearg_get_int_hk, nullptr);

		if (const auto t = CreateThread(nullptr, 0, gta4::find_game_window_by_sha1, nullptr, 0, nullptr); t) {
			CloseHandle(t);
		}
	}

	return TRUE;
}