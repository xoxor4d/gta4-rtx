#include "std_include.hpp"
#include "d3d9ex.hpp"
#include "renderer_api.hpp"

#include "imgui.hpp"
#include "renderer.hpp"
#include "remix_vars.hpp"
#include "shared/common/shader_cache.hpp"

namespace gta4
{
#pragma region D3D9Device

	HRESULT d3d9ex::D3D9Device::QueryInterface(REFIID riid, void** ppvObj)
	{
		*ppvObj = nullptr;

		HRESULT hRes = m_pIDirect3DDevice9->QueryInterface(riid, ppvObj);
		//shared::common::log("d3d9", "m_pIDirect3DDevice9->QueryInterface", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

		if (hRes == NOERROR) {
			*ppvObj = this;
		}

		return hRes;
	}

	ULONG d3d9ex::D3D9Device::AddRef()
	{
		return m_pIDirect3DDevice9->AddRef();
	}

	ULONG d3d9ex::D3D9Device::Release()
	{
		ULONG count = m_pIDirect3DDevice9->Release();
		if (!count)
		{
			if (shared::globals::d3d_device == this) {
				shared::globals::d3d_device = nullptr;
			}
			delete this;
		}
		return count;
	}

	HRESULT d3d9ex::D3D9Device::TestCooperativeLevel()
	{
		return m_pIDirect3DDevice9->TestCooperativeLevel();
	}

	UINT d3d9ex::D3D9Device::GetAvailableTextureMem()
	{
		return m_pIDirect3DDevice9->GetAvailableTextureMem();
	}

	HRESULT d3d9ex::D3D9Device::EvictManagedResources()
	{
		return m_pIDirect3DDevice9->EvictManagedResources();
	}

	HRESULT d3d9ex::D3D9Device::GetDirect3D(IDirect3D9** ppD3D9)
	{
		return m_pIDirect3DDevice9->GetDirect3D(ppD3D9);
	}

	HRESULT d3d9ex::D3D9Device::GetDeviceCaps(D3DCAPS9* pCaps)
	{
		return m_pIDirect3DDevice9->GetDeviceCaps(pCaps);
	}

