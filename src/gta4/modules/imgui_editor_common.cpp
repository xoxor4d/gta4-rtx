#include "std_include.hpp"
#include "imgui_editor_common.hpp"

#include "free_camera.hpp"
#include "imgui.hpp"
#include "imgui_light_editor.hpp"
#include "imgui_marker_editor.hpp"
#include "imgui_mesh_inspector.hpp"
#include "map_settings.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/imgui_helper.hpp"

namespace gta4::editor_common
{
	using shared::imgui::editor::tool_button;

	constexpr const char* RELOAD_POPUP_NAME = "Reload MapSettings?";

	void pause_buttons(bool& step_held, const char* pause_tooltip)
	{
		const auto im = imgui::get();

		ImGui::BeginDisabled(!game::is_in_game || !game::ms_bUserPause);
		if (tool_button(im->m_pause_game ? ICON_FA_PLAY "##pause" : ICON_FA_PAUSE "##pause", pause_tooltip, im->m_pause_game))
		{
			im->m_pause_game = !im->m_pause_game;
			if (!im->m_pause_game && !(game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive)) {
				*game::ms_bUserPause = 0u;
			}
		}

		ImGui::BeginDisabled(!im->m_pause_game);
		tool_button(ICON_FA_STEP_FORWARD "##step", "Click to advance a single frame, hold to let the game run until released");
		ImGui::SameLine(0.0f, 0.0f);

		// the button above is the last item (tool_button ends with SameLine)
		const bool held = ImGui::IsItemActive();
		if (held || step_held) {
			im->m_pause_step_held = held;
		}
		step_held = held;

		ImGui::EndDisabled();
		ImGui::EndDisabled();
		ImGui::SameLine(0.0f, 4.0f);
	}

	void camera_buttons(bool& rmb_game_input)
	{
		const bool freecam = free_camera::is_active();
		if (tool_button(ICON_FA_VIDEO "##freecam",
			"Free camera, the player stays put\n"
			"Hold the right mouse button to look around and move:\n"
			"WASD: Forward & Strafing\n"
			"Q/E:  Down & Up\n"
			"Shift/Space: Speedup & Slowdown\n"
			"Mouse wheel: Speed", freecam))
		{
			free_camera::set_active(!freecam);
		}

		ImGui::BeginDisabled(freecam);
		if (tool_button(ICON_FA_GAMEPAD "##rmb_input", "Hold the right mouse button over the world to hand mouse and keyboard to the game", rmb_game_input && !freecam)) {
			rmb_game_input = !rmb_game_input;
		}
		ImGui::EndDisabled();

		if (freecam && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
		{
			ImGui::SetTooltip("%s",
				"Hold the right mouse button over the world to hand mouse and keyboard to the game\n"
				"Not available while the free camera is on: it uses the right mouse button itself. Turn the free camera off first.");
		}

		ImGui::SameLine(0.0f, 4.0f);
	}

	void camera_speed_slider(const float width)
	{
		float speed = free_camera::get_speed();
		ImGui::SetNextItemWidth(width);
		if (ImGui::SliderFloat("##camera_speed", &speed, 0.25f, 400.0f, "Camera speed  %.1f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
			free_camera::set_speed(speed);
		}
		ImGui::SetItemTooltipBlur("Speed of the free camera, also changed with the mouse wheel while the right mouse button is held");
	}

	void open_reload_map_settings_popup()
	{
		if (!ImGui::IsPopupOpen(RELOAD_POPUP_NAME)) {
			ImGui::OpenPopup(RELOAD_POPUP_NAME);
		}
	}

	bool reload_map_settings_popup()
	{
		static bool popup_rendered_this_frame = false;
		static int last_frame_count = -1;

		if (const int current_frame = ImGui::GetFrameCount(); current_frame != last_frame_count)
		{
			popup_rendered_this_frame = false;
			last_frame_count = current_frame;
		}

		if (popup_rendered_this_frame) {
			return false;
		}

		bool result = false;
		if (ImGui::BeginPopupModal(RELOAD_POPUP_NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			popup_rendered_this_frame = true;
			shared::imgui::draw_background_blur();
			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			const auto line1_str = "You'll loose all unsaved changes if you continue!";
			const auto line2_str = light_editor::has_unsaved_changes() ? "The light editor has unsaved changes. Save them in its toolbar first.  "
				: marker_editor::has_unsaved_changes() ? "The marker editor has unsaved changes. Save them in its toolbar first.  "
				: "Use the copy to clipboard buttons and manually update  ";
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
			ImGui::Spacing(0, 0);
			ImGui::SameLine();

			ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				result = true;
				map_settings::load_settings();
				light_editor::on_map_settings_reloaded();
				marker_editor::on_map_settings_reloaded();
				mesh_inspector::on_map_settings_reloaded();
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
}
