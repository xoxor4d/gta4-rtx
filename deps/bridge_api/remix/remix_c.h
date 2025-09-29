/*
 * Copyright (c) 2023-2024, NVIDIA CORPORATION. All rights reserved.
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
#include <windows.h>

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
#define REMIXAPI_VERSION_MINOR 5
#define REMIXAPI_VERSION_PATCH 2


// External
typedef struct IDirect3D9Ex       IDirect3D9Ex;
typedef struct IDirect3DDevice9Ex IDirect3DDevice9Ex;
typedef struct IDirect3DSurface9  IDirect3DSurface9;


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
    REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_PARTICLE_SYSTEM_EXT    = 24,
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

  typedef const wchar_t* remixapi_Path;



  typedef struct remixapi_StartupInfo {
    remixapi_StructType sType;
    void*               pNext;
    HWND                hwnd;
    remixapi_Bool       disableSrgbConversionForOutput;
    // If true, 'dxvk_GetExternalSwapchain' can be used to retrieve a raw VkImage,
    // so the application can present it, for example by using OpenGL interop:
    // converting VkImage to OpenGL, and presenting it via OpenGL.
    // Default: false. Use VkSwapchainKHR to present frame into HWND.
    remixapi_Bool       forceNoVkSwapchain;
    remixapi_Bool       editorModeEnabled;
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

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyMesh)(
    remixapi_MeshHandle       handle);



  typedef enum remixapi_CameraType {
    REMIXAPI_CAMERA_TYPE_WORLD,
    REMIXAPI_CAMERA_TYPE_SKY,
    REMIXAPI_CAMERA_TYPE_VIEW_MODEL,
  } remixapi_CameraType;

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
    REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_UI                  = 1 << 0,
    REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_MATTE               = 1 << 1,
    REMIXAPI_INSTANCE_CATEGORY_BIT_SKY                       = 1 << 2,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE                    = 1 << 3,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_LIGHTS             = 1 << 4,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING       = 1 << 5,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_MOTION_BLUR        = 1 << 6,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_OPACITY_MICROMAP   = 1 << 7,
    REMIXAPI_INSTANCE_CATEGORY_BIT_HIDDEN                    = 1 << 8,
    REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE                  = 1 << 9,
    REMIXAPI_INSTANCE_CATEGORY_BIT_BEAM                      = 1 << 10,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC              = 1 << 11,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_DYNAMIC             = 1 << 12,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_SINGLE_OFFSET       = 1 << 13,
    REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_NO_OFFSET           = 1 << 14,
    REMIXAPI_INSTANCE_CATEGORY_BIT_ALPHA_BLEND_TO_CUTOUT     = 1 << 15,
    REMIXAPI_INSTANCE_CATEGORY_BIT_TERRAIN                   = 1 << 16,
    REMIXAPI_INSTANCE_CATEGORY_BIT_ANIMATED_WATER            = 1 << 17,
    REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL = 1 << 18,
    REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY  = 1 << 19,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_BAKED_LIGHTING     = 1 << 20,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ALPHA_CHANNEL      = 1 << 21,
    REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_TRANSPARENCY_LAYER = 1 << 22,
    REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE_EMITTER          = 1 << 23,
  } remixapi_InstanceCategoryBit;

  typedef uint32_t remixapi_InstanceCategoryFlags;

  typedef struct remixapi_InstanceInfoParticleSystemEXT {
    remixapi_StructType      sType;
    void*                    pNext;
    uint32_t         maxNumParticles;
    remixapi_Bool    useTurbulence;
    remixapi_Bool    alignParticlesToVelocity;
    remixapi_Bool    useSpawnTexcoords;
    remixapi_Bool    enableCollisionDetection;
    remixapi_Bool    enableMotionTrail;
    remixapi_Bool    hideEmitter;
    remixapi_Float4D minSpawnColor;
    remixapi_Float4D maxSpawnColor;
    float            minTimeToLive;
    float            maxTimeToLive;
    float            initialVelocityFromNormal;
    float            initialVelocityConeAngleDegrees;
    float            minSpawnSize;
    float            maxSpawnSize;
    float            gravityForce;
    float            maxSpeed;
    float            turbulenceFrequency;
    float            turbulenceForce;
    float            minSpawnRotationSpeed;
    float            maxSpawnRotationSpeed;
    float            spawnRatePerSecond;
    float            collisionThickness;
    float            collisionRestitution;
    float            motionTrailMultiplier;
    float            initialVelocityFromMotion;
    float            minTargetSize;
    float            maxTargetSize;
    float            minTargetRotationSpeed;
    float            maxTargetRotationSpeed;
    remixapi_Float4D minTargetColor;
    remixapi_Float4D maxTargetColor;
    uint32_t         billboardType;
  } remixapi_InstanceInfoParticleSystemEXT;

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
  } remixapi_LightInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_CreateLight)(
    const remixapi_LightInfo* info,
    remixapi_LightHandle*     out_handle);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DestroyLight)(
    remixapi_LightHandle      handle);


  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_DrawLightInstance)(
    remixapi_LightHandle      lightHandle);


  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_SetConfigVariable)(
    const char*               key,
    const char*               value);

  typedef struct remixapi_PresentInfo {
    remixapi_StructType       sType;
    void*                     pNext;
    HWND                      hwndOverride; // Can be NULL
  } remixapi_PresentInfo;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_Present)(const remixapi_PresentInfo* info);

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

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_GetVkImage)(
    IDirect3DSurface9*  source,
    uint64_t*           out_vkImage);

  typedef enum remixapi_dxvk_CopyRenderingOutputType {
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_FINAL_COLOR = 0,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_DEPTH = 1,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_NORMALS = 2,
    REMIXAPI_DXVK_COPY_RENDERING_OUTPUT_TYPE_OBJECT_PICKING = 3,
  } remixapi_dxvk_CopyRenderingOutputType;

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_CopyRenderingOutput)(
    IDirect3DSurface9*                    destination,
    remixapi_dxvk_CopyRenderingOutputType type);

  typedef remixapi_ErrorCode(REMIXAPI_PTR* PFN_remixapi_dxvk_SetDefaultOutput)(
    remixapi_dxvk_CopyRenderingOutputType type,
    const remixapi_Float4D* color);


  typedef struct remixapi_InitializeLibraryInfo {
    remixapi_StructType sType;
    void*               pNext;
    uint64_t            version;
  } remixapi_InitializeLibraryInfo;

  typedef struct remixapi_Interface {
    PFN_remixapi_Shutdown           Shutdown;
    PFN_remixapi_CreateMaterial     CreateMaterial;
    PFN_remixapi_DestroyMaterial    DestroyMaterial;
    PFN_remixapi_CreateMesh         CreateMesh;
    PFN_remixapi_DestroyMesh        DestroyMesh;
    PFN_remixapi_SetupCamera        SetupCamera;
    PFN_remixapi_DrawInstance       DrawInstance;
    PFN_remixapi_CreateLight        CreateLight;
    PFN_remixapi_DestroyLight       DestroyLight;
    PFN_remixapi_DrawLightInstance  DrawLightInstance;
    PFN_remixapi_SetConfigVariable  SetConfigVariable;

    // DXVK interoperability
    PFN_remixapi_dxvk_CreateD3D9            dxvk_CreateD3D9;
    PFN_remixapi_dxvk_RegisterD3D9Device    dxvk_RegisterD3D9Device;
    PFN_remixapi_dxvk_GetExternalSwapchain  dxvk_GetExternalSwapchain;
    PFN_remixapi_dxvk_GetVkImage            dxvk_GetVkImage;
    PFN_remixapi_dxvk_CopyRenderingOutput   dxvk_CopyRenderingOutput;
    PFN_remixapi_dxvk_SetDefaultOutput      dxvk_SetDefaultOutput;
    // Object picking utils
    PFN_remixapi_pick_RequestObjectPicking  pick_RequestObjectPicking;
    PFN_remixapi_pick_HighlightObjects      pick_HighlightObjects;

    PFN_remixapi_Startup            Startup;
    PFN_remixapi_Present            Present;
  } remixapi_Interface;

  REMIXAPI remixapi_ErrorCode REMIXAPI_CALL remixapi_InitializeLibrary(
    const remixapi_InitializeLibraryInfo* info,
    remixapi_Interface*                   out_result);

  typedef remixapi_ErrorCode(REMIXAPI_CALL* PFN_remixapi_InitializeLibrary)(
    const remixapi_InitializeLibraryInfo* info,
    remixapi_Interface*                   out_result);


  inline remixapi_ErrorCode REMIXAPI_CALL remixapi_lib_loadRemixDllAndInitialize(
    const wchar_t*      remixD3D9DllPath,
    remixapi_Interface* out_remixInterface,
    HMODULE*            out_remixDll
  ) {
    if (remixD3D9DllPath == NULL || remixD3D9DllPath[0] == '\0') {
      return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
    }
    if (out_remixInterface == NULL || out_remixDll == NULL) {
      return REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS;
    }

    HMODULE remixDll = NULL;
    PFN_remixapi_InitializeLibrary pfn_InitializeLibrary = NULL;
    {
      // firstly, try the default method first, e.g. DLL is already loaded, 
      // DLL-s are around .exe, or an app has called SetDllDirectory
      {
        HMODULE dll = LoadLibraryW(remixD3D9DllPath);
        if (dll) {
          PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
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
        HMODULE dll = LoadLibraryExW(remixD3D9DllPath, NULL,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                     LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (dll) {
          PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
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
        wchar_t absoluteD3D9DllPath[MAX_PATH];
        {
          DWORD ret = GetFullPathNameW(remixD3D9DllPath, MAX_PATH, absoluteD3D9DllPath, NULL);
          if (ret == 0) {
            return REMIXAPI_ERROR_CODE_GET_FULL_PATH_NAME_FAILURE;
          }
        }
        wchar_t parentDir[MAX_PATH];
        {
          int len = 0;
          for (int i = 0; i < MAX_PATH; i++) {
            if (absoluteD3D9DllPath[i] == '\0') {
              break;
            }
            parentDir[i] = absoluteD3D9DllPath[i] == '/' ? '\\' : absoluteD3D9DllPath[i];
            ++len;
          }
          if (len <= 0 || len >= MAX_PATH) {
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
        wchar_t dirToRestore[MAX_PATH];
        {
          DWORD len = GetDllDirectoryW(MAX_PATH, dirToRestore);
          if (len > 0 && len < MAX_PATH - 1) {
            dirToRestore[MAX_PATH - 1] = '\0';
          }
        }

        {
          BOOL s = SetDllDirectoryW(parentDir);
          if (!s) {
            return REMIXAPI_ERROR_CODE_SET_DLL_DIRECTORY_FAILURE;
          }
        }

        HMODULE dll = LoadLibraryW(absoluteD3D9DllPath);
        if (dll) {
          PROC func = GetProcAddress(dll, "remixapi_InitializeLibrary");
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

    remixapi_InitializeLibraryInfo info = { 0 };
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
    HMODULE             remixDll
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

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // REMIX_C_H_