	HRESULT d3d9ex::D3D9Device::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode)
	{
		return m_pIDirect3DDevice9->GetDisplayMode(iSwapChain, pMode);
	}

	HRESULT d3d9ex::D3D9Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
	{
		return m_pIDirect3DDevice9->GetCreationParameters(pParameters);
	}

	HRESULT d3d9ex::D3D9Device::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap)
	{
		return m_pIDirect3DDevice9->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
	}

	void d3d9ex::D3D9Device::SetCursorPosition(int X, int Y, DWORD Flags)
	{
		return m_pIDirect3DDevice9->SetCursorPosition(X, Y, Flags);
	}

	BOOL d3d9ex::D3D9Device::ShowCursor(BOOL bShow)
	{
		return m_pIDirect3DDevice9->ShowCursor(bShow);
	}

	HRESULT d3d9ex::D3D9Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
	{
		return m_pIDirect3DDevice9->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
	}

	HRESULT d3d9ex::D3D9Device::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
	{
		return m_pIDirect3DDevice9->GetSwapChain(iSwapChain, pSwapChain);
	}

	UINT d3d9ex::D3D9Device::GetNumberOfSwapChains()
	{
		return m_pIDirect3DDevice9->GetNumberOfSwapChains();
	}

	const char* d3d9ex::call_type_name(const call_type t)
	{
		switch (t)
		{
		case call_type::RenderState: return "render state";
		case call_type::TextureStageState: return "texture stage state";
		case call_type::SamplerState: return "sampler state";
		case call_type::Texture: return "texture";
		case call_type::Transform: return "transform";
		case call_type::ShaderConstant: return "shader constant";
		case call_type::Shader: return "shader";
		case call_type::VertexDeclaration: return "vertex declaration";
		case call_type::StreamSource: return "stream source";
		case call_type::Indices: return "indices";
		case call_type::Draw: return "draw";
		default: return "?";
		}
	}

	void d3d9ex::on_end_scene()
	{
		s_calls_last_frame = s_calls_this_frame;
		s_calls_this_frame = {};
		PROFILE_PLOT("d3d9 draw calls", s_calls_last_frame[static_cast<std::size_t>(call_type::Draw)]);
	}

	// D3D9 resets viewport and scissor to the full target on SetRenderTarget(0) and Reset but the bridge's cached state does not.
	// The game reads the viewport back after switching targets, so it would keep re-applying the pre-resolution-change viewport.
	static void reset_viewport_to_target(IDirect3DDevice9* dev, IDirect3DSurface9* target)
	{
		D3DSURFACE_DESC desc = {};
		if (!target || FAILED(target->GetDesc(&desc))) {
			return;
		}

		const D3DVIEWPORT9 vp = { 0u, 0u, desc.Width, desc.Height, 0.0f, 1.0f };
		dev->SetViewport(&vp);

		const RECT rect = { 0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height) };
		dev->SetScissorRect(&rect);
	}

	HRESULT d3d9ex::D3D9Device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		PROFILE_ZONE();
		PROFILE_MESSAGE("D3D9 device reset");
		shared::common::g_shader_cache.clear_cache();
		shared::imgui::blur::release_resources();
		imgui::on_device_reset();
		ImGui_ImplDX9_InvalidateDeviceObjects();
		const auto hr = m_pIDirect3DDevice9->Reset(pPresentationParameters);

		if (SUCCEEDED(hr))
		{
			IDirect3DSurface9* backbuffer = nullptr;
			if (SUCCEEDED(m_pIDirect3DDevice9->GetRenderTarget(0, &backbuffer)) && backbuffer)
			{
				reset_viewport_to_target(m_pIDirect3DDevice9, backbuffer);
				backbuffer->Release();
			}
		}

		ImGui_ImplDX9_CreateDeviceObjects();
		return hr;
	}

	// not in use?
	HRESULT d3d9ex::D3D9Device::Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
	{
		return m_pIDirect3DDevice9->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}

	HRESULT d3d9ex::D3D9Device::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
	{
		return m_pIDirect3DDevice9->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
	}

	HRESULT d3d9ex::D3D9Device::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
	{
		return m_pIDirect3DDevice9->GetRasterStatus(iSwapChain, pRasterStatus);
	}

	HRESULT d3d9ex::D3D9Device::SetDialogBoxMode(BOOL bEnableDialogs)
	{
		return m_pIDirect3DDevice9->SetDialogBoxMode(bEnableDialogs);
	}

	void d3d9ex::D3D9Device::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp)
	{
		return m_pIDirect3DDevice9->SetGammaRamp(iSwapChain, Flags, pRamp);
	}

	void d3d9ex::D3D9Device::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
	{
		return m_pIDirect3DDevice9->GetGammaRamp(iSwapChain, pRamp);
	}

	HRESULT d3d9ex::D3D9Device::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }

		const auto hr = m_pIDirect3DDevice9->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
		if (SUCCEEDED(hr) && ppTexture && *ppTexture) {
			renderer_api::on_texture_created(*ppTexture);
		}
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }

		return m_pIDirect3DDevice9->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }

		return m_pIDirect3DDevice9->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }

		return m_pIDirect3DDevice9->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; Usage |= D3DUSAGE_DYNAMIC; }

		return m_pIDirect3DDevice9->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint)
	{
		PROFILE_ZONE();
		const auto hr = m_pIDirect3DDevice9->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
		if (SUCCEEDED(hr) && pDestinationSurface)
		{
			IDirect3DBaseTexture9* texture = nullptr;
			if (SUCCEEDED(pDestinationSurface->GetContainer(__uuidof(IDirect3DBaseTexture9), reinterpret_cast<void**>(&texture))) && texture)
			{
				renderer_api::on_texture_written(texture);
				texture->Release();
			}
		}
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture)
	{
		PROFILE_ZONE();
		const auto hr = m_pIDirect3DDevice9->UpdateTexture(pSourceTexture, pDestinationTexture);
		if (SUCCEEDED(hr)) {
			renderer_api::on_texture_written(pDestinationTexture);
		}
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->GetRenderTargetData(pRenderTarget, pDestSurface);
	}

	HRESULT d3d9ex::D3D9Device::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface)
	{
		return m_pIDirect3DDevice9->GetFrontBufferData(iSwapChain, pDestSurface);
	}

	HRESULT d3d9ex::D3D9Device::StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
	}

	HRESULT d3d9ex::D3D9Device::ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color)
	{
		return m_pIDirect3DDevice9->ColorFill(pSurface, pRect, color);
	}

	HRESULT d3d9ex::D3D9Device::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		PROFILE_ZONE();
		//if (Pool == D3DPOOL_MANAGED) { Pool = D3DPOOL_DEFAULT; }

		return m_pIDirect3DDevice9->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
	}

	HRESULT d3d9ex::D3D9Device::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
	{
		const auto hr = m_pIDirect3DDevice9->SetRenderTarget(RenderTargetIndex, pRenderTarget);
		if (SUCCEEDED(hr) && RenderTargetIndex == 0u) {
			reset_viewport_to_target(m_pIDirect3DDevice9, pRenderTarget);
		}
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget)
	{
		return m_pIDirect3DDevice9->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
	}

	HRESULT d3d9ex::D3D9Device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
	{
		return m_pIDirect3DDevice9->SetDepthStencilSurface(pNewZStencil);
	}

	HRESULT d3d9ex::D3D9Device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
	{
		return m_pIDirect3DDevice9->GetDepthStencilSurface(ppZStencilSurface);
	}

	HRESULT d3d9ex::D3D9Device::BeginScene()
	{
		PROFILE_ZONE();
		if (const auto p = performance_logger::get(); p) {
			p->begin_frame();
		}

		if (renderer::is_initialized()) {
			on_begin_scene_cb();
		}
		
		return m_pIDirect3DDevice9->BeginScene();
	}

	HRESULT d3d9ex::D3D9Device::EndScene()
	{
		shared::common::profiler::name_current_thread("Render");

		if (imgui::is_initialized()) {
			imgui::get()->on_present();
		}

		const auto current_time = std::chrono::high_resolution_clock::now();
		if (shared::globals::last_frame_time.time_since_epoch().count() != 0)
		{
			const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - shared::globals::last_frame_time).count();
			shared::globals::frame_time_ms = delta / 1000.0f; // microseconds to ms
		}

		shared::globals::last_frame_time = current_time;
		remix_vars::on_client_frame();
		renderer_api::on_end_scene();
		on_end_scene();

		//renderer::get()->m_modified_draw_prim = false;

		// delay reset of phone_last_frame to end of next frame
		if (!g_was_rendering_phone_last_frame_delay_reset_helper) {
			g_was_rendering_phone_last_frame_delay_reset_helper = true;
		} 
		else
		{
			g_was_rendering_phone_last_frame = false;
			g_was_rendering_phone_last_frame_delay_reset_helper = false;
		}

		HRESULT hr = D3D_OK;
		{
			GTA4_PERF_SCOPE(performance_section::Submit);
			hr = m_pIDirect3DDevice9->EndScene();
		}

		if (const auto p = performance_logger::get(); p) {
			p->end_frame(shared::globals::frame_time_ms);
		}

		PROFILE_PLOT("frame time (ms)", shared::globals::frame_time_ms);
		PROFILE_FRAME_MARK();
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
	{
		PROFILE_ZONE();
		g_is_rendering_phone = shared::utils::float_equal(Z, 0.1337f);

		if (g_is_rendering_phone) 
		{
			g_was_rendering_phone_last_frame = true;
			g_was_rendering_phone_last_frame_delay_reset_helper = false; // reset helper if phone was drawn this frame
		}

		/*if (imgui::is_initialized())
		{
			const auto im = imgui::get();
			if (im->m_dbg_debug_shader_float_constants)
			{
				{
					float a = ((Color >> 24) & 0xFF) / 255.0f;
					float r = ((Color >> 16) & 0xFF) / 255.0f;
					float g = ((Color >> 8) & 0xFF) / 255.0f;
					float b = ((Color >> 0) & 0xFF) / 255.0f;
					shared::common::log("CLEAR", std::format("r:{} g:{} b:{} a:{} --- z: {}",
						r, g, b, a, Z), shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
				}
			}
		}*/

		return m_pIDirect3DDevice9->Clear(Count, pRects, Flags, Color, Z, Stencil);
	}

	HRESULT d3d9ex::D3D9Device::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix)
	{
		count_call(call_type::Transform);
		return m_pIDirect3DDevice9->SetTransform(State, pMatrix);
	}

	HRESULT d3d9ex::D3D9Device::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix)
	{
		return m_pIDirect3DDevice9->GetTransform(State, pMatrix);
	}

	HRESULT d3d9ex::D3D9Device::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix)
	{
		return m_pIDirect3DDevice9->MultiplyTransform(State, pMatrix);
	}

	HRESULT d3d9ex::D3D9Device::SetViewport(CONST D3DVIEWPORT9* pViewport)
	{
		return m_pIDirect3DDevice9->SetViewport(pViewport);
	}

	HRESULT d3d9ex::D3D9Device::GetViewport(D3DVIEWPORT9* pViewport)
	{
		return m_pIDirect3DDevice9->GetViewport(pViewport);
	}

	HRESULT d3d9ex::D3D9Device::SetMaterial(CONST D3DMATERIAL9* pMaterial)
	{
		return m_pIDirect3DDevice9->SetMaterial(pMaterial);
	}

	HRESULT d3d9ex::D3D9Device::GetMaterial(D3DMATERIAL9* pMaterial)
	{
		return m_pIDirect3DDevice9->GetMaterial(pMaterial);
	}

	HRESULT d3d9ex::D3D9Device::SetLight(DWORD Index, CONST D3DLIGHT9* pLight)
	{
		return m_pIDirect3DDevice9->SetLight(Index, pLight);
	}

	HRESULT d3d9ex::D3D9Device::GetLight(DWORD Index, D3DLIGHT9* pLight)
	{
		return m_pIDirect3DDevice9->GetLight(Index, pLight);
	}

	HRESULT d3d9ex::D3D9Device::LightEnable(DWORD Index, BOOL Enable)
	{
		return m_pIDirect3DDevice9->LightEnable(Index, Enable);
	}

	HRESULT d3d9ex::D3D9Device::GetLightEnable(DWORD Index, BOOL* pEnable)
	{
		return m_pIDirect3DDevice9->GetLightEnable(Index, pEnable);
	}

	HRESULT d3d9ex::D3D9Device::SetClipPlane(DWORD Index, CONST float* pPlane)
	{
		return m_pIDirect3DDevice9->SetClipPlane(Index, pPlane);
	}

	HRESULT d3d9ex::D3D9Device::GetClipPlane(DWORD Index, float* pPlane)
	{
		return m_pIDirect3DDevice9->GetClipPlane(Index, pPlane);
	}

	HRESULT d3d9ex::D3D9Device::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
	{
		count_call(call_type::RenderState);
		return m_pIDirect3DDevice9->SetRenderState(State, Value);
	}

	HRESULT d3d9ex::D3D9Device::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue)
	{
		return m_pIDirect3DDevice9->GetRenderState(State, pValue);
	}

	HRESULT d3d9ex::D3D9Device::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB)
	{
		return m_pIDirect3DDevice9->CreateStateBlock(Type, ppSB);
	}

	HRESULT d3d9ex::D3D9Device::BeginStateBlock()
	{
		return m_pIDirect3DDevice9->BeginStateBlock();
	}

	HRESULT d3d9ex::D3D9Device::EndStateBlock(IDirect3DStateBlock9** ppSB)
	{
		return m_pIDirect3DDevice9->EndStateBlock(ppSB);
	}

	HRESULT d3d9ex::D3D9Device::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus)
	{
		return m_pIDirect3DDevice9->SetClipStatus(pClipStatus);
	}

	HRESULT d3d9ex::D3D9Device::GetClipStatus(D3DCLIPSTATUS9* pClipStatus)
	{
		return m_pIDirect3DDevice9->GetClipStatus(pClipStatus);
	}

	HRESULT d3d9ex::D3D9Device::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture)
	{
		return m_pIDirect3DDevice9->GetTexture(Stage, ppTexture);
	}

	HRESULT d3d9ex::D3D9Device::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture)
	{
		count_call(call_type::Texture);
		return m_pIDirect3DDevice9->SetTexture(Stage, pTexture);  
	}

	HRESULT d3d9ex::D3D9Device::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
	{
		return m_pIDirect3DDevice9->GetTextureStageState(Stage, Type, pValue);
	}

	HRESULT d3d9ex::D3D9Device::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
	{
		count_call(call_type::TextureStageState);
		return m_pIDirect3DDevice9->SetTextureStageState(Stage, Type, Value);
	}

	HRESULT d3d9ex::D3D9Device::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
	{
		return m_pIDirect3DDevice9->GetSamplerState(Sampler, Type, pValue);
	}

	HRESULT d3d9ex::D3D9Device::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
	{
		count_call(call_type::SamplerState);
		return m_pIDirect3DDevice9->SetSamplerState(Sampler, Type, Value);
	}

	HRESULT d3d9ex::D3D9Device::ValidateDevice(DWORD* pNumPasses)
	{
		return m_pIDirect3DDevice9->ValidateDevice(pNumPasses);
	}

	HRESULT d3d9ex::D3D9Device::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries)
	{
		return m_pIDirect3DDevice9->SetPaletteEntries(PaletteNumber, pEntries);
	}

	HRESULT d3d9ex::D3D9Device::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries)
	{
		return m_pIDirect3DDevice9->GetPaletteEntries(PaletteNumber, pEntries);
	}

	HRESULT d3d9ex::D3D9Device::SetCurrentTexturePalette(UINT PaletteNumber)
	{
		return m_pIDirect3DDevice9->SetCurrentTexturePalette(PaletteNumber);
	}

	HRESULT d3d9ex::D3D9Device::GetCurrentTexturePalette(UINT *PaletteNumber)
	{
		return m_pIDirect3DDevice9->GetCurrentTexturePalette(PaletteNumber);
	}

	HRESULT d3d9ex::D3D9Device::SetScissorRect(CONST RECT* pRect)
	{
		return m_pIDirect3DDevice9->SetScissorRect(pRect);
	}

	HRESULT d3d9ex::D3D9Device::GetScissorRect(RECT* pRect)
	{
		return m_pIDirect3DDevice9->GetScissorRect(pRect);
	}

	HRESULT d3d9ex::D3D9Device::SetSoftwareVertexProcessing(BOOL bSoftware)
	{
		return m_pIDirect3DDevice9->SetSoftwareVertexProcessing(bSoftware);
	}

	BOOL d3d9ex::D3D9Device::GetSoftwareVertexProcessing()
	{
		return m_pIDirect3DDevice9->GetSoftwareVertexProcessing();
	}

	HRESULT d3d9ex::D3D9Device::SetNPatchMode(float nSegments)
	{
		return m_pIDirect3DDevice9->SetNPatchMode(nSegments);
	}

	float d3d9ex::D3D9Device::GetNPatchMode()
	{
		return m_pIDirect3DDevice9->GetNPatchMode();
	}

	HRESULT d3d9ex::D3D9Device::DrawPrimitive([[maybe_unused]] D3DPRIMITIVETYPE PrimitiveType, [[maybe_unused]] UINT StartVertex, [[maybe_unused]] UINT PrimitiveCount)
	{
		count_call(call_type::Draw);
		//return D3D_OK;
		//auto hr = m_pIDirect3DDevice9->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);

		const auto hr = renderer::get()->on_draw_primitive(m_pIDirect3DDevice9, PrimitiveType, StartVertex, PrimitiveCount);
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
	{
		count_call(call_type::Draw);
		const auto hr = renderer::get()->on_draw_indexed_prim(m_pIDirect3DDevice9, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
	{
		PROFILE_ZONE();
		count_call(call_type::Draw);
		//return D3D_OK;
		//pre_drawindexedprim();
		const auto hr = m_pIDirect3DDevice9->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
		//post_drawindexedprim();
		return hr;
	}

	HRESULT d3d9ex::D3D9Device::DrawIndexedPrimitiveUP(
		[[maybe_unused]] D3DPRIMITIVETYPE PrimitiveType, 
		[[maybe_unused]] UINT MinVertexIndex, 
		[[maybe_unused]] UINT NumVertices, 
		[[maybe_unused]] UINT PrimitiveCount, 
		[[maybe_unused]] CONST void* pIndexData, 
		[[maybe_unused]] D3DFORMAT IndexDataFormat,
		[[maybe_unused]] CONST void* pVertexStreamZeroData,
		[[maybe_unused]] UINT VertexStreamZeroStride)
	{
		PROFILE_ZONE();
		count_call(call_type::Draw);
		//return D3D_OK;
		return m_pIDirect3DDevice9->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	}

	HRESULT d3d9ex::D3D9Device::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags)
	{
		return m_pIDirect3DDevice9->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
	}

	HRESULT d3d9ex::D3D9Device::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->CreateVertexDeclaration(pVertexElements, ppDecl);
	}

	HRESULT d3d9ex::D3D9Device::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
	{
		count_call(call_type::VertexDeclaration);
		return m_pIDirect3DDevice9->SetVertexDeclaration(pDecl);
	}

	HRESULT d3d9ex::D3D9Device::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
	{
		return m_pIDirect3DDevice9->GetVertexDeclaration(ppDecl);
	}

	HRESULT d3d9ex::D3D9Device::SetFVF(DWORD FVF)
	{
		count_call(call_type::VertexDeclaration);
		return m_pIDirect3DDevice9->SetFVF(FVF);
	}

	HRESULT d3d9ex::D3D9Device::GetFVF(DWORD* pFVF)
	{
		return m_pIDirect3DDevice9->GetFVF(pFVF);
	}

	HRESULT d3d9ex::D3D9Device::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->CreateVertexShader(pFunction, ppShader);
	}

	HRESULT d3d9ex::D3D9Device::SetVertexShader(IDirect3DVertexShader9* pShader)
	{
		count_call(call_type::Shader);
		return m_pIDirect3DDevice9->SetVertexShader(pShader);
	}

	HRESULT d3d9ex::D3D9Device::GetVertexShader(IDirect3DVertexShader9** ppShader)
	{
		return m_pIDirect3DDevice9->GetVertexShader(ppShader);
	}

	HRESULT d3d9ex::D3D9Device::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}

	HRESULT d3d9ex::D3D9Device::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
	{
		return m_pIDirect3DDevice9->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}

	HRESULT d3d9ex::D3D9Device::SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}

	HRESULT d3d9ex::D3D9Device::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
	{
		return m_pIDirect3DDevice9->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}

	HRESULT d3d9ex::D3D9Device::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	}

	HRESULT d3d9ex::D3D9Device::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
	{
		return m_pIDirect3DDevice9->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	}

	HRESULT d3d9ex::D3D9Device::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
	{
		count_call(call_type::StreamSource);
		return m_pIDirect3DDevice9->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
	}

	HRESULT d3d9ex::D3D9Device::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride)
	{
		return m_pIDirect3DDevice9->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
	}

	HRESULT d3d9ex::D3D9Device::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
	{
		return m_pIDirect3DDevice9->SetStreamSourceFreq(StreamNumber, Divider);
	}

	HRESULT d3d9ex::D3D9Device::GetStreamSourceFreq(UINT StreamNumber, UINT* Divider)
	{
		return m_pIDirect3DDevice9->GetStreamSourceFreq(StreamNumber, Divider);
	}

	HRESULT d3d9ex::D3D9Device::SetIndices(IDirect3DIndexBuffer9* pIndexData)
	{
		count_call(call_type::Indices);
		return m_pIDirect3DDevice9->SetIndices(pIndexData);
	}

	HRESULT d3d9ex::D3D9Device::GetIndices(IDirect3DIndexBuffer9** ppIndexData)
	{
		return m_pIDirect3DDevice9->GetIndices(ppIndexData);
	}

	HRESULT d3d9ex::D3D9Device::CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader)
	{
		PROFILE_ZONE();
		return m_pIDirect3DDevice9->CreatePixelShader(pFunction, ppShader);
	}

	HRESULT d3d9ex::D3D9Device::SetPixelShader(IDirect3DPixelShader9* pShader)
	{
		count_call(call_type::Shader);
		return m_pIDirect3DDevice9->SetPixelShader(pShader);
	}

	HRESULT d3d9ex::D3D9Device::GetPixelShader(IDirect3DPixelShader9** ppShader)
	{
		return m_pIDirect3DDevice9->GetPixelShader(ppShader);
	}

	HRESULT d3d9ex::D3D9Device::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}

	HRESULT d3d9ex::D3D9Device::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
	{
		return m_pIDirect3DDevice9->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}

	HRESULT d3d9ex::D3D9Device::SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}

	HRESULT d3d9ex::D3D9Device::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
	{
		return m_pIDirect3DDevice9->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}

	HRESULT d3d9ex::D3D9Device::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount)
	{
		count_call(call_type::ShaderConstant);
		return m_pIDirect3DDevice9->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	}

	HRESULT d3d9ex::D3D9Device::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
	{
		return m_pIDirect3DDevice9->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	}

	HRESULT d3d9ex::D3D9Device::DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo)
	{
		return m_pIDirect3DDevice9->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
	}

	HRESULT d3d9ex::D3D9Device::DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo)
	{
		return m_pIDirect3DDevice9->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
	}

	HRESULT d3d9ex::D3D9Device::DeletePatch(UINT Handle)
	{
		return m_pIDirect3DDevice9->DeletePatch(Handle);
	}

	HRESULT d3d9ex::D3D9Device::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
	{
		PROFILE_ZONE();
		//shared::common::log("d3d9", "m_pIDirect3DDevice9->CreateQuery", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		return m_pIDirect3DDevice9->CreateQuery(Type, ppQuery);
	}

