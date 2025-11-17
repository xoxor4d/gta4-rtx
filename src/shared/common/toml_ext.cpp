#include "std_include.hpp"

namespace shared::common::toml_ext
{
	/// Builds a string containing all map markers
	/// @return the final string in toml format
	std::string build_map_marker_array(const std::vector<gta4::map_settings::marker_settings_s>& markers)
	{
		std::string toml_str = "MARKER = [\n"s;
		for (auto& m : markers)
		{
			if (!m.comment.empty()) {
				toml_str += "\n    # " + m.comment + "\n";
			}

			toml_str += "    { marker_num = " + std::to_string(m.index);

			
			toml_str += ", position = [" + format_float(m.origin.x) + ", " + format_float(m.origin.y) + ", " + format_float(m.origin.z) + "]";
			toml_str += ", rotation = [" + format_float(RAD2DEG(m.rotation.x)) + ", " + format_float(RAD2DEG(m.rotation.y)) + ", " + format_float(RAD2DEG(m.rotation.z)) + "]";
			toml_str += ", scale = [" + format_float(m.scale.x) + ", " + format_float(m.scale.y) + ", " + format_float(m.scale.z) + "]";

			if (m.cull_distance > 0.0f) {
				toml_str += ", cull_distance = " + format_float(m.cull_distance);
			}

			toml_str += " },\n";
		}

		toml_str += "]";
		return toml_str;
	}

	/// Builds a string containing all ignore game light hashes
	/// @return the final string in toml format
	std::string build_ignore_lights_array(const std::unordered_set<uint64_t>& hashes)
	{
		auto hash_count = 0u;

		std::string toml_str = "IGNORE_LIGHTS = [\n    "s;
		bool first_hash = true;

		for (auto& hash : hashes)
		{
			if (!first_hash) {
				toml_str += ", ";
			}
			else {
				first_hash = false;
			}
			
			hash_count++;

			if (!(hash_count % 10)) {
				toml_str += "\n    ";
			}

			toml_str += std::format("0x{:X}", hash);
		}

		toml_str += "\n]";
		return toml_str;
	}

	/// Builds a string containing all allowed game light hashes
	/// @return the final string in toml format
	std::string build_allow_lights_array(const std::unordered_set<uint64_t>& hashes)
	{
		auto hash_count = 0u;

		std::string toml_str = "ALLOW_LIGHTS = [\n    "s;
		bool first_hash = true;

		for (auto& hash : hashes)
		{
			if (!first_hash) {
				toml_str += ", ";
			}
			else {
				first_hash = false;
			}

			hash_count++;

			if (!(hash_count % 10)) {
				toml_str += "\n    ";
			}

			toml_str += std::format("0x{:X}", hash);
		}

		toml_str += "\n]";
		return toml_str;
	}

	/// Builds a string containing all anti culling elements
	/// @return the final string in toml format
	std::string build_anticull_array(const std::vector<gta4::map_settings::anti_cull_meshes_s>& entries)
	{
		auto index_count = 0u;

		std::string toml_str = "ANTICULL = [\n"s;
		for (auto& m : entries)
		{
			if (m.indices.empty()) {
				continue;
			}

			if (!m.comment.empty()) {
				toml_str += "\n    # " + m.comment + "\n";
			}

			toml_str += "    { distance = " + std::to_string(m.distance);

			bool first_hash = true;

			toml_str += ", indices = [\n        ";
			for (auto& index : m.indices)
			{
				if (!first_hash) {
					toml_str += ", ";
				}
				else {
					first_hash = false;
				}

				index_count++;

				if (!(index_count % 10)) {
					toml_str += "\n        ";
				}

				toml_str += std::to_string(index);
			}

			toml_str += "\n    ]},\n";
		}

		toml_str += "]";
		return toml_str;
	}

	/// Builds a string containing all anti culling elements
	/// @return the final string in toml format
	std::string build_lightweak_array(const std::unordered_map<uint64_t, gta4::map_settings::light_override_s>& entries)
	{
		std::string toml_str = "LIGHT_OVERRIDES = [\n"s;
		for (auto& m : entries)
		{
			if (!m.second.comment.empty()) {
				toml_str += "\n    # " + m.second.comment + "\n";
			}

			toml_str += "    { hash = " + std::format("0x{:X}", m.first);

			if (m.second._use_pos) {
				toml_str += ", pos = [" + format_float(m.second.pos.x) + ", " + format_float(m.second.pos.y) + ", " + format_float(m.second.pos.z) + "]";
			}

			if (m.second._use_dir) {
				toml_str += ", dir = [" + format_float(m.second.dir.x) + ", " + format_float(m.second.dir.y) + ", " + format_float(m.second.dir.z) + "]";
			}

			if (m.second._use_color) {
				toml_str += ", color = [" + format_float(m.second.color.x) + ", " + format_float(m.second.color.y) + ", " + format_float(m.second.color.z) + "]";
			}

			if (m.second._use_radius) {
				toml_str += ", radius = " + format_float(m.second.radius);
			}

			if (m.second._use_intensity) {
				toml_str += ", intensity = " + format_float(m.second.intensity);
			}

			if (m.second._use_outer_cone_angle) {
				toml_str += ", outer_cone_angle = " + format_float(m.second.outer_cone_angle);
			}

			if (m.second._use_inner_cone_angle) {
				toml_str += ", inner_cone_angle = " + format_float(m.second.inner_cone_angle);
			}

			if (m.second._use_volumetric_scale) {
				toml_str += ", volumetric_scale = " + format_float(m.second.volumetric_scale);
			}

			if (m.second._use_light_type) {
				toml_str += ", light_type = " + (m.second.light_type ? "true"s : "false"s);
			}

			toml_str += " },\n";
		}

		toml_str += "]";
		return toml_str;
	}
}