#pragma once
#include "gta4/modules/map_settings.hpp"

#define TOML_ERROR(TITLE, ENTRY, MSG, ...) \
	shared::common::log("Toml", std::format("{}", toml::format_error(toml::make_error_info(#TITLE, (ENTRY), shared::utils::va(#MSG, __VA_ARGS__)))), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);

#define TOML_CATCH_ERROR_WHAT	{ shared::common::log("Toml", std::format("{}", err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true); }

#define TOML_CATCH_SYNTAX_ERROR	catch (toml::syntax_error& err) TOML_CATCH_ERROR_WHAT
#define TOML_CATCH_TYPE_ERROR	catch (toml::type_error& err) TOML_CATCH_ERROR_WHAT

namespace shared::common::toml_ext
{
	std::string build_map_marker_array(const std::vector<gta4::map_settings::marker_settings_s>& markers);
	std::string build_ignore_lights_array(const std::unordered_set<uint64_t>& hashes); // Legacy - use build_ignore_lights_array_from_toml_info instead
	std::string build_allow_lights_array(const std::unordered_set<uint64_t>& hashes); // Legacy - use build_allow_lights_array_from_toml_info instead
	std::string build_ignore_lights_array_from_toml_info(const std::unordered_map<std::string, gta4::map_settings::lights_toml_info_s>& toml_info);
	std::string build_allow_lights_array_from_toml_info(const std::unordered_map<std::string, gta4::map_settings::lights_toml_info_s>& toml_info);
	std::string build_anticull_array(const std::vector<gta4::map_settings::anti_cull_meshes_s>& entries);
	std::string build_lod_fade_exclude_array(const std::unordered_set<int>& indices);
	std::string build_lightweak_array(const std::unordered_map<uint64_t, gta4::map_settings::light_override_s>& entries);
	std::string build_lightweak_toml_file(const std::string& filename, const gta4::map_settings::light_overrides_toml_info_s& toml_info);
	std::string build_lightweak_single_category(const gta4::map_settings::light_override_category_info_s& category);
	std::string build_override_properties(const gta4::map_settings::light_override_s& override_data);
	std::string build_attached_lights_array(const std::vector<gta4::map_settings::light_override_s>& attached_lights, const std::string& indent_prefix);
	std::string build_lightweak_mixed_array(const std::unordered_map<std::string, gta4::map_settings::light_overrides_toml_info_s>& all_toml_info);

	// format 2 decimals
	inline std::string format_float(float value)
	{
		return std::format("{:.2f}", value);
	}

	inline bool to_bool(const toml::value& entry, const bool default_setting = false)
	{
		if (entry.is_boolean()) {
			return static_cast<bool>(entry.as_boolean());
		}

		if (entry.is_integer()) {
			return static_cast<bool>(entry.as_integer());
		}

		try { // this will fail and let the user know whats wrong
			return static_cast<bool>(entry.as_boolean());
		} TOML_CATCH_TYPE_ERROR;

		return default_setting;
	}

	inline int to_int(const toml::value& entry, const int default_setting = 0)
	{
		if (entry.is_boolean()) {
			return static_cast<int>(entry.as_boolean());
		}

		if (entry.is_integer()) {
			return static_cast<int>(entry.as_integer());
		}

		if (entry.is_floating()) {
			return static_cast<int>(entry.as_floating());
		}

		try { // this will fail and let the user know whats wrong
			return static_cast<int>(entry.as_integer());
		} TOML_CATCH_TYPE_ERROR;

		return default_setting;
	}

	inline std::uint32_t to_uint(const toml::value& entry, const std::uint32_t default_val = 0u)
	{
		if (entry.is_floating()) {
			return static_cast<std::uint32_t>(entry.as_floating());
		}

		if (entry.is_integer()) {
			return static_cast<std::uint32_t>(entry.as_integer());
		}

		try { // this will fail and let the user know whats wrong
			return static_cast<std::uint32_t>(entry.as_integer());
		} TOML_CATCH_TYPE_ERROR;

		return default_val;
	}

	inline float to_float(const toml::value& entry, const float default_setting = 0.0f)
	{
		if (entry.is_integer()) {
			return static_cast<float>(entry.as_integer());
		}

		if (entry.is_floating()) {
			return static_cast<float>(entry.as_floating());
		}

		try { // this will fail and let the user know whats wrong
			return static_cast<float>(entry.as_floating());
		} TOML_CATCH_TYPE_ERROR;

		return default_setting;
	}
}
