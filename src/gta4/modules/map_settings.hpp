#pragma once

#include <ranges>
#include <gta4/game/structs.hpp>

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
			game::eWeatherType weather_type = game::WEATHER_NONE;
			float weather_transition_value = 0.2f;
			int from_hour = -1;
			int to_hour = -1;
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
			std::vector<light_override_s> attached_lights; // Attached lights for this override

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

	// Category info: holds category name, comment, and all overrides in that category
	struct light_override_category_info_s
	{
		std::string category_name;
		std::string category_comment;
		std::unordered_map<uint64_t, light_override_s> overrides; // hash -> override data (for rebuilding TOML)

		std::string _internal_buffer; // For UI category name input
		std::string _internal_comment_buffer; // For UI category comment input
	};

	// TOML file info: holds all categories and uncategorized overrides for a specific TOML file
	struct light_overrides_toml_info_s
	{
		std::vector<light_override_category_info_s> categories; // Categorized overrides
		std::unordered_map<uint64_t, light_override_s> flat_overrides; // Uncategorized overrides (for rebuilding TOML)
	};

	// TOML file info: holds ignored/allowed lights for a specific TOML file
	struct lights_toml_info_s
	{
		std::unordered_set<uint64_t> ignored_lights; // Hash set for ignored lights in this TOML file
		std::unordered_set<uint64_t> allow_lights; // Hash set for allowed lights in this TOML file
	};

	struct map_settings_s
	{
		std::vector<marker_settings_s> map_markers;
		std::unordered_set<uint64_t> ignored_lights; // Flat set: highest priority (for runtime use)
		std::unordered_set<uint64_t> allow_lights; // Flat set: highest priority (for runtime use)
		std::unordered_map<uint64_t, light_override_s> light_overrides; // Flat map: highest priority override wins (for runtime use)
		std::unordered_map<std::string, light_overrides_toml_info_s> light_overrides_toml_info; // TOML filename -> TOML info (preserves all data for rebuilding)
		std::unordered_map<std::string, lights_toml_info_s> lights_toml_info; // TOML filename -> lights info (preserves all data for ignored/allow lights)
		std::vector<anti_cull_meshes_s> anticull_meshes;
		std::unordered_set<int> lod_fade_excluded; // model indices never dithered: not drawn while the game cross fades them with their lod

		template <typename Map>
		std::vector<typename Map::key_type> get_sorted_keys(const Map& map)
		{
			std::vector<typename Map::key_type> keys;
			keys.reserve(map.size());

			for (const auto& key : map | std::views::keys) {
				keys.push_back(key);
			}

			std::ranges::sort(keys);
			return keys;
		}

		std::string find_highest_priority_ignored_hash_in_toml(uint64_t hash)
		{
			for (const auto& toml_file : get_sorted_keys(lights_toml_info))
			{
				const auto& toml_info = lights_toml_info[toml_file];
				if (toml_info.ignored_lights.contains(hash)) {
					return toml_file;
				}
			}

			return "";
		};

		std::string find_highest_priority_allowed_hash_in_toml(uint64_t hash)
		{
			for (const auto& toml_file : get_sorted_keys(lights_toml_info))
			{
				const auto& toml_info = lights_toml_info[toml_file];
				if (toml_info.allow_lights.contains(hash)) {
					return toml_file;
				}
			}

			return "";
		};
	};

		static map_settings_s& get_map_settings() { return m_map_settings; }
		static void load_settings();
		static void clear_map_settings();
		static void rebuild_light_overrides_from_toml_info(); // Rebuilds light_overrides from toml_info based on priority
		static void rebuild_lights_from_toml_info(); // Rebuilds ignored_lights and allow_lights from toml_info based on priority (excludes hashes with overrides)

	private:
		bool m_initialized = false;
		static inline map_settings_s m_map_settings = {};
		static inline std::vector<std::string> m_args;
		static inline bool m_spawned_markers = false;
		static inline bool m_loaded = false;
		bool parse_toml();
	};
}
