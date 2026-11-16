#pragma once
#include "map_settings.hpp"

namespace gta4::map_settings_toml
{
	std::string build_map_marker_array(const std::vector<map_settings::marker_settings_s>& markers);
	std::string build_ignore_lights_array(const std::unordered_set<uint64_t>& hashes); // Legacy - use build_ignore_lights_array_from_toml_info instead
	std::string build_allow_lights_array(const std::unordered_set<uint64_t>& hashes); // Legacy - use build_allow_lights_array_from_toml_info instead
	std::string build_ignore_lights_array_from_toml_info(const std::unordered_map<std::string, map_settings::lights_toml_info_s>& toml_info);
	std::string build_allow_lights_array_from_toml_info(const std::unordered_map<std::string, map_settings::lights_toml_info_s>& toml_info);
	std::string build_anticull_array(const std::vector<map_settings::anti_cull_meshes_s>& entries);
	std::string build_lod_fade_exclude_array(const std::unordered_set<int>& indices);
	std::string build_lightweak_array(const std::unordered_map<uint64_t, map_settings::light_override_s>& entries);
	std::string build_lightweak_toml_file(const std::string& filename, const map_settings::light_overrides_toml_info_s& toml_info);
	std::string build_lightweak_single_category(const map_settings::light_override_category_info_s& category);
	std::string light_type_to_toml(int light_type);
	std::string build_override_properties(const map_settings::light_override_s& override_data);
	std::string build_attached_lights_array(const std::vector<map_settings::light_override_s>& attached_lights, const std::string& indent_prefix);
	std::string build_lightweak_mixed_array(const std::unordered_map<std::string, map_settings::light_overrides_toml_info_s>& all_toml_info);
}
