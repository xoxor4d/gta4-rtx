#include "std_include.hpp"
#include "free_camera.hpp"

#include <algorithm>
#include <mutex>

namespace gta4::free_camera
{
	namespace
	{
		constexpr float LOOK_SENSITIVITY = 0.0025f;		// radians per pixel
		constexpr float PITCH_LIMIT = 1.553f;			// 89 degrees
		constexpr float SPEED_MIN = 0.25f;
		constexpr float SPEED_MAX = 400.0f;

		struct state_s
		{
			bool active = false;
			bool initialized = false;		// position and angles were taken from the game's camera
			Vector pos;
			float yaw = 0.0f;				// 0 looks along +y, positive turns towards +x
			float pitch = 0.0f;
			float speed = 8.0f;
			float fov = 50.0f;
			float look_sensitivity = 1.0f;
		};

		std::mutex g_mtx;
		state_s g;

		// render thread
		bool g_looking = false;
		POINT g_look_anchor = {};

		// game thread
		using set_camera_mtx_t = void(__fastcall*)(game::grcViewport* viewport, void* edx, const float* matrix);
		set_camera_mtx_t g_set_camera_mtx_og = nullptr;
		game::grcViewport* g_viewport = nullptr;
		bool g_applied_this_frame = false;

		using perspective_t = int(__fastcall*)(void* viewport_manager, void* edx, game::grcViewport* viewport, float fov, float aspect, float near_clip, float far_clip);
		perspective_t g_perspective_og = nullptr;

		Vector forward_of(const state_s& s) {
			return { cosf(s.pitch) * sinf(s.yaw), cosf(s.pitch) * cosf(s.yaw), sinf(s.pitch) };
		}

		Vector right_of(const state_s& s) {
			return { cosf(s.yaw), -sinf(s.yaw), 0.0f };
		}

		/// rage camera matrix (rows: right, up, back, position), 'm' is the matrix of the game's camera
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

		/// the game's scene viewport, other viewports (radar ..) have their own CCamFinal
		bool is_scene_viewport(const game::grcViewport* viewport)
		{
			const auto viewports = game::pViewports;
			return viewports && viewports->sceneviewport
				&& reinterpret_cast<const std::uint8_t*>(viewport) == reinterpret_cast<const std::uint8_t*>(viewports->sceneviewport) + 0x10u;
		}

		/// the camera steps aside while the game's pause menu is open
		bool is_game_menu_open() {
			return game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive;
		}

		/// replaces grcViewport::SetCameraMtx in CCamFinal::InformRenderEngineOfFinalFrame
		void __fastcall set_camera_mtx_hk(game::grcViewport* viewport, void* edx, const float* game_matrix)
		{
			alignas(16) float matrix[16];
			bool use_free_camera = false;

			if (is_scene_viewport(viewport))
			{
				std::lock_guard lock(g_mtx);
				if (g.active && !is_game_menu_open())
				{
					if (!g.initialized) {
						init_from_matrix(g, game_matrix);
					}

					build_matrix(g, matrix);
					use_free_camera = true;
				}

				g_viewport = viewport;
				g_applied_this_frame = true;
			}

			g_set_camera_mtx_og(viewport, edx, use_free_camera ? matrix : game_matrix);
		}
	}

	namespace
	{
		/// replaces the perspective call in CCamFinal::InformRenderEngineOfFinalFrame
		int __fastcall perspective_hk(void* viewport_manager, void* edx, game::grcViewport* viewport, float fov, const float aspect, const float near_clip, const float far_clip)
		{
			if (is_scene_viewport(viewport))
			{
				std::lock_guard lock(g_mtx);
				if (g.active && !is_game_menu_open()) {
					fov = g.fov;
				}
			}

			return g_perspective_og(viewport_manager, edx, viewport, fov, aspect, near_clip, far_clip);
		}
	}

	bool is_active()
	{
		std::lock_guard lock(g_mtx);
		return g.active;
	}

	float get_fov()
	{
		std::lock_guard lock(g_mtx);
		return g.fov;
	}

	void set_fov(const float fov)
	{
		std::lock_guard lock(g_mtx);
		g.fov = std::clamp(fov, 5.0f, 150.0f);
	}

	void set_active(const bool active)
	{
		std::lock_guard lock(g_mtx);
		if (g.active != active)
		{
			g.active = active;
			g.initialized = false;
			shared::globals::free_camera_active = active;
		}
	}

