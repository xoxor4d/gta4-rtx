#include "std_include.hpp"
#include "imgui_menu.hpp"

#include "comp_settings.hpp"
#include "game_lights.hpp"
#include "imgui.hpp"
#include "remix_lights.hpp"
#include "remix_vars.hpp"
#include "water.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/imgui_helper.hpp"

namespace gta4::menu
{
#pragma region SHARED_ROWS

	void settings::on_changed(comp_settings::variable& var)
	{
		const auto& cs = comp_settings::get();

		if (&var == &cs->water_wind_waves || &var == &cs->water_interactive || &var == &cs->freecam_type || &var == &cs->menu_background_blur) {
			comp_settings::write_comp_settings_toml();
		}
		else if (&var == &cs->timecycle_use_remix_atmos_system)
		{
			const bool state = cs->timecycle_use_remix_atmos_system._bool();
			if (const auto skyMode = remix_vars::get_option("rtx.skyMode", "1"); skyMode)
			{
				remix_vars::option_value val { .value = state ? 1.0f : 0.0f };
				remix_vars::get()->add_queue_entry(skyMode, val, 0.1f);
			}

			if (const auto tempResampling = remix_vars::get_option("rtx.volumetrics.enableTemporalResampling", "False"); tempResampling)
			{
				remix_vars::option_value val { .enabled = state };
				remix_vars::get()->add_queue_entry(tempResampling, val, 0.1f);
			}
		}
		else if (&var == &cs->translate_vehicle_vsirens_make_spotlight || &var == &cs->translate_vehicle_vsirens_secondary_spherelight_enabled) {
			remix_lights::clear_light_cache();
		}
	}

	void settings::wind_waves()
	{
		const auto& cs = comp_settings::get();

		ImGui::BeginDisabled(!cs->api_water_enabled.get_as<bool>());
		rows::toggle("Wind Waves", cs->water_wind_waves);
		ImGui::EndDisabled();
	}

	void settings::interactive_water()
	{
		const auto& cs = comp_settings::get();

		ImGui::BeginDisabled(!cs->api_water_enabled.get_as<bool>());
		rows::toggle("Interactive Water", cs->water_interactive);
		ImGui::EndDisabled();
	}

	void settings::phone_mode()
	{
		const auto& cs = comp_settings::get();
		rows::combo("Phone Mode", cs->phone_mode, "Path Traced\0Rasterized\0Rasterized 2D Overlay\0");
	}

	void settings::remix_atmosphere()
	{
		const auto& cs = comp_settings::get();
		rows::toggle("Remix Atmosphere System", cs->timecycle_use_remix_atmos_system);
	}

	void settings::ped_wounds()
	{
		const auto& cs = comp_settings::get();
		rows::toggle_with_float("Ped Wounds", cs->api_peds_wounds_enabled, "Wounds Opacity", cs->api_peds_wounds_opacity, 0.0f, 4.0f, 0.01f);
	}

	void settings::distant_light_sprites()
	{
		const auto& cs = comp_settings::get();
		rows::toggle("Distant Light Sprites", cs->distant_lights_enabled);
	}

	void settings::fake_distant_cars()
	{
		const auto& cs = comp_settings::get();
		rows::toggle("Fake Distant Cars", cs->api_distant_cars_enabled);
	}

	void settings::static_lod_fade()
	{
		const auto& cs = comp_settings::get();

		ImGui::BeginDisabled(!cs->api_static_enabled.get_as<bool>());
		rows::toggle("Static LOD Cross Fade", cs->api_static_lod_fade);
		ImGui::EndDisabled();
	}

#pragma endregion

#pragma region WATER

	void water_quality_presets()
	{
		const auto& gs = comp_settings::get();

		// every preset starts from the defaults of the tessellation and wave settings
		const auto apply_preset = [&](const std::function<void()>& changes)
			{
				comp_settings::variable* vars[] =
				{
					&gs->water_lod, &gs->water_detail_radius, &gs->water_lod_radius_4, &gs->water_lod_radius_8, &gs->water_block_distance_scale,
					&gs->water_sim_edge_fade, &gs->water_vertex_normal_scale, &gs->water_far_waves, &gs->water_far_wave_scale, &gs->water_far_wave_length,
					&gs->water_far_wave_speed, &gs->water_far_wave_direction, &gs->water_far_wave_fade, &gs->water_far_wave_height, &gs->api_water_cull_pieces,
					&gs->water_lod_fine_cells, &gs->water_lod_radius_1, &gs->water_lod_radius_2, &gs->water_lod_blend,
				};

				for (const auto v : vars) {
					v->reset_default();
				}

				changes();
				comp_settings::write_comp_settings_toml();
			};

		const auto button_4way_width = (ImGui::GetContentRegionAvail().x * 0.25f) - (ImGui::GetStyle().ItemSpacing.x);

		ImGui::BeginDisabled(!gs->api_water_enabled.get_as<bool>());
		{
			if (ImGui::Button("Performance", ImVec2(button_4way_width, 0)))
			{
				apply_preset([&]()
					{
						gs->water_lod_fine_cells.set_var(0, true);
						gs->water_detail_radius.set_var(150.0f, true);
						gs->water_sim_edge_fade.set_var(30.0f, true);
						gs->water_far_waves.set_var(false, true);
						gs->water_lod_fine_cells.set_var(1, true);
						gs->water_lod_radius_1.set_var(16.0f, true);
						gs->water_lod_radius_2.set_var(32.0f, true);
						gs->water_lod_radius_4.set_var(48.0f, true);
						gs->water_lod_radius_8.set_var(64.0f, true);
					});
			} TT("Least tesselation with max radius of 150, no procedural far waves.");

			ImGui::SameLine();
			if (ImGui::Button("Standard", ImVec2(button_4way_width, 0))) {
				apply_preset([]() {});
			} TT("The default water settings. Medium tesselation.");

			ImGui::SameLine();
			if (ImGui::Button("Quality", ImVec2(button_4way_width, 0)))
			{
				apply_preset([&]()
					{
						gs->water_lod_fine_cells.set_var(2, true);
						gs->water_detail_radius.set_var(400.0f, true);
						gs->water_far_wave_fade.set_var(250.0f, true);
						gs->water_lod_radius_2.set_var(64.0f, true);
						gs->water_lod_radius_4.set_var(64.0f, true);
						gs->water_lod_radius_8.set_var(100.0f, true);
					});
			} TT("More tesselation with max radius of 400. Detailed.");

			ImGui::SameLine();
			if (ImGui::Button("Insane", ImVec2(button_4way_width, 0)))
			{
				apply_preset([&]()
					{
						gs->water_lod_fine_cells.set_var(2, true);
						gs->water_detail_radius.set_var(600.0f, true);
						gs->water_far_wave_fade.set_var(350.0f, true);
						gs->water_lod_radius_1.set_var(48.0f, true);
						gs->water_lod_radius_2.set_var(64.0f, true);
						gs->water_lod_radius_4.set_var(96.0f, true);
						gs->water_lod_radius_8.set_var(200.0f, true);
						
					});
			} TT("Even more tesselation with max radius of 600.");

			ImGui::EndDisabled();
		}
	}

