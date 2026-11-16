#include "std_include.hpp"
#include "free_camera.hpp"

#include <algorithm>
#include <mutex>

#include "shared/common/free_camera_controller.hpp"

namespace gta4::free_camera
{
	namespace
	{
		namespace controller = shared::common::free_camera;

		std::mutex g_mtx;
		controller::state_s g;

		// render thread
		controller::look_s g_look;

		// game thread
		using set_camera_mtx_t = void(__fastcall*)(game::grcViewport* viewport, void* edx, const float* matrix);
		set_camera_mtx_t g_set_camera_mtx_og = nullptr;
		game::grcViewport* g_viewport = nullptr;
		bool g_applied_this_frame = false;

		using perspective_t = int(__fastcall*)(void* viewport_manager, void* edx, game::grcViewport* viewport, float fov, float aspect, float near_clip, float far_clip);
		perspective_t g_perspective_og = nullptr;

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
						controller::init_from_matrix(g, game_matrix);
					}

					controller::build_matrix(g, matrix);
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
		g.speed = std::clamp(speed, controller::SPEED_MIN, controller::SPEED_MAX);
	}

	void focus(const Vector& target, const float distance)
	{
		std::lock_guard lock(g_mtx);
		if (!g.active || !g.initialized) {
			return;
		}

		const Vector forward = controller::forward_of(g);
		g.pos.x = target.x - forward.x * distance;
		g.pos.y = target.y - forward.y * distance;
		g.pos.z = target.z - forward.z * distance;
	}

	bool is_looking() {
		return g_look.looking;
	}

	void update_input()
	{
		float dx = 0.0f;
		float dy = 0.0f;

		if (!controller::update_look(g_look, !is_game_menu_open(), dx, dy)) {
			return;
		}

		std::lock_guard lock(g_mtx);
		if (!g.active || !g.initialized) {
			return;
		}

		controller::apply_input(g, dx, dy);
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
						controller::init_from_matrix(g, &g_viewport->cameraInv.m[0][0]);
					}

					controller::build_matrix(g, matrix);
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
