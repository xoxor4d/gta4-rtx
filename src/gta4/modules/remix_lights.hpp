#pragma once

namespace gta4
{
	class remix_lights final : public shared::common::loader::component_module
	{
	public:
		remix_lights();

		static inline remix_lights* p_this = nullptr;
		static remix_lights* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static void on_client_frame();
		static void on_map_load();

	public:
		// -----

		struct remix_light_def
		{
			remixapi_LightHandle m_handle = nullptr;
			remixapi_LightInfoSphereEXT m_ext = {};
			remixapi_LightInfo m_info = {};
			game::CLightSource m_def = {};
			uint64_t m_hash;
			std::uint32_t m_light_num = 0u;
			std::uint32_t m_updateframe = 0u;
			bool m_is_ignored = false;
		};

		struct remix_distant_light_def
		{
			remixapi_LightHandle m_handle = nullptr;
			remixapi_LightInfoDistantEXT m_ext = {};
			remixapi_LightInfo m_info = {};
			//game::CDirectionalLight m_def = {};
			uint64_t m_hash;
			std::uint32_t m_updateframe = 0u;
		};

		bool spawn_or_update_remix_sphere_light(remix_light_def& light, bool update = false);

		void add_light(const game::CLightSource& def, const uint64_t& hash, bool add_but_do_not_draw = false);
		void destroy_light(remix_light_def& light);

		void destroy_all_lights();
		void destroy_and_clear_all_active_lights();
		void iterate_all_game_lights();
		void draw_all_active_lights();

		size_t get_active_light_count() { return m_active_lights.size(); }
		//remix_light_def* get_first_active_light() { return !m_active_lights.empty() ? &m_active_lights.front() : nullptr; }

		std::unordered_map<std::uint64_t, remix_light_def>* get_active_lights() {
			return &m_active_lights;
		}

		remix_distant_light_def* get_distant_light() {
			return &m_distant_light;
		}

	private:
		static inline std::uint32_t m_active_light_spawn_tracker = 0u;
		static inline std::unordered_map<std::uint64_t, remix_light_def> m_active_lights = {};
		static inline remix_distant_light_def m_distant_light = {};
		std::uint32_t m_updateframe = 0u;
		bool m_is_paused = false;
		bool m_initialized = false;
	};
}
