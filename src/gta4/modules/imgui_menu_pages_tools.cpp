#include "std_include.hpp"
#include "imgui_menu.hpp"

#include <algorithm>

#include "comp_settings.hpp"
#include "free_camera.hpp"
#include "imgui.hpp"
#include "imgui_editor_common.hpp"
#include "imgui_light_editor.hpp"
#include "imgui_marker_editor.hpp"
#include "imgui_mesh_inspector.hpp"
#include "map_settings.hpp"
#include "natives.hpp"
#include "remix_vars.hpp"
#include "renderer.hpp"
#include "shared/common/remix_api.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/imgui_helper.hpp"

namespace gta4::menu
{
#pragma region TOOLS

	const ImVec4 COL_BUTTON_ON = ImVec4(0.22f, 0.5f, 0.26f, 1.0f);

	void tools_camera()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();

		ImGui::SeparatorText("Camera");
		ImGui::BeginDisabled(!game::is_in_game);

		float control_width = widgets::row_begin("Screenshot Mode");
		ImGui::Style_ColorButtonPush(im->m_screenshot_mode ? COL_BUTTON_ON : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		if (ImGui::Button(im->m_screenshot_mode ? "Disable##screenshot" : "Enable##screenshot", ImVec2(control_width, 0.0f)))
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
		} TT("Hides the HUD and the radar, and the player if 'Hide Player' is on.");
		ImGui::Style_ColorButtonPop();

		widgets::row_begin("Hide Player");
		widgets::row_align_right(widgets::switch_width());
		widgets::toggle_switch("##hide_player", im->m_screenshot_mode_hide_player);

		// noclip moves the player and is not available in a vehicle
		const auto set_noclip = [im](const bool enable)
			{
				const auto n = natives::get();

				natives::Ped ped;
				n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);