	float get_look_sensitivity()
	{
		std::lock_guard lock(g_mtx);
		return g.look_sensitivity;
	}

	void set_look_sensitivity(const float sensitivity)
	{
		std::lock_guard lock(g_mtx);
		g.look_sensitivity = std::clamp(sensitivity, 0.1f, 5.0f);
	}

	float get_speed()
	{
		std::lock_guard lock(g_mtx);
		return g.speed;
	}

	void set_speed(const float speed)
	{
		std::lock_guard lock(g_mtx);
		g.speed = std::clamp(speed, SPEED_MIN, SPEED_MAX);
	}

	bool is_looking() {
		return g_looking;
	}

	void update_input()
	{
		auto& io = ImGui::GetIO();

		// the look starts over the world and lasts until the button is released
		const bool rmb = io.MouseDown[ImGuiMouseButton_Right] && (g_looking || !io.WantCaptureMouse) && !is_game_menu_open();
		if (rmb && !g_looking)
		{
			GetCursorPos(&g_look_anchor);
			g_looking = true;
		} 
		else if (!rmb) {
			g_looking = false;
		}

		if (!g_looking) {
			return;
		}

		// the cursor stays where the look started, so it never runs into a screen edge
		POINT cursor;
		GetCursorPos(&cursor);
		SetCursorPos(g_look_anchor.x, g_look_anchor.y);
		io.MouseDrawCursor = false;

		const float dx = static_cast<float>(cursor.x - g_look_anchor.x);
		const float dy = static_cast<float>(cursor.y - g_look_anchor.y);

		std::lock_guard lock(g_mtx);
		if (!g.active || !g.initialized) {
			return;
		}

		g.yaw += dx * LOOK_SENSITIVITY * g.look_sensitivity;
		g.pitch = std::clamp(g.pitch - dy * LOOK_SENSITIVITY * g.look_sensitivity, -PITCH_LIMIT, PITCH_LIMIT);

		if (io.MouseWheel != 0.0f) {
			g.speed = std::clamp(g.speed * powf(1.2f, io.MouseWheel), SPEED_MIN, SPEED_MAX);
		}

		const float step = g.speed * io.DeltaTime * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : ImGui::IsKeyDown(ImGuiKey_Space) ? 0.25f : 1.0f);
		const auto axis = [](const ImGuiKey positive, const ImGuiKey negative) {
			return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
		};

		g.pos += forward_of(g) * (axis(ImGuiKey_W, ImGuiKey_S) * step);
		g.pos += right_of(g) * (axis(ImGuiKey_D, ImGuiKey_A) * step);
		g.pos.z += axis(ImGuiKey_E, ImGuiKey_Q) * step;
	}

	void on_game_frame()
	{
		// the game skips its camera update while it is paused, the viewport then keeps the last matrix
		if (!g_applied_this_frame && g_viewport && g_set_camera_mtx_og)
		{
			alignas(16) float matrix[16];
			bool use_free_camera = false;

			{
				std::lock_guard lock(g_mtx);
				if (g.active && !is_game_menu_open())
				{
					if (!g.initialized) {
						init_from_matrix(g, &g_viewport->cameraInv.m[0][0]);
					}

					build_matrix(g, matrix);
					use_free_camera = true;
				}
			}

			if (use_free_camera) {
				g_set_camera_mtx_og(g_viewport, nullptr, matrix);
			}
		}

		g_applied_this_frame = false;
	}

	void install()
	{
		if (!game::hk_addr__camfinal_set_camera_mtx) {
			return;
		}

		g_set_camera_mtx_og = reinterpret_cast<set_camera_mtx_t>(shared::utils::mem::resolve_relative_call_address(game::hk_addr__camfinal_set_camera_mtx));
		shared::utils::hook(game::hk_addr__camfinal_set_camera_mtx, set_camera_mtx_hk, HOOK_CALL).install()->quick(); // 0xC462FE

		if (game::hk_addr__camfinal_perspective)
		{
			g_perspective_og = reinterpret_cast<perspective_t>(shared::utils::mem::resolve_relative_call_address(game::hk_addr__camfinal_perspective));
			shared::utils::hook(game::hk_addr__camfinal_perspective, perspective_hk, HOOK_CALL).install()->quick(); // 0xC463AB
		}
	}
}
