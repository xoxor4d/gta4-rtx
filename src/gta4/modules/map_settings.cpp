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
			// Parse main map_settings.toml
			shared::common::log("MapSettings", "Parsing 'map_settings.toml' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			toml::basic_value<toml::type_config> config;

			try {
				config = toml::parse("rtx_comp\\map_settings.toml", toml::spec::v(1, 1, 0));
			} catch (const toml::file_io_error& err) {
				shared::common::log("MapSettings", std::format("{}", err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}

			// ####################
			// parse 'MARKER' table from markers folder
			shared::common::log("MapSettings", "Parsing toml files inside 'rtx_comp/markers/' folder ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			{
				std::unordered_set<std::uint32_t> processed_marker_indices;
				
				auto process_marker_entry = [&processed_marker_indices](const toml::value& entry)
					{
						std::uint32_t temp_marker_index = 0u;
						if (entry.contains("marker_num")) {
							temp_marker_index = static_cast<std::uint32_t>(to_int(entry.at("marker_num"), 0u));
						}
						else
						{
							TOML_ERROR("[!] [MARKER] #index", entry, "Marker did not define an index via 'marker_num' -> skipping");
							return;
						}

						if (temp_marker_index >= 9000u && temp_marker_index < 9100)
						{
							shared::common::log("MapSettings", "Marker indices 9000-9099 are reserved and can not be used. Skipping marker with num: " + std::to_string(temp_marker_index), shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
							return;
						}

						// Priority: skip if this marker index was already processed
						if (processed_marker_indices.contains(temp_marker_index)) {
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

								processed_marker_indices.insert(temp_marker_index);
							}
							else { TOML_ERROR("[!] [MARKER] #position", entry.at("position"), "expected a 3D vector but got => %d ", entry.at("position").as_array().size()); }
						}
					};

				// Process all marker files in alphabetical order
				const auto marker_files = shared::utils::get_sorted_files("rtx_comp\\markers", ".toml");
				for (const auto& file_path : marker_files)
				{
					try 
					{
						toml::basic_value<toml::type_config> marker_config = toml::parse(file_path, toml::spec::v(1, 1, 0));
						shared::common::log("MapSettings", std::format("> MARKER '{}' ...", file_path), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

						if (marker_config.contains("MARKER"))
						{
							if (const auto marker = marker_config.at("MARKER");
								!marker.is_empty() && !marker.as_array().empty())
							{
								for (const auto& entry : marker.as_array()) {
									process_marker_entry(entry);
								}
							}
						}
						else {
							shared::common::log("MapSettings", "> Couldn't find 'MARKER' section. Skipping ...", shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
						}
					}
					catch (const toml::file_io_error& err) {
						shared::common::log("MapSettings", std::format("Failed to parse marker file '{}': {}", file_path, err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
					}
					catch (const toml::syntax_error& err) {
						shared::common::log("MapSettings", std::format("Syntax error in marker file '{}': {}", file_path, err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
					}
				}
			} // end 'MARKER'


			// ####################
			// parse 'IGNORE_LIGHTS' / 'ALLOW_LIGHTS' / 'LIGHT_OVERRIDES' from light_tweaks/ folder

			shared::common::log("MapSettings", "Parsing light tweaks from 'rtx_comp/light_tweaks/' folder ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			{
				// Process all light tweak files in alphabetical order (earlier files have higher priority)
				const auto light_tweak_files = shared::utils::get_sorted_files("rtx_comp\\light_tweaks", ".toml");
				for (const auto& file_path : light_tweak_files)
				{
					try 
					{
						toml::basic_value<toml::type_config> light_config = toml::parse(file_path, toml::spec::v(1, 1, 0));
						shared::common::log("MapSettings", std::format("> Light Tweaks '{}' ...", file_path), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
						
						// Extract just the filename from the full path
						std::filesystem::path path_obj(file_path);
						std::string filename = path_obj.filename().string();

						lights_toml_info_s& toml_info_allow_ignore = m_map_settings.lights_toml_info[filename];

						// Parse IGNORE_LIGHTS (priority: don't overwrite existing)
						if (light_config.contains("IGNORE_LIGHTS"))
						{
							if (const auto ignore_lights = light_config.at("IGNORE_LIGHTS");
								!ignore_lights.is_empty() && !ignore_lights.as_array().empty())
							{
								const auto new_lights = toml::get<std::unordered_set<uint64_t>>(ignore_lights);
								for (const auto& hash : new_lights)
								{
									// Store in TOML-specific info
									toml_info_allow_ignore.ignored_lights.insert(hash);
									
									// Add to flat set (only if not already present - higher priority files already added)
									if (!m_map_settings.ignored_lights.contains(hash)) {
										m_map_settings.ignored_lights.insert(hash);
									}
								}
							}
						}

						// Parse ALLOW_LIGHTS (priority: don't overwrite existing)
						if (light_config.contains("ALLOW_LIGHTS"))
						{
							if (const auto allow_lights = light_config.at("ALLOW_LIGHTS");
								!allow_lights.is_empty() && !allow_lights.as_array().empty())
							{
								const auto new_lights = toml::get<std::unordered_set<uint64_t>>(allow_lights);
								for (const auto& hash : new_lights)
								{
									// Store in TOML-specific info
									toml_info_allow_ignore.allow_lights.insert(hash);
									
									// Add to flat set (only if not already present - higher priority files already added)
									if (!m_map_settings.allow_lights.contains(hash)) {
										m_map_settings.allow_lights.insert(hash);
									}
								}
							}
						}

						light_overrides_toml_info_s& toml_info_overrides = m_map_settings.light_overrides_toml_info[filename];

						// Parse LIGHT_OVERRIDES (can be in categories or flat)
						if (light_config.contains("LIGHT_OVERRIDES"))
						{
							auto parse_light_override_entry = [](const toml::value& entry) -> std::pair<uint64_t, light_override_s>
								{
									std::uint64_t temp_light_hash = 0u;
									if (entry.contains("hash")) {
										temp_light_hash = static_cast<std::uint64_t>(to_uint(entry.at("hash"), 0u));
									}
									else
									{
										TOML_ERROR("[!] [LIGHT_OVERRIDES] #hash", entry, "LightOverride did not define a hash via 'hash' -> skipping");
										return { 0, {} };
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

									light_override_s override_data =
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
										.attached_lights = {},

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

									// Parse attached lights if present
									if (entry.contains("attached") && entry.at("attached").is_array())
									{
										// Helper function to parse a single attached light override entry (without hash requirement)
										auto parse_attached_light_entry = [](const toml::value& attached_entry) -> light_override_s
											{
												std::string temp_comment;
												if (!attached_entry.comments().empty())
												{
													temp_comment = attached_entry.comments().at(0);
													temp_comment.erase(0, 2); // rem '# '
												}

												Vector temp_pos;
												bool   temp_use_pos = false;
												if (attached_entry.contains("pos"))
												{
													if (const auto& pos = attached_entry.at("pos").as_array(); pos.size() == 3)
													{
														temp_pos = { to_float(pos[0]), to_float(pos[1]), to_float(pos[2]) };
														temp_use_pos = true;
													}
													else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #pos", attached_entry.at("pos"), "expected a 3D vector but got => %d ", attached_entry.at("pos").as_array().size()); }
												}

												Vector temp_dir;
												bool   temp_use_dir = false;
												if (attached_entry.contains("dir"))
												{
													if (const auto& dir = attached_entry.at("dir").as_array(); dir.size() == 3)
													{
														temp_dir = { to_float(dir[0]), to_float(dir[1]), to_float(dir[2]) };
														temp_use_dir = true;
													}
													else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #dir", attached_entry.at("dir"), "expected a 3D vector but got => %d ", attached_entry.at("dir").as_array().size()); }
												}

												Vector temp_color;
												bool   temp_use_color = false;
												if (attached_entry.contains("color"))
												{
													if (const auto& color = attached_entry.at("color").as_array(); color.size() == 3)
													{
														temp_color = { to_float(color[0]), to_float(color[1]), to_float(color[2]) };
														temp_use_color = true;
													}
													else { TOML_ERROR("[!] [LIGHT_OVERRIDES] #color", attached_entry.at("color"), "expected a 3D vector but got => %d ", attached_entry.at("color").as_array().size()); }
												}

												float temp_radius = 0;
												bool  temp_use_radius = false;
												if (attached_entry.contains("radius"))
												{
													temp_radius = to_float(attached_entry.at("radius"), 0.0f);
													temp_use_radius = true;
												}

												float temp_intensity = 0;
												bool  temp_use_intensity = false;
												if (attached_entry.contains("intensity"))
												{
													temp_intensity = to_float(attached_entry.at("intensity"), 0.0f);
													temp_use_intensity = true;
												}

												float temp_outer_cone_angle = 0;
												bool  temp_use_outer_cone_angle = false;
												if (attached_entry.contains("outer_cone_angle"))
												{
													temp_outer_cone_angle = to_float(attached_entry.at("outer_cone_angle"), 0.0f);
													temp_use_outer_cone_angle = true;
												}

												float temp_inner_cone_angle = 0;
												bool  temp_use_inner_cone_angle = false;
												if (attached_entry.contains("inner_cone_angle"))
												{
													temp_inner_cone_angle = to_float(attached_entry.at("inner_cone_angle"), 0.0f);
													temp_use_inner_cone_angle = true;
												}

												float temp_volumetric_scale = 0;
												bool  temp_use_volumetric_scale = false;
												if (attached_entry.contains("volumetric_scale"))
												{
													temp_volumetric_scale = to_float(attached_entry.at("volumetric_scale"), 0.0f);
													temp_use_volumetric_scale = true;
												}

												bool temp_light_type = false;
												bool temp_use_light_type = false;
												if (attached_entry.contains("light_type"))
												{
													temp_light_type = to_bool(attached_entry.at("light_type"), false);
													temp_use_light_type = true;
												}

												return light_override_s
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
													.attached_lights = {},

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

										for (const auto& attached_entry : entry.at("attached").as_array())
										{
											light_override_s attached_light = parse_attached_light_entry(attached_entry);

											// Only add if it has at least one property set
											if (attached_light._use_pos || attached_light._use_dir || attached_light._use_color ||
												attached_light._use_radius || attached_light._use_intensity ||
												attached_light._use_outer_cone_angle || attached_light._use_inner_cone_angle ||
												attached_light._use_volumetric_scale || attached_light._use_light_type)
											{
												override_data.attached_lights.emplace_back(std::move(attached_light));
											}
										}
									}

									return { temp_light_hash, std::move(override_data) };
								};

							const auto& lov = light_config.at("LIGHT_OVERRIDES");

							// Process each entry individually - supports mixed format (categories and flat entries)
							if (lov.is_array() && !lov.as_array().empty())
							{
								for (const auto& entry : lov.as_array())
								{
									if (!entry.is_table()) {
										continue;
									}

									// Check if this entry has "overrides" field (category format)
									if (entry.contains("overrides"))
									{
										// Category format entry
										std::string category_name;
										std::string category_comment_text;

										// Get TOML comment (above the category entry)
										if (!entry.comments().empty())
										{
											category_comment_text = entry.comments().at(0);
											category_comment_text.erase(0, 2); // rem '# '
										}

										// Get category name from "category" field
										if (entry.contains("category") && entry.at("category").is_string()) {
											category_name = entry.at("category").as_string();
										}

										// If no category name but we have a comment, use comment as name
										if (category_name.empty() && !category_comment_text.empty()) {
											category_name = category_comment_text;
										}

										if (category_name.empty())
										{
											category_name = std::to_string(rand());
											TOML_ERROR("Overrides", entry.at("overrides"), "Category with no name and no comment as fallback - using random number");
										}

										light_override_category_info_s category_info;
										category_info.category_name = category_name;
										category_info.category_comment = category_comment_text;

										if (entry.at("overrides").is_array())
										{
											for (const auto& override_entry : entry.at("overrides").as_array())
											{
												auto [hash, override_data] = parse_light_override_entry(override_entry);

												if (hash != 0) {
													category_info.overrides[hash] = std::move(override_data);
												}
											}
										}

										if (!category_info.overrides.empty()) {
											toml_info_overrides.categories.emplace_back(std::move(category_info));
										}
									}
									else if (entry.contains("hash"))
									{
										// Flat format entry (backward compatible)
										auto [hash, override_data] = parse_light_override_entry(entry);

										if (hash != 0) {
											toml_info_overrides.flat_overrides[hash] = std::move(override_data);
										}
									}
								}
							}
						}
					}
					catch (const toml::file_io_error& err) {
						shared::common::log("MapSettings", std::format("Failed to parse light tweak file '{}': {}", file_path, err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
					}
					catch (const toml::syntax_error& err) {
						shared::common::log("MapSettings", std::format("Syntax error in light tweak file '{}': {}", file_path, err.what()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
					}
				}

				// After parsing all files, rebuild light_overrides from toml_info based on priority
				// This will also rebuild ignored/allowed lights (excluding those with overrides)
				rebuild_light_overrides_from_toml_info();

			} // end 'IGNORE_LIGHTS' and 'ALLOW_LIGHTS'

			// Rebuild ignored/allowed lights after all parsing is complete
			// (in case light overrides were parsed after ignored/allowed lights)
			rebuild_lights_from_toml_info();

			// ------------------

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
							anti_cull_meshes_s{
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
			// parse 'LOD_FADE_EXCLUDE' array
			if (config.contains("LOD_FADE_EXCLUDE"))
			{
				if (const auto& ex = config.at("LOD_FADE_EXCLUDE"); ex.is_array()) {
					m_map_settings.lod_fade_excluded = toml::get<std::unordered_set<int>>(ex);
				}
			}
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

	void map_settings::rebuild_light_overrides_from_toml_info()
	{
		// Clear the flat map
		m_map_settings.light_overrides.clear();
		
		// Get sorted TOML filenames (alphabetical order = priority order, earlier = higher priority)
		std::vector<std::string> sorted_filenames;
		for (const auto& [filename, _] : m_map_settings.light_overrides_toml_info) {
			sorted_filenames.push_back(filename);
		}

		std::sort(sorted_filenames.begin(), sorted_filenames.end());
		
		// Process files in priority order (earlier files overwrite later ones)
		for (const auto& filename : sorted_filenames)
		{
			const auto& toml_info = m_map_settings.light_overrides_toml_info.at(filename);
			
			// Process categorized overrides
			for (const auto& category : toml_info.categories)
			{
				for (const auto& [hash, override_data] : category.overrides)
				{
					// Only add if not already present (higher priority file already has it)
					if (!m_map_settings.light_overrides.contains(hash)) {
						m_map_settings.light_overrides[hash] = override_data;
					}
				}
			}
			
			// Process flat (uncategorized) overrides
			for (const auto& [hash, override_data] : toml_info.flat_overrides)
			{
				// Only add if not already present (higher priority file already has it)
				if (!m_map_settings.light_overrides.contains(hash)) {
					m_map_settings.light_overrides[hash] = override_data;
				}
			}
		}
		
		// Rebuild ignored/allowed lights (excluding those with overrides)
		rebuild_lights_from_toml_info();
	}

	void map_settings::rebuild_lights_from_toml_info()
	{
		// Clear the flat sets
		m_map_settings.ignored_lights.clear();
		m_map_settings.allow_lights.clear();
		
		// Get sorted TOML filenames for lights (alphabetical order = priority order, earlier = higher priority)
		std::vector<std::string> sorted_light_filenames;
		for (const auto& [filename, _] : m_map_settings.lights_toml_info) {
			sorted_light_filenames.push_back(filename);
		}

		std::sort(sorted_light_filenames.begin(), sorted_light_filenames.end());
		
		// Get sorted TOML filenames for overrides (alphabetical order = priority order, earlier = higher priority)
		std::vector<std::string> sorted_override_filenames;
		for (const auto& [filename, _] : m_map_settings.light_overrides_toml_info) {
			sorted_override_filenames.push_back(filename);
		}

		std::sort(sorted_override_filenames.begin(), sorted_override_filenames.end());
		
		// Helper function to check if a hash has an override in a higher priority TOML file than the given filename
		auto has_override_in_higher_priority_toml = [&](uint64_t hash, const std::string& current_filename) -> bool
		{
			// Find the index of current_filename in sorted_light_filenames
			auto current_it = std::find(sorted_light_filenames.begin(), sorted_light_filenames.end(), current_filename);
			if (current_it == sorted_light_filenames.end()) {
				return false; // Current file not found, can't determine priority
			}
			
			// Check all override TOML files that come before current_filename in alphabetical order
			for (const auto& override_filename : sorted_override_filenames)
			{
				// Compare alphabetically - if override_filename < current_filename, it has higher priority
				if (override_filename < current_filename)
				{
					const auto& override_toml_info = m_map_settings.light_overrides_toml_info.at(override_filename);
					
					// Check if hash is in any category
					for (const auto& category : override_toml_info.categories)
					{
						if (category.overrides.contains(hash)) {
							return true;
						}
					}
					
					// Check flat overrides
					if (override_toml_info.flat_overrides.contains(hash)) {
						return true;
					}
				}
			}
			
			return false;
		};
		
		// Helper function to check if a hash has an override in the same TOML file
		auto has_override_in_same_toml = [&](uint64_t hash, const std::string& filename) -> bool
		{
			// Check if this TOML file has an override
			if (m_map_settings.light_overrides_toml_info.contains(filename))
			{
				const auto& override_toml_info = m_map_settings.light_overrides_toml_info.at(filename);
				
				// Check if hash is in any category
				for (const auto& category : override_toml_info.categories)
				{
					if (category.overrides.contains(hash)) {
						return true;
					}
				}
				
				// Check flat overrides
				if (override_toml_info.flat_overrides.contains(hash)) {
					return true;
				}
			}
			
			return false;
		};
		
		// Process files in priority order (earlier files overwrite later ones)
		for (const auto& filename : sorted_light_filenames)
		{
			const auto& toml_info = m_map_settings.lights_toml_info.at(filename);
			
			// Process ignored lights - only add if not already present and no higher priority override
			// Also exclude if there's an override in the same TOML file (override takes precedence)
			for (const auto& hash : toml_info.ignored_lights)
			{
				// Only add if:
				// 1. Not already present (higher priority file already has it)
				// 2. No light override in a HIGHER priority TOML file
				// 3. No light override in the SAME TOML file (override takes precedence)
				if (!m_map_settings.ignored_lights.contains(hash) 
					&& !has_override_in_higher_priority_toml(hash, filename)
					&& !has_override_in_same_toml(hash, filename))
				{
					m_map_settings.ignored_lights.insert(hash);
				}
			}
			
			// Process allowed lights - only add if not already present
			for (const auto& hash : toml_info.allow_lights)
			{
				// Only add if not already present (higher priority file already has it)
				if (!m_map_settings.allow_lights.contains(hash))
				{
					m_map_settings.allow_lights.insert(hash);
				}
			}
		}
	}

	void map_settings::clear_map_settings()
	{
		m_map_settings.map_markers.clear();
		m_map_settings.ignored_lights.clear();
		m_map_settings.allow_lights.clear();
		m_map_settings.light_overrides.clear();
		m_map_settings.light_overrides_toml_info.clear();
		m_map_settings.lights_toml_info.clear();
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
