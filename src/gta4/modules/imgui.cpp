#include "std_include.hpp"
#include "imgui.hpp"

#include "game_settings.hpp"
#include "imgui_internal.h"
#include "map_settings.hpp"
#include "natives.hpp"
#include "remix_lights.hpp"
#include "remix_vars.hpp"
#include "renderer.hpp"
#include "shared/common/toml_ext.hpp"
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

#define CENTER_URL(text, link)					\
	ImGui::SetCursorForCenteredText((text));	\
	ImGui::TextURL((text), (link), true);

constexpr float TREENODE_SPACING = 6.0f;
constexpr float TREENODE_SPACING_INSIDE = 6.0f;

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

		if (shared::globals::imgui_menu_open)
		{
			auto& io = ImGui::GetIO();
			ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);
		} else {
			shared::globals::imgui_allow_input_bypass = false; // always reset if there is no imgui window open
		}

		return shared::globals::imgui_menu_open;
	}

	// ------

	void imgui::tab_about()
	{
		if (tex_addons::berry)
		{
			const float cursor_y = ImGui::GetCursorPosY();
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() * 0.85f, 24));
			ImGui::Image((ImTextureID)tex_addons::berry, ImVec2(48.0f, 48.0f), ImVec2(0.03f, 0.03f), ImVec2(0.96f, 0.96f));
			ImGui::SetCursorPosY(cursor_y);
		}

		ImGui::Spacing(0.0f, 20.0f);

		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText("GTAIV - RTX REMIX COMPATIBILITY MOD");
		ImGui::PopFont();
		ImGui::CenterText("                      by #xoxor4d");

		ImGui::Spacing(0.0f, 24.0f);
		ImGui::CenterText("current version");

		const char* version_str = shared::utils::va("%d.%d.%d :: %s", 
			COMP_MOD_VERSION_MAJOR, COMP_MOD_VERSION_MINOR, COMP_MOD_VERSION_PATCH, __DATE__);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText(version_str);

#if DEBUG
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.64f, 0.23f, 0.18f, 1.0f));
		ImGui::CenterText("DEBUG BUILD");
		ImGui::PopStyleColor();