				if (!enable || !n->IsCharSittingInAnyCar(ped))
				{
					im->m_freecam_mode = enable;
					n->SetCharCollision(ped, !enable);
					n->FreezeCharPosition(ped, enable);
				}
			};

		const auto set_freecam = [&](const int type, const bool enable)
			{
				if (type == 0)
				{
					if (im->m_freecam_mode) {
						set_noclip(false);
					}

					free_camera::set_active(enable);
				}
				else
				{
					free_camera::set_active(false);
					set_noclip(enable);
				}
			};

		const bool freecam_active = free_camera::is_active() || im->m_freecam_mode;

		control_width = widgets::row_begin("Free Camera");
		ImGui::Style_ColorButtonPush(freecam_active ? COL_BUTTON_ON : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		if (ImGui::Button(freecam_active ? "Disable##freecam" : "Enable##freecam", ImVec2(control_width, 0.0f))) {
			set_freecam(cs->freecam_type._int(), !freecam_active);
		}

		if (cs->freecam_type._int() == 0)
		{
			TT(	"Enable the FreeCam - The player stays put, works in vehicles and cutscenes\n"
				"Hold the right mouse button to look around and move:\n"
				"WASD: Forward & Strafing\n"
				"Q/E:  Down & Up\n"
				"Shift/Space: Speedup & Slowdown\n"
				"Mouse wheel: Speed\n\n"
				"Switch between FreeCam and Noclip with the mode below.");
		}
		else
		{
			TT(	"Enable Noclip - Moves the player, not available when sitting in a car!\n"
				"WASD: Forward & Strafing\n"
				"Q/E:  Down & Up\n"
				"Shift/Space: Speedup & Slowdown\n\n"
				"Switch between FreeCam and Noclip with the mode below.");
		}
		ImGui::Style_ColorButtonPop();

		// an active camera switches over
		if (rows::combo("Camera Mode", cs->freecam_type, "FreeCam\0Noclip (Traditional)\0") && freecam_active) {
			set_freecam(cs->freecam_type._int(), true);
		}

		if (cs->freecam_type._int() == 0)
		{
			float speed = free_camera::get_speed();
			widgets::row_begin("Speed");
			if (ImGui::SliderFloat("##freecam_speed", &speed, 0.25f, 400.0f, "%.2f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
				free_camera::set_speed(speed);
			} TT("Shared with the FreeCam of the light editor, also changed with the mouse wheel while the right mouse button is held");

			float fov = free_camera::get_fov();
			widgets::row_begin("Field of View");
			if (ImGui::SliderFloat("##freecam_fov", &fov, 5.0f, 150.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
				free_camera::set_fov(fov);
			} TT("Vertical field of view in degrees. The FreeCam ignores the field of view of the game's cameras.");

			float sensitivity = free_camera::get_look_sensitivity();
			widgets::row_begin("Look Sensitivity");
			if (ImGui::SliderFloat("##freecam_sensitivity", &sensitivity, 0.1f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
				free_camera::set_look_sensitivity(sensitivity);
			}
		}
		else
		{
			widgets::row_begin("Forward Speed");
			ImGui::SliderFloat("##noclip_forward", &im->m_freecam_fwd_speed, 0.01f, 10.0f, "%.2f");

			widgets::row_begin("Strafe Speed");
			ImGui::SliderFloat("##noclip_strafe", &im->m_freecam_rt_speed, 0.01f, 10.0f, "%.2f");

			widgets::row_begin("Upward Speed");
			ImGui::SliderFloat("##noclip_up", &im->m_freecam_up_speed, 0.01f, 10.0f, "%.2f");
		}

		ImGui::EndDisabled();
	}

	void tools_game()
	{
		struct weather_button_s
		{
			const char* label;
			const char* game_name;
			std::uint32_t type;
		};

		constexpr weather_button_s WEATHER_BUTTONS[] =
		{
			{ "Extra Sunny", "EXTRASUNNY", game::WEATHER_EXTRASUNNY },
			{ "Sunny", "SUNNY", game::WEATHER_SUNNY },
			{ "Windy", "SUNNY_WINDY", game::WEATHER_SUNNY_WINDY },
			{ "Cloudy", "CLOUDY", game::WEATHER_CLOUDY },
			{ "Rain", "RAIN", game::WEATHER_RAIN },
			{ "Drizzle", "DRIZZLE", game::WEATHER_DRIZZLE },
			{ "Foggy", "FOGGY", game::WEATHER_FOGGY },
			{ "Lightning", "LIGHTNING", game::WEATHER_LIGHTNING },
		};

		constexpr std::size_t WEATHER_BUTTONS_PER_ROW = 4u;

		const auto& im = imgui::get();
		const ImGuiStyle& style = ImGui::GetStyle();

		ImGui::SeparatorText("Game");

		const float step_width = ImGui::GetFrameHeight() + 8.0f;
		widgets::row_begin("Pause Game");
		const float pause_width = widgets::row_split(step_width);

		ImGui::BeginDisabled(!game::is_in_game || !game::ms_bUserPause);
		ImGui::Style_ColorButtonPush(im->m_pause_game ? COL_BUTTON_ON : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		if (ImGui::Button(im->m_pause_game ? ICON_FA_PLAY "  Resume" : ICON_FA_PAUSE "  Pause", ImVec2(pause_width, 0.0f)))
		{
			im->m_pause_game = !im->m_pause_game;
			if (!im->m_pause_game && !(game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive)) {
				*game::ms_bUserPause = 0u;
			}
		} TT("Pause the game like the pause menu does, cutscenes included. The scene and its lights keep rendering, so light overrides can be tweaked on a frozen frame.");
		ImGui::Style_ColorButtonPop();

		widgets::row_second(step_width);
		ImGui::BeginDisabled(!im->m_pause_game);
		ImGui::Button(ICON_FA_STEP_FORWARD "##pause_step", ImVec2(step_width, 0.0f));
		im->m_pause_step_held = ImGui::IsItemActive();
		TT("Click to advance a single frame, hold to let the game run until released");
		ImGui::EndDisabled();
		ImGui::EndDisabled();

		widgets::row_begin("Modify Time");
		widgets::row_align_right(widgets::switch_width());
		if (widgets::toggle_switch("##modify_time", im->m_freeze_time)) {
			*game::m_game_timer_length = im->m_freeze_time ? 9999999 : 2000;
		}

		ImGui::BeginDisabled(!im->m_freeze_time);
		{
			widgets::row_begin("Hour");
			if (ImGui::InputInt("##hour", &im->m_curr_game_hour, 1, 2))
			{
				im->m_curr_game_hour = im->m_curr_game_hour > 23 ? 0 : im->m_curr_game_hour < 0 ? 23 : im->m_curr_game_hour;
				im->m_time_was_changed = true;
			}

			widgets::row_begin("Minutes");
			if (ImGui::InputInt("##minutes", &im->m_curr_game_minute, 1, 4))
			{
				// minutes carry into the hour in both directions
				const int total = im->m_curr_game_hour * 60 + im->m_curr_game_minute;
				const int wrapped = ((total % 1440) + 1440) % 1440;
				im->m_curr_game_hour = wrapped / 60;
				im->m_curr_game_minute = wrapped % 60;
				im->m_time_was_changed = true;
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing(0, 6);
		ImGui::TextDisabled("Weather   (transition %.2f)", *game::weather_change_value);
		ImGui::Spacing(0, 2);

		const float weather_button_width = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * static_cast<float>(WEATHER_BUTTONS_PER_ROW - 1u)) / static_cast<float>(WEATHER_BUTTONS_PER_ROW);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, style.FramePadding.y));
		for (std::size_t i = 0u; i < std::size(WEATHER_BUTTONS); i++)
		{
			if (i % WEATHER_BUTTONS_PER_ROW) {
				ImGui::SameLine();
			}

			if (ImGui::Button(WEATHER_BUTTONS[i].label, ImVec2(weather_button_width, 0.0f)))
			{
				const auto n = natives::get();
				shared::common::remix_api::get().m_bridge.SetGameValue("__weather.blend_absolute", "1.0");
				n->ForceWeatherNow(WEATHER_BUTTONS[i].type);
				n->ReleaseWeather();
			}
			TT(WEATHER_BUTTONS[i].game_name);
		}
		ImGui::PopStyleVar();
	}

	void tools_diagnostics()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();

		ImGui::SeparatorText("Diagnostics");

		const float control_width = widgets::row_begin("Anti Culling");
		ImGui::Style_ColorButtonPush(im->m_anti_cull_capture_toggle ? ImVec4(0.72f, 0.5f, 0.26f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		if (ImGui::Button(im->m_anti_cull_capture_toggle ? "Restore##anticull" : "Disable##anticull", ImVec2(control_width, 0.0f)))
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

		} TT("Quick AntiCull Toggle: quickly toggle all anti culling logic if you want quicker and smaller captures.");
		ImGui::Style_ColorButtonPop();

		if (const auto p = performance_logger::get(); p)
		{
			widgets::row_begin("Performance Overlay");
			widgets::row_align_right(widgets::switch_width());
			widgets::toggle_switch("##performance_overlay", p->m_popout_enabled);
			p->draw_imgui_panel_embedded();
		}
	}

	void pages::tools()
	{
		simple("Tools", ICON_FA_TOOLS, []()
			{
				ImGui::Spacing(0, 4);
				widgets::blocks_begin("##tools_blocks");
				{
					widgets::block_next();
					tools_camera();

					// blocks without settings do not run while the settings index is built
					widgets::block_next();
					if (!is_indexing()) {
						tools_game();
					}

					widgets::block_next();
					if (!is_indexing()) {
						tools_diagnostics();
					}
				}
				widgets::blocks_end();
				ImGui::Spacing(0, 4);
			});
	}

#pragma endregion

#pragma region WORLD_EDITS

	constexpr float EDITOR_TILE_SIZE = 40.0f;
	constexpr float EDITOR_BUTTON_WIDTH = 96.0f;
	constexpr float EDITOR_ACTION_GAP = 6.0f;
	constexpr float EDITOR_HEADER_INSET = 8.0f;

// separates the parts of a status line
#define STATUS_DOT "  \xC2\xB7  "

	float text_button_width(const char* label) {
		return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	void reload_rtx_conf_popup()
	{
		if (!ImGui::BeginPopupModal("Reload RtxConf?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
			return;
		}

		shared::imgui::draw_background_blur();
		ImGui::Spacing(0.0f, 0.0f);

		const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
		const auto line1_str = "This will reload the rtx.conf file and re-apply all of it's variables.  ";
		const auto line3_str = "(excluding texture hashes)";

		ImGui::Spacing();
		ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
		ImGui::TextUnformatted(line1_str);

		ImGui::PushFont(shared::imgui::font::BOLD);
		ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
		ImGui::TextUnformatted(line3_str);
		ImGui::PopFont();

		ImGui::Spacing(0, 8);
		ImGui::Spacing(0, 0);
		ImGui::SameLine();

		const ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
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

	/// Title strip of the section: its title on the left, the reload buttons for the files behind the editors on the right.
	void world_edits_header()
	{
		const char* map_settings_label = ICON_FA_REDO "  Map settings";
		const char* rtx_conf_label = ICON_FA_REDO "  rtx.conf";
		const float buttons_width = text_button_width(map_settings_label) + EDITOR_ACTION_GAP + text_button_width(rtx_conf_label);
		const float right_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - EDITOR_HEADER_INSET;

		ImGui::AlignTextToFramePadding();
		ImGui::PushFont(shared::imgui::font::BOLD);
		ImGui::TextUnformatted("[ World Edits ]");
		ImGui::PopFont();

		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetCursorPosX(right_x - buttons_width);
		if (widgets::quiet_button(map_settings_label)) {
			editor_common::open_reload_map_settings_popup();
		} TT("Reload the map settings: markers, light tweaks, anti cull and lod fade lists.");

		ImGui::SameLine(0.0f, EDITOR_ACTION_GAP);
		if (widgets::quiet_button(rtx_conf_label))
		{
			if (!ImGui::IsPopupOpen("Reload RtxConf?")) {
				ImGui::OpenPopup("Reload RtxConf?");
			}
		} TT("Reload the rtx.conf file and re-apply its variables (excluding texture hashes).");

		editor_common::reload_map_settings_popup();
		reload_rtx_conf_popup();
	}

	/// Starts the row of an editor: icon tile, title and status line. The cursor is left in the actions cell,
	/// placed for frame high items that take 'actions_width' and end at the right edge. Call editor_row_end after the actions.
	void editor_row_begin(const char* icon, const char* title, const char* status, const float actions_width)
	{
		ImGui::TableNextRow();
		ImGui::PushID(title);

		ImGui::TableNextColumn();
		widgets::icon_tile(icon, EDITOR_TILE_SIZE);

		ImGui::TableNextColumn();
		const float text_height = ImGui::GetFontSize() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (EDITOR_TILE_SIZE - text_height) * 0.5f);
		ImGui::PushFont(shared::imgui::font::BOLD);
		ImGui::TextUnformatted(title);
		ImGui::PopFont();
		ImGui::TextDisabled("%s", status);

		ImGui::TableNextColumn();
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - actions_width, 0.0f),
			ImGui::GetCursorPosY() + (EDITOR_TILE_SIZE - ImGui::GetFrameHeight()) * 0.5f));
	}

	void editor_row_end() {
		ImGui::PopID();
	}

	bool editor_launcher_button(const bool is_open, const char* tooltip) {
		return shared::imgui::editor::launcher_button(is_open ? "Close" : "Open", is_open, ImVec2(EDITOR_BUTTON_WIDTH, 0.0f), tooltip);
	}

	void marker_editor_row()
	{
		auto spawning = 0u;
		auto hidden = 0u;
		auto overridden = 0u;

		for (const auto& m : map_settings::get_map_settings().map_markers)
		{
			if (m.internal__is_overridden)
			{
				overridden++;
				continue;
			}

			spawning++;
			hidden += m.internal__is_hidden ? 1u : 0u;
		}

		editor_row_begin(ICON_FA_MAP_MARKER_ALT, "Marker Editor",
			shared::utils::va("%u markers" STATUS_DOT "%u hidden" STATUS_DOT "%u overridden" STATUS_DOT "saves to file", spawning, hidden, overridden), EDITOR_BUTTON_WIDTH);

		if (editor_launcher_button(marker_editor::is_open(),
			"Free floating editor for the map markers.\n"
			"It stays open when this menu is closed (F4) and saves to 'rtx_comp/markers/'.\n"
			"Overridden markers do not spawn, a file with a higher priority uses their number."))
		{
			marker_editor::toggle();
		}

		editor_row_end();
	}

	void light_editor_row()
	{
		const auto& ms = map_settings::get_map_settings();

		editor_row_begin(ICON_FA_LIGHTBULB, "Light Editor",
			shared::utils::va("%u overrides" STATUS_DOT "%u ignored" STATUS_DOT "saves to file", static_cast<std::uint32_t>(ms.light_overrides.size()),
				static_cast<std::uint32_t>(ms.ignored_lights.size())), EDITOR_BUTTON_WIDTH);

		if (editor_launcher_button(light_editor::is_open(),
			"Free floating editor for light overrides and ignored / allowed lights.\n"
			"It stays open when this menu is closed (F4) and saves to 'rtx_comp/light_tweaks/'."))
		{
			light_editor::toggle();
		}

		editor_row_end();
	}

	void mesh_inspector_row(const float actions_width)
	{
		const auto& ms = map_settings::get_map_settings();

		std::size_t index_count = 0u;
		for (const auto& cat : ms.anticull_meshes) {
			index_count += cat.indices.size();
		}

		editor_row_begin(ICON_FA_EYE, "Mesh Inspector",
			shared::utils::va("%u categories" STATUS_DOT "%u indices" STATUS_DOT "%u LOD fade exclusions" STATUS_DOT "clipboard only", static_cast<std::uint32_t>(ms.anticull_meshes.size()),
				static_cast<std::uint32_t>(index_count), static_cast<std::uint32_t>(ms.lod_fade_excluded.size())), actions_width);

		const ImVec4* copy_color = mesh_inspector::has_uncopied_changes() ? &shared::imgui::editor::COL_BUTTON_UNSAVED : nullptr;

		if (widgets::quiet_button("Copy anticull", copy_color)) {
			mesh_inspector::copy_anticull_to_clipboard();
		} TT("Copies the ANTICULL array to the clipboard, paste it into map_settings.toml.\nOrange: changed since the last copy, a reload drops the changes.");

		ImGui::SameLine(0.0f, EDITOR_ACTION_GAP);
		if (widgets::quiet_button("Copy LOD fade", copy_color)) {
			mesh_inspector::copy_lod_fade_exclusions_to_clipboard();
		} TT("Copies the LOD_FADE_EXCLUDE array to the clipboard, paste it into map_settings.toml.\nOrange: changed since the last copy, a reload drops the changes.");

		ImGui::SameLine(0.0f, EDITOR_ACTION_GAP);
		if (editor_launcher_button(mesh_inspector::is_open(),
			"Free floating editor for the anti cull categories and the lod fade exclusions.\n"
			"Lists the building meshes around the camera, traces why a mesh is culled and hides meshes.\n"
			"It stays open when this menu is closed (F4). It does not save, export with the copy buttons."))
		{
			mesh_inspector::toggle();
		}

		editor_row_end();
	}

	void pages::world_edits()
	{
		const auto im = imgui::get();

		widgets::section_begin(im->ImGuiCol_ContainerBackground, im->ImGuiCol_ContainerBorder);
		world_edits_header();

		if (widgets::section_body("##world_edits"))
		{
			const float mesh_actions_width = text_button_width("Copy anticull") + text_button_width("Copy LOD fade") + EDITOR_BUTTON_WIDTH + EDITOR_ACTION_GAP * 2.0f;

			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 10.0f));
			ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImGui::GetColorU32(ImGuiCol_Separator, 0.5f));
			if (ImGui::BeginTable("##editors", 3, ImGuiTableFlags_BordersInnerH))
			{
				ImGui::TableSetupColumn("##tile", ImGuiTableColumnFlags_WidthFixed, EDITOR_TILE_SIZE);
				ImGui::TableSetupColumn("##text", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, mesh_actions_width);

				marker_editor_row();
				light_editor_row();
				mesh_inspector_row(mesh_actions_width);

				ImGui::EndTable();
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		widgets::section_end();
	}

#pragma endregion

#pragma region ABOUT

	void pages::about()
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
		CENTER_URL("Tracy", "https://github.com/wolfpld/tracy");
		CENTER_URL("Pocketfft", "https://github.com/mreineck/pocketfft");

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
		CENTER_URL("Budgie (Vegetation)", "https://www.patreon.com/c/BudgieGames");
		CENTER_URL("Alex from Digital Foundry", "https://www.youtube.com/watch?v=vGxPdcMQfwg");
		CENTER_URL("Sparkles (Remix Plus - Numos - Initial DLSS5 Integration)", "https://github.com/Kim2091");
		CENTER_URL("CR (Remix Plus)", "https://github.com/sambow23");
		CENTER_URL("TheGreatHMMMM (Remix Plus - HDR Impl.)", "https://github.com/TheGreatHMMMM");
		CENTER_URL("Gokuwashere (Remix Plus)", "https://github.com/BrunchyChineapple");
		CENTER_URL("Deezy (Dynamic Water - Rasterized Phone Contributions)", "https://github.com/itsdeeezy");

		ImGui::Spacing(0.0f, 24.0f);
		ImGui::CenterText("And of course, all my fellow Ko-Fi and Patreon supporters");
		ImGui::CenterText("and all the people that helped along the way.");
		ImGui::Spacing(0.0f, 4.0f);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::CenterText("Thank you!");
		ImGui::PopFont();
	}

#pragma endregion
}