	void water_general()
	{
		const auto& gs = comp_settings::get();

		category("General", [&]()
			{
				ImGui::PushID("generalwater");
				compsettings_bool_widget("Water through the Remix API", gs->api_water_enabled);
				compsettings_bool_widget("Stabilize Water Texture Hash", gs->override_water_texture_hash);
				compsettings_bool_widget("Assign Animated Water Category", gs->water_apply_animated_water_category);
				compsettings_bool_widget("Apply Shoreline Modifier", gs->water_apply_shoreline_modifier);
				compsettings_bool_widget("Disable Seabed Plane", gs->water_disable_seabed_plane);
				compsettings_float_widget("Water Texture Scale", gs->water_texture_uv_scale, 0.00001f, 100.0f, 0.001f, "%.4f");
				compsettings_float_widget("Water Normal Fadeout Distance", gs->water_texture_normal_fadeout_distance, 0.0f, 1000.0f, 0.5f);
				ImGui::PopID();
			});
	}

	void water_waves()
	{
		const auto& gs = comp_settings::get();

		category("General", [&]()
			{
				ImGui::PushID("waves");
				compsettings_float_widget("Wave Simulation Edge Fade", gs->water_sim_edge_fade, 1.0f, 100.0f, 0.5f, "%.1f");
				compsettings_float_widget("Wave Normal Strength", gs->water_vertex_normal_scale, 0.0f, 4.0f, 0.01f);
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Far Waves", [&]()
			{
				ImGui::PushID("farwaves");
				// the far waves have a fixed height while the game's waves are replaced
				const bool simulated_water = gs->water_wind_waves.get_as<bool>() || gs->water_interactive.get_as<bool>();
				compsettings_bool_widget("Procedural Far Waves", gs->water_far_waves);
				ImGui::BeginDisabled(simulated_water);
				compsettings_float_widget("Far Wave Strength", gs->water_far_wave_scale, 0.0f, 4.0f, 0.01f);
				ImGui::EndDisabled();
				ImGui::BeginDisabled(!simulated_water);
				compsettings_float_widget("Far Wave Height", gs->water_far_wave_height, 0.0f, 1.0f, 0.001f, "%.3f");
				ImGui::EndDisabled();
				compsettings_float_widget("Far Wave Length", gs->water_far_wave_length, 0.5f, 100.0f, 0.1f, "%.1f");
				compsettings_float_widget("Far Wave Speed", gs->water_far_wave_speed, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Far Wave Direction", gs->water_far_wave_direction, -180.0f, 180.0f, 0.5f, "%.1f");
				compsettings_float_widget("Far Wave Edge Fade", gs->water_far_wave_fade, 0.0f, 600.0f, 0.5f, "%.1f");
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Wind Waves", [&]()
			{
				ImGui::PushID("windwaves");
				ImGui::BeginDisabled(!gs->water_wind_waves.get_as<bool>());
				compsettings_float_widget("Wind Wave Height", gs->water_wind_wave_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Long Waves", gs->water_wind_wave_long_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Short Waves", gs->water_wind_wave_short_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Swell On Calm Days", gs->water_wind_swell_base, 0.0f, 8.0f, 0.01f);
				compsettings_float_widget("Swell From Wind", gs->water_wind_swell_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Wind Wave Direction", gs->water_wind_wave_direction, -180.0f, 180.0f, 0.5f, "%.1f");
				compsettings_float_widget("Inland Water Waves", gs->water_inland_wave_scale, 0.0f, 1.0f, 0.01f);
				compsettings_float_widget("Inland Water Above", gs->water_inland_level, 0.0f, 50.0f, 0.05f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Interactive", [&]()
			{
				ImGui::PushID("interactive");
				ImGui::BeginDisabled(!gs->water_interactive.get_as<bool>());
				compsettings_float_widget("Interactive Strength", gs->water_interactive_strength, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Peds", gs->water_interactive_ped_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Vehicles", gs->water_interactive_vehicle_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Boats", gs->water_interactive_boat_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Helicopters", gs->water_interactive_heli_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Objects", gs->water_interactive_object_scale, 0.0f, 4.0f, 0.01f);
				compsettings_float_widget("Lightest Object (kg)", gs->water_interactive_object_min_mass, 0.0f, 500.0f, 0.5f, "%.1f");
				compsettings_float_widget("Bullets And Explosions", gs->water_interactive_impact_scale, 0.0f, 4.0f, 0.01f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});
	}

	void water_tessellation()
	{
		const auto& gs = comp_settings::get();
		const bool api_water = gs->api_water_enabled.get_as<bool>();

		category("Tessellation", [&]()
			{
				ImGui::PushID("tessellation");
				compsettings_bool_widget("Coarser Tessellation Further Away", gs->water_lod);

				ImGui::BeginDisabled(!gs->water_lod.get_as<bool>());
				compsettings_float_widget("Tessellated Water Radius", gs->water_detail_radius, 100.0f, 600.0f, 1.0f, "%.0f");
				compsettings_float_widget("4 Unit Cells From", gs->water_lod_radius_4, 16.0f, 600.0f, 1.0f, "%.0f");
				compsettings_float_widget("8 Unit Cells From", gs->water_lod_radius_8, 32.0f, 600.0f, 1.0f, "%.0f");
				ImGui::EndDisabled();
				compsettings_float_widget("Cell Size Transition Width", gs->water_lod_blend, 0.0f, 128.0f, 1.0f, "%.0f");

				ImGui::Spacing(0, 6);
				ImGui::BeginDisabled(!api_water);
				compsettings_combo_widget("Smallest Cells", gs->water_lod_fine_cells, "2 Units\0" "1 Unit\0" "0.5 Units\0");
				compsettings_float_widget("1 Unit Cells From", gs->water_lod_radius_1, 16.0f, 112.0f, 1.0f, "%.0f");
				compsettings_float_widget("2 Unit Cells From", gs->water_lod_radius_2, 16.0f, 128.0f, 1.0f, "%.0f");
				compsettings_bool_widget("Cull Water Outside The View", gs->api_water_cull_pieces);
				ImGui::EndDisabled();

				compsettings_float_widget("Water Block Distance Scale", gs->water_block_distance_scale, 0.1f, 10.0f, 0.01f);

				ImGui::PopID();
			});
	}

	void pages::water()
	{
		simple("Water", ICON_FA_WATER, []()
			{
				ImGui::Spacing(0, 4);
				ImGui::TextUnformatted("Simulated waves and presets for the tessellated water area.");
				ImGui::Spacing(0, SEPARATOR_SPACING);

				widgets::grid_begin("##water_rows");
				settings::wind_waves();
				settings::interactive_water();
				widgets::grid_end();

				ImGui::Spacing(0, 4);
				ImGui::SeparatorText("Quality");
				ImGui::Spacing(0, 2);
				water_quality_presets();

				ImGui::Spacing(0, 4);
				ImGui::SeparatorText("Stats");
				ImGui::Spacing(0, 2);

				if (!is_indexing()) {
					water::devgui_overview();
				}

				ImGui::Spacing(0, 4);
			});

		advanced([]()
			{
				group("General", ICON_FA_SLIDERS_H, water_general);
				group("Waves", ICON_FA_WATER, water_waves);
				group("Tessellation", ICON_FA_TH, water_tessellation);
				group("Statistics", ICON_FA_CHART_BAR, []()
					{
						if (!is_indexing()) {
							water::devgui_stats();
						}
					});
			});
	}

#pragma endregion

#pragma region WEATHER

	void weather_simple()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();

		ImGui::Spacing(0, 4);
		widgets::grid_begin("##weather_rows");
		{
			settings::remix_atmosphere();
			rows::toggle("Rain Particle System", cs->rain_particle_system_enabled);
			rows::toggle("Weather Wetness", cs->timecycle_wetness_enabled);

			ImGui::BeginDisabled(!cs->timecycle_wetness_enabled.get_as<bool>());
			rows::toggle("Puddle Layer", cs->timecycle_wetness_world_puddle_layer_enable);
			ImGui::EndDisabled();

			const float toggle_width = widgets::switch_width();
			widgets::row_begin("Override Global Wetness");

			ImGui::BeginDisabled(!im->m_dbg_global_wetness_override);
			widgets::row_split(toggle_width);
			ImGui::SliderFloat("##global_wetness", &im->m_dbg_global_wetness, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp); TT("Not a saved setting.");
			ImGui::EndDisabled();

			widgets::row_second(toggle_width);
			widgets::toggle_switch("##global_wetness_override", im->m_dbg_global_wetness_override); TT("Not a saved setting.");
		}
		widgets::grid_end();

		ImGui::Spacing(0, 4);
		if (ImGui::Button(ICON_FA_REDO "   Reload Weather Presets", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			remix_vars::parse_and_apply_weather_presets();
		} TT("Parse and apply all conf files stored in 'rtx_comp/weather_presets/'");

		ImGui::Spacing(0, 4);
		ImGui::SeparatorText("Wetness Presets");
		ImGui::Spacing(0, 2);

		ImGui::TextUnformatted("More wetness tuning can be found within the Advanced Remix Menu (Alt+X):");
		ImGui::TextUnformatted("> Game Setup - Step2: Parameter Tuning - Global Wetness.");
		ImGui::Spacing(0, 4);

		const auto button_3way_width = (ImGui::GetContentRegionAvail().x * 0.33f) - (ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button("Performance", ImVec2(button_3way_width, 0)))
		{
			shared::common::log("ImGui", "Preset: Parsing Addon CompSetting 'rtx_comp/addon_settings/presets/wetness_performance.conf' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			remix_vars::parse_and_apply_conf("rtx_comp\\addon_settings\\presets\\", "wetness_performance.conf", 0.1f, true);
		}

		ImGui::SameLine();
		if (ImGui::Button("Standard", ImVec2(button_3way_width, 0)))
		{
			shared::common::log("ImGui", "Preset: Parsing Addon CompSetting 'rtx_comp/addon_settings/presets/wetness_standard.conf' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			remix_vars::parse_and_apply_conf("rtx_comp\\addon_settings\\presets\\", "wetness_standard.conf", 0.1f, true);
		}

		ImGui::SameLine();
		if (ImGui::Button("High (Puddles++)", ImVec2(button_3way_width, 0)))
		{
			shared::common::log("ImGui", "Preset: Parsing Addon CompSetting 'rtx_comp/addon_settings/presets/wetness_high_more_puddles.conf' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			remix_vars::parse_and_apply_conf("rtx_comp\\addon_settings\\presets\\", "wetness_high_more_puddles.conf", 0.1f, true);
		}

		ImGui::Spacing(0, 4);
	}

	void tc_tab_fog()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		const bool using_atmos = cs->timecycle_use_remix_atmos_system._bool();
		
		category("Fog Color", [&]() 
			{
				ImGui::PushID("fogcol");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable FogColor Logic", cs->timecycle_fogcolor_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_fogcolor_enabled.get_as<bool>());
				{
					compsettings_float_widget("FogColor Base Strength", cs->timecycle_fogcolor_base_strength, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("FogColor Influence Scalar", cs->timecycle_fogcolor_influence_scalar, 0.0f, 0.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mSkyBottomColorFogDensity: [ %.2f, %.2f, %.2f, (Density) %.2f ]",
						im->m_timecyc_curr_mSkyBottomColorFogDensity.x,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.y,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.z,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.w);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxVolumetricsSingleScatteringAlbedo: [ %.2f, %.2f, %.2f ]",
						im->m_timecyc_curr_singleScatteringAlbedo.x,
						im->m_timecyc_curr_singleScatteringAlbedo.y,
						im->m_timecyc_curr_singleScatteringAlbedo.z);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Fog Density", [&]()
			{
				ImGui::PushID("fogdensity");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable FogDensity Logic", cs->timecycle_fogdensity_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_fogdensity_enabled.get_as<bool>());
				{
					compsettings_float_widget("FogDensity Influence Scalar", cs->timecycle_fogdensity_influence_scalar, 0.0f, 0.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mSkyBottomColorFogDensity: [ %.2f, %.2f, %.2f, (Density) %.2f ]",
						im->m_timecyc_curr_mSkyBottomColorFogDensity.x,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.y,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.z,
						im->m_timecyc_curr_mSkyBottomColorFogDensity.w);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxVolumetricsTransmittanceMeasurementDistanceMeters: [ %.2f ]",
						im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Horizon", [&]()
			{
				ImGui::PushID("foghorz");

				compsettings_bool_widget("Enable SkyHorizonHeight Logic", cs->timecycle_skyhorizonheight_enabled);
				ImGui::BeginDisabled(!cs->timecycle_skyhorizonheight_enabled.get_as<bool>());
				{
					compsettings_float_widget("SkyHorizonHeight Scalar", cs->timecycle_skyhorizonheight_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("SkyHorizonHeight Low - Transmittance Offset", cs->timecycle_skyhorizonheight_low_transmittance_offset, 0.0f, 0.0f, 0.01f);
					compsettings_float_widget("SkyHorizonHeight High - Transmittance Offset", cs->timecycle_skyhorizonheight_high_transmittance_offset, 0.0f, 0.0f, 0.01f);

					ImGui::Spacing(0, 4);
					compsettings_float_widget("Camera Height Threshold", cs->timecycle_skyhorizonheight_cam_height_threshold, 0.0f, 0.0f, 1.0f);
					compsettings_float_widget("Camera Height Influence (Low)", cs->timecycle_skyhorizonheight_cam_height_influence_low, 0.0f, 0.0f, 0.01f);
					compsettings_float_widget("Camera Height Influence (High)", cs->timecycle_skyhorizonheight_cam_height_influence_high, 0.0f, 0.0f, 0.01f);

					ImGui::TextDisabled("Timecycle mSkyHorizonHeight: [ %.2f ]",
						im->m_timecyc_curr_mSkyHorizonHeight);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxVolumetricsAtmosphereHeightMeters: [ %.2f ]",
						im->m_timecyc_curr_mSkyHorizonHeight_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_sun()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		const bool using_atmos = cs->timecycle_use_remix_atmos_system._bool();

		category("Sky Light", [&]()
			{
				ImGui::PushID("skyl");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable SkyLight Logic", cs->timecycle_skylight_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->timecycle_skylight_enabled.get_as<bool>() || using_atmos);
				{
					compsettings_float_widget("SkyLight Scalar", cs->timecycle_skylight_scalar, 0.0f, 10.0f, 0.0001f, "%.4f");
					compsettings_float_widget("SkyLight Bad Weather Offset", cs->timecycle_skylight_max_offset_bad_weather, 0.0f, 2.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mSkyLightMultiplier: [ %.2f ]", im->m_timecyc_curr_mSkyLightMultiplier);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxSkyBrightness: [ %.2f ]", im->m_timecyc_curr_mSkyLightMultiplier_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Fog Vol", [&]()
			{
				ImGui::PushID("skyl");

				ImGui::BeginDisabled(using_atmos);
				compsettings_bool_widget("Enable Fogdensity Influence on Volumetric Scale", cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.get_as<bool>());
				{
					compsettings_float_widget("Fogdensity Volumetric Influence Scalar", cs->translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar, 0.0f, 0.0f, 0.005f);
					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_postprocess()
	{
		const auto& im = imgui::get();
		const auto& cs = comp_settings::get();
		
		category("Color Correction", [&]()
			{
				ImGui::PushID("colorcorr");

				compsettings_bool_widget("Enable ColorCorrection Logic", cs->timecycle_colorcorrection_enabled);
				ImGui::BeginDisabled(!cs->timecycle_colorcorrection_enabled.get_as<bool>());
				{
					compsettings_float_widget("ColorCorrection Influence", cs->timecycle_colorcorrection_influence, 0.0f, 15.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mColorCorrection: [ %.2f, %.2f, %.2f ]",
						im->m_timecyc_curr_mColorCorrection.x,
						im->m_timecyc_curr_mColorCorrection.y,
						im->m_timecyc_curr_mColorCorrection.z);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxTonemapColorBalance: [ %.2f, %.2f, %.2f ]",
						im->m_timecyc_curr_mColorCorrection_final.x,
						im->m_timecyc_curr_mColorCorrection_final.y,
						im->m_timecyc_curr_mColorCorrection_final.z);
					ImGui::PopFont();

					ImGui::Spacing(0, SEPARATOR_SPACING);
					ImGui::BeginDisabled(!cs->timecycle_colorcorrection_enabled.get_as<bool>());
					{
						compsettings_bool_widget("Enable ColorTemperature Logic", cs->timecycle_colortemp_enabled);
						compsettings_float_widget("ColorTemperature Value", cs->timecycle_colortemp_value, 0.0f, 15.0f, 0.005f);
						compsettings_float_widget("ColorTemperature Influence", cs->timecycle_colortemp_influence, 0.0f, 0.0f, 0.005f);

						//ImGui::TextDisabled("Timecycle mTemperature: [ %.2f ]",
						//	im->m_timecyc_curr_mTemperature);
						ImGui::TextDisabled("ColorTemp Offset applied to rtxTonemapColorBalance: [ %.2f, %.2f, %.2f ]",
							im->m_timecyc_curr_mTemperature_offset.x,
							im->m_timecyc_curr_mTemperature_offset.y,
							im->m_timecyc_curr_mTemperature_offset.z);

						ImGui::EndDisabled();
					}

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Desaturation", [&]()
			{
				ImGui::PushID("desat");

				compsettings_bool_widget("Enable Desaturation Logic", cs->timecycle_desaturation_enabled);
				ImGui::BeginDisabled(!cs->timecycle_desaturation_enabled.get_as<bool>());
				{
					compsettings_float_widget("Desaturation Influence", cs->timecycle_desaturation_influence, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Far Desaturation Influence", cs->timecycle_fardesaturation_influence, 0.0f, 0.0f, 0.005f);

					ImGui::TextDisabled("Timecycle mDesaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturation);
					ImGui::TextDisabled("Timecycle mDesaturationFar: [ %.2f ]", im->m_timecyc_curr_mDesaturationFar);
					ImGui::TextDisabled("mDesaturationFar influence on rtxTonemapSaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturationFar_offset);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxTonemapSaturation: [ %.2f ]", im->m_timecyc_curr_mDesaturation_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Gamma", [&]()
			{
				ImGui::PushID("gammatwk");

				compsettings_bool_widget("Enable Gamma Logic", cs->timecycle_gamma_enabled);
				ImGui::BeginDisabled(!cs->timecycle_gamma_enabled.get_as<bool>());
				{
					compsettings_float_widget("Gamma Offset", cs->timecycle_gamma_offset, 0.0f, 0.0f, 0.005f);
					ImGui::TextDisabled("Timecycle mGamma: [ %.2f ]", im->m_timecyc_curr_mGamma);

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxTonemapExposureBias: [ %.2f ]", im->m_timecyc_curr_mGamma_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Bloom", [&]()
			{
				ImGui::PushID("bloomtwk");

				compsettings_bool_widget("Enable Bloom Logic", cs->timecycle_bloom_enabled);
				ImGui::BeginDisabled(!cs->timecycle_bloom_enabled.get_as<bool>());
				{
					compsettings_float_widget("Bloom Intensity Scalar", cs->timecycle_bloomintensity_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Bloom Threshold Scalar", cs->timecycle_bloomthreshold_scalar, 0.0f, 0.0f, 0.005f);

					ImGui::Spacing(0, SEPARATOR_SPACING);
					compsettings_bool_widget("Clamp Bloom Min at Night", cs->timecycle_bloom_night_min_clamp_enabled);
					ImGui::BeginDisabled(!cs->timecycle_bloom_night_min_clamp_enabled.get_as<bool>());
					{
						compsettings_float_widget("Intensity Night Min", cs->timecycle_bloom_night_min_clamp_value, 0.0f, 0.0f, 0.005f);
						compsettings_float_widget("Threshold Night Min", cs->timecycle_bloomthreshold_night_min_clamp_value, 0.0f, 0.0f, 0.005f);
						ImGui::EndDisabled();
					}

					ImGui::Spacing(0, SEPARATOR_SPACING);
					compsettings_bool_widget("Clamp Bloom Min at Day", cs->timecycle_bloom_day_min_clamp_enabled);
					ImGui::BeginDisabled(!cs->timecycle_bloom_day_min_clamp_enabled.get_as<bool>());
					{
						compsettings_float_widget("Intensity Day Min", cs->timecycle_bloom_day_min_clamp_value, 0.0f, 0.0f, 0.005f);
						compsettings_float_widget("Threshold Day Min", cs->timecycle_bloomthreshold_day_min_clamp_value, 0.0f, 0.0f, 0.005f);
						ImGui::EndDisabled();
					}

					ImGui::PushFont(shared::imgui::font::BOLD);
					ImGui::TextDisabled("Out rtxBloomBurnIntensity: [ %.2f ]", im->m_timecyc_curr_mBloomIntensity_final);
					ImGui::TextDisabled("Out rtxBloomLuminanceThreshold: [ %.2f ]", im->m_timecyc_curr_mBloomThreshold_final);
					ImGui::PopFont();

					ImGui::EndDisabled();
				}

				ImGui::PopID();
			});
	}

	void tc_tab_wetness()
	{
		const auto& cs = comp_settings::get();

		ImGui::BeginDisabled(!cs->timecycle_wetness_enabled.get_as<bool>());
		{
			category("World", [&]()
				{
					ImGui::PushID("world");
					compsettings_float_widget("Wetness Scalar", cs->timecycle_wetness_world_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Additional Wetness Offset", cs->timecycle_wetness_world_offset, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_world_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_world_blending, 0.0f, 1.0f, 0.005f);

					compsettings_bool_widget("Enable Wetness Variation", cs->timecycle_wetness_world_variation_enable);
					compsettings_bool_widget("Enable World Raindrops", cs->timecycle_wetness_world_raindrop_enable);
					compsettings_float_widget("World Raindrop Scale", cs->timecycle_wetness_world_raindrop_scalar, 0.0f, 10.0f, 0.005f);

					ImGui::Spacing(0, 4.0f);
					compsettings_bool_widget("Enable World Occlusion Check", cs->timecycle_wetness_world_occlusion_check_enable);
					compsettings_bool_widget("Enable Occlusion Smoothing", cs->timecycle_wetness_world_occlusion_smoothing_enable);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			category("Ped", [&]() 
				{
					ImGui::PushID("ped");
					compsettings_bool_widget("Enable Ped Raindrops", cs->timecycle_wetness_ped_raindrop_enable);
					compsettings_float_widget("Ped Raindrop Scale", cs->timecycle_wetness_ped_raindrop_scalar, 0.0f, 10.0f, 0.005f);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			category("Vehicle", [&]() 
				{
					ImGui::PushID("vehicle");
					compsettings_float_widget("Vehicle Wetness Scalar", cs->timecycle_wetness_vehicle_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_vehicle_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_vehicle_blending, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Wet Duration (s)", cs->timecycle_wetness_vehicle_wet_seconds, 0.0f, 120.0f, 0.1f, "%.1f");

					compsettings_bool_widget("Enable Vehicle Raindrops", cs->timecycle_wetness_vehicle_raindrop_enable);
					compsettings_float_widget("Vehicle Raindrop Scale", cs->timecycle_wetness_vehicle_raindrop_scalar, 0.0f, 10.0f, 0.005f);
					ImGui::PopID();
				});

			ImGui::Spacing(0, SEPARATOR_SPACING);
			category("Vehicle Dirt", [&]()
				{
					ImGui::PushID("vehicledirt");
					compsettings_float_widget("Intensity Scalar", cs->timecycle_wetness_vehicle_dirt_intensity_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Wetness Scalar", cs->timecycle_wetness_vehicle_dirt_roughness_scalar, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Min Surface Z-Normal", cs->timecycle_wetness_vehicle_dirt_z_normal, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("Blending Strength", cs->timecycle_wetness_vehicle_dirt_blending, 0.0f, 1.0f, 0.005f);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			category("After Rain", [&]() 
				{
					ImGui::PushID("afterrain");
					compsettings_float_widget("Hold Duration (s)", cs->timecycle_wetness_afterrain_hold_seconds, 0.0f, 3600.0f, 0.5f, "%.0f");
					compsettings_float_widget("Fade Duration (s)", cs->timecycle_wetness_afterrain_fade_seconds, 0.0f, 3600.0f, 0.5f, "%.0f");
					compsettings_float_widget("Wetness Level", cs->timecycle_wetness_afterrain_scalar, 0.0f, 1.0f, 0.005f);
					ImGui::PopID();
				});


			ImGui::Spacing(0, SEPARATOR_SPACING);
			category("Ripples", [&]() 
				{
					ImGui::PushID("ripples");
					compsettings_bool_widget("Scale Ripples with Wetness", cs->timecycle_wetness_ripple_enabled);

					ImGui::BeginDisabled(!cs->timecycle_wetness_ripple_enabled.get_as<bool>());
					compsettings_float_widget("Min Ripple Strength", cs->timecycle_wetness_ripple_min, 0.0f, 1.0f, 0.0005f, "%.3f");
					compsettings_float_widget("Max Ripple Strength", cs->timecycle_wetness_ripple_max, 0.0f, 1.0f, 0.0005f, "%.3f");
					compsettings_float_widget("Wetness Curve", cs->timecycle_wetness_ripple_curve, 0.1f, 8.0f, 0.01f);
					ImGui::EndDisabled();
					ImGui::PopID();
				});

			ImGui::EndDisabled();
		}
	}

	void pages::weather()
	{
		simple("Weather", ICON_FA_CLOUD_SUN_RAIN, weather_simple);

		advanced([]()
			{
				ImGui::Spacing(0, 4);
				ImGui::Indent(4);
				ImGui::TextWrapped("Note: Some of these settings only work with the original sky.");
				ImGui::Unindent(4);
				ImGui::Spacing(0, 8);

				group("Fog", ICON_FA_WATER, tc_tab_fog);
				group("Sky / Sun", ICON_FA_SUN, tc_tab_sun);
				group("Post Processing", ICON_FA_ADJUST, tc_tab_postprocess);
				group("Wetness", ICON_FA_CLOUD, tc_tab_wetness);
			});
	}

#pragma endregion

#pragma region WORLD

	void world_simple()
	{
		const auto& gs = comp_settings::get();

		ImGui::Spacing(0, 4);
		widgets::grid_begin("##world_rows");
		{
			settings::ped_wounds();
			rows::toggle("NPC Hair Alpha Testing", gs->npc_expensive_hair_alpha_testing);

			settings::fake_distant_cars();

			ImGui::BeginDisabled(!gs->api_distant_cars_enabled.get_as<bool>());
			rows::toggle("Distant Cars: Distance Fade", gs->api_distant_cars_fade);
			ImGui::EndDisabled();

			settings::distant_light_sprites();
			settings::static_lod_fade();

			rows::toggle("Vehicle Dirt", gs->vehicle_dirt_enabled);
			rows::toggle("Vehicle Livery", gs->vehicle_livery_enabled);
			rows::toggle("Limit Wheel Spin", gs->vehicle_wheel_spin_limit);
			rows::toggle("Limit Game Quality Sliders", gs->limit_option_sliders);
		}
		widgets::grid_end();
		ImGui::Spacing(0, 4);
	}

	void rs_tab_general()
	{
		const auto& gs = comp_settings::get();

		category("Effects", [&]()
			{
				ImGui::PushID("effects");
				compsettings_float_widget("Litsprite Alpha Scalar", gs->gta_rmptfx_litsprite_alpha_scalar, 0.0f, 20.0f);
				compsettings_bool_widget("Litsprite Unlit", gs->gta_rmptfx_litsprite_unlit);
				ImGui::PopID();
			});
	}

	void rs_tab_remix_api()
	{
		const auto& gs = comp_settings::get();

		category("Geometry", [&]()
			{
				ImGui::PushID("geometry");
				compsettings_bool_widget("Static Geometry", gs->api_static_enabled);

				ImGui::BeginDisabled(!gs->api_static_enabled.get_as<bool>());
				compsettings_bool_widget("Static Geometry: Retained Instances", gs->api_static_retained);
				ImGui::EndDisabled();

				compsettings_bool_widget("Peds", gs->api_peds_enabled);
				compsettings_bool_widget("Vehicles", gs->api_vehicles_enabled);

				ImGui::BeginDisabled(!gs->api_vehicles_enabled.get_as<bool>());
				{
					compsettings_bool_widget("Vehicle Lights", gs->api_vehicle_lights_enabled);
					compsettings_float_widget("Vehicle Lights Emissive Scale", gs->api_vehicle_lights_emissive_scale, 0.0f, 100.0f, 0.005f, "%.3f");
					ImGui::EndDisabled();
				}
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Distant Imposters", [&]()
			{
				ImGui::PushID("distantlights_api");
				compsettings_bool_widget("Distant Light Sprites", gs->api_distant_lights_enabled);

				ImGui::BeginDisabled(!gs->api_distant_lights_enabled.get_as<bool>());
				compsettings_float_widget("Sprite Emissive Scale", gs->api_distant_lights_emissive_scale, 0.0f, 50.0f, 0.01f);
				compsettings_float_widget("Sprite Size Scale", gs->api_distant_lights_size_scale, 0.1f, 10.0f, 0.01f);
				compsettings_float_widget("Sprite Size Scale (Foggy)", gs->api_distant_lights_size_scale_foggy, 0.1f, 10.0f, 0.01f);
				ImGui::EndDisabled();

				ImGui::Spacing(0, 8);
				ImGui::BeginDisabled(!gs->api_distant_cars_enabled.get_as<bool>());
				compsettings_float_widget("Headlight Emissive Scale", gs->api_distant_cars_emissive_scale, 0.0f, 50.0f, 0.01f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Mesh Cache", [&]()
			{
				ImGui::PushID("meshcache");
				compsettings_int_widget("Mesh Creates Per Frame", gs->api_static_creates_per_frame, 0, 256, 0.2f);
				compsettings_int_widget("Mesh Cache Budget (MB)", gs->api_static_cache_mb, 0, 8192, 2.0f);
				ImGui::PopID();
			});
	}

	void rs_tab_vehicles()
	{
		const auto& gs = comp_settings::get();

		category("Dirt", [&]()
			{
				ImGui::PushID("dirt");
				ImGui::BeginDisabled(!gs->vehicle_dirt_enabled.get_as<bool>());
				compsettings_bool_widget("Enable Vehicle Dirt Color Override", gs->vehicle_dirt_custom_color_enabled);
				compsettings_color_widget("Vehicle Dirt Color", gs->vehicle_dirt_custom_color, 3, ImGuiColorEditFlags_Float);

				compsettings_float_widget("Dirt Roughness: Expo", gs->vehicle_dirt_expo, 0.5f, 5.0f, 0.005f);
				compsettings_float_widget("Dirt Roughness: Min Z-Normal", gs->vehicle_dirt_roughness_z_normal, 0.0f, 1.0f, 0.005f);
				compsettings_float_widget("Dirt Roughness: Blending", gs->vehicle_dirt_roughness_blending, 0.0f, 1.0f, 0.005f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("General", [&]()
			{
				ImGui::PushID("vehgeneral");
				compsettings_bool_widget("Force Vertex Color Usage", gs->vehicle_force_vertex_colors);
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Wheels", [&]()
			{
				ImGui::PushID("vehwheels");
				ImGui::BeginDisabled(!gs->vehicle_wheel_spin_limit.get_as<bool>());
				compsettings_float_widget("Wheel Spin: Max Degrees per Frame", gs->vehicle_wheel_spin_max_step, 1.0f, 90.0f, 0.1f, "%.1f");
				ImGui::EndDisabled();
				ImGui::PopID();
			});
	}

	void rs_tab_foliage_hair()
	{
		const auto& gs = comp_settings::get();

		category("Foliage", [&]()
			{
				ImGui::PushID("foliage");
				compsettings_bool_widget("Fixed function Trees", gs->fixed_function_trees);
				compsettings_float_widget("Tree Alpha Cutout Value", gs->tree_foliage_alpha_cutout_value, 0.0f, 20.0f);
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Hair", [&]()
			{
				ImGui::PushID("hair");
				ImGui::BeginDisabled(!gs->npc_expensive_hair_alpha_testing.get_as<bool>());
				compsettings_float_widget("NPC Hair Alpha Cutout Value", gs->npc_expensive_hair_alpha_cutout_value, 0.0f, 1.0f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});
	}

	void pages::world()
	{
		simple("World", ICON_FA_CITY, world_simple);

		advanced([]()
			{
				group("General", ICON_FA_SLIDERS_H, rs_tab_general);
				group("Remix API", ICON_FA_MICROCHIP, rs_tab_remix_api);
				group("Vehicles", ICON_FA_CAR, rs_tab_vehicles);
				group("Foliage / Hair", ICON_FA_LEAF, rs_tab_foliage_hair);
			});
	}

#pragma endregion

#pragma region PHONE

	void phone_advanced()
	{
		const auto& gs = comp_settings::get();
		const auto phone_mode = gs->phone_mode.get_as<int>();

		category("Path Traced", [&]()
			{
				ImGui::PushID("phonepathtraced");
				ImGui::BeginDisabled(phone_mode != 0);
				{
					compsettings_bool_widget("Hide Covers over Screen", gs->phone_hide_covers);
					compsettings_bool_widget("Phone Emissive Override", gs->phone_emissive_override);
					ImGui::BeginDisabled(!gs->phone_emissive_override.get_as<bool>());
					{
						compsettings_float_widget("Phone Emissive Scalar", gs->phone_emissive_scalar, 0.0f, 20.0f);
						ImGui::EndDisabled();
					}
					ImGui::EndDisabled();
				}
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("2D Overlay", [&]()
			{
				ImGui::PushID("phoneoverlay");
				ImGui::BeginDisabled(phone_mode != 2);
				{
					compsettings_vec_widget("Phone Overlay Slide", gs->phone_overlay_slide, 2, -100.0f, 100.0f, 0.0005f, "%.4f");
					compsettings_float_widget("Phone Overlay Screen Overlap", gs->phone_overlay_screen_overlap, -20.0f, 20.0f, 0.05f);
					compsettings_vec_widget("Phone Overlay Screen UV", gs->phone_overlay_screen_uv, 4, -2.0f, 3.0f, 0.001f, "%.3f");
					ImGui::EndDisabled();
				}
				ImGui::PopID();
			});
	}

	void pages::phone()
	{
		simple("Phone", ICON_FA_MOBILE_ALT, []()
			{
				const auto& gs = comp_settings::get();

				ImGui::Spacing(0, 4);
				widgets::grid_begin("##phone_rows");
				{
					settings::phone_mode();
					rows::toggle("Hide Weekday", gs->phone_hide_weekday);

					ImGui::BeginDisabled(gs->phone_mode.get_as<int>() != 2);
					rows::drag_float("Overlay Scale", gs->phone_overlay_scale, 0.1f, 3.0f, 0.005f, "%.3f");
					ImGui::EndDisabled();
				}
				widgets::grid_end();
				ImGui::Spacing(0, 4);
			});

		advanced([]()
			{
				group("Phone Modes", ICON_FA_MOBILE_ALT, phone_advanced, true);
			});
	}

#pragma endregion

#pragma region OTHER

	constexpr auto COLOR_FLAGS_ALPHA = static_cast<ImGuiColorEditFlags_>(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
	constexpr auto COLOR_FLAGS_OPAQUE = static_cast<ImGuiColorEditFlags_>(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoAlpha);

	void pages::other()
	{
		simple("Other", ICON_FA_COG, []()
			{
				const auto& gs = comp_settings::get();

				category("Remix", [&]()
					{
						ImGui::PushID("otherremix");
						compsettings_int_widget("RTXDI Initial Sample Count Override", gs->remix_override_rtxdi_samplecount, 0, 64, 0.01f);
						compsettings_int_widget("RemixVar Set FrameTimeout", gs->remix_var_queue_frame_timeout, 0, 60);
						ImGui::PopID();
					});

				ImGui::Spacing(0, SEPARATOR_SPACING);
				category("Game", [&]()
					{
						ImGui::PushID("othergame");
						compsettings_bool_widget("Load ColorMaps Only", gs->load_colormaps_only);
						compsettings_bool_widget("Unpin Main Thread from Processor 0", gs->unpin_main_thread);
						compsettings_bool_widget("Do not Pause on Lost Focus", gs->do_not_pause_on_lost_focus);
						ImGui::PopID();
					});

				ImGui::Spacing(0, SEPARATOR_SPACING);
				category("Menu", [&]()
					{
						ImGui::PushID("othermenu");
						if (compsettings_bool_widget("Blur Background", gs->menu_background_blur)) {
							settings::on_changed(gs->menu_background_blur);
						}

						ImGui::Spacing(0, 4);
						compsettings_color_widget("Window Background", gs->menu_color_window, 4, COLOR_FLAGS_ALPHA);
						compsettings_color_widget("Container Background", gs->menu_color_container, 4, COLOR_FLAGS_ALPHA);
						compsettings_color_widget("Container Border", gs->menu_color_container_border, 4, COLOR_FLAGS_ALPHA);
						compsettings_color_widget("Category Fade Start", gs->menu_color_category_fade_start, 4, COLOR_FLAGS_ALPHA);
						compsettings_color_widget("Category Fade End", gs->menu_color_category_fade_end, 4, COLOR_FLAGS_ALPHA);
						compsettings_color_widget("Accent", gs->menu_color_accent, 4, COLOR_FLAGS_OPAQUE);
						compsettings_color_widget("New Badge", gs->menu_color_badge, 4, COLOR_FLAGS_OPAQUE);
						ImGui::PopID();
					});
			});
	}

#pragma endregion

#pragma region CREATOR

	void compsettings_culling_container()
	{
		static const auto& gs = comp_settings::get();
		const float inbetween_spacing = SEPARATOR_SPACING;

		category("AntiCulling Cascade", [&]()
			{
				ImGui::PushID("ac_cascade");

				compsettings_float_widget("Near: No Culling Until Distance", gs->nocull_dist_near_static, 0.0f, FLT_MAX, 0.5f);
				ImGui::Spacing(0, inbetween_spacing);

				compsettings_float_widget("Near to Medium Cascade: Medium Distance", gs->nocull_dist_medium_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Near to Medium Cascade: Min. Object Radius", gs->nocull_radius_medium_static, 0.0f, FLT_MAX, 0.5f);
				ImGui::Spacing(0, inbetween_spacing);

				compsettings_float_widget("Medium to Far Cascade: Far Distance", gs->nocull_dist_far_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Medium to Far Cascade: Min. Object Radius", gs->nocull_radius_far_static, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Medium to Far Cascade: Min. Object Height", gs->nocull_height_far_static, 0.0f, FLT_MAX, 0.5f);

				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		category("AntiCulling Extended", [&]()
			{
				ImGui::PushID("exac");
				compsettings_bool_widget("Use Extended AntiCulling", gs->nocull_extended);

				ImGui::BeginDisabled(!gs->nocull_extended._bool());
				compsettings_bool_widget("Force Offscreen AntiCull Meshes", gs->nocull_extended_offscreen);
				compsettings_bool_widget("Use Auto Extended AntiCulling", gs->nocull_extended_auto);
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!gs->nocull_extended_auto._bool());
				compsettings_float_widget("Consider until Distance", gs->nocull_extended_dist, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Min. Object Radius", gs->nocull_extended_radius, 0.0f, FLT_MAX, 0.5f);
				compsettings_float_widget("Min. Object Height", gs->nocull_extended_height, 0.0f, FLT_MAX, 0.5f);
				ImGui::EndDisabled();

				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		category("NoCull MapArea", [&]()
			{
				ImGui::PushID("nocullarea");

				compsettings_bool_widget("Enable NoCull Logic", gs->nocull_map_areas);
				ImGui::BeginDisabled(!gs->nocull_map_areas._bool());
					compsettings_int_widget("Area Count", gs->nocull_map_areas_count, 1, 5);

					ImGui::Spacing(0, inbetween_spacing);
					compsettings_bool_widget("Always draw lowest LOD", gs->nocull_map_areas_always_draw_lowest_lod);
					compsettings_float_widget("Lowest LOD Min. Size", gs->nocull_map_areas_always_draw_lowest_lod_min_size, 0.0f, 500.0f, 0.5f);
				ImGui::EndDisabled();

				ImGui::Spacing(0, inbetween_spacing);
				compsettings_bool_widget("Enable High LOD Logic", gs->nocull_map_areas_high_lod_logic);
				ImGui::BeginDisabled(!gs->nocull_map_areas_high_lod_logic._bool());
					compsettings_float_widget("High LOD Distance", gs->nocull_map_areas_high_lod_logic_distance, -5000.0f, 5000.0f, 1.0f);
				ImGui::EndDisabled();
				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		category("Other Settings", [&]()
			{
				ImGui::PushID("otherac");
				compsettings_float_widget("Light/Prop Distance", gs->nocull_dist_lights, 0.0f, 500.0f, 0.5f);
				compsettings_float_widget("Interior Object Distance", gs->nocull_dist_sphere_interior, 0.0f, 500.0f, 0.5f);

				ImGui::PopID();
			});

		ImGui::Spacing(0, 4);
	}

	void compsettings_light_container()
	{
		static const auto& gs = comp_settings::get();

		const float inbetween_spacing = SEPARATOR_SPACING;
		bool clear = false;

		category("Sphere/Spotlights", [&]()
			{
				ImGui::PushID("spherelight");
				compsettings_bool_widget("Translate Game Lights", gs->translate_game_lights);
				compsettings_bool_widget("Ignore Filler Lights", gs->translate_game_lights_ignore_filler_lights);
				compsettings_bool_widget("No Volumetrics on Filler Lights", gs->translate_game_lights_no_volumetrics_on_filler_lights);

				compsettings_bool_widget("Cull Distant Dim Lights", gs->translate_game_lights_cull);
				compsettings_float_widget("Cull Min Distance", gs->translate_game_lights_cull_min_distance, 0.0f, 1000.0f, 0.5f);
				compsettings_float_widget("Cull Threshold (In View)", gs->translate_game_lights_cull_threshold_in_view, 0.0f, 10.0f, 0.0005f, "%.4f");
				compsettings_float_widget("Cull Threshold (Out Of View)", gs->translate_game_lights_cull_threshold_out_of_view, 0.0f, 10.0f, 0.0005f, "%.4f");

				if (!is_indexing()) {
					ImGui::TextDisabled("Translated: %zu, culled: %u", remix_lights::get()->get_active_light_count(), game_lights::get()->get_culled_light_count());
				}

				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Radius Scalar", gs->translate_game_light_radius_scalar, 0.0f, 0.0f, 0.005f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Intensity Scalar", gs->translate_game_light_intensity_scalar, 0.0f, 0.0f, 0.005f));

				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Softness Offset", gs->translate_game_light_softness_offset, -1.0f, 1.0f, 0.005f, "%.3f"));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Light Softness Scalar", gs->translate_game_light_softness_scalar, 0.0f, 1.0f, 0.005f, "%.3f"));

				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SphereLight Volumetric Scale", gs->translate_game_light_spherelight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Volumetric Scale", gs->translate_game_light_spotlight_volumetric_radiance_scale, 0.0f, 10.0f, 0.005f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Angle Offset", gs->translate_game_light_angle_offset, -180.0f, 180.0f, 0.1f));
				CLEAR_CACHE_CHECK(clear, compsettings_float_widget("SpotLight Focus Expo", gs->translate_game_light_focus_expo, 0.0f, 10.0f, 0.01f));

				ImGui::PopID();
			});

		ImGui::Spacing(0, inbetween_spacing);
		category("Distant Lights", [&]()
			{
				ImGui::PushID("distantlight");
				ImGui::BeginDisabled(gs->timecycle_use_remix_atmos_system._bool());
				{
					compsettings_float_widget("SunLight Intensity Scalar", gs->translate_sunlight_intensity_scalar, 0.0f, 0.0f, 0.005f);
					compsettings_float_widget("SunLight Bad Weather Influence", gs->translate_sunlight_intensity_bad_weather_influence, 0.0f, 1.0f, 0.005f);
					compsettings_float_widget("SunLight Angular Diameter Degrees", gs->translate_sunlight_angular_diameter_degrees, 0.0f, 45.0f, 0.005f);
					compsettings_float_widget("SunLight Volumetric Base", gs->translate_sunlight_volumetric_radiance_base, 0.0f, 10.0f, 0.005f);
					compsettings_float_widget("MoonLight Intensity Scalar", gs->translate_moonlight_intensity_scalar, 0.0f, 1.0f, 0.005f, "%.3f");
					ImGui::EndDisabled();
				}
				ImGui::PopID();
			});
		
		ImGui::Spacing(0, inbetween_spacing);
		category("Headlights", [&]()
		{
			ImGui::PushID("headlight");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Headlight Intensity Scalar", gs->translate_vehicle_headlight_intensity_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Headlight Radius Scalar", gs->translate_vehicle_headlight_radius_scalar, 0.0f, 0.0f, 0.005f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, inbetween_spacing);
		category("Rearlights", [&]()
		{
			ImGui::PushID("rearlight");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Intensity Scalar", gs->translate_vehicle_rearlight_intensity_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Radius Scalar", gs->translate_vehicle_rearlight_radius_scalar, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Inner ConeAngle Offset", gs->translate_vehicle_rearlight_inner_cone_angle_offset, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Rearlight Outer ConeAngle Offset", gs->translate_vehicle_rearlight_outer_cone_angle_offset, 0.0f, 0.0f, 0.005f));

			ImGui::Spacing(0, 4);
			CLEAR_CACHE_CHECK(clear, compsettings_vec_widget("Rearlight Direction Offset", gs->translate_vehicle_rearlight_direction_offset, 3, 0.0f, 0.0f, 0.005f));

			ImGui::PopID();
		});

		ImGui::Spacing(0, inbetween_spacing);
		category("Fake Sirens", [&]()
		{
			ImGui::PushID("fakesiren");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Z Offset", gs->translate_vehicle_fake_siren_z_offset, 0.0f, 0.0f, 0.005f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Intensity Offset", gs->translate_vehicle_fake_siren_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Fake Siren Light Radius Offset", gs->translate_vehicle_fake_siren_radius_offset, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Sirens", [&]()
		{
			ImGui::PushID("siren");
			CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Make Spotlight", gs->translate_vehicle_vsirens_make_spotlight));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Intensity Offset", gs->translate_vehicle_vsirens_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Light Radius Offset", gs->translate_vehicle_vsirens_radius_offset, 0.0f, 0.0f, 0.01f));

			ImGui::Spacing(0.0f, 4.0f);
			CLEAR_CACHE_CHECK(clear, compsettings_bool_widget("Siren Secondary Spherelight", gs->translate_vehicle_vsirens_secondary_spherelight_enabled));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Intensity Offset", gs->translate_vehicle_vsirens_secondary_spherelight_intensity_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Radius Offset", gs->translate_vehicle_vsirens_secondary_spherelight_radius_offset, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Siren Spherelight Z Offset", gs->translate_vehicle_vsirens_secondary_spherelight_z_offset, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("Bar Sirens", [&]()
		{
			ImGui::PushID("barsiren");
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Intensity Scalar", gs->translate_vehicle_barsirens_intensity_scalar, 0.0f, 0.0f, 0.01f));
			CLEAR_CACHE_CHECK(clear, compsettings_float_widget("Bar-Siren Radius Scalar", gs->translate_vehicle_barsirens_radius_scalar, 0.0f, 0.0f, 0.01f));
			ImGui::PopID();
		});

		if (clear) {
			remix_lights::clear_light_cache();
		}

		ImGui::Spacing(0, 4);
	}

	void compsettings_emissive_container()
	{
		static const auto& gs = comp_settings::get();

		category("Vehicle", [&]()
			{
				ImGui::PushID("emsvehicle");
				compsettings_float_widget("Vehicle Light Emissive Scalar", gs->vehicle_lights_emissive_scalar, 0.0f, 0.0f, 0.005f, "%.3f");
				ImGui::PopID();
			});

		ImGui::Spacing(0, SEPARATOR_SPACING);
		category("World", [&]()
			{
				ImGui::PushID("emsworld");
				compsettings_float_widget("EmissiveNight Surfaces Scalar", gs->emissive_night_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
				compsettings_float_widget("Emissive Surfaces Scalar", gs->emissive_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
				compsettings_float_widget("EmissiveStrong Surfaces Scalar", gs->emissive_strong_surfaces_emissive_scalar, 0.0f, 1000.0f, 0.001f, "%.3f");
				compsettings_float_widget("Generic Emissive Scale", gs->emissive_generic_scale, 0.0f, 1000.0f, 0.001f, "%.3f");
				compsettings_bool_widget("AlphaBlended Emissives Hack", gs->emissive_alpha_blend_hack);
				ImGui::BeginDisabled(!gs->emissive_alpha_blend_hack._bool());
				{
					compsettings_float_widget("AlphaBlended Emissives Hack Scale", gs->emissive_alpha_blend_hack_scale, 0.0f, 100.0f, 0.001f, "%.3f");
					ImGui::EndDisabled();
				}

				compsettings_bool_widget("Allow Vertex Colors on Emissives", gs->emissive_allow_vertex_colors);
				compsettings_bool_widget("Allow Vertex Colors on Emissive Alpha", gs->emissive_alpha_allow_vertex_colors);
				ImGui::PopID();
			});
	}

	void pages::lights()
	{
		group("Light Translation", ICON_FA_LIGHTBULB, compsettings_light_container, true);
		group("Emissives", ICON_FA_RSS, compsettings_emissive_container, true);
	}

	void pages::culling()
	{
		group("Anti Culling", ICON_FA_TV, compsettings_culling_container, true);
	}

#pragma endregion
}
