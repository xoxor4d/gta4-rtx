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

		struct light_override_s
		{
			Vector pos;
			Vector dir;
			Vector color = { 1.0f, 1.0f, 1.0f };
			float radius = 5.0f;
			float intensity = 5.0f;
			float outer_cone_angle = 1.5f;
			float inner_cone_angle = 0.78f;
			float volumetric_scale = 1.0f;
			bool light_type = false;
			std::string comment;

			bool _use_pos = false;
			bool _use_dir = false;
			bool _use_color = false;
			bool _use_radius = false;
			bool _use_intensity = false;
			bool _use_outer_cone_angle = false;
			bool _use_inner_cone_angle = false;
			bool _use_volumetric_scale = false;
			bool _use_light_type = false;
		};

		struct anti_cull_meshes_s
		{
			int distance;
			std::unordered_set<int> indices;
			std::string comment;

			std::string _internal_buffer;
			std::string _internal_comment_buffer;
		};

		struct map_settings_s
		{
			std::vector<marker_settings_s> map_markers;
			std::unordered_set<uint64_t> ignored_lights;
			std::unordered_set<uint64_t> allow_lights;
			std::unordered_map<uint64_t, light_override_s> light_overrides;
			std::vector<anti_cull_meshes_s> anticull_meshes;
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
