#include "std_include.hpp"
#include "imgui.hpp"

#include "comp_settings.hpp"
#include "imgui_internal.h"
#include "map_settings.hpp"
#include "natives.hpp"
#include "remix_lights.hpp"
#include "remix_vars.hpp"
#include "renderer.hpp"
#include "shared/common/flags.hpp"
#include "shared/common/remix_api.hpp"
#include "shared/common/toml_ext.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/font_awesome_solid_900.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/font_opensans.hpp"

// Allow us to directly call the ImGui WndProc function.
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

constexpr float TREENODE_SPACING = 6.0f;
constexpr float TREENODE_SPACING_INSIDE = 6.0f;
constexpr float SEPARATOR_SPACING = 12.0f;

#define SPACING_INDENT_BEGIN(SPACEY) ImGui::Spacing(0, SPACEY); ImGui::Indent()
#define SPACING_INDENT_END(SPACEY) ImGui::Spacing(0, SPACEY); ImGui::Unindent()
#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

#define SET_CHILD_WIDGET_WIDTH			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
#define SET_CHILD_WIDGET_WIDTH_MAN(V)	ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild((V)));

#define CENTER_URL(text, link)					\
	ImGui::SetCursorForCenteredText((text));	\
	ImGui::TextURL((text), (link), true);

#define CLEAR_CACHE_CHECK(B, FN) \
	(B) = (FN) ? true : (B);

