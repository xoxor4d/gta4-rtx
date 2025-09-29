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

#pragma once

#include "remix_c.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <optional>

#ifndef REMIXAPI_ASSERT
#include <cassert>
#define REMIXAPI_ASSERT(mustBeTrue) assert(mustBeTrue)
#endif

namespace remix {
  namespace detail {
    template< typename T >
    struct Result {
      Result(T&& value)
        : m_status { REMIXAPI_ERROR_CODE_SUCCESS }
        , m_value { std::forward< T >(value) } {
      }

      Result(remixapi_ErrorCode error)
        : m_status { error }
        , m_value {} {
        REMIXAPI_ASSERT(error != REMIXAPI_ERROR_CODE_SUCCESS);
      }

      Result(const Result&) = delete;
      Result(Result&&) = delete;
      Result& operator=(const Result&) = delete;
      Result& operator=(Result&&) = delete;

      operator bool() const {
        return m_status == REMIXAPI_ERROR_CODE_SUCCESS;
      }

      T& value() {
        REMIXAPI_ASSERT(bool { *this });
        return m_value;
      }

      const T& value() const {
        REMIXAPI_ASSERT(bool { *this });
        return m_value;
      }

      remixapi_ErrorCode status() const {
        return m_status;
      }

      const T& operator*() const {
        return value();
      }
      T& operator*() {
        return value();
      }
      const T* operator->() const {
        return &value();
      }
      T* operator->() {
        return &value();
      }

    private:
      const remixapi_ErrorCode m_status;
      T                        m_value;
    };

    template<>
    struct Result< void > {
      Result(remixapi_ErrorCode error) : m_status { error } { }

      Result(const Result&) = delete;
      Result(Result&&) = delete;
      Result& operator=(const Result&) = delete;
      Result& operator=(Result&&) = delete;

      operator bool() const {
        return m_status == REMIXAPI_ERROR_CODE_SUCCESS;
      }

      remixapi_ErrorCode status() const {
        return m_status;
      }

    private:
      const remixapi_ErrorCode m_status;
    };

    template< typename T >
    struct Span
    {
        const T* values{ nullptr };
        uint32_t count{ 0 };
    };

    template< typename T >
    void assign_if(remixapi_Bool& hasvalue, T& value, const std::optional< T >& src) {
      if (src) {
        hasvalue = true;
        value = src.value();
      } else {
        hasvalue = false;
      }
    }
  }

  template< typename T >
  using Result = detail::Result< T >;

  template< typename T >
  using Span = detail::Span< T >;

  using StructType = remixapi_StructType;
  using Rect2D = remixapi_Rect2D;
  using Float2D = remixapi_Float2D;
  using Float3D = remixapi_Float3D;
  using Float4D = remixapi_Float4D;
  using Transform = remixapi_Transform;



  struct MaterialInfo;
  struct MeshInfo;
  struct CameraInfo;
  struct InstanceInfo;
  struct LightInfo;
  namespace detail {
    struct dxvk_ExternalSwapchain;
    struct dxvk_VkImage;
  }

  struct Interface {
    HMODULE            m_RemixDLL { nullptr };
    remixapi_Interface m_CInterface {};

    // Functions
    Result< void >                    Startup(const remixapi_StartupInfo& info);
    Result< void >                    Shutdown();
    Result< void >                    Present(const remixapi_PresentInfo* info = nullptr);
    Result< remixapi_MaterialHandle > CreateMaterial(const remixapi_MaterialInfo& info);
    Result< void >                    DestroyMaterial(remixapi_MaterialHandle handle);
    Result< remixapi_MeshHandle >     CreateMesh(const remixapi_MeshInfo& info);
    Result< void >                    DestroyMesh(remixapi_MeshHandle handle);
    Result< void >                    SetupCamera(const remixapi_CameraInfo& info);
    Result< void >                    DrawInstance(const remixapi_InstanceInfo& info);
    Result< remixapi_LightHandle >    CreateLight(const remixapi_LightInfo& info);
    Result< void >                    DestroyLight(remixapi_LightHandle handle);
    Result< void >                    DrawLightInstance(remixapi_LightHandle handle);
    Result< void >                    SetConfigVariable(const char* key, const char* value);

    // DXVK interoperability
    Result< IDirect3D9Ex* >                  dxvk_CreateD3D9(bool editorModeEnabled = false);
    Result< void >                           dxvk_RegisterD3D9Device(IDirect3DDevice9Ex* d3d9Device);
    Result< detail::dxvk_ExternalSwapchain > dxvk_GetExternalSwapchain();
    Result< detail::dxvk_VkImage >           dxvk_GetVkImage(IDirect3DSurface9* source);
    Result< void >                           dxvk_CopyRenderingOutput(IDirect3DSurface9* destination,
                                                                      remixapi_dxvk_CopyRenderingOutputType type);
    Result< void >                           dxvk_SetDefaultOutput(remixapi_dxvk_CopyRenderingOutputType type,
                                                                   const remixapi_Float4D& color);
    // Object picking utils
    template< typename CallbackLambda > // void( remix::Span<uint32_t> objectPickingValues )
    Result< void >                           pick_RequestObjectPicking(const Rect2D& region, CallbackLambda &&callback);
    Result< void >                           pick_HighlightObjects(const uint32_t* objectPickingValues_values,
                                                                   uint32_t objectPickingValues_count,
                                                                   uint8_t colorR, uint8_t colorG, uint8_t colorB);
  };

