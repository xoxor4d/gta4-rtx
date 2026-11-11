#pragma once

namespace gta4
{
	class game_lights final : public shared::common::loader::component_module
	{
	public:
		game_lights();

		static inline game_lights* p_this = nullptr;
		static game_lights* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		void	draw_debug();
		void	iterate_all_game_lights();

		/// dev tab: readout, preview and key editor of the atmos sun's path through the day
		static void devgui_atmos_sun();

		/// readable source of a light with a stable id (cutscene / vehicle light), empty for all other hashes
		static std::string get_light_source_name(std::uint64_t hash);

		std::uint32_t get_culled_light_count() const {
			return m_culled_light_count;
		}

	private:
		std::uint32_t m_culled_light_count = 0u; // lights skipped by distance culling in the last frame
		bool m_initialized = false;
	};
}
