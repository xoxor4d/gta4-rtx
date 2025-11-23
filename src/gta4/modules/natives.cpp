#include "std_include.hpp"
#include "natives.hpp"
#include "imgui.hpp"

namespace gta4
{
	void on_cgame_process_hk()
	{
		const auto im = imgui::get();

		if (im->m_freecam_mode)
		{
			const auto n = natives::get();

			natives::Ped ped;
			n->GetPlayerChar(n->GetPlayerId(), &ped);

			if (n->IsCharSittingInAnyCar(ped))
			{
				im->m_freecam_mode = false;
				n->SetCharCollision(ped, true);
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
				offset.z -= im->m_freecam_up_offset;

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

		shared::utils::hook(game::hk_addr__on_cgame_process_hk, on_cgame_process_hk, HOOK_CALL).install()->quick();

		// -----
		m_initialized = true;
		shared::common::log("Natives", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
