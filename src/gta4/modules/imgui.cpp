#include "std_include.hpp"
#include "imgui.hpp"

#include "game_settings.hpp"
#include "remix_lights.hpp"
#include "renderer.hpp"
#include "shared/common/flags.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/font_awesome_solid_900.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/font_opensans.hpp"

// Allow us to directly call the ImGui WndProc function.
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#define SPACING_INDENT_BEGIN ImGui::Spacing(); ImGui::Indent()
#define SPACING_INDENT_END ImGui::Spacing(); ImGui::Unindent()
#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

#define SET_CHILD_WIDGET_WIDTH			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
#define SET_CHILD_WIDGET_WIDTH_MAN(V)	ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild((V)));

namespace mods::gta4
{
	WNDPROC g_game_wndproc = nullptr;
	
	LRESULT __stdcall wnd_proc_hk(HWND window, UINT message_type, WPARAM wparam, LPARAM lparam)
	{
		if (message_type != WM_MOUSEMOVE || message_type != WM_NCMOUSEMOVE)
		{
			if (imgui::get()->input_message(message_type, wparam, lparam)) {
				return true;
			}
		}

		//game::console(); printf("MSG 0x%x -- w: 0x%x -- l: 0x%x\n", message_type, wparam, lparam);
		return CallWindowProc(g_game_wndproc, window, message_type, wparam, lparam);
	}

	bool imgui::input_message(const UINT message_type, const WPARAM wparam, const LPARAM lparam)
	{
		/*if (message_type == WM_KEYUP && wparam == VK_F6) {
			imgui::get()->m_dbg_use_fake_camera = !imgui::get()->m_dbg_use_fake_camera;
		}*/

		if (message_type == WM_KEYUP && wparam == VK_F4) 
		{
			const auto& io = ImGui::GetIO();
			if (!io.MouseDown[1]) {
				shared::globals::imgui_menu_open = !shared::globals::imgui_menu_open;
			} else {
				ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);
			}
		}

		if (shared::globals::imgui_menu_open)
		{
			auto& io = ImGui::GetIO();
			ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);

			// enable game input if no imgui window is hovered and right mouse is held
			if (!m_im_window_hovered && io.MouseDown[1])
			{
				ImGui::SetWindowFocus(); // unfocus input text
				shared::globals::imgui_allow_input_bypass = true;
				return false;
			}

