#include "std_include.hpp"
#include "free_camera_controller.hpp"

#include <algorithm>
#include "imgui.h"

namespace shared::common::free_camera
{
	namespace
	{
		constexpr float LOOK_SENSITIVITY = 0.0025f;		// radians per pixel
		constexpr float PITCH_LIMIT = 1.553f;			// 89 degrees
	}

	Vector forward_of(const state_s& s) {
		return { cosf(s.pitch) * sinf(s.yaw), cosf(s.pitch) * cosf(s.yaw), sinf(s.pitch) };
	}

	Vector right_of(const state_s& s) {
		return { cosf(s.yaw), -sinf(s.yaw), 0.0f };
	}

	void init_from_matrix(state_s& s, const float* m)
	{
		Vector fwd(-m[8], -m[9], -m[10]);
		fwd.Normalize();

		s.pos = Vector(m[12], m[13], m[14]);
		s.yaw = atan2f(fwd.x, fwd.y);
		s.pitch = asinf(std::clamp(fwd.z, -1.0f, 1.0f));
		s.initialized = true;
	}

	void build_matrix(const state_s& s, float* m)
	{
		const Vector fwd = forward_of(s);
		const Vector right = right_of(s);
		const Vector up = right.Cross(fwd);

		m[0] = right.x;	m[1] = right.y;	m[2] = right.z;		m[3] = 0.0f;
		m[4] = up.x;	m[5] = up.y;	m[6] = up.z;		m[7] = 0.0f;
		m[8] = -fwd.x;	m[9] = -fwd.y;	m[10] = -fwd.z;		m[11] = 0.0f;
		m[12] = s.pos.x; m[13] = s.pos.y; m[14] = s.pos.z;	m[15] = 1.0f;
	}

	bool update_look(look_s& look, const bool allowed, float& dx, float& dy)
	{
		auto& io = ImGui::GetIO();

		const bool rmb = io.MouseDown[ImGuiMouseButton_Right] && (look.looking || !io.WantCaptureMouse) && allowed;
		if (rmb && !look.looking)
		{
			GetCursorPos(&look.anchor);
			look.looking = true;
		}
		else if (!rmb) {
			look.looking = false;
		}

		if (!look.looking) {
			return false;
		}

		// the cursor stays where the look started, so it never runs into a screen edge
		POINT cursor;
		GetCursorPos(&cursor);
		SetCursorPos(look.anchor.x, look.anchor.y);
		io.MouseDrawCursor = false;

		dx = static_cast<float>(cursor.x - look.anchor.x);
		dy = static_cast<float>(cursor.y - look.anchor.y);
		return true;
	}

	void apply_input(state_s& s, const float dx, const float dy)
	{
		const auto& io = ImGui::GetIO();

		s.yaw += dx * LOOK_SENSITIVITY * s.look_sensitivity;
		s.pitch = std::clamp(s.pitch - dy * LOOK_SENSITIVITY * s.look_sensitivity, -PITCH_LIMIT, PITCH_LIMIT);

		if (io.MouseWheel != 0.0f) {
			s.speed = std::clamp(s.speed * powf(1.2f, io.MouseWheel), SPEED_MIN, SPEED_MAX);
		}

		const float step = s.speed * io.DeltaTime * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : ImGui::IsKeyDown(ImGuiKey_Space) ? 0.25f : 1.0f);
		const auto axis = [](const ImGuiKey positive, const ImGuiKey negative) {
			return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
		};

		s.pos += forward_of(s) * (axis(ImGuiKey_W, ImGuiKey_S) * step);
		s.pos += right_of(s) * (axis(ImGuiKey_D, ImGuiKey_A) * step);
		s.pos.z += axis(ImGuiKey_E, ImGuiKey_Q) * step;
	}
}
