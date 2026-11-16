#include "std_include.hpp"
#include "imgui_editor_common.hpp"

#include "free_camera.hpp"
#include "imgui.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/font_defines.hpp"

namespace gta4::editor_common
{
	using shared::imgui::editor::tool_button;

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
}
