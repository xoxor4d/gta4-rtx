/*
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#pragma once

// remix_c.h throws a #error if not targeting x64 arch, because RemixAPI does not
// work on 32-bit arches. However, we are only using the header for its types
// so this should be fine to circumvent
#if _WIN64 != 1
#define REMIX_ALLOW_X86
#endif
#include <remix/remix_c.h>
#if _WIN64 != 1
#undef REMIX_ALLOW_X86
#endif
#include <windows.h>

#if defined(__WINE__)
  #define DLLEXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
  #define DLLEXPORT
#else
  #define DLLEXPORT __declspec(dllexport)
#endif

typedef void(__cdecl* PFN_remixapi_BridgeCallback)(void);
typedef remixapi_ErrorCode(DLLEXPORT REMIXAPI_CALL* PFN_remixapi_RegisterCallbacks)(
  PFN_remixapi_BridgeCallback beginSceneCallback,
  PFN_remixapi_BridgeCallback endSceneCallback,
  PFN_remixapi_BridgeCallback presentCallback
);

#ifdef __cplusplus
namespace remixapi {

namespace exported_func_name {
static constexpr char initRemixApi[] = "remixapi_InitializeLibrary";
static constexpr char registerCallbacks[] = "remixapi_RegisterCallbacks";
}

inline remixapi_ErrorCode bridge_initRemixApi(remixapi_Interface* out_remixInterface, bool is_asi = false) {
  HMODULE hModule = is_asi ? GetModuleHandleA("d3d9.asi") : GetModuleHandleA("d3d9.dll");
  if (hModule) {
    PFN_remixapi_InitializeLibrary const pfn_Initialize =
      (PFN_remixapi_InitializeLibrary)GetProcAddress(hModule, exported_func_name::initRemixApi);
    if (!pfn_Initialize) {
      return REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE;
    }
  
    const remixapi_InitializeLibraryInfo initInfo{
      REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO,
      nullptr,
      REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,REMIXAPI_VERSION_MINOR,REMIXAPI_VERSION_PATCH)};
    remixapi_Interface remixInterface = { 0 };
    const remixapi_ErrorCode status = pfn_Initialize(&initInfo, &remixInterface);
    if (status == REMIXAPI_ERROR_CODE_SUCCESS) {
      *out_remixInterface = remixInterface;
    }
    return status;
  }
  return REMIXAPI_ERROR_CODE_LOAD_LIBRARY_FAILURE;
}

inline remixapi_ErrorCode bridge_setRemixApiCallbacks(
  PFN_remixapi_BridgeCallback beginSceneCallback = nullptr,
  PFN_remixapi_BridgeCallback endSceneCallback = nullptr,
  PFN_remixapi_BridgeCallback presentCallback = nullptr) {

  HMODULE hModule = GetModuleHandleA("d3d9.dll");
  if (hModule) {
    PFN_remixapi_RegisterCallbacks const pfn_RegisterCallbacks =
      (PFN_remixapi_RegisterCallbacks)GetProcAddress(hModule, exported_func_name::registerCallbacks);
    if (!pfn_RegisterCallbacks) {
      return REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE;
    }
    const remixapi_ErrorCode status = pfn_RegisterCallbacks(
      beginSceneCallback, endSceneCallback, presentCallback);
    return status;
  }
  return REMIXAPI_ERROR_CODE_LOAD_LIBRARY_FAILURE;
}

}
#endif
