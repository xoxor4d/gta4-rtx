#include "std_include.hpp"
#include "imgui.hpp"
#include "free_camera.hpp"
#include "imgui_light_editor.hpp"
#include "imgui_marker_editor.hpp"
#include "imgui_menu.hpp"
#include "imgui_mesh_inspector.hpp"

#include "scene_view.hpp"
#include "shared/common/flags.hpp"
#include "shared/imgui/editor_host.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/world_draw.hpp"
#include "shared/imgui/font_awesome_solid_900.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/font_opensans.hpp"

// Allow us to directly call the ImGui WndProc function.
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace gta4
{
	WNDPROC g_game_wndproc = nullptr;
	
	LRESULT __stdcall wnd_proc_hk(HWND window, UINT message_type, WPARAM wparam, LPARAM lparam)
	{
		if (message_type != WM_MOUSEMOVE && message_type != WM_NCMOUSEMOVE)
		{
			if (imgui::get()->input_message(message_type, wparam, lparam)) {
			//	return true;
			}
		}

		if (message_type == WM_KILLFOCUS)
		{
			uint32_t counter = 0u;
			while (::ShowCursor(TRUE) < 0 && ++counter < 3) {}
			ClipCursor(NULL);
		}

		//printf("MSG 0x%x -- w: 0x%x -- l: 0x%x\n", message_type, wparam, lparam);
		return CallWindowProc(g_game_wndproc, window, message_type, wparam, lparam);
	}

	bool imgui::input_message(const UINT message_type, const WPARAM wparam, const LPARAM lparam)
	{
		if (message_type == WM_KEYUP && wparam == VK_F4) 
		{
			const auto& io = ImGui::GetIO();
			if (!io.MouseDown[1]) {
				shared::globals::imgui_menu_open = !shared::globals::imgui_menu_open;
			} else {
				ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);
			}
		}

		if (shared::globals::imgui_menu_open || shared::globals::imgui_editor_open || shared::globals::free_camera_active || imgui::get()->m_freecam_mode)
		{
			//auto& io = ImGui::GetIO();
			ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);
		} else {
			shared::globals::imgui_allow_input_bypass = false; // always reset if there is no imgui window open
		}

		return shared::globals::imgui_menu_open || shared::globals::imgui_editor_open || shared::globals::free_camera_active;
	}

	// draw imgui widget
	void imgui::ImGuiStats::draw_stats()
	{
		if (!m_tracking_enabled) {
			return;
		}

		for (const auto& p : m_stat_list) 
		{
			if (p.second) {
				display_single_stat(p.first, *p.second);
			}
			else {
				ImGui::Spacing(0, 4);
			}
		}
	}

	void imgui::ImGuiStats::display_single_stat(const char* name, const StatObj& stat)
	{
		switch (stat.get_mode())
		{
		case StatObj::Mode::Single:
			ImGui::Text("%s", name);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.65f);
			ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
			ImGui::Text("%d total", stat.get_total());
			ImGui::PopFont();
			break;

		case StatObj::Mode::ConditionalCheck:
			ImGui::Text("%s", name);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.65f);
			ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
			ImGui::Text("%d total, %d successful", stat.get_total(), stat.get_successful());
			ImGui::PopFont();
			break;

		default:
			throw std::runtime_error("Uncovered Mode in StatObj");
		}
	}

	ImU32 get_color_for_hash(uint32_t hash)
	{
		hash ^= hash >> 17;
		hash *= 0xED5AD4BB;
		hash ^= hash >> 11;
		hash *= 0xAC4C1B51;
		hash ^= hash >> 15;
		hash *= 0x31848BAB;
		hash ^= hash >> 14;

		const int r = (hash & 0xFF);
		const int g = ((hash >> 8) & 0xFF);
		const int b = ((hash >> 16) & 0xFF);
		const int a = 255;

		return IM_COL32(r, g, b, a);
	};

	void imgui::draw_debug()
	{
		const auto im = imgui::get();

		shared::imgui::world::view_s scene;
		const bool has_scene = scene_view::get(scene);

		if (m_dbg_visualize_decal_renderstates)
		{
			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				const float draw_dist = im->m_dbg_visualize_distance;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				for (auto& l : visualized_decal_renderstates)
				{
					if (fabs(cam_org.DistToSqr(l.pos) < draw_dist * draw_dist))
					{
						if (has_scene && shared::imgui::world::project(scene, l.pos, viewport_pos))
						{
							std::ostringstream oss;
							oss << "[ALPHABLEND] " << (l.rs_alpha_blending ? "true" : "false") << "\n"
								<< "[BLEND_OP] " << l.rs_blendop << "\n"
								<< "[SRC_BLEND] " << l.rs_srcblend << "\n"
								<< "[DEST_BLEND] " << l.rs_destblend << "\n"
								<< "[ALPHA_OP] " << l.tss_alphaop << "\n"
								<< "[ALPHA_ARG1] " << l.tss_alphaarg1 << "\n"
								<< "[ALPHA_ARG2] " << l.tss_alphaarg2;

							ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
								ImGui::GetColorU32(ImGuiCol_Text),
								oss.str().c_str());
						}
					}
				}

				visualized_decal_renderstates.clear();
			}
		}

		if (m_dbg_visualize_stencil_state)
		{
			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				const float draw_dist = im->m_dbg_visualize_distance;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				for (auto& l : visualized_stencil_states)
				{
					if (fabs(cam_org.DistToSqr(l.pos) < draw_dist * draw_dist))
					{
						if (has_scene && shared::imgui::world::project(scene, l.pos, viewport_pos))
						{
							std::ostringstream oss;
							oss << "[ENABLED] " << (l.stencil_enable ? "true" : "false") << "\n"
								<< "[REF] " << l.stencil_ref << "\n"
								<< "[MASK] " << l.stencil_mask << "\n"
								<< "[SHADER] " << l.shader_name;

							ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
								ImGui::GetColorU32(ImGuiCol_Text),
								oss.str().c_str());
						}
					}
				}

				visualized_stencil_states.clear();
			}
		}

		if (static auto debug_model_hash = shared::common::flags::has_flag("debug_model_hash"); debug_model_hash)
		{
			if (m_dbg_visualize_model_hashes_info)
			{
				const auto vp = game::pViewports;
				if (vp->sceneviewport)
				{
					ImVec2 viewport_pos = {};
					const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

					for (auto& m : visualized_model_hashes)
					{
						if (fabs(cam_org.DistToSqr(m.pos) < im->m_dbg_visualize_model_hashes_distance * im->m_dbg_visualize_model_hashes_distance))
						{
							if (has_scene && shared::imgui::world::project(scene, m.pos, viewport_pos))
							{
								const ImU32 color = get_color_for_hash(m.hash);
								bool vis_hash = true;

								if (m.model_reference)
								{
									if (const game::CBaseModelInfo* basemodel = game::g_modelPointers[m.model_reference]; basemodel)
									{
										const auto& min = im->m_dbg_visualize_model_hashes_min_radius;
										const auto& max = im->m_dbg_visualize_model_hashes_max_radius;

										const auto has_min = min > 0.0f;
										const auto has_max = max > 0.0f;
										const auto fits_min = has_min && basemodel->m_fRadius > min;
										const auto fits_max = has_max && basemodel->m_fRadius < max;

										if (has_min && !fits_min || has_max && !fits_max) {
											vis_hash = false;
										}
										else {
											shared::imgui::world::draw_sphere_outline(scene, m.pos, basemodel->m_fRadius, color);
										}
									}
								}

								if (vis_hash)
								{
									std::ostringstream oss;
									oss << "HASH: " << std::format("0x{:08X}", m.hash);

									const auto it = game::g_modelHashToName.find(m.hash);
									if (it != game::g_modelHashToName.end()) {
										oss << "\nNAME: " << it->second;
									}

									ImGui::GetBackgroundDrawList()->AddText(viewport_pos, color, oss.str().c_str());
								}
							}
						}
					}

					visualized_model_hashes.clear();
				}
			}
		}
	}

	void imgui::devgui()
	{
		constexpr float FOOTER_HEIGHT = 20.0f;
		constexpr float BLUR_TINT_ALPHA_SCALE = 0.6f;

		ImGui::SetNextWindowSize(ImVec2(996, 720), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(720.0f, 520.0f), ImVec2(FLT_MAX, FLT_MAX));

		const auto& cs = comp_settings::get();
		const float* window_color = cs->menu_color_window.get_as<float*>();

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(window_color[0], window_color[1], window_color[2], window_color[3]));
		const bool visible = ImGui::Begin("Remix Compatibility-Mod Settings", &shared::globals::imgui_menu_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse/*, &shared::imgui::draw_window_blur_callback*/);
		ImGui::PopStyleColor();

		if (!visible)
		{
			ImGui::End();
			return;
		}

		if (cs->menu_background_blur._bool())
		{
			shared::imgui::draw_window_blur();

			// the blurred scene replaces the window background, the window colour tints it with less alpha to let the blur show
			const ImVec2 window_pos = ImGui::GetWindowPos();
			const ImVec2 content_min(window_pos.x, window_pos.y + ImGui::GetFrameHeight());
			const ImVec4 tint(window_color[0], window_color[1], window_color[2], window_color[3] * BLUR_TINT_ALPHA_SCALE);
			ImGui::GetWindowDrawList()->AddRectFilled(content_min, window_pos + ImGui::GetWindowSize(), ImGui::ColorConvertFloat4ToU32(tint));
		}

		m_im_window_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
		m_im_window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);

		static bool im_demo_menu = false;
		if (im_demo_menu) {
			ImGui::ShowDemoWindow(&im_demo_menu);
		}

		// resets after one frame
		m_dbg_debug_single_frame_timecycle_remix_vars = false;
		m_dbg_debug_single_frame_emissive_intensity_vars = false;

		menu::draw(FOOTER_HEIGHT);

		{
			ImGui::Separator();
			const char* movement_hint_str = "Hold Right Mouse to enable Game Input ";
			const auto avail_width = ImGui::GetContentRegionAvail().x;
			float cur_pos = avail_width - 54.0f;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y * 0.5f);
				const auto spos = ImGui::GetCursorScreenPos();
				ImGui::TextUnformatted(m_devgui_custom_footer_content.c_str());
				ImGui::SetCursorScreenPos(spos);
				m_devgui_custom_footer_content.clear();
			}

			ImGui::SetCursorPos(ImVec2(cur_pos, ImGui::GetCursorPosY() /*+ 2.0f*/));
			if (ImGui::TextLink("[Demo]")) {
				im_demo_menu = !im_demo_menu;
			}

			ImGui::SameLine();
			cur_pos = cur_pos - ImGui::CalcTextSize(movement_hint_str).x - 6.0f;
			ImGui::SetCursorPosX(cur_pos);
			ImGui::TextUnformatted(movement_hint_str);
		}
		ImGui::PopStyleVar(1);
		ImGui::End();
	}

	namespace
	{
		// Whether Windows has HDR on for the display showing `window`, asked the way Remix asks it when it
		// creates the back buffers. Advanced color with wide color enforced is SDR auto color management.
		bool windows_hdr_enabled(HWND window)
		{
			if (!window) {
				return false;
			}

			MONITORINFOEXW monitor_info = {};
			monitor_info.cbSize = sizeof(monitor_info);
			if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor_info)) {
				return false;
			}

			UINT32 path_count = 0u;
			UINT32 mode_count = 0u;
			if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS) {
				return false;
			}

			std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
			std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
			if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr) != ERROR_SUCCESS) {
				return false;
			}

			for (UINT32 i = 0u; i < path_count; i++)
			{
				DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
				source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
				source_name.header.size = sizeof(source_name);
				source_name.header.adapterId = paths[i].sourceInfo.adapterId;
				source_name.header.id = paths[i].sourceInfo.id;
				if (DisplayConfigGetDeviceInfo(&source_name.header) != ERROR_SUCCESS
					|| wcscmp(source_name.viewGdiDeviceName, monitor_info.szDevice) != 0) {
					continue;
				}

				DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color_info = {};
				color_info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
				color_info.header.size = sizeof(color_info);
				color_info.header.adapterId = paths[i].targetInfo.adapterId;
				color_info.header.id = paths[i].targetInfo.id;
				if (DisplayConfigGetDeviceInfo(&color_info.header) != ERROR_SUCCESS) {
					return false;
				}

				return color_info.advancedColorEnabled && !color_info.wideColorEnforced;
			}

			return false;
		}

		// Remix creates a float back buffer while Windows HDR is on. D3DRS_SRGBWRITEENABLE has no effect on
		// it, so the ImGui pass is rendered into an 8-bit target with its usual sRGB states and composited
		// onto the back buffer as premultiplied color, which keeps the SDR look. The bridge reports the
		// format the game asked for, so the decision repeats Remix's: Windows HDR at back buffer creation.
		// On an SDR back buffer the detour only costs the extra target.
		struct hdr_ui_target
		{
			IDirect3DTexture9* texture = nullptr;
			IDirect3DSurface9* surface = nullptr;
			IDirect3DSurface9* backup = nullptr;
			D3DVIEWPORT9 game_viewport = {};
			UINT width = 0u;
			UINT height = 0u;
			bool evaluated = false;
			bool wanted = false;
			bool active = false;

			void release()
			{
				if (surface)
				{
					surface->Release();
					surface = nullptr;
				}

				if (texture)
				{
					texture->Release();
					texture = nullptr;
				}

				width = 0u;
				height = 0u;
				evaluated = false;
			}

			void begin(IDirect3DDevice9* dev)
			{
				active = false;

				IDirect3DSurface9* back_buffer = nullptr;
				if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) || !back_buffer) {
					return;
				}

				D3DSURFACE_DESC desc;
				back_buffer->GetDesc(&desc);
				back_buffer->Release();

				if (texture && (width != desc.Width || height != desc.Height)) {
					release();
				}

				if (!evaluated)
				{
					evaluated = true;
					wanted = windows_hdr_enabled(shared::globals::main_window);
				}

				if (!wanted) {
					return;
				}

				if (!texture)
				{
					if (FAILED(dev->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr))
						|| FAILED(texture->GetSurfaceLevel(0, &surface)))
					{
						release();
						return;
					}

					width = desc.Width;
					height = desc.Height;
				}

				if (FAILED(dev->GetRenderTarget(0, &backup))) {
					return;
				}

				dev->GetViewport(&game_viewport);

				// SetRenderTarget resets the viewport to the whole target. Clear still honours the scissor rect.
				DWORD scissor = FALSE;
				dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor);
				dev->SetRenderTarget(0, surface);
				dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
				dev->SetRenderState(D3DRS_SCISSORTESTENABLE, scissor);
				active = true;
			}

			void end(IDirect3DDevice9* dev)
			{
				if (!active) {
					return;
				}

				active = false;
				dev->SetRenderTarget(0, backup);
				backup->Release();
				backup = nullptr;

				IDirect3DStateBlock9* state_block = nullptr;
				if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state_block)))
				{
					dev->SetViewport(&game_viewport);
					return;
				}
				state_block->Capture();

				struct vertex { float x, y, z, rhw, u, v; };
				const float w = static_cast<float>(width);
				const float h = static_cast<float>(height);
				const vertex quad[4] =
				{
					{ -0.5f,    -0.5f,    0.0f, 1.0f, 0.0f, 0.0f },
					{ w - 0.5f, -0.5f,    0.0f, 1.0f, 1.0f, 0.0f },
					{ -0.5f,    h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
					{ w - 0.5f, h - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
				};

				const D3DVIEWPORT9 viewport = { 0, 0, width, height, 0.0f, 1.0f };
				dev->SetViewport(&viewport);
				dev->SetPixelShader(nullptr);
				dev->SetVertexShader(nullptr);
				dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
				dev->SetTexture(0, texture);
				dev->SetRenderState(D3DRS_ZENABLE, FALSE);
				dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
				dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
				dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
				dev->SetRenderState(D3DRS_LIGHTING, FALSE);
				dev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
				dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
				dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
				dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
				dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
				dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
				dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
				dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
				dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
				dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
				dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
				dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
				dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(vertex));

				state_block->Apply();
				state_block->Release();
				dev->SetViewport(&game_viewport);
			}
		};

		hdr_ui_target g_hdr_ui_target;
	}

	// The back buffers are recreated on reset, and with them Remix's HDR decision.
	void imgui::on_device_reset()
	{
		g_hdr_ui_target.release();
	}

	void imgui::on_present()
	{
		GTA4_PERF_SCOPE(performance_section::ImGui);
		if (auto* im = imgui::get(); im)
		{
			if (const auto dev = shared::globals::d3d_device; dev)
			{
				if (!im->m_initialized_device)
				{
					//Sleep(1000);
					shared::common::log("ImGui", "ImGui_ImplDX9_Init");
					ImGui_ImplDX9_Init(dev);
					im->m_initialized_device = true;
				}

				// else so we render the first frame one frame later
				else if (im->m_initialized_device)
				{
					// fix imgui colors / background if no hud elem is visible
					DWORD og_srgb_samp, og_srgb_write;
					dev->GetSamplerState(0, D3DSAMP_SRGBTEXTURE, &og_srgb_samp);
					dev->GetRenderState(D3DRS_SRGBWRITEENABLE, &og_srgb_write);
					dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 1);
					dev->SetRenderState(D3DRS_SRGBWRITEENABLE, 1);

					ImGui_ImplDX9_NewFrame();
					ImGui_ImplWin32_NewFrame();
					ImGui::NewFrame();
					menu::apply_colors();

					auto& io = ImGui::GetIO();

					if (shared::globals::imgui_allow_input_bypass_timeout) {
						shared::globals::imgui_allow_input_bypass_timeout--;
					}

					if (shared::globals::imgui_menu_open || shared::imgui::editor_host::any_open())
					{
						io.MouseDrawCursor = true;

						if (shared::globals::imgui_menu_open) {
							im->devgui();
						}

						if (shared::imgui::editor_host::any_open())
						{
							shared::imgui::editor_host::draw();
							im->m_im_window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
						}

						// ---
						// enable game input via right mouse button logic (an editor only hands it over if asked to)

						const bool rmb_input_allowed = shared::imgui::editor_host::any_open()
							? shared::imgui::editor_host::allows_game_input_on_rmb()
							: shared::globals::imgui_menu_open && !free_camera::is_active();
						if (rmb_input_allowed && !im->m_im_window_hovered && io.MouseDown[1])
						{
							// reset stuck rmb if timeout is active 
							if (shared::globals::imgui_allow_input_bypass_timeout)
							{
								io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
								shared::globals::imgui_allow_input_bypass_timeout = 0u;
							}

							// enable game input if no imgui window is hovered and right mouse is held
							else
							{
								ImGui::SetWindowFocus(); // unfocus input text
								shared::globals::imgui_allow_input_bypass = true;
							}
						}

						// ^ wait until mouse is up
						else if (shared::globals::imgui_allow_input_bypass && !io.MouseDown[1] && !shared::globals::imgui_allow_input_bypass_timeout)
						{
							shared::globals::imgui_allow_input_bypass_timeout = 2u;
							shared::globals::imgui_allow_input_bypass = false;
						}
					}
					else 
					{
						io.MouseDrawCursor = false;
						shared::globals::imgui_allow_input_bypass_timeout = 0u;
						shared::globals::imgui_allow_input_bypass = false;
					}

					if (free_camera::is_active()) {
						free_camera::update_input();
					}

					if (const auto p = performance_logger::get(); p) {
						p->draw_imgui_popout_window();
					}

					im->draw_debug();

					if (im->m_stats.is_tracking_enabled()) {
						im->m_stats.reset_stats();
					}

					shared::globals::imgui_is_rendering = true;
					ImGui::EndFrame();
					ImGui::Render();
					g_hdr_ui_target.begin(dev);
					ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
					g_hdr_ui_target.end(dev);
					shared::globals::imgui_is_rendering = false;

					// restore
					dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, og_srgb_samp);
					dev->SetRenderState(D3DRS_SRGBWRITEENABLE, og_srgb_write);
				}
			}
		}
	}

	void imgui::style_xo()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.5f;

		style.WindowPadding = ImVec2(8.0f, 10.0f);
		style.FramePadding = ImVec2(7.0f, 6.0f);
		style.ItemSpacing = ImVec2(3.0f, 3.0f);
		style.ItemInnerSpacing = ImVec2(3.0f, 8.0f);
		style.IndentSpacing = 16.0f;
		style.ColumnsMinSpacing = 10.0f;
		style.ScrollbarSize = 10.0f;
		style.GrabMinSize = 10.0f;

		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;

		style.WindowRounding = 0.0f;
		style.ChildRounding = 2.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 2.0f;
		style.GrabRounding = 1.0f;
		style.TabRounding = 2.0f;
		
		style.CellPadding = ImVec2(5.0f, 4.0f);

		auto& colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.97f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.21f, 0.21f, 0.21f, 0.80f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.15f, 0.15f, 0.15f, 0.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.23f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.39f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.54f, 0.54f, 0.54f, 0.47f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.78f, 0.78f, 0.78f, 0.33f);
		colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.31f);
		colors[ImGuiCol_Button] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.77f, 0.33f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.110f, 0.110f, 0.110f, 0.569f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 0.75f, 0.26f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.43f, 0.43f, 0.43f, 0.51f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.76f, 0.30f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.37f);
		colors[ImGuiCol_TabSelected] = ImVec4(0.94f, 0.63f, 0.01f, 1.00f);
		colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_TabDimmed] = ImVec4(0.00f, 0.00f, 0.00f, 0.16f);
		colors[ImGuiCol_TabDimmedSelected] = ImVec4(1.00f, 1.00f, 1.00f, 0.24f);
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
		colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.00f, 0.51f, 0.39f, 0.31f);
		colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.56f);

		// custom colors
		ImGuiCol_ButtonGreen = ImVec4(0.3f, 0.4f, 0.05f, 0.7f);
		ImGuiCol_ButtonYellow = ImVec4(0.4f, 0.3f, 0.1f, 0.8f);
		ImGuiCol_ButtonRed = ImVec4(0.48f, 0.15f, 0.15f, 1.00f);

		menu::apply_colors();
	}

	void init_fonts()
	{
		using namespace shared::imgui::font;

		auto merge_icons_with_latest_font = [](const float& font_size, const bool font_data_owned_by_atlas = false)
			{
				static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0 };

				ImFontConfig icons_config;
				icons_config.MergeMode = true;
				icons_config.PixelSnapH = true;
				icons_config.FontDataOwnedByAtlas = font_data_owned_by_atlas;
				icons_config.GlyphOffset.y = 1.5f;

				ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)fa_solid_900, sizeof(fa_solid_900), font_size, &icons_config, icons_ranges);
			};

		ImGuiIO& io = ImGui::GetIO();

		io.Fonts->AddFontFromMemoryCompressedTTF(opensans_bold_compressed_data, opensans_bold_compressed_size, 18.0f);
		merge_icons_with_latest_font(12.0f, false);

		io.Fonts->AddFontFromMemoryCompressedTTF(opensans_bold_compressed_data, opensans_bold_compressed_size, 17.0f);
		merge_icons_with_latest_font(12.0f, false);

		io.Fonts->AddFontFromMemoryCompressedTTF(opensans_regular_compressed_data, opensans_regular_compressed_size, 18.0f);
		io.Fonts->AddFontFromMemoryCompressedTTF(opensans_regular_compressed_data, opensans_regular_compressed_size, 16.0f);

		ImFontConfig font_cfg;
		font_cfg.FontDataOwnedByAtlas = false;

		io.FontDefault = io.Fonts->AddFontFromMemoryCompressedTTF(opensans_regular_compressed_data, opensans_regular_compressed_size, 17.0f, &font_cfg);
		merge_icons_with_latest_font(17.0f, false);
	}

	imgui::imgui()
	{
		p_this = this;

		memset(&m_dbg_phone_projection_matrix_offset, 0, sizeof(D3DXMATRIX));
		memset(&m_debug_mtx02, 0, sizeof(D3DXMATRIX));
		memset(&m_debug_mtx03, 0, sizeof(D3DXMATRIX));

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		init_fonts();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.MouseDrawCursor = true;

		//io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
		//io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

		style_xo();
		light_editor::register_editor();
		marker_editor::register_editor();
		mesh_inspector::register_editor();

		ImGui_ImplWin32_Init(shared::globals::main_window);
		g_game_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(shared::globals::main_window, GWLP_WNDPROC, LONG_PTR(wnd_proc_hk)));

		// ---
		m_initialized = true;
		shared::common::log("ImGui", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}



