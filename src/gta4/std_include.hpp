#pragma once

constexpr auto COMP_MOD_VERSION_MAJOR = 0;
constexpr auto COMP_MOD_VERSION_MINOR = 9;
constexpr auto COMP_MOD_VERSION_PATCH = 3;

//#define LOG_SHADERPRESETS

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#define NOMINMAX
#include <windows.h>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <cassert>
#include <map>
#include <set>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <xmmintrin.h>
#include <intrin.h>

#pragma comment(lib, "psapi.lib")

#pragma warning(push)
#pragma warning(disable: 26495)
#include <d3d9.h>
#include <d3dx9.h>
#pragma warning(pop)

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define AssertSize(x, size)								static_assert(sizeof(x) == size, STRINGIZE(x) " structure has an invalid size.")
#define STATIC_ASSERT_SIZE(struct, size)				static_assert(sizeof(struct) == size, "Size check")
#define STATIC_ASSERT_OFFSET(struct, member, offset)	static_assert(offsetof(struct, member) == offset, "Offset check")
#define XASSERT(x) if (x) MessageBoxA(HWND_DESKTOP, #x, "FATAL ERROR", MB_ICONERROR)

#include "MinHook.h"
#include "toml.hpp"
#include "bridge_remix_api.h"

#pragma warning(push)
#pragma warning(disable: 6011)
#pragma warning(disable: 28182)
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <misc/cpp/imgui_stdlib.h>
#pragma warning(pop)

#include "../shared/globals.hpp"
#include "../shared/utils/utils.hpp"
#include "../shared/utils/vector.hpp"
#include "../shared/utils/hooking.hpp"
#include "../shared/utils/memory.hpp"
#include "../shared/utils/function.hpp"
#include "../shared/common/console.hpp"
#include "../shared/common/loader.hpp"
#include "game/structs.hpp"
#include "game/game.hpp"
#include "gta4.hpp"

using namespace std::literals;