#endif
		ImGui::PopFont();

		ImGui::Spacing(0.0f, 16.0f);
		CENTER_URL("Github Repository", "https://github.com/xoxor4d/gta4-rtx");
		CENTER_URL("Github Project Page", "https://xoxor4d.github.io/projects/gta4-rtx");
		CENTER_URL("Latest build", "https://github.com/xoxor4d/gta4-rtx/releases");

		ImGui::Spacing(0.0f, 16.0f);
		ImGui::Separator();
		ImGui::Spacing(0.0f, 16.0f);

		const char* credits_title_str = "Credits / Thanks to:";
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText(credits_title_str);
		ImGui::PopFont();

		ImGui::Spacing(0.0f, 8.0f);

		CENTER_URL("NVIDIA - RTX Remix", "https://github.com/NVIDIAGameWorks/rtx-remix");
		CENTER_URL("Dear Imgui", "https://github.com/ocornut/imgui");
		CENTER_URL("Imgui Blur Effect", "https://github.com/3r4y/imgui-blur-effect");
		CENTER_URL("Minhook", "https://github.com/TsudaKageyu/minhook");
		CENTER_URL("Toml11", "https://github.com/ToruNiina/toml11");
		CENTER_URL("Ultimate-ASI-Loader", "https://github.com/ThirteenAG/Ultimate-ASI-Loader");
		CENTER_URL("AssaultKifle47", "https://github.com/akifle47");
		CENTER_URL("FusionFix", "https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix");
		CENTER_URL("FusionShaders", "https://github.com/Parallellines0451/GTAIV.EFLC.FusionShaders");
		CENTER_URL("Rage-Shader-Editor", "https://github.com/ImpossibleEchoes/rage-shader-editor-cpp");
		CENTER_URL("IV-SDK", "https://github.com/Zolika1351/iv-sdk/");
		CENTER_URL("IV-SDK-DotNet", "https://github.com/ClonkAndre/IV-SDK-DotNet");
		CENTER_URL("DayL", "https://www.gtainside.de/de/user/falcogray");

		ImGui::Spacing(0.0f, 24.0f);
		ImGui::CenterText("And of course, all my fellow Ko-Fi and Patreon supporters");
		ImGui::CenterText("and all the people that helped along the way.");
		ImGui::Spacing(0.0f, 4.0f);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText("Thank you!");
		ImGui::PopFont();
	}

	void dev_shader_container()
	{
		static const auto& im = imgui::get();

		ImGui::Checkbox("Enable Ignore Shader Logic", &im->m_dbg_enable_ignore_shader_logic);
		ImGui::BeginDisabled(!im->m_dbg_enable_ignore_shader_logic);
		{
			ImGui::Checkbox("Also Ignore on DrawPrimitive calls", &im->m_dbg_ignore_drawprimitive);
			ImGui::Checkbox("Ignore ALL", &im->m_dbg_ignore_all);

			static bool toggle_all_state = false;
			if(ImGui::Checkbox("Toggle All", &toggle_all_state))
			{
				im->m_dbg_ignore_cascade = toggle_all_state;
				im->m_dbg_ignore_deferred_lighting = toggle_all_state;
				im->m_dbg_ignore_gpuptfx_simplerender = toggle_all_state;
				im->m_dbg_ignore_gta_atmoscatt_clouds = toggle_all_state;
				im->m_dbg_ignore_gta_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_cutout_fence = toggle_all_state;
				im->m_dbg_ignore_gta_decal = toggle_all_state;
				im->m_dbg_ignore_gta_decal_amb_only = toggle_all_state;
				im->m_dbg_ignore_gta_decal_dirt = toggle_all_state;
				im->m_dbg_ignore_gta_decal_glue = toggle_all_state;
				im->m_dbg_ignore_gta_decal_normal_only = toggle_all_state;
				im->m_dbg_ignore_gta_default = toggle_all_state;
				im->m_dbg_ignore_gta_diffuse_instance = toggle_all_state;
				im->m_dbg_ignore_gta_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_emissivestrong = toggle_all_state;
				im->m_dbg_ignore_gta_glass = toggle_all_state;
				im->m_dbg_ignore_gta_glass_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_glass_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_glass_normal_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_glass_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_glass_spec = toggle_all_state;
				im->m_dbg_ignore_gta_grass = toggle_all_state;
				im->m_dbg_ignore_gta_hair_sorted_alpha = toggle_all_state;
				im->m_dbg_ignore_gta_hair_sorted_alpha_exp = toggle_all_state;
				im->m_dbg_ignore_gta_im = toggle_all_state;
				im->m_dbg_ignore_gta_normal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect_alpha = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_parallax = toggle_all_state;
				im->m_dbg_ignore_gta_parallax_specmap = toggle_all_state;
				im->m_dbg_ignore_gta_parallax_steep = toggle_all_state;
				im->m_dbg_ignore_gta_ped = toggle_all_state;
				im->m_dbg_ignore_gta_ped_face = toggle_all_state;
				im->m_dbg_ignore_gta_ped_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_ped_skin = toggle_all_state;
				im->m_dbg_ignore_gta_ped_skin_blendshape = toggle_all_state;
				im->m_dbg_ignore_gta_projtex = toggle_all_state;
				im->m_dbg_ignore_gta_projtex_steep = toggle_all_state;
				im->m_dbg_ignore_gta_radar = toggle_all_state;
				im->m_dbg_ignore_gta_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_gpurender = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_litsprite = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_mesh = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_raindrops = toggle_all_state;
				im->m_dbg_ignore_gta_spec = toggle_all_state;
				im->m_dbg_ignore_gta_spec_decal = toggle_all_state;
				im->m_dbg_ignore_gta_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_spec_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_2lyr = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_3lyr = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_4lyr = toggle_all_state;
				im->m_dbg_ignore_gta_trees = toggle_all_state;
				im->m_dbg_ignore_gta_trees_extended = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_badges = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_basic = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_chrome = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_disc = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_generic = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_interior = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_interior2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_lightsemissive = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_mesh = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint1 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint3 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims1 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims3 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rubber = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_shuts = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_tire = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_vehglass = toggle_all_state;
				im->m_dbg_ignore_gta_wire = toggle_all_state;
				im->m_dbg_ignore_mirror = toggle_all_state;
				im->m_dbg_ignore_rage_atmoscatt_clouds = toggle_all_state;
				im->m_dbg_ignore_rage_billboard_nobump = toggle_all_state;
				im->m_dbg_ignore_rage_bink = toggle_all_state;
				im->m_dbg_ignore_rage_default = toggle_all_state;
				im->m_dbg_ignore_rage_fastmipmap = toggle_all_state;
				im->m_dbg_ignore_rage_im = toggle_all_state;
				im->m_dbg_ignore_rage_perlinnoise = toggle_all_state;
				im->m_dbg_ignore_rage_postfx = toggle_all_state;
				im->m_dbg_ignore_rmptfx_collision = toggle_all_state;
				im->m_dbg_ignore_rmptfx_default = toggle_all_state;
				im->m_dbg_ignore_rmptfx_litsprite = toggle_all_state;
				im->m_dbg_ignore_shadowSmartBlit = toggle_all_state;
				im->m_dbg_ignore_shadowZ = toggle_all_state;
				im->m_dbg_ignore_shadowZDir = toggle_all_state;
				im->m_dbg_ignore_water = toggle_all_state;
				im->m_dbg_ignore_waterTex = toggle_all_state;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("Uncategorized ..."))
			{
				ImGui::Checkbox("cascade.fxc", &im->m_dbg_ignore_cascade);
				ImGui::Checkbox("deferred_lighting.fxc", &im->m_dbg_ignore_deferred_lighting);
				ImGui::Checkbox("gpuptfx_simplerender.fxc", &im->m_dbg_ignore_gpuptfx_simplerender);
				ImGui::Checkbox("gta_atmoscatt_clouds.fxc", &im->m_dbg_ignore_gta_atmoscatt_clouds);
				ImGui::Checkbox("gta_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_cubemap_reflect);
				ImGui::Checkbox("gta_cutout_fence.fxc", &im->m_dbg_ignore_gta_cutout_fence);
				ImGui::Checkbox("gta_default.fxc", &im->m_dbg_ignore_gta_default);
				ImGui::Checkbox("gta_diffuse_instance.fxc", &im->m_dbg_ignore_gta_diffuse_instance);
				ImGui::Checkbox("gta_grass.fxc", &im->m_dbg_ignore_gta_grass);
				ImGui::Checkbox("gta_hair_sorted_alpha.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha);
				ImGui::Checkbox("gta_hair_sorted_alpha_exp.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha_exp);
				ImGui::Checkbox("gta_im.fxc", &im->m_dbg_ignore_gta_im);
				ImGui::Checkbox("gta_projtex.fxc", &im->m_dbg_ignore_gta_projtex);
				ImGui::Checkbox("gta_projtex_steep.fxc", &im->m_dbg_ignore_gta_projtex_steep);
				ImGui::Checkbox("gta_radar.fxc", &im->m_dbg_ignore_gta_radar);
				ImGui::Checkbox("gta_reflect.fxc", &im->m_dbg_ignore_gta_reflect);
				ImGui::Checkbox("gta_reflect_decal.fxc", &im->m_dbg_ignore_gta_reflect_decal);
				ImGui::Checkbox("gta_terrain_va_2lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_2lyr);
				ImGui::Checkbox("gta_terrain_va_3lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_3lyr);
				ImGui::Checkbox("gta_terrain_va_4lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_4lyr);
				ImGui::Checkbox("gta_trees.fxc", &im->m_dbg_ignore_gta_trees);
				ImGui::Checkbox("gta_trees_extended.fxc", &im->m_dbg_ignore_gta_trees_extended);
				ImGui::Checkbox("gta_wire.fxc", &im->m_dbg_ignore_gta_wire);
				ImGui::Checkbox("mirror.fxc", &im->m_dbg_ignore_mirror);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_DECAL ..."))
			{
				ImGui::Checkbox("gta_decal.fxc", &im->m_dbg_ignore_gta_decal);
				ImGui::Checkbox("gta_decal_amb_only.fxc", &im->m_dbg_ignore_gta_decal_amb_only);
				ImGui::Checkbox("gta_decal_dirt.fxc", &im->m_dbg_ignore_gta_decal_dirt);
				ImGui::Checkbox("gta_decal_glue.fxc", &im->m_dbg_ignore_gta_decal_glue);
				ImGui::Checkbox("gta_decal_normal_only.fxc", &im->m_dbg_ignore_gta_decal_normal_only);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_EMISSIVE ..."))
			{
				ImGui::Checkbox("gta_emissive.fxc", &im->m_dbg_ignore_gta_emissive);
				ImGui::Checkbox("gta_emissivenight.fxc", &im->m_dbg_ignore_gta_emissivenight);
				ImGui::Checkbox("gta_emissivestrong.fxc", &im->m_dbg_ignore_gta_emissivestrong);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_GLASS ..."))
			{
				ImGui::Checkbox("gta_glass.fxc", &im->m_dbg_ignore_gta_glass);
				ImGui::Checkbox("gta_glass_emissive.fxc", &im->m_dbg_ignore_gta_glass_emissive);
				ImGui::Checkbox("gta_glass_emissivenight.fxc", &im->m_dbg_ignore_gta_glass_emissivenight);
				ImGui::Checkbox("gta_glass_normal_spec_reflect.fxc", &im->m_dbg_ignore_gta_glass_normal_spec_reflect);
				ImGui::Checkbox("gta_glass_reflect.fxc", &im->m_dbg_ignore_gta_glass_reflect);
				ImGui::Checkbox("gta_glass_spec.fxc", &im->m_dbg_ignore_gta_glass_spec);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_NORMAL ..."))
			{
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
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_PARALLAX ..."))
			{
				ImGui::Checkbox("gta_parallax.fxc", &im->m_dbg_ignore_gta_parallax);
				ImGui::Checkbox("gta_parallax_specmap.fxc", &im->m_dbg_ignore_gta_parallax_specmap);
				ImGui::Checkbox("gta_parallax_steep.fxc", &im->m_dbg_ignore_gta_parallax_steep);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_PED ..."))
			{
				ImGui::Checkbox("gta_ped.fxc", &im->m_dbg_ignore_gta_ped);
				ImGui::Checkbox("gta_ped_face.fxc", &im->m_dbg_ignore_gta_ped_face);
				ImGui::Checkbox("gta_ped_reflect.fxc", &im->m_dbg_ignore_gta_ped_reflect);
				ImGui::Checkbox("gta_ped_skin.fxc", &im->m_dbg_ignore_gta_ped_skin);
				ImGui::Checkbox("gta_ped_skin_blendshape.fxc", &im->m_dbg_ignore_gta_ped_skin_blendshape);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_RMPTFX ..."))
			{
				ImGui::Checkbox("gta_rmptfx_gpurender.fxc", &im->m_dbg_ignore_gta_rmptfx_gpurender);
				ImGui::Checkbox("gta_rmptfx_litsprite.fxc", &im->m_dbg_ignore_gta_rmptfx_litsprite);
				ImGui::Checkbox("gta_rmptfx_mesh.fxc", &im->m_dbg_ignore_gta_rmptfx_mesh);
				ImGui::Checkbox("gta_rmptfx_raindrops.fxc", &im->m_dbg_ignore_gta_rmptfx_raindrops);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_SPEC ..."))
			{
				ImGui::Checkbox("gta_spec.fxc", &im->m_dbg_ignore_gta_spec);
				ImGui::Checkbox("gta_spec_decal.fxc", &im->m_dbg_ignore_gta_spec_decal);
				ImGui::Checkbox("gta_spec_reflect.fxc", &im->m_dbg_ignore_gta_spec_reflect);
				ImGui::Checkbox("gta_spec_reflect_decal.fxc", &im->m_dbg_ignore_gta_spec_reflect_decal);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_VEHICLE ..."))
			{
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
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("RAGE ..."))
			{
				ImGui::Checkbox("rage_atmoscatt_clouds.fxc", &im->m_dbg_ignore_rage_atmoscatt_clouds);
				ImGui::Checkbox("rage_billboard_nobump.fxc", &im->m_dbg_ignore_rage_billboard_nobump);
				ImGui::Checkbox("rage_bink.fxc", &im->m_dbg_ignore_rage_bink);
				ImGui::Checkbox("rage_default.fxc", &im->m_dbg_ignore_rage_default);
				ImGui::Checkbox("rage_fastmipmap.fxc", &im->m_dbg_ignore_rage_fastmipmap);
				ImGui::Checkbox("rage_im.fxc", &im->m_dbg_ignore_rage_im);
				ImGui::Checkbox("rage_perlinnoise.fxc", &im->m_dbg_ignore_rage_perlinnoise);
				ImGui::Checkbox("rage_postfx.fxc", &im->m_dbg_ignore_rage_postfx);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("RMPTFX ..."))
			{
				ImGui::Checkbox("rmptfx_collision.fxc", &im->m_dbg_ignore_rmptfx_collision);
				ImGui::Checkbox("rmptfx_default.fxc", &im->m_dbg_ignore_rmptfx_default);
				ImGui::Checkbox("rmptfx_litsprite.fxc", &im->m_dbg_ignore_rmptfx_litsprite);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("SHADOW ..."))
			{
				ImGui::Checkbox("shadowSmartBlit.fxc", &im->m_dbg_ignore_shadowSmartBlit);
				ImGui::Checkbox("shadowZ.fxc", &im->m_dbg_ignore_shadowZ);
				ImGui::Checkbox("shadowZDir.fxc", &im->m_dbg_ignore_shadowZDir);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("WATER ..."))
			{
				ImGui::Checkbox("water.fxc", &im->m_dbg_ignore_water);
				ImGui::Checkbox("waterTex.fxc", &im->m_dbg_ignore_waterTex);
				ImGui::TreePop();
			}
			ImGui::EndDisabled();
		}
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
			ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
			ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
			ImGui::Text("%d total", stat.get_total());
			ImGui::PopFont();
			break;

		case StatObj::Mode::ConditionalCheck:
			ImGui::Text("%s", name);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
			ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
			ImGui::Text("%d total, %d successful", stat.get_total(), stat.get_successful());
			ImGui::PopFont();
			break;

		default:
			throw std::runtime_error("Uncovered Mode in StatObj");
		}
	}


	void dev_debug_container()
	{
		static const auto& im = imgui::get();

#ifdef LOG_SHADERPRESETS
		if (ImGui::Button("Copy Shader PresetLog to Clipboard", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			auto log_str = [&]()
				{
					std::string out;
					for (auto& p : im->preset_list) {
						out += p.second + " = " + std::to_string(p.first) + ",\n";
					}
					return out;
				};

			ImGui::LogToClipboard();
			ImGui::LogText("%s", log_str().c_str());
			ImGui::LogFinish();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
#endif
		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Statistics ..."))
		{
			im->m_stats.enable_tracking(true);
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			im->m_stats.draw_stats();
			ImGui::TreePop();
		}
		else {
			im->m_stats.enable_tracking(false);
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Disable Functionalities ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::Checkbox("Toggle Shader/FF Rendering (On: Shader)", &im->m_dbg_toggle_ff);
			ImGui::Checkbox("Disable Pixelshader for Static Objects Rendered via FF", &im->m_dbg_disable_ps_for_static);
			TT(	"Remix only grabs VS output when no pixel shader is bound.\n"
				"If this makes a difference somewhere, please report it on Discord or GitHub!")

			ImGui::Checkbox("Skip DrawIndexedPrim Logic", &im->m_dbg_skip_draw_indexed_checks); TT("Disables all checks in DrawIndexedPrim wrapper and renders via Shaders");
			ImGui::Checkbox("Do not restore Drawcall Context", &im->m_dbg_do_not_restore_drawcall_context);
			ImGui::Checkbox("Do not restore Drawcall Context on Early Out", &im->m_dbg_do_not_restore_drawcall_context_on_early_out);

			ImGui::Spacing(0, 4);

			ImGui::Checkbox("Never Cull Statics", &im->m_dbg_never_cull_statics); TT("No distance/radii checks for custom anti culling code.");
			ImGui::Checkbox("Disable HUD Hack", &im->m_dbg_disable_hud_fixup); TT("Disables hack that helps remix detect the first HUD elem");
			ImGui::Checkbox("Disable IgnoreBackedLighting Enforcement", &im->m_dbg_disable_ignore_baked_lighting_enforcement);
			TT("CompMod forces the IgnoreBakedLighting category for almost every mesh. This disables that")

			//ImGui::Checkbox("Disable Alphablend On VEHGLASS", &im->m_dbg_vehglass_disable_alphablend);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Do not render ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Do not render Static", &im->m_dbg_do_not_render_static);
			ImGui::Checkbox("Do not render Vehicle", &im->m_dbg_do_not_render_vehicle);
			ImGui::Checkbox("Do not render Instances", &im->m_dbg_do_not_render_instances);
			ImGui::Checkbox("Do not render Stencil 0", &im->m_dbg_do_not_render_stencil_zero);
			ImGui::Checkbox("Do not render Tree Foliage", &im->m_dbg_do_not_render_tree_foliage);
			ImGui::Checkbox("Do not render FX", &im->m_dbg_do_not_render_fx);
			ImGui::Checkbox("Do not render FF", &im->m_dbg_do_not_render_ff);
			ImGui::Checkbox("Do not render Prims with VS", &im->m_dbg_do_not_render_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Indexed Prims with VS", &im->m_dbg_do_not_render_indexed_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Water", &im->m_dbg_do_not_render_water);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Light related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Api Lights 3D", &im->m_dbg_visualize_api_lights); TT("Visualize all spawned api lights");
			ImGui::Checkbox("Visualize Unstable Light Hashes", &im->m_dbg_visualize_api_light_unstable_hashes);
			ImGui::Checkbox("Skip Ignore Light Hash Logic", &im->m_dbg_disable_ignore_light_hash_logic); TT("For performance impact testing");


			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Ignore Lights with certain flags ...");
			ImGui::Spacing(0, 4);

			{
				ImGui::Checkbox("Enable Flag Logic", &im->m_dbg_ignore_lights_with_flag_logic);

				ImGui::BeginDisabled(!im->m_dbg_ignore_lights_with_flag_logic);
				{
					static const char* bit_list[] = {
						"Bit 0:  0x1",
						"Bit 1:  0x2",
						"Bit 2:  0x4",
						"Bit 3:  0x8",
						"Bit 4:  0x10",
						"Bit 5:  0x20",
						"Bit 6:  0x40",
						"Bit 7:  0x80",
						"Bit 8:  0x100",
						"Bit 9:  0x200",
						"Bit 10: 0x400",
						"Bit 11: 0x800",
						"Bit 12: 0x1000",
						"Bit 13: 0x2000",
						"Bit 14: 0x4000",
						"Bit 15: 0x8000",
						"Bit 16: 0x10000"
					};
					static const int num_bit_list = std::size(bit_list);

					uint32_t current_selection_flag1 = im->m_dbg_ignore_lights_with_flag_01;
					if (ImGui::BeginCombo("Ignore Lights with Flag:", bit_list[current_selection_flag1]))
					{
						for (auto i = 0u; i < num_bit_list; ++i)
						{
							const bool is_selected = (current_selection_flag1 == i);
							if (ImGui::Selectable(bit_list[i], is_selected)) {
								current_selection_flag1 = i;
							}

							if (is_selected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					if (current_selection_flag1 != (uint32_t)im->m_dbg_ignore_lights_with_flag_01) {
						im->m_dbg_ignore_lights_with_flag_01 = static_cast<int>(current_selection_flag1);
					}

					ImGui::Checkbox("ADD Second Flag", &im->m_dbg_ignore_lights_with_flag_add_second_flag);
					TT("This will add a second flag the light has to have to ignore it.\n"
						"In short, both flags have to be set for a light to be ignored.")

						ImGui::BeginDisabled(!im->m_dbg_ignore_lights_with_flag_add_second_flag);
					{
						uint32_t current_selection_flag2 = im->m_dbg_ignore_lights_with_flag_02;
						if (ImGui::BeginCombo("Additional Flag:", bit_list[current_selection_flag2]))
						{
							for (auto i = 0u; i < num_bit_list; ++i)
							{
								const bool is_selected = (current_selection_flag2 == i);
								if (ImGui::Selectable(bit_list[i], is_selected)) {
									current_selection_flag2 = i;
								}

								if (is_selected) {
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}

						if (current_selection_flag2 != (uint32_t)im->m_dbg_ignore_lights_with_flag_02) {
							im->m_dbg_ignore_lights_with_flag_02 = static_cast<int>(current_selection_flag2);
						}

						ImGui::EndDisabled();
					}

					ImGui::EndDisabled();
				}
			}

			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Directional Light Info");
			ImGui::Spacing(0, 4);

			for (auto i = 0u; i < 2; i++)
			{
				auto& def = game::g_directionalLights[i];
				ImGui::Text("Directional Light #%d", i);
				ImGui::Text("mDirection: [%.2f, %.2f, %.2f]", def.mDirection.x, def.mDirection.y, def.mDirection.z);
				ImGui::Text("mColor: [%.2f, %.2f, %.2f, %.2f]", def.mColor.x, def.mColor.y, def.mColor.z, def.mColor.w);
				ImGui::Text("mIntensity: [%.2f]", def.mIntensity);
				ImGui::Spacing(0, 4);
				ImGui::Separator();
				ImGui::Spacing(0, 4);
			}

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Emissive Related"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SliderInt("Tag EmissiveNight Surfaces as Category ..", &im->m_dbg_tag_static_emissive_as_index, -1, 23, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Checkbox("FF Emissive: Enable Alphablend on non alpha Emissives", &im->m_dbg_emissive_ff_with_alphablend);
			//ImGui::Checkbox("FF Emissive: Enable Emissive Override", &im->m_dbg_emissive_nonalpha_override);
			//ImGui::DragFloat("FF Emissive: Enable Emissive Override Scale", &im->m_dbg_emissive_nonalpha_override_scale, 0.005f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Visualizations / Rendering related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Decal Renderstates", &im->m_dbg_visualize_decal_renderstates); TT("Visualize renderstates of nearby decal surfaces.");

			ImGui::Spacing(0, 4);
			ImGui::SliderInt("Tag Exp Hair Surfaces as Category ..", &im->m_dbg_tag_exp_hair_as_index, -1, 23, "%d", ImGuiSliderFlags_AlwaysClamp);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Timecycle related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::SliderInt("Used Timecycle for Remix Translation ..", &im->m_dbg_used_timecycle, -1, 2, "%d", ImGuiSliderFlags_AlwaysClamp);
			TT("Sets the Timecycle to be used to translate its settings to fitting remix variables.\n"
				"-1: No override\n0: Timecycle 1 (World/Interior)\n1: Timecycle 2 (World/Interior)\n3: Timecycle 3 (Cutscenes)");

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Temp Debug Values"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat3("Debug Vector", &im->m_debug_vector.x, 0.01f);
			ImGui::DragFloat3("Debug Vector 2", &im->m_debug_vector2.x, 0.1f);
			ImGui::Checkbox("Debug Bool 1", &im->m_dbg_debug_bool01);
			ImGui::DragInt("Debug Int 1", &im->m_dbg_int_01, 0.005f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Phone Projection Offset Matrix"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Phone Proj Offset Row0", im->m_dbg_phone_projection_matrix_offset.m[0], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row1", im->m_dbg_phone_projection_matrix_offset.m[1], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row2", im->m_dbg_phone_projection_matrix_offset.m[2], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row3", im->m_dbg_phone_projection_matrix_offset.m[3], 0.01f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Offset Matrix 2"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Debug Mtx02 Row0", im->m_debug_mtx02.m[0], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row1", im->m_debug_mtx02.m[1], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row2", im->m_debug_mtx02.m[2], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row3", im->m_debug_mtx02.m[3], 0.01f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Offset Matrix 3"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Debug Mtx03 Row0", im->m_debug_mtx02.m[0], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row1", im->m_debug_mtx02.m[1], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row2", im->m_debug_mtx02.m[2], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row3", im->m_debug_mtx02.m[3], 0.01f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Custom ImGui Colors"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			const auto coloredit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_Float;

			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ContainerBg", &im->ImGuiCol_ContainerBackground.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ContainerBorder", &im->ImGuiCol_ContainerBorder.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonGreen", &im->ImGuiCol_ButtonGreen.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonYellow", &im->ImGuiCol_ButtonYellow.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonRed", &im->ImGuiCol_ButtonRed.x, coloredit_flags);
			ImGui::TreePop();
		}
	}

	void dev_other_container()
	{
		static const auto& im = imgui::get();

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::Checkbox("Do not Pause on Lost Focus", &im->m_do_not_pause_on_lost_focus)) {
			im->m_do_not_pause_on_lost_focus_changed = true;
		}
	}

	void imgui::tab_dev()
	{
		static const auto& im = imgui::get();

		{
			static float cont_cull_height = 0.0f;
			cont_cull_height = ImGui::Widget_ContainerWithCollapsingTitle("Shaders", cont_cull_height, 
				dev_shader_container, false, ICON_FA_ELLIPSIS_H, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

//#if DEBUG
		{
			static float cont_debug_height = 0.0f;
			cont_debug_height = ImGui::Widget_ContainerWithCollapsingTitle("DEBUG Section", cont_debug_height, 
				dev_debug_container, false, ICON_FA_ELLIPSIS_H, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
//#endif

		{
			static float cont_other_height = 0.0f;
			cont_other_height = ImGui::Widget_ContainerWithCollapsingTitle("Other Settings", cont_other_height,
				dev_other_container, false, ICON_FA_MEMORY, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
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


	bool gamesettings_bool_widget(const char* desc, game_settings::variable& var)
	{
		const auto gs_var_ptr = var.get_as<bool*>();
		const bool result = ImGui::Checkbox(desc, gs_var_ptr);
		TT(var.get_tooltip_string().c_str());
		return result;
	}

	bool gamesettings_float_widget(const char* desc, game_settings::variable& var, const float& min = 0.0f, const float& max = 0.0f, const float& speed = 0.02f)
	{
		const auto gs_var_ptr = var.get_as<float*>();
		const bool result = ImGui::DragFloat(desc, gs_var_ptr, speed, min, max, "%.2f", (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
		TT(var.get_tooltip_string().c_str());
		return result;
	}

	// --------------------

	void gamesettings_rendering_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();

		const float inbetween_spacing = 8.0f;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" General ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Load ColorMaps Only", gs->load_colormaps_only);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Foliage ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Fixed function Trees", gs->fixed_function_trees);
		gamesettings_float_widget("Tree Alpha Cutout Value", gs->tree_foliage_alpha_cutout_value, 0.0f, 20.0f);
		//gamesettings_float_widget("Grass Alpha Cutout Value", gs->grass_foliage_alpha_cutout_value, 0.0f, 20.0f);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Hair ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("NPC Hair Alpha Testing", gs->npc_expensive_hair_alpha_testing);
		ImGui::BeginDisabled(!gs->npc_expensive_hair_alpha_testing.get_as<bool>());
		{
			gamesettings_float_widget("NPC Hair Alpha Cutout Value", gs->npc_expensive_hair_alpha_cutout_value, 0.0f, 1.0f);
			ImGui::EndDisabled();
		}

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Anti Culling of Static Objects ");
		ImGui::Spacing(0, 4);

		gamesettings_float_widget("Near: No Culling Until Distance", gs->nocull_dist_near_static, 0.0f, FLT_MAX, 0.5f);
		
		ImGui::Spacing(0, inbetween_spacing);

		// ----

		gamesettings_float_widget("Near to Medium Cascade: Medium Distance", gs->nocull_dist_medium_static, 0.0f, FLT_MAX, 0.5f);
		gamesettings_float_widget("Near to Medium Cascade: Min. Object Radius", gs->nocull_radius_medium_static, 0.0f, FLT_MAX, 0.5f);

		ImGui::Spacing(0, inbetween_spacing);

		// ----

		gamesettings_float_widget("Medium to Far Cascade: Far Distance", gs->nocull_dist_far_static, 0.0f, FLT_MAX, 0.5f);
		gamesettings_float_widget("Medium to Far Cascade: Min. Object Radius", gs->nocull_radius_far_static, 0.0f, FLT_MAX, 0.5f);
		gamesettings_float_widget("Medium to Far Cascade: Min. Object Height", gs->nocull_height_far_static, 0.0f, FLT_MAX, 0.5f);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Vehicle Dirt ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Enable Vehicle Dirt", gs->vehicle_dirt_enabled);
		ImGui::BeginDisabled(!gs->vehicle_dirt_enabled.get_as<bool>());
		{
			gamesettings_bool_widget("Enable Vehicle Dirt Color Override", gs->vehicle_dirt_custom_color_enabled);

			{
				auto gs_var_ptr = gs->vehicle_dirt_custom_color.get_as<float*>();
				if (ImGui::ColorEdit3("Vehicle Dirt Color", gs_var_ptr, ImGuiColorEditFlags_Float)) 
				{
					gs_var_ptr[0] = std::clamp(gs_var_ptr[0], 0.0f, 1.0f);
					gs_var_ptr[1] = std::clamp(gs_var_ptr[1], 0.0f, 1.0f);
					gs_var_ptr[2] = std::clamp(gs_var_ptr[2], 0.0f, 1.0f);
				}
				TT(gs->vehicle_dirt_custom_color.get_tooltip_string().c_str());
			}

			ImGui::EndDisabled();
		}


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Interioir Dirt ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Decal Dirt Shader Usage", gs->decal_dirt_shader_usage);
		ImGui::BeginDisabled(!gs->vehicle_dirt_enabled.get_as<bool>());
		{
			gamesettings_float_widget("Dirt Decal Shader Scalar", gs->decal_dirt_shader_scalar, 0.0f, 8.0f);
			gamesettings_float_widget("Dirt Decal Shader Contrast", gs->decal_dirt_shader_contrast, 0.0f, 8.0f);
			ImGui::EndDisabled();
		}


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Effects ");
		ImGui::Spacing(0, 4);

		gamesettings_float_widget("GTA_RMPTFX_Litsprite Alpha Scalar", gs->gta_rmptfx_litsprite_alpha_scalar, 0.0f, 20.0f);
	}

	void gamesettings_light_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();

		const float inbetween_spacing = 8.0f;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Sphere/Spot ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Translate Game Lights", gs->translate_game_lights);
		gamesettings_bool_widget("Ignore Filler Lights", gs->translate_game_lights_ignore_filler_lights);

		gamesettings_float_widget("Light Radius Scalar", gs->translate_game_light_radius_scalar, 0.0f, 0.0f, 0.005f);
		gamesettings_float_widget("Light Intensity Scalar", gs->translate_game_light_intensity_scalar, 0.0f, 0.0f, 0.005f);

		gamesettings_float_widget("Light Softness Offset", gs->translate_game_light_softness_offset, -1.0f, 1.0f, 0.005f);
		gamesettings_float_widget("SpotLight Volumetric Scale", gs->translate_game_light_spotlight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f);
		gamesettings_float_widget("SphereLight Volumetric Scale", gs->translate_game_light_spherelight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f);
		gamesettings_float_widget("Light Angle Offset", gs->translate_game_light_angle_offset, -180.0f, 180.0f, 0.1f);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Distant ");
		ImGui::Spacing(0, 4);

		gamesettings_float_widget("SunLight Intensity Scalar", gs->translate_sunlight_intensity_scalar, 0.0f, 0.0f, 0.005f);
		gamesettings_float_widget("SunLight Angular Diameter Degrees", gs->translate_sunlight_angular_diameter_degrees, 0.0f, 45.0f, 0.005f);
		gamesettings_float_widget("SunLight Volumetric Base", gs->translate_sunlight_volumetric_radiance_base, 0.0f, 10.0f, 0.005f);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::Separator();

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Anti Culling of Lights ");
		ImGui::Spacing(0, 4);

		gamesettings_float_widget("No Culling Until Distance", gs->nocull_dist_lights, 0.0f, 500.0f, 0.5f);
	}

	void gamesettings_emissive_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();

		const float inbetween_spacing = 8.0f;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Vehicle ");
		ImGui::Spacing(0, 4);

		gamesettings_float_widget("Vehicle Light Emissive Scalar", gs->vehicle_lights_emissive_scalar, 0.0f, 0.0f, 0.005f);

		gamesettings_bool_widget("Render Surfs a Second Time with Proxy Texture", gs->vehicle_lights_dual_render_proxy_texture);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" World ");
		ImGui::Spacing(0, 8);

		gamesettings_bool_widget("Render Emissive Surfaces using Shaders", gs->render_emissive_surfaces_using_shaders);
		gamesettings_bool_widget("Assign Decal Texture Category to Emissive Surfaces", gs->assign_decal_category_to_emissive_surfaces);

		gamesettings_float_widget("EmissiveNight Surfaces Scalar", gs->emissive_night_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f);
		gamesettings_float_widget("Emissive Surfaces Scalar", gs->emissive_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f);
		gamesettings_float_widget("EmissiveStrong Surfaces Scalar", gs->emissive_strong_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f);
		gamesettings_float_widget("Generic Emissive Scale", gs->emissive_generic_scale, 0.0f, 1000.0f, 0.001f);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Phone ");
		ImGui::Spacing(0, 4);

		gamesettings_bool_widget("Phone Emissive Override", gs->phone_emissive_override);
		ImGui::BeginDisabled(!gs->phone_emissive_override.get_as<bool>());
		{
			gamesettings_float_widget("Phone Emissive Scalar", gs->phone_emissive_scalar, 0.0f, 20.0f);
			ImGui::EndDisabled();
		}
	}

	void gamesettings_timecycle_container()
	{
		static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();

		const float inbetween_spacing = 8.0f;

		ImGui::Spacing(0, 4);
		ImGui::Indent(4);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::TextUnformatted("Note:");
		ImGui::PopFont();
		ImGui::TextWrapped(
			"Remix' Tonemapper has to be set to 'global' for some of these settings to work.\n"
			"You can make global adjustments using 'Tuning Mode'");
		ImGui::Unindent(4);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Fog ");
		ImGui::Spacing(0, 4);

		{
			gamesettings_bool_widget("Enable FogColor Logic", gs->timecycle_fogcolor_enabled);
			ImGui::BeginDisabled(!gs->timecycle_fogcolor_enabled.get_as<bool>());
			{
				gamesettings_float_widget("FogColor Base Strength", gs->timecycle_fogcolor_base_strength, 0.0f, 0.0f, 0.005f);
				gamesettings_float_widget("FogColor Influence Scalar", gs->timecycle_fogcolor_influence_scalar, 0.0f, 0.0f, 0.005f);

				ImGui::TextDisabled("Timecycle mSkyBottomColorFogDensity: [ %.2f, %.2f, %.2f, (Density) %.2f ]",
					im->m_timecyc_curr_mSkyBottomColorFogDensity.x,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.y,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.z,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.w);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxVolumetricsSingleScatteringAlbedo: [ %.2f, %.2f, %.2f ]",
					im->m_timecyc_curr_singleScatteringAlbedo.x,
					im->m_timecyc_curr_singleScatteringAlbedo.y,
					im->m_timecyc_curr_singleScatteringAlbedo.z);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);

		{
			gamesettings_bool_widget("Enable FogDensity Logic", gs->timecycle_fogdensity_enabled);
			ImGui::BeginDisabled(!gs->timecycle_fogdensity_enabled.get_as<bool>());
			{
				gamesettings_float_widget("FogDensity Influence Scalar", gs->timecycle_fogdensity_influence_scalar, 0.0f, 0.0f, 0.005f);

				ImGui::TextDisabled("Timecycle mSkyBottomColorFogDensity: [ %.2f, %.2f, %.2f, (Density) %.2f ]",
					im->m_timecyc_curr_mSkyBottomColorFogDensity.x,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.y,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.z,
					im->m_timecyc_curr_mSkyBottomColorFogDensity.w);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxVolumetricsTransmittanceMeasurementDistanceMeters: [ %.2f ]",
					im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);

		{
			gamesettings_bool_widget("Enable SkyHorizonHeight Logic", gs->timecycle_skyhorizonheight_enabled);
			ImGui::BeginDisabled(!gs->timecycle_skyhorizonheight_enabled.get_as<bool>());
			{
				gamesettings_float_widget("SkyHorizonHeight Scalar", gs->timecycle_skyhorizonheight_scalar, 0.0f, 0.0f, 0.005f);
				gamesettings_float_widget("SkyHorizonHeight Low - Transmittance Offset", gs->timecycle_skyhorizonheight_low_transmittance_offset, 0.0f, 0.0f, 0.01f);
				gamesettings_float_widget("SkyHorizonHeight High - Transmittance Offset", gs->timecycle_skyhorizonheight_high_transmittance_offset, 0.0f, 0.0f, 0.01f);

				ImGui::TextDisabled("Timecycle mSkyHorizonHeight: [ %.2f ]",
					im->m_timecyc_curr_mSkyHorizonHeight);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxVolumetricsAtmosphereHeightMeters: [ %.2f ]",
					im->m_timecyc_curr_mSkyHorizonHeight_final);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Sky/Sun ");
		ImGui::Spacing(0, 4);

		{
			gamesettings_bool_widget("Enable SkyLight Logic", gs->timecycle_skylight_enabled);
			ImGui::BeginDisabled(!gs->timecycle_skylight_enabled.get_as<bool>());
			{
				gamesettings_float_widget("SkyLight Scalar", gs->timecycle_skylight_scalar, 0.0f, 0.0f, 0.005f);

				ImGui::TextDisabled("Timecycle mSkyLightMultiplier: [ %.2f ]",
					im->m_timecyc_curr_mSkyLightMultiplier);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxSkyBrightness: [ %.2f ]",
					im->m_timecyc_curr_mSkyLightMultiplier_final);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}


			ImGui::Spacing(0, inbetween_spacing);


			gamesettings_bool_widget("Enable Fogdensity Influence on Volumetric Scale", gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled);
			ImGui::BeginDisabled(!gs->timecycle_skylight_enabled.get_as<bool>());
			{
				gamesettings_float_widget("Fogdensity Volumetric Influence Scalar", gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar, 0.0f, 0.0f, 0.005f);
				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Color Correction ");
		ImGui::Spacing(0, 4);

		{
			gamesettings_bool_widget("Enable ColorCorrection Logic", gs->timecycle_colorcorrection_enabled);
			ImGui::BeginDisabled(!gs->timecycle_colorcorrection_enabled.get_as<bool>());
			{
				gamesettings_bool_widget("Enable ColorTemperature Logic", gs->timecycle_colortemp_enabled);

				ImGui::TextDisabled("Timecycle mColorCorrection: [ %.2f, %.2f, %.2f ]",
					im->m_timecyc_curr_mColorCorrection.x,
					im->m_timecyc_curr_mColorCorrection.y,
					im->m_timecyc_curr_mColorCorrection.z);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxTonemapColorBalance: [ %.2f, %.2f, %.2f ]",
					im->m_timecyc_curr_mColorCorrection_final.x,
					im->m_timecyc_curr_mColorCorrection_final.y,
					im->m_timecyc_curr_mColorCorrection_final.z);
				ImGui::PopFont();

				ImGui::BeginDisabled(!gs->timecycle_colorcorrection_enabled.get_as<bool>());
				{
					gamesettings_float_widget("ColorTemperature Influence", gs->timecycle_colortemp_influence, 0.0f, 0.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mTemperature: [ %.2f ]",
						im->m_timecyc_curr_mTemperature);
					ImGui::TextDisabled("ColorTemp Offset applied to rtxTonemapColorBalance: [ %.2f, %.2f, %.2f ]",
						im->m_timecyc_curr_mTemperature_offset.x,
						im->m_timecyc_curr_mTemperature_offset.y,
						im->m_timecyc_curr_mTemperature_offset.z);

					ImGui::EndDisabled();
				}

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);

		{
			gamesettings_bool_widget("Enable Desaturation Logic", gs->timecycle_desaturation_enabled);
			ImGui::BeginDisabled(!gs->timecycle_desaturation_enabled.get_as<bool>());
			{
				gamesettings_float_widget("Desaturation Influence", gs->timecycle_desaturation_influence, 0.0f, 0.0f, 0.005f);
				gamesettings_float_widget("Far Desaturation Influence", gs->timecycle_fardesaturation_influence, 0.0f, 0.0f, 0.005f);

				ImGui::TextDisabled("Timecycle mDesaturation: [ %.2f ]",
					im->m_timecyc_curr_mDesaturation);

				ImGui::TextDisabled("Timecycle mDesaturationFar: [ %.2f ]",
					im->m_timecyc_curr_mDesaturationFar);

				ImGui::TextDisabled("mDesaturationFar influence on rtxTonemapSaturation: [ %.2f ]",
					im->m_timecyc_curr_mDesaturationFar_offset);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxTonemapSaturation: [ %.2f ]",
					im->m_timecyc_curr_mDesaturation_final);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);

		{
			gamesettings_bool_widget("Enable Gamma Logic", gs->timecycle_gamma_enabled);
			ImGui::BeginDisabled(!gs->timecycle_gamma_enabled.get_as<bool>());
			{
				gamesettings_float_widget("Gamma Offset", gs->timecycle_gamma_offset, 0.0f, 0.0f, 0.005f);

				ImGui::TextDisabled("Timecycle mGamma: [ %.2f ]",
					im->m_timecyc_curr_mGamma);

				ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::TextDisabled("Out rtxTonemapExposureBias: [ %.2f ]",
					im->m_timecyc_curr_mGamma_final);
				ImGui::PopFont();

				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Bloom ");
		ImGui::Spacing(0, 4);

		{
			gamesettings_bool_widget("Enable Bloom Logic", gs->timecycle_bloom_enabled);
			ImGui::BeginDisabled(!gs->timecycle_bloom_enabled.get_as<bool>());
			{
				gamesettings_float_widget("Bloom Intensity Scalar", gs->timecycle_bloomintensity_scalar, 0.0f, 0.0f, 0.005f);
				gamesettings_float_widget("Bloom Threshold Scalar", gs->timecycle_bloomthreshold_scalar, 0.0f, 0.0f, 0.005f);
				ImGui::EndDisabled();
			}
		}

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Weather ");
		ImGui::Spacing(0, 4);

		{
			gamesettings_bool_widget("Enable Weather Wetness Logic", gs->timecycle_wetness_enabled);
			ImGui::BeginDisabled(!gs->timecycle_wetness_enabled.get_as<bool>());
			{
				gamesettings_float_widget("Weather Wetness Scalar", gs->timecycle_wetness_scalar, 0.0f, 0.0f, 0.005f);
				gamesettings_float_widget("Additional Wetness Offset", gs->timecycle_wetness_offset, 0.0f, 0.0f, 0.005f);
				ImGui::EndDisabled();
			}
		}
	}

	void gamesettings_other_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();
		const float inbetween_spacing = 8.0f;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Remix ");
		ImGui::Spacing(0, 4);

		ImGui::DragInt("RTXDI Initial Sample Count Override", gs->remix_override_rtxdi_samplecount.get_as<int*>(), 0.01f);
		TT(gs->remix_override_rtxdi_samplecount.get_tooltip_string().c_str());

		ImGui::Spacing(0, inbetween_spacing);
		/*ImGui::SeparatorText(" Huh ");
		ImGui::Spacing(0, 8);*/
	}

	void imgui::tab_gamesettings()
	{
		const auto& im = imgui::get();
		//const auto gs = game_settings::get();

		// quick commands
		{
			static float cont_quickcmd_height = 0.0f;
			cont_quickcmd_height = ImGui::Widget_ContainerWithCollapsingTitle("Quick Commands", cont_quickcmd_height, cont_gamesettings_quick_cmd,
				true, ICON_FA_TERMINAL, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// rendering related
		{
			static float cont_gs_renderer_height = 0.0f;
			cont_gs_renderer_height = ImGui::Widget_ContainerWithCollapsingTitle("Rendering Related Settings", cont_gs_renderer_height, 
				gamesettings_rendering_container, false, ICON_FA_CAMERA, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// light related
		{
			static float cont_gs_light_height = 0.0f;
			cont_gs_light_height = ImGui::Widget_ContainerWithCollapsingTitle("Light Related Settings", cont_gs_light_height, 
				gamesettings_light_container, false, ICON_FA_LIGHTBULB, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// emissive related
		{
			static float cont_gs_emissive_height = 0.0f;
			cont_gs_emissive_height = ImGui::Widget_ContainerWithCollapsingTitle("Emissive Related Settings", cont_gs_emissive_height, 
				gamesettings_emissive_container, false, ICON_FA_RSS, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// timecycle related
		{
			static float cont_gs_timecycle_height = 0.0f;
			cont_gs_timecycle_height = ImGui::Widget_ContainerWithCollapsingTitle("Timecycle Related Settings", cont_gs_timecycle_height, 
				gamesettings_timecycle_container, false, ICON_FA_CLOCK, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// other
		{
			static float cont_gs_other_height = 0.0f;
			cont_gs_other_height = ImGui::Widget_ContainerWithCollapsingTitle("Other Settings", cont_gs_other_height,
				gamesettings_other_container, false, ICON_FA_RANDOM, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
	}

	bool reload_mapsettings_popup()
	{
		bool result = false;
		if (ImGui::BeginPopupModal("Reload MapSettings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			shared::imgui::draw_background_blur();
			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			auto line1_str = "You'll loose all unsaved changes if you continue!";
			auto line2_str = "Use the copy to clipboard buttons and manually update  ";
			auto line3_str = "the map_settings.toml file if you've made changes.";

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
			ImGui::TextUnformatted(line1_str);

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line2_str).x * 0.5f));
			ImGui::TextUnformatted(line2_str);
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
			ImGui::TextUnformatted(line3_str);

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0); ImGui::SameLine();

			ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				result = true;
				map_settings::load_settings();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6);
			if (ImGui::Button("Cancel", button_size)) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		return result;
	}

	bool reload_mapsettings_button_with_popup(const char* ID)
	{
		ImGui::PushFont(shared::imgui::font::BOLD);
		if (ImGui::Button(shared::utils::va("Reload MapSettings  %s##%s", ICON_FA_REDO, ID), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			if (!ImGui::IsPopupOpen("Reload MapSettings?")) {
				ImGui::OpenPopup("Reload MapSettings?");
			}
		}
		ImGui::PopFont();

		return reload_mapsettings_popup();
	}

	void cont_mapsettings_general()
	{
		//auto& ms = map_settings::get_map_settings();

		ImGui::PushFont(shared::imgui::font::BOLD);
		if (ImGui::Button("Reload rtx.conf    " ICON_FA_REDO, ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
		{
			if (!ImGui::IsPopupOpen("Reload RtxConf?")) {
				ImGui::OpenPopup("Reload RtxConf?");
			}
		} ImGui::PopFont();

		// popup
		if (ImGui::BeginPopupModal("Reload RtxConf?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			shared::imgui::draw_background_blur();
			ImGui::Spacing(0.0f, 0.0f);

			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			auto line1_str = "This will reload the rtx.conf file and re-apply all of it's variables.  ";
			auto line3_str = "(excluding texture hashes)";

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
			ImGui::TextUnformatted(line1_str);

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
			ImGui::TextUnformatted(line3_str);
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0); ImGui::SameLine();

			ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				remix_vars::xo_vars_parse_options_fn();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size)) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		reload_mapsettings_button_with_popup("General");

		//const auto two_row_button_size = ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 1) / 2.0f, 0);
		//ImGui::SeparatorTextLarge(" Debug Views / Info ", true);
	}

	void cont_mapsettings_marker_manipulation()
	{
		auto& markers = map_settings::get_map_settings().map_markers;
		ImGui::PushFont(shared::imgui::font::BOLD);
		if (ImGui::Button("Copy All Markers to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
		{
			ImGui::LogToClipboard();
			ImGui::LogText("%s", shared::common::toml_ext::build_map_marker_array(markers).c_str());
			ImGui::LogFinish();
		} ImGui::PopFont();

		ImGui::SameLine();
		reload_mapsettings_button_with_popup("MapMarker");
		//ImGui::Spacing(0, 4);

		constexpr auto in_buflen = 1024u;
		static char in_area_buf[in_buflen], in_nleaf_buf[in_buflen];
		static map_settings::marker_settings_s* selection = nullptr;

		//
		// MARKER TABLE

		ImGui::TableHeaderDropshadow();
		if (ImGui::BeginTable("MarkerTable", 9,
			ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody |
			ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_ScrollY, ImVec2(0, 380)))
		{
			ImGui::TableSetupScrollFreeze(0, 1); // make top row always visible
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoHide, 12.0f);
			ImGui::TableSetupColumn("Num", ImGuiTableColumnFlags_NoResize, 24.0f);
			ImGui::TableSetupColumn("Vis", ImGuiTableColumnFlags_NoResize, 24.0f);
			ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthStretch, 200.0f);
			ImGui::TableSetupColumn("Pos", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 200.0f);
			ImGui::TableSetupColumn("Rot", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 180.0f);
			ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 130.0f);
			ImGui::TableSetupColumn("CullDist", ImGuiTableColumnFlags_NoResize, 60.0f);
			ImGui::TableSetupColumn("##Delete", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoClip, 16.0f);
			ImGui::TableHeadersRow();

			bool selection_matches_any_entry = false;
			map_settings::marker_settings_s* marked_for_deletion = nullptr;

			for (auto i = 0u; i < markers.size(); i++)
			{
				auto& m = markers[i];

				// default selection
				if (!selection) {
					selection = &m;
				}

				ImGui::TableNextRow();

				// save Y offset
				const auto save_row_min_y_pos = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().FramePadding.y + ImGui::GetStyle().CellPadding.y;

				// handle row background color for selected entry
				const bool is_selected = selection && selection == &m;
				if (is_selected) {
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
				}

				// -
				ImGui::TableNextColumn();
				if (!is_selected) // only selectable if not selected
				{
					ImGui::Style_InvisibleSelectorPush(); // never show selection - we use tablebg
					if (ImGui::Selectable(shared::utils::va("%d", i), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 22 + ImGui::GetStyle().CellPadding.y * 1.0f))) {
						selection = &m;
					}
					ImGui::Style_InvisibleSelectorPop();

					if (ImGui::IsItemHovered()) {
						ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.6f)));///*ImGui::GetColorU32(ImGuiCol_TableRowBgAlt)*/);
					}
				}
				else {
					ImGui::Text("%d", i); // if selected
				}

				if (selection && selection == &m) {
					selection_matches_any_entry = true; // check that the selection ptr is up to date
				}

				// - marker num
				ImGui::TableNextColumn();
				ImGui::Text("%d", m.index);

				// - is visible
				ImGui::TableNextColumn();
				ImGui::Text(m.internal__is_hidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE);

				// - comment
				ImGui::TableNextColumn();
				ImGui::TextWrapped(m.comment.c_str());

				const auto row_max_y_pos = ImGui::GetItemRectMax().y;

				// - pos
				ImGui::TableNextColumn(); ImGui::Spacing();
				ImGui::Text("%.2f, %.2f, %.2f", m.origin.x, m.origin.y, m.origin.z);

				// - rot
				ImGui::TableNextColumn(); ImGui::Spacing();
				ImGui::Text("%.2f, %.2f, %.2f", m.rotation.x, m.rotation.y, m.rotation.z);

				// - scale
				ImGui::TableNextColumn(); ImGui::Spacing();
				ImGui::Text("%.2f, %.2f, %.2f", m.scale.x, m.scale.y, m.scale.z);

				// - scale
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", m.cull_distance);

				// delete Button
				ImGui::TableNextColumn();
				{
					ImGui::Style_DeleteButtonPush();
					ImGui::PushID((int)i);

					const auto btn_size = ImVec2(16, is_selected ? (row_max_y_pos - save_row_min_y_pos) : 25.0f);
					if (ImGui::Button("x##Marker", btn_size)) {
						marked_for_deletion = &m;
					}

					ImGui::Style_DeleteButtonPop();
					ImGui::PopID();
				}

			} // end for loop

			if (!selection_matches_any_entry)
			{
				for (auto& m : markers)
				{
					if (selection && selection == &m)
					{
						selection_matches_any_entry = true;
						break;
					}
				}

				if (!selection_matches_any_entry) {
					selection = nullptr;
				}
			}
			else if (selection) 
			{
				ImVec2 viewport_pos = {};
				shared::imgui::world_to_screen(selection->origin, viewport_pos);

				ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
				ImGui::GetBackgroundDrawList()->AddText(viewport_pos, ImGui::GetColorU32(ImGuiCol_Text), "[ImGui] Selected Marker");
				ImGui::PopFont();
			}

			// remove entry
			if (marked_for_deletion)
			{
				for (auto it = markers.begin(); it != markers.end(); ++it)
				{
					if (&*it == marked_for_deletion)
					{
						markers.erase(it);
						selection = nullptr;
						break;
					}
				}
			}
			ImGui::EndTable();
		}

		ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonGreen, true);
		if (ImGui::Button("++ Marker"))
		{
			std::uint32_t free_marker = 0u;
			for (auto i = 0u; i < markers.size(); i++)
			{
				if (markers[i].index == free_marker)
				{
					free_marker++;
					i = 0u; // restart loop
				}
			}

			Vector player_pos;
			player_pos = game::FindPlayerCentreOfWorld(&player_pos);

			markers.emplace_back(map_settings::marker_settings_s {
					free_marker, player_pos - Vector(0, 0, 0.5f)
				});

			selection = &markers.back();
		}
		ImGui::Style_ColorButtonPop();

		if (selection)
		{
			ImGui::SameLine();
			ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonYellow, true);
			if (ImGui::Button("Duplicate Current Marker"))
			{
				markers.emplace_back(map_settings::marker_settings_s{
					.index = selection->index,
					.origin = selection->origin,
					.rotation = selection->rotation,
					.scale = selection->scale,
					.cull_distance = selection->cull_distance,
					.comment = selection->comment,
					});

				selection = &markers.back();
			}
			ImGui::Style_ColorButtonPop();
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!selection);
		{
			/*if (ImGui::Button("TP to Marker")) {
				interfaces::get()->m_engine->execute_client_cmd_unrestricted(shared::utils::va("sv_cheats 1; noclip; setpos %.2f %.2f %.2f", selection->origin.x, selection->origin.y, selection->origin.z - 40.0f));
			}

			ImGui::SameLine();*/

			Vector player_pos;
			player_pos = game::FindPlayerCentreOfWorld(&player_pos);

			if (ImGui::Button("TP Marker to Player", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				selection->origin = player_pos;
				selection->origin.z -= 0.5f;
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::SeparatorText("Modify Marker");

		ImGui::Spacing();
		ImGui::Spacing();

		if (selection)
		{
			int temp_num = (int)selection->index;

			SET_CHILD_WIDGET_WIDTH;
			if (ImGui::DragInt("Number", &temp_num, 0.1f, 0, 50000, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				if (temp_num < 0) {
					temp_num = 0;
				}
				
				selection->index = (std::uint32_t)temp_num;
			}

			//ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
			ImGui::Widget_PrettyDragVec3("Origin", &selection->origin.x, true, 80.0f, 0.5f,
				-FLT_MAX, FLT_MAX, "X", "Y", "Z");
			//ImGui::PopStyleVar();

			// RAD2DEG -> DEG2RAD 
			Vector temp_rot = { RAD2DEG(selection->rotation.x), RAD2DEG(selection->rotation.y), RAD2DEG(selection->rotation.z) };

			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
			if (ImGui::Widget_PrettyDragVec3("Rotation", &temp_rot.x, true, 80.0f, 0.1f,
				-360.0f, 360.0f, "Rx", "Ry", "Rz"))
			{
				selection->rotation = { DEG2RAD(temp_rot.x), DEG2RAD(temp_rot.y), DEG2RAD(temp_rot.z) };
			} ImGui::PopStyleVar();

			{
				ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
				ImGui::Widget_PrettyDragVec3("Scale", &selection->scale.x, true, 80.0f, 0.01f,
					-FLT_MAX, FLT_MAX, "Sx", "Sy", "Sz");
				ImGui::PopStyleVar();
			}

			SET_CHILD_WIDGET_WIDTH;
			ImGui::DragFloat("Cull Distance", &selection->cull_distance, 0.05f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);

			SET_CHILD_WIDGET_WIDTH;
			ImGui::InputText("Comment", &selection->comment);

		} // selection

		ImGui::Spacing();
	}

	void cont_mapsettings_ignore_allow_lights()
	{
		static const auto& im = imgui::get();
		static const auto& gs = game_settings::get();

		auto& ignored_lights = map_settings::get_map_settings().ignored_lights;
		auto& allowed_lights = map_settings::get_map_settings().allow_lights;

		ImGui::PushFont(shared::imgui::font::BOLD);
		if (ImGui::Button("Copy Ignored to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
		{
			ImGui::LogToClipboard();
			ImGui::LogText("%s", shared::common::toml_ext::build_ignore_lights_array(ignored_lights).c_str());
			ImGui::LogFinish();
		} ImGui::PopFont();

		ImGui::SameLine();

		ImGui::BeginDisabled(!gs->translate_game_lights_ignore_filler_lights.get_as<bool>());
		{
			ImGui::PushFont(shared::imgui::font::BOLD);
			if (ImGui::Button("Copy Allowed to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				ImGui::LogToClipboard();
				ImGui::LogText("%s", shared::common::toml_ext::build_allow_lights_array(allowed_lights).c_str());
				ImGui::LogFinish();
			} ImGui::PopFont();

			ImGui::EndDisabled();
		}

		reload_mapsettings_button_with_popup("Ignore-Allow-Lights");

		ImGui::Spacing(0, 8.0f);
		ImGui::Checkbox("Visualize Api Light Hashes", &im->m_dbg_visualize_api_light_hashes); TT("Visualize all spawned api light hashes closeby");

		ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f, 0);
		ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes);
		{
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("  Draw Distance      ").x);
			ImGui::DragFloat("  Draw Distance      ", &im->m_dbg_visualize_api_light_hashes_distance, 0.005f, 0.0f, 30.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::EndDisabled();
		}

		ImGui::Checkbox("Ignore Filler Lights (Game Setting)", gs->translate_game_lights_ignore_filler_lights.get_as<bool*>());
			TT(gs->translate_game_lights_ignore_filler_lights.get_tooltip_string().c_str());

		ImGui::Spacing(0, 8.0f);
		if (!im->m_dbg_visualize_api_light_hashes) {
			ImGui::SeparatorText("Nearby lights (Double-Click to Ignore) ~ Enable 'Visualize Api Light Hashes' to enable this feature.");
		}
		else {
			ImGui::SeparatorText("Nearby lights (Double-Click to Ignore)");
		}
		
		ImGui::Spacing(0.0f, 8.0f);

		ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes);
		{
			{
				static ImGuiTextFilter ignore_filter;
				if (ImGui::BeginListBox("##ignore_lights", ImVec2(ImGui::GetContentRegionAvail().x, 140)))
				{
					for (size_t i = 0; i < im->visualized_api_lights.size(); ++i)
					{
						const auto& light = im->visualized_api_lights[i];
						const bool is_ignored = light.ignored;

						if (light.allowed_filler) {
							continue;
						}

						// only add lights that are alive for more than 5 frames
						if (light.m_frames_since_addition > 5u)
						{
							char hash_str[17];
							std::snprintf(hash_str, sizeof(hash_str), "%llx", static_cast<unsigned long long>(light.hash));
							if (!ignore_filter.PassFilter(hash_str)) {
								continue;
							}

							if (is_ignored)
							{
								ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
							}

							if (ImGui::Selectable(shared::utils::va("%llx", light.hash), false, ImGuiSelectableFlags_AllowDoubleClick))
							{
								if (ImGui::IsMouseDoubleClicked(0))
								{
									if (ignored_lights.contains(light.hash))
									{
										auto it = std::find(ignored_lights.begin(), ignored_lights.end(), light.hash);
										if (it != ignored_lights.end()) {
											ignored_lights.erase(it);
										}
									}
									else {
										ignored_lights.insert(light.hash);
									}
								}
							}

							if (is_ignored)
							{
								ImGui::PopStyleColor();
								ImGui::PopFont();
							}
						}
					}
					ImGui::EndListBox();
				}

				ignore_filter.Draw("##Filter", ImGui::GetContentRegionAvail().x
					- ImGui::GetFrameHeight()
					- ImGui::GetStyle().FramePadding.x + 3.0f);

				ImGui::SameLine();
				if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
					ignore_filter.Clear();
				}
			}
			

			ImGui::Spacing(0.0f, 12.0f);

			ImGui::PushID("AllowLights");

			ImGui::Spacing(0, 8.0f);
			if (!im->m_dbg_visualize_api_light_hashes) {
				ImGui::SeparatorText("Nearby filler lights (Double-Click to Ignore) ~ Enable 'Visualize Light Hashes'");
			}
			else {
				ImGui::SeparatorText("Nearby filler lights (Double-Click to Ignore)");
			}

			ImGui::Spacing(0.0f, 8.0f);

			if (gs->translate_game_lights_ignore_filler_lights.get_as<bool>())
			{
				static ImGuiTextFilter filter_allow_lights;
				if (ImGui::BeginListBox("##allow_lights", ImVec2(ImGui::GetContentRegionAvail().x, 140)))
				{
					for (size_t i = 0; i < im->visualized_api_lights.size(); ++i)
					{
						const auto& light = im->visualized_api_lights[i];
						//const bool is_ignored = light.ignored;
						const bool is_allowed = light.allowed_filler;

						// only add lights that are ignored (filler or manually) and alive for more than 5 frames
						if (/*is_ignored && */ light.is_filler && light.m_frames_since_addition > 5u)
						{
							char hash_str[17];
							std::snprintf(hash_str, sizeof(hash_str), "%llx", static_cast<unsigned long long>(light.hash));
							if (!filter_allow_lights.PassFilter(hash_str)) {
								continue;
							}

							if (is_allowed)
							{
								ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.1f, 1.0f));
							}

							if (ImGui::Selectable(shared::utils::va("%llx", light.hash), false, ImGuiSelectableFlags_AllowDoubleClick))
							{
								if (ImGui::IsMouseDoubleClicked(0))
								{
									if (allowed_lights.contains(light.hash))
									{
										auto it = std::find(allowed_lights.begin(), allowed_lights.end(), light.hash);
										if (it != allowed_lights.end()) {
											allowed_lights.erase(it);
										}
									}
									else {
										allowed_lights.insert(light.hash);
									}
								}
							}

							if (is_allowed)
							{
								ImGui::PopStyleColor();
								ImGui::PopFont();
							}
						}
					}
					ImGui::EndListBox();
				}

				filter_allow_lights.Draw("##Filter", ImGui::GetContentRegionAvail().x
					- ImGui::GetFrameHeight()
					- ImGui::GetStyle().FramePadding.x + 3.0f);

				ImGui::SameLine();
				if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
					filter_allow_lights.Clear();
				}
			}

			ImGui::PopID();
			ImGui::EndDisabled();
		}
	}

	void imgui::tab_map_settings()
	{
		// general settings
		{
			static float cont_general_height = 0.0f;
			cont_general_height = ImGui::Widget_ContainerWithCollapsingTitle("General Settings", cont_general_height, cont_mapsettings_general,
				false, ICON_FA_ELLIPSIS_H, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		ImGui::Spacing(0, 6.0f);
		ImGui::SeparatorText("The following settings do NOT auto-save.");
		ImGui::TextDisabled("Export to clipboard and override the settings manually!");
		ImGui::Spacing(0, 6.0f);

		// marker manipulation
		{
			static float cont_marker_manip_height = 0.0f;
			cont_marker_manip_height = ImGui::Widget_ContainerWithCollapsingTitle("Marker Manipulation", cont_marker_manip_height, cont_mapsettings_marker_manipulation,
				false, ICON_FA_DICE_D6, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// ignore/allow game lights
		{
			static float cont_ignore_lights_height = 0.0f;
			cont_ignore_lights_height = ImGui::Widget_ContainerWithCollapsingTitle("Ignore/Allow Game Lights", cont_ignore_lights_height, cont_mapsettings_ignore_allow_lights,
				true, ICON_FA_LIGHTBULB, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}
	}

	void imgui::draw_debug()
	{
		static auto im = imgui::get();
		
		if (m_dbg_visualize_api_light_hashes)
		{
			static auto rml = remix_lights::get();

			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				const float draw_dist = im->m_dbg_visualize_api_light_hashes_distance;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				ImGui::PushFont(shared::imgui::font::BOLD_LARGE);

				if (rml->get_active_light_count())
				{
					for (auto& l : *rml->get_active_lights())
					{
						if (fabs(cam_org.DistToSqr(l.second.m_def.mPosition) < draw_dist * draw_dist))
						{
							shared::imgui::world_to_screen(l.second.m_def.mPosition, viewport_pos);

							bool is_light_hash_stable = im->m_dbg_visualize_api_light_unstable_hashes; // false by default
							bool is_light_in_vis_list = false;

							if (!im->m_dbg_visualize_api_light_unstable_hashes)
							{
								for (auto& ign : visualized_api_lights)
								{
									if (ign.hash == l.second.m_hash) 
									{
										is_light_in_vis_list = true;
										ign.ignored = l.second.m_is_ignored; // ignored state might have changed
										ign.allowed_filler = l.second.m_is_allowed_filler; // ^
										ign.m_updateframe++;
										is_light_hash_stable = ign.m_frames_since_addition > 5u;
										break;
									}
								}
							}

							if (!is_light_in_vis_list)
							{
								visualized_api_lights.emplace_back(
									visualized_api_light_s {
										.hash = l.second.m_hash,
										.pos = l.second.m_def.mPosition,
										.ignored = l.second.m_is_ignored,
										.allowed_filler = l.second.m_is_allowed_filler,
										.is_filler = l.second.m_is_filler,
										.m_updateframe = 0u,
										.m_frames_since_addition = 0u
									}
								);
							}

							if (is_light_hash_stable)
							{
								if (l.second.m_is_ignored)
								{
									ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
										ImColor(1.0f, 0.0f, 0.0f, 1.0f), shared::utils::va("[LIGHT-HASH] %llx\n~ IGNORED ~", l.second.m_hash));
								}
								else if (l.second.m_is_allowed_filler)
								{
									ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
										ImColor(0.1f, 0.8f, 0.1f, 1.0f), shared::utils::va("[LIGHT-HASH] %llx\n~ ALLOWED FILLER ~", l.second.m_hash));
								}
								else
								{
									ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
										ImGui::GetColorU32(ImGuiCol_Text), shared::utils::va("[LIGHT-HASH] %llx", l.second.m_hash));
								}
							}
						}
					}
				}

				// clean vis list
				for (auto it = visualized_api_lights.begin(); it != visualized_api_lights.end(); )
				{
					auto& elem = *it;

					if (elem.m_frames_since_addition != elem.m_updateframe || fabs(cam_org.DistToSqr(elem.pos) > draw_dist * draw_dist)) {
						it = visualized_api_lights.erase(it);
					}
					else 
					{
						// used to detect unstable hashes
						elem.m_frames_since_addition++;
						++it;
					}
				}

				ImGui::PopFont();
			}
		}

		if (m_dbg_visualize_decal_renderstates)
		{
			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				const float draw_dist = 20.0f;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				for (auto& l : visualized_decal_renderstates)
				{
					if (fabs(cam_org.DistToSqr(l.pos) < draw_dist * draw_dist))
					{
						shared::imgui::world_to_screen(l.pos, viewport_pos);

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

				visualized_decal_renderstates.clear();
			}
		}
	}


	void imgui::devgui()
	{
		ImGui::SetNextWindowSize(ImVec2(900, 800), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Remix Compatibility-Mod Settings", &shared::globals::imgui_menu_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollWithMouse/*, &shared::imgui::draw_window_blur_callback*/))
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
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + 12.0f, 8));	\
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

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + 12.0f, 8));
		ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		if (ImGui::BeginTabBar("devgui_tabs"))
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);
			ADD_TAB("Game Settings", tab_gamesettings);
			ADD_TAB("Map Settings", tab_map_settings);
			ADD_TAB("Dev", tab_dev);
			ADD_TAB("About", tab_about);
			ImGui::EndTabBar();
		}
		else {
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);
		}
#undef ADD_TAB

		{
			ImGui::Separator();
			const char* movement_hint_str = "Hold Right Mouse to enable Game Input ";
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

					if (shared::globals::imgui_allow_input_bypass_timeout) {
						shared::globals::imgui_allow_input_bypass_timeout--;
					}

					if (shared::globals::imgui_menu_open) 
					{
						io.MouseDrawCursor = true;
						im->devgui();

						// ---
						// enable game input via right mouse button logic

						if (!im->m_im_window_hovered && io.MouseDown[1])
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

					im->draw_debug();

					if (im->m_stats.is_tracking_enabled()) {
						im->m_stats.reset_stats();
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
		colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.96f);
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
		colors[ImGuiCol_Header] = ImVec4(0.180f, 0.180f, 0.180f, 0.388f);
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
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.67f, 0.52f, 0.24f, 1.00f);
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
		ImGuiCol_ContainerBackground = ImVec4(0.17f, 0.17f, 0.17f, 0.7f);
		ImGuiCol_ContainerBorder = ImVec4(0.477f, 0.39f, 0.25f, 0.90f);
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

		// ---
		m_initialized = true;
		std::cout << "[IMGUI] loaded\n";
	}
}
