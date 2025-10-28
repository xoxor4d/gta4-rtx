#pragma once
namespace gta4
{
	class map_settings final : public shared::common::loader::component_module
	{
	public:
		map_settings();
		~map_settings() = default;

		static inline map_settings* p_this = nullptr;
		static map_settings* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		struct marker_settings_s
		{
			std::uint32_t index = 0;
			Vector origin = {};
			Vector rotation = { 0.0f, 0.0f, 0.0f };
			Vector scale = { 1.0f, 1.0f, 1.0f };
			float cull_distance = 0.0f; // 0 = no culling
			std::string comment;

			std::uint32_t internal__frames_until_next_vis_check = 0u;
			bool internal__is_hidden = false;
		};

		struct map_settings_s
		{
			std::vector<marker_settings_s> map_markers;
			std::unordered_set<uint64_t> ignored_lights;
			std::unordered_set<uint64_t> allow_lights;
		};

		static map_settings_s& get_map_settings() { return m_map_settings; }
		static void load_settings();
		static void clear_map_settings();

	private:
		bool m_initialized = false;
		static inline map_settings_s m_map_settings = {};
		static inline std::vector<std::string> m_args;
		static inline bool m_spawned_markers = false;
		static inline bool m_loaded = false;
		bool parse_toml();
	};
}