#define ADD_CONTAINER_TAB(NAME, FUNC, TOOLTIP) \
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0)));			\
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + 12.0f, 8));	\
	if (ImGui::BeginTabItem(NAME)) {																		\
		ImGui::PopStyleVar(1); TT(TOOLTIP); SPACING_INDENT_BEGIN(8); FUNC(); SPACING_INDENT_END(8); ImGui::EndTabItem();	\
	}																										\
	else { ImGui::PopStyleVar(1); TT(TOOLTIP); } ImGui::PopStyleColor(); 

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

		if (shared::globals::imgui_menu_open || imgui::get()->m_freecam_mode)
		{
			//auto& io = ImGui::GetIO();
			ImGui_ImplWin32_WndProcHandler(shared::globals::main_window, message_type, wparam, lparam);
		} else {
			shared::globals::imgui_allow_input_bypass = false; // always reset if there is no imgui window open
		}

		return shared::globals::imgui_menu_open;
	}

	// ------

	// re-used in dev tab
	void im_logic_weather_clock_adjustment()
	{
		ImGui::Spacing(0, 4);
		ImGui::SeparatorText("    Adjust Game Clock     ");
		ImGui::Spacing(0, 2);

		const auto im = imgui::get();
		const auto n = natives::get();

		if (ImGui::Checkbox("Modify / Freeze Time", &im->m_freeze_time)) {
			*game::m_game_timer_length = im->m_freeze_time ? 9999999 : 2000;
		}

		ImGui::BeginDisabled(!im->m_freeze_time);
		{
			if (ImGui::InputInt("Hour", &im->m_curr_game_hour, 1, 2))
			{
				im->m_curr_game_hour = im->m_curr_game_hour > 23 ? 0 : im->m_curr_game_hour < 0 ? 23 : im->m_curr_game_hour;
				im->m_time_was_changed = true;
			}
			if (ImGui::InputInt("Minutes", &im->m_curr_game_minute, 1, 4))
			{
				im->m_curr_game_minute = im->m_curr_game_minute > 59 ? 0 : im->m_curr_game_minute < 0 ? 59 : im->m_curr_game_minute;
				im->m_time_was_changed = true;
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing(0, 12);
		ImGui::SeparatorText("    Adjust Weather     ");
		ImGui::Spacing(0, 2);

		auto set_weather = [n](uint32_t idx) 
			{
				n->ForceWeatherNow(idx);
				n->ReleaseWeather();
			};

		const auto button_4way_width = (ImGui::GetContentRegionAvail().x * 0.25f) - (ImGui::GetStyle().ItemSpacing.x);

		if (ImGui::Button("EXTRASUNNY", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_EXTRASUNNY);
		}

		ImGui::SameLine();
		if (ImGui::Button("SUNNY", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_SUNNY);
		}

		ImGui::SameLine();
		if (ImGui::Button("SUNNY_WINDY", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_SUNNY_WINDY);
		}

		ImGui::SameLine();
		if (ImGui::Button("CLOUDY", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_CLOUDY);
		}


		if (ImGui::Button("RAIN", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_RAIN);
		}

		ImGui::SameLine();
		if (ImGui::Button("DRIZZLE", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_DRIZZLE);
		}

		ImGui::SameLine();
		if (ImGui::Button("FOGGY", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_FOGGY);
		}

		ImGui::SameLine();
		if (ImGui::Button("LIGHTNING", ImVec2(button_4way_width, 0))) {
			set_weather(game::WEATHER_LIGHTNING);
		}

		ImGui::Spacing(0, 4);
		ImGui::Text("Current Weather Transition: %.2f", *game::weather_change_value);

		ImGui::Spacing(0.0f, 4.0f);
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

		const char* version_str = nullptr;

		if constexpr (COMP_MOD_PRE_RELEASE_NUM != 0) 
		{
			version_str = shared::utils::va("%d.%d.%d - Pre-Release %d :: %s",
				COMP_MOD_VERSION_MAJOR, COMP_MOD_VERSION_MINOR, COMP_MOD_VERSION_PATCH, COMP_MOD_PRE_RELEASE_NUM, __DATE__);
		} 
		else
		{
			version_str = shared::utils::va("%d.%d.%d :: %s",
				COMP_MOD_VERSION_MAJOR, COMP_MOD_VERSION_MINOR, COMP_MOD_VERSION_PATCH, __DATE__);
		}
		

		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText(version_str);

#if DEBUG
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.64f, 0.23f, 0.18f, 1.0f));
		ImGui::CenterText("DEBUG BUILD");
		ImGui::PopStyleColor();
#endif
		ImGui::PopFont();

		ImGui::Spacing(0.0f, 16.0f);
		CENTER_URL("GitHub Repository", "https://github.com/xoxor4d/gta4-rtx");
		CENTER_URL("GitHub Project Page", "https://xoxor4d.github.io/projects/gta4-rtx");
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
		CENTER_URL("Miniz", "https://github.com/richgel999/miniz");
		CENTER_URL("Rapidjson", "https://github.com/Tencent/rapidjson");
		CENTER_URL("DiscordRPC", "https://github.com/discord/discord-rpc");

		CENTER_URL("FusionFix", "https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix");
		CENTER_URL("FusionShaders", "https://github.com/Parallellines0451/GTAIV.EFLC.FusionShaders");
		CENTER_URL("Rage-Shader-Editor", "https://github.com/ImpossibleEchoes/rage-shader-editor-cpp");
		CENTER_URL("IV-SDK", "https://github.com/Zolika1351/iv-sdk/");
		CENTER_URL("IV-SDK-DotNet", "https://github.com/ClonkAndre/IV-SDK-DotNet");

		CENTER_URL("AssaultKifle47", "https://github.com/akifle47");
		CENTER_URL("DayL", "https://www.gtainside.de/de/user/falcogray");
		CENTER_URL("Entity", "https://www.youtube.com/@paprykszadolowski8796");
		CENTER_URL("Gabdeg", "https://www.youtube.com/@gabdeg793");
		CENTER_URL("Hemry", "https://www.youtube.com/@Hemry81");
		CENTER_URL("Thundery_Dan", "https://github.com/DANLOPAND");
		CENTER_URL("KapibosRU", "https://www.youtube.com/channel/UCqZ2NI_fQKRN-Onypt9aIGQ");
		CENTER_URL("Budgie", "https://www.patreon.com/c/BudgieGames");
		CENTER_URL("Alex from Digital Foundry", "https://www.youtube.com/watch?v=vGxPdcMQfwg");
		CENTER_URL("Sparkles (Remix Plus - Numos - DLSS5 Integration)", "https://github.com/Kim2091");
		CENTER_URL("CR (Remix Plus)", "https://github.com/sambow23");
		CENTER_URL("TheGreatHMMMM (Remix Plus)", "https://github.com/TheGreatHMMMM");
		CENTER_URL("Gokuwashere (Remix Plus)", "https://github.com/BrunchyChineapple");

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

			ImGui::Spacing(0.0f, 4.0f);
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


	void dev_debug_container()
	{
		const auto& im = imgui::get();
		//const auto& gs = comp_settings::get();

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
			ImGui::Checkbox("Extended Anticull Always TRUE", &im->m_dbg_extended_anticull_always_true); TT("Always return true when extended anticull is on.");

			ImGui::Checkbox("Disable HUD Hack", &im->m_dbg_disable_hud_fixup); TT("Disables hack that helps remix detect the first HUD elem");
			ImGui::Checkbox("Disable HUD Hack #2", &im->m_dbg_disable_hud_fixup2); TT("Disables 2nd hack that helps remix detect the first HUD elem (on drawing radar background)");

			ImGui::Checkbox("Disable Phone Hack", &im->m_dbg_disable_phone_fixup); TT("Disables phone hack that clears the RT before drawing the first elem on the phone screen. Fixes smearing.");

			ImGui::Checkbox("Disable IgnoreBackedLighting Enforcement", &im->m_dbg_disable_ignore_baked_lighting_enforcement);
			TT("CompMod forces the IgnoreBakedLighting category for almost every mesh. This disables that")

			ImGui::Checkbox("Disable Global UV Animations", &im->m_dbg_disable_global_uv_anims);
			ImGui::Checkbox("Disable OMM Override on Alphatested UV Anims", &im->m_dbg_disable_omm_override_on_alpha_uv_anims);
			TT("Disables automatic ignore OMM tagging on alphatested emissive surfaces with animated UVs");

			ImGui::Checkbox("Disable Water Worldpos Logic", &im->m_dbg_disable_water_worldpos_logic);

			//ImGui::Checkbox("Disable Alphablend On VEHGLASS", &im->m_dbg_vehglass_disable_alphablend);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Do not render / render only ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Do not render Static", &im->m_dbg_do_not_render_static);
			ImGui::Checkbox("Only render Static", &im->m_dbg_only_render_static);
			ImGui::Checkbox("Do not render Vehicle", &im->m_dbg_do_not_render_vehicle);
			ImGui::Checkbox("Do not render Instances", &im->m_dbg_do_not_render_instances);
			ImGui::Checkbox("Do not render Stencil 0", &im->m_dbg_do_not_render_stencil_zero);
			ImGui::Checkbox("Do not render Tree Foliage", &im->m_dbg_do_not_render_tree_foliage);
			ImGui::Checkbox("Do not render FX", &im->m_dbg_do_not_render_fx);
			ImGui::Checkbox("Do not render FF", &im->m_dbg_do_not_render_ff);
			ImGui::Checkbox("Do not render Prims with VS", &im->m_dbg_do_not_render_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Indexed Prims with VS", &im->m_dbg_do_not_render_indexed_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Water", &im->m_dbg_do_not_render_water);
			ImGui::Checkbox("Do not render Tri Surfaces", &im->m_dbg_do_not_render_tri_surface);
			ImGui::Checkbox("Do not render Map markers", &im->m_dbg_do_not_render_map_markers);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Light related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Api Lights 3D", &im->m_dbg_visualize_api_lights); TT("Visualize all spawned api lights");
			ImGui::DragFloat("Vis. 3D Light Distance", &im->m_dbg_visualize_api_lights_3d_distance, 0.01f, 1.0f, 50.0f, "%.0f");
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

			ImGui::Checkbox("Force Directional Light Translation", &im->m_dbg_force_distant_light_translation);
			TT("Force Translation even when Remix Atmosphere System is active.");

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

			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Fun");
			ImGui::Spacing(0, 4);

			ImGui::Checkbox("Enable Custom Vehicle Headlight Color", &im->m_dbg_custom_veh_headlight_enabled);
			ImGui::BeginDisabled(!im->m_dbg_custom_veh_headlight_enabled);
			{
				ImGui::ColorEdit3("Custom Vehicle Headlight Color", &im->m_dbg_custom_veh_headlight_color.x);
				ImGui::EndDisabled();
			}

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Emissive Related"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SliderInt("Tag EmissiveNight Surfaces as Category ..", &im->m_dbg_tag_static_emissive_as_index, -1, (int)InstanceCategories::Count, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Checkbox("FF Emissive: Enable Alphablend on non alpha Emissives", &im->m_dbg_emissive_ff_with_alphablend);
			ImGui::Checkbox("FF Emissive: Tag as WorldUI + IgnoreTransparencyLayer", &im->m_dbg_emissive_ff_worldui_ignore_alpha);

			ImGui::Checkbox("Render Emissives using Shaders", &im->m_dbg_render_emissives_with_shaders);
			ImGui::Checkbox("Tag Emissives drawn by Shaders as Decal/WorldUI", &im->m_dbg_render_emissives_with_shaders_tag_as_decal);

			ImGui::Checkbox("Do Not Render FF Emissives", &im->m_dbg_emissive_ff_do_not_render);
			ImGui::Checkbox("Do Not Render FF Alphablended Emissives", &im->m_dbg_emissive_ff_alphablend_do_not_render);

			ImGui::Checkbox("FF Alphablended Emissives Assign IgnoreTransparency", &im->m_dbg_emissive_ff_alphablend_test1);
			ImGui::Checkbox("FF Alphablended Emissives Alphablend", &im->m_dbg_emissive_ff_alphablend_enable_alphablend);

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Emissive Intensity Debug Switches");

			ImGui::Widget_CategoryWithVerticalLabel("Renderer", [&]() 
			{
				ImGui::PushID("intens");
				ImGui::Checkbox("Disable Veh SwitchOn", &im->m_dbg_emissive_disable_veh_switch_on);
				ImGui::Checkbox("Disable OnEmissiveMultiplier", &im->m_dbg_emissive_disable_on_emissive_multi);
				ImGui::Checkbox("Disable PsConst72 VehLightsEmissive", &im->m_dbg_emissive_disable_const77_lightsemissive);
				ImGui::Checkbox("Disable PsConst66 EmissiveNight", &im->m_dbg_emissive_disable_const66_emissivenight);
				ImGui::Checkbox("Disable PsConst66 Default", &im->m_dbg_emissive_disable_const66_default);
				ImGui::Checkbox("Disable PsConst66 EmissiveStrong", &im->m_dbg_emissive_disable_const66_emissivestrong);
				ImGui::Checkbox("Disable PsConst51 TFACTOR EmsMulti", &im->m_dbg_emissive_disable_const51_tfactor_ems_multi);
				ImGui::PopID();
			});

			ImGui::Spacing(0, SEPARATOR_SPACING);

			ImGui::Widget_CategoryWithVerticalLabel("Renderer FF", [&]()
				{
					ImGui::PushID("intensFF");
					ImGui::Checkbox("Disable FF EmissiveNight (Night)", &im->m_dbg_emissive_disable_ff_emissivenight_nighttime);
					ImGui::Checkbox("Disable FF EmissiveNight (Day 0)", &im->m_dbg_emissive_disable_ff_emissivenight_daytime);
					ImGui::Checkbox("Disable FF NOT EmissiveNight", &im->m_dbg_emissive_disable_ff_not_emissivenight);
					ImGui::Checkbox("Disable AlphaFF", &im->m_dbg_emissive_disable_alpha_ff);
					ImGui::PopID();
				});

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Visualizations / Rendering related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Decal Renderstates", &im->m_dbg_visualize_decal_renderstates); TT("Visualize renderstates of nearby decal surfaces.");
			ImGui::Checkbox("Visualize Stencil States", &im->m_dbg_visualize_stencil_state); TT("Visualize stencil states of nearby surfaces.");
			ImGui::DragFloat("Visualize Dist", &im->m_dbg_visualize_distance, 0.1f);

			if (static auto debug_model_hash = shared::common::flags::has_flag("debug_model_hash"); debug_model_hash)
			{
				ImGui::Spacing(0, 4);

				ImGui::Checkbox("Visualize Model Hashes", &im->m_dbg_visualize_model_hashes_info);
				ImGui::DragFloat("Visualize Model Hashes Dist", &im->m_dbg_visualize_model_hashes_distance, 0.1f);
				ImGui::DragFloat("Visualize Model Hashes Min Radius", &im->m_dbg_visualize_model_hashes_min_radius, 0.1f);
				ImGui::DragFloat("Visualize Model Hashes Max Radius", &im->m_dbg_visualize_model_hashes_max_radius, 0.1f);
			}

			ImGui::Spacing(0, 4);
			ImGui::SliderInt("Tag Exp Hair Surfaces as Category ..", &im->m_dbg_tag_exp_hair_as_index, -1, 23, "%d", ImGuiSliderFlags_AlwaysClamp);



			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Timecycle related ..."))
		{
			const auto bridge = shared::common::remix_api::get().m_bridge;

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::SeparatorText("Remix Atmos System");
			ImGui::TextUnformatted("Logic to adjust current Atmos Preset and Blending strength");
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::Checkbox("Atmos System - Manual Mode", &im->m_dbg_manual_atmos_system);

			ImGui::BeginDisabled(!im->m_dbg_manual_atmos_system);
			{
				/* clear, partlyCloudy, overcast, hazy, foggy, drizzle, rainstorm, thunderstorm, snow, blizzard,
				 * sandstorm, smoggy. Empty string to return to dormant. */

				if (ImGui::Button("clear")) {
					bridge.SetGameValue("__weather.target", "clear");
				} TT("EXTRASUNNY");

				ImGui::SameLine();
				if (ImGui::Button("smoggy")) {
					bridge.SetGameValue("__weather.target", "smoggy");
				} TT("SUNNY");

				ImGui::SameLine();
				if (ImGui::Button("partlyCloudy")) {
					bridge.SetGameValue("__weather.target", "partlyCloudy");
				} TT("SUNNY_WINDY");

				ImGui::SameLine();
				if (ImGui::Button("overcast")) {
					bridge.SetGameValue("__weather.target", "overcast");
				} TT("CLOUDY");

				if (ImGui::Button("drizzle")) {
					bridge.SetGameValue("__weather.target", "drizzle");
				}

				ImGui::SameLine();
				if (ImGui::Button("foggy")) {
					bridge.SetGameValue("__weather.target", "foggy");
				}

				ImGui::SameLine();
				if (ImGui::Button("rainstorm")) {
					bridge.SetGameValue("__weather.target", "rainstorm");
				} TT("RAIN");

				ImGui::SameLine();
				if (ImGui::Button("thunderstorm")) {
					bridge.SetGameValue("__weather.target", "thunderstorm");
				} TT("LIGHTNING");

				ImGui::SameLine();
				

				ImGui::Spacing(0, TREENODE_SPACING);
				ImGui::Separator();
				ImGui::Spacing(0, TREENODE_SPACING);

				static float trans_val = 0.0f;
				ImGui::SliderFloat("Absolute Val", &trans_val, 0.0f, 1.0f);

				if (ImGui::Button("Apply Absolute Value", ImVec2(ImGui::CalcItemWidth(), 0))) {
					bridge.SetGameValue("__weather.blend_absolute", std::to_string(trans_val).c_str());
				}

				if (ImGui::Button("Disable Absolute Mode", ImVec2(ImGui::CalcItemWidth(), 0))) 
				{
					bridge.SetGameValue("__weather.blend_absolute", "-1");
					trans_val = -1.0f;
				}

				ImGui::Spacing(0, TREENODE_SPACING);
				ImGui::Separator();
				ImGui::Spacing(0, TREENODE_SPACING);

				static float trans_blend_val = 0.0f;
				ImGui::SliderFloat("Transition Blend Val", &trans_blend_val, 0.0f, 10.0f);
				TT("Blend time in s");

				if (ImGui::Button("Apply Blend Value", ImVec2(ImGui::CalcItemWidth(), 0)))
				{
					bridge.SetGameValue("__weather.blend_absolute", "-1.0");
					bridge.SetGameValue("__weather.blend_seconds", std::to_string(trans_blend_val).c_str());
				}

				ImGui::Spacing(0, TREENODE_SPACING);
				ImGui::Separator();
				ImGui::Spacing(0, TREENODE_SPACING);

				static char str_buff[512] = {};
				static uint32_t str_len = 0u;

				bridge.GetGameValue("__weather.current", str_buff, ARRAYSIZE(str_buff), &str_len);
				const std::string weather_current = str_buff;

				bridge.GetGameValue("__weather.target", str_buff, ARRAYSIZE(str_buff), &str_len);
				const std::string weather_target = str_buff;

				bridge.GetGameValue("__weather.previous", str_buff, ARRAYSIZE(str_buff), &str_len);
				const std::string weather_previous = str_buff;

				bridge.GetGameValue("__weather.blend_absolute", str_buff, ARRAYSIZE(str_buff), &str_len);
				const std::string weather_blend_absolute = str_buff;

				bridge.GetGameValue("__weather.blend_progress", str_buff, ARRAYSIZE(str_buff), &str_len);
				const std::string weather_blend_progress = str_buff;

				ImGui::TextUnformatted(std::format("__weather.current: {}", weather_current).c_str());
				ImGui::TextUnformatted(std::format("__weather.target: {}", weather_target).c_str());
				ImGui::TextUnformatted(std::format("__weather.previous: {}", weather_previous).c_str());
				ImGui::TextUnformatted(std::format("__weather.blend_absolute: {}", weather_blend_absolute).c_str());
				ImGui::TextUnformatted(std::format("__weather.blend_progress: {}", weather_blend_progress).c_str());

				ImGui::EndDisabled();
			}

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Separator();
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::SliderInt("Used Timecycle for Remix Translation ..", &im->m_dbg_used_timecycle, -1, 2, "%d", ImGuiSliderFlags_AlwaysClamp);
			TT("Sets the Timecycle to be used to translate its settings to fitting remix variables.\n"
				"-1: No override\n0: Timecycle 1 (World/Interior)\n1: Timecycle 2 (World/Interior)\n3: Timecycle 3 (Cutscenes)");

			if (game::weather_type_prev)
			{
				switch (*game::weather_type_prev)
				{
				case game::WEATHER_EXTRASUNNY:
					ImGui::TextUnformatted("WeatherPrev: EXTRASUNNY"); break;
				case game::WEATHER_SUNNY:
					ImGui::TextUnformatted("WeatherPrev: SUNNY"); break;
				case game::WEATHER_SUNNY_WINDY:
					ImGui::TextUnformatted("WeatherPrev: SUNNY_WINDY"); break;
				case game::WEATHER_CLOUDY:
					ImGui::TextUnformatted("WeatherPrev: CLOUDY"); break;
				case game::WEATHER_RAIN:
					ImGui::TextUnformatted("WeatherPrev: RAIN"); break;
				case game::WEATHER_DRIZZLE:
					ImGui::TextUnformatted("WeatherPrev: DRIZZLE"); break;
				case game::WEATHER_FOGGY:
					ImGui::TextUnformatted("WeatherPrev: FOGGY"); break;
				case game::WEATHER_LIGHTNING:
					ImGui::TextUnformatted("WeatherPrev: LIGHTNING"); break;
				default:
					ImGui::TextUnformatted("WeatherPrev: UNKOWN"); break;
				}
			}

			if (game::weather_type_new)
			{
				switch (*game::weather_type_new)
				{
				case game::WEATHER_EXTRASUNNY:
					ImGui::TextUnformatted("WeatherNew: EXTRASUNNY"); break;
				case game::WEATHER_SUNNY:
					ImGui::TextUnformatted("WeatherNew: SUNNY"); break;
				case game::WEATHER_SUNNY_WINDY:
					ImGui::TextUnformatted("WeatherNew: SUNNY_WINDY"); break;
				case game::WEATHER_CLOUDY:
					ImGui::TextUnformatted("WeatherNew: CLOUDY"); break;
				case game::WEATHER_RAIN:
					ImGui::TextUnformatted("WeatherNew: RAIN"); break;
				case game::WEATHER_DRIZZLE:
					ImGui::TextUnformatted("WeatherNew: DRIZZLE"); break;
				case game::WEATHER_FOGGY:
					ImGui::TextUnformatted("WeatherNew: FOGGY"); break;
				case game::WEATHER_LIGHTNING:
					ImGui::TextUnformatted("WeatherNew: LIGHTNING"); break;
				default:
					ImGui::TextUnformatted("WeatherNew: UNKOWN"); break;
				}
			}

			ImGui::Text("Weather Transition: %.2f", *game::weather_change_value);
			ImGui::Text("Clock Hour: %d", *game::m_game_clock_hours);
			ImGui::Text("Clock Minutes: %d", *game::m_game_clock_minutes);

			auto frac = [](const float& x) {
					return x - std::floor(x);
				};

			im->m_plot_sun_lerp.add(frac(im->m_dbg_vis_sun_lerp_t));
			im->m_plot_sun_elevation.add(frac(im->m_dbg_vis_sun_elevation));
			im->m_plot_sun_rotation.add(frac(im->m_dbg_vis_sun_rotation));
			im->m_plot_sun_framecount.add(static_cast<float>(im->m_dbg_vis_sun_frame_count % 1000)); // count wrapped every 1000 frames

			ImGui::PlotLines(	"Atmos Sun Lerp T", im->m_plot_sun_lerp.values,
								imgui::debug_plot::history_size, im->m_plot_sun_lerp.offset, nullptr,
								0.0f, 1.0f, ImVec2(0, 60));

			ImGui::PlotLines(	"Atmos Sun Elevation", im->m_plot_sun_elevation.values,
								imgui::debug_plot::history_size, im->m_plot_sun_elevation.offset, nullptr, 
								0.0f, 1.0f, ImVec2(0, 60));

			ImGui::PlotLines(	"Atmos Sun Rotation", im->m_plot_sun_rotation.values,
								imgui::debug_plot::history_size, im->m_plot_sun_rotation.offset, nullptr,
								0.0f, 1.0f, ImVec2(0, 60));

			ImGui::PlotLines(	"Atmos Sun Framecount", im->m_plot_sun_framecount.values, 
								imgui::debug_plot::history_size, im->m_plot_sun_framecount.offset, nullptr, 
								0.0f, 1000.0f, ImVec2(0, 60));

			ImGui::Text("Atmos Sun Lerp T: %.5f", im->m_dbg_vis_sun_lerp_t);
			ImGui::Text("Atmos Sun Elevation: %.5f", im->m_dbg_vis_sun_elevation);
			ImGui::Text("Atmos Sun Rotation: %.5f", im->m_dbg_vis_sun_rotation);
			ImGui::Text("Atmos Sun Framecount: %d", im->m_dbg_vis_sun_frame_count);

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Separator();
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::Checkbox("Override Global Wetness Scale", &im->m_dbg_global_wetness_override);
			ImGui::BeginDisabled(!im->m_dbg_global_wetness_override);
			{
				ImGui::SliderFloat("Global Wetness", &im->m_dbg_global_wetness, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndDisabled();
			}

			ImGui::Checkbox("Wetness OcclTest Allow on Ignored", &im->m_dbg_global_wetness_occl_allow_on_ignored); TT("Allows occlusion tests on ignored surfaces.");
			ImGui::DragFloat("Wetness Rain Marker Height Offset", &im->m_dbg_global_wetness_rain_marker_height_offset, 0.05f, -100.0f, 100.0f, "%.0f");

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Temp Debug Values"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat3("Debug Vector", &im->m_debug_vector.x, 0.01f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 2", &im->m_debug_vector2.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 3", &im->m_debug_vector3.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 4", &im->m_debug_vector4.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 5", &im->m_debug_vector5.x, 0.1f, 0, 0, "%.6f");

			ImGui::Checkbox("Debug Bool 1", &im->m_dbg_debug_bool01);
			ImGui::Checkbox("Debug Bool 2", &im->m_dbg_debug_bool02);
			ImGui::Checkbox("Debug Bool 3", &im->m_dbg_debug_bool03);
			ImGui::Checkbox("Debug Bool 4", &im->m_dbg_debug_bool04);
			ImGui::Checkbox("Debug Bool 5", &im->m_dbg_debug_bool05);
			ImGui::Checkbox("Debug Bool 6", &im->m_dbg_debug_bool06);
			ImGui::Checkbox("Debug Bool 7", &im->m_dbg_debug_bool07);
			ImGui::Checkbox("Debug Bool 8", &im->m_dbg_debug_bool08);
			ImGui::Checkbox("Debug Bool 9", &im->m_dbg_debug_bool09);

			ImGui::DragInt("Debug Int 1", &im->m_dbg_int_01, 0.01f);
			ImGui::DragInt("Debug Int 2", &im->m_dbg_int_02, 0.01f);
			ImGui::DragInt("Debug Int 3", &im->m_dbg_int_03, 0.01f);
			ImGui::DragInt("Debug Int 4", &im->m_dbg_int_04, 0.01f);
			ImGui::DragInt("Debug Int 5", &im->m_dbg_int_05, 0.01f);
			
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Phone Debug"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Phone Proj Offset Row0", im->m_dbg_phone_projection_matrix_offset.m[0], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row1", im->m_dbg_phone_projection_matrix_offset.m[1], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row2", im->m_dbg_phone_projection_matrix_offset.m[2], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row3", im->m_dbg_phone_projection_matrix_offset.m[3], 0.01f);

			ImGui::Spacing(0, 8.0f);
			ImGui::Separator();

			ImGui::ColorEdit3("Phone Clear Color", &im->m_dbg_phone_clear_hack_color.x, ImGuiColorEditFlags_InputRGB);
			TT("Color used for the clear hack used on phones");

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
			ImGui::DragFloat4("##Debug Mtx03 Row0", im->m_debug_mtx03.m[0], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row1", im->m_debug_mtx03.m[1], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row2", im->m_debug_mtx03.m[2], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row3", im->m_debug_mtx03.m[3], 0.01f);
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

			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("FadeContainerBg Start", &im->ImGuiCol_VerticalFadeContainerBackgroundStart.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("FadeContainerBg End", &im->ImGuiCol_VerticalFadeContainerBackgroundEnd.x, coloredit_flags);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
	}

	void dev_other_container()
	{
		static const auto& im = imgui::get();

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::Checkbox("Do not Pause on Lost Focus", &im->m_do_not_pause_on_lost_focus)) {
			im->m_do_not_pause_on_lost_focus_changed = true;
		}

#if DEBUG
		ImGui::Spacing(0.0f, 4.0f);
		if (ImGui::Button("Timecycle Vars - Debug Single Frame", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			im->m_dbg_debug_single_frame_timecycle_remix_vars = true;
		}

		ImGui::Spacing(0.0f, 4.0f);
		if (ImGui::Button("Emissive Intensity - Debug Single Frame", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			im->m_dbg_debug_single_frame_emissive_intensity_vars = true;
		}

		//ImGui::Checkbox("Log Shader Float Constants", &im->m_dbg_debug_shader_float_constants);
#endif

		ImGui::Spacing(0.0f, 4.0f);
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
				dev_debug_container, false, ICON_FA_TERMINAL, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
//#endif

		{
			static float cont_other_height = 0.0f;
			cont_other_height = ImGui::Widget_ContainerWithCollapsingTitle("Other Settings", cont_other_height,
				dev_other_container, true, ICON_FA_MEMORY, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
	}

	void cont_compsettings_quick_cmd()
	{
		if (ImGui::Button(ICON_FA_SAVE "  Save Current Settings", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0))) {
			comp_settings::write_comp_settings_toml();
		} TT("Saves current settings to 'comp_settings.toml'. Ignores settings that were modified by addon setting files.");

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_REDO "   Reload CompSettings", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			if (!ImGui::IsPopupOpen("Reload CompSettings?")) {
				ImGui::OpenPopup("Reload CompSettings?");
			}
		}

		static bool has_addon_files = !shared::utils::get_sorted_files("rtx_comp\\addon_settings", ".toml").empty();
		static bool wants_reload_with_addons = false;
		if (has_addon_files)
		{
			if (ImGui::Button(ICON_FA_REDO "   Reload CompSettings + Addons", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				if (!ImGui::IsPopupOpen("Reload CompSettings?"))
				{
					wants_reload_with_addons = true;
					ImGui::OpenPopup("Reload CompSettings?");
				}
			} TT("Reload the comp_settings file and all addon files stored in 'rtx_comp/addon_settings/");
		}

		// popup
		if (ImGui::BeginPopupModal("Reload CompSettings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			// re-check if we have addon files by now
			has_addon_files = !shared::utils::get_sorted_files("rtx_comp\\addon_settings", ".toml").empty();

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
				if (wants_reload_with_addons) {
					comp_settings::load_all_settings();
				} else {
					comp_settings::load_comp_settings_only();
				}

				wants_reload_with_addons = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size)) 
			{
				wants_reload_with_addons = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	std::vector<std::string> get_presets(const std::string& dir_path)
	{
		std::vector<std::string> presets;
		std::unordered_set<std::string> unique_presets;

		std::filesystem::path dir(shared::globals::root_path + "\\" + dir_path);

		if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
			return presets;
		}

		for (const auto& entry : std::filesystem::directory_iterator(dir))
		{
			if (!entry.is_regular_file()) {
				continue;
			}

			const auto ext = entry.path().extension().string();

			if (ext == ".toml" || ext == ".conf") {
				unique_presets.insert(entry.path().stem().string());
			}
		}

		presets.assign(unique_presets.begin(), unique_presets.end());
		std::sort(presets.begin(), presets.end());

		return presets;
	}

	void compsetting_presets_container()
	{
		//const auto& im = imgui::get();
		static std::vector<std::string> presets;

		if (static bool initial_setup = false; !initial_setup)
		{
			presets = get_presets("rtx_comp\\addon_settings\\presets");
			initial_setup = true;
		}

		ImGui::Spacing(0, 4);
		ImGui::TextUnformatted( "Setting Presets to quickly change the look of the game.\n"
								"Files can be found in 'rtx_comp/addon_settings/presets'");
		ImGui::Spacing(0, SEPARATOR_SPACING);

		if (ImGui::Button(ICON_FA_REDO "   Reset Settings", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
		{
			if (!ImGui::IsPopupOpen("Reset Settings?")) {
				ImGui::OpenPopup("Reset Settings?");
			}
		} TT("Reset everything back to how the game initially set things up\n"
			 "by reloading the comp_settings file and all addon files stored in 'rtx_comp/addon_settings/");

		ImGui::SameLine();
		if (ImGui::Button("Refresh List", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			presets = get_presets("rtx_comp\\addon_settings\\presets");
		} TT("Refresh presets found in 'rtx_comp/addon_settings/presets'")

		// Listbox

		static int selected_preset = -1;

		auto apply_config = []()
			{
				// comp settings (TOML)
				shared::common::log("ImGui", std::format("Preset: Parsing Addon CompSetting 'rtx_comp/addon_settings/presets/{}' ...", presets[selected_preset]), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
				comp_settings::parse_toml("presets\\" + presets[selected_preset] + ".toml");

				// remix config (CONF)
				shared::common::log("ImGui", std::format("Preset: Parsing Addon Config 'rtx_comp/addon_settings/presets/{}' ...", presets[selected_preset]), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
				remix_vars::parse_and_apply_conf("rtx_comp\\addon_settings\\presets\\", presets[selected_preset] + ".conf", 1);
			};

		if (ImGui::BeginListBox("##preset_list", ImVec2(-FLT_MIN, 80.0f)))
		{
			for (auto i = 0u; i < presets.size(); ++i)
			{
				const bool is_selected = (selected_preset == static_cast<int>(i));

				if (ImGui::Selectable(presets[i].c_str(), is_selected)) {
					selected_preset = static_cast<int>(i);
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					apply_config();
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		} TT("List of available presets. Double click to apply.");

		if (ImGui::Button("Apply Selected Preset", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
		{
			if (selected_preset >= 0 && selected_preset < static_cast<int>(presets.size())) {
				apply_config();
			}
		}

		// popup
		if (ImGui::BeginPopupModal("Reset Settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			shared::imgui::draw_background_blur();
			ImGui::Spacing(0.0f, 0.0f);

			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			const auto line1_str = "You'll loose all unsaved changes if you continue!   ";
			const auto line2_str = "To save your changes, use:";
			const auto line3_str = "Save Current Settings";

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
				comp_settings::load_all_settings();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size)) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void compsettings_var_reset_logic(comp_settings::variable& var)
	{
		std::string popup_id = "Reset "s + var.m_name + " ?";

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			if (!ImGui::IsPopupOpen(popup_id.c_str())) {
				ImGui::OpenPopup(popup_id.c_str());
			}
		}

		ImGui::SetNextWindowSize(ImVec2(400.0f, 160.0f));
		if (ImGui::BeginPopupModal(popup_id.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::Spacing(0.0f, 0.0f);

			ImGui::Spacing();
			ImGui::CenterText("This will reset the current variable");

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::CenterText("Are you sure?");
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0); ImGui::SameLine();

			const auto xpos = ImGui::GetCursorPosX();
			ImGui::BeginGroup();

			const auto half_width = ImGui::GetContentRegionAvail().x * 0.5f;
			ImVec2 button_size(half_width - (ImGui::GetStyle().WindowPadding.x * 2.0f) - ImGui::GetStyle().ItemSpacing.x, 0.0f);
			if (ImGui::Button("Back To Saved", button_size))
			{
				var.reset_base();
				ImGui::CloseCurrentPopup();
			} TT("Restores setting to value stored in your comp_settings.toml file.");

			ImGui::SameLine();
			if (ImGui::Button("Back To Default", button_size)) 
			{
				var.reset_default();
				ImGui::CloseCurrentPopup();
			} TT("Restores setting to the default value defined by the compatibility mod.");
			ImGui::EndGroup();
			const auto group_width = ImGui::GetItemRectSize().x;

			ImGui::SetCursorPosX(xpos);
			if (ImGui::Button("Cancel", ImVec2(group_width, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	bool compsettings_bool_widget(const char* desc, comp_settings::variable& var)
	{
		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto gs_var_ptr = var.get_as<bool*>();
		const bool result = ImGui::Checkbox(desc, gs_var_ptr);
		
		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();
		return result;
	}

	bool compsettings_int_widget(const char* desc, comp_settings::variable& var, const int& min = 0, const int& max = 0, const float& speed = 0.02f)
	{
		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto gs_var_ptr = var.get_as<int*>();
		const bool result = ImGui::DragInt(desc, gs_var_ptr, speed, min, max, "%d", (min != 0 || max != 0) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_float_widget(const char* desc, comp_settings::variable& var, const float& min = 0.0f, const float& max = 0.0f, const float& speed = 0.02f, const char* fmt = "%.2f")
	{
		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());
		
		const auto gs_var_ptr = var.get_as<float*>();
		const bool result = ImGui::DragFloat(desc, gs_var_ptr, speed, min, max, fmt, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str()); 
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_vec_widget(const char* desc, comp_settings::variable& var, const int& size, const float& min = 0.0f, const float& max = 0.0f, const float& speed = 0.02f)
	{
		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto cs_var_ptr = var.get_as<float*>();
		bool result = false;
		switch (size)
		{
		case 2:
			assert(var.get_type() == comp_settings::var_type_vec2 && "Type mismatch: expected vec2");
			result = ImGui::DragFloat2(desc, cs_var_ptr, speed, min, max, "%.2f", (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;

		case 3:
			assert(var.get_type() == comp_settings::var_type_vec3 && "Type mismatch: expected vec3");
			result = ImGui::DragFloat3(desc, cs_var_ptr, speed, min, max, "%.2f", (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;

		default:
		case 4:
			assert(var.get_type() == comp_settings::var_type_vec4 && "Type mismatch: expected vec4");
			result = ImGui::DragFloat4(desc, cs_var_ptr, speed, min, max, "%.2f", (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;
		}

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_color_widget(const char* desc, comp_settings::variable& var, const int& size, const ImGuiColorEditFlags_& flags)
	{
		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto cs_var_ptr = var.get_as<float*>();
		bool result = false;

		switch (size)
		{
		case 3:
			assert(var.get_type() == comp_settings::var_type_vec3 && "Type mismatch: expected vec3");
			result = ImGui::ColorEdit3(desc, cs_var_ptr, flags);
			break;

		default:
		case 4:
			assert(var.get_type() == comp_settings::var_type_vec4 && "Type mismatch: expected vec4");
			result = ImGui::ColorEdit4(desc, cs_var_ptr, flags);
			break;
		}

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();
		return result;
	}

	// --------------------

	void compsettings_rendering_container()
	{
		//const auto& im = imgui::get();
		const auto& gs = comp_settings::get();

		const float inbetween_spacing = SEPARATOR_SPACING;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" General ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Load ColorMaps Only", gs->load_colormaps_only);
		compsettings_bool_widget("Limit Game Quality Sliders", gs->limit_option_sliders);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Water ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Stabilize Water Texture Hash", gs->override_water_texture_hash);
		compsettings_bool_widget("Assign Animated Water Category", gs->water_apply_animated_water_category);
		compsettings_float_widget("Water Texture Scale", gs->water_texture_uv_scale, 0.00001f, 100.0f, 0.001f, "%.4f");
		compsettings_float_widget("Water Normal Fadeout Distance", gs->water_texture_normal_fadeout_distance, 0.0f, 1000.0f, 0.5f);


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Foliage ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Fixed function Trees", gs->fixed_function_trees);
		compsettings_float_widget("Tree Alpha Cutout Value", gs->tree_foliage_alpha_cutout_value, 0.0f, 20.0f);
		//compsettings_float_widget("Grass Alpha Cutout Value", gs->grass_foliage_alpha_cutout_value, 0.0f, 20.0f);


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Hair ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("NPC Hair Alpha Testing", gs->npc_expensive_hair_alpha_testing);
		ImGui::BeginDisabled(!gs->npc_expensive_hair_alpha_testing.get_as<bool>());
		{
			compsettings_float_widget("NPC Hair Alpha Cutout Value", gs->npc_expensive_hair_alpha_cutout_value, 0.0f, 1.0f);
			ImGui::EndDisabled();
		}


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Vehicle Dirt ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Enable Vehicle Dirt", gs->vehicle_dirt_enabled);
		ImGui::BeginDisabled(!gs->vehicle_dirt_enabled.get_as<bool>());
		{
			compsettings_bool_widget("Enable Vehicle Dirt Color Override", gs->vehicle_dirt_custom_color_enabled);
			compsettings_color_widget("Vehicle Dirt Color", gs->vehicle_dirt_custom_color, 3, ImGuiColorEditFlags_Float);

			compsettings_float_widget("Dirt Roughness: Expo", gs->vehicle_dirt_expo, 0.5f, 5.0f, 0.005f);
			compsettings_float_widget("Dirt Roughness: Min Z-Normal", gs->vehicle_dirt_roughness_z_normal, 0.0f, 1.0f, 0.005f);
			compsettings_float_widget("Dirt Roughness: Blending", gs->vehicle_dirt_roughness_blending, 0.0f, 1.0f, 0.005f);
	
			ImGui::EndDisabled();
		}


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" General Vehicle Settings ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Enable Vehicle Livery", gs->vehicle_livery_enabled);
		compsettings_bool_widget("Force Vertex Color Usage", gs->vehicle_force_vertex_colors);


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Interior Dirt ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Decal Dirt Shader Usage", gs->decal_dirt_shader_usage);
		ImGui::BeginDisabled(!gs->vehicle_dirt_enabled.get_as<bool>());
		{
			compsettings_float_widget("Dirt Decal Shader Scalar", gs->decal_dirt_shader_scalar, 0.0f, 8.0f);
			compsettings_float_widget("Dirt Decal Shader Contrast", gs->decal_dirt_shader_contrast, 0.0f, 8.0f);
			ImGui::EndDisabled();
		}


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Effects ");
		ImGui::Spacing(0, 4);

		compsettings_float_widget("GTA_RMPTFX_Litsprite Alpha Scalar", gs->gta_rmptfx_litsprite_alpha_scalar, 0.0f, 20.0f);

		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Enable Rain - Remix Particle System", gs->rain_particle_system_enabled);

		ImGui::Spacing(0, 4);
	}

	void compsettings_culling_container()
	{
		static const auto& gs = comp_settings::get();
		const float inbetween_spacing = SEPARATOR_SPACING;

		ImGui::Widget_CategoryWithVerticalLabel("AntiCulling Cascade", [&]()
			{
				ImGui::PushID("ac_cascade");

				compsettings_float_widget("Near: No Culling Until Distance", gs->nocull_dist_near_static, 0.0f, FLT_MAX, 0.5f);
				ImGui::Spacing(0, inbetween_spacing);

				compsettings_float_widget("Near to Medium Cascade: Medium Distance", gs->nocull_dist_medium_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Near to Medium Cascade: Min. Object Radius", gs->nocull_radius_medium_static, 0.0f, FLT_MAX, 0.5f);
				ImGui::Spacing(0, inbetween_spacing);

				compsettings_float_widget("Medium to Far Cascade: Far Distance", gs->nocull_dist_far_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Medium to Far Cascade: Min. Object Radius", gs->nocull_radius_far_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Medium to Far Cascade: Min. Object Height", gs->nocull_height_far_static, 0.0f, FLT_MAX, 0.5f);

				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::Widget_CategoryWithVerticalLabel("AntiCulling Extended", [&]()
			{
				ImGui::PushID("exac");
				compsettings_bool_widget("Use Extended AntiCulling", gs->nocull_extended);

				ImGui::BeginDisabled(!gs->nocull_extended._bool());
				compsettings_bool_widget("Use Auto Extended AntiCulling", gs->nocull_extended_auto);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!gs->nocull_extended_auto._bool());
				compsettings_float_widget("Consider until Distance", gs->nocull_extended_dist, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Min. Object Radius", gs->nocull_extended_radius, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Min. Object Height", gs->nocull_extended_height, 0.0f, FLT_MAX, 0.5f);
				ImGui::EndDisabled();

				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::Widget_CategoryWithVerticalLabel("NoCull MapArea", [&]()
			{
				ImGui::PushID("nocullarea");

				compsettings_bool_widget("Enable NoCull Logic", gs->nocull_map_areas);
				ImGui::BeginDisabled(!gs->nocull_map_areas._bool());
					compsettings_int_widget("Area Count", gs->nocull_map_areas_count, 1, 5);

					ImGui::Spacing(0, inbetween_spacing);
					compsettings_bool_widget("Always draw lowest LOD", gs->nocull_map_areas_always_draw_lowest_lod);
					compsettings_float_widget("Lowest LOD Min. Size", gs->nocull_map_areas_always_draw_lowest_lod_min_size, 0.0f, 500.0f, 0.5f);
				ImGui::EndDisabled();

				ImGui::Spacing(0, inbetween_spacing);
				compsettings_bool_widget("Enable High LOD Logic", gs->nocull_map_areas_high_lod_logic);
				ImGui::BeginDisabled(!gs->nocull_map_areas_high_lod_logic._bool());
					compsettings_float_widget("High LOD Distance", gs->nocull_map_areas_high_lod_logic_distance, -5000.0f, 5000.0f, 1.0f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::Widget_CategoryWithVerticalLabel("Other Settings", [&]()
			{
				ImGui::PushID("otherac");
				compsettings_float_widget("Light/Prop Distance", gs->nocull_dist_lights, 0.0f, 500.0f, 0.5f);
				compsettings_float_widget("Interior Object Distance", gs->nocull_dist_sphere_interior, 0.0f, 500.0f, 0.5f);

				ImGui::PopID();
			});

		ImGui::Spacing(0, 4);
	}

	void compsettings_light_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = comp_settings::get();

		const float inbetween_spacing = SEPARATOR_SPACING;

		bool clear = false;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Sphere/Spot ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Translate Game Lights", gs->translate_game_lights);
		compsettings_bool_widget("Ignore Filler Lights", gs->translate_game_lights_ignore_filler_lights);

		compsettings_bool_widget("No Volumetrics on Filler Lights", gs->translate_game_lights_no_volumetrics_on_filler_lights);

		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Radius Scalar", gs->translate_game_light_radius_scalar, 0.0f, 0.0f, 0.005f));
		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Intensity Scalar", gs->translate_game_light_intensity_scalar, 0.0f, 0.0f, 0.005f));

		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Softness Offset", gs->translate_game_light_softness_offset, -1.0f, 1.0f, 0.005f, "%.3f"));
		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Softness Scalar", gs->translate_game_light_softness_scalar, 0.0f, 1.0f, 0.005f, "%.3f"));

		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SphereLight Volumetric Scale", gs->translate_game_light_spherelight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f));
		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Volumetric Scale", gs->translate_game_light_spotlight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f));
		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Angle Offset", gs->translate_game_light_angle_offset, -180.0f, 180.0f, 0.1f));
		CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Focus Expo", gs->translate_game_light_focus_expo, 0.0f, 10.0f, 0.01f));

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Distant ");
		ImGui::Spacing(0, 4);

		ImGui::BeginDisabled(gs->timecycle_use_remix_atmos_system._bool());
		{
			compsettings_float_widget("SunLight Intensity Scalar", gs->translate_sunlight_intensity_scalar, 0.0f, 0.0f, 0.005f);
			compsettings_float_widget("SunLight Bad Weather Influence", gs->translate_sunlight_intensity_bad_weather_influence, 0.0f, 1.0f, 0.005f);
			compsettings_float_widget("SunLight Angular Diameter Degrees", gs->translate_sunlight_angular_diameter_degrees, 0.0f, 45.0f, 0.005f);
			compsettings_float_widget("SunLight Volumetric Base", gs->translate_sunlight_volumetric_radiance_base, 0.0f, 10.0f, 0.005f);
			compsettings_float_widget("MoonLight Intensity Scalar", gs->translate_moonlight_intensity_scalar, 0.0f, 1.0f, 0.005f, "%.3f");
			ImGui::EndDisabled();
		}
		
		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Vehicle Headlights / Rearlights ");
		ImGui::Spacing(0, 4);

		ImGui::Widget_CategoryWithVerticalLabel("Head", [&]()
		{
			ImGui::PushID("headlight");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Headlight Intensity Scalar", gs->translate_vehicle_headlight_intensity_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Headlight Radius Scalar", gs->translate_vehicle_headlight_radius_scalar, 0.0f, 0.0f, 0.005f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, SEPARATOR_SPACING);

		ImGui::Widget_CategoryWithVerticalLabel("Rear", [&]()
		{
			ImGui::PushID("rearlight");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Intensity Scalar", gs->translate_vehicle_rearlight_intensity_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Radius Scalar", gs->translate_vehicle_rearlight_radius_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Inner ConeAngle Offset", gs->translate_vehicle_rearlight_inner_cone_angle_offset, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Outer ConeAngle Offset", gs->translate_vehicle_rearlight_outer_cone_angle_offset, 0.0f, 0.0f, 0.005f));

			ImGui::Spacing(0, 4);

			{
				const auto gs_var_ptr = gs->translate_vehicle_rearlight_direction_offset.get_as<float*>();
				CLEAR_CACHE_CHECK(clear, ImGui::DragFloat3("Rearlight Direction Offset", gs_var_ptr, 0.005f, 0.0f, 0.0f, "%.2f"));
				TT(gs->translate_vehicle_rearlight_direction_offset.get_tooltip_string().c_str());
			}

			ImGui::PopID();
		});

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Sirens ");
		ImGui::Spacing(0, 4);

		ImGui::Widget_CategoryWithVerticalLabel("Fake", [&]()
		{
			ImGui::PushID("fakesiren");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Z Offset", gs->translate_vehicle_fake_siren_z_offset, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Intensity Offset", gs->translate_vehicle_fake_siren_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Radius Offset", gs->translate_vehicle_fake_siren_radius_offset, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, SEPARATOR_SPACING);

		ImGui::Widget_CategoryWithVerticalLabel("Siren", [&]()
		{
			ImGui::PushID("siren");
			CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Make Spotlight", gs->translate_vehicle_vsirens_make_spotlight));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Intensity Offset", gs->translate_vehicle_vsirens_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Radius Offset", gs->translate_vehicle_vsirens_radius_offset, 0.0f, 0.0f, 0.01f));

			ImGui::Spacing(0.0f, 4.0f);
			CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Secondary Spherelight", gs->translate_vehicle_vsirens_secondary_spherelight_enabled));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Intensity Offset", gs->translate_vehicle_vsirens_secondary_spherelight_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Radius Offset", gs->translate_vehicle_vsirens_secondary_spherelight_radius_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Z Offset", gs->translate_vehicle_vsirens_secondary_spherelight_z_offset, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, SEPARATOR_SPACING);

		ImGui::Widget_CategoryWithVerticalLabel("Bar", [&]()
		{
			ImGui::PushID("barsiren");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Intensity Scalar", gs->translate_vehicle_barsirens_intensity_scalar, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Radius Scalar", gs->translate_vehicle_barsirens_radius_scalar, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		if (clear) {
			remix_lights::clear_light_cache();
		}

		ImGui::Spacing(0, 4);
	}

	void compsettings_emissive_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = comp_settings::get();

		const float inbetween_spacing = SEPARATOR_SPACING;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Vehicle ");
		ImGui::Spacing(0, 4);

		compsettings_float_widget("Vehicle Light Emissive Scalar", gs->vehicle_lights_emissive_scalar, 0.0f, 0.0f, 0.005f, "%.3f");

		compsettings_bool_widget("Render Surfs a Second Time with Proxy Texture", gs->vehicle_lights_dual_render_proxy_texture);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" World ");
		ImGui::Spacing(0, 8);

		compsettings_float_widget("EmissiveNight Surfaces Scalar", gs->emissive_night_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
		compsettings_float_widget("Emissive Surfaces Scalar", gs->emissive_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
		compsettings_float_widget("EmissiveStrong Surfaces Scalar", gs->emissive_strong_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
		compsettings_float_widget("Generic Emissive Scale", gs->emissive_generic_scale, 0.0f, 1000.0f, 0.001f, "%.3f");
		compsettings_bool_widget("AlphaBlended Emissives Hack", gs->emissive_alpha_blend_hack);
		ImGui::BeginDisabled(!gs->emissive_alpha_blend_hack._bool());
		{
			compsettings_float_widget("AlphaBlended Emissives Hack Scale", gs->emissive_alpha_blend_hack_scale, 0.0f, 100.0f, 0.001f, "%.3f");
			ImGui::EndDisabled();
		}

		compsettings_bool_widget("Allow Vertex Colors on Emissives", gs->emissive_allow_vertex_colors);

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Phone ");
		ImGui::Spacing(0, 4);

		compsettings_bool_widget("Phone Emissive Override", gs->phone_emissive_override);
		ImGui::BeginDisabled(!gs->phone_emissive_override.get_as<bool>());
		{
			compsettings_float_widget("Phone Emissive Scalar", gs->phone_emissive_scalar, 0.0f, 20.0f);
			ImGui::EndDisabled();
		}

		ImGui::Spacing(0, 4);
	}

	void remix_atmospheric_toggle()
	{
		static const auto& cs = comp_settings::get();
		if (compsettings_bool_widget("Use Remix Atmosphere System", cs->timecycle_use_remix_atmos_system))
		{
			const bool state = cs->timecycle_use_remix_atmos_system._bool();
			if (const auto skyMode = remix_vars::get_option("rtx.skyMode"); skyMode)
			{
				remix_vars::option_value val { .value = state ? 1.0f : 0.0f };
				remix_vars::get()->add_queue_entry(skyMode, val, 0.1f);
			}

			if (const auto tempResampling = remix_vars::get_option("rtx.volumetrics.enableTemporalResampling"); tempResampling)
			{
				remix_vars::option_value val { .enabled = state };
				remix_vars::get()->add_queue_entry(tempResampling, val, 0.1f);
			}
		}
	}

	void tc_tab_fog()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		const bool using_atmos = cs->timecycle_use_remix_atmos_system._bool();
		
		ImGui::Widget_CategoryWithVerticalLabel("Fog Color", [&]() 
			{
				ImGui::PushID("fogcol");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable FogColor Logic", cs->timecycle_fogcolor_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_fogcolor_enabled.get_as<bool>());
				{
					compsettings_float_widget("FogColor Base Strength", cs->timecycle_fogcolor_base_strength, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("FogColor Influence Scalar", cs->timecycle_fogcolor_influence_scalar, 0.0f, 0.0f, 0.005f);

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

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Fog Density", [&]()
			{
				ImGui::PushID("fogdensity");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable FogDensity Logic", cs->timecycle_fogdensity_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_fogdensity_enabled.get_as<bool>());
				{
					compsettings_float_widget("FogDensity Influence Scalar", cs->timecycle_fogdensity_influence_scalar, 0.0f, 0.0f, 0.005f);

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

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Horizon", [&]()
			{
				ImGui::PushID("foghorz");

				compsettings_bool_widget("Enable SkyHorizonHeight Logic", cs->timecycle_skyhorizonheight_enabled);
				ImGui::BeginDisabled(!cs->timecycle_skyhorizonheight_enabled.get_as<bool>());
				{
					compsettings_float_widget("SkyHorizonHeight Scalar", cs->timecycle_skyhorizonheight_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("SkyHorizonHeight Low - Transmittance Offset", cs->timecycle_skyhorizonheight_low_transmittance_offset, 0.0f, 0.0f, 0.01f);
					compsettings_float_widget("SkyHorizonHeight High - Transmittance Offset", cs->timecycle_skyhorizonheight_high_transmittance_offset, 0.0f, 0.0f, 0.01f);

					ImGui::Spacing(0, 4);
					compsettings_float_widget("Camera Height Threshold", cs->timecycle_skyhorizonheight_cam_height_threshold, 0.0f, 0.0f, 1.0f);
					compsettings_float_widget("Camera Height Influence (Low)", cs->timecycle_skyhorizonheight_cam_height_influence_low, 0.0f, 0.0f, 0.01f);
					compsettings_float_widget("Camera Height Influence (High)", cs->timecycle_skyhorizonheight_cam_height_influence_high, 0.0f, 0.0f, 0.01f);

					ImGui::TextDisabled("Timecycle mSkyHorizonHeight: [ %.2f ]",
						im->m_timecyc_curr_mSkyHorizonHeight);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxVolumetricsAtmosphereHeightMeters: [ %.2f ]",
						im->m_timecyc_curr_mSkyHorizonHeight_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_sun()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		const bool using_atmos = cs->timecycle_use_remix_atmos_system._bool();

		ImGui::Widget_CategoryWithVerticalLabel("Sky Light", [&]()
			{
				ImGui::PushID("skyl");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable SkyLight Logic", cs->timecycle_skylight_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_skylight_enabled.get_as<bool>() || using_atmos);
				{
					compsettings_float_widget("SkyLight Scalar", cs->timecycle_skylight_scalar, 0.0f, 10.0f, 0.0001f, "%.4f");
					compsettings_float_widget("SkyLight Bad Weather Offset", cs->timecycle_skylight_max_offset_bad_weather, 0.0f, 2.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mSkyLightMultiplier: [ %.2f ]", im->m_timecyc_curr_mSkyLightMultiplier);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxSkyBrightness: [ %.2f ]", im->m_timecyc_curr_mSkyLightMultiplier_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Fog Vol", [&]()
			{
				ImGui::PushID("skyl");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable Fogdensity Influence on Volumetric Scale", cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.get_as<bool>());
				{
					compsettings_float_widget("Fogdensity Volumetric Influence Scalar", cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar, 0.0f, 0.0f, 0.005f);
					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_postprocess()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		
		ImGui::Widget_CategoryWithVerticalLabel("Color Correction", [&]()
			{
				ImGui::PushID("colorcorr");

				compsettings_bool_widget("Enable ColorCorrection Logic", cs->timecycle_colorcorrection_enabled);
				ImGui::BeginDisabled(!cs->timecycle_colorcorrection_enabled.get_as<bool>());
				{
					compsettings_float_widget("ColorCorrection Influence", cs->timecycle_colorcorrection_influence, 0.0f, 15.0f, 0.005f);

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

					ImGui::Spacing(0, SEPARATOR_SPACING);
					ImGui::BeginDisabled(!cs->timecycle_colorcorrection_enabled.get_as<bool>());
					{
						compsettings_bool_widget("Enable ColorTemperature Logic", cs->timecycle_colortemp_enabled);
						compsettings_float_widget("ColorTemperature Value", cs->timecycle_colortemp_value, 0.0f, 15.0f, 0.005f);
						compsettings_float_widget("ColorTemperature Influence", cs->timecycle_colortemp_influence, 0.0f, 0.0f, 0.005f);

						//ImGui::TextDisabled("Timecycle mTemperature: [ %.2f ]",
						//	im->m_timecyc_curr_mTemperature);
						ImGui::TextDisabled("ColorTemp Offset applied to rtxTonemapColorBalance: [ %.2f, %.2f, %.2f ]",
							im->m_timecyc_curr_mTemperature_offset.x,
							im->m_timecyc_curr_mTemperature_offset.y,
							im->m_timecyc_curr_mTemperature_offset.z);

						ImGui::EndDisabled();
					}

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Desaturation", [&]()
			{
				ImGui::PushID("desat");

				compsettings_bool_widget("Enable Desaturation Logic", cs->timecycle_desaturation_enabled);
				ImGui::BeginDisabled(!cs->timecycle_desaturation_enabled.get_as<bool>());
				{
					compsettings_float_widget("Desaturation Influence", cs->timecycle_desaturation_influence, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Far Desaturation Influence", cs->timecycle_fardesaturation_influence, 0.0f, 0.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mDesaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturation);
					ImGui::TextDisabled("Timecycle mDesaturationFar: [ %.2f ]", im->m_timecyc_curr_mDesaturationFar);
					ImGui::TextDisabled("mDesaturationFar influence on rtxTonemapSaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturationFar_offset);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxTonemapSaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturation_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Gamma", [&]()
			{
				ImGui::PushID("gammatwk");

				compsettings_bool_widget("Enable Gamma Logic", cs->timecycle_gamma_enabled);
				ImGui::BeginDisabled(!cs->timecycle_gamma_enabled.get_as<bool>());
				{
					compsettings_float_widget("Gamma Offset", cs->timecycle_gamma_offset, 0.0f, 0.0f, 0.005f);
					ImGui::TextDisabled("Timecycle mGamma: [ %.2f ]", im->m_timecyc_curr_mGamma);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxTonemapExposureBias: [ %.2f ]", im->m_timecyc_curr_mGamma_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Bloom", [&]()
			{
				ImGui::PushID("bloomtwk");

				compsettings_bool_widget("Enable Bloom Logic", cs->timecycle_bloom_enabled);
				ImGui::BeginDisabled(!cs->timecycle_bloom_enabled.get_as<bool>());
				{
					compsettings_float_widget("Bloom Intensity Scalar", cs->timecycle_bloomintensity_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Bloom Threshold Scalar", cs->timecycle_bloomthreshold_scalar, 0.0f, 0.0f, 0.005f);

					ImGui::Spacing(0, SEPARATOR_SPACING);
					compsettings_bool_widget("Clamp Min Intensity at Night", cs->timecycle_bloom_night_min_clamp_enabled);
					ImGui::BeginDisabled(!cs->timecycle_bloom_night_min_clamp_enabled.get_as<bool>());
					{
						compsettings_float_widget("Bloom Night Min Value", cs->timecycle_bloom_night_min_clamp_value, 0.0f, 0.0f, 0.005f);
						ImGui::EndDisabled();
					}

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxBloomBurnIntensity: [ %.2f ]", im->m_timecyc_curr_mBloomIntensity_final);
					ImGui::TextDisabled("Out rtxBloomLuminanceThreshold: [ %.2f ]", im->m_timecyc_curr_mBloomThreshold_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_weather()
	{
		const auto& cs = comp_settings::get();
		
		compsettings_bool_widget("Enable Weather Wetness Logic", cs->timecycle_wetness_enabled);
		ImGui::Spacing(0, SEPARATOR_SPACING);

		ImGui::BeginDisabled(!cs->timecycle_wetness_enabled.get_as<bool>());
		{
			ImGui::Widget_CategoryWithVerticalLabel("World", [&]()
				{
					ImGui::PushID("world");
					compsettings_float_widget("Wetness Scalar", cs->timecycle_wetness_world_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Additional Wetness Offset", cs->timecycle_wetness_world_offset, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_world_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_world_blending, 0.0f, 1.0f, 0.005f);

					compsettings_bool_widget("Enable Wetness Variation", cs->timecycle_wetness_world_variation_enable);
					compsettings_bool_widget("Enable Puddle Layer", cs->timecycle_wetness_world_puddle_layer_enable);
					compsettings_bool_widget("Enable World Raindrops", cs->timecycle_wetness_world_raindrop_enable);
					compsettings_float_widget("World Raindrop Scale", cs->timecycle_wetness_world_raindrop_scalar, 0.0f, 10.0f, 0.005f);

					ImGui::Spacing(0, 4.0f);
					compsettings_bool_widget("Enable World Occlusion Check", cs->timecycle_wetness_world_occlusion_check_enable);
					compsettings_bool_widget("Enable Occlusion Smoothing", cs->timecycle_wetness_world_occlusion_smoothing_enable);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			ImGui::Widget_CategoryWithVerticalLabel("Ped", [&]() 
				{
					ImGui::PushID("ped");
					compsettings_bool_widget("Enable Ped Raindrops", cs->timecycle_wetness_ped_raindrop_enable);
					compsettings_float_widget("Ped Raindrop Scale", cs->timecycle_wetness_ped_raindrop_scalar, 0.0f, 10.0f, 0.005f);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			ImGui::Widget_CategoryWithVerticalLabel("Vehicle", [&]() 
				{
					ImGui::PushID("vehicle");
					compsettings_float_widget("Vehicle Wetness Scalar", cs->timecycle_wetness_vehicle_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_vehicle_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_vehicle_blending, 0.0f, 1.0f, 0.005f);

					compsettings_bool_widget("Enable Vehicle Raindrops", cs->timecycle_wetness_vehicle_raindrop_enable);
					compsettings_float_widget("Vehicle Raindrop Scale", cs->timecycle_wetness_vehicle_raindrop_scalar, 0.0f, 10.0f, 0.005f);

					ImGui::SeparatorText("Vehicle Dirt");
					ImGui::PushID("vehicledirt");
					compsettings_float_widget("Intensity Scalar", cs->timecycle_wetness_vehicle_dirt_intensity_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Wetness Scalar", cs->timecycle_wetness_vehicle_dirt_roughness_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_vehicle_dirt_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_vehicle_dirt_blending, 0.0f, 1.0f, 0.005f);
					ImGui::PopID();

					ImGui::PopID();
				});

			ImGui::EndDisabled();
		}
	}

	void compsettings_timecycle_container()
	{
		static const auto& cs = comp_settings::get();

		ImGui::Spacing(0, 4);
		ImGui::Indent(4);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::TextUnformatted("Note:");
		ImGui::PopFont();
		ImGui::TextWrapped("Some of these settings only work with the original sky.");
		ImGui::Unindent(4);

		ImGui::Spacing(0, SEPARATOR_SPACING);

		// no need to show that setting
		//compsettings_bool_widget("Set TimeCycle Variables on EndScene", cs->timecycle_set_on_endscene);
		remix_atmospheric_toggle();

		ImGui::Spacing(0, SEPARATOR_SPACING);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + 12.0f, 8));
		ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGui::GetColorU32(ImGuiCol_Separator));
		if (ImGui::BeginTabBar("##tctabs"))
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);

			ADD_CONTAINER_TAB("Fog  " ICON_FA_WATER, tc_tab_fog, "Fog related Settings");
			ADD_CONTAINER_TAB("Sky/Sun  " ICON_FA_SUN, tc_tab_sun, "Sky and Sun related Settings");
			ADD_CONTAINER_TAB("Post  " ICON_FA_ADJUST, tc_tab_postprocess, "Postprocessing related Settings");
			ADD_CONTAINER_TAB("Weather  " ICON_FA_CLOUD, tc_tab_weather, "Weather related Settings");
			ImGui::EndTabBar();
		} 
		else
		{
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(1);
		}

		ImGui::Spacing(0, 4);
	}

	void compsettings_other_container()
	{
		//static const auto& im = imgui::get();
		static const auto& gs = comp_settings::get();
		//const float inbetween_spacing = SEPARATOR_SPACING;

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText(" Remix ");
		ImGui::Spacing(0, 4);

		compsettings_int_widget("RTXDI Initial Sample Count Override", gs->remix_override_rtxdi_samplecount, 0, 64, 0.01f);
		compsettings_int_widget("RemixVar Set FrameTimeout", gs->remix_var_queue_frame_timeout, 0, 60);

		ImGui::Spacing(0, 4);
	}

	void wip_cs_container()
	{
		//const auto& im = imgui::get();
		const auto& gs = comp_settings::get();
		const float inbetween_spacing = SEPARATOR_SPACING;

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Version 1.3.X ");
		ImGui::Spacing(0, 4);

		remix_atmospheric_toggle();
		
		compsettings_bool_widget("No Volumetrics on Filler Lights", gs->translate_game_lights_no_volumetrics_on_filler_lights);

		ImGui::Spacing(0, inbetween_spacing);

		ImGui::Widget_CategoryWithVerticalLabel("Anti Culling", [&]()
			{
				ImGui::PushID("exac");
				compsettings_bool_widget("Use Extended AntiCulling", gs->nocull_extended);

				ImGui::BeginDisabled(!gs->nocull_extended._bool());
					compsettings_bool_widget("Use Auto Extended AntiCulling", gs->nocull_extended_auto);
				ImGui::EndDisabled();
				
				ImGui::BeginDisabled(!gs->nocull_extended_auto._bool());
					compsettings_float_widget("Consider until Distance", gs->nocull_extended_dist, 0.0f, FLT_MAX, 0.5f);
					compsettings_float_widget("Min. Object Radius", gs->nocull_extended_radius, 0.0f, FLT_MAX, 0.5f);
					compsettings_float_widget("Min. Object Height", gs->nocull_extended_height, 0.0f, FLT_MAX, 0.5f);
				ImGui::EndDisabled();

				ImGui::Spacing(0, inbetween_spacing);
				compsettings_bool_widget("Enable NoCull MapArea Logic", gs->nocull_map_areas);

				ImGui::BeginDisabled(!gs->nocull_map_areas._bool());
					compsettings_int_widget("Area Count", gs->nocull_map_areas_count, 1, 5);

					ImGui::Spacing(0, inbetween_spacing);
					compsettings_bool_widget("Always draw lowest LOD", gs->nocull_map_areas_always_draw_lowest_lod);
					compsettings_float_widget("Lowest LOD Min. Size", gs->nocull_map_areas_always_draw_lowest_lod_min_size, 0.0f, 500.0f, 0.5f);
				ImGui::EndDisabled();

				ImGui::Spacing(0, inbetween_spacing);
				compsettings_bool_widget("Enable MapArea High LOD Logic", gs->nocull_map_areas_high_lod_logic);
				
				ImGui::BeginDisabled(!gs->nocull_map_areas_high_lod_logic._bool());
					compsettings_float_widget("High LOD Distance", gs->nocull_map_areas_high_lod_logic_distance, -5000.0f, 5000.0f, 1.0f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		ImGui::Widget_CategoryWithVerticalLabel("Ems.", [&]()
			{
				ImGui::PushID("emissives");
				compsettings_bool_widget("Allow Vertex Colors on Emissives", gs->emissive_allow_vertex_colors);
				ImGui::PopID();
			});


		ImGui::Spacing(0, inbetween_spacing);
		ImGui::SeparatorText(" Sirens ");
		ImGui::Spacing(0, 4);

		bool clear = false;

		ImGui::Widget_CategoryWithVerticalLabel("Fake", [&]()
			{
				ImGui::PushID("fakesiren");
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Z Offset", gs->translate_vehicle_fake_siren_z_offset, 0.0f, 0.0f, 0.005f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Intensity Offset", gs->translate_vehicle_fake_siren_intensity_offset, 0.0f, 0.0f, 0.01f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Radius Offset", gs->translate_vehicle_fake_siren_radius_offset, 0.0f, 0.0f, 0.01f));
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Siren", [&]()
			{
				ImGui::PushID("vsiren");
				CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Make Spotlight", gs->translate_vehicle_vsirens_make_spotlight));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Intensity Offset", gs->translate_vehicle_vsirens_intensity_offset, 0.0f, 0.0f, 0.01f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Radius Offset", gs->translate_vehicle_vsirens_radius_offset, 0.0f, 0.0f, 0.01f));

				ImGui::Spacing(0.0f, 4.0f);
				CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Secondary Spherelight", gs->translate_vehicle_vsirens_secondary_spherelight_enabled));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Intensity Offset", gs->translate_vehicle_vsirens_secondary_spherelight_intensity_offset, 0.0f, 0.0f, 0.01f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Radius Offset", gs->translate_vehicle_vsirens_secondary_spherelight_radius_offset, 0.0f, 0.0f, 0.01f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Z Offset", gs->translate_vehicle_vsirens_secondary_spherelight_z_offset, 0.0f, 0.0f, 0.01f));
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		ImGui::Widget_CategoryWithVerticalLabel("Bar", [&]()
			{
				ImGui::PushID("barsiren");
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Intensity Scalar", gs->translate_vehicle_barsirens_intensity_scalar, 0.0f, 0.0f, 0.01f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Radius Scalar", gs->translate_vehicle_barsirens_radius_scalar, 0.0f, 0.0f, 0.01f));
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);

		if (clear) {
			remix_lights::clear_light_cache();
		}

		ImGui::Spacing(0, 4);
	}

	void imgui::tab_wip()
	{
		ImGui::Spacing(0, 4);
		ImGui::Indent(4);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::TextUnformatted("Note:");
		ImGui::PopFont();
		ImGui::TextWrapped(
			"Options listed here are still WIP, might cause problems, instability or reduce Performance.");
		ImGui::Unindent(4);
		ImGui::Spacing(0, 16);

		// cs
		{
			static float cont_wip_comp_settings_height = 0.0f;
			cont_wip_comp_settings_height = ImGui::Widget_ContainerWithCollapsingTitle("Comp Settings", cont_wip_comp_settings_height, wip_cs_container,
				true, ICON_FA_CAMERA, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}
	}

	// -----------

	void quicksettings_util_container()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText("    Screenshot Mode     ");
		ImGui::Spacing(0, 2);

		ImGui::BeginDisabled(!game::is_in_game);
		{
			ImGui::Style_ColorButtonPush(im->m_screenshot_mode ? ImVec4(0.22f, 0.5f, 0.26f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
			if (ImGui::Button("Toggle Screenshot Mode", ImVec2(ImGui::GetContentRegionAvail().x, 48)))
			{
				const auto n = natives::get();

				natives::Ped ped;
				n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);
				n->DisplayRadar(im->m_screenshot_mode);

				if (im->m_screenshot_mode) {
					n->HideHudAndRadarThisFrame();
				}

				n->DisplayHud(im->m_screenshot_mode);

				if (im->m_screenshot_mode_hide_player) {
					n->SetCharVisible(ped, im->m_screenshot_mode);
				} else {
					n->SetCharVisible(ped, true);
				}

				im->m_screenshot_mode = !im->m_screenshot_mode;
			}
			ImGui::Style_ColorButtonPop();

			ImGui::Checkbox("Hide Player", &im->m_screenshot_mode_hide_player);

			// ---

			ImGui::Spacing(0, 12);
			ImGui::SeparatorText("    FreeCam     ");
			ImGui::Spacing(0, 2);

			ImGui::Style_ColorButtonPush(im->m_freecam_mode ? ImVec4(0.22f, 0.5f, 0.26f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
			if (ImGui::Button("FreeCam Mode", ImVec2(ImGui::GetContentRegionAvail().x, 48)))
			{
				const auto n = natives::get();

				natives::Ped ped;
				n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);

				if (!n->IsCharSittingInAnyCar(ped))
				{
					im->m_freecam_mode = !im->m_freecam_mode;
					n->SetCharCollision(ped, !im->m_freecam_mode);
					n->FreezeCharPosition(ped, im->m_freecam_mode);
				}
			} 
			TT(	"Enable FreeCam Mode - Not available when sitting in a car!\n"
				"WASD: Forward & Strafing\n"
				"Q/E:  Down & Up\n"
				"Shift/Space: Speedup & Slowdown");
			ImGui::Style_ColorButtonPop();

			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::SliderFloat("FreeCam Forward Speed", &im->m_freecam_fwd_speed, 0.01f, 10.0f, "%.2f");
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::SliderFloat("FreeCam Strafe Speed", &im->m_freecam_rt_speed, 0.01f, 10.0f, "%.2f");
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::SliderFloat("FreeCam Upward Speed", &im->m_freecam_up_speed, 0.01f, 10.0f, "%.2f");
			//SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::DragFloat("FreeCam Upward Offset", &im->m_freecam_up_offset, 0.0001f, 0, 0, "%.5f");
			ImGui::EndDisabled();

			ImGui::Spacing(0, 12);
			ImGui::SeparatorText("    Misc     ");
			ImGui::Spacing(0, 2);
			
			ImGui::Style_ColorButtonPush(im->m_anti_cull_capture_toggle ? ImVec4(0.72f, 0.5f, 0.26f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
			if (ImGui::Button("Quick AntiCull Toggle for Smaller Captures", ImVec2(ImGui::GetContentRegionAvail().x, 48)))
			{
				im->m_anti_cull_capture_toggle = !im->m_anti_cull_capture_toggle;
				const bool temp_overwrite = im->m_anti_cull_capture_toggle;
				const char* comment_str = "Quick AntiCull Toggle";

				// enable temp override so that all get() calls return temp values 
				// temp values default to 0 so we don't need to re-set them here (because we want most of these at 0)
				cs->nocull_dist_near_static.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_dist_medium_static.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_dist_far_static.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_dist_lights.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_dist_sphere_interior.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_extended.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_map_areas.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_map_areas_high_lod_logic.set_temp_override_state(temp_overwrite, comment_str);
				cs->nocull_map_areas_always_draw_lowest_lod.set_temp_override_state(temp_overwrite, comment_str);

			} TT("Quickly toggle all anti culling logic if you want quicker and smaller captures.");
			ImGui::Style_ColorButtonPop();

			ImGui::Spacing(0, TREENODE_SPACING);
			if (const auto p = performance_logger::get(); p) {
				p->draw_imgui_panel_embedded();
			}

			ImGui::Spacing(0.0f, 4.0f);
		}
	}

	void quicksettings_clock_weather_container()
	{
		im_logic_weather_clock_adjustment();
	}

	void imgui::tab_utilities()
	{
		//const auto& im = imgui::get();

		// utilities
		{
			static float cont_quick_utilities_height = 0.0f;
			cont_quick_utilities_height = ImGui::Widget_ContainerWithCollapsingTitle("Utilities", cont_quick_utilities_height, quicksettings_util_container,
				true, ICON_FA_TERMINAL, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// clock / weather
		{
			static float cont_quick_clock_weather_height = 0.0f;
			cont_quick_clock_weather_height = ImGui::Widget_ContainerWithCollapsingTitle("Clock / Weather", cont_quick_clock_weather_height, quicksettings_clock_weather_container,
				true, ICON_FA_CLOCK, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}
	}

	void imgui::tab_compsettings()
	{
		const auto& im = imgui::get();
		//const auto gs = comp_settings::get();

		// quick commands
		{
			static float cont_quickcmd_height = 0.0f;
			cont_quickcmd_height = ImGui::Widget_ContainerWithCollapsingTitle("Quick Commands", cont_quickcmd_height, 
				cont_compsettings_quick_cmd, true, ICON_FA_TERMINAL, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// setting presets
		{
			static float cont_setting_presets_height = 0.0f;
			cont_setting_presets_height = ImGui::Widget_ContainerWithCollapsingTitle("Setting Presets", cont_setting_presets_height, 
				compsetting_presets_container, false, ICON_FA_DOLLY_FLATBED, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// rendering related
		{
			static float cont_cs_renderer_height = 0.0f;
			cont_cs_renderer_height = ImGui::Widget_ContainerWithCollapsingTitle("Rendering Related Settings", cont_cs_renderer_height, 
				compsettings_rendering_container, false, ICON_FA_CAMERA, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// culling related
		{
			static float cont_cs_renderer_height = 0.0f;
			cont_cs_renderer_height = ImGui::Widget_ContainerWithCollapsingTitle("Culling Settings", cont_cs_renderer_height,
				compsettings_culling_container, false, ICON_FA_TV, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// light related
		{
			static float cont_cs_light_height = 0.0f;
			cont_cs_light_height = ImGui::Widget_ContainerWithCollapsingTitle("Light Related Settings", cont_cs_light_height, 
				compsettings_light_container, false, ICON_FA_LIGHTBULB, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// emissive related
		{
			static float cont_cs_emissive_height = 0.0f;
			cont_cs_emissive_height = ImGui::Widget_ContainerWithCollapsingTitle("Emissive Related Settings", cont_cs_emissive_height, 
				compsettings_emissive_container, false, ICON_FA_RSS, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// timecycle related
		{
			static float cont_cs_timecycle_height = 0.0f;
			cont_cs_timecycle_height = ImGui::Widget_ContainerWithCollapsingTitle("Timecycle Related Settings", cont_cs_timecycle_height, 
				compsettings_timecycle_container, false, ICON_FA_CLOCK, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

		// other
		{
			static float cont_cs_other_height = 0.0f;
			cont_cs_other_height = ImGui::Widget_ContainerWithCollapsingTitle("Other Settings", cont_cs_other_height,
				compsettings_other_container, false, ICON_FA_RANDOM, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}
	}

	bool reload_mapsettings_popup()
	{
		static bool popup_rendered_this_frame = false;
		static int last_frame_count = -1;
		
		// Reset flag if we're in a new frame
		if (const int current_frame = ImGui::GetFrameCount(); 
					  current_frame != last_frame_count)
		{
			popup_rendered_this_frame = false;
			last_frame_count = current_frame;
		}
		
		// Only render the popup once per frame
		if (popup_rendered_this_frame) {
			return false;
		}
		
		bool result = false;
		if (ImGui::BeginPopupModal("Reload MapSettings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			popup_rendered_this_frame = true;
			shared::imgui::draw_background_blur();
			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			const auto line1_str = "You'll loose all unsaved changes if you continue!";
			const auto line2_str = "Use the copy to clipboard buttons and manually update  ";
			const auto line3_str = "the map_settings.toml file if you've made changes.";

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
		ImGui::Spacing(0, 4);
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
		if (ImGui::BeginTable("MarkerTable", 10,
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
			ImGui::TableSetupColumn("SpawnOn", ImGuiTableColumnFlags_NoResize, 12.0f);
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

				// - cull dist
				ImGui::TableNextColumn();
				ImGui::Text("%.0f", m.cull_distance);

				// - spawn on
				ImGui::TableNextColumn();
				ImGui::Text("%s", (m.weather_type != game::WEATHER_NONE || m.from_hour > 0 || m.to_hour > 0) ? "X" : "");

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

		ImGui::Spacing(0, 4);

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
					.weather_type = selection->weather_type,
					.weather_transition_value = selection->weather_transition_value,
					.from_hour = selection->from_hour,
					.to_hour = selection->to_hour,
					.comment = selection->comment,
					});

				selection = &markers.back();
			}
			ImGui::Style_ColorButtonPop();
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!selection);
		{
			if (ImGui::Button("TP to Marker")) 
			{
				const auto n = natives::get();
				natives::Ped ped;

				n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);
				n->SetCharCoordinatesNoOffset(ped, selection->origin.x, selection->origin.y, selection->origin.z);
			}

			ImGui::SameLine();

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

			SET_CHILD_WIDGET_WIDTH;
			ImGui::DragFloat("Cull Distance", &selection->cull_distance, 0.05f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::Spacing();

			//ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
			ImGui::Widget_PrettyDragVec3("Origin", &selection->origin.x, true, 80.0f, 0.01f,
				-FLT_MAX, FLT_MAX, "X", "Y", "Z");
			//ImGui::PopStyleVar();

			// RAD2DEG -> DEG2RAD 
			Vector temp_rot = { RAD2DEG(selection->rotation.x), RAD2DEG(selection->rotation.y), RAD2DEG(selection->rotation.z) };

			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
			if (ImGui::Widget_PrettyDragVec3("Rotation", &temp_rot.x, true, 80.0f, 0.05f,
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

			ImGui::Spacing(0, SEPARATOR_SPACING);
			ImGui::SeparatorText("  Spawn On:   ");
			ImGui::Spacing(0, 6);

			SET_CHILD_WIDGET_WIDTH;
			int curr_weather_selection = selection->weather_type;
			if (ImGui::Combo("On Weather", &curr_weather_selection, game::eWeatherTypeStr, 9)) 
			{
				selection->weather_type = (game::eWeatherType)curr_weather_selection;

				if (curr_weather_selection == game::eWeatherType::WEATHER_NONE) {
					selection->internal__is_hidden = false;
				}
			}

			ImGui::BeginDisabled(selection->weather_type == game::WEATHER_NONE);
			{
				SET_CHILD_WIDGET_WIDTH;
				ImGui::SliderFloat("Weather Transition Value", &selection->weather_transition_value, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndDisabled();
			}

			SET_CHILD_WIDGET_WIDTH;
			int temp_hours[2] = { selection->from_hour, selection->to_hour };

			if (temp_hours[1] < 0) {
				temp_hours[1] = selection->from_hour + 1 <= 24 ? selection->from_hour + 1 : selection->from_hour - 1;
			}

			if (ImGui::SliderInt2("Between Hours", temp_hours, -1, 24, "%d", ImGuiSliderFlags_AlwaysClamp))
			{
				selection->from_hour = temp_hours[0];
				selection->to_hour = temp_hours[1];
			}

			SET_CHILD_WIDGET_WIDTH;
			ImGui::InputText("Comment", &selection->comment);

		} // selection

		ImGui::Spacing();
	}

	void cont_mapsettings_light_tweaks()
	{
		const auto& im = imgui::get();
		const auto& gs = comp_settings::get();
		auto& ms = map_settings::get_map_settings();

		auto& ignored_lights = ms.ignored_lights;
		auto& allowed_lights = ms.allow_lights;
		auto& lights_toml_info = ms.lights_toml_info;

		ImGui::Spacing(0, 4);
		reload_mapsettings_button_with_popup("Light-Tweaks");

		ImGui::Spacing(0, 8.0f);
		ImGui::Style_BoldOrangeTextPush();
		ImGui::Checkbox("Visualize Api Light Hashes", &im->m_dbg_visualize_api_light_hashes); TT("Visualize all spawned api light hashes closeby");
		ImGui::Style_BoldOrangeTextPop();

		ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f, 0);
		ImGui::Checkbox("##in_view_only", &im->m_vis_api_lights_show_only_in_view); TT("Visualize only in view");
		ImGui::SameLine();
		ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes);
		{
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("  Draw Distance      ").x);
			ImGui::DragFloat("  Draw Distance      ", &im->m_dbg_visualize_api_light_hashes_distance, 0.02f, 0.0f, 3000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::EndDisabled();
		}

		ImGui::Checkbox("Ignore Filler Lights (Game Setting)", gs->translate_game_lights_ignore_filler_lights.get_as<bool*>());
			TT(gs->translate_game_lights_ignore_filler_lights.get_tooltip_string().c_str());
		ImGui::Spacing(0, SEPARATOR_SPACING);

		{
			if (ImGui::CollapsingHeader("Ignore Lights"))
			{
				ImGui::Spacing(0, 4);
				ImGui::PushFont(shared::imgui::font::BOLD);
				if (ImGui::Button("Copy Ignored to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
				{
					ImGui::LogToClipboard();
					ImGui::LogText("%s", shared::common::toml_ext::build_ignore_lights_array_from_toml_info(lights_toml_info).c_str());
					ImGui::LogFinish();
				} ImGui::PopFont();

				ImGui::Spacing(0, 8.0f);

				if (!im->m_dbg_visualize_api_light_hashes) {
					ImGui::SeparatorText("Nearby lights (Double-Click to Ignore) ~ Enable 'Visualize Api Light Hashes' to enable this feature.");
				}
				else {
					ImGui::SeparatorText("Nearby lights (Double-Click to Ignore)");
				}

				ImGui::Spacing(0.0f, 8.0f);
				ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes);

				static ImGuiTextFilter ignore_filter;
				if (ImGui::BeginListBox("##ignore_lights", ImVec2(ImGui::GetContentRegionAvail().x, 140)))
				{
					for (size_t i = 0; i < im->visualized_api_lights.size(); ++i)
					{
						const auto& light = im->visualized_api_lights[i];

						if (light.allowed_filler) {
							continue;
						}

						// only add lights that are alive for more than 5 frames
						if (light.m_frames_since_addition > 5u)
						{
							char hash_str[17];
							std::snprintf(hash_str, sizeof(hash_str), "%llx", static_cast<unsigned long long>(light.hash));
							
							// Get TOML filename for this hash
							std::string toml_filename = ms.find_highest_priority_ignored_hash_in_toml(light.hash);
							
							// Check if hash is in any TOML file (not just the flat set, since overrides exclude it)
							bool hash_in_any_toml = !toml_filename.empty();
							
							// Filter by hash or TOML filename
							if (!ignore_filter.PassFilter(hash_str) && !ignore_filter.PassFilter(toml_filename.c_str())) {
								continue;
							}

							ImGui::PushID(static_cast<int>(i));

							if (hash_in_any_toml)
							{
								ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
							}

							// Hash column (selectable) - use same format as light overrides
							if (ImGui::Selectable(shared::utils::va("%llx", light.hash), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x * 0.33f, 0)))
							{
								if (ImGui::IsMouseDoubleClicked(0))
								{
									// Add to highest priority TOML file
									if (!ignored_lights.contains(light.hash))
									{
										if (!lights_toml_info.empty())
										{
											if (auto sorted_toml_files = ms.get_sorted_keys(lights_toml_info); 
												!sorted_toml_files.empty())
											{
												lights_toml_info[sorted_toml_files[0]].ignored_lights.insert(light.hash);
												map_settings::rebuild_lights_from_toml_info();
											}
										}
									}
									else
									{
										// Remove from highest priority TOML file
										std::string existing_toml = ms.find_highest_priority_ignored_hash_in_toml(light.hash);
										if (!existing_toml.empty())
										{
											lights_toml_info[existing_toml].ignored_lights.erase(light.hash);
											map_settings::rebuild_lights_from_toml_info();
										}
									}
								}
							}

							if (hash_in_any_toml)
							{
								ImGui::PopStyleColor();
								ImGui::PopFont();
							}

							// Context menu - pop style colors before opening
							if (ImGui::BeginPopupContextItem())
							{
								for (const auto& toml_file : ms.get_sorted_keys(ms.lights_toml_info))
								{
									auto& toml_info = lights_toml_info[toml_file];
									const bool hash_in_this_toml = toml_info.ignored_lights.contains(light.hash);
									
									if (ImGui::BeginMenu(toml_file.c_str()))
									{
										shared::imgui::draw_window_blur();
										
										if (hash_in_this_toml)
										{
											if (ImGui::MenuItem("Remove from this TOML"))
											{
												toml_info.ignored_lights.erase(light.hash);
												map_settings::rebuild_lights_from_toml_info();
											}
										}
										else
										{
											if (ImGui::MenuItem("Add to this TOML"))
											{
												// Copy to this TOML file (don't remove from other TOML files)
												toml_info.ignored_lights.insert(light.hash);
												map_settings::rebuild_lights_from_toml_info();
											}
										}
										
										ImGui::EndMenu();
									}
								}
								
								// Option to remove from all TOML files
								if (hash_in_any_toml)
								{
									ImGui::Spacing(0, 3);
									ImGui::Separator();
									ImGui::Spacing(0, 3);

									if (ImGui::MenuItem("Remove from all TOML files"))
									{
										for (auto& key : lights_toml_info | std::views::values) {
											key.ignored_lights.erase(light.hash);
										}

										map_settings::rebuild_lights_from_toml_info();
									}
								}

								ImGui::EndPopup();
							}

							// TOML filename column (non-selectable text) - only show if hash is in any TOML
							if (hash_in_any_toml)
							{
								ImGui::SameLine();
								ImGui::PushFont(shared::imgui::font::BOLD);
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
								ImGui::Text("%s", toml_filename.empty() ? "" : toml_filename.c_str());
								ImGui::PopFont();
								ImGui::PopStyleColor();
							}

							ImGui::PopID();
						}
					}
					ImGui::EndListBox();
				}

				ImGui::BeginGroup();
				const auto screenpos_prefilter = ImGui::GetCursorScreenPos();
				ignore_filter.Draw("##Filter", ImGui::GetContentRegionAvail().x
					- ImGui::GetFrameHeight()
					- ImGui::GetStyle().FramePadding.x + 3.0f);

				if (!ignore_filter.IsActive())
				{
					ImGui::SetCursorScreenPos(ImVec2(screenpos_prefilter.x + 12.0f, screenpos_prefilter.y + 5.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
					ImGui::TextUnformatted("Filter ..");
					ImGui::PopStyleColor();
				}
				ImGui::EndGroup();


				ImGui::SameLine();
				if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
					ignore_filter.Clear();
				}

				ImGui::EndDisabled();

				ImGui::Spacing(0, 4.0f);
				ImGui::Separator();
				ImGui::Spacing(0, 4.0f);
			}

			// no spacing here because filler lights might not be ignored and next section not visible

			if (const auto filler_ignored = gs->translate_game_lights_ignore_filler_lights._bool(); filler_ignored)
			{
				ImGui::Spacing(0, 4.0f);

				if (ImGui::CollapsingHeader("Allow Lights"))
				{
					ImGui::Spacing(0, 4);
					ImGui::BeginDisabled(!filler_ignored);
					{
						ImGui::PushFont(shared::imgui::font::BOLD);
						if (ImGui::Button("Copy Allowed to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
						{
							ImGui::LogToClipboard();
							ImGui::LogText("%s", shared::common::toml_ext::build_allow_lights_array_from_toml_info(lights_toml_info).c_str());
							ImGui::LogFinish();
						} ImGui::PopFont();

						ImGui::EndDisabled();
					}

					ImGui::Spacing(0, 8.0f);

					ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes || !gs->translate_game_lights_ignore_filler_lights.get_as<bool>());
					ImGui::PushID("AllowLights");

					ImGui::Spacing(0, 8.0f);

					if (!im->m_dbg_visualize_api_light_hashes) {
						ImGui::SeparatorText("Nearby filler lights (Double-Click to Ignore) ~ Enable 'Visualize Light Hashes'");
					} else {
						ImGui::SeparatorText("Nearby filler lights (Double-Click to Ignore)");
					}

					ImGui::Spacing(0.0f, 8.0f);

					//if (gs->translate_game_lights_ignore_filler_lights.get_as<bool>())
					{
						static ImGuiTextFilter filter_allow_lights;
						if (ImGui::BeginListBox("##allow_lights", ImVec2(ImGui::GetContentRegionAvail().x, 140)))
						{
							for (size_t i = 0; i < im->visualized_api_lights.size(); ++i)
							{
								const auto& light = im->visualized_api_lights[i];

								// only add lights that are ignored (filler or manually) and alive for more than 5 frames
								if (/*is_ignored && */ light.is_filler && light.m_frames_since_addition > 5u)
								{
									char hash_str[17];
									std::snprintf(hash_str, sizeof(hash_str), "%llx", static_cast<unsigned long long>(light.hash));
									
									// Get TOML filename for this hash
									std::string toml_filename = ms.find_highest_priority_allowed_hash_in_toml(light.hash);
									
									// Check if hash is in any TOML file
									bool hash_in_any_toml = !toml_filename.empty();
									
									// Filter by hash or TOML filename
									if (!filter_allow_lights.PassFilter(hash_str) && !filter_allow_lights.PassFilter(toml_filename.c_str())) {
										continue;
									}

									ImGui::PushID(static_cast<int>(i));

									if (hash_in_any_toml)
									{
										ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
										ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.1f, 1.0f));
									}

									// Hash column (selectable) - use same format as light overrides
									if (ImGui::Selectable(shared::utils::va("%llx", light.hash), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x * 0.33f, 0)))
									{
										if (ImGui::IsMouseDoubleClicked(0))
										{
											// Add to highest priority TOML file
											if (!allowed_lights.contains(light.hash))
											{
												if (!lights_toml_info.empty())
												{
													if (auto sorted_toml_files = ms.get_sorted_keys(ms.lights_toml_info); 
														!sorted_toml_files.empty())
													{
														lights_toml_info[sorted_toml_files[0]].allow_lights.insert(light.hash);
														map_settings::rebuild_lights_from_toml_info();
													}
												}
											}
											else
											{
												// Remove from highest priority TOML file
												std::string existing_toml = ms.find_highest_priority_allowed_hash_in_toml(light.hash);
												if (!existing_toml.empty())
												{
													lights_toml_info[existing_toml].allow_lights.erase(light.hash);
													map_settings::rebuild_lights_from_toml_info();
												}
											}
										}
									}

									if (hash_in_any_toml)
									{
										ImGui::PopStyleColor();
										ImGui::PopFont();
									}

									// Context menu - pop style colors before opening
									if (ImGui::BeginPopupContextItem())
									{
										for (const auto& toml_file : ms.get_sorted_keys(ms.lights_toml_info))
										{
											auto& toml_info = lights_toml_info[toml_file];
											const bool hash_in_this_toml = toml_info.allow_lights.contains(light.hash);
											
											if (ImGui::BeginMenu(toml_file.c_str()))
											{
												shared::imgui::draw_window_blur();
												
												if (hash_in_this_toml)
												{
													if (ImGui::MenuItem("Remove from this TOML"))
													{
														toml_info.allow_lights.erase(light.hash);
														map_settings::rebuild_lights_from_toml_info();
													}
												}
												else
												{
													if (ImGui::MenuItem("Add to this TOML"))
													{
														// Copy to this TOML file (don't remove from other TOML files)
														toml_info.allow_lights.insert(light.hash);
														map_settings::rebuild_lights_from_toml_info();
													}
												}
												
												ImGui::EndMenu();
											}
										}
										
										// Option to remove from all TOML files
										if (hash_in_any_toml)
										{
											ImGui::Spacing(0, 3);
											ImGui::Separator();
											ImGui::Spacing(0, 3);
											if (ImGui::MenuItem("Remove from all TOML files"))
											{
												for (auto& [toml_file, toml_info] : lights_toml_info) {
													toml_info.allow_lights.erase(light.hash);
												}

												map_settings::rebuild_lights_from_toml_info();
											}
										}

										ImGui::EndPopup();
									}

									// TOML filename column (non-selectable text) - only show if hash is in any TOML
									if (hash_in_any_toml)
									{
										ImGui::SameLine();
										ImGui::PushFont(shared::imgui::font::BOLD);
										ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
										ImGui::Text("%s", toml_filename.empty() ? "" : toml_filename.c_str());
										ImGui::PopFont();
										ImGui::PopStyleColor();
									}

									ImGui::PopID();
								}
							}
							ImGui::EndListBox();
						}

						ImGui::BeginGroup();
						const auto screenpos_prefilter = ImGui::GetCursorScreenPos();
						filter_allow_lights.Draw("##Filter", ImGui::GetContentRegionAvail().x
							- ImGui::GetFrameHeight()
							- ImGui::GetStyle().FramePadding.x + 3.0f);

						if (!filter_allow_lights.IsActive())
						{
							ImGui::SetCursorScreenPos(ImVec2(screenpos_prefilter.x + 12.0f, screenpos_prefilter.y + 5.0f));
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
							ImGui::TextUnformatted("Filter ..");
							ImGui::PopStyleColor();
						}
						ImGui::EndGroup();

						ImGui::SameLine();
						if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
							filter_allow_lights.Clear();
						}
					}
					ImGui::EndDisabled();
					ImGui::PopID();

					ImGui::Spacing(0, 4.0f);
					ImGui::Separator();
					ImGui::Spacing(0, 4.0f);
				}
			}

			ImGui::Spacing(0, 4.0f);

			if (ImGui::CollapsingHeader("Light Overrides"))
			{
				auto& light_overrides_flat = map_settings::get_map_settings().light_overrides;
				auto& light_overrides_toml_info = map_settings::get_map_settings().light_overrides_toml_info;


				using toml_lookup_result_t = std::pair<std::string, map_settings::light_overrides_toml_info_s*>;
				auto find_highest_priority_toml_for_hash = [&ms, &light_overrides_toml_info](uint64_t hash) -> toml_lookup_result_t
					{
						for (const auto& toml_file : ms.get_sorted_keys(light_overrides_toml_info))
						{
							auto& toml_info = light_overrides_toml_info[toml_file];
							for (auto& category : toml_info.categories)
							{
								if (category.overrides.contains(hash)) {
									return { toml_file, &toml_info };
								}
							}

							if (toml_info.flat_overrides.contains(hash)) {
								return { toml_file, &toml_info };
							}
						}

						return { "", nullptr };
					};

				// Helper function to find override data from any TOML file (searches all files, returns first found)
				auto find_override_data_in_any_toml = [&ms, &light_overrides_toml_info](uint64_t hash) -> std::optional<map_settings::light_override_s>
					{
						// Search all TOML files for this hash (highest priority first)
						for (const auto& toml_file : ms.get_sorted_keys(ms.light_overrides_toml_info))
						{
							const auto& toml_info = light_overrides_toml_info[toml_file];
							
							// Check if it's in a category
							for (const auto& category : toml_info.categories)
							{
								if (category.overrides.contains(hash)) {
									return category.overrides.at(hash);
								}
							}
							
							// Check flat overrides
							if (toml_info.flat_overrides.contains(hash)) {
								return toml_info.flat_overrides.at(hash);
							}
						}
						
						return std::nullopt;
					};

				// Helper function to find which TOML file contains a hash (returns filename, empty if not found)
				auto find_toml_file_containing_hash = [&light_overrides_toml_info](uint64_t hash) -> std::string
				{
					// Get sorted TOML filenames (by priority - alphabetical order)
					std::vector<std::string> sorted_toml_files;
					for (const auto& [toml_file, _] : light_overrides_toml_info) {
						sorted_toml_files.push_back(toml_file);
					}

					std::sort(sorted_toml_files.begin(), sorted_toml_files.end());
					
					// Search all TOML files for this hash
					for (const auto& toml_file : sorted_toml_files) 
					{
						const auto& toml_info = light_overrides_toml_info[toml_file];
						
						// Check if it's in a category
						for (const auto& category : toml_info.categories) 
						{
							if (category.overrides.contains(hash)) {
								return toml_file;
							}
						}
						
						// Check flat overrides
						if (toml_info.flat_overrides.contains(hash)) {
							return toml_file;
						}
					}
					
					return "";
				};

				// Build set of all hashes in categories and map hash to (toml_file, category_name) - only store highest priority
				std::unordered_set<uint64_t> hashes_in_categories;
				std::unordered_map<uint64_t, std::pair<std::string, std::string>> hash_to_toml_and_category; // hash -> (toml_file, category_name)
				
				// Get sorted TOML filenames (by priority - alphabetical order)
				std::vector<std::string> sorted_toml_files_for_map;
				for (const auto& [toml_file, _] : light_overrides_toml_info) {
					sorted_toml_files_for_map.push_back(toml_file);
				}

				std::sort(sorted_toml_files_for_map.begin(), sorted_toml_files_for_map.end());
				
				// Iterate in priority order, only store first occurrence (highest priority)
				for (const auto& toml_file : sorted_toml_files_for_map)
				{
					const auto& toml_info = light_overrides_toml_info[toml_file];
					for (const auto& category : toml_info.categories)
					{
						const std::string cat_name = category.category_name.empty() ? "Unnamed" : category.category_name;
						for (const auto& [hash, _] : category.overrides)
						{
							hashes_in_categories.insert(hash);

							// Only store if not already present (first/highest priority wins)
							if (!hash_to_toml_and_category.contains(hash)) {
								hash_to_toml_and_category[hash] = {toml_file, cat_name};
							}
						}
					}
				}

				ImGui::Spacing(0, 4);
				ImGui::PushFont(shared::imgui::font::BOLD);
				if (ImGui::Button("Copy All Overrides to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
				{
					ImGui::LogToClipboard();
					ImGui::LogText("%s", shared::common::toml_ext::build_lightweak_mixed_array(light_overrides_toml_info).c_str());
					ImGui::LogFinish();
				} 
				ImGui::PopFont();

				// Nearby lights list
				ImGui::PushID("LightOverrides");
				ImGui::Spacing(0, 8.0f);

				if (!im->m_dbg_visualize_api_light_hashes) {
					ImGui::SeparatorText("Nearby lights (Select to Tweak) ~ Enable 'Visualize Light Hashes'");
				} else {
					ImGui::SeparatorText("Nearby lights (Select to Tweak) ~ Use Right Click Context Menu");
				}

				ImGui::Spacing(0.0f, 8.0f);
				static uint64_t selected_hash = 0u;

				ImGui::BeginDisabled(!im->m_dbg_visualize_api_light_hashes);
				{
					static ImGuiTextFilter filter_light_tweaks;
					imgui::visualized_api_light_s* selected_vis_light = nullptr;

					if (ImGui::BeginListBox("##lighttweaks", ImVec2(ImGui::GetContentRegionAvail().x, 140)))
					{
						for (size_t i = 0; i < im->visualized_api_lights.size(); ++i)
						{
							auto& vislight = im->visualized_api_lights[i];
							const bool has_override = light_overrides_flat.contains(vislight.hash);
							std::string category_name = "";
							std::string toml_filename = "";

							if (hash_to_toml_and_category.contains(vislight.hash)) 
							{
								category_name = hash_to_toml_and_category[vislight.hash].second;
								toml_filename = hash_to_toml_and_category[vislight.hash].first;
							} 
							// Override exists but not in a category, get TOML filename directly
							else if (has_override) {
								toml_filename = find_highest_priority_toml_for_hash(vislight.hash).first;
							}

							// only add lights that are alive for more than 5 frames
							if (vislight.m_frames_since_addition > 5u)
							{
								char hash_str[17];
								std::snprintf(hash_str, sizeof(hash_str), "%llx", static_cast<unsigned long long>(vislight.hash));

								if (!filter_light_tweaks.PassFilter(hash_str) && !filter_light_tweaks.PassFilter(category_name.c_str()) && !filter_light_tweaks.PassFilter(toml_filename.c_str())) {
									continue;
								}

								// Display hash and category in two columns
								ImGui::PushID(static_cast<int>(vislight.hash));
								char popup_id[64];
								std::snprintf(popup_id, sizeof(popup_id), "##ContextMenu_%llx", static_cast<unsigned long long>(vislight.hash));
								
								// Hash column selectable
								// Highlight if in category (green) or has override (green)
								if (has_override)
								{
									ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
									ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.1f, 1.0f));
								}

								if (ImGui::Selectable(shared::utils::va("%llx", vislight.hash), selected_hash == vislight.hash, 0, ImVec2(ImGui::GetContentRegionAvail().x * 0.33f, 0))) {
									selected_hash = vislight.hash;
								}

								if (has_override)
								{
									ImGui::PopStyleColor();
									ImGui::PopFont();
								}
								
								// Right-click context menu (must be right after the item)
								if (ImGui::BeginPopupContextItem(popup_id))
								{
									ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
									ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 4.0f));

									// Apply blur and padding similar to tooltip
									ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.124f, 0.124f, 0.124f, 0.776f));

									// Draw blur on the popup window
									shared::imgui::draw_window_blur();

									const auto padding = 0.0f;

									ImGui::Spacing(0, padding); // top padding
									ImGui::Spacing(padding, 0); ImGui::SameLine(); // left pad
									
									// Header with hash
									ImGui::PushFont(shared::imgui::font::FONTS::BOLD);
									ImGui::Text("Hash: 0x%llx", static_cast<unsigned long long>(vislight.hash));
									ImGui::PopFont();

									ImGui::Spacing(0, 3);
									ImGui::Separator();
									ImGui::Spacing(0, 3);

									// Remove override option if it exists
									if (has_override)
									{
										ImGui::Spacing(padding, 0); ImGui::SameLine(); // left pad
										if (ImGui::MenuItem("Remove Override"))
										{
											// Remove from highest priority TOML file only
											auto* toml_info_ptr = find_highest_priority_toml_for_hash(vislight.hash).second;
											if (toml_info_ptr)
											{
												// Remove from categories
												for (auto& category : toml_info_ptr->categories) {
													category.overrides.erase(vislight.hash);
												}
												// Remove from flat overrides
												toml_info_ptr->flat_overrides.erase(vislight.hash);
											}
											
											// Rebuild flat map from TOML info (will use next priority if available)
											map_settings::rebuild_light_overrides_from_toml_info();
										}

										ImGui::Spacing(0, 3);
										ImGui::Separator();
										ImGui::Spacing(0, 3);
									}
									
									// Add uncategorized override option if it doesn't exist
									if (!has_override)
									{
										ImGui::Spacing(padding, 0); ImGui::SameLine(); // left pad
										if (ImGui::MenuItem("Add Override (Uncategorized)"))
										{
											map_settings::light_override_s override_data;
											
											// Check if override exists in any lower-priority TOML file and copy its data
											auto existing_override = find_override_data_in_any_toml(vislight.hash);

											// Copy existing override data (don't remove from lower-priority TOML)
											if (existing_override.has_value()) {
												override_data = existing_override.value();
											}
											else
											{
												// Create override from light data
												override_data = {
													.pos = vislight.m_def_copy.mPosition,
													.dir = vislight.m_def_copy.mDirection,
													.color = vislight.m_def_copy.mColor,
													.radius = vislight.m_def_copy.mRadius,
													.intensity = vislight.m_def_copy.mIntensity,
													.volumetric_scale = vislight.m_def_copy.mVolumeScale,
												};

												if (vislight.m_def_copy.mType == game::LT_SPOT)
												{
													override_data.outer_cone_angle = vislight.m_def_copy.mInnerConeAngle;
													override_data.inner_cone_angle = vislight.m_def_copy.mOuterConeAngle;
													override_data.light_type = true;
												}
											}
											
											// Add to flat map
											light_overrides_flat[vislight.hash] = override_data;
											
											// Add to first (highest priority) TOML file's flat_overrides (for rebuilding)
											if (!light_overrides_toml_info.empty())
											{
												std::vector<std::string> temp_sorted;
												for (const auto& [toml_file, _] : light_overrides_toml_info) {
													temp_sorted.push_back(toml_file);
												}

												std::sort(temp_sorted.begin(), temp_sorted.end());
												
												if (!temp_sorted.empty()) {
													light_overrides_toml_info[temp_sorted[0]].flat_overrides[vislight.hash] = override_data;
												}
											}
										}

										ImGui::Spacing(0, 3);
										ImGui::Separator();
										ImGui::Spacing(0, 6);
									}
									
									// Show TOML files as sub-menus with categories
									if (!light_overrides_toml_info.empty())
									{
										ImGui::Spacing(padding, 0); ImGui::SameLine(); // left pad
										ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
										ImGui::Text("Add to Category:");
										ImGui::PopStyleColor();
										ImGui::Spacing(0, 2);

										// Get sorted TOML filenames (by priority)
										std::vector<std::string> sorted_toml_files;
										for (const auto& [toml_file, _] : light_overrides_toml_info) {
											sorted_toml_files.push_back(toml_file);
										}

										std::sort(sorted_toml_files.begin(), sorted_toml_files.end());

										for (const auto& toml_file : sorted_toml_files)
										{
											auto& toml_info = light_overrides_toml_info[toml_file];
											
											if (ImGui::BeginMenu(toml_file.c_str()))
											{
												// Draw blur on the popup window
												shared::imgui::draw_window_blur();

												// Show existing categories in this TOML file
												for (size_t cat_idx = 0; cat_idx < toml_info.categories.size(); ++cat_idx)
												{
													auto& category = toml_info.categories[cat_idx];
													const bool already_in_this_cat = category.overrides.contains(vislight.hash);
													const char* cat_name = category.category_name.empty() ? ("Category " + std::to_string(cat_idx)).c_str() : category.category_name.c_str();
													
													if (ImGui::MenuItem(cat_name, nullptr, already_in_this_cat, !already_in_this_cat))
													{
														// Only remove hash if it's in the same TOML file we're adding to
														// This is a move operation within the same TOML file (different category)
														// If adding to a different TOML file, we copy instead of moving
														std::string existing_toml_file = find_toml_file_containing_hash(vislight.hash);
														if (!existing_toml_file.empty() && existing_toml_file == toml_file)
														{
															// Remove from all categories in this TOML file
															for (auto& other_category : toml_info.categories) {
																other_category.overrides.erase(vislight.hash);
															}

															// Remove from flat overrides in this TOML file
															toml_info.flat_overrides.erase(vislight.hash);
														}
														
														// Get override data: copy from existing override if it exists, otherwise create from light data
														map_settings::light_override_s override_data;
														if (light_overrides_flat.contains(vislight.hash))
														{
															// Use existing override data from flat map (which may be from lower-priority TOML)
															override_data = light_overrides_flat[vislight.hash];
														}
														else
														{
															// Check if override exists in any lower-priority TOML file and copy its data
															auto existing_override = find_override_data_in_any_toml(vislight.hash);
															if (existing_override.has_value())
															{
																// Copy existing override data (don't remove from lower-priority TOML)
																override_data = existing_override.value();
															}
															else
															{
																// Create override from light data
																override_data = {
																	.pos = vislight.m_def_copy.mPosition,
																	.dir = vislight.m_def_copy.mDirection,
																	.color = vislight.m_def_copy.mColor,
																	.radius = vislight.m_def_copy.mRadius,
																	.intensity = vislight.m_def_copy.mIntensity,
																	.volumetric_scale = vislight.m_def_copy.mVolumeScale,
																};

																if (vislight.m_def_copy.mType == game::LT_SPOT)
																{
																	override_data.outer_cone_angle = vislight.m_def_copy.mInnerConeAngle;
																	override_data.inner_cone_angle = vislight.m_def_copy.mOuterConeAngle;
																	override_data.light_type = true;
																}
															}
															
															light_overrides_flat[vislight.hash] = override_data;
														}

														// Add to this category
														category.overrides[vislight.hash] = override_data;
														
														// Rebuild flat map
														map_settings::rebuild_light_overrides_from_toml_info();
													}
												}
												
												if (!toml_info.categories.empty())
												{
													ImGui::Spacing(0, 3);
													ImGui::Separator();
													ImGui::Spacing(0, 3);
												}

												// Option to create new category in this TOML file
												if (ImGui::MenuItem("Create New Category"))
												{
													// Generate unique category name
													std::string base_name = "new";
													std::string new_category_name = base_name + std::to_string(toml_info.categories.size());
													int counter = 0;

													// Check for duplicates in this TOML file
													while (std::any_of(toml_info.categories.begin(), toml_info.categories.end(), 
														[&new_category_name](const auto& cat) {
															return cat.category_name == new_category_name;
														}))
													{
														new_category_name = base_name + std::to_string(toml_info.categories.size() + counter);
														counter++;
													}
													
													// Only remove hash if it's in the same TOML file we're adding to
													// This is a move operation within the same TOML file (different category)
													// If adding to a different TOML file, we copy instead of moving
													std::string existing_toml_file = find_toml_file_containing_hash(vislight.hash);
													if (!existing_toml_file.empty() && existing_toml_file == toml_file)
													{
														// Remove from all categories in this TOML file
														for (auto& other_category : toml_info.categories) {
															other_category.overrides.erase(vislight.hash);
														}

														// Remove from flat overrides in this TOML file
														toml_info.flat_overrides.erase(vislight.hash);
													}
													
													// Create new category
													map_settings::light_override_category_info_s new_category;
													new_category.category_name = new_category_name;
													toml_info.categories.emplace_back(new_category);
													
													// Get override data: copy from existing override if it exists, otherwise create from light data
													map_settings::light_override_s override_data;
													if (light_overrides_flat.contains(vislight.hash))
													{
														// Use existing override data from flat map (which may be from lower-priority TOML)
														override_data = light_overrides_flat[vislight.hash];
													}
													else
													{
														// Check if override exists in any lower-priority TOML file and copy its data
														auto existing_override = find_override_data_in_any_toml(vislight.hash);
														if (existing_override.has_value())
														{
															// Copy existing override data (don't remove from lower-priority TOML)
															override_data = existing_override.value();
														}
														else
														{
															// Create override from light data
															override_data = 
															{
																.pos = vislight.m_def_copy.mPosition,
																.dir = vislight.m_def_copy.mDirection,
																.color = vislight.m_def_copy.mColor,
																.radius = vislight.m_def_copy.mRadius,
																.intensity = vislight.m_def_copy.mIntensity,
																.volumetric_scale = vislight.m_def_copy.mVolumeScale,
															};

															if (vislight.m_def_copy.mType == game::LT_SPOT)
															{
																override_data.outer_cone_angle = vislight.m_def_copy.mInnerConeAngle;
																override_data.inner_cone_angle = vislight.m_def_copy.mOuterConeAngle;
																override_data.light_type = true;
															}
														}
														
														light_overrides_flat[vislight.hash] = override_data;
													}
													
													// Add light to the new category
													toml_info.categories.back().overrides[vislight.hash] = override_data;
													
													// Rebuild flat map
													map_settings::rebuild_light_overrides_from_toml_info();
												}
												
												ImGui::EndMenu();
											}
										}
									}

									ImGui::Spacing(0, padding); // bottom padding

									ImGui::PopStyleVar(2);
									ImGui::PopStyleColor();
									ImGui::EndPopup();
								} // end context menu
								
								// Category column (non-selectable text) - only show if override exists
								if (has_override)
								{
									// TOML filename column (non-selectable text) - only show if override exists
									ImGui::SameLine();
									ImGui::PushFont(shared::imgui::font::BOLD);
									ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.7f, 1.0f));
									ImGui::Text("%s  //  ", toml_filename.empty() ? "" : toml_filename.c_str());
									ImGui::PopFont();
									ImGui::PopStyleColor();

									ImGui::SameLine();
									ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
									ImGui::Text("%s", category_name.empty() ? "(Uncategorized)" : category_name.c_str());
									ImGui::PopStyleColor();
								}
								
								ImGui::PopID();

								if (selected_hash == vislight.hash) {
									selected_vis_light = &vislight;
								}
							}
						}
						ImGui::EndListBox();
					}

					const bool does_override_exist = selected_hash && light_overrides_flat.contains(selected_hash);
					bool pending_erase = false;

					// Put buttons on same line as filter
					if (!does_override_exist && selected_hash)
					{
						if (ImGui::Button("Add Override (Uncategorized)", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, 0)))
						{
							map_settings::light_override_s override_data;
							
							// Check if override exists in any lower-priority TOML file and copy its data
							auto existing_override = find_override_data_in_any_toml(selected_hash);
							if (existing_override.has_value())
							{
								// Copy existing override data (don't remove from lower-priority TOML)
								override_data = existing_override.value();
							}
							else if (selected_vis_light)
							{
								// Create override from light data
								override_data = {
									.pos = selected_vis_light->m_def_copy.mPosition,
									.dir = selected_vis_light->m_def_copy.mDirection,
									.color = selected_vis_light->m_def_copy.mColor,
									.radius = selected_vis_light->m_def_copy.mRadius,
									.intensity = selected_vis_light->m_def_copy.mIntensity,
									.volumetric_scale = selected_vis_light->m_def_copy.mVolumeScale,
								};

								if (selected_vis_light->m_def_copy.mType == game::LT_SPOT)
								{
									override_data.outer_cone_angle = selected_vis_light->m_def_copy.mInnerConeAngle;
									override_data.inner_cone_angle = selected_vis_light->m_def_copy.mOuterConeAngle;
									override_data.light_type = true;
								}
							}
							
							// Add to flat map
							light_overrides_flat[selected_hash] = override_data;
							
							// Add to first (highest priority) TOML file's flat_overrides (for rebuilding)
							if (!light_overrides_toml_info.empty())
							{
								std::vector<std::string> temp_sorted;
								for (const auto& [toml_file, _] : light_overrides_toml_info) {
									temp_sorted.push_back(toml_file);
								}

								std::sort(temp_sorted.begin(), temp_sorted.end());
								
								if (!temp_sorted.empty()) {
									light_overrides_toml_info[temp_sorted[0]].flat_overrides[selected_hash] = override_data;
								}
							}
						}
						ImGui::SameLine();
					}
					else if (does_override_exist)
					{
						if (ImGui::Button("Remove Override", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, 0))) 
						{
							// Remove from highest priority TOML file only
							auto* toml_info_ptr = find_highest_priority_toml_for_hash(selected_hash).second;
							if (toml_info_ptr)
							{
								// Remove from categories
								for (auto& category : toml_info_ptr->categories) {
									category.overrides.erase(selected_hash);
								}

								// Remove from flat overrides
								toml_info_ptr->flat_overrides.erase(selected_hash);
							}
							
							// Rebuild flat map from TOML info (will use next priority if available)
							map_settings::rebuild_light_overrides_from_toml_info();

							pending_erase = true;
						}
						ImGui::SameLine();
					}

					ImGui::BeginGroup();
					const auto screenpos_prefilter = ImGui::GetCursorScreenPos();
					filter_light_tweaks.Draw("##Filter", ImGui::GetContentRegionAvail().x
						- ImGui::GetFrameHeight()
						- ImGui::GetStyle().FramePadding.x + 3.0f);

					if (!filter_light_tweaks.IsActive())
					{
						ImGui::SetCursorScreenPos(ImVec2(screenpos_prefilter.x + 12.0f, screenpos_prefilter.y + 5.0f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
						ImGui::TextUnformatted("Filter ..");
						ImGui::PopStyleColor();
					}

					ImGui::EndGroup();

					ImGui::SameLine();
					if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
						filter_light_tweaks.Clear();
					}

					// Show tweaking UI if override exists (and wasn't just erased)
					if (does_override_exist && !pending_erase)
					{
						ImGui::Spacing(0, 12);

						// Helper lambda to sync changes from flat map to highest priority TOML file
						auto sync_override_to_toml = [&ms, &light_overrides_flat, &light_overrides_toml_info](uint64_t hash)
							{
								if (!light_overrides_flat.contains(hash)) {
									return;
								}
							
								const auto& override_data = light_overrides_flat[hash];
								auto sorted_toml_files = ms.get_sorted_keys(ms.light_overrides_toml_info);
								bool found_anywhere = false;
								
								// Update in the first (highest priority) TOML file that contains this hash
								for (const auto& toml_file : sorted_toml_files)
								{
									auto& toml_info = light_overrides_toml_info[toml_file];
									
									// Check if it's in a category
									bool found_in_category = false;
									for (auto& category : toml_info.categories)
									{
										if (category.overrides.contains(hash)) 
										{
											category.overrides[hash] = override_data;
											found_in_category = true;
											found_anywhere = true;
											break;
										}
									}
									
									// If not in a category, check flat overrides
									if (!found_in_category && toml_info.flat_overrides.contains(hash))
									{
										toml_info.flat_overrides[hash] = override_data;
										found_anywhere = true;
									}
									
									// If we found it in this TOML file, we're done (highest priority)
									if (found_in_category || toml_info.flat_overrides.contains(hash)) {
										break;
									}
								}
								
								// If override doesn't exist in any TOML file yet, create it in the highest priority TOML file
								// This is needed when the main override has no properties but has attached lights
								if (!found_anywhere && !sorted_toml_files.empty())
								{
									// Check if override has any properties or attached lights - if so, save it
									const bool has_properties = 
										override_data._use_pos				|| override_data._use_dir				|| override_data._use_color || 
										override_data._use_radius			|| override_data._use_intensity			|| override_data._use_outer_cone_angle || 
										override_data._use_inner_cone_angle ||	override_data._use_volumetric_scale || override_data._use_light_type;

									// Add to highest priority TOML file's flat_overrides
									if (has_properties || !override_data.attached_lights.empty()) {
										light_overrides_toml_info[sorted_toml_files[0]].flat_overrides[hash] = override_data;
									}
								}
							};

						auto& l = light_overrides_flat[selected_hash];
						
						// Get all lights for this hash (main + attached)
						struct light_entry_s
						{
							bool is_main;
							size_t attached_index; // Only valid if !is_main
						};
						
						std::vector<light_entry_s> all_lights;
						all_lights.push_back({true, 0});
						
						// Get attached lights from the override (if it exists)
						const auto& attached_list = l.attached_lights;
						for (size_t i = 0; i < attached_list.size(); ++i) {
							all_lights.push_back({false, i});
						}
						
						// Track which light is currently selected for editing
						static std::unordered_map<uint64_t, size_t> selected_light_index; // hash -> index in all_lights
						if (!selected_light_index.contains(selected_hash)) {
							selected_light_index[selected_hash] = 0; // Default to main light
						}
						
						size_t& current_light_idx = selected_light_index[selected_hash];
						if (current_light_idx >= all_lights.size()) {
							current_light_idx = 0;
						}
						
						// Helper function to get current light entry (fresh each time to avoid invalidation)
						auto get_current_light_entry = [&]() -> light_entry_s& {
							return all_lights[current_light_idx];
						};
						
						// Helper function to get current light reference (fresh each time to avoid invalidation)
						auto get_current_light = [&]() -> map_settings::light_override_s&
						{
							auto& entry = get_current_light_entry();
							if (entry.is_main) {
								return l;
							} 

							return l.attached_lights[entry.attached_index];
						};
						
						// List of all lights for this hash
						ImGui::SeparatorText(shared::utils::va("Lights for hash [0x%llx]  -  Select Light from List to Edit", static_cast<unsigned long long>(selected_hash)));

						/*static float dbgfloat01 = 1.0f, dbgfloat02 = 0.0f, dbgfloat03 = 0.0f, dbgfloat04 = 0.0f;
						ImGui::Begin("DevSettings");
						ImGui::DragFloat("DevFloat1", &dbgfloat01, 0.005f);
						ImGui::DragFloat("DevFloat2", &dbgfloat02, 0.005f);
						ImGui::DragFloat("DevFloat3", &dbgfloat03, 0.005f);
						ImGui::DragFloat("DevFloat4", &dbgfloat04, 0.005f);
						ImGui::End();*/

						const float row_height = ImGui::GetFrameHeight() * 0.8f;

						if (ImGui::BeginTable("##lights_list", 2, ImGuiTableFlags_SizingStretchProp))
						{
							ImGui::TableSetupColumn("Light", ImGuiTableColumnFlags_WidthStretch, 1.0f);
							ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, row_height + 4);

							for (size_t i = 0; i < all_lights.size(); ++i)
							{
								ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

								const auto& entry = all_lights[i];
								const bool selected = (current_light_idx == i);

								ImGui::TableNextColumn();

								std::string display_name;
								if (entry.is_main) {
									display_name = shared::utils::va("0 - Main Light [0x%llx]", static_cast<unsigned long long>(selected_hash));
								}
								else
								{
									std::string comment;
									if (entry.attached_index < l.attached_lights.size()) {
										comment = l.attached_lights[entry.attached_index].comment;
									}

									if (comment.empty()) {
										comment = "Attached light";
									}

									display_name = shared::utils::va("%zu - %s", entry.attached_index + 1, comment.c_str());
								}

								// Left padding
								ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x);

								// Interaction only (no background)
								if (ImGui::Selectable(display_name.c_str(), selected, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 20.0f))) {
									current_light_idx = i;
								}

								// Delete button
								ImGui::TableNextColumn();
								if (!entry.is_main)
								{
									ImGui::PushID(static_cast<int>(i));
									ImGui::Style_DeleteButtonPush();

									if (ImGui::Button("x", ImVec2(row_height -2.0f, row_height -1.3f)))
									{
										l.attached_lights.erase(l.attached_lights.begin() + entry.attached_index);
										sync_override_to_toml(selected_hash);

										if (current_light_idx > i) {
											current_light_idx--;
										}
											
										else if (current_light_idx == i) {
											current_light_idx = 0;
										}
									}

									ImGui::Style_DeleteButtonPop();
									ImGui::PopID();
								}
							}

							ImGui::EndTable();
						}
						
						ImGui::Spacing(0, 4);
						
						// Button to attach new light (below the table)
						if (ImGui::Button("Attach New Light to Hash", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
						{
							// Get current light fresh to avoid invalid reference
							auto& current_light_ref = get_current_light();
							
							// Create a duplicate of the current light with 0.1 unit offset
							map_settings::light_override_s new_attached_light = current_light_ref;
							
							// If position isn't set, use default from game light
							if (!new_attached_light._use_pos && selected_vis_light) {
								new_attached_light.pos = selected_vis_light->m_def_copy.mPosition;
							}

							new_attached_light.pos.x += 0.1f;
							new_attached_light.pos.y += 0.1f;
							new_attached_light.pos.z += 0.1f;
							new_attached_light._use_pos = true;
							
							new_attached_light.comment = "Attached light #" + std::to_string(l.attached_lights.size());
							
							l.attached_lights.emplace_back(new_attached_light);
							sync_override_to_toml(selected_hash);
							
							// Rebuild all_lights to get the new entry
							all_lights.clear();
							all_lights.push_back({true, 0});

							for (size_t i = 0; i < l.attached_lights.size(); ++i) {
								all_lights.push_back({false, i});
							}
							
							// Select the newly added light (last entry)
							current_light_idx = all_lights.size() - 1;
						}
						
						ImGui::Spacing(0, 4);
						
						// Show which light is being edited
						{
							auto& entry = get_current_light_entry();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
							ImGui::SeparatorText(entry.is_main ? "Editing main light" :
								shared::utils::va("Editing attached light %zu", entry.attached_index + 1));
							ImGui::PopStyleColor();
							ImGui::Spacing(0, 4);
						}

						// Get current light reference fresh each time to avoid invalidation
						map_settings::light_override_s& current_light = get_current_light();
						
						// Helper to get default value from game light
						auto get_default_pos = [&]() -> Vector {
							return selected_vis_light ? selected_vis_light->m_def_copy.mPosition : Vector{};
						};

						auto get_default_dir = [&]() -> Vector {
							return selected_vis_light ? selected_vis_light->m_def_copy.mDirection : Vector{};
						};

						auto get_default_color = [&]() -> Vector {
							return selected_vis_light ? Vector(selected_vis_light->m_def_copy.mColor) : Vector{1.0f, 1.0f, 1.0f};
						};

						auto get_default_radius = [&]() -> float {
							return selected_vis_light ? selected_vis_light->m_def_copy.mRadius : 5.0f;
						};

						auto get_default_intensity = [&]() -> float {
							return selected_vis_light ? selected_vis_light->m_def_copy.mIntensity : 5.0f;
						};

						auto get_default_volumetric_scale = [&]() -> float {
							return selected_vis_light ? selected_vis_light->m_def_copy.mVolumeScale : 1.0f;
						};

						auto get_default_outer_cone = [&]() -> float
						{
							if (selected_vis_light && selected_vis_light->m_def_copy.mType == game::LT_SPOT) {
								return selected_vis_light->m_def_copy.mOuterConeAngle;
							}
							return cosf(45.0f);
						};

						auto get_default_inner_cone = [&]() -> float 
						{
							if (selected_vis_light && selected_vis_light->m_def_copy.mType == game::LT_SPOT) {
								return selected_vis_light->m_def_copy.mInnerConeAngle;
							}
							return cosf(30.0f);
						};

						bool was_use_pos = current_light._use_pos;
						if (ImGui::Checkbox("##Tweak Pos", &current_light._use_pos)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_pos && !was_use_pos) {
								current_light.pos = get_default_pos();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak Pos");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_pos);
						Vector pos_value = current_light._use_pos ? current_light.pos : get_default_pos();
						if (ImGui::DragFloat3("Pos Override", &pos_value.x, 0.025f, 0, 0, "%.2f"))
						{
							current_light.pos = pos_value;
							current_light._use_pos = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_dir = current_light._use_dir;
						if (ImGui::Checkbox("##Tweak Dir", &current_light._use_dir)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_dir && !was_use_dir) {
								current_light.dir = get_default_dir();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak Dir");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_dir);
						Vector dir_value = current_light._use_dir ? current_light.dir : get_default_dir();
						if (ImGui::DragFloat3("Dir Override", &dir_value.x, 0.025f, 0, 0, "%.2f")) 
						{
							current_light.dir = dir_value;
							current_light.dir.Normalize();
							current_light._use_dir = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_color = current_light._use_color;
						if (ImGui::Checkbox("##Tweak Color", &current_light._use_color)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_color && !was_use_color) {
								current_light.color = get_default_color();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak Color");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_color);
						Vector color_value = current_light._use_color ? current_light.color : get_default_color();
						if (ImGui::ColorEdit3("Color Override", &color_value.x)) 
						{
							current_light.color = color_value;
							current_light._use_color = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_radius = current_light._use_radius;
						if (ImGui::Checkbox("##Tweak Radius", &current_light._use_radius)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_radius && !was_use_radius) {
								current_light.radius = get_default_radius();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak Radius");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_radius);
						float radius_value = current_light._use_radius ? current_light.radius : get_default_radius();
						if (ImGui::DragFloat("Radius Override", &radius_value, 0.025f, 0.0f, 0.0f, "%.2f"))
						{
							current_light.radius = radius_value;
							current_light._use_radius = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_intensity = current_light._use_intensity;
						if (ImGui::Checkbox("##Tweak Intensity", &current_light._use_intensity)) {
							// Initialize with default value if enabling for the first time
							if (current_light._use_intensity && !was_use_intensity) {
								current_light.intensity = get_default_intensity();
							}
							sync_override_to_toml(selected_hash);
						} TT("Tweak Intensity");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_intensity);
						float intensity_value = current_light._use_intensity ? current_light.intensity : get_default_intensity();
						if (ImGui::DragFloat("Intensity Override", &intensity_value, 0.025f, 0.0f, 0.0f, "%.2f")) 
						{
							current_light.intensity = intensity_value;
							current_light._use_intensity = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_volumetric_scale = current_light._use_volumetric_scale;
						if (ImGui::Checkbox("##Tweak VolumetricScale", &current_light._use_volumetric_scale)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_volumetric_scale && !was_use_volumetric_scale) {
								current_light.volumetric_scale = get_default_volumetric_scale();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak VolumetricScale");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_volumetric_scale);
						float volumetric_value = current_light._use_volumetric_scale ? current_light.volumetric_scale : get_default_volumetric_scale();
						if (ImGui::DragFloat("VolumetricScale Override", &volumetric_value, 0.025f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) 
						{
							current_light.volumetric_scale = volumetric_value;
							current_light._use_volumetric_scale = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						if (ImGui::Checkbox("##Tweak Light Type", &current_light._use_light_type))
						{
							sync_override_to_toml(selected_hash);
						} TT("Tweak Light Type");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_light_type);
						if (ImGui::Checkbox("Light Type (False: Sphere -- True: Spot)", &current_light.light_type)) 
						{
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_outer_cone_angle = current_light._use_outer_cone_angle;
						if (ImGui::Checkbox("##Tweak OuterConeAngle", &current_light._use_outer_cone_angle)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_outer_cone_angle && !was_use_outer_cone_angle) {
								current_light.outer_cone_angle = get_default_outer_cone();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak OuterConeAngle");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_outer_cone_angle);
						float outer_cone_value = current_light._use_outer_cone_angle ? current_light.outer_cone_angle : get_default_outer_cone();
						float temp_outer_angle = RAD2DEG(acosf(outer_cone_value));
						if (ImGui::DragFloat("OuterConeAngle Override", &temp_outer_angle, 0.025f, 0.0f, 180.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) 
						{
							current_light.outer_cone_angle = cosf(DEG2RAD(temp_outer_angle));
							current_light._use_outer_cone_angle = true;
							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						bool was_use_inner_cone_angle = current_light._use_inner_cone_angle;
						if (ImGui::Checkbox("##Tweak InnerConeAngle", &current_light._use_inner_cone_angle)) 
						{
							// Initialize with default value if enabling for the first time
							if (current_light._use_inner_cone_angle && !was_use_inner_cone_angle) {
								current_light.inner_cone_angle = get_default_inner_cone();
							}

							sync_override_to_toml(selected_hash);
						} TT("Tweak InnerConeAngle");

						ImGui::SameLine(0, 6);
						ImGui::BeginDisabled(!current_light._use_inner_cone_angle);
						float inner_cone_value = current_light._use_inner_cone_angle ? current_light.inner_cone_angle : get_default_inner_cone();
						float temp_inner_angle = RAD2DEG(acosf(inner_cone_value));

						if (ImGui::DragFloat("InnerConeAngle Override", &temp_inner_angle, 0.025f, 0.0f, 180.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
						{
							current_light.inner_cone_angle = cosf(DEG2RAD(temp_inner_angle));
							current_light._use_inner_cone_angle = true;
							if (temp_inner_angle > temp_outer_angle) {
								current_light.inner_cone_angle = current_light.outer_cone_angle;
							}

							sync_override_to_toml(selected_hash);
						}
						ImGui::EndDisabled();

						
						// Get reference fresh to avoid invalidation from vector reallocation
						map_settings::light_override_s& current_light_for_comment = get_current_light();

						ImGui::SetNextItemWidth(306);
						if (ImGui::InputText("Comment", &current_light_for_comment.comment))
						{
							// Sync comment to all TOML files that contain this light (only for main light)
							if (get_current_light_entry().is_main) 
							{
								for (auto& [toml_file, toml_info] : light_overrides_toml_info)
								{
									// Sync to categories
									for (auto& category : toml_info.categories)
									{
										if (category.overrides.contains(selected_hash)) {
											category.overrides[selected_hash].comment = current_light_for_comment.comment;
										}
									}

									// Sync to flat overrides
									if (toml_info.flat_overrides.contains(selected_hash)) {
										toml_info.flat_overrides[selected_hash].comment = current_light_for_comment.comment;
									}
								}
							}
						}
					} // End if (does_override_exist && !pending_erase)
				}
				ImGui::EndDisabled();
				ImGui::PopID();

				ImGui::Spacing(0, 12);
				ImGui::Separator();
				ImGui::Spacing(0, 4);

				// Categories section - organized by TOML files

				// Count nearby lights
				std::unordered_set<uint64_t> nearby_hashes;
				for (const auto& vislight : im->visualized_api_lights)
				{
					if (vislight.m_frames_since_addition > 5u) {
						nearby_hashes.insert(vislight.hash);
					}
				}

				ImGui::Spacing(0, 8);

				static uint64_t selected_hash_for_categories = 0u;
				selected_hash_for_categories = selected_hash; // Sync with the list selection

				// Show TOML files as TreeNodes
				for (const auto& toml_file : ms.get_sorted_keys(ms.light_overrides_toml_info))
				{
					auto& toml_info = light_overrides_toml_info[toml_file];
					
					ImGui::PushID(toml_file.c_str());

					int pushed_treenode_color = 0;
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.060f, 0.060f, 0.060f, 0.275f)); pushed_treenode_color++;
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.360f, 0.360f, 0.360f, 0.275f)); pushed_treenode_color++;

					// TOML file TreeNode with export and add category buttons
					if (ImGui::TreeNodeEx(toml_file.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding))
					{
						const auto og_fpx = ImGui::GetStyle().FramePadding.x;
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, og_fpx - 4.0f));
						ImGui::PopStyleColor(pushed_treenode_color); pushed_treenode_color = 0;

						// Calculate button positions - ensure buttons align with TreeNode header line height
						const float frame_height = ImGui::GetFrameHeight();
						const float export_button_width = ImGui::CalcTextSize(ICON_FA_SAVE " Export").x + ImGui::GetStyle().FramePadding.x * 2;
						const float add_category_button_width = ImGui::CalcTextSize(ICON_FA_PLUS "  Category").x + ImGui::GetStyle().FramePadding.x * 2;
						const float spacing = ImGui::GetStyle().ItemSpacing.x;
						const float total_buttons_width = export_button_width + add_category_button_width + spacing;

						// Position "Add new Category" button to the left of Export button
						ImGui::SameLine(ImGui::GetContentRegionAvail().x - total_buttons_width + ImGui::GetStyle().FramePadding.x);
						ImGui::SetItemAllowOverlap();

						// center y inside treenode header
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);

						ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonGreen, false);
						ImGui::PushFont(shared::imgui::font::BOLD);
						if (ImGui::Button(ICON_FA_PLUS "  Category", ImVec2(add_category_button_width, frame_height)))
						{
							// Generate unique category name
							std::string base_name = "new";
							std::string new_category_name = base_name + std::to_string(toml_info.categories.size());
							int counter = 0;

							// Check for duplicates in this TOML file
							while (std::any_of(toml_info.categories.begin(), toml_info.categories.end(),
								[&new_category_name](const auto& cat)
								{
									return cat.category_name == new_category_name;
								}))
							{
								new_category_name = base_name + std::to_string(toml_info.categories.size() + counter);
								counter++;
							}

							map_settings::light_override_category_info_s new_category;
							new_category.category_name = new_category_name;
							toml_info.categories.emplace_back(new_category);
						}
						ImGui::PopFont();
						ImGui::Style_ColorButtonPop();

						// Position Export button
						ImGui::SameLine();
						ImGui::SetItemAllowOverlap();

						// center y inside treenode header
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);

						ImGui::PushFont(shared::imgui::font::BOLD);
						if (ImGui::Button(ICON_FA_SAVE " Export", ImVec2(export_button_width, frame_height)))
						{
							ImGui::LogToClipboard();
							ImGui::LogText("%s", shared::common::toml_ext::build_lightweak_toml_file(toml_file, toml_info).c_str());
							ImGui::LogFinish();
						}
						ImGui::PopFont();

						// Reset cursor position after buttons
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() + frame_height - ImGui::GetTextLineHeight());

						//ImGui::Spacing(0, 4);

						ImGui::PopStyleVar();
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 6));

						ImGui::Widget_CategoryWithVerticalLabel(toml_file.c_str(), [&]()
							{
								// Show categories for this TOML file
								for (auto cat_it = toml_info.categories.begin(); cat_it != toml_info.categories.end(); )
								{
									auto& category = *cat_it;
									bool pending_cat_removal = false;

									ImGui::PushID(shared::utils::va("light_cat_%zu", std::distance(toml_info.categories.begin(), cat_it)));

									// Count nearby lights in this category
									int nearby_count = 0;
									for (const auto& [hash, _] : category.overrides)
									{
										if (nearby_hashes.contains(hash)) {
											nearby_count++;
										}
									}

									SET_CHILD_WIDGET_WIDTH_MAN(80);

									const auto header_y = ImGui::GetCursorPosY();
									const auto section_name = category.category_name.empty() ? ("Category " + std::to_string(std::distance(toml_info.categories.begin(), cat_it))).c_str() : category.category_name.c_str();
									bool header_open = ImGui::CollapsingHeader(section_name);
									
									const auto post_header_y = ImGui::GetCursorPosY();
									const auto nearby_overrides_str = shared::utils::va("%d / %zu total overrides nearby", nearby_count, category.overrides.size());
									const auto nearby_overrides_str_size = ImGui::CalcTextSize(nearby_overrides_str);

									ImGui::SetCursorPos(ImVec2(
										ImGui::GetContentRegionAvail().x - nearby_overrides_str_size.x + 30.0f,
										header_y + ImGui::GetStyle().FramePadding.y) /*(ImGui::GetFrameHeight() * 0.5f) + (nearby_overrides_str_size.y * 0.5f))*/);

									ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
									ImGui::TextUnformatted(nearby_overrides_str);
									ImGui::PopFont();

									ImGui::SetCursorPosY(post_header_y);
									
									if (header_open)
									{
										ImGui::Spacing(0, 4);

										// Export single category button
										ImGui::PushFont(shared::imgui::font::BOLD);
										if (ImGui::Button("Copy Category to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
										{
											ImGui::LogToClipboard();
											ImGui::LogText("%s", shared::common::toml_ext::build_lightweak_single_category(category).c_str());
											ImGui::LogFinish();
										}
										ImGui::PopFont();

										ImGui::Spacing(0, 4);

										// Category name input
										if (category._internal_buffer.empty()) {
											category._internal_buffer = category.category_name;
										}

										SET_CHILD_WIDGET_WIDTH_MAN(ImGui::GetContentRegionAvail().x * 0.4f - 18);
										if (ImGui::InputText("Category Name", &category._internal_buffer, ImGuiInputTextFlags_EnterReturnsTrue))
										{
											if (!category._internal_buffer.empty()) // do not allow empty name
											{
												category.category_name = category._internal_buffer;
												category._internal_buffer.clear();
											}
										}
										else if (!ImGui::IsItemActive() && ImGui::IsItemDeactivated()) {
											category._internal_buffer = category.category_name; // Reset buffer if user clicked away without pressing Enter
										} TT("Press ENTER to update category name");

										ImGui::Spacing(0, 4);

										// Category comment input
										if (category._internal_comment_buffer.empty()) {
											category._internal_comment_buffer = category.category_comment;
										}

										SET_CHILD_WIDGET_WIDTH_MAN(ImGui::GetContentRegionAvail().x * 0.4f - 18);
										if (ImGui::InputText("Comment", &category._internal_comment_buffer)) {
											category.category_comment = category._internal_comment_buffer;
										}

										ImGui::Spacing(0, 4);

										// Button to add selected hash to this category and remove category on same line
										const float button_width = ImGui::GetContentRegionAvail().x;
										const float add_button_width = button_width * 0.6f;
										const float remove_button_width = button_width * 0.4f - ImGui::GetStyle().ItemSpacing.x;

										ImGui::BeginDisabled(!selected_hash_for_categories || category.overrides.contains(selected_hash_for_categories));
										{
											if (ImGui::Button(shared::utils::va("Add (0x%llx) to Category", static_cast<unsigned long long>(selected_hash_for_categories)), ImVec2(add_button_width, 0)))
											{
												// Only remove hash if it's in the same TOML file we're adding to
												// This is a move operation within the same TOML file (different category)
												// If adding to a different TOML file, we copy instead of moving
												std::string existing_toml_file = find_toml_file_containing_hash(selected_hash_for_categories);
												if (!existing_toml_file.empty() && existing_toml_file == toml_file)
												{
													// Remove from all categories in this TOML file
													for (auto& other_category : toml_info.categories) {
														other_category.overrides.erase(selected_hash_for_categories);
													}

													// Remove from flat overrides in this TOML file
													toml_info.flat_overrides.erase(selected_hash_for_categories);
												}

												// Get override data: copy from existing override if it exists, otherwise create from light data
												map_settings::light_override_s override_data;
												if (light_overrides_flat.contains(selected_hash_for_categories))
												{
													// Use existing override data from flat map (which may be from lower-priority TOML)
													override_data = light_overrides_flat[selected_hash_for_categories];
												}
												else
												{
													// Check if override exists in any lower-priority TOML file and copy its data
													auto existing_override = find_override_data_in_any_toml(selected_hash_for_categories);
													if (existing_override.has_value())
													{
														// Copy existing override data (don't remove from lower-priority TOML)
														override_data = existing_override.value();
													}
													else
													{
														// Create override from selected light
														for (const auto& vislight : im->visualized_api_lights)
														{
															if (vislight.hash == selected_hash_for_categories)
															{
																override_data = 
																{
																	.pos = vislight.m_def_copy.mPosition,
																	.dir = vislight.m_def_copy.mDirection,
																	.color = vislight.m_def_copy.mColor,
																	.radius = vislight.m_def_copy.mRadius,
																	.intensity = vislight.m_def_copy.mIntensity,
																	.volumetric_scale = vislight.m_def_copy.mVolumeScale,
																};

																if (vislight.m_def_copy.mType == game::LT_SPOT)
																{
																	override_data.outer_cone_angle = vislight.m_def_copy.mInnerConeAngle;
																	override_data.inner_cone_angle = vislight.m_def_copy.mOuterConeAngle;
																	override_data.light_type = true;
																}
																break;
															}
														}
													}

													light_overrides_flat[selected_hash_for_categories] = override_data;
												}

												category.overrides[selected_hash_for_categories] = override_data;

												// Rebuild flat map
												map_settings::rebuild_light_overrides_from_toml_info();
											}
										}
										ImGui::EndDisabled();

										ImGui::SameLine();

										ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonRed, true);
										if (ImGui::Button(ICON_FA_TIMES "  Category", ImVec2(remove_button_width, 0)))
										{
											// Remove all overrides in this category (this category is in the current TOML file)
											// We only need to remove from this category since we're working within a specific TOML file
											// The category will be removed entirely, so no need to remove individual hashes

											// Rebuild flat map (will use next priority if available for each hash)
											map_settings::rebuild_light_overrides_from_toml_info();

											pending_cat_removal = true;
										}
										ImGui::Style_ColorButtonPop();

										ImGui::Spacing(0, 14);
									}

									if (pending_cat_removal) {
										cat_it = toml_info.categories.erase(cat_it);
									} else {
										++cat_it;
									}

									ImGui::PopID();

									if (toml_info.categories.size() > 1 && cat_it != toml_info.categories.end()) {
										ImGui::Spacing(0, 4);
									}
								}
							});

						ImGui::PopStyleVar(1);
						
						ImGui::TreePop();
					}

					if (pushed_treenode_color) {
						ImGui::PopStyleColor(pushed_treenode_color); pushed_treenode_color = 0;
					}

					ImGui::PopID();
					ImGui::Spacing(0, 10);
				}
			}
		}
	}

	void cont_mapsettings_anticull_meshes()
	{
		/*ImGui::Checkbox("Visualize Anti Culling Info", &im->m_dbg_visualize_anti_cull_info); TT("Visualize Anti Culling Info");
		ImGui::DragFloat("Info Distance", &im->m_dbg_visualize_anti_cull_info_distance, 0.05f);  TT("Only draw mesh vis. up until this distance.");
		ImGui::DragFloat("Info Min Radius", &im->m_dbg_visualize_anti_cull_info_min_radius, 0.05f); TT("A mesh needs to have at least this radius to be visualized.");
		ImGui::DragInt("Highlight Mesh with Index", &im->m_dbg_visualize_anti_cull_highlight); TT("Draw bounding box around mesh with this index.");
		ImGui::DragFloat("Highlight Line Width", &im->m_dbg_visualize_anti_cull_info_highlight_line_width, 0.05f); TT("Line width for bounding box.");*/

		//const auto& gs = comp_settings::get();

		const auto& im = imgui::get();
		auto& ac = map_settings::get_map_settings().anticull_meshes;
		
		ImGui::Spacing(0, 4);
		ImGui::PushFont(shared::imgui::font::BOLD);
		if (ImGui::Button("Copy to Clipboard   " ICON_FA_SAVE, ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
		{
			ImGui::LogToClipboard();
			ImGui::LogText("%s", shared::common::toml_ext::build_anticull_array(ac).c_str());
			ImGui::LogFinish();
		} ImGui::PopFont();

		ImGui::SameLine();
		reload_mapsettings_button_with_popup("AnticullMeshes");

		ImGui::Spacing(0, 12);
		ImGui::Separator();
		ImGui::Spacing(0, 4);

		ImGui::Checkbox("Visualize Anti Culling Info", &im->m_dbg_visualize_anti_cull_info); TT("Visualize Anti Culling Info");
		ImGui::DragFloat("Info Distance", &im->m_dbg_visualize_anti_cull_info_distance, 0.05f);  TT("Only draw mesh vis. up until this distance.");
		ImGui::DragFloat("Info Min Radius", &im->m_dbg_visualize_anti_cull_info_min_radius, 0.05f); TT("A mesh needs to have at least this radius to be visualized.");
		ImGui::DragInt("Highlight Mesh with Index", &im->m_dbg_visualize_anti_cull_highlight); TT("Draw bounding box around mesh with this index.");
		ImGui::DragFloat("Highlight Line Width", &im->m_dbg_visualize_anti_cull_info_highlight_line_width, 0.05f); TT("Line width for bounding box.");

		ImGui::Spacing(0, 4);
		ImGui::Separator();
		ImGui::Spacing(0, 12);

		ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonGreen, true);
		if (ImGui::Button(ICON_FA_PLUS "  Category", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			ac.emplace_back(map_settings::anti_cull_meshes_s { .distance = 0, .comment = "new" + std::to_string(ac.size())});
		}
		ImGui::Style_ColorButtonPop();

		ImGui::Spacing(0, 6);

		// unassigned
		static ImGuiTextFilter filter_index;

		ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
		ImGui::TextUnformatted("  Filter Indices ... ");
		ImGui::PopFont();

		filter_index.Draw("##Filter", ImGui::GetContentRegionAvail().x
			- ImGui::GetFrameHeight()
			- ImGui::GetStyle().FramePadding.x + 3.0f);

		ImGui::SameLine();
		if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
			filter_index.Clear();
		}

		int cat_idx = 0;
		for (auto it = ac.begin(); it != ac.end(); )
		{
			auto& elem = *it;

			if (!cat_idx) {
				ImGui::Spacing(0, 20);
			} else {
				ImGui::Spacing(0, 8);
			}

			cat_idx++;
			bool pending_cat_removal = false;

			ImGui::PushID(shared::utils::va("cat_%d", cat_idx));
			const auto section_name = shared::utils::va("Section: %s", elem.comment.c_str());
			if (ImGui::CollapsingHeader(section_name))
			{
				/*ImGui::PushFont(shared::imgui::font::BOLD);
				ImGui::SeparatorText(section_name);
				ImGui::PopFont();*/
				ImGui::Spacing(0, 4);

				{
					SET_CHILD_WIDGET_WIDTH;
					ImGui::DragInt("Distance", &elem.distance);

					SET_CHILD_WIDGET_WIDTH;
					if (ImGui::InputText("Comment", &elem._internal_comment_buffer, ImGuiInputTextFlags_EnterReturnsTrue))
					{
						elem.comment = elem._internal_comment_buffer;
						elem._internal_comment_buffer.clear();
					}

					ImGui::Spacing(0, 4);

					ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
					ImGui::TextUnformatted("  Double Click to remove an entry .. ");
					ImGui::PopFont();


					ImGui::Widget_ContainerWithDropdownShadowSquare(120, [&elem, im]()
						{
							if (ImGui::BeginTable("##ac_table", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(ImGui::GetContentRegionAvail().x, 100)))
							{
								ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x / 5);
								ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x / 5);
								ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x / 5);
								ImGui::TableSetupColumn("##col4", ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x / 5);
								ImGui::TableSetupColumn("##col5", ImGuiTableColumnFlags_WidthFixed, ImGui::GetContentRegionAvail().x / 5);
								int col = 0;

								for (auto set_it = elem.indices.begin(); set_it != elem.indices.end(); )
								{
									const auto index_str = shared::utils::va("%d", *set_it);
									if (!filter_index.PassFilter(index_str))
									{
										++set_it;
										continue;
									}
									if (col % 4 == 0) {
										ImGui::TableNextRow();
									}

									ImGui::TableNextColumn();
									bool erase_this = false;

									if (ImGui::Selectable(index_str, false, ImGuiSelectableFlags_AllowDoubleClick))
									{
										if (ImGui::IsMouseDoubleClicked(0)) {
											erase_this = true;
										}
										else {
											im->m_dbg_visualize_anti_cull_highlight = *set_it;
										}
									}

									++col;
									if (erase_this) {
										set_it = elem.indices.erase(set_it);
									}
									else {
										++set_it;
									}
								}
								ImGui::EndTable();
							}
						});


					auto add_mesh_index = [](map_settings::anti_cull_meshes_s& ac)
						{
							try
							{
								const int val = std::stoi(ac._internal_buffer);
								ac.indices.emplace(val);
							}
							catch (const std::invalid_argument&) {
								shared::common::log("ImGui", "AntiCull - Add Index - Invalid Argument", shared::common::LOG_TYPE::LOG_TYPE_ERROR);
							}
							catch (const std::out_of_range&) {
								shared::common::log("ImGui", "AntiCull - Add Index -Out of Range", shared::common::LOG_TYPE::LOG_TYPE_ERROR);
							}

							ac._internal_buffer.clear();
						};

					ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonRed, true);
					if (ImGui::Button("Remove Category")) {
						pending_cat_removal = true;
					}
					ImGui::Style_ColorButtonPop();

					ImGui::SameLine(0, 24);

					ImGui::BeginDisabled(elem._internal_buffer.empty());
					{
						if (ImGui::Button(" + ")) {
							add_mesh_index(elem);
						}

						ImGui::EndDisabled();
					}

					ImGui::SameLine();

					SET_CHILD_WIDGET_WIDTH;
					if (ImGui::InputText("Add Index", &elem._internal_buffer, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
						add_mesh_index(elem);
					} TT("Use the + Button on the left or press ENTER to add the index.");
				}
			}
			ImGui::PopID();

			if (pending_cat_removal) {
				it = ac.erase(it);
			}
			else {
				++it;
			}
		}

		ImGui::Spacing(0.0f, 4.0f);
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

		// game light tweaks
		{
			static float cont_ignore_lights_height = 0.0f;
			cont_ignore_lights_height = ImGui::Widget_ContainerWithCollapsingTitle("Game Light Tweaks", cont_ignore_lights_height, cont_mapsettings_light_tweaks,
				false, ICON_FA_LIGHTBULB, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}

		// anticull meshes
		{
			static float cont_anticull_height = 0.0f;
			cont_anticull_height = ImGui::Widget_ContainerWithCollapsingTitle("Anti Cull Meshes", cont_anticull_height, cont_mapsettings_anticull_meshes,
				false, ICON_FA_EYE, &ImGuiCol_ContainerBackground, &ImGuiCol_ContainerBorder);
		}
	}

	bool w2s(const Vector& world_pos, ImVec2& screen_coords, bool allow_offscreen = false)
	{
		if (const auto vpscene = game::pViewports; vpscene && vpscene->sceneviewport)
		{
			if (shared::globals::d3d_device)
			{
				D3DVIEWPORT9 vp;
				shared::globals::d3d_device->GetViewport(&vp);

				const auto wp = world_pos.ToD3DXVector();

				D3DXVECTOR3 clip_space;
				D3DXVec3Project( &clip_space, &wp, &vp, &vpscene->sceneviewport->proj,  &vpscene->sceneviewport->view, nullptr );

				if (clip_space.z < 0.0f || clip_space.z > 1.0f) {
					return false; // behind camera or too far
				}

				screen_coords.x = clip_space.x;
				screen_coords.y = clip_space.y;

				if (!allow_offscreen)
				{
					// cull off-screen points
					if (screen_coords.x < 0 || screen_coords.x > vp.Width || screen_coords.y < 0 || screen_coords.y > vp.Height) {
						return false;
					}
				}

				return true;
			}
		}

		return false;
	}

	void w2s_draw_sphere(const Vector& world_pos, const Vector& cam_right, float radius, ImU32 color)
	{
		ImVec2 screen_pos;
		if (!w2s(world_pos, screen_pos)) {
			return;
		}

		// Project a point offset by radius along +X to determine on-screen radius
		//Vector offset_pos = world_pos + Vector(radius, 0.f, 0.f);
		Vector offset_pos = world_pos + cam_right * radius;

		ImVec2 screen_edge;
		if (!w2s(offset_pos, screen_edge, true)) {
			return;
		}

		const float dx = screen_edge.x - screen_pos.x;
		const float dy = screen_edge.y - screen_pos.y;
		const float screen_radius = std::sqrt(dx * dx + dy * dy);

		ImGui::GetBackgroundDrawList()->AddCircle(screen_pos, screen_radius, color, 32, 1.0f);
	}

	// Calculate text scale based on distance from screen center (closer to center = larger scale)
	float calculate_text_scale_from_center(const ImVec2& screen_pos, float max_distance = 500.0f, float min_scale = 0.8f, float max_scale = 1.5f)
	{
		if (const auto vpscene = game::pViewports; vpscene && vpscene->sceneviewport)
		{
			if (shared::globals::d3d_device)
			{
				D3DVIEWPORT9 vp;
				shared::globals::d3d_device->GetViewport(&vp);

				// Calculate screen center
				const float center_x = vp.Width * 0.5f;
				const float center_y = vp.Height * 0.5f;

				// Calculate distance from center
				const float dx = screen_pos.x - center_x;
				const float dy = screen_pos.y - center_y;
				const float distance = std::sqrt(dx * dx + dy * dy);

				// Normalize distance (0 = center, 1 = max_distance)
				const float normalized_dist = std::min(distance / max_distance, 1.0f);

				// Interpolate scale (closer to center = larger scale)
				const float scale = max_scale - (normalized_dist * (max_scale - min_scale));

				return scale;
			}
		}

		return 1.0f; // Default scale if viewport not available
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
		
		if (m_dbg_visualize_api_light_hashes)
		{
			const auto rml = remix_lights::get();
			auto ms = map_settings::get_map_settings();

			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				const float draw_dist = im->m_dbg_visualize_api_light_hashes_distance;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				ImGui::PushFont(shared::imgui::font::BOLD_LARGE);

				if (rml->get_active_light_count())
				{
					for (auto& l : rml->get_active_lights())
					{
						if (fabs(cam_org.DistToSqr(l.second.m_def.mPosition) < draw_dist * draw_dist))
						{
							auto msov = ms.light_overrides;

							const map_settings::light_override_s* lov = nullptr;
							uint64_t base_hash = l.second.m_hash;
							
							// For attached lights, extract base hash and get the attached override
							if (l.second.m_is_attached_light)
							{
								const uint64_t attached_index_upper = (l.second.m_hash >> 32);
								base_hash = l.second.m_hash ^ (attached_index_upper << 32);
								
								// Get the attached light override data from the base override
								if (msov.contains(base_hash))
								{
									const auto& override_data = msov.at(base_hash);
									const size_t attached_index = static_cast<size_t>(attached_index_upper - 1);
									if (attached_index < override_data.attached_lights.size()) {
										lov = &override_data.attached_lights[attached_index];
									}
								}
							}
							else
							{
								// Normal light - get override if it exists
								if (const auto it = msov.find(l.second.m_hash); it != msov.end()) {
									lov = &it->second;
								}
							}

							const Vector& light_pos = remix_lights::get_light_position(l.second.m_def, lov);
							bool in_view = w2s(light_pos, viewport_pos);

							// we usually want to also add lights to the list that are not in view
							if (!im->m_vis_api_lights_show_only_in_view || in_view)
							{
								// Only add to visualized_api_lights list if NOT an attached light
								if (!l.second.m_is_attached_light)
								{
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
											visualized_api_light_s
											{
												.hash = l.second.m_hash,
												.pos = l.second.m_def.mPosition,
												.m_def_copy = l.second.m_def,
												.ignored = l.second.m_is_ignored,
												.allowed_filler = l.second.m_is_allowed_filler,
												.is_filler = l.second.m_is_filler,
												.m_updateframe = 0u,
												.m_frames_since_addition = 0u
											}
										);
									}
								}

								// only draw light debug info when the light is in view
								if (in_view)
								{
									// Draw all lights (both normal and attached) in 3D space
									bool is_light_hash_stable = false;

									if (l.second.m_is_attached_light) {
										is_light_hash_stable = true;
									}
									else if (im->m_dbg_visualize_api_light_unstable_hashes) {
										is_light_hash_stable = true;
									}
									else
									{
										for (const auto& ign : visualized_api_lights)
										{
											if (ign.hash == l.second.m_hash)
											{
												is_light_hash_stable = ign.m_frames_since_addition > 5u;
												break;
											}
										}
									}

									if (is_light_hash_stable)
									{
										const auto radius = remix_lights::get_light_radius(l.second.m_def, lov) * comp_settings::get()->translate_game_light_radius_scalar.get_as<float>() * 3.7f;
										const auto& color = remix_lights::get_light_color(l.second.m_def, lov);
										ImGui::GetBackgroundDrawList()->AddCircleFilled(viewport_pos, radius, ImColor(color.x, color.y, color.z));


										std::string toml_filename;
										uint64_t hash_to_check = l.second.m_is_attached_light ? base_hash : l.second.m_hash;

										for (const auto& toml_file : ms.get_sorted_keys(ms.light_overrides_toml_info))
										{
											bool found = false;
											const auto& toml_info = ms.light_overrides_toml_info.at(toml_file);

											for (const auto& category : toml_info.categories)
											{
												if (category.overrides.contains(hash_to_check))
												{
													toml_filename = toml_file;
													found = true;
													break;
												}
											}

											if (!found && toml_info.flat_overrides.contains(hash_to_check))
											{
												toml_filename = toml_file;
												found = true;
											}

											if (found) {
												break;
											}
										}

										// Calculate text scale based on distance from screen center
										const float text_scale = calculate_text_scale_from_center(viewport_pos, 200.0f);

										// Get the current font and use AddText with explicit font size for scaling
										ImFont* font = ImGui::GetFont();
										const float base_font_size = font ? font->FontSize : ImGui::GetFontSize();
										const float scaled_font_size = base_font_size * text_scale;

										if (l.second.m_is_ignored)
										{
											const char* text = shared::utils::va("    [HASH] %llx\n    ~ IGNORED ~", l.second.m_hash);
											if (font) {
												ImGui::GetBackgroundDrawList()->AddText(font, scaled_font_size, viewport_pos, ImColor(1.0f, 0.0f, 0.0f, 1.0f), text);
											}
											else {
												ImGui::GetBackgroundDrawList()->AddText(viewport_pos, ImColor(1.0f, 0.0f, 0.0f, 1.0f), text);
											}
										}
										else if (l.second.m_is_allowed_filler)
										{
											std::string toml_str = toml_filename.empty() ? "" : ("\n    ~ " + toml_filename + " ~");
											const char* text = shared::utils::va("    [HASH] %llx\n~ ALLOWED FILLER ~%s%s", l.second.m_hash, lov ? "\n    ~ OVERRIDE ~" : "", toml_str.c_str());

											if (font) {
												ImGui::GetBackgroundDrawList()->AddText(font, scaled_font_size, viewport_pos, ImColor(0.1f, 0.8f, 0.1f, 1.0f), text);
											}
											else {
												ImGui::GetBackgroundDrawList()->AddText(viewport_pos, ImColor(0.1f, 0.8f, 0.1f, 1.0f), text);
											}
										}
										else if (l.second.m_is_attached_light)
										{
											// Use comment instead of hash for attached lights
											std::string display_text = lov && !lov->comment.empty() ? lov->comment : ("Attached light " + std::to_string((l.second.m_hash >> 32)));
											std::string toml_str = toml_filename.empty() ? "" : ("\n    ~ " + toml_filename + " ~");
											const char* text = shared::utils::va("    %s\n    ~ ATTACHED to 0x%llx ~%s", display_text.c_str(), static_cast<unsigned long long>(base_hash), toml_str.c_str());

											if (font) {
												ImGui::GetBackgroundDrawList()->AddText(font, scaled_font_size, viewport_pos, ImColor(0.7f, 0.7f, 0.9f, 1.0f), text);
											}
											else {
												ImGui::GetBackgroundDrawList()->AddText(viewport_pos, ImColor(0.7f, 0.7f, 0.9f, 1.0f), text);
											}
										}
										else
										{
											std::string toml_str = toml_filename.empty() ? "" : ("\n    ~ " + toml_filename + " ~");
											const char* text = shared::utils::va("    [HASH] %llx %s%s", l.second.m_hash, lov ? "\n    ~ OVERRIDE ~" : "", toml_str.c_str());

											if (font) {
												ImGui::GetBackgroundDrawList()->AddText(font, scaled_font_size, viewport_pos, ImGui::GetColorU32(ImGuiCol_Text), text);
											}
											else {
												ImGui::GetBackgroundDrawList()->AddText(viewport_pos, ImGui::GetColorU32(ImGuiCol_Text), text);
											}
										}
									}
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
				const float draw_dist = im->m_dbg_visualize_distance;

				ImVec2 viewport_pos = {};
				const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				for (auto& l : visualized_decal_renderstates)
				{
					if (fabs(cam_org.DistToSqr(l.pos) < draw_dist * draw_dist))
					{
						if (w2s(l.pos, viewport_pos))
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
						if (w2s(l.pos, viewport_pos))
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

		if (m_dbg_visualize_anti_cull_info)
		{
			const auto vp = game::pViewports;
			if (vp->sceneviewport)
			{
				//const float draw_dist = im->m_dbg_visualize_anti_cull_info_distance;

				ImVec2 viewport_pos = {};
				//const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];

				for (auto& l : visualized_anti_cull)
				{
					//if (fabs(cam_org.DistToSqr(l.pos) < draw_dist * draw_dist))
					{
						if (w2s(l.pos, viewport_pos))
						{
							std::ostringstream oss;
							oss << "R: " << std::format("{:.2f}", l.radius) << "\n"
								<< "H: " << std::format("{:.2f}", l.height) << "\n"
								<< "ID: " << std::to_string(l.m_wModelIndex);

							ImGui::GetBackgroundDrawList()->AddText(viewport_pos,
								l.forced_visible ? ImGui::GetColorU32(ImVec4(0.3f, 0.8f, 0.2f, 1.0f)) : ImGui::GetColorU32(ImGuiCol_Text),
								oss.str().c_str());

							if (l.draw_debug_box) {
								shared::common::remix_api::get().debug_draw_box(l.mins, l.maxs, im->m_dbg_visualize_anti_cull_info_highlight_line_width, shared::common::remix_api::DEBUG_REMIX_LINE_COLOR::GREEN);
							}
						}
					}
				}

				visualized_anti_cull.clear();
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
							if (w2s(m.pos, viewport_pos))
							{
								const ImU32 color = get_color_for_hash(m.hash);
								bool vis_hash = true;

								if (m.model_reference)
								{
									if (const game::CBaseModelInfo* basemodel = game::g_modelPointers[m.model_reference]; basemodel)
									{
										Vector cam_right(
											vp->sceneviewport->cameraInv.m[0][0],
											vp->sceneviewport->cameraInv.m[0][1],
											vp->sceneviewport->cameraInv.m[0][2]
										);

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
											w2s_draw_sphere(m.pos, cam_right, basemodel->m_fRadius, color);
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

		// resets after one frame
		m_dbg_debug_single_frame_timecycle_remix_vars = false;
		m_dbg_debug_single_frame_emissive_intensity_vars = false;

#define ADD_TAB(NAME, FUNC) \
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0)));			\
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + 12.0f, 8));	\
	if (ImGui::BeginTabItem(NAME)) {																		\
		ImGui::PopStyleVar(1);																				\
		if (ImGui::BeginChild("##child_" NAME, ImVec2(0, ImGui::GetContentRegionAvail().y - 20), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar )) {	\
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
			ADD_TAB("WIP", tab_wip);
			ADD_TAB("Comp Settings", tab_compsettings);
			ADD_TAB("Map Settings", tab_map_settings);

			ADD_TAB("Utilities", tab_utilities);
			ADD_TAB("Dev", tab_dev);
			ADD_TAB("About", tab_about);
			ImGui::EndTabBar();
		}
		else 
		{
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

					auto& io = ImGui::GetIO();

					if (shared::globals::imgui_allow_input_bypass_timeout) {
						shared::globals::imgui_allow_input_bypass_timeout--;
					}

					shared::globals::imgui_wants_text_input = ImGui::GetIO().WantTextInput;

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
		ImGuiCol_ContainerBackground = ImVec4(0.17f, 0.17f, 0.17f, 0.875f);
		ImGuiCol_ContainerBorder = ImVec4(0.477f, 0.39f, 0.25f, 0.90f);

		ImGuiCol_VerticalFadeContainerBackgroundStart = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);
		ImGuiCol_VerticalFadeContainerBackgroundEnd = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
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

		ImGui_ImplWin32_Init(shared::globals::main_window);
		g_game_wndproc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(shared::globals::main_window, GWLP_WNDPROC, LONG_PTR(wnd_proc_hk)));

		// ---
		m_initialized = true;
		shared::common::log("ImGui", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}



