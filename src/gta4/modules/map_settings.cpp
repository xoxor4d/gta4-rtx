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

								// optional
								game::eWeatherType temp_weather = game::WEATHER_NONE;
								float temp_weather_transition_value = 0.2f;
								int temp_from_hour = -1;
								int temp_to_hour = -1;

								if (entry.contains("spawn_on"))
								{
									const auto& spawn_on = entry.at("spawn_on");

									// optional
									if (spawn_on.contains("weather") && spawn_on.at("weather").is_string())
									{
										const auto& weather_str = spawn_on.at("weather").as_string();

										if (weather_str == "EXTRASUNNY") {
											temp_weather = game::WEATHER_EXTRASUNNY;
										} else if (weather_str == "SUNNY") {
											temp_weather = game::WEATHER_SUNNY;
										} else if (weather_str == "SUNNY_WINDY") {
											temp_weather = game::WEATHER_SUNNY_WINDY;
										} else if (weather_str == "CLOUDY") {
											temp_weather = game::WEATHER_CLOUDY;
										} else if (weather_str == "RAIN") {
											temp_weather = game::WEATHER_RAIN;
										} else if (weather_str == "DRIZZLE") {
											temp_weather = game::WEATHER_DRIZZLE;
										} else if (weather_str == "FOGGY") {
											temp_weather = game::WEATHER_FOGGY;
										} else if (weather_str == "LIGHTNING") {
											temp_weather = game::WEATHER_LIGHTNING;
										}
									}

									// optional
									if (spawn_on.contains("weather_transition_value")) {
										temp_weather_transition_value = to_float(spawn_on.at("weather_transition_value"), 0.2f);
									}

									// optional
									if (spawn_on.contains("between_hours")) 
									{
										if (const auto hours = spawn_on.at("between_hours"); hours.is_array() && hours.as_array().size() == 2u)
										{
											temp_from_hour = to_int(hours.as_array()[0], -1);
											temp_to_hour = to_int(hours.as_array()[1], -1);
										}
									}
								}

								m_map_settings.map_markers.emplace_back(
									marker_settings_s
									{
										.index = temp_marker_index,
										.origin = { to_float(pos[0]), to_float(pos[1]), to_float(pos[2]) },
										.rotation = temp_rotation,
										.scale = temp_scale,
										.cull_distance = temp_cull_distance,
										.weather_type = temp_weather,
										.weather_transition_value = temp_weather_transition_value,
										.from_hour = temp_from_hour,
										.to_hour = temp_to_hour,
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


			// ####################
			// parse 'ANTICULL' table
			if (config.contains("ANTICULL"))
			{
				// #
				auto process_anticull_entry = [](const toml::value& entry)
					{
						std::string temp_comment;
						if (!entry.comments().empty())
						{
							temp_comment = entry.comments().at(0);
							temp_comment.erase(0, 2); // rem '# '
						}

						int temp_distance = 0;
						if (entry.contains("distance")) {
								temp_distance = to_int(entry.at("distance"), 0);
						}

						std::unordered_set<int> temp_set;
						if (entry.contains("indices"))
						{
							if (auto& idx = entry.at("indices"); idx.is_array()) {
								temp_set = toml::get<std::unordered_set<int>>(idx);
							}
						}

						m_map_settings.anticull_meshes.emplace_back(
							anti_cull_meshes_s {
								.distance = temp_distance,
								.indices = std::move(temp_set),
								.comment = std::move(temp_comment)
							});
					};

				if (const auto ac = config.at("ANTICULL");
					!ac.is_empty() && !ac.as_array().empty())
				{
					for (const auto& entry : ac.as_array()) {
						process_anticull_entry(entry);
					}
				}
			} // end 'ANTICULL'


			// ####################
			// parse 'LIGHT_OVERRIDES' table
			if (config.contains("LIGHT_OVERRIDES"))
			{
				// #
				auto process_light_override_entry = [](const toml::value& entry)
					{
						std::uint32_t temp_light_hash = 0u;
						if (entry.contains("hash")) {
							temp_light_hash = static_cast<std::uint32_t>(to_uint(entry.at("hash"), 0u));
						}
						else
						{
							TOML_ERROR("[!] [LIGHT_OVERRIDES] #hash", entry, "LightOverride did not define a hash via 'hash' -> skipping");
							return;
						}

						std::string temp_comment;
						if (!entry.comments().empty())
						{
							temp_comment = entry.comments().at(0);
							temp_comment.erase(0, 2); // rem '# '
						}

						Vector temp_pos;
						bool   temp_use_pos = false;
						if (entry.contains("pos"))
						{
							if (const auto& pos = entry.at("pos").as_array(); pos.size() == 3)
							{
								temp_pos = { to_float(pos[0]), to_float(pos[1]), to_float(pos[2]) };
								temp_use_pos = true;
							}
							else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #pos", entry.at("pos"), "expected a 3D vector but got => %d ", entry.at("pos").as_array().size()); }
						}

						Vector temp_dir;
						bool   temp_use_dir = false;
						if (entry.contains("dir"))
						{
							if (const auto& dir = entry.at("dir").as_array(); dir.size() == 3)
							{
								temp_dir = { to_float(dir[0]), to_float(dir[1]), to_float(dir[2]) };
								temp_use_dir = true;
							}
							else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #dir", entry.at("dir"), "expected a 3D vector but got => %d ", entry.at("dir").as_array().size()); }
						}

						Vector temp_color;
						bool   temp_use_color = false;
						if (entry.contains("color"))
						{
							if (const auto& color = entry.at("color").as_array(); color.size() == 3) 
							{
								temp_color = { to_float(color[0]), to_float(color[1]), to_float(color[2]) };
								temp_use_color = true;
							}
							else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #color", entry.at("color"), "expected a 3D vector but got => %d ", entry.at("color").as_array().size()); }
						}

						float temp_radius = 0;
						bool  temp_use_radius = false;
						if (entry.contains("radius")) 
						{
							temp_radius = to_float(entry.at("radius"), 0.0f);
							temp_use_radius = true;
						}

						float temp_intensity = 0;
						bool  temp_use_intensity = false;
						if (entry.contains("intensity"))
						{
							temp_intensity = to_float(entry.at("intensity"), 0.0f);
							temp_use_intensity = true;
						}

						float temp_outer_cone_angle = 0;
						bool  temp_use_outer_cone_angle = false;
						if (entry.contains("outer_cone_angle"))
						{
							temp_outer_cone_angle = to_float(entry.at("outer_cone_angle"), 0.0f);
							temp_use_outer_cone_angle = true;
						}

						float temp_inner_cone_angle = 0;
						bool  temp_use_inner_cone_angle = false;
						if (entry.contains("inner_cone_angle"))
						{
							temp_inner_cone_angle = to_float(entry.at("inner_cone_angle"), 0.0f);
							temp_use_inner_cone_angle = true;
						}

						float temp_volumetric_scale = 0;
						bool  temp_use_volumetric_scale = false;
						if (entry.contains("volumetric_scale"))
						{
							temp_volumetric_scale = to_float(entry.at("volumetric_scale"), 0.0f);
							temp_use_volumetric_scale = true;
						}

						bool temp_light_type = false;
						bool temp_use_light_type = false;
						if (entry.contains("light_type")) 
						{
							temp_light_type = to_bool(entry.at("light_type"), false);
							temp_use_light_type = true;
						}

						m_map_settings.light_overrides[temp_light_hash] =
							light_override_s
							{
								.pos = std::move(temp_pos),
								.dir = std::move(temp_dir),
								.color = std::move(temp_color),
								.radius = temp_radius,
								.intensity = temp_intensity,
								.outer_cone_angle = temp_outer_cone_angle,
								.inner_cone_angle = temp_inner_cone_angle,
								.volumetric_scale = temp_volumetric_scale,
								.light_type = temp_light_type,
								.comment = temp_comment,

								._use_pos = temp_use_pos,
								._use_dir = temp_use_dir,
								._use_color = temp_use_color,
								._use_radius = temp_use_radius,
								._use_intensity = temp_use_intensity,
								._use_outer_cone_angle = temp_use_outer_cone_angle,
								._use_inner_cone_angle = temp_use_inner_cone_angle,
								._use_volumetric_scale = temp_use_volumetric_scale,
								._use_light_type = temp_use_light_type,
							};
					};

				if (const auto lo = config.at("LIGHT_OVERRIDES");
					!lo.is_empty() && !lo.as_array().empty())
				{
					for (const auto& entry : lo.as_array()) {
						process_light_override_entry(entry);
					}
				}
			} // end 'LIGHT_OVERRIDES'
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
