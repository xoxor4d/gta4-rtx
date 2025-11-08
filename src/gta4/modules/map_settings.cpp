#include "std_include.hpp"
#include "map_settings.hpp"

#include "remix_markers.hpp"
#include "shared/common/remix_api.hpp"
#include "shared/common/toml_ext.hpp"

using namespace shared::common::toml_ext;

namespace gta4
{
	void map_settings::load_settings()
	{
		if (m_loaded) {
			get()->clear_map_settings();
		}

		if (get()->parse_toml()) {
			m_loaded = true;
		}
	}

	bool map_settings::parse_toml()
	{
		try 
		{
			shared::common::log("MapSettings", "Parsing 'map_settings.toml' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			toml::basic_value<toml::type_config> config;

			try {
				config = toml::parse("rtx_comp\\map_settings.toml", toml::spec::v(1, 1, 0));
			}
			catch (const toml::file_io_error& err)
			{
				shared::common::log("MapSettings", std::format("{}", err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				return false;
			}

			// ####################
			// parse 'MARKER' table
			if (config.contains("MARKER"))
			{
				// #
				auto process_marker_entry = [](const toml::value& entry)
					{
						std::uint32_t temp_marker_index = 0u;
						if (entry.contains("marker_num")) {
							temp_marker_index = static_cast<std::uint32_t>(to_int(entry.at("marker_num"), 0u));
						}
						else
						{
							TOML_ERROR("[!] [MARKER] #index", entry, "Marker did not define an index via 'marker' or 'nocull' -> skipping");
							return;
						}

						std::string temp_comment;
						if (!entry.comments().empty())
						{
							temp_comment = entry.comments().at(0);
							temp_comment.erase(0, 2); // rem '# '
						}

						if (entry.contains("position"))
						{
							if (const auto& pos = entry.at("position").as_array();
								pos.size() == 3)
							{
								Vector temp_rotation;
								Vector temp_scale = { 1.0, 1.0f, 1.0f };
								float temp_cull_distance = 0.0f;

								// optional
								if (entry.contains("rotation"))
								{
									if (const auto& rot = entry.at("rotation").as_array(); rot.size() == 3) {
										temp_rotation = { DEG2RAD(to_float(rot[0])), DEG2RAD(to_float(rot[1])), DEG2RAD(to_float(rot[2])) };
									} else { TOML_ERROR("[!] [MARKER] #rotation", entry.at("rotation"), "expected a 3D vector but got => %d ", entry.at("rotation").as_array().size()); }
								}

								// optional
								if (entry.contains("scale"))
								{
									if (const auto& scale = entry.at("scale").as_array(); scale.size() == 3) {
										temp_scale = {to_float(scale[0]), to_float(scale[1]), to_float(scale[2]) };
									} else { TOML_ERROR("[!] [MARKER] #scale", entry.at("scale"), "expected a 3D vector but got => %d ", entry.at("scale").as_array().size()); }
								}

								// optional
								if (entry.contains("cull_distance")) {
									temp_cull_distance = to_float(entry.at("cull_distance"), 0.0f);
								}

								m_map_settings.map_markers.emplace_back(
									marker_settings_s
									{
										.index = temp_marker_index,
										.origin = { to_float(pos[0]), to_float(pos[1]), to_float(pos[2]) },
										.rotation = temp_rotation,
										.scale = temp_scale,
										.cull_distance = temp_cull_distance,
										.comment = std::move(temp_comment),
										.internal__frames_until_next_vis_check = static_cast<uint32_t>(temp_marker_index % remix_markers::DISTANCE_CHECK_FRAME_INTERVAL),
									});
							}
							else { TOML_ERROR("[!] [MARKER] #position", entry.at("position"), "expected a 3D vector but got => %d ", entry.at("position").as_array().size()); }
						}
					};

				if (const auto marker = config.at("MARKER");
					!marker.is_empty() && !marker.as_array().empty())
				{
					for (const auto& entry : marker.as_array()) {
						process_marker_entry(entry);
					}
				}
			} // end 'MARKER'


			// ####################
			// parse 'IGNORE_LIGHTS' table
			if (config.contains("IGNORE_LIGHTS"))
			{
				if (const auto ignore_lights = config.at("IGNORE_LIGHTS");
					!ignore_lights.is_empty() && !ignore_lights.as_array().empty())
				{
					m_map_settings.ignored_lights = toml::get<std::unordered_set<uint64_t>>(ignore_lights);
				}
			} // end 'IGNORE_LIGHTS'


			// ####################
			// parse 'ALLOW_LIGHTS' table
			if (config.contains("ALLOW_LIGHTS"))
			{
				if (const auto allow_lights = config.at("ALLOW_LIGHTS");
					!allow_lights.is_empty() && !allow_lights.as_array().empty())
				{
					m_map_settings.allow_lights = toml::get<std::unordered_set<uint64_t>>(allow_lights);
				}
			} // end 'ALLOW_LIGHTS'
		}

		catch (const toml::syntax_error& err)
		{
			shared::common::set_console_color_red(true);
			printf("%s\n", err.what());
			shared::common::set_console_color_default();
			return false;
		}

		return true;
	}

	void map_settings::clear_map_settings()
	{
		m_map_settings.map_markers.clear();
		m_map_settings.ignored_lights.clear();
		m_map_settings.allow_lights.clear();
		m_map_settings = {};
		m_loaded = false;
	}

	map_settings::map_settings()
	{
		p_this = this;
		load_settings();

		// -----
		m_initialized = true;
		shared::common::log("MapSettings", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
