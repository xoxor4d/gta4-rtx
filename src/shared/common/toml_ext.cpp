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
}