  namespace lib {
    // Helper function to load a .dll of Remix, and initialize it.
    // pRemixD3D9DllPath is a path to .dll file, e.g. "C:\dxvk-remix-nv\public\bin\d3d9.dll"
    [[nodiscard]] inline Result< Interface > loadRemixDllAndInitialize(const std::filesystem::path& remixD3D9DllPath) {

      remixapi_Interface interfaceInC = {};
      HMODULE remixDll = nullptr;

      remixapi_ErrorCode status =
        remixapi_lib_loadRemixDllAndInitialize(remixD3D9DllPath.c_str(),
                                               &interfaceInC,
                                               &remixDll);
      if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
        REMIXAPI_ASSERT(remixDll == nullptr);
        return status;
      }

      static_assert(sizeof(remixapi_Interface) == 168,
                    "Change version, update C++ wrapper when adding new functions");

      remix::Interface interfaceInCpp = {};
      {
        interfaceInCpp.m_RemixDLL = remixDll;
        interfaceInCpp.m_CInterface = interfaceInC;
      }
      static_assert(sizeof(remix::Interface) ==
                    sizeof(interfaceInCpp.m_RemixDLL) + sizeof(interfaceInCpp.m_CInterface),
                    "Not all members of \'interfaceInCpp\' are set here");

      return interfaceInCpp;
    }

