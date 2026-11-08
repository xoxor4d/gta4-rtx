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

	/// Builds a string containing all ignore game light hashes grouped by TOML file
	/// @return the final string in toml format with TOML file comments
	std::string build_ignore_lights_array_from_toml_info(const std::unordered_map<std::string, gta4::map_settings::lights_toml_info_s>& toml_info)
	{
		std::string toml_str = ""s;
		
		// Get sorted TOML filenames (alphabetical order)
		std::vector<std::string> sorted_filenames;
		for (const auto& [filename, _] : toml_info) {
			sorted_filenames.push_back(filename);
		}

		std::sort(sorted_filenames.begin(), sorted_filenames.end());
		
		bool first_file = true;
		for (const auto& filename : sorted_filenames)
		{
			const auto& info = toml_info.at(filename);
			
			// Skip empty TOML files
			if (info.ignored_lights.empty()) {
				continue;
			}
			
			if (!first_file) {
				toml_str += "\n\n";
			}
			first_file = false;
			
			// Add TOML file comment
			toml_str += "# " + filename + " -----------------------------------------------------------------------------------\n";
			toml_str += "IGNORE_LIGHTS = [\n    "s;
			
			auto hash_count = 0u;
			bool first_hash = true;
			
			for (const auto& hash : info.ignored_lights)
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
		}
		
		return toml_str;
	}

	/// Builds a string containing all allowed game light hashes grouped by TOML file
	/// @return the final string in toml format with TOML file comments
	std::string build_allow_lights_array_from_toml_info(const std::unordered_map<std::string, gta4::map_settings::lights_toml_info_s>& toml_info)
	{
		std::string toml_str = ""s;
		
		// Get sorted TOML filenames (alphabetical order)
		std::vector<std::string> sorted_filenames;
		for (const auto& [filename, _] : toml_info) {
			sorted_filenames.push_back(filename);
		}
		std::sort(sorted_filenames.begin(), sorted_filenames.end());
		
		bool first_file = true;
		for (const auto& filename : sorted_filenames)
		{
			const auto& info = toml_info.at(filename);
			
			// Skip empty TOML files
			if (info.allow_lights.empty()) {
				continue;
			}
			
			if (!first_file) {
				toml_str += "\n\n";
			}
			first_file = false;
			
			// Add TOML file comment
			toml_str += "# " + filename + " -----------------------------------------------------------------------------------\n";
			toml_str += "ALLOW_LIGHTS = [\n    "s;
			
			auto hash_count = 0u;
			bool first_hash = true;
			
			for (const auto& hash : info.allow_lights)
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
		}
		
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
				} else {
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

	/// Builds the LOD_FADE_EXCLUDE array
	/// @return the final string in toml format
	std::string build_lod_fade_exclude_array(const std::unordered_set<int>& indices)
	{
		std::string toml_str = "LOD_FADE_EXCLUDE = [\n    "s;

		auto count = 0u;
		for (const auto index : indices)
		{
			if (count) {
				toml_str += count % 10 ? ", " : ",\n    ";
			}

			toml_str += std::to_string(index);
			count++;
		}

		toml_str += "\n]";
		return toml_str;
	}

	/// Builds a string containing all light override entries
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

	/// Helper function to build override properties string (without hash, without attached lights)
	/// @param override_data The override data to build properties from
	/// @return String containing comma-separated properties
	std::string build_override_properties(const gta4::map_settings::light_override_s& override_data)
	{
		std::string toml_str = "";

		if (override_data._use_pos) {
			toml_str += "pos = [" + format_float(override_data.pos.x) + ", " + format_float(override_data.pos.y) + ", " + format_float(override_data.pos.z) + "]";
		}

		if (override_data._use_dir) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "dir = [" + format_float(override_data.dir.x) + ", " + format_float(override_data.dir.y) + ", " + format_float(override_data.dir.z) + "]";
		}

		if (override_data._use_color) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "color = [" + format_float(override_data.color.x) + ", " + format_float(override_data.color.y) + ", " + format_float(override_data.color.z) + "]";
		}

		if (override_data._use_radius) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "radius = " + format_float(override_data.radius);
		}

		if (override_data._use_intensity) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "intensity = " + format_float(override_data.intensity);
		}

		if (override_data._use_outer_cone_angle) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "outer_cone_angle = " + format_float(override_data.outer_cone_angle);
		}

		if (override_data._use_inner_cone_angle) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "inner_cone_angle = " + format_float(override_data.inner_cone_angle);
		}

		if (override_data._use_volumetric_scale) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "volumetric_scale = " + format_float(override_data.volumetric_scale);
		}

		if (override_data._use_light_type) {
			if (!toml_str.empty()) toml_str += ", ";
			toml_str += "light_type = " + (override_data.light_type ? "true"s : "false"s);
		}

		return toml_str;
	}

	/// Helper function to build attached lights array string
	/// @param attached_lights The vector of attached lights
	/// @param indent_prefix The prefix for indentation (e.g., "        " for 8 spaces)
	/// @return String containing the attached array or empty string if no attached lights
	std::string build_attached_lights_array(const std::vector<gta4::map_settings::light_override_s>& attached_lights, const std::string& indent_prefix)
	{
		if (attached_lights.empty()) {
			return "";
		}

		std::string toml_str = ", attached = [\n";
		bool first_attached = true;
		for (const auto& attached_light : attached_lights)
		{
			if (!first_attached) {
				toml_str += ",\n";
			}

			if (!attached_light.comment.empty()) {
				toml_str += "\n" + indent_prefix + "    # " + attached_light.comment + "\n";
			}
			else if (first_attached) {
				toml_str += "\n";
			}

			toml_str += indent_prefix + "    { " + build_override_properties(attached_light) + " }";
			first_attached = false;
		}
		toml_str += "\n" + indent_prefix + "]";
		return toml_str;
	}

	/// Builds a string containing a complete TOML file's LIGHT_OVERRIDES
	/// @return the final string in toml format
	std::string build_lightweak_toml_file([[maybe_unused]] const std::string& filename, const gta4::map_settings::light_overrides_toml_info_s& toml_info)
	{
		std::string toml_str = "LIGHT_OVERRIDES = [\n"s;

		// Output categorized overrides
		for (const auto& cat : toml_info.categories)
		{
			if (cat.overrides.empty()) {
				continue;
			}

			if (!cat.category_comment.empty()) {
				toml_str += "\n    # " + cat.category_comment + "\n";
			}

			toml_str += "    { category = \"" + cat.category_name + "\", overrides = [\n";

			bool first_done = false;
			for (const auto& [hash, override_data] : cat.overrides)
			{
				// space between overrides with comments
				if (first_done && !override_data.comment.empty()) {
					toml_str += "\n";
				}

				first_done = true;

				if (!override_data.comment.empty()) {
					toml_str += "        # " + override_data.comment + "\n";
				}

				toml_str += "        { hash = " + std::format("0x{:X}", hash);
				
				const std::string props = build_override_properties(override_data);
				if (!props.empty()) {
					toml_str += ", " + props;
				}

				toml_str += build_attached_lights_array(override_data.attached_lights, "        ");
				toml_str += " },\n";
				first_done = true;
			}

			toml_str += "    ]},\n";
		}

		// Output uncategorized (flat) overrides
		bool first_flat_override = true;
		for (const auto& [hash, override_data] : toml_info.flat_overrides)
		{
			if (!first_flat_override && !override_data.comment.empty()) {
				toml_str += "\n";
			}

			first_flat_override = false;

			if (!override_data.comment.empty()) {
				toml_str += "    # " + override_data.comment + "\n";
			}

			toml_str += "    { hash = " + std::format("0x{:X}", hash);
			
			const std::string props = build_override_properties(override_data);
			if (!props.empty()) {
				toml_str += ", " + props;
			}

			toml_str += build_attached_lights_array(override_data.attached_lights, "    ");
			toml_str += " },\n";
		}

		toml_str += "]";
		return toml_str;
	}

	/// Builds a string containing a single light override category
	/// @return the final string in toml format
	std::string build_lightweak_single_category(const gta4::map_settings::light_override_category_info_s& category)
	{
		if (category.overrides.empty()) {
			return "";
		}

		std::string toml_str = "";

		if (!category.category_comment.empty()) {
			toml_str += "    # " + category.category_comment + "\n";
		}

		toml_str += "    { category = \"" + category.category_name + "\", overrides = [\n";

		bool first_override = true;
		for (const auto& [hash, override_data] : category.overrides)
		{
			if (!first_override && !override_data.comment.empty()) {
				toml_str += "\n";
			}

			first_override = false;

			if (!override_data.comment.empty()) {
				toml_str += "        # " + override_data.comment + "\n";
			}

			toml_str += "        { hash = " + std::format("0x{:X}", hash);
			
			const std::string props = build_override_properties(override_data);
			if (!props.empty()) {
				toml_str += ", " + props;
			}

			toml_str += build_attached_lights_array(override_data.attached_lights, "        ");
			toml_str += " },\n";
		}

		toml_str += "    ]},\n";
		return toml_str;
	}

	/// Builds a string containing mixed format from all TOML files (categories + flat overrides)
	/// @return the final string in toml format with TOML file comments
	std::string build_lightweak_mixed_array(const std::unordered_map<std::string, gta4::map_settings::light_overrides_toml_info_s>& all_toml_info)
	{
		std::string toml_str = ""s;

		// Get sorted filenames to maintain order
		std::vector<std::string> sorted_filenames;
		for (const auto& [filename, _] : all_toml_info) {
			sorted_filenames.push_back(filename);
		}

		std::sort(sorted_filenames.begin(), sorted_filenames.end());
		bool first_file = true;

		// Process each TOML file
		for (const auto& filename : sorted_filenames)
		{
			const auto& toml_info = all_toml_info.at(filename);
			
			// Check if this TOML file has any content
			bool has_content = false;
			if (!toml_info.categories.empty())
			{
				for (const auto& cat : toml_info.categories) 
				{
					if (!cat.overrides.empty())
					{
						has_content = true;
						break;
					}
				}
			}

			if (!has_content && !toml_info.flat_overrides.empty()) {
				has_content = true;
			}
			
			// Skip empty TOML files
			if (!has_content) {
				continue;
			}
			
			// Add TOML file comment and start new LIGHT_OVERRIDES section
			if (!first_file) {
				toml_str += "\n\n";
			}

			first_file = false;
			
			toml_str += "# " + filename + " -----------------------------------------------------------------------------------\n";
			toml_str += "LIGHT_OVERRIDES = [\n";

			// Output categorized overrides
			for (const auto& cat : toml_info.categories)
			{
				if (cat.overrides.empty()) {
					continue;
				}

				toml_str += "\n";

				if (!cat.category_comment.empty()) {
					toml_str += "    # " + cat.category_comment + "\n";
				}

				toml_str += "    { category = \"" + cat.category_name + "\", overrides = [\n";

				bool first_override = true;
				for (const auto& [hash, override_data] : cat.overrides)
				{
					if (!first_override && !override_data.comment.empty()) {
						toml_str += "\n";
					}

					first_override = false;

					if (!override_data.comment.empty()) {
						toml_str += "        # " + override_data.comment + "\n";
					}

					toml_str += "        { hash = " + std::format("0x{:X}", hash);
					
					const std::string props = build_override_properties(override_data);
					if (!props.empty()) {
						toml_str += ", " + props;
					}

					toml_str += build_attached_lights_array(override_data.attached_lights, "        ");
					toml_str += " },\n";
				}

				toml_str += "    ]},\n";
			}

			// Output uncategorized (flat) overrides
			// Check if there were categories before (to determine if this is the first item)
			bool has_categories_before = false;
			for (const auto& cat : toml_info.categories) {
				if (!cat.overrides.empty()) {
					has_categories_before = true;
					break;
				}
			}
			
			bool first_flat_override = true;
			for (const auto& [hash, override_data] : toml_info.flat_overrides)
			{
				// Add newline before comment if not first item (either not first flat override, or there were categories before)
				if ((!first_flat_override || has_categories_before) && !override_data.comment.empty()) {
					toml_str += "\n";
				}

				first_flat_override = false;

				if (!override_data.comment.empty()) {
					toml_str += "    # " + override_data.comment + "\n";
				}

				toml_str += "    { hash = " + std::format("0x{:X}", hash);
				
				const std::string props = build_override_properties(override_data);
				if (!props.empty()) {
					toml_str += ", " + props;
				}

				toml_str += build_attached_lights_array(override_data.attached_lights, "    ");
				toml_str += " },\n";
			}
			
			// Close LIGHT_OVERRIDES array for this TOML file
			toml_str += "]";
		}

		return toml_str;
	}
}