#pragma endregion

#pragma region _D3D9

	HRESULT __stdcall d3d9ex::_d3d9::QueryInterface(REFIID riid, void** ppvObj)
	{
		*ppvObj = nullptr;

		HRESULT hRes = m_pIDirect3D9->QueryInterface(riid, ppvObj);
		//shared::common::log("d3d9", "m_pIDirect3D9->QueryInterface", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

		if (hRes == NOERROR) {
			*ppvObj = this;
		}

		return hRes;
	}

	ULONG __stdcall d3d9ex::_d3d9::AddRef()
	{
		return m_pIDirect3D9->AddRef();
	}

	ULONG __stdcall d3d9ex::_d3d9::Release()
	{
		ULONG count = m_pIDirect3D9->Release();
		if (!count)
		{
			if (shared::globals::d3d9_interface == this) {
				shared::globals::d3d9_interface = nullptr;
			}
			delete this;
		}
		return count;
	}

	HRESULT __stdcall d3d9ex::_d3d9::RegisterSoftwareDevice(void* pInitializeFunction)
	{
		return m_pIDirect3D9->RegisterSoftwareDevice(pInitializeFunction);
	}

	UINT __stdcall d3d9ex::_d3d9::GetAdapterCount()
	{
		return m_pIDirect3D9->GetAdapterCount();
	}

	HRESULT __stdcall d3d9ex::_d3d9::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
	{
		return m_pIDirect3D9->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
	}

	UINT __stdcall d3d9ex::_d3d9::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
	{
		return m_pIDirect3D9->GetAdapterModeCount(Adapter, Format);
	}

	HRESULT __stdcall d3d9ex::_d3d9::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode)
	{
		return m_pIDirect3D9->EnumAdapterModes(Adapter, Format, Mode, pMode);
	}

	HRESULT __stdcall d3d9ex::_d3d9::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
	{
		return m_pIDirect3D9->GetAdapterDisplayMode(Adapter, pMode);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CheckDeviceType(UINT iAdapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
	{
		return m_pIDirect3D9->CheckDeviceType(iAdapter, DevType, DisplayFormat, BackBufferFormat, bWindowed);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
	{
		return m_pIDirect3D9->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels)
	{
		return m_pIDirect3D9->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
	{
		return m_pIDirect3D9->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
	{
		return m_pIDirect3D9->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
	{
		return m_pIDirect3D9->GetDeviceCaps(Adapter, DeviceType, pCaps);
	}

	HMONITOR __stdcall d3d9ex::_d3d9::GetAdapterMonitor(UINT Adapter)
	{
		return m_pIDirect3D9->GetAdapterMonitor(Adapter);
	}

	// Overlays (Social Club SDK 2.4+) create a throwaway device on their own window to hook the vtable.
	// Such a device is refused so it never reaches the bridge.
	bool d3d9ex::is_game_device(HWND hFocusWindow, const D3DPRESENT_PARAMETERS* pp)
	{
		const auto game_wnd = shared::globals::main_window;
		if (!game_wnd) {
			return true;
		}

		return hFocusWindow == game_wnd || (pp && pp->hDeviceWindow == game_wnd);
	}

	HRESULT __stdcall d3d9ex::_d3d9::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		if (!is_game_device(hFocusWindow, pPresentationParameters))
		{
			shared::common::log("d3d9", "m_pIDirect3D9->CreateDevice: not for the game window. Refusing the device.", shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
			if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = nullptr;
			return D3DERR_NOTAVAILABLE;
		}

		HRESULT hres = m_pIDirect3D9->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
		shared::common::log("d3d9", "m_pIDirect3D9->CreateDevice", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

		if (FAILED(hres)) {
			return hres;
		}

		*ppReturnedDeviceInterface = new d3d9ex::D3D9Device(*ppReturnedDeviceInterface);
		shared::globals::d3d_device = *ppReturnedDeviceInterface;

		return hres;
	}


#pragma endregion

#pragma region _D3D9Ex

	HRESULT __stdcall d3d9ex::_d3d9ex::QueryInterface(REFIID riid, void** ppvObj)
	{
		*ppvObj = nullptr;

		HRESULT hRes = m_pIDirect3D9Ex->QueryInterface(riid, ppvObj);
		//shared::common::log("d3d9", "m_pIDirect3D9Ex->QueryInterface", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

		if (hRes == NOERROR) {
			*ppvObj = this;
		}

		return hRes;
	}

	ULONG __stdcall d3d9ex::_d3d9ex::AddRef()
	{
		return m_pIDirect3D9Ex->AddRef();
	}

	ULONG __stdcall d3d9ex::_d3d9ex::Release()
	{
		ULONG count = m_pIDirect3D9Ex->Release();
		if (!count)
		{
			if (shared::globals::d3d9_interface == this) {
				shared::globals::d3d9_interface = nullptr;
			}
			delete this;
		}
		return count;
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::RegisterSoftwareDevice(void* pInitializeFunction)
	{
		return m_pIDirect3D9Ex->RegisterSoftwareDevice(pInitializeFunction);
	}

	UINT __stdcall d3d9ex::_d3d9ex::GetAdapterCount()
	{
		return m_pIDirect3D9Ex->GetAdapterCount();
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
	{
		return m_pIDirect3D9Ex->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
	}

	UINT __stdcall d3d9ex::_d3d9ex::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
	{
		return m_pIDirect3D9Ex->GetAdapterModeCount(Adapter, Format);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode)
	{
		return m_pIDirect3D9Ex->EnumAdapterModes(Adapter, Format, Mode, pMode);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
	{
		return m_pIDirect3D9Ex->GetAdapterDisplayMode(Adapter, pMode);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CheckDeviceType(UINT iAdapter, D3DDEVTYPE DevType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
	{
		return m_pIDirect3D9Ex->CheckDeviceType(iAdapter, DevType, DisplayFormat, BackBufferFormat, bWindowed);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
	{
		return m_pIDirect3D9Ex->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels)
	{
		return m_pIDirect3D9Ex->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
	{
		return m_pIDirect3D9Ex->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
	{
		return m_pIDirect3D9Ex->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
	{
		return m_pIDirect3D9Ex->GetDeviceCaps(Adapter, DeviceType, pCaps);
	}

	HMONITOR __stdcall d3d9ex::_d3d9ex::GetAdapterMonitor(UINT Adapter)
	{
		return m_pIDirect3D9Ex->GetAdapterMonitor(Adapter);
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		if (!is_game_device(hFocusWindow, pPresentationParameters))
		{
			shared::common::log("d3d9", "m_pIDirect3D9Ex->CreateDevice: not for the game window. Refusing the device.", shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
			if (ppReturnedDeviceInterface) *ppReturnedDeviceInterface = nullptr;
			return D3DERR_NOTAVAILABLE;
		}

		HRESULT hres = m_pIDirect3D9Ex->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
		shared::common::log("d3d9", "m_pIDirect3D9Ex->CreateDevice", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

		if (FAILED(hres)) {
			return hres;
		}

		*ppReturnedDeviceInterface = new d3d9ex::D3D9Device(*ppReturnedDeviceInterface);
		shared::globals::d3d_device = *ppReturnedDeviceInterface;

		return hres;
	}

	UINT __stdcall d3d9ex::_d3d9ex::GetAdapterModeCountEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter)
	{
		return (m_pIDirect3D9Ex->GetAdapterModeCountEx(Adapter, pFilter));
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::EnumAdapterModesEx(UINT Adapter, const D3DDISPLAYMODEFILTER* pFilter, UINT Mode, D3DDISPLAYMODEEX* pMode)
	{
		return (m_pIDirect3D9Ex->EnumAdapterModesEx(Adapter, pFilter, Mode, pMode));
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::GetAdapterDisplayModeEx(UINT Adapter, D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation)
	{
		return (m_pIDirect3D9Ex->GetAdapterDisplayModeEx(Adapter, pMode, pRotation));
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::CreateDeviceEx(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode, IDirect3DDevice9Ex** ppReturnedDeviceInterface)
	{
		return (m_pIDirect3D9Ex->CreateDeviceEx(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface));
	}

	HRESULT __stdcall d3d9ex::_d3d9ex::GetAdapterLUID(UINT Adapter, LUID* pLUID)
	{
		return (m_pIDirect3D9Ex->GetAdapterLUID(Adapter, pLUID));
	}
#pragma endregion

	IDirect3D9* __stdcall d3d9ex::direct3d_create9_stub(UINT sdk)
	{
		/*std::cout << "[D3D9] Game is invoking 'Direct3DCreate9'. Creating proxy interface.\n";
		{
			IDirect3D9Ex* d3d9ex = nullptr;
			if (SUCCEEDED(Direct3DCreate9Ex(sdk, &d3d9ex))) {
				return (new d3d9ex::_d3d9ex(d3d9ex));
			}
		}*/

		shared::common::log("d3d9", "Game is invoking 'Direct3DCreate9'. Creating proxy interface.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		shared::globals::d3d9_interface = new d3d9ex::_d3d9(Direct3DCreate9(sdk));
		return shared::globals::d3d9_interface;
	}

	typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT SDKVersion);
	Direct3DCreate9_t Direct3DCreate9_original = nullptr;

	IDirect3D9* WINAPI d3d9ex::HookedDirect3DCreate9(UINT SDKVersion)
	{
		shared::common::log("d3d9", "Game is invoking 'Direct3DCreate9'. Creating proxy interface.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		shared::globals::d3d9_interface = new d3d9ex::_d3d9(Direct3DCreate9_original(SDKVersion));
		return shared::globals::d3d9_interface;
	}

	d3d9ex::d3d9ex()
	{
		const auto addr = (DWORD)GetProcAddress(GetModuleHandle(L"d3d9.dll"), "Direct3DCreate9");
;
		const auto status = MH_CreateHook((LPVOID)addr, &d3d9ex::HookedDirect3DCreate9, (LPVOID*)&Direct3DCreate9_original);
		MH_EnableHook(MH_ALL_HOOKS);

		if (status == MH_OK) {
			shared::common::log("d3d9", "Hooked 'Direct3DCreate9' import.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}
		

		// hook interface creation
		//const auto d3d9_import_addr = shared::utils::mem::find_import_addr(shared::globals::exe_hmodule, "d3d8.dll", "Direct3DCreate9");
		//if (d3d9_import_addr)
		//if (xy)
		//{
		//	//shared::utils::hook::set(d3d9_import_addr, direct3d_create9_stub);
		//	shared::utils::hook::set(xy, direct3d_create9_stub);
		//	std::cout << "[D3D9] Successfully hooked 'Direct3DCreate9'\n";
		//}
		//else
		//{
		//	std::cout << "[D3D9] Could not find 'Direct3DCreate9' import -> ImGui will not work.\n";
		//}

		shared::common::log("d3d9", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