			// ^ wait until mouse is up and call set_cursor_always_visible once
			if (shared::globals::imgui_allow_input_bypass && !io.MouseDown[1])
			{
				shared::globals::imgui_allow_input_bypass = false;
				return false;
			}
		}
		else {
			shared::globals::imgui_allow_input_bypass = false; // always reset if there is no imgui window open
		}

		return shared::globals::imgui_menu_open;
	}

	// ------

	void imgui::tab_dev()
	{
		const auto& im = imgui::get();

		{
			static float cont_cull_height = 0.0f;
			cont_cull_height = ImGui::Widget_ContainerWithCollapsingTitle("Shaders", cont_cull_height, [&]
				{
					ImGui::Indent(12.0f);

					ImGui::Checkbox("Enable Ignore Shader Logic", &im->m_dbg_enable_ignore_shader_logic);
					ImGui::Checkbox("Also Ignore on DrawPrimitive calls", &im->m_dbg_ignore_drawprimitive);

					ImGui::BeginDisabled(!im->m_dbg_enable_ignore_shader_logic);
					{
						ImGui::Checkbox("Ignore ALL", &im->m_dbg_ignore_all);
						ImGui::Checkbox("cascade.fxc", &im->m_dbg_ignore_cascade);
						ImGui::Checkbox("deferred_lighting.fxc", &im->m_dbg_ignore_deferred_lighting);
						ImGui::Checkbox("gpuptfx_simplerender.fxc", &im->m_dbg_ignore_gpuptfx_simplerender);
						ImGui::Checkbox("gta_atmoscatt_clouds.fxc", &im->m_dbg_ignore_gta_atmoscatt_clouds);
						ImGui::Checkbox("gta_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_cubemap_reflect);
						ImGui::Checkbox("gta_cutout_fence.fxc", &im->m_dbg_ignore_gta_cutout_fence);
						ImGui::Checkbox("gta_decal.fxc", &im->m_dbg_ignore_gta_decal);
						ImGui::Checkbox("gta_decal_amb_only.fxc", &im->m_dbg_ignore_gta_decal_amb_only);
						ImGui::Checkbox("gta_decal_dirt.fxc", &im->m_dbg_ignore_gta_decal_dirt);
						ImGui::Checkbox("gta_decal_glue.fxc", &im->m_dbg_ignore_gta_decal_glue);
						ImGui::Checkbox("gta_decal_normal_only.fxc", &im->m_dbg_ignore_gta_decal_normal_only);
						ImGui::Checkbox("gta_default.fxc", &im->m_dbg_ignore_gta_default);
						ImGui::Checkbox("gta_diffuse_instance.fxc", &im->m_dbg_ignore_gta_diffuse_instance);
						ImGui::Checkbox("gta_emissive.fxc", &im->m_dbg_ignore_gta_emissive);
						ImGui::Checkbox("gta_emissivenight.fxc", &im->m_dbg_ignore_gta_emissivenight);
						ImGui::Checkbox("gta_emissivestrong.fxc", &im->m_dbg_ignore_gta_emissivestrong);
						ImGui::Checkbox("gta_glass.fxc", &im->m_dbg_ignore_gta_glass);
						ImGui::Checkbox("gta_glass_emissive.fxc", &im->m_dbg_ignore_gta_glass_emissive);
						ImGui::Checkbox("gta_glass_emissivenight.fxc", &im->m_dbg_ignore_gta_glass_emissivenight);
						ImGui::Checkbox("gta_glass_normal_spec_reflect.fxc", &im->m_dbg_ignore_gta_glass_normal_spec_reflect);
						ImGui::Checkbox("gta_glass_reflect.fxc", &im->m_dbg_ignore_gta_glass_reflect);
						ImGui::Checkbox("gta_glass_spec.fxc", &im->m_dbg_ignore_gta_glass_spec);
						ImGui::Checkbox("gta_grass.fxc", &im->m_dbg_ignore_gta_grass);
						ImGui::Checkbox("gta_hair_sorted_alpha.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha);
						ImGui::Checkbox("gta_hair_sorted_alpha_exp.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha_exp);
						ImGui::Checkbox("gta_im.fxc", &im->m_dbg_ignore_gta_im);
						ImGui::Checkbox("gta_normal.fxc", &im->m_dbg_ignore_gta_normal);
						ImGui::Checkbox("gta_normal_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_normal_cubemap_reflect);
						ImGui::Checkbox("gta_normal_decal.fxc", &im->m_dbg_ignore_gta_normal_decal);
						ImGui::Checkbox("gta_normal_reflect.fxc", &im->m_dbg_ignore_gta_normal_reflect);
						ImGui::Checkbox("gta_normal_reflect_alpha.fxc", &im->m_dbg_ignore_gta_normal_reflect_alpha);
						ImGui::Checkbox("gta_normal_reflect_decal.fxc", &im->m_dbg_ignore_gta_normal_reflect_decal);
						ImGui::Checkbox("gta_normal_spec.fxc", &im->m_dbg_ignore_gta_normal_spec);
						ImGui::Checkbox("gta_normal_spec_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_normal_spec_cubemap_reflect);
						ImGui::Checkbox("gta_normal_spec_decal.fxc", &im->m_dbg_ignore_gta_normal_spec_decal);
						ImGui::Checkbox("gta_normal_spec_reflect.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect);
						ImGui::Checkbox("gta_normal_spec_reflect_decal.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_decal);
						ImGui::Checkbox("gta_normal_spec_reflect_emissive.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_emissive);
						ImGui::Checkbox("gta_normal_spec_reflect_emissivenight.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_emissivenight);
						ImGui::Checkbox("gta_parallax.fxc", &im->m_dbg_ignore_gta_parallax);
						ImGui::Checkbox("gta_parallax_specmap.fxc", &im->m_dbg_ignore_gta_parallax_specmap);
						ImGui::Checkbox("gta_parallax_steep.fxc", &im->m_dbg_ignore_gta_parallax_steep);
						ImGui::Checkbox("gta_ped.fxc", &im->m_dbg_ignore_gta_ped);
						ImGui::Checkbox("gta_ped_face.fxc", &im->m_dbg_ignore_gta_ped_face);
						ImGui::Checkbox("gta_ped_reflect.fxc", &im->m_dbg_ignore_gta_ped_reflect);
						ImGui::Checkbox("gta_ped_skin.fxc", &im->m_dbg_ignore_gta_ped_skin);
						ImGui::Checkbox("gta_ped_skin_blendshape.fxc", &im->m_dbg_ignore_gta_ped_skin_blendshape);
						ImGui::Checkbox("gta_projtex.fxc", &im->m_dbg_ignore_gta_projtex);
						ImGui::Checkbox("gta_projtex_steep.fxc", &im->m_dbg_ignore_gta_projtex_steep);
						ImGui::Checkbox("gta_radar.fxc", &im->m_dbg_ignore_gta_radar);
						ImGui::Checkbox("gta_reflect.fxc", &im->m_dbg_ignore_gta_reflect);
						ImGui::Checkbox("gta_reflect_decal.fxc", &im->m_dbg_ignore_gta_reflect_decal);
						ImGui::Checkbox("gta_rmptfx_gpurender.fxc", &im->m_dbg_ignore_gta_rmptfx_gpurender);
						ImGui::Checkbox("gta_rmptfx_litsprite.fxc", &im->m_dbg_ignore_gta_rmptfx_litsprite);
						ImGui::Checkbox("gta_rmptfx_mesh.fxc", &im->m_dbg_ignore_gta_rmptfx_mesh);
						ImGui::Checkbox("gta_rmptfx_raindrops.fxc", &im->m_dbg_ignore_gta_rmptfx_raindrops);
						ImGui::Checkbox("gta_spec.fxc", &im->m_dbg_ignore_gta_spec);
						ImGui::Checkbox("gta_spec_decal.fxc", &im->m_dbg_ignore_gta_spec_decal);
						ImGui::Checkbox("gta_spec_reflect.fxc", &im->m_dbg_ignore_gta_spec_reflect);
						ImGui::Checkbox("gta_spec_reflect_decal.fxc", &im->m_dbg_ignore_gta_spec_reflect_decal);
						ImGui::Checkbox("gta_terrain.fxc", &im->m_dbg_ignore_gta_terrain);
						ImGui::Checkbox("gta_trees.fxc", &im->m_dbg_ignore_gta_trees);
						ImGui::Checkbox("gta_vehicle_badges.fxc", &im->m_dbg_ignore_gta_vehicle_badges);
						ImGui::Checkbox("gta_vehicle_basic.fxc", &im->m_dbg_ignore_gta_vehicle_basic);
						ImGui::Checkbox("gta_vehicle_chrome.fxc", &im->m_dbg_ignore_gta_vehicle_chrome);
						ImGui::Checkbox("gta_vehicle_disc.fxc", &im->m_dbg_ignore_gta_vehicle_disc);
						ImGui::Checkbox("gta_vehicle_generic.fxc", &im->m_dbg_ignore_gta_vehicle_generic);
						ImGui::Checkbox("gta_vehicle_interior.fxc", &im->m_dbg_ignore_gta_vehicle_interior);
						ImGui::Checkbox("gta_vehicle_interior2.fxc", &im->m_dbg_ignore_gta_vehicle_interior2);
						ImGui::Checkbox("gta_vehicle_lightsemissive.fxc", &im->m_dbg_ignore_gta_vehicle_lightsemissive);
						ImGui::Checkbox("gta_vehicle_mesh.fxc", &im->m_dbg_ignore_gta_vehicle_mesh);
						ImGui::Checkbox("gta_vehicle_paint1.fxc", &im->m_dbg_ignore_gta_vehicle_paint1);
						ImGui::Checkbox("gta_vehicle_paint2.fxc", &im->m_dbg_ignore_gta_vehicle_paint2);
						ImGui::Checkbox("gta_vehicle_paint3.fxc", &im->m_dbg_ignore_gta_vehicle_paint3);
						ImGui::Checkbox("gta_vehicle_rims1.fxc", &im->m_dbg_ignore_gta_vehicle_rims1);
						ImGui::Checkbox("gta_vehicle_rims2.fxc", &im->m_dbg_ignore_gta_vehicle_rims2);
						ImGui::Checkbox("gta_vehicle_rims3.fxc", &im->m_dbg_ignore_gta_vehicle_rims3);
						ImGui::Checkbox("gta_vehicle_rubber.fxc", &im->m_dbg_ignore_gta_vehicle_rubber);
						ImGui::Checkbox("gta_vehicle_shuts.fxc", &im->m_dbg_ignore_gta_vehicle_shuts);
						ImGui::Checkbox("gta_vehicle_tire.fxc", &im->m_dbg_ignore_gta_vehicle_tire);
						ImGui::Checkbox("gta_vehicle_vehglass.fxc", &im->m_dbg_ignore_gta_vehicle_vehglass);
						ImGui::Checkbox("gta_wire.fxc", &im->m_dbg_ignore_gta_wire);
						ImGui::Checkbox("mirror.fxc", &im->m_dbg_ignore_mirror);
						ImGui::Checkbox("rage_atmoscatt_clouds.fxc", &im->m_dbg_ignore_rage_atmoscatt_clouds);
						ImGui::Checkbox("rage_billboard_nobump.fxc", &im->m_dbg_ignore_rage_billboard_nobump);
						ImGui::Checkbox("rage_bink.fxc", &im->m_dbg_ignore_rage_bink);
						ImGui::Checkbox("rage_default.fxc", &im->m_dbg_ignore_rage_default);
						ImGui::Checkbox("rage_fastmipmap.fxc", &im->m_dbg_ignore_rage_fastmipmap);
						ImGui::Checkbox("rage_im.fxc", &im->m_dbg_ignore_rage_im);
						ImGui::Checkbox("rage_perlinnoise.fxc", &im->m_dbg_ignore_rage_perlinnoise);
						ImGui::Checkbox("rage_postfx.fxc", &im->m_dbg_ignore_rage_postfx);
						ImGui::Checkbox("rmptfx_collision.fxc", &im->m_dbg_ignore_rmptfx_collision);
						ImGui::Checkbox("rmptfx_default.fxc", &im->m_dbg_ignore_rmptfx_default);
						ImGui::Checkbox("rmptfx_litsprite.fxc", &im->m_dbg_ignore_rmptfx_litsprite);
						ImGui::Checkbox("shadowSmartBlit.fxc", &im->m_dbg_ignore_shadowSmartBlit);
						ImGui::Checkbox("shadowZ.fxc", &im->m_dbg_ignore_shadowZ);
						ImGui::Checkbox("shadowZDir.fxc", &im->m_dbg_ignore_shadowZDir);
						ImGui::Checkbox("water.fxc", &im->m_dbg_ignore_water);
						ImGui::Checkbox("waterTex.fxc", &im->m_dbg_ignore_waterTex);

						ImGui::EndDisabled();
					}

					//SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::SliderFloat3("Camera Position (X, Y, Z)", im->m_dbg_camera_pos, -200.0f, 200.0f);

					ImGui::Unindent();

				}, false, ICON_FA_ELLIPSIS_H, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

#if DEBUG
		{
			static float cont_debug_height = 0.0f;
			cont_debug_height = ImGui::Widget_ContainerWithCollapsingTitle("DEBUG Build Section", cont_debug_height, [&]
				{
					ImGui::Checkbox("Do not render Static", &im->m_dbg_do_not_render_static);
					ImGui::Checkbox("Do not render Vehicle", &im->m_dbg_do_not_render_vehicle);
					ImGui::Checkbox("Do not render Instances", &im->m_dbg_do_not_render_instances);
					ImGui::Checkbox("Do not render Stencil 0", &im->m_dbg_do_not_render_stencil_zero);
					ImGui::Checkbox("Do not render Tree Foliage", &im->m_dbg_do_not_render_tree_foliage);
					ImGui::Checkbox("Do not render FF", &im->m_dbg_do_not_render_ff);
					ImGui::Checkbox("Toggle Shader/FF Rendering (On: Shader)", &im->m_dbg_toggle_ff);
					ImGui::Checkbox("Disable Pixelshader for Static objects rendered via FF", &im->m_dbg_disable_ps_for_static);
					ImGui::SliderInt("Tag EmissiveNight surfaces as Category", &im->m_dbg_tag_static_emissive_as_index, -1, 23);

					ImGui::Checkbox("Visualize Api Lights", &cmd::show_api_lights);
					TT("Visualize all spawned api lights");

					ImGui::DragFloat3("Debug Vector", &im->m_debug_vector.x, 0.01f);
					ImGui::DragFloat3("Debug Vector 2", &im->m_debug_vector2.x, 0.1f);

					ImGui::Spacing(0, 6);
					ImGui::SeparatorText("Phone Projection Offset Matrix");
					ImGui::DragFloat4("##Phone Proj Offset Row0", im->m_dbg_phone_projection_matrix_offset.m[0], 0.01f);
					ImGui::DragFloat4("##Phone Proj Offset Row1", im->m_dbg_phone_projection_matrix_offset.m[1], 0.01f);
					ImGui::DragFloat4("##Phone Proj Offset Row2", im->m_dbg_phone_projection_matrix_offset.m[2], 0.01f);
					ImGui::DragFloat4("##Phone Proj Offset Row3", im->m_dbg_phone_projection_matrix_offset.m[3], 0.01f);

					ImGui::Spacing(0, 6);
					ImGui::SeparatorText("Debug Offset Matrix 2");
					ImGui::DragFloat4("##Debug Mtx02 Row0", im->m_debug_mtx02.m[0], 0.01f);
					ImGui::DragFloat4("##Debug Mtx02 Row1", im->m_debug_mtx02.m[1], 0.01f);
					ImGui::DragFloat4("##Debug Mtx02 Row2", im->m_debug_mtx02.m[2], 0.01f);
					ImGui::DragFloat4("##Debug Mtx02 Row3", im->m_debug_mtx02.m[3], 0.01f);

					ImGui::Spacing(0, 6);
					ImGui::SeparatorText("Debug Offset Matrix 3");
					ImGui::DragFloat4("##Debug Mtx03 Row0", im->m_debug_mtx02.m[0], 0.01f);
					ImGui::DragFloat4("##Debug Mtx03 Row1", im->m_debug_mtx02.m[1], 0.01f);
					ImGui::DragFloat4("##Debug Mtx03 Row2", im->m_debug_mtx02.m[2], 0.01f);
					ImGui::DragFloat4("##Debug Mtx03 Row3", im->m_debug_mtx02.m[3], 0.01f);

					ImGui::Spacing(0, 6);

					const auto coloredit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_Float;

					SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ContainerBg", &im->ImGuiCol_ContainerBackground.x, coloredit_flags);
					SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ContainerBorder", &im->ImGuiCol_ContainerBorder.x, coloredit_flags);

					SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonGreen", &im->ImGuiCol_ButtonGreen.x, coloredit_flags);
					SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonYellow", &im->ImGuiCol_ButtonYellow.x, coloredit_flags);
					SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonRed", &im->ImGuiCol_ButtonRed.x, coloredit_flags);

				}, true, ICON_FA_ELLIPSIS_H, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
#endif
	}

	void cont_gamesettings_quick_cmd()
	{
		if (ImGui::Button("Save Current Settings", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0))) {
			game_settings::write_toml();
		}

		ImGui::SameLine();
		if (ImGui::Button("Reload GameSettings", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			if (!ImGui::IsPopupOpen("Reload GameSettings?")) {
				ImGui::OpenPopup("Reload GameSettings?");
			}
		}

		// popup
		if (ImGui::BeginPopupModal("Reload GameSettings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			shared::imgui::draw_background_blur();
			ImGui::Spacing(0.0f, 0.0f);

			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			auto line1_str = "You'll loose all unsaved changes if you continue!   ";
			auto line2_str = "To save your changes, use:";
			auto line3_str = "Save Current Settings";

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
			ImGui::TextUnformatted(line1_str);

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line2_str).x * 0.5f));
			ImGui::TextUnformatted(line2_str);

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
			ImGui::TextUnformatted(line3_str);
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0); ImGui::SameLine();

			ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				game_settings::parse_toml();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size)) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void imgui::tab_gamesettings()
	{
		const auto& im = imgui::get();
		const auto gs = game_settings::get();

		// quick commands
		{
			static float cont_quickcmd_height = 0.0f;
			cont_quickcmd_height = ImGui::Widget_ContainerWithCollapsingTitle("Quick Commands", cont_quickcmd_height, cont_gamesettings_quick_cmd,
				true, ICON_FA_TERMINAL, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// rendering related
		{
			static float cont_gs_renderer_height = 0.0f;
			cont_gs_renderer_height = ImGui::Widget_ContainerWithCollapsingTitle("Rendering Related Settings", cont_gs_renderer_height, [&]
			{
				ImGui::Spacing(0, 4);
				ImGui::SeparatorText(" Foliage ");
				ImGui::Spacing(0, 4);

				ImGui::Checkbox("Fixed function Trees", gs->fixed_function_trees.get_as<bool*>()); TT(gs->fixed_function_trees.get_tooltip_string().c_str());

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->tree_foliage_alpha_cutout_value.get_as<float*>();

					if (ImGui::DragFloat("Tree Alpha Cutout Value", gs_var_ptr, 0.02f, 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->tree_foliage_alpha_cutout_value.get_tooltip_string().c_str());
				}

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->grass_foliage_alpha_cutout_value.get_as<float*>();

					if (ImGui::DragFloat("Grass Alpha Cutout Value", gs_var_ptr, 0.02f, 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->grass_foliage_alpha_cutout_value.get_tooltip_string().c_str());
				}

				ImGui::Spacing(0, 8);
				ImGui::SeparatorText(" Culling ");
				ImGui::Spacing(0, 8);

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->nocull_dist_near_static.get_as<float*>();

					if (ImGui::DragFloat("Near NoCull Distance Static Objects", gs_var_ptr, 0.5f, 0.0f, FLT_MAX, "%.2f")) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->nocull_dist_near_static.get_tooltip_string().c_str());
				}

				// ----

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->nocull_dist_medium_static.get_as<float*>();
					const auto& near_dist = gs->nocull_dist_near_static.get_as<float>();

					if (ImGui::DragFloat("Medium NoCull Distance Static Objects", gs_var_ptr, 0.5f, near_dist, FLT_MAX, "%.2f")) {
						*gs_var_ptr = *gs_var_ptr < near_dist ? 0.0f : *gs_var_ptr;
					}
					TT(gs->nocull_dist_medium_static.get_tooltip_string().c_str());
				}

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->nocull_radius_medium_static.get_as<float*>();

					if (ImGui::DragFloat("Medium NoCull Radius Static Objects", gs_var_ptr, 0.5f, 0.0f, FLT_MAX, "%.2f")) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->nocull_radius_medium_static.get_tooltip_string().c_str());
				}

				// ----

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->nocull_dist_far_static.get_as<float*>();
					const auto& med_dist = gs->nocull_dist_medium_static.get_as<float>();

					if (ImGui::DragFloat("Far NoCull Distance Static Objects", gs_var_ptr, 0.5f, med_dist, FLT_MAX, "%.2f")) {
						*gs_var_ptr = *gs_var_ptr < med_dist ? 0.0f : *gs_var_ptr;
					}
					TT(gs->nocull_dist_far_static.get_tooltip_string().c_str());
				}

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->nocull_radius_far_static.get_as<float*>();

					if (ImGui::DragFloat("Far NoCull Radius Static Objects", gs_var_ptr, 0.5f, 0.0f, FLT_MAX, "%.2f")) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->nocull_radius_far_static.get_tooltip_string().c_str());
				}

				ImGui::Spacing(0, 8);
				ImGui::SeparatorText(" TimeCycle ");
				ImGui::Spacing(0, 8);

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->game_wetness_scalar.get_as<float*>();

					if (ImGui::DragFloat("Game Wetness Scalar", gs_var_ptr, 0.02f, 0.0f, 4.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->game_wetness_scalar.get_tooltip_string().c_str());
				}

				ImGui::Spacing(0, 4);
				ImGui::SeparatorText(" Dirt ");
				ImGui::Spacing(0, 4);

				ImGui::Checkbox("Decal Dirt Shader Usage", gs->decal_dirt_shader_usage.get_as<bool*>());
				TT(gs->decal_dirt_shader_usage.get_tooltip_string().c_str());

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->decal_dirt_shader_scalar.get_as<float*>();

					if (ImGui::DragFloat("Dirt Decal Shader Scalar", gs_var_ptr, 0.02f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->decal_dirt_shader_scalar.get_tooltip_string().c_str());
				}

				{
					//SET_CHILD_WIDGET_WIDTH_MAN(120.0f);
					auto gs_var_ptr = gs->decal_dirt_shader_contrast.get_as<float*>();

					if (ImGui::DragFloat("Dirt Decal Shader Contrast", gs_var_ptr, 0.02f, 0.0f, 8.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->decal_dirt_shader_contrast.get_tooltip_string().c_str());
				}

				ImGui::Spacing(0, 8);
				ImGui::SeparatorText(" Effects ");
				ImGui::Spacing(0, 8);

				{
					auto gs_var_ptr = gs->gta_rmptfx_litsprite_alpha_scalar.get_as<float*>();
					if (ImGui::DragFloat("gta_rmptfx_litsprite Alpha Scalar", gs_var_ptr, 0.02f, 0.0f, 20.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->gta_rmptfx_litsprite_alpha_scalar.get_tooltip_string().c_str());
				}

			}, true, ICON_FA_CAMERA, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// light related
		{
			static float cont_gs_light_height = 0.0f;
			cont_gs_light_height = ImGui::Widget_ContainerWithCollapsingTitle("Light Related Settings", cont_gs_light_height, [&]
			{
				ImGui::Checkbox("Translate Game Lights", gs->translate_game_lights.get_as<bool*>());
				TT(gs->translate_game_lights.get_tooltip_string().c_str());

				ImGui::DragFloat("Global Light Radius Scalar", gs->translate_game_light_radius_scalar.get_as<float*>(), 0.005f);
				ImGui::DragFloat("Global Light Intensity Scalar", gs->translate_game_light_intensity_scalar.get_as<float*>(), 0.005f);
				ImGui::DragFloat("Global Light Softness Offset", gs->translate_game_light_softness_offset.get_as<float*>(), 0.005f, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				TT("Add or Subtract this value for the softness constant of all lights");

				ImGui::DragFloat("Global Light Angle Offset", gs->translate_game_light_angle_offset.get_as<float*>(), 0.005f, -180.0f, 180.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				TT("Add or Subtract this value for the angle constant of all lights");

				ImGui::DragFloat("Global SunLight Intensity Scalar", gs->translate_sunlight_intensity_scalar.get_as<float*>(), 0.005f);

			}, true, ICON_FA_LIGHTBULB, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// emissive related
		{
			static float cont_gs_emissive_height = 0.0f;
			cont_gs_emissive_height = ImGui::Widget_ContainerWithCollapsingTitle("Emissive Related Settings", cont_gs_emissive_height, [&]
			{
				ImGui::Spacing(0, 4);
				ImGui::SeparatorText(" Vehicle ");
				ImGui::Spacing(0, 4);

				ImGui::DragFloat("Vehicle Light Emissive Scalar", gs->vehicle_lights_emissive_scalar.get_as<float*>(), 0.005f);
				TT(gs->vehicle_lights_emissive_scalar.get_tooltip_string().c_str());

				ImGui::Checkbox("Render Surfs a Second Time with Proxy Texture", gs->vehicle_lights_dual_render_proxy_texture.get_as<bool*>());
				TT(gs->vehicle_lights_dual_render_proxy_texture.get_tooltip_string().c_str());

				ImGui::Spacing(0, 8);
				ImGui::SeparatorText(" World ");
				ImGui::Spacing(0, 8);

				ImGui::Checkbox("Render Emissive Surfaces using Shaders", gs->render_emissive_surfaces_using_shaders.get_as<bool*>());
				TT(gs->render_emissive_surfaces_using_shaders.get_tooltip_string().c_str());

				ImGui::Checkbox("Assign Decal Texture Category to Emissive Surfaces", gs->assign_decal_category_to_emissive_surfaces.get_as<bool*>());
				TT(gs->assign_decal_category_to_emissive_surfaces.get_tooltip_string().c_str());

				{
					auto gs_var_ptr = gs->emissive_night_surfaces_emissive_scalar.get_as<float*>();
					if (ImGui::DragFloat("EmissiveNight Surfaces Scalar", gs_var_ptr, 0.02f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->emissive_night_surfaces_emissive_scalar.get_tooltip_string().c_str());
				}

				{
					auto gs_var_ptr = gs->emissive_surfaces_emissive_scalar.get_as<float*>();
					if (ImGui::DragFloat("Emissive Surfaces Scalar", gs_var_ptr, 0.001f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->emissive_surfaces_emissive_scalar.get_tooltip_string().c_str());
				}

				{
					auto gs_var_ptr = gs->emissive_strong_surfaces_emissive_scalar.get_as<float*>();
					if (ImGui::DragFloat("EmissiveStrong Surfaces Scalar", gs_var_ptr, 0.001f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
						*gs_var_ptr = *gs_var_ptr < 0.0f ? 0.0f : *gs_var_ptr;
					}
					TT(gs->emissive_strong_surfaces_emissive_scalar.get_tooltip_string().c_str());
				}

			}, true, ICON_FA_LIGHTBULB, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
	}

	void imgui::devgui()
	{
		ImGui::SetNextWindowSize(ImVec2(900, 800), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Devgui", &shared::globals::imgui_menu_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse/*, &shared::imgui::draw_window_blur_callback*/))
		{
			ImGui::End();
			return;
		}

		m_im_window_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
		m_im_window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);

		static bool im_demo_menu = false;
		if (im_demo_menu) {
			ImGui::ShowDemoWindow(&im_demo_menu);
		}

#define ADD_TAB(NAME, FUNC) \
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0)));			\
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 8));			\
	if (ImGui::BeginTabItem(NAME)) {																		\
		ImGui::PopStyleVar(1);																				\
		if (ImGui::BeginChild("##child_" NAME, ImVec2(0, ImGui::GetContentRegionAvail().y - 38), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar )) {	\
			FUNC(); ImGui::EndChild();																		\
		} else {																							\
			ImGui::EndChild();																				\
		} ImGui::EndTabItem();																				\
	} else { ImGui::PopStyleVar(1); } ImGui::PopStyleColor();

		// ---------------------------------------

		const auto col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.0f));
		const auto col_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f));
		const auto col_border = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.8f));
		const auto pre_tabbar_spos = ImGui::GetCursorScreenPos() - ImGui::GetStyle().WindowPadding;

		ImGui::GetWindowDrawList()->AddRectFilledMultiColor(pre_tabbar_spos, pre_tabbar_spos + ImVec2(ImGui::GetWindowWidth(), 40.0f),
			col_top, col_top, col_bottom, col_bottom);

		ImGui::GetWindowDrawList()->AddLine(pre_tabbar_spos + ImVec2(0, 40.0f), pre_tabbar_spos + ImVec2(ImGui::GetWindowWidth(), 40.0f),
			col_border, 1.0f);

		ImGui::SetCursorScreenPos(pre_tabbar_spos + ImVec2(12,8));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 8));
		ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		if (ImGui::BeginTabBar("devgui_tabs"))
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);
			ADD_TAB("Dev", tab_dev);
			ADD_TAB("Game Settings", tab_gamesettings);
			ImGui::EndTabBar();
		}
		else {
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);
		}
#undef ADD_TAB

		{
			ImGui::Separator();
			const char* movement_hint_str = "Press and Hold the Right Mouse Button outside ImGui to allow for Game Input ";
			const auto avail_width = ImGui::GetContentRegionAvail().x;
			float cur_pos = avail_width - 54.0f;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);
				const auto spos = ImGui::GetCursorScreenPos();
				ImGui::TextUnformatted(m_devgui_custom_footer_content.c_str());
				ImGui::SetCursorScreenPos(spos);
				m_devgui_custom_footer_content.clear();
			}
			

			ImGui::SetCursorPos(ImVec2(cur_pos, ImGui::GetCursorPosY() + 2.0f));
			if (ImGui::Button("Demo", ImVec2(50, 0))) {
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

	void imgui::on_present()
	{
		if (auto* im = imgui::get(); im)
		{
			if (const auto dev = shared::globals::d3d_device; dev)
			{
				if (!im->m_initialized_device)
				{
					ImGui_ImplDX9_Init(dev);
					im->m_initialized_device = true;
				}

				if (im->m_initialized_device)
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

					auto& io = ImGui::GetIO();

					if (shared::globals::imgui_menu_open) 
					{
						io.MouseDrawCursor = true;
						im->devgui();
					}
					else 
					{
						io.MouseDrawCursor = false;
					}

					shared::globals::imgui_is_rendering = true;
					ImGui::EndFrame();
					ImGui::Render();
					ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
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
		style.IndentSpacing = 0.0f;
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
		colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.26f, 0.26f, 0.26f, 0.78f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.28f, 0.28f, 0.28f, 0.92f);
		colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.23f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.25f, 0.27f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.07f, 0.39f, 0.47f, 0.59f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.98f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 0.98f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 0.98f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.39f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.54f, 0.54f, 0.54f, 0.47f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.78f, 0.78f, 0.78f, 0.33f);
		colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.39f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.31f);
		colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.45f, 0.45f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.17f, 0.25f, 0.27f, 0.78f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.25f, 0.27f, 0.78f);
		colors[ImGuiCol_Separator] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15f, 0.52f, 0.66f, 0.30f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.69f, 0.84f, 0.39f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.43f, 0.43f, 0.43f, 0.51f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.07f, 0.39f, 0.47f, 0.59f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.69f, 0.84f, 0.39f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.53f, 0.66f, 0.39f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.37f);
		colors[ImGuiCol_TabSelected] = ImVec4(0.11f, 0.39f, 0.51f, 0.64f);
		colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.10f, 0.34f, 0.43f, 0.30f);
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
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.11f, 0.42f, 0.51f, 0.35f);
		colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.00f, 0.51f, 0.39f, 0.31f);
		colors[ImGuiCol_NavCursor] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.56f);

		// custom colors
		ImGuiCol_ButtonGreen = ImVec4(0.3f, 0.4f, 0.05f, 0.7f);
		ImGuiCol_ButtonYellow = ImVec4(0.4f, 0.3f, 0.1f, 0.8f);
		ImGuiCol_ButtonRed = ImVec4(0.48f, 0.15f, 0.15f, 1.00f);
		ImGuiCol_ContainerBackground = ImVec4(0.220f, 0.220f, 0.220f, 0.863f);
		ImGuiCol_ContainerBorder = ImVec4(0.099f, 0.099f, 0.099f, 0.901f);
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

		ImGui_ImplWin32_Init(shared::globals::main_window);
		g_game_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(shared::globals::main_window, GWLP_WNDPROC, LONG_PTR(wnd_proc_hk)));

		m_initialized = true;

		std::cout << "[IMGUI] loaded\n";
	}
}