    inline Result<void> shutdownAndUnloadRemixDll(Interface& interfaceInCpp) {
      return remixapi_lib_shutdownAndUnloadRemixDll(&interfaceInCpp.m_CInterface, interfaceInCpp.m_RemixDLL);
    }
  }



  inline Result< void > Interface::Startup(const remixapi_StartupInfo& info) {
    if (!m_CInterface.Startup) {
      return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    }
    return m_CInterface.Startup(&info);
  }

  inline Result< void > Interface::Shutdown() {
    if (!m_CInterface.Shutdown) {
      return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    }
    return m_CInterface.Shutdown();
  }

  inline Result< void > Interface::SetConfigVariable(const char* key, const char* value) {
    if (!m_CInterface.SetConfigVariable) {
      return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    }
    return m_CInterface.SetConfigVariable(key, value);
  }

  inline Result< void > Interface::Present(const remixapi_PresentInfo* info) {
    if (!m_CInterface.Present) {
      return REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
    }
    return m_CInterface.Present(info);
  }



  struct MaterialInfoOpaqueEXT : remixapi_MaterialInfoOpaqueEXT {
    MaterialInfoOpaqueEXT() {
      sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
      pNext = nullptr;
      roughnessTexture = {};
      metallicTexture = {};
      anisotropy = 0.0f;
      albedoConstant = { 0.2f, 0.2f, 0.2f };
      opacityConstant = 1.0f;
      roughnessConstant = 0.5f;
      metallicConstant = 0.0f;
      thinFilmThickness_hasvalue = false;
      thinFilmThickness_value = 200.f;
      alphaIsThinFilmThickness = false;
      heightTexture = {};
      displaceIn = 0.0f;
      useDrawCallAlphaState = true;
      blendType_hasvalue = false;
      blendType_value = 0;
      invertedBlend = false;
      alphaTestType = 7;
      alphaReferenceValue = 0;
      displaceOut = 0.0f;
      static_assert(sizeof remixapi_MaterialInfoOpaqueEXT == 112);
    }

    MaterialInfoOpaqueEXT(const MaterialInfoOpaqueEXT& other)
      : remixapi_MaterialInfoOpaqueEXT(other)
      , cpp_roughnessTexture(other.cpp_roughnessTexture)
      , cpp_metallicTexture(other.cpp_metallicTexture)
      , cpp_heightTexture(other.cpp_heightTexture) {
      cpp_fixPointers();
    }
    MaterialInfoOpaqueEXT(MaterialInfoOpaqueEXT&& other) noexcept
      : remixapi_MaterialInfoOpaqueEXT(other)
      , cpp_roughnessTexture(std::move(other.cpp_roughnessTexture))
      , cpp_metallicTexture(std::move(other.cpp_metallicTexture))
      , cpp_heightTexture(std::move(other.cpp_heightTexture)) {
      cpp_fixPointers();
    }
    MaterialInfoOpaqueEXT& operator=(const MaterialInfoOpaqueEXT& other) {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoOpaqueEXT::operator=(other);
      cpp_roughnessTexture = other.cpp_roughnessTexture;
      cpp_metallicTexture  = other.cpp_metallicTexture;
      cpp_heightTexture    = other.cpp_heightTexture;
      cpp_fixPointers();
      return *this;
    }
    MaterialInfoOpaqueEXT& operator=(MaterialInfoOpaqueEXT&& other) noexcept {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoOpaqueEXT::operator=(other);
      cpp_roughnessTexture = std::move(other.cpp_roughnessTexture);
      cpp_metallicTexture  = std::move(other.cpp_metallicTexture);
      cpp_heightTexture    = std::move(other.cpp_heightTexture);
      cpp_fixPointers();
      return *this;
    }

    void set_roughnessTexture(std::filesystem::path v) {
      cpp_roughnessTexture = std::move(v);
      roughnessTexture = cpp_roughnessTexture.c_str();
    }
    void set_metallicTexture(std::filesystem::path v) {
      cpp_metallicTexture = std::move(v);
      metallicTexture = cpp_metallicTexture.c_str();
    }
    void set_heightTexture(std::filesystem::path v) {
      cpp_heightTexture = std::move(v);
      heightTexture = cpp_heightTexture.c_str();
    }
    void set_thinFilmThickness(const std::optional< float >& v) {
      detail::assign_if(thinFilmThickness_hasvalue, thinFilmThickness_value, v);
    }
    void set_blendType(const std::optional< int >& v) {
      detail::assign_if(blendType_hasvalue, blendType_value, v);
    }

  private:
    void cpp_fixPointers() {
      roughnessTexture = cpp_roughnessTexture.c_str();
      metallicTexture = cpp_metallicTexture.c_str();
      heightTexture = cpp_heightTexture.c_str();
      static_assert(sizeof remixapi_MaterialInfoOpaqueEXT == 112, "Recheck pointers");
    }

    std::filesystem::path cpp_roughnessTexture {};
    std::filesystem::path cpp_metallicTexture {};
    std::filesystem::path cpp_heightTexture {};
  };

  // Can be linked to MaterialInfoOpaqueEXT
  struct MaterialInfoOpaqueSubsurfaceEXT : remixapi_MaterialInfoOpaqueSubsurfaceEXT {
    MaterialInfoOpaqueSubsurfaceEXT() {
      sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_SUBSURFACE_EXT;
      pNext = nullptr;
      subsurfaceTransmittanceTexture = {};
      subsurfaceThicknessTexture = {};
      subsurfaceSingleScatteringAlbedoTexture = {};
      subsurfaceTransmittanceColor = { 0.5f, 0.5f, 0.5f };
      subsurfaceMeasurementDistance = 0.0f;
      subsurfaceSingleScatteringAlbedo = { 0.5f, 0.5f, 0.5f };
      subsurfaceVolumetricAnisotropy = 0.0f;
      subsurfaceDiffusionProfile = false;
      subsurfaceRadius = { 0.5f, 0.5f, 0.5f };
      subsurfaceRadiusScale = 0.0f;
      subsurfaceMaxSampleRadius = 0.0f;
      subsurfaceRadiusTexture = {};
      static_assert(sizeof remixapi_MaterialInfoOpaqueSubsurfaceEXT == 104);
    }

    MaterialInfoOpaqueSubsurfaceEXT(const MaterialInfoOpaqueSubsurfaceEXT& other)
      : remixapi_MaterialInfoOpaqueSubsurfaceEXT(other)
      , cpp_subsurfaceTransmittanceTexture(other.cpp_subsurfaceTransmittanceTexture)
      , cpp_subsurfaceThicknessTexture(other.cpp_subsurfaceThicknessTexture)
      , cpp_subsurfaceSingleScatteringAlbedoTexture(other.cpp_subsurfaceSingleScatteringAlbedoTexture)
      , cpp_subsurfaceRadiusTexture(other.cpp_subsurfaceRadiusTexture) {
      cpp_fixPointers();
    }
    MaterialInfoOpaqueSubsurfaceEXT(MaterialInfoOpaqueSubsurfaceEXT&& other) noexcept
      : remixapi_MaterialInfoOpaqueSubsurfaceEXT(other)
      , cpp_subsurfaceTransmittanceTexture(std::move(other.cpp_subsurfaceTransmittanceTexture))
      , cpp_subsurfaceThicknessTexture(std::move(other.cpp_subsurfaceThicknessTexture))
      , cpp_subsurfaceSingleScatteringAlbedoTexture(std::move(other.cpp_subsurfaceSingleScatteringAlbedoTexture))
      , cpp_subsurfaceRadiusTexture(std::move(other.cpp_subsurfaceRadiusTexture)) {
      cpp_fixPointers();
    }
    MaterialInfoOpaqueSubsurfaceEXT& operator=(const MaterialInfoOpaqueSubsurfaceEXT& other) {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoOpaqueSubsurfaceEXT::operator=(other);
      cpp_subsurfaceTransmittanceTexture          = other.cpp_subsurfaceTransmittanceTexture;
      cpp_subsurfaceThicknessTexture              = other.cpp_subsurfaceThicknessTexture;
      cpp_subsurfaceSingleScatteringAlbedoTexture = other.cpp_subsurfaceSingleScatteringAlbedoTexture;
      cpp_subsurfaceRadiusTexture                 = other.cpp_subsurfaceRadiusTexture;
      cpp_fixPointers();
      return *this;
    }
    MaterialInfoOpaqueSubsurfaceEXT& operator=(MaterialInfoOpaqueSubsurfaceEXT&& other) noexcept {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoOpaqueSubsurfaceEXT::operator=(other);
      cpp_subsurfaceTransmittanceTexture          = std::move(other.cpp_subsurfaceTransmittanceTexture);
      cpp_subsurfaceThicknessTexture              = std::move(other.cpp_subsurfaceThicknessTexture);
      cpp_subsurfaceSingleScatteringAlbedoTexture = std::move(other.cpp_subsurfaceSingleScatteringAlbedoTexture);
      cpp_subsurfaceRadiusTexture                 = std::move(other.cpp_subsurfaceRadiusTexture);
      cpp_fixPointers();
      return *this;
    }

    void set_subsurfaceTransmittanceTexture(std::filesystem::path v) {
      cpp_subsurfaceTransmittanceTexture = std::move(v);
      subsurfaceTransmittanceTexture = cpp_subsurfaceTransmittanceTexture.c_str();
    }
    void set_subsurfaceThicknessTexture(std::filesystem::path v) {
      cpp_subsurfaceThicknessTexture = std::move(v);
      subsurfaceThicknessTexture = cpp_subsurfaceThicknessTexture.c_str();
    }
    void set_subsurfaceSingleScatteringAlbedoTexture(std::filesystem::path v) {
      cpp_subsurfaceSingleScatteringAlbedoTexture = std::move(v);
      subsurfaceSingleScatteringAlbedoTexture = cpp_subsurfaceSingleScatteringAlbedoTexture.c_str();
    }
    void set_subsurfaceRadiusTexture(std::filesystem::path v) {
      cpp_subsurfaceRadiusTexture = std::move(v);
      subsurfaceRadiusTexture = cpp_subsurfaceRadiusTexture.c_str();
    }

  private:
    void cpp_fixPointers() {
      subsurfaceTransmittanceTexture = cpp_subsurfaceTransmittanceTexture.c_str();
      subsurfaceThicknessTexture = cpp_subsurfaceThicknessTexture.c_str();
      subsurfaceSingleScatteringAlbedoTexture = cpp_subsurfaceSingleScatteringAlbedoTexture.c_str();
      subsurfaceRadiusTexture = cpp_subsurfaceRadiusTexture.c_str();
      static_assert(sizeof remixapi_MaterialInfoOpaqueSubsurfaceEXT == 104, "Recheck pointers");
    }

    std::filesystem::path cpp_subsurfaceTransmittanceTexture {};
    std::filesystem::path cpp_subsurfaceThicknessTexture {};
    std::filesystem::path cpp_subsurfaceSingleScatteringAlbedoTexture {};
    std::filesystem::path cpp_subsurfaceRadiusTexture{};
  };

  struct MaterialInfoTranslucentEXT : remixapi_MaterialInfoTranslucentEXT {
    MaterialInfoTranslucentEXT() {
      sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT;
      pNext = nullptr;
      transmittanceTexture = {};
      refractiveIndex = 1.3f;
      transmittanceColor = { 0.97f, 0.97f, 0.97f };
      transmittanceMeasurementDistance = 1.0f;
      thinWallThickness_hasvalue = false;
      thinWallThickness_value = 0.001f;
      useDiffuseLayer = false;
      static_assert(sizeof remixapi_MaterialInfoTranslucentEXT == 56);
    }

    MaterialInfoTranslucentEXT(const MaterialInfoTranslucentEXT& other)
      : remixapi_MaterialInfoTranslucentEXT(other)
      , cpp_transmittanceTexture(other.cpp_transmittanceTexture) {
      cpp_fixPointers();
    }
    MaterialInfoTranslucentEXT(MaterialInfoTranslucentEXT&& other) noexcept
      : remixapi_MaterialInfoTranslucentEXT(other)
      , cpp_transmittanceTexture(std::move(other.cpp_transmittanceTexture)) {
      cpp_fixPointers();
    }
    MaterialInfoTranslucentEXT& operator=(const MaterialInfoTranslucentEXT& other) {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoTranslucentEXT::operator=(other);
      cpp_transmittanceTexture = other.cpp_transmittanceTexture;
      cpp_fixPointers();
      return *this;
    }
    MaterialInfoTranslucentEXT& operator=(MaterialInfoTranslucentEXT&& other) noexcept {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfoTranslucentEXT::operator=(other);
      cpp_transmittanceTexture = std::move(other.cpp_transmittanceTexture);
      cpp_fixPointers();
      return *this;
    }

    void set_transmittanceTexture(std::filesystem::path v) {
      cpp_transmittanceTexture = std::move(v);
      transmittanceTexture = cpp_transmittanceTexture.c_str();
    }
    void set_thinWallThickness(const std::optional< float >& v) {
      detail::assign_if(thinWallThickness_hasvalue, thinWallThickness_value, v);
    }

  private:
    void cpp_fixPointers() {
      transmittanceTexture = cpp_transmittanceTexture.c_str();
      static_assert(sizeof remixapi_MaterialInfoTranslucentEXT == 56, "Recheck pointers");
    }

    std::filesystem::path cpp_transmittanceTexture {};
  };

  struct MaterialInfoPortalEXT : remixapi_MaterialInfoPortalEXT {
    MaterialInfoPortalEXT() {
      sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_PORTAL_EXT;
      pNext = nullptr;
      rayPortalIndex = 0;
      rotationSpeed = 0.0f;
      static_assert(sizeof remixapi_MaterialInfoPortalEXT == 24);
    }
  };

  struct MaterialInfo : remixapi_MaterialInfo {
    MaterialInfo() {
      sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
      pNext = nullptr;
      hash = 0;
      albedoTexture = {};
      normalTexture = {};
      tangentTexture = {};
      emissiveTexture = {};
      emissiveIntensity = 40.0f;
      emissiveColorConstant = { 0.0f, 0.0f, 0.0f };
      spriteSheetRow = 1;
      spriteSheetCol = 1;
      spriteSheetFps = 0;
      filterMode = 1; // Linear
      wrapModeU = 1; // Repeat
      wrapModeV = 1; // Repeat
      static_assert(sizeof remixapi_MaterialInfo == 80);
    }

    MaterialInfo(const MaterialInfo& other)
      : remixapi_MaterialInfo(other)
      , cpp_albedoTexture(other.cpp_albedoTexture)
      , cpp_normalTexture(other.cpp_normalTexture)
      , cpp_tangentTexture(other.cpp_tangentTexture)
      , cpp_emissiveTexture(other.cpp_emissiveTexture) {
      cpp_fixPointers();
    }
    MaterialInfo(MaterialInfo&& other) noexcept
      : remixapi_MaterialInfo(other)
      , cpp_albedoTexture(std::move(other.cpp_albedoTexture))
      , cpp_normalTexture(std::move(other.cpp_normalTexture))
      , cpp_tangentTexture(std::move(other.cpp_tangentTexture))
      , cpp_emissiveTexture(std::move(other.cpp_emissiveTexture)) {
      cpp_fixPointers();
    }
    MaterialInfo& operator=(const MaterialInfo& other) {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfo::operator=(other);
      cpp_albedoTexture   = other.cpp_albedoTexture;
      cpp_normalTexture   = other.cpp_normalTexture;
      cpp_tangentTexture  = other.cpp_tangentTexture;
      cpp_emissiveTexture = other.cpp_emissiveTexture;
      cpp_fixPointers();
      return *this;
    }
    MaterialInfo& operator=(MaterialInfo&& other) noexcept {
      if (this == &other) {
        return *this;
      }
      remixapi_MaterialInfo::operator=(other);
      cpp_albedoTexture   = std::move(other.cpp_albedoTexture);
      cpp_normalTexture   = std::move(other.cpp_normalTexture);
      cpp_tangentTexture  = std::move(other.cpp_tangentTexture);
      cpp_emissiveTexture = std::move(other.cpp_emissiveTexture);
      cpp_fixPointers();
      return *this;
    }

    void set_albedoTexture(std::filesystem::path v) {
      cpp_albedoTexture = std::move(v);
      albedoTexture = cpp_albedoTexture.c_str();
    }
    void set_normalTexture(std::filesystem::path v) {
      cpp_normalTexture = std::move(v);
      normalTexture = cpp_normalTexture.c_str();
    }
    void set_tangentTexture(std::filesystem::path v) {
      cpp_tangentTexture = std::move(v);
      tangentTexture = cpp_tangentTexture.c_str();
    }
    void set_emissiveTexture(std::filesystem::path v) {
      cpp_emissiveTexture = std::move(v);
      emissiveTexture = cpp_emissiveTexture.c_str();
    }

  private:
    void cpp_fixPointers() {
      albedoTexture = cpp_albedoTexture.c_str();
      normalTexture = cpp_normalTexture.c_str();
      tangentTexture = cpp_tangentTexture.c_str();
      emissiveTexture = cpp_emissiveTexture.c_str();
      static_assert(sizeof remixapi_MaterialInfo == 80, "Recheck pointers");
    }

    std::filesystem::path cpp_albedoTexture {};
    std::filesystem::path cpp_normalTexture {};
    std::filesystem::path cpp_tangentTexture {};
    std::filesystem::path cpp_emissiveTexture {};
  };

  inline Result< remixapi_MaterialHandle > Interface::CreateMaterial(const remixapi_MaterialInfo& info) {
    remixapi_MaterialHandle handle = nullptr;
    remixapi_ErrorCode status = m_CInterface.CreateMaterial(&info, &handle);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return handle;
  }

  inline Result< void > Interface::DestroyMaterial(remixapi_MaterialHandle handle) {
    return m_CInterface.DestroyMaterial(handle);
  }



  struct MeshInfo : remixapi_MeshInfo {
    MeshInfo() {
      sType = REMIXAPI_STRUCT_TYPE_MESH_INFO;
      pNext = nullptr;
      hash = 0;
      surfaces_values = {};
      surfaces_count = 0;
      static_assert(sizeof remixapi_MeshInfo == 40);
    }
  };

  inline Result< remixapi_MeshHandle > Interface::CreateMesh(const remixapi_MeshInfo& info) {
    remixapi_MeshHandle handle = nullptr;
    remixapi_ErrorCode status = m_CInterface.CreateMesh(&info, &handle);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return handle;
  }

  inline Result< void > Interface::DestroyMesh(remixapi_MeshHandle handle) {
    return m_CInterface.DestroyMesh(handle);
  }



  using CameraType = remixapi_CameraType;

  // Ignores view / projection matrices from CameraInfo
  // by recalculating them from the given arguments in this struct.
  struct CameraInfoParameterizedEXT : remixapi_CameraInfoParameterizedEXT {
    CameraInfoParameterizedEXT() {
      sType = { REMIXAPI_STRUCT_TYPE_CAMERA_INFO_PARAMETERIZED_EXT };
      pNext = { nullptr };
      position = { 0, 0, 0 };
      forward = { 0, 0, 1 };
      up = { 0, 1, 0 };
      right = { 1, 0, 0 };
      fovYInDegrees = 75.f;
      aspect = 16.f / 9.f;
      nearPlane = 0.1f;
      farPlane = 1000.f;
      static_assert(sizeof remixapi_CameraInfoParameterizedEXT == 80);
    }
  };

  struct CameraInfo : remixapi_CameraInfo {
    CameraInfo() {
      sType = { REMIXAPI_STRUCT_TYPE_CAMERA_INFO };
      pNext = { nullptr };
      type = { REMIXAPI_CAMERA_TYPE_WORLD };
      view[0][0] = view[1][1] = view[2][2] = view[3][3] = 1.f;
      projection[0][0] = projection[1][1] = projection[2][2] = projection[3][3] = 1.f;
      static_assert(sizeof remixapi_CameraInfo == 152);
    }
  };

  inline Result< void > Interface::SetupCamera(const remixapi_CameraInfo& info) {
    return m_CInterface.SetupCamera(&info);
  }



  struct InstanceInfoBoneTransformsEXT : remixapi_InstanceInfoBoneTransformsEXT {
    InstanceInfoBoneTransformsEXT() {
      sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT;
      pNext = nullptr;
      boneTransforms_count = 0;
      boneTransforms_values = {};
      static_assert(sizeof remixapi_InstanceInfoBoneTransformsEXT == 32);
    }
  };

  struct InstanceInfoBlendEXT : remixapi_InstanceInfoBlendEXT {
    InstanceInfoBlendEXT() {
      sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT;
      pNext = nullptr;
      alphaTestEnabled = false;
      alphaTestReferenceValue = 0;
      alphaTestCompareOp = 7 /* VK_COMPARE_OP_ALWAYS */;
      alphaBlendEnabled = false;
      srcColorBlendFactor = 1 /* VK_BLEND_FACTOR_ONE */;
      dstColorBlendFactor = 0 /* VK_BLEND_FACTOR_ZERO */;
      colorBlendOp = 0 /* VK_BLEND_OP_ADD */;
      srcAlphaBlendFactor = 1 /* VK_BLEND_FACTOR_ONE */;
      dstAlphaBlendFactor = 0 /* VK_BLEND_FACTOR_ZERO */;
      alphaBlendOp = 0 /* VK_BLEND_OP_ADD */;
      writeMask = 0xFFFFFFFF;
      textureColorArg1Source = 1 /* RtTextureArgSource::Texture */;
      textureColorArg2Source = 0 /* RtTextureArgSource::None */;
      textureColorOperation = 3 /* DxvkRtTextureOperation::Modulate */;
      textureAlphaArg1Source = 1 /* RtTextureArgSource::Texture */;
      textureAlphaArg2Source = 0 /* RtTextureArgSource::None */;
      textureAlphaOperation = 1 /* DxvkRtTextureOperation::SelectArg1 */;
      tFactor = 0XFFFFFFFF;
      isTextureFactorBlend = false;
      isVertexColorBakedLighting = true;
      static_assert(sizeof remixapi_InstanceInfoBlendEXT == 96);
    }
  };

  struct InstanceInfoObjectPickingEXT : remixapi_InstanceInfoObjectPickingEXT {
    InstanceInfoObjectPickingEXT() {
      sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_OBJECT_PICKING_EXT;
      pNext = nullptr;
      objectPickingValue = 0;
    }
  };

  struct InstanceInfoParticleSystemEXT : remixapi_InstanceInfoParticleSystemEXT {
    InstanceInfoParticleSystemEXT() {
      sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_PARTICLE_SYSTEM_EXT;
      pNext = nullptr;
      maxNumParticles = 10000;
      spawnRatePerSecond = 0.f;
      minTimeToLive = 1.0f;
      maxTimeToLive = 1.0f;
      minSpawnSize = 10.0f;
      maxSpawnSize = 10.0f;
      minSpawnRotationSpeed = 0.0f;
      maxSpawnRotationSpeed = 0.0f;
      minSpawnColor = {1, 1, 1, 1};
      maxSpawnColor = {1, 1, 1, 1};
      minTargetSize = 0.0f;
      maxTargetSize = 0.0f;
      minTargetRotationSpeed = 0.0f;
      maxTargetRotationSpeed = 0.0f;
      minTargetColor = { 1, 1, 1, 0 };
      maxTargetColor = { 1, 1, 1, 0 };
      useSpawnTexcoords = false;
      initialVelocityFromMotion = 0.0f;
      initialVelocityFromNormal = 0.0f;
      initialVelocityConeAngleDegrees = 0.0f;
      maxSpeed = -1.0f;
      gravityForce = -0.98f;
      useTurbulence = false;
      turbulenceForce = 5.0f;
      turbulenceFrequency = 0.05f;
      enableCollisionDetection = false;
      collisionRestitution = 0.5f;
      collisionThickness = 5.0f;
      alignParticlesToVelocity = false;
      enableMotionTrail = false;
      motionTrailMultiplier = 1.0f;
      hideEmitter = false;
      billboardType = 0;
      static_assert(sizeof InstanceInfoParticleSystemEXT == 200);
    }
  };

  using InstanceCategoryBit = remixapi_InstanceCategoryBit;
  using InstanceCategoryFlags = remixapi_InstanceCategoryFlags;

  struct InstanceInfo : remixapi_InstanceInfo {
    InstanceInfo() {
      sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
      pNext = nullptr;
      categoryFlags = 0;
      mesh = 0;
      transform = {};
      doubleSided = false;
      static_assert(sizeof remixapi_InstanceInfo == 88);
    }
  };

  inline Result< void > Interface::DrawInstance(const remixapi_InstanceInfo& info) {
    return m_CInterface.DrawInstance(&info);
  }



  namespace detail {
    inline remixapi_LightInfoLightShaping defaultLightShaping() {
      remixapi_LightInfoLightShaping shaping {};
      {
        shaping.direction = { 0.0f, 0.0f, 1.0f };
        shaping.coneAngleDegrees = 180.0f;
        shaping.coneSoftness = 0.0f;
        shaping.focusExponent = 0.0f;
      }
      return shaping;
    };
  }

  using LightInfoLightShaping = remixapi_LightInfoLightShaping;

  struct LightInfoSphereEXT : remixapi_LightInfoSphereEXT {
    LightInfoSphereEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
      pNext = nullptr;
      position = { 0.0f, 0.0f, 0.0f };
      radius = 0.05f;
      shaping_hasvalue = false;
      shaping_value = detail::defaultLightShaping();
      volumetricRadianceScale = 1.0f;
      static_assert(sizeof remixapi_LightInfoSphereEXT == 64);
    }

    void set_shaping(const std::optional< remixapi_LightInfoLightShaping >& v) {
      detail::assign_if(shaping_hasvalue, shaping_value, v);
    }
  };

  struct LightInfoRectEXT : remixapi_LightInfoRectEXT {
    LightInfoRectEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_RECT_EXT;
      pNext = nullptr;
      position = { 0.0f, 0.0f, 0.0f };
      xAxis = { 1.0f, 0.0f, 0.0f };
      xSize = 1.0f;
      yAxis = { 0.0f, 1.0f, 0.0f };
      ySize = 1.0f;
      direction = { 0.0f, 0.0f, 1.0f };
      shaping_hasvalue = false;
      shaping_value = detail::defaultLightShaping();
      volumetricRadianceScale = 1.0f;
      static_assert(sizeof remixapi_LightInfoRectEXT == 104);
    }

    void set_shaping(const std::optional< remixapi_LightInfoLightShaping >& v) {
      detail::assign_if(shaping_hasvalue, shaping_value, v);
    }
  };

  struct LightInfoDiskEXT : remixapi_LightInfoDiskEXT {
    LightInfoDiskEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISK_EXT;
      pNext = nullptr;
      position = { 0.0f, 0.0f, 0.0f };
      xAxis = { 1.0f, 0.0f, 0.0f };
      xRadius = 1.0f;
      yAxis = { 0.0f, 1.0f, 0.0f };
      yRadius = 1.0f;
      direction = { 0.0f, 0.0f, 1.0f };
      shaping_hasvalue = false;
      shaping_value = detail::defaultLightShaping();
      volumetricRadianceScale = 1.0f;
      static_assert(sizeof remixapi_LightInfoDiskEXT == 104);
    }

    void set_shaping(const std::optional< remixapi_LightInfoLightShaping >& v) {
      detail::assign_if(shaping_hasvalue, shaping_value, v);
    }
  };

  struct LightInfoCylinderEXT : remixapi_LightInfoCylinderEXT {
    LightInfoCylinderEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_CYLINDER_EXT;
      pNext = nullptr;
      position = { 0.0f, 0.0f, 0.0f };
      radius = 1.0f;
      axis = { 1.0f, 0.0f, 0.0f };
      axisLength = 1.0f;
      volumetricRadianceScale = 1.0f;
      static_assert(sizeof remixapi_LightInfoCylinderEXT == 56);
    }
  };

  struct LightInfoDistantEXT : remixapi_LightInfoDistantEXT {
    LightInfoDistantEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
      pNext = nullptr;
      direction = { 0.0f, -1.0f, 0.0f };
      angularDiameterDegrees = 0.5f;
      volumetricRadianceScale = 1.0f;
      static_assert(sizeof remixapi_LightInfoDistantEXT == 40);
    }
  };

  struct LightInfoDomeEXT : remixapi_LightInfoDomeEXT {
    LightInfoDomeEXT() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DOME_EXT;
      pNext = nullptr;
      transform = {};
      colorTexture = {};
      static_assert(sizeof remixapi_LightInfoDomeEXT == 72);
    }

    LightInfoDomeEXT(const LightInfoDomeEXT& other)
      : remixapi_LightInfoDomeEXT(other)
      , cpp_colorTexture(other.cpp_colorTexture) {
      cpp_fixPointers();
    }
    LightInfoDomeEXT(LightInfoDomeEXT&& other) noexcept
      : remixapi_LightInfoDomeEXT(other)
      , cpp_colorTexture(std::move(other.cpp_colorTexture)) {
      cpp_fixPointers();
    }
    LightInfoDomeEXT& operator=(const LightInfoDomeEXT& other) {
      if (this == &other) {
        return *this;
      }
      remixapi_LightInfoDomeEXT::operator=(other);
      cpp_colorTexture = other.cpp_colorTexture;
      cpp_fixPointers();
      return *this;
    }
    LightInfoDomeEXT& operator=(LightInfoDomeEXT&& other) noexcept {
      if (this == &other) {
        return *this;
      }
      remixapi_LightInfoDomeEXT::operator=(other);
      cpp_colorTexture = std::move(other.cpp_colorTexture);
      cpp_fixPointers();
      return *this;
    }

    void set_colorTexture(std::filesystem::path v) {
      cpp_colorTexture = std::move(v);
      colorTexture = cpp_colorTexture.c_str();
    }

  private:
    void cpp_fixPointers() {
      colorTexture = cpp_colorTexture.c_str();
      static_assert(sizeof remixapi_LightInfoDomeEXT == 72, "Recheck pointers");
    }

    std::filesystem::path cpp_colorTexture {};
  };

  struct LightInfo : remixapi_LightInfo {
    LightInfo() {
      sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
      pNext = nullptr;
      hash = 0;
      radiance = { 1.0f, 1.0f, 1.0f };
      static_assert(sizeof remixapi_LightInfo == 40);
    }
  };

  inline Result< remixapi_LightHandle > Interface::CreateLight(const remixapi_LightInfo& info) {
    remixapi_LightHandle handle = nullptr;
    remixapi_ErrorCode status = m_CInterface.CreateLight(&info, &handle);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return handle;
  }

  inline Result< void > Interface::DestroyLight(remixapi_LightHandle handle) {
    return m_CInterface.DestroyLight(handle);
  }

  inline Result< void > Interface::DrawLightInstance(remixapi_LightHandle handle) {
    return m_CInterface.DrawLightInstance(handle);
  }

  namespace detail {
    struct dxvk_ExternalSwapchain {
      uint64_t vkImage;
      uint64_t vkSemaphoreRenderingDone;
      uint64_t vkSemaphoreResumeSemaphore;
    };

    struct dxvk_VkImage {
      uint64_t vkImage;
    };
  }

  inline Result< IDirect3D9Ex* > Interface::dxvk_CreateD3D9(bool editorModeEnabled) {
    IDirect3D9Ex* d3d9 { nullptr };
    remixapi_ErrorCode status = m_CInterface.dxvk_CreateD3D9(editorModeEnabled, &d3d9);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return d3d9;
  }

  inline Result< void > Interface::dxvk_RegisterD3D9Device(IDirect3DDevice9Ex* d3d9Device) {
    return m_CInterface.dxvk_RegisterD3D9Device(d3d9Device);
  }

  inline Result< detail::dxvk_ExternalSwapchain > Interface::dxvk_GetExternalSwapchain() {
    detail::dxvk_ExternalSwapchain externalSwapchain {};
    remixapi_ErrorCode status = m_CInterface.dxvk_GetExternalSwapchain(
      &externalSwapchain.vkImage,
      &externalSwapchain.vkSemaphoreRenderingDone,
      &externalSwapchain.vkSemaphoreResumeSemaphore);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return externalSwapchain;
  }

  inline Result< detail::dxvk_VkImage > Interface::dxvk_GetVkImage(IDirect3DSurface9* source) {
    detail::dxvk_VkImage externalImage {};
    remixapi_ErrorCode status = m_CInterface.dxvk_GetVkImage(source, &externalImage.vkImage);
    if (status != REMIXAPI_ERROR_CODE_SUCCESS) {
      return status;
    }
    return externalImage;
  }

  inline Result< void > Interface::dxvk_CopyRenderingOutput(
      IDirect3DSurface9* destination, remixapi_dxvk_CopyRenderingOutputType type) {
    return m_CInterface.dxvk_CopyRenderingOutput(destination, type);
  }

  inline Result< void > Interface::dxvk_SetDefaultOutput(
      remixapi_dxvk_CopyRenderingOutputType type, const remixapi_Float4D& color) {
    return m_CInterface.dxvk_SetDefaultOutput(type, &color);
  }

  template< typename CallbackLambda >
  inline Result< void > Interface::pick_RequestObjectPicking(const Rect2D& region, CallbackLambda&& callback) {
    using Func = std::function< void(Span<uint32_t>) >;

    static auto bootstrapForC = [](const uint32_t* objectPickingValues_values,
                                   uint32_t objectPickingValues_count,
                                   void* callbackUserData) {
      // unwrap 'callbackUserData', it is a user's lambda
      if (auto* userLambda = static_cast<Func*>(callbackUserData)) {
        auto arg = Span<uint32_t>{ objectPickingValues_values, objectPickingValues_count };
        // call user's lambda
        (*userLambda)(arg);
        delete userLambda;
      }
    };

    // pass user's lambda as as 'callbackUserData'
    auto* userLambda = new Func { callback };
    return m_CInterface.pick_RequestObjectPicking(&region, bootstrapForC, userLambda);
  }

  inline Result< void > Interface::pick_HighlightObjects(const uint32_t* objectPickingValues_values,
                                                         uint32_t objectPickingValues_count,
                                                         uint8_t colorR, uint8_t colorG, uint8_t colorB) {
    return m_CInterface.pick_HighlightObjects(objectPickingValues_values,
                                              objectPickingValues_count,
                                              colorR, colorG, colorB);
  }
}
