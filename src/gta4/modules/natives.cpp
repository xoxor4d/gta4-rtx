#include "std_include.hpp"
#include "natives.hpp"

#include "discord.hpp"
#include "imgui.hpp"
#include "remix_vars.hpp"

namespace gta4
{
	void on_cgame_process_hk()
	{
		const auto im = imgui::get();
		const auto n = natives::get();

		// TODO: hack to fix issues with SSS-fixing commit (that makes head mesh invisible)
		// + latest remix versions also make player char invisible
		// + sometimes even parts of the world ...

		static uint32_t frame_counter = 0u;
		if (static auto first_frame = false; !first_frame)
		{
			first_frame = true;
			const auto vars = remix_vars::get();

			remix_vars::option_value on { .enabled = true };
			remix_vars::option_value off { .enabled = false };

			if (const auto useVertexCapture = remix_vars::get_option("rtx.useVertexCapture"); useVertexCapture)
			{
				vars->add_queue_entry(useVertexCapture, off, 0.1f);
				vars->add_queue_entry(useVertexCapture, on, 0.5f);
				//vars->add_interpolate_entry(useVertexCapture, off, 1.5f);
				//vars->add_interpolate_entry(useVertexCapture, on, 2.5f);
				//vars->add_interpolate_entry(useVertexCapture, on, 3.5f);
			}

			im->m_dbg_do_not_render_ff = true;
		}
		else if (static auto second = false; !second)
		{
			++frame_counter;
			if (frame_counter > 30)
			{
				second = true;
				im->m_dbg_do_not_render_ff = false;
			}
		}

		discord::update_discord();

		// ----

		if (shared::globals::imgui_menu_open || im->m_freeze_time)
		{
			if (!im->m_freeze_time)
			{
				uint32_t h_from_game = 0u, m_from_game = 0u;
				n->GetTimeOfDay(&h_from_game, &m_from_game);

				im->m_curr_game_hour = static_cast<int>(h_from_game);
				im->m_curr_game_minute = static_cast<int>(m_from_game);
			}

			if (im->m_freeze_time)
			{
				n->SetTimeOfDay(im->m_curr_game_hour, im->m_curr_game_minute);
				*game::m_game_clock_hours = static_cast<uint8_t>(im->m_curr_game_hour);
				*game::m_game_clock_minutes = static_cast<uint8_t>(im->m_curr_game_minute);
				*game::m_game_clock_seconds = 0u;
				*game::m_game_timer_length = 9999999;
			}
			else if (im->m_time_was_changed)
			{
				//n->ReleaseWeather();
				im->m_time_was_changed = false;
			}
		}

		if (im->m_freecam_mode)
		{
			natives::Ped ped;
			n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);

			if (n->IsCharSittingInAnyCar(ped))
			{
				im->m_freecam_mode = false;
				n->SetCharCollision(ped, true);
				n->FreezeCharPosition(ped, false);
			}
			else
			{
				const bool pressed_w = ImGui::IsKeyDown(ImGuiKey_W);
				const bool pressed_a = ImGui::IsKeyDown(ImGuiKey_A);
				const bool pressed_s = ImGui::IsKeyDown(ImGuiKey_S);
				const bool pressed_d = ImGui::IsKeyDown(ImGuiKey_D);
				const bool pressed_e = ImGui::IsKeyDown(ImGuiKey_E);
				const bool pressed_q = ImGui::IsKeyDown(ImGuiKey_Q);
				const bool pressed_shift = ImGui::IsKeyDown(ImGuiKey_LeftShift);
				const bool pressed_space = ImGui::IsKeyDown(ImGuiKey_Space);

				const float forward_speed = im->m_freecam_fwd_speed * (pressed_shift ? 2.0f : pressed_space ? 0.25f : 1.0f);
				const float strafe_speed = im->m_freecam_rt_speed * (pressed_shift ? 2.0f : pressed_space ? 0.25f : 1.0f);
				const float upward_speed = im->m_freecam_up_speed * (pressed_shift ? 2.0f : pressed_space ? 0.25f : 1.0f);

				Vector offset;

				if (!(pressed_w && pressed_s))
				{
					if (pressed_w) {
						offset.y = forward_speed;
					}
					else if (pressed_s) {
						offset.y = -forward_speed;
					}
				}

				if (!(pressed_d && pressed_a))
				{
					if (pressed_d) {
						offset.x = strafe_speed;
					}
					else if (pressed_a) {
						offset.x -= strafe_speed;
					}
				}

				if (!(pressed_e && pressed_q))
				{
					if (pressed_e) {
						offset.z = upward_speed;
					}
					else if (pressed_q) {
						offset.z = -upward_speed;
					}
				}

				natives::Camera cam;
				n->GetGameCam(&cam);

				Vector cam_rotation;
				n->GetCamRot(cam, &cam_rotation.x, &cam_rotation.y, &cam_rotation.z);

				offset.z += offset.y * sinf(cam_rotation.x * 0.01745329252f);
				//offset.z -= im->m_freecam_up_offset;

				Vector new_player_pos;
				n->GetOffsetFromCharInWorldCoords(ped, offset.x, offset.y, offset.z, &new_player_pos.x, &new_player_pos.y, &new_player_pos.z);
				n->SetCharCoordinatesNoOffset(ped, new_player_pos.x, new_player_pos.y, new_player_pos.z);

				const float new_heading = fmodf(cam_rotation.z + 360.0f, 360.0f);
				n->SetCharHeading(ped, new_heading);
			}
		}
	}

	natives::natives()
	{
		p_this = this;

		shared::utils::hook(game::hk_addr__on_cgame_process_hk, on_cgame_process_hk, HOOK_CALL).install()->quick(); // 0x59D79F

		// -----
		m_initialized = true;
		shared::common::log("Natives", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
