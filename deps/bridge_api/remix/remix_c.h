/*
 * Copyright (c) 2023-2025, NVIDIA CORPORATION. All rights reserved.
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

#ifndef REMIX_C_H_
#define REMIX_C_H_

#include <stdint.h>

#ifndef REMIX_WINAPI_NO_INCLUDE
#include <windows.h>
#endif


#ifndef REMIX_ALLOW_X86
#if _WIN64 != 1
  #error Remix API requires 64-bit for the ray tracing features.
#endif
#endif


// __stdcall convention
#define REMIXAPI_CALL __stdcall
#define REMIXAPI_PTR  REMIXAPI_CALL

#ifdef REMIX_LIBRARY_EXPORTS
  #define REMIXAPI __declspec(dllexport)
#else
  #define REMIXAPI __declspec(dllimport)
#endif // REMIX_LIBRARY_EXPORTS


#define REMIXAPI_VERSION_MAKE(major, minor, patch) ( \
    (((uint64_t)(major)) << 48) | \
    (((uint64_t)(minor)) << 16) | \
    (((uint64_t)(patch))      ) )
#define REMIXAPI_VERSION_GET_MAJOR(version) (((uint64_t)(version) >> 48) & (uint64_t)0xFFFF)
#define REMIXAPI_VERSION_GET_MINOR(version) (((uint64_t)(version) >> 16) & (uint64_t)0xFFFFFFFF)
#define REMIXAPI_VERSION_GET_PATCH(version) (((uint64_t)(version)      ) & (uint64_t)0xFFFF)

#define REMIXAPI_VERSION_MAJOR 0
#define REMIXAPI_VERSION_MINOR 6
#define REMIXAPI_VERSION_PATCH 4


// External
typedef struct IDirect3D9Ex       IDirect3D9Ex;
typedef struct IDirect3DDevice9Ex IDirect3DDevice9Ex;
typedef struct IDirect3DSurface9  IDirect3DSurface9;
typedef struct IDirect3DTexture9  IDirect3DTexture9;

#ifndef REMIX_WINAPI_NO_INCLUDE
  typedef HWND remixapi_HWND;
  typedef HMODULE remixapi_HMODULE;
  typedef FARPROC remixapi_loader_PROC;
  #define REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
  #define REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
  #define REMIX_WINAPI_MAX_PATH                         MAX_PATH
#else // if there's no 'Windows.h', then declare WINAPI types/functions manually
  typedef struct HWND__* remixapi_HWND;
  typedef struct HINSTANCE__* remixapi_HMODULE;
  typedef long long(__stdcall* remixapi_loader_PROC)();
  #define REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
  #define REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
  #define REMIX_WINAPI_MAX_PATH                         260
  #ifndef REMIX_WINAPI_NO_LIBRARY_LOADER // if loader is needed
  #ifdef __cplusplus
  extern "C" {
  #endif // __cplusplus
    __declspec(dllimport) unsigned long        __stdcall GetDllDirectoryW(unsigned long nBufferLength, wchar_t* lpBuffer);
    __declspec(dllimport) int                  __stdcall FreeLibrary(remixapi_HMODULE hLibModule);
    __declspec(dllimport) remixapi_HMODULE     __stdcall LoadLibraryW(const wchar_t* lpLibFileName);
    __declspec(dllimport) remixapi_HMODULE     __stdcall LoadLibraryExW(const wchar_t* lpLibFileName, void* hFile, unsigned long dwFlags);
    __declspec(dllimport) remixapi_loader_PROC __stdcall GetProcAddress(remixapi_HMODULE hModule, const char* lpProcName);
    __declspec(dllimport) int                  __stdcall SetDllDirectoryW(const wchar_t* lpPathName);
    __declspec(dllimport) unsigned long        __stdcall GetFullPathNameW(const wchar_t* lpFileName, unsigned long nBufferLength, wchar_t* lpBuffer, wchar_t** lpFilePart);
  #ifdef __cplusplus
  }
  #endif // __cplusplus
  #endif // !REMIX_WINAPI_NO_LIBRARY_LOADER
#endif // !REMIX_WINAPI_NO_INCLUDE


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  typedef enum remixapi_StructType {
    REMIXAPI_STRUCT_TYPE_NONE                                 = 0,
    REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO              = 1,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO                        = 2,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_PORTAL_EXT             = 3,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT        = 4,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT             = 5,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO                           = 6,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT               = 7,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_CYLINDER_EXT              = 8,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISK_EXT                  = 9,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT                  = 10,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT                = 11,
    REMIXAPI_STRUCT_TYPE_MESH_INFO                            = 12,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO                        = 13,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT    = 14,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT              = 15,
    REMIXAPI_STRUCT_TYPE_CAMERA_INFO                          = 16,
    REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT        = 17,
    REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_SUBSURFACE_EXT  = 18,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_OBJECT_PICKING_EXT     = 19,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DOME_EXT                  = 20,
    REMIXAPI_STRUCT_TYPE_LIGHT_INFO_USD_EXT                   = 21,
    REMIXAPI_STRUCT_TYPE_STARTUP_INFO                         = 22,
    REMIXAPI_STRUCT_TYPE_PRESENT_INFO                         = 23,
    REMIXAPI_STRUCT_TYPE_DEPRECATED_LEGACY_PARTICLE_SYSTEM    = 24,
    REMIXAPI_STRUCT_TYPE_TEXTURE_INFO                         = 25,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_PARTICLE_SYSTEM_EXT    = 26,
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_GPU_INSTANCING_EXT     = 27,
    REMIXAPI_STRUCT_TYPE_CAMERA_MEDIUM_INFO                   = 28,
    // NOTE: if adding a new struct, register it in 'rtx_remix_specialization.inl'
    //       and only extend this enum by appending, never adjust the order of these 
    //       as that will break backwards compatibility.
  } remixapi_StructType;

  typedef enum remixapi_ErrorCode {
    REMIXAPI_ERROR_CODE_SUCCESS                           = 0,
    REMIXAPI_ERROR_CODE_GENERAL_FAILURE                   = 1,
    // WinAPI's LoadLibrary has failed
    REMIXAPI_ERROR_CODE_LOAD_LIBRARY_FAILURE              = 2,
    REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS                 = 3,
    // Couldn't find 'remixInitialize' function in the .dll
    REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE          = 4,
    // CreateD3D9 / RegisterD3D9Device can be called only once
    REMIXAPI_ERROR_CODE_ALREADY_EXISTS                    = 5,
    // RegisterD3D9Device requires the device that was created with IDirect3DDevice9Ex, returned by CreateD3D9
    REMIXAPI_ERROR_CODE_REGISTERING_NON_REMIX_D3D9_DEVICE = 6,
    // RegisterD3D9Device was not called
    REMIXAPI_ERROR_CODE_REMIX_DEVICE_WAS_NOT_REGISTERED   = 7,
    REMIXAPI_ERROR_CODE_INCOMPATIBLE_VERSION              = 8,
    // WinAPI's SetDllDirectory has failed
    REMIXAPI_ERROR_CODE_SET_DLL_DIRECTORY_FAILURE         = 9,
    // WinAPI's GetFullPathName has failed
    REMIXAPI_ERROR_CODE_GET_FULL_PATH_NAME_FAILURE        = 10,
    REMIXAPI_ERROR_CODE_NOT_INITIALIZED                   = 11,
    // Error codes that are encoded as HRESULT, i.e. returned from D3D9 functions.
    // Look MAKE_D3DHRESULT, but with _FACD3D=0x896, instead of D3D9's 0x876
    REMIXAPI_ERROR_CODE_HRESULT_NO_REQUIRED_GPU_FEATURES      = 0x88960001,
    REMIXAPI_ERROR_CODE_HRESULT_DRIVER_VERSION_BELOW_MINIMUM  = 0x88960002,
    REMIXAPI_ERROR_CODE_HRESULT_DXVK_INSTANCE_EXTENSION_FAIL  = 0x88960003,
    REMIXAPI_ERROR_CODE_HRESULT_VK_CREATE_INSTANCE_FAIL       = 0x88960004,
    REMIXAPI_ERROR_CODE_HRESULT_VK_CREATE_DEVICE_FAIL         = 0x88960005,
    REMIXAPI_ERROR_CODE_HRESULT_GRAPHICS_QUEUE_FAMILY_MISSING = 0x88960006,
  } remixapi_ErrorCode;

  typedef uint32_t remixapi_Bool;

  typedef struct remixapi_Rect2D {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
  } remixapi_Rect2D;

  typedef struct remixapi_Float2D {
    float x;
    float y;
  } remixapi_Float2D;

  typedef struct remixapi_Float3D {
    float x;
    float y;
    float z;
  } remixapi_Float3D;

  typedef struct remixapi_Float4D {
    float x;
    float y;
    float z;
    float w;
  } remixapi_Float4D;

  typedef struct remixapi_Transform {
    float matrix[3][4];
  } remixapi_Transform;

  typedef struct remixapi_MaterialHandle_T* remixapi_MaterialHandle;
  typedef struct remixapi_MeshHandle_T* remixapi_MeshHandle;
  typedef struct remixapi_LightHandle_T* remixapi_LightHandle;
  typedef struct remixapi_TextureHandle_T* remixapi_TextureHandle;

  typedef const wchar_t* remixapi_Path;



  typedef struct remixapi_StartupInfo {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_HWND       hwnd;
    remixapi_Bool       disableSrgbConversionForOutput;
    // If true, 'dxvk_GetExternalSwapchain' can be used to retrieve a raw VkImage,
    // so the application can present it, for example by using OpenGL interop:
    // converting VkImage to OpenGL, and presenting it via OpenGL.
    // Default: false. Use VkSwapchainKHR to present frame into HWND.
    remixapi_Bool       forceNoVkSwapchain;
    remixapi_Bool       editorModeEnabled;
    // With this disabled, the user must fetch the GUI buffer using 
    // the, remixapi_dxvk_CopyRenderingOutputType, api with the, 
    // remixapi_dxvk_CopyRenderingOutputType, field set to: 'REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_GUI'
    // Otherwise the GUI will be drawn in the final color buffer.
    remixapi_Bool       combineGuiInFinalColor;
  } remixapi_StartupInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Startup)(const remixapi_StartupInfo* info);
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Shutdown)(void);



  typedef struct remixapi_MaterialInfoOpaqueEXT {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_Path       roughnessTexture;
    remixapi_Path       metallicTexture;
    float               anisotropy;
    remixapi_Float3D    albedoConstant;
    float               opacityConstant;
    float               roughnessConstant;
    float               metallicConstant;
    remixapi_Bool       thinFilmThickness_hasvalue;
    float               thinFilmThickness_value;
    remixapi_Bool       alphaIsThinFilmThickness;
    remixapi_Path       heightTexture;
    float               displaceIn;
    // If true, InstanceInfoBlendEXT is used as a source for alpha state
    remixapi_Bool       useDrawCallAlphaState;
    remixapi_Bool       blendType_hasvalue;
    int                 blendType_value;
    remixapi_Bool       invertedBlend;
    int                 alphaTestType;
    uint8_t             alphaReferenceValue;
    float               displaceOut;
  } remixapi_MaterialInfoOpaqueEXT;

  // Valid only if remixapi_MaterialInfo contains remixapi_MaterialInfoOpaqueEXT in pNext chain
  typedef struct remixapi_MaterialInfoOpaqueSubsurfaceEXT {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_Path       subsurfaceTransmittanceTexture;
    remixapi_Path       subsurfaceThicknessTexture;
    remixapi_Path       subsurfaceSingleScatteringAlbedoTexture;
    remixapi_Float3D    subsurfaceTransmittanceColor;
    float               subsurfaceMeasurementDistance;
    remixapi_Float3D    subsurfaceSingleScatteringAlbedo;
    float               subsurfaceVolumetricAnisotropy;
    remixapi_Bool       subsurfaceDiffusionProfile;
    remixapi_Float3D    subsurfaceRadius;
    float               subsurfaceRadiusScale;
    float               subsurfaceMaxSampleRadius;
    remixapi_Path       subsurfaceRadiusTexture;
  } remixapi_MaterialInfoOpaqueSubsurfaceEXT;

  typedef struct remixapi_MaterialInfoTranslucentEXT {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_Path       transmittanceTexture;
    float               refractiveIndex;
    remixapi_Float3D    transmittanceColor;
    float               transmittanceMeasurementDistance;
    remixapi_Bool       thinWallThickness_hasvalue;
    float               thinWallThickness_value;
    remixapi_Bool       useDiffuseLayer;
  } remixapi_MaterialInfoTranslucentEXT;

  typedef struct remixapi_MaterialInfoPortalEXT {
    remixapi_StructType sType;
    void*               pNext;
    uint8_t             rayPortalIndex;
    float               rotationSpeed;
  } remixapi_MaterialInfoPortalEXT;

  typedef struct remixapi_MaterialInfo {
    remixapi_StructType sType;
    void*               pNext;
    uint64_t            hash;
    remixapi_Path       albedoTexture;
    remixapi_Path       normalTexture;
    remixapi_Path       tangentTexture;
    remixapi_Path       emissiveTexture;
    float               emissiveIntensity;
    remixapi_Float3D    emissiveColorConstant;
    uint8_t             spriteSheetRow;
    uint8_t             spriteSheetCol;
    uint8_t             spriteSheetFps;
    uint8_t             filterMode;
    uint8_t             wrapModeU;
    uint8_t             wrapModeV;
  } remixapi_MaterialInfo;


  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateMaterial)(
    const remixapi_MaterialInfo*  info,
    remixapi_MaterialHandle*      out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyMaterial)(
    remixapi_MaterialHandle       handle);

  typedef struct remixapi_HardcodedVertex {
    float    position[3];
    float    normal[3];
    float    texcoord[2];
    uint32_t color;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
    uint32_t _pad3;
    uint32_t _pad4;
    uint32_t _pad5;
    uint32_t _pad6;
  } remixapi_HardcodedVertex;

  typedef struct remixapi_MeshInfoSkinning {
    uint32_t                        bonesPerVertex;
    // Each tuple of 'bonesPerVertex' float-s defines a vertex.
    // I.e. the size must be (bonesPerVertex * vertexCount).
    const float*                    blendWeights_values;
    uint32_t                        blendWeights_count;
    // Each tuple of 'bonesPerVertex' uint32_t-s defines a vertex.
    // I.e. the size must be (bonesPerVertex * vertexCount).
    const uint32_t*                 blendIndices_values;
    uint32_t                        blendIndices_count;
  } remixapi_MeshInfoSkinning;

  typedef struct remixapi_MeshInfoSurfaceTriangles {
    const remixapi_HardcodedVertex* vertices_values;
    uint64_t                        vertices_count;
    const uint32_t*                 indices_values;
    uint64_t                        indices_count;
    remixapi_Bool                   skinning_hasvalue;
    remixapi_MeshInfoSkinning       skinning_value;
    remixapi_MaterialHandle         material;
  } remixapi_MeshInfoSurfaceTriangles;

  typedef struct remixapi_MeshInfo {
    remixapi_StructType                      sType;
    void*                                    pNext;
    uint64_t                                 hash;
    const remixapi_MeshInfoSurfaceTriangles* surfaces_values;
    uint32_t                                 surfaces_count;
  } remixapi_MeshInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateMesh)(
    const remixapi_MeshInfo*  info,
    remixapi_MeshHandle*      out_handle);

  // Batched variant of CreateMesh: deep-copies the provided info and defers
  // DXVK buffer allocation / asset-replacer registration to the next render
  // thread flush point (DrawInstance, Present, AutoInstancePersistentLights).
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateMeshBatched)(
    const remixapi_MeshInfo*  info,
    remixapi_MeshHandle*      out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyMesh)(
    remixapi_MeshHandle       handle);



  typedef enum remixapi_CameraType {
    REMIXAPI_CAMERA_TYPE_WORLD,
    REMIXAPI_CAMERA_TYPE_SKY,
    REMIXAPI_CAMERA_TYPE_VIEW_MODEL,
  } remixapi_CameraType;

  typedef enum remixapi_UIState {
      REMIXAPI_UI_STATE_NONE = 0,
      REMIXAPI_UI_STATE_BASIC = 1,
      REMIXAPI_UI_STATE_ADVANCED = 2
  } remixapi_UIState;

  REMIXAPI remixapi_UIState REMIXAPI_CALL remixapi_GetUIState(void);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_SetUIState(remixapi_UIState state);

  typedef struct remixapi_CameraInfoParameterizedEXT {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_Float3D    position;
    remixapi_Float3D    forward;
    remixapi_Float3D    up;
    remixapi_Float3D    right;
    float               fovYInDegrees;
    float               aspect;
    float               nearPlane;
    float               farPlane;
  } remixapi_CameraInfoParameterizedEXT;

  typedef struct remixapi_CameraInfo {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_CameraType type;
    float               view[4][4];
    float               projection[4][4];
  } remixapi_CameraInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetupCamera)(
    const remixapi_CameraInfo* info);

  typedef struct remixapi_CameraMediumInfo {
    remixapi_StructType     sType;
    void*                   pNext;
    remixapi_MaterialHandle medium;
  } remixapi_CameraMediumInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetCameraMediumMaterial)(
    const remixapi_CameraMediumInfo* info);



#define REMIXAPI_INSTANCE_INFO_MAX_BONES_COUNT 256

  typedef struct remixapi_InstanceInfoBoneTransformsEXT {
      remixapi_StructType       sType;
      void*                     pNext;
      const remixapi_Transform* boneTransforms_values;
      uint32_t                  boneTransforms_count;
  } remixapi_InstanceInfoBoneTransformsEXT;

  typedef struct remixapi_InstanceInfoBlendEXT {
    remixapi_StructType sType;
    void*               pNext;
    remixapi_Bool       alphaTestEnabled;
    uint8_t             alphaTestReferenceValue;
    uint32_t            alphaTestCompareOp;
    remixapi_Bool       alphaBlendEnabled;
    uint32_t            srcColorBlendFactor;
    uint32_t            dstColorBlendFactor;
    uint32_t            colorBlendOp;
    uint32_t            textureColorArg1Source;
    uint32_t            textureColorArg2Source;
    uint32_t            textureColorOperation;
    uint32_t            textureAlphaArg1Source;
    uint32_t            textureAlphaArg2Source;
    uint32_t            textureAlphaOperation;
    uint32_t            tFactor;
    remixapi_Bool       isTextureFactorBlend;
    uint32_t            srcAlphaBlendFactor;
    uint32_t            dstAlphaBlendFactor;
    uint32_t            alphaBlendOp;
    uint32_t            writeMask;
    remixapi_Bool       isVertexColorBakedLighting;
  } remixapi_InstanceInfoBlendEXT;

  typedef struct remixapi_InstanceInfoObjectPickingEXT {
    remixapi_StructType            sType;
    void*                          pNext;
    // A value to write for 'RequestObjectPicking'
    uint32_t                       objectPickingValue;
  } remixapi_InstanceInfoObjectPickingEXT;

  typedef enum remixapi_InstanceCategoryBit {
    REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_UI = 1 << 0,
    REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_MATTE = 1 << 1,
    REMIXAPI_INSTANCE_CATEGORY_BIT_SKY = 1 << 2,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE = 1 << 3,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_LIGHTS = 1 << 4,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING = 1 << 5,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_MOTION_BLUR = 1 << 6,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_OPACITY_MICROMAP = 1 << 7,
    REMIXAPI_INSTANCE_CATEGORY_BIT_HIDDEN = 1 << 8,
    REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE = 1 << 9,
    REMIXAPI_INSTANCE_CATEGORY_BIT_BEAM = 1 << 10,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC = 1 << 11,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_DYNAMIC = 1 << 12,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_SINGLE_OFFSET = 1 << 13,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_NO_OFFSET = 1 << 14,
    REMIXAPI_INSTANCE_CATEGORY_BIT_ALPHA_BLEND_TO_CUTOUT = 1 << 15,
    REMIXAPI_INSTANCE_CATEGORY_BIT_TERRAIN = 1 << 16,
    REMIXAPI_INSTANCE_CATEGORY_BIT_ANIMATED_WATER = 1 << 17,
    REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL = 1 << 18,
    REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY = 1 << 19,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_BAKED_LIGHTING = 1 << 20,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ALPHA_CHANNEL = 1 << 21,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_TRANSPARENCY_LAYER = 1 << 22,
    REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE_EMITTER = 1 << 23,
    REMIXAPI_INSTANCE_CATEGORY_BIT_SMOOTH_NORMALS = 1 << 24,
    REMIXAPI_INSTANCE_CATEGORY_BIT_HAIR_CARDS = 1 << 25,
    REMIXAPI_INSTANCE_CATEGORY_BIT_VIEW_MODEL = 1 << 26,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DISABLE_BACKFACE_CULLING = 1 << 27,
  } remixapi_InstanceCategoryBit;

  typedef uint32_t remixapi_InstanceCategoryFlags;


  typedef struct remixapi_AnimatedFloat1D {
    float*     pData;
    uint32_t   numberElements;
  } remixapi_AnimatedFloat1D;
  
  typedef struct remixapi_AnimatedFloat2D {
    remixapi_Float2D*   pData;
    uint32_t            numberElements;
  } remixapi_AnimatedFloat2D;
  
  typedef struct remixapi_AnimatedFloat3D {
    remixapi_Float3D*   pData;
    uint32_t            numberElements;
  } remixapi_AnimatedFloat3D;

  typedef struct remixapi_AnimatedFloat4D {
    remixapi_Float4D*   pData;
    uint32_t            numberElements;
  } remixapi_AnimatedFloat4D;


  // New particle system struct with animated curve support
  typedef struct remixapi_InstanceInfoParticleSystemEXT {
    remixapi_StructType sType;
    void*               pNext;
    uint32_t            maxNumParticles;
    remixapi_Bool       useTurbulence;
    remixapi_Bool       alignParticlesToVelocity;
    remixapi_Bool       useSpawnTexcoords;
    remixapi_Bool       enableCollisionDetection;
    remixapi_Bool       enableMotionTrail;
    remixapi_Bool       hideEmitter;
    remixapi_Bool       restrictVelocityX;
    remixapi_Bool       restrictVelocityY;
    remixapi_Bool       restrictVelocityZ;
    remixapi_AnimatedFloat4D   minColor;
    remixapi_AnimatedFloat4D   maxColor;
    remixapi_AnimatedFloat1D   minRotationSpeed;
    remixapi_AnimatedFloat1D   maxRotationSpeed;
    remixapi_AnimatedFloat2D   minSize;
    remixapi_AnimatedFloat2D   maxSize;
    remixapi_AnimatedFloat3D   maxVelocity;
    remixapi_Float3D    attractorPosition;
    float               minTimeToLive;
    float               maxTimeToLive;
    float               initialVelocityFromNormal;
    float               initialVelocityConeAngleDegrees;
    float               dragCoefficient;
    float               initialRotationDeviationDegrees;
    float               gravityForce;
    float               turbulenceFrequency;
    float               turbulenceForce;
    float               spawnRatePerSecond;
    float               collisionThickness;
    float               collisionRestitution;
    float               motionTrailMultiplier;
    float               initialVelocityFromMotion;
    float               spawnBurstDuration;
    float               attractorRadius;
    float               attractorForce;
    uint8_t             billboardType;
    uint8_t             spriteSheetMode;
    uint8_t             collisionMode;
    uint8_t             randomFlipAxis;
  } remixapi_InstanceInfoParticleSystemEXT;

  typedef struct remixapi_InstanceInfoGpuInstancingEXT {
    remixapi_StructType       sType;
    void*                     pNext;
    const remixapi_Transform* instanceTransforms_values;
    uint32_t                  instanceTransforms_count;
  } remixapi_InstanceInfoGpuInstancingEXT;

  typedef struct remixapi_InstanceInfo {
    remixapi_StructType            sType;
    void*                          pNext;
    remixapi_InstanceCategoryFlags categoryFlags;
    remixapi_MeshHandle            mesh;
    remixapi_Transform             transform;
    remixapi_Bool                  doubleSided;
  } remixapi_InstanceInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DrawInstance)(
    const remixapi_InstanceInfo* info);


  typedef struct remixapi_LightInfoLightShaping {
    // The direction the Light Shaping is pointing in. Must be normalized.
    remixapi_Float3D               direction;
    float                          coneAngleDegrees;
    float                          coneSoftness;
    float                          focusExponent;
  } remixapi_LightInfoLightShaping;

  typedef struct remixapi_LightInfoSphereEXT {
    remixapi_StructType            sType;
    void*                          pNext;
    remixapi_Float3D               position;
    float                          radius;
    remixapi_Bool                  shaping_hasvalue;
    remixapi_LightInfoLightShaping shaping_value;
    float                          volumetricRadianceScale;
  } remixapi_LightInfoSphereEXT;

  typedef struct remixapi_LightInfoRectEXT {
    remixapi_StructType            sType;
    void*                          pNext;
    remixapi_Float3D               position;
    // The X axis of the Rect Light. Must be normalized and orthogonal to the Y and direction axes.
    remixapi_Float3D               xAxis;
    float                          xSize;
    // The Y axis of the Rect Light. Must be normalized and orthogonal to the X and direction axes.
    remixapi_Float3D               yAxis;
    float                          ySize;
    // The direction the Rect Light is pointing in, should match the Shaping direction if present.
    // Must be normalized and orthogonal to the X and Y axes.
    remixapi_Float3D               direction;
    remixapi_Bool                  shaping_hasvalue;
    remixapi_LightInfoLightShaping shaping_value;
    float                          volumetricRadianceScale;
  } remixapi_LightInfoRectEXT;

  typedef struct remixapi_LightInfoDiskEXT {
    remixapi_StructType            sType;
    void*                          pNext;
    remixapi_Float3D               position;
    // The X axis of the Disk Light. Must be normalized and orthogonal to the Y and direction axes.
    remixapi_Float3D               xAxis;
    float                          xRadius;
    // The Y axis of the Disk Light. Must be normalized and orthogonal to the X and direction axes.
    remixapi_Float3D               yAxis;
    float                          yRadius;
    // The direction the Disk Light is pointing in, should match the Shaping direction if present
    // Must be normalized and orthogonal to the X and Y axes.
    remixapi_Float3D               direction;
    remixapi_Bool                  shaping_hasvalue;
    remixapi_LightInfoLightShaping shaping_value;
    float                          volumetricRadianceScale;
  } remixapi_LightInfoDiskEXT;

  typedef struct remixapi_LightInfoCylinderEXT {
    remixapi_StructType            sType;
    void*                          pNext;
    remixapi_Float3D               position;
    float                          radius;
    // The "center" axis of the Cylinder Light. Must be normalized.
    remixapi_Float3D               axis;
    float                          axisLength;
    float                          volumetricRadianceScale;
  } remixapi_LightInfoCylinderEXT;

  typedef struct remixapi_LightInfoDistantEXT {
    remixapi_StructType             sType;
    void*                           pNext;
    // The direction the Distant Light is pointing in. Must be normalized.
    remixapi_Float3D                direction;
    float                           angularDiameterDegrees;
    float                           volumetricRadianceScale;
  } remixapi_LightInfoDistantEXT;

  typedef struct remixapi_LightInfoDomeEXT {
    remixapi_StructType             sType;
    void*                           pNext;
    remixapi_Transform              transform;
    remixapi_Path                   colorTexture;
  } remixapi_LightInfoDomeEXT;

  // Attachable to remixapi_LightInfo.
  // If attached, 'remixapi_LightInfo::radiance' is ignored.
  // Any other attached 'remixapi_LightInfo*EXT' are ignored.
  // Most fields correspond to a usd token. Set to null, if no value.
  typedef struct remixapi_LightInfoUSDEXT {
    remixapi_StructType             sType;
    void*                           pNext;
    remixapi_StructType             lightType;
    remixapi_Transform              transform;
    const float*                    pRadius;                  // "radius"
    const float*                    pWidth;                   // "width"
    const float*                    pHeight;                  // "height"
    const float*                    pLength;                  // "length"
    const float*                    pAngleRadians;            // "angle"
    const remixapi_Bool*            pEnableColorTemp;         // "enableColorTemperature"
    const remixapi_Float3D*         pColor;                   // "color"
    const float*                    pColorTemp;               // "colorTemperature"
    const float*                    pExposure;                // "exposure"
    const float*                    pIntensity;               // "intensity"
    const float*                    pConeAngleRadians;        // "shaping:cone:angle"
    const float*                    pConeSoftness;            // "shaping:cone:softness"
    const float*                    pFocus;                   // "shaping:focus"
    const float*                    pVolumetricRadianceScale; // "volumetric_radiance_scale"
  } remixapi_LightInfoUSDEXT;

  typedef struct remixapi_LightInfo {
    remixapi_StructType             sType;
    void*                           pNext;
    uint64_t                        hash;
    remixapi_Float3D                radiance;
    remixapi_Bool                   isDynamic;
    remixapi_Bool                   ignoreViewModel;
  } remixapi_LightInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateLight)(
    const remixapi_LightInfo* info,
    remixapi_LightHandle*     out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateLightBatched)(
    const remixapi_LightInfo* info,
    remixapi_LightHandle*     out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyLight)(
    remixapi_LightHandle      handle);


  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DrawLightInstance)(
    remixapi_LightHandle      lightHandle);


  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetConfigVariable)(
    const char*               key,
    const char*               value);

  // Plugin-injected game-state write. Stores `value` under `key` in a
  // fork-owned, thread-safe string/string map that graph components
  // (GameValueReadBool / GameValueReadNumber) read by name.
  //
  // Keys are chosen by the plugin; Remix does not validate or namespace them.
  // The store survives Shutdown / re-init, so callers do not have to
  // re-populate their state across device resets.
  //
  // Returns REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS on a null or empty key,
  // REMIXAPI_ERROR_CODE_SUCCESS otherwise.
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetGameValue)(
    const char*               key,
    const char*               value);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_SetGameValue(
    const char*               key,
    const char*               value);

  // Plugin-driven game-state read. Looks up `key` in the fork-owned
  // thread-safe string/string map populated by remixapi_SetGameValue and graph
  // components.
  //
  // Returns:
  //   REMIXAPI_ERROR_CODE_SUCCESS — read attempt completed (whether or not the
  //     key existed). The caller checks *out_actual_size == 0 to detect missing
  //     keys, and compares it against in_buffer_size to detect buffer
  //     truncation.
  //   REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS — null/empty key, null
  //     out_actual_size, or in_buffer_size > 0 with null out_buffer.
  //
  // On success when the key exists:
  //   *out_actual_size = strlen(value) + 1 (incl. null terminator)
  //   if in_buffer_size >= *out_actual_size: out_buffer is filled with value + '\0'
  //   if in_buffer_size <  *out_actual_size: out_buffer is left untouched
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_GetGameValue)(
    const char* key,
    char*       out_buffer,
    uint32_t    in_buffer_size,
    uint32_t*   out_actual_size);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_GetGameValue(
    const char* key,
    char*       out_buffer,
    uint32_t    in_buffer_size,
    uint32_t*   out_actual_size);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_AddTextureHash)(
    const char* textureCategory,
    const char* textureHash);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_RemoveTextureHash)(
    const char* textureCategory,
    const char* textureHash);

  typedef struct remixapi_PresentInfo {
    remixapi_StructType       sType;
    void*                     pNext;
    remixapi_HWND             hwndOverride; // Can be NULL
  } remixapi_PresentInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Present)(const remixapi_PresentInfo* info);

  // Optional frame-boundary callbacks (mirrors bridge API semantics)
  typedef void (REMIXAPI_PTR* PFN_remixapi_BridgeCallback2)(void);
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_RegisterCallbacks2)(
    PFN_remixapi_BridgeCallback2 beginSceneCallback,
    PFN_remixapi_BridgeCallback2 endSceneCallback,
    PFN_remixapi_BridgeCallback2 presentCallback);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_RegisterCallbacks2(
    PFN_remixapi_BridgeCallback2 beginSceneCallback,
    PFN_remixapi_BridgeCallback2 endSceneCallback,
    PFN_remixapi_BridgeCallback2 presentCallback);

  // Internal helper to auto-instance persistent external API lights once per frame
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_AutoInstancePersistentLights)(void);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_AutoInstancePersistentLights(void);

  // Queue a definition update for an existing analytical light. Applied safely each frame.
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_UpdateLightDefinition)(
    remixapi_LightHandle handle,
    const remixapi_LightInfo* info);
  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_UpdateLightDefinition(
    remixapi_LightHandle handle,
    const remixapi_LightInfo* info);

  typedef void (REMIXAPI_PTR* PFN_remixapi_pick_RequestObjectPickingUserCallback)(
    const uint32_t*           objectPickingValues_values,
    uint32_t                  objectPickingValues_count,
    void*                     callbackUserData);

  // Invokes 'callback' on a successful readback of 'remixapi_InstanceInfoObjectPickingEXT::objectPickingValue'
  // of objects that are drawn in the 'pixelRegion'. 'pixelRegion' specified relative to the output size,
  // not render size. 'callback' can be invoked from any thread.
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_pick_RequestObjectPicking)(
    const remixapi_Rect2D*                              pixelRegion,
    PFN_remixapi_pick_RequestObjectPickingUserCallback  callback,
    void*                                               callbackUserData);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_pick_HighlightObjects)(
    const uint32_t*           objectPickingValues_values,
    uint32_t                  objectPickingValues_count,
    uint8_t                   colorR,
    uint8_t                   colorG,
    uint8_t                   colorB);



  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_CreateD3D9)(
    remixapi_Bool       editorModeEnabled,
    IDirect3D9Ex**      out_pD3D9);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_RegisterD3D9Device)(
    IDirect3DDevice9Ex* d3d9Device);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_GetExternalSwapchain)(
    uint64_t*           out_vkImage,
    uint64_t*           out_vkSemaphoreRenderingDone,
    uint64_t*           out_vkSemaphoreResumeSemaphore);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_GetSharedD3D11TextureHandle)(
    void**    out_sharedHandle,
    uint32_t* out_width,
    uint32_t* out_height);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_GetVkImage)(
    IDirect3DSurface9*  source,
    uint64_t*           out_vkImage);

  typedef enum remixapi_dxvk_CopyRenderingOutputType {
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_FINAL_COLOR = 0,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_DEPTH = 1,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_NORMALS = 2,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_OBJECT_PICKING = 3,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_GUI = 4,
  } remixapi_dxvk_CopyRenderingOutputType;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_CopyRenderingOutput)(
    IDirect3DSurface9*                    destination,
    remixapi_dxvk_CopyRenderingOutputType type);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_SetDefaultOutput)(
    remixapi_dxvk_CopyRenderingOutputType type,
    const remixapi_Float4D* color);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_GetTextureHash)(
    IDirect3DTexture9* texture,
    uint64_t*          out_hash);


  // Texture upload API
  typedef enum remixapi_Format {
    REMIXAPI_FORMAT_R8G8B8A8_UNORM = 37,   // VK_FORMAT_R8G8B8A8_UNORM
    REMIXAPI_FORMAT_R8G8B8A8_SRGB = 43,    // VK_FORMAT_R8G8B8A8_SRGB
    REMIXAPI_FORMAT_B8G8R8A8_UNORM = 44,   // VK_FORMAT_B8G8R8A8_UNORM
    REMIXAPI_FORMAT_B8G8R8A8_SRGB = 50,    // VK_FORMAT_B8G8R8A8_SRGB
    REMIXAPI_FORMAT_BC1_RGB_UNORM = 131,   // VK_FORMAT_BC1_RGB_UNORM_BLOCK
    REMIXAPI_FORMAT_BC1_RGB_SRGB = 132,    // VK_FORMAT_BC1_RGB_SRGB_BLOCK
    REMIXAPI_FORMAT_BC3_UNORM = 135,       // VK_FORMAT_BC3_UNORM_BLOCK
    REMIXAPI_FORMAT_BC3_SRGB = 136,        // VK_FORMAT_BC3_SRGB_BLOCK
    REMIXAPI_FORMAT_BC5_UNORM = 139,       // VK_FORMAT_BC5_UNORM_BLOCK (normal maps)
    REMIXAPI_FORMAT_BC7_UNORM = 145,       // VK_FORMAT_BC7_UNORM_BLOCK
    REMIXAPI_FORMAT_BC7_SRGB = 146,        // VK_FORMAT_BC7_SRGB_BLOCK
  } remixapi_Format;

  typedef struct remixapi_TextureInfo {
    remixapi_StructType sType;
    void*               pNext;
    uint64_t            hash;           // Unique identifier for this texture
    uint32_t            width;
    uint32_t            height;
    uint32_t            depth;          // Set to 1 for 2D textures
    uint32_t            mipLevels;      // Set to 1 for no mipmaps
    remixapi_Format     format;
    const void*         data;           // Pointer to pixel data (all mips sequential)
    uint64_t            dataSize;       // Total size in bytes
  } remixapi_TextureInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateTexture)(
    const remixapi_TextureInfo* info,
    remixapi_TextureHandle*     out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyTexture)(
    remixapi_TextureHandle      handle);

  // Screen overlay API
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DrawScreenOverlay)(
    const void*       pPixelData,
    uint32_t          width,
    uint32_t          height,
    remixapi_Format   format,
    float             opacity);


  typedef struct remixapi_InitializeLibraryInfo {
    remixapi_StructType sType;
    void*               pNext;
    uint64_t            version;
  } remixapi_InitializeLibraryInfo;

  // NOTE: If adding a new function, append at the end of the struct.
  //       Reordering breaks backwards compatibility.

  // Force the RtxTextureManager to demote/clear textures not currently needed.
  // Hooks SceneManager::requestTextureVramFree, which sets an atomic flag
  // consumed next render-thread tick; the tick calls textureManager.clear(),
  // which wipes the SparseUniqueCache and (if over the fork's texturemanager
  // budget) demotes streaming textures to 0 mips. Complements the plugin's
  // own material/texture refcount chain: this catches orphans whose cache
  // entry outlives their last owner.
  //
  // Not free -- the cache wipe runs on the render thread. Fire at bulk
  // scene-turnover events (cell transitions, fast-travel), not every frame.
  //
  // Thread-safe: sets an atomic on SceneManager; no lock acquired.
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_RequestTextureVramFree)(void);

  // Release unused memory back to the driver (force compaction).
  // DXVK's memory allocator retains freed VkDeviceMemory chunks as spare empty
  // chunks in a high-water-mark pattern, only returning them to the driver
  // when a second spare chunk of the same type appears or the heap is over
  // budget. This call asks the renderer to explicitly release those retained
  // chunks on the next render-thread tick (see
  // SceneManager::requestVramCompaction + DxvkMemoryAllocator::freeUnusedChunks).
  //
  // Intent: plugins call this after bulk destroy events (cell transitions,
  // fast-travel, scene turnover) where the allocator is likely holding onto
  // VRAM that will not be reused. The call is not free -- it blocks on the
  // next frame's vkFreeMemory sweep -- so fire it at a low rate, not every
  // frame.
  //
  // Thread-safe: sets an atomic flag on SceneManager; no lock acquired.
  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_RequestVramCompaction)(void);

  // Per-category VRAM usage snapshot aggregated across all device-local heaps.
  // Sizes are in bytes. poolRetainedBytes == totalAllocatedBytes - totalUsedBytes
  // and represents memory owned by the DXVK allocator's empty-chunk pool that
  // has not been returned to the driver -- the number that RequestVramCompaction
  // moves. Category breakdown mirrors DxvkMemoryStats::Category and covers only
  // RTX-owned suballocations; app-owned buffers/textures (D3D9 app traffic) are
  // not included.
  typedef struct remixapi_VramStats {
    uint64_t totalAllocatedBytes;
    uint64_t totalUsedBytes;
    uint64_t poolRetainedBytes;
    uint64_t usedReplacementGeometryBytes;
    uint64_t usedBufferBytes;
    uint64_t usedAccelerationStructureBytes;
    uint64_t usedOpacityMicromapBytes;
    uint64_t usedMaterialTextureBytes;
    uint64_t usedRenderTargetBytes;
    // Driver-reported allocation on device-local heaps. Sourced from
    // VK_EXT_memory_budget (adapter->getMemoryHeapInfo -> memoryAllocated).
    // This is the Task-Manager / nvidia-smi view of the process's Vulkan
    // footprint. totalAllocatedBytes only counts DXVK's own allocator path,
    // so (driverAllocatedBytes - totalAllocatedBytes) is the non-DXVK
    // overhead: DLSS/NGX internal buffers, raytracing pipeline driver
    // state, bindless descriptor pools, NRC CUDA, pipeline caches, etc.
    uint64_t driverAllocatedBytes;
    uint64_t driverBudgetBytes;
    // Size of the fork-side RtxTextureManager's SparseUniqueCache. Grows
    // with both D3D9-native and plugin-created textures tracked by Remix.
    // If this climbs while plugin's own texture count is stable, the
    // growth is in fork-side streaming/native textures, not plugin uploads.
    uint32_t forkTextureCacheCount;
  } remixapi_VramStats;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_GetVramStats)(
    remixapi_VramStats* out_stats);

  typedef struct remixapi_Interface {
    PFN_remixapi_Shutdown           Shutdown;
    PFN_remixapi_CreateMaterial     CreateMaterial;
    PFN_remixapi_DestroyMaterial    DestroyMaterial;
    PFN_remixapi_CreateMesh         CreateMesh;
    PFN_remixapi_CreateMeshBatched  CreateMeshBatched;
    PFN_remixapi_DestroyMesh        DestroyMesh;
    PFN_remixapi_SetupCamera        SetupCamera;
    PFN_remixapi_DrawInstance       DrawInstance;
    PFN_remixapi_CreateLight        CreateLight;
    PFN_remixapi_CreateLightBatched CreateLightBatched;
    PFN_remixapi_DestroyLight       DestroyLight;
    PFN_remixapi_DrawLightInstance  DrawLightInstance;
    PFN_remixapi_SetConfigVariable  SetConfigVariable;
    PFN_remixapi_AddTextureHash     AddTextureHash;
    PFN_remixapi_RemoveTextureHash  RemoveTextureHash;
    PFN_remixapi_CreateTexture      CreateTexture;
    PFN_remixapi_DestroyTexture     DestroyTexture;

    // DXVK interoperability
    PFN_remixapi_dxvk_CreateD3D9            dxvk_CreateD3D9;
    PFN_remixapi_dxvk_RegisterD3D9Device    dxvk_RegisterD3D9Device;
    PFN_remixapi_dxvk_GetExternalSwapchain  dxvk_GetExternalSwapchain;
    PFN_remixapi_dxvk_GetVkImage            dxvk_GetVkImage;
    PFN_remixapi_dxvk_CopyRenderingOutput   dxvk_CopyRenderingOutput;
    PFN_remixapi_dxvk_SetDefaultOutput      dxvk_SetDefaultOutput;
    PFN_remixapi_dxvk_GetTextureHash        dxvk_GetTextureHash;
    PFN_remixapi_dxvk_GetSharedD3D11TextureHandle dxvk_GetSharedD3D11TextureHandle;
    // Object picking utils
    PFN_remixapi_pick_RequestObjectPicking  pick_RequestObjectPicking;
    PFN_remixapi_pick_HighlightObjects      pick_HighlightObjects;

    PFN_remixapi_Startup            Startup;
    PFN_remixapi_Present            Present;

    PFN_remixapi_SetCameraMediumMaterial SetCameraMediumMaterial;
    // NOTE: REMIXAPI_PTR is required so the calling convention matches the
    // actual remixapi_GetUIState / remixapi_SetUIState entry functions
    // (which are __stdcall via REMIXAPI_CALL). On x64 this is moot — there's
    // a single calling convention — but on x86 (bridge client) the bare
    // `(*Fn)(...)` syntax defaulted to __cdecl and silently corrupted the
    // stack when callers invoked the __stdcall entry through the field.
    remixapi_UIState                (REMIXAPI_PTR *GetUIState)(void);
    remixapi_ErrorCode              (REMIXAPI_PTR *SetUIState)(remixapi_UIState state);

    // Optional extension functions (present starting in v0.5.1+)
    PFN_remixapi_RegisterCallbacks2          RegisterCallbacks;
    PFN_remixapi_AutoInstancePersistentLights AutoInstancePersistentLights;
    PFN_remixapi_UpdateLightDefinition      UpdateLightDefinition;
    PFN_remixapi_DrawScreenOverlay          DrawScreenOverlay;
    PFN_remixapi_SetGameValue               SetGameValue;
    PFN_remixapi_RequestVramCompaction      RequestVramCompaction;
    PFN_remixapi_GetVramStats               GetVramStats;
    PFN_remixapi_RequestTextureVramFree     RequestTextureVramFree;
    PFN_remixapi_GetGameValue               GetGameValue;
  } remixapi_Interface;

  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_InitializeLibrary(
    const remixapi_InitializeLibraryInfo* info,
    remixapi_Interface*                   out_result);

  typedef remixapi_ErrorCode(REMIXAPI_CALL* PFN_remixapi_InitializeLibrary)(
    const remixapi_InitializeLibraryInfo* info,
    remixapi_Interface*                   out_result);

#ifndef REMIX_WINAPI_NO_LIBRARY_LOADER
  inline remixapi_ErrorCode REMIXAPI_CALL remixapi_lib_loadRemixDllAndInitialize(
    const wchar_t*      remixD3D9DllPath,
    remixapi_Interface* out_remixInterface,
    remixapi_HMODULE*   out_remixDll
  ) {
    if (remixD3D9DllPath == NULL || remixD3D9DllPath[0] == '\0') {
      return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
    }
    if (out_remixInterface == NULL || out_remixDll == NULL) {
      return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
    }

    remixapi_HMODULE remixDll = NULL;
    PFN_remixapi_InitializeLibrary pfn_InitializeLibrary = NULL;
    {
      // firstly, try the default method first, e.g. DLL is already loaded, 
      // DLL-s are around .exe, or an app has called SetDllDirectory
      {
        remixapi_HMODULE dll = LoadLibraryW(remixD3D9DllPath);
        if (dll) {
          remixapi_loader_PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
          if (func) {
            remixDll = dll;
            pfn_InitializeLibrary = (PFN_remixapi_InitializeLibrary) func;
          } else {
            FreeLibrary(dll);
          }
        }
      }

      // then try raw user-provided 'remixD3D9DllPath' file
      if (!pfn_InitializeLibrary) {
        // set LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR to search 
        // dependency DLL-s in the folder of 'remixD3D9DllPath'
        remixapi_HMODULE dll = LoadLibraryExW(remixD3D9DllPath, NULL,
                                     REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                     REMIX_WINAPI_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (dll) {
          remixapi_loader_PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
          if (func) {
            remixDll = dll;
            pfn_InitializeLibrary = (PFN_remixapi_InitializeLibrary) func;
          } else {
            FreeLibrary(dll);
          }
        }
      }

      // at last, try to SetDllDirectory manually
      if (!pfn_InitializeLibrary) {
        wchar_t absoluteD3D9DllPath[REMIX_WINAPI_MAX_PATH];
        {
          unsigned ret = (unsigned)GetFullPathNameW(remixD3D9DllPath, REMIX_WINAPI_MAX_PATH, absoluteD3D9DllPath, NULL);
          if (ret == 0) {
            return REMIXAPI_ERROR_CODE_GET_FULL_PATH_NAME_FAILURE;
          }
        }
        wchar_t parentDir[REMIX_WINAPI_MAX_PATH];
        {
          int len = 0;
          for (int i = 0; i < REMIX_WINAPI_MAX_PATH; i++) {
            if (absoluteD3D9DllPath[i] == '\0') {
              break;
            }
            parentDir[i] = absoluteD3D9DllPath[i] == '/' ? '\\' : absoluteD3D9DllPath[i];
            ++len;
          }
          if (len <= 0 || len >= REMIX_WINAPI_MAX_PATH) {
            return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
          }
          parentDir[len] = '\0';
          for (int i = len - 1; i >= 0; --i) {
            if (parentDir[i] == '\\') {
              // remove one or more path separators
              for (int k = i; k >= 0; --k) {
                if (parentDir[k] == '\\') {
                  parentDir[k] = '\0';
                  continue;
                }
                break;
              }
              break;
            }
          }
          if (parentDir[0] == '\0') {
            return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
          }
        }

        // save the previous value that is in SetDllDirectory
        wchar_t dirToRestore[REMIX_WINAPI_MAX_PATH];
        {
          unsigned len = (unsigned)GetDllDirectoryW(REMIX_WINAPI_MAX_PATH, dirToRestore);
          if (len > 0 && len < REMIX_WINAPI_MAX_PATH - 1) {
            dirToRestore[REMIX_WINAPI_MAX_PATH - 1] = '\0';
          }
        }

        {
          int s = SetDllDirectoryW(parentDir);
          if (!s) {
            return REMIXAPI_ERROR_CODE_SET_DLL_DIRECTORY_FAILURE;
          }
        }

        remixapi_HMODULE dll = LoadLibraryW(absoluteD3D9DllPath);
        if (dll) {
          remixapi_loader_PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
          if (func) {
            remixDll = dll;
            pfn_InitializeLibrary = (PFN_remixapi_InitializeLibrary) func;
          } else {
            FreeLibrary(dll);
          }
        }

        // restore the previous value
        {
          SetDllDirectoryW(dirToRestore);
        }
      }
    }

    if (!remixDll) {
      return REMIXAPI_ERROR_CODE_LOAD_LIBRARY_FAILURE;
    }
    if (!pfn_InitializeLibrary){
      return REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE;
    }

    remixapi_InitializeLibraryInfo info = { (remixapi_StructType)0 };
    {
      info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
      info.version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR,
                                           REMIXAPI_VERSION_MINOR,
                                           REMIXAPI_VERSION_PATCH);
    }
    remixapi_Interface remixInterface = { 0 };

    remixapi_ErrorCode status = pfn_InitializeLibrary(&info, &remixInterface);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      FreeLibrary(remixDll);
      return status;
    }

    *out_remixInterface = remixInterface;
    *out_remixDll = remixDll;
    return status;
  }

  inline remixapi_ErrorCode REMIXAPI_CALL remixapi_lib_shutdownAndUnloadRemixDll(
    remixapi_Interface* remixInterface,
    remixapi_HMODULE    remixDll
  ) {
    if (remixInterface == NULL || remixInterface->Shutdown == NULL) {
      if (remixDll != NULL) {
        FreeLibrary(remixDll);
      }
      return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
    }

    remixapi_ErrorCode status = remixInterface->Shutdown();
    if (remixDll != NULL) {
      FreeLibrary(remixDll);
    }

    remixapi_Interface nullInterface = { 0 };
    *remixInterface = nullInterface;
    return status;
  }
#endif // !REMIX_WINAPI_NO_LIBRARY_LOADER

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // REMIX_C_H_
