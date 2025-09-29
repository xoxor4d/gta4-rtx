#pragma once
#include "bridge_remix_api.h"
#include "remix/remix_c.h"

namespace shared::common
{
	class remix_api
	{
	public:
		// enforce singleton pattern
		remix_api(const remix_api&) = delete;
		remix_api& operator=(const remix_api&) = delete;

		static remix_api& get();

		static void begin_scene_callback_internal();
		static void end_scene_callback_internal();
		static void present_callback_internal();


		static constexpr std::uint32_t M_MAX_DEBUG_LINES = 512u;
		enum DEBUG_REMIX_LINE_COLOR
		{
			RED = 0u,
			GREEN = 1u,
			TEAL = 2u,
			WHITE = 3u,
		};

		void init_debug_lines();
		void create_quad(remixapi_HardcodedVertex* v_out, uint32_t* i_out, float scale);
		void create_line_quad(remixapi_HardcodedVertex* v_out, uint32_t* i_out, const Vector& p1, const Vector& p2, float width);
		void add_debug_line(const Vector& p1, const Vector& p2, float width, DEBUG_REMIX_LINE_COLOR color);

		void add_debug_circle_based_on_previous(const Vector& center, const Vector& rot, const Vector& scale);
		void add_debug_circle(const Vector& center, const Vector& normal, float radius, float thickness, const Vector& color, bool drawcall_alpha = true);

		static bool can_add_debug_lines() { return get().m_debug_line_amount + 1u < M_MAX_DEBUG_LINES; }

		void debug_draw_box(const Vector& mins, const Vector& maxs, float line_width, const DEBUG_REMIX_LINE_COLOR& color);
		void debug_draw_box(const VectorAligned& center, const VectorAligned& half_diagonal, float line_width, const DEBUG_REMIX_LINE_COLOR& color);

		static void initialize(
			PFN_remixapi_BridgeCallback begin_scene_callback,
			PFN_remixapi_BridgeCallback end_scene_callback,
			PFN_remixapi_BridgeCallback present_callback,
			bool is_asi = false);

		static bool is_initialized() { return get().m_initialized; }

		PFN_remixapi_BridgeCallback begin_scene_callback_external = nullptr;
		PFN_remixapi_BridgeCallback end_scene_callback_external = nullptr;
		PFN_remixapi_BridgeCallback present_callback_external = nullptr;

		remixapi_Interface m_bridge;

		struct flashlight_def_s
		{
			Vector pos = {};
			Vector fwd = { 0.0f, 1.0f, 0.0f };
			Vector rt = {};
			Vector up = {};
			Vector offset = {};
			float radius = 4.0f;
			float angle = 30.0f;
			float softness = 0.1f;
			float expo = 0.0f;
			float intensity = 4.0f;
		};

		struct flashlight_s
		{
			remixapi_LightHandle handle = nullptr;
			remixapi_LightInfoSphereEXT ext = {};
			remixapi_LightInfo info = {};
			flashlight_def_s def = {};
			bool is_player = false;
			bool is_enabled = false;
		};
		std::unordered_map<std::string, flashlight_s> m_flashlights;

		void flashlight_create_or_update(const char* player_name, flashlight_def_s& def, bool is_enabled, bool is_player = false);
		void flashlight_frame();

	private:
		remix_api() : m_initialized(false) {}
		bool m_initialized;


		bool m_debug_lines_initialized = false;
		remixapi_MaterialHandle m_debug_line_materials[4];
		remixapi_MeshHandle m_debug_line_list[M_MAX_DEBUG_LINES];
		std::uint32_t m_debug_line_amount = 0u;
		std::uint64_t m_debug_last_line_hash = 0u;

		struct dbg_circle
		{
			remixapi_MeshHandle handle = nullptr;
			remixapi_Transform transform = {};
			bool uses_custom_transform = false;
		};
		std::vector<dbg_circle> m_debug_circles;
		std::uint64_t m_debug_circles_last_hash = 0u;
		std::vector<remixapi_MaterialHandle> m_debug_circle_materials;
	};
}