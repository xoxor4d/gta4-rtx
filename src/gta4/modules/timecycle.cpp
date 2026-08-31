#include "std_include.hpp"
#include "timecycle.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "natives.hpp"
#include "remix_vars.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_FLOAT(name) \
		im->m_timecyc_curr_##name = timecycle->##name; \
		im->m_timecyc_curr_##name##_final = val.value;

			// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_VEC3(tc_name) \
		im->m_timecyc_curr_##tc_name##.x = val.vector[0]; \
		im->m_timecyc_curr_##tc_name##.y = val.vector[1]; \
		im->m_timecyc_curr_##tc_name##.z = val.vector[2];

		// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_UNPACKED_COLOR(tc_name, temp_vec) \
		im->m_timecyc_curr_##tc_name = ##temp_vec; \
		im->m_timecyc_curr_##tc_name##_final.x = val.vector[0]; \
		im->m_timecyc_curr_##tc_name##_final.y = val.vector[1]; \
		im->m_timecyc_curr_##tc_name##_final.z = val.vector[2]; \
		im->m_timecyc_curr_##tc_name##_final.w = val.vector[3];

	// returns a factor between 0 and 1 based on prev and next weather
	// 0 = good weather, 1 = bad weather
	// can be used like: val.value += (max_offset * weather_factor);
	float timecycle::get_bad_weather_factor()
	{
		auto is_bad_weather = [](game::eWeatherType wt) -> bool
			{
				return	wt == game::eWeatherType::WEATHER_RAIN ||
						wt == game::eWeatherType::WEATHER_LIGHTNING ||
						wt == game::eWeatherType::WEATHER_CLOUDY;
			};

		const float transition = *game::weather_change_value; // 0.0f = fully previous, 1.0f = fully new
		float weather_factor = 0.0f;

		if (is_bad_weather(*game::weather_type_prev)) {
			weather_factor += (1.0f - transition);
		}

		if (is_bad_weather(*game::weather_type_new)) {
			weather_factor += transition;
		}

		return std::clamp(weather_factor, 0.0f, 1.0f);
	}

	void timecycle::translate_and_apply_timecycle_settings()
	{
		auto unpack_uint32 = [](const uint32_t& in, float* out)
			{
				out[0] = static_cast<float>(in >> 16 & 0xFF) / 255.0f;
				out[1] = static_cast<float>(in >>  8 & 0xFF) / 255.0f;
				out[2] = static_cast<float>(in >>  0 & 0xFF) / 255.0f;
				out[3] = static_cast<float>(in >> 24 & 0xFF) / 255.0f;
			};

		auto map_range = [](const float& input, const float& in_min, const float& in_max, const float& out_min, const float& out_max)
			{
				return out_min + (out_max - out_min) * ((input - in_min) / (in_max - in_min));
			};

		{
			const auto& im = imgui::get();
			const auto& vars = remix_vars::get();
			const auto& gs = comp_settings::get();
			remix_vars::option_value val{};

			//auto first_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8910);
			//auto third_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8B20);

			bool is_playing_cutscene = false;
			game::TimeCycleParams* timecycle = game::m_pCurrentTimeCycleParams_01;

			if (game::m_dwCutsceneState && *game::m_dwCutsceneState > 0) 
			{
				timecycle = game::m_pCurrentTimeCycleParams_Cutscene;
				is_playing_cutscene = true;
			}

			// manual override
			if (im->m_dbg_used_timecycle >= 0)
			{
				switch (im->m_dbg_used_timecycle)
				{
				default:
				case 0:
					timecycle = game::m_pCurrentTimeCycleParams_01;
					break;

				case 1:
					timecycle = game::m_pCurrentTimeCycleParams_02;
					break;

				case 2:
					timecycle = game::m_pCurrentTimeCycleParams_Cutscene;
					break;
				}
			}

			const auto using_atmos = gs->timecycle_use_remix_atmos_system.get_as<bool>() || im->m_dbg_manual_atmos_system;

			// temp. disable certain settings when atmos is on
			gs->timecycle_fogcolor_enabled.set_temp_override_state(using_atmos);
			gs->timecycle_fogdensity_enabled.set_temp_override_state(using_atmos);
			gs->timecycle_skylight_enabled.set_temp_override_state(using_atmos);
			gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.set_temp_override_state(using_atmos);

			gs->translate_sunlight_intensity_scalar.set_temp_override_state(using_atmos);
			gs->translate_sunlight_intensity_bad_weather_influence.set_temp_override_state(using_atmos);
			gs->translate_sunlight_angular_diameter_degrees.set_temp_override_state(using_atmos);
			gs->translate_sunlight_volumetric_radiance_base.set_temp_override_state(using_atmos);
			gs->translate_moonlight_intensity_scalar.set_temp_override_state(using_atmos);

			// no need to process these when we are not in the game yet
			if (!game::is_in_game) {
				return;
			}

			// ----------------

			static auto rtxSkybrightness = vars->get_option("rtx.skyBrightness");
			if (!using_atmos && gs->timecycle_skylight_enabled.get_as<bool>() && rtxSkybrightness)
			{
				val.value = timecycle->mSkyLightMultiplier * gs->timecycle_skylight_scalar.get_as<float>();

				// bad weather
				auto weather_factor = get_bad_weather_factor();
				if (game::m_game_clock_hours && game::m_game_clock_minutes)
				{
					const int& hour = *game::m_game_clock_hours;
					const int& minute = *game::m_game_clock_minutes;
					
					if (hour >= 21 || hour < 6) {
						weather_factor = 0.0f; // no skylight increase at night
					}
					else if (hour >= 20)
					{
						// fade from: 20:00-21:00
						const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 20:00, 1.0 at 21:00
						weather_factor = 1.0f - fade_progress;
					}
					else if (hour == 6)
					{
						// fade from: 06:00-07:00
						const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 06:00, 1.0 at 07:00
						weather_factor = fade_progress * weather_factor;
					}
				}

				val.value += (gs->timecycle_skylight_max_offset_bad_weather._float() * weather_factor);
				vars->add_queue_entry(rtxSkybrightness, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyLightMultiplier);
			}


			static auto rtxBloomBurnIntensity = vars->get_option("rtx.bloom.burnIntensity");
			static auto rtxBloomLuminanceThreshold = vars->get_option("rtx.bloom.luminanceThreshold");
			if (gs->timecycle_bloom_enabled.get_as<bool>() && rtxBloomBurnIntensity && rtxBloomLuminanceThreshold)
			{
				val.value = timecycle->mBloomIntensity * gs->timecycle_bloomintensity_scalar.get_as<float>();

				// allow min clamping at night with no cutscene playing
				if (gs->timecycle_bloom_night_min_clamp_enabled._bool() && !is_playing_cutscene && game::m_game_clock_hours && game::m_game_clock_minutes &&
					(*game::m_game_clock_hours < 6 || *game::m_game_clock_hours >= 19)) 
				{
					const int& hour = *game::m_game_clock_hours;
					const int& minute = *game::m_game_clock_minutes;
					float fade_scale = 1.0f;

					if (hour >= 20 || hour < 5) {
						fade_scale = 1.0f;
					}
					else if (hour >= 19) {
						fade_scale = static_cast<float>(minute) / 60.0f; // 0.0 at 18:00, 1.0 at 19:00
					}
					else if (hour == 5)
					{
						const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 05:00, 1.0 at 06:00
						fade_scale = 1.0f - fade_progress;
					}
					
					val.value = std::max(gs->timecycle_bloom_night_min_clamp_value._float() * fade_scale, val.value);
				}

				vars->add_queue_entry(rtxBloomBurnIntensity, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomIntensity);
				

				val.value = timecycle->mBloomThreshold * gs->timecycle_bloomthreshold_scalar.get_as<float>();
				vars->add_queue_entry(rtxBloomLuminanceThreshold, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomThreshold);
			}


			static auto rtxTonemapColorBalance = vars->get_option("rtx.tonemap.colorBalance");
			if (gs->timecycle_colorcorrection_enabled.get_as<bool>() && rtxTonemapColorBalance)
			{
				Vector temp_color_offset;
				if (gs->timecycle_colortemp_enabled.get_as<bool>())
				{
					const float nrml_temp = std::clamp(gs->timecycle_colortemp_value.get_as<float>() /*timecycle->mTemperature*/ / 15.0f, -1.0f, 1.0f);
					temp_color_offset.x = nrml_temp * 0.3f;
					temp_color_offset.y = 0.0f;
					temp_color_offset.z = -nrml_temp * 0.3f;
					temp_color_offset *= gs->timecycle_colortemp_influence.get_as<float>();
					//im->m_timecyc_curr_mTemperature = timecycle->mTemperature;
					im->m_timecyc_curr_mTemperature_offset = temp_color_offset;
				}

				Vector4D color_correction;
				unpack_uint32(timecycle->mColorCorrection, &color_correction.x);

				const auto& cc_influence = gs->timecycle_colorcorrection_influence.get_as<float>();
				color_correction.x *= cc_influence;
				color_correction.y *= cc_influence;
				color_correction.z *= cc_influence;

				val.vector[0] = color_correction.x + temp_color_offset.x;
				val.vector[1] = color_correction.y + temp_color_offset.y;
				val.vector[2] = color_correction.z + temp_color_offset.z;
				vars->add_queue_entry(rtxTonemapColorBalance, val);
				ASSIGN_IMGUI_VIS_UNPACKED_COLOR(mColorCorrection, color_correction);
			}


			static auto rtxTonemapSaturation = vars->get_option("rtx.tonemap.saturation");
			if (gs->timecycle_desaturation_enabled.get_as<bool>() && rtxTonemapSaturation)
			{
				const float far_desaturation_influence = gs->timecycle_fardesaturation_influence.get_as<float>() * map_range(timecycle->mDesaturationFar, 0.0f, 1.0f, 0.0f, 0.4f);
				val.value = 1.0f - ((1.0f - timecycle->mDesaturation) * gs->timecycle_desaturation_influence.get_as<float>());
				val.value -= far_desaturation_influence;
				vars->add_queue_entry(rtxTonemapSaturation, val);
				ASSIGN_IMGUI_VIS_FLOAT(mDesaturation);
				im->m_timecyc_curr_mDesaturationFar = timecycle->mDesaturationFar;
				im->m_timecyc_curr_mDesaturationFar_offset = far_desaturation_influence;
			}


			static auto rtxTonemapExposureBias = vars->get_option("rtx.tonemap.exposureBias");
			if (gs->timecycle_gamma_enabled.get_as<bool>() && rtxTonemapExposureBias)
			{
				val.value = -(1.0f - timecycle->mGamma) + gs->timecycle_gamma_offset.get_as<float>();
				vars->add_queue_entry(rtxTonemapExposureBias, val);
				ASSIGN_IMGUI_VIS_FLOAT(mGamma);
			}


			{
				//val.value = log2(game::m_pCurrentTimeCycleParams->mLumMin * 0.01f) + 4.0f; // +6?m_dbg_timecyc_skylight_scalar
				//vars->set_option(vars->get_option("rtx.autoExposure.evMinValue"), val);

				//val.value = log2(game::m_pCurrentTimeCycleParams->mLumMax * 0.01f) + 4.0f; // +4?
				//vars->set_option(vars->get_option("rtx.autoExposure.evMaxValue"), val);
			}


			Vector4D fog_color_density;
			unpack_uint32(timecycle->mSkyBottomColorFogDensity, &fog_color_density.x);
			game::helper_timecycle_current_fog_density = fog_color_density.w; // global

			// vis.
			im->m_timecyc_curr_mSkyBottomColorFogDensity.x = val.vector[0];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.y = val.vector[1];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.z = val.vector[2];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.w = val.vector[3];

			static auto rtxVolumetricsSingleScatteringAlbedo = vars->get_option("rtx.volumetrics.singleScatteringAlbedo");
			if (!using_atmos && gs->timecycle_fogcolor_enabled.get_as<bool>() && rtxVolumetricsSingleScatteringAlbedo)
			{
				const auto& base_strength = gs->timecycle_fogcolor_base_strength.get_as<float>();
				const auto& influence = gs->timecycle_fogcolor_influence_scalar.get_as<float>();

				Vector t = fog_color_density * influence;
				t += base_strength;

				t.Normalize();

				val.vector[0] = t.x;
				val.vector[1] = t.y;
				val.vector[2] = t.z;

				//val.vector[0] = base_strength + fog_color_density.x * influence;
				//val.vector[1] = base_strength + fog_color_density.y * influence;
				//val.vector[2] = base_strength + fog_color_density.z * influence;

				vars->add_queue_entry(rtxVolumetricsSingleScatteringAlbedo, val);
				ASSIGN_IMGUI_VIS_VEC3(singleScatteringAlbedo);
			}

			float atmos_height = 0.0f;
			static auto rtxVolumetricsAtmosphereHeightMeters = vars->get_option("rtx.volumetrics.atmosphereHeightMeters");
			if (gs->timecycle_skyhorizonheight_enabled.get_as<bool>() && rtxVolumetricsAtmosphereHeightMeters)
			{
				// Base atmosphere height from timecycle
				atmos_height = timecycle->mSkyHorizonHeight * 100.0f * gs->timecycle_skyhorizonheight_scalar.get_as<float>();

				// Add camera height influence with smooth non-linear curve
				const auto vp = game::pViewports;
				if (vp && vp->sceneviewport)
				{
					const float cam_height = vp->sceneviewport->cameraInv.m[3][2];
					const float threshold = gs->timecycle_skyhorizonheight_cam_height_threshold.get_as<float>();
					const float influence_low = gs->timecycle_skyhorizonheight_cam_height_influence_low.get_as<float>();
					const float influence_high = gs->timecycle_skyhorizonheight_cam_height_influence_high.get_as<float>();

					float cam_height_contribution = 0.0f;

					if (cam_height > 0.0f && threshold > 0.0f)
					{
						if (cam_height <= threshold)
						{
							// Below/at threshold: linear scaling (minimal impact per meter)
							cam_height_contribution = influence_low * cam_height;
						}
						else
						{
							// Above threshold: contribution from below threshold + linear scaled excess height
							// The curve is smooth at threshold: both formulas give the same value at threshold
							const float excess_height = cam_height - threshold;
							
							// Contribution up to threshold + linear scaling of excess height
							const float base_contribution = influence_low * threshold;
							const float excess_contribution = influence_high * excess_height;
							
							cam_height_contribution = base_contribution + excess_contribution;
						}

						atmos_height += cam_height_contribution;
					}
				}

				val.value = atmos_height;
				vars->add_queue_entry(rtxVolumetricsAtmosphereHeightMeters, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyHorizonHeight);
			}

			static auto rtxVolumetricsTransmittanceMeasurementDistanceMeters = vars->get_option("rtx.volumetrics.transmittanceMeasurementDistanceMeters");
			if (!using_atmos && gs->timecycle_fogdensity_enabled.get_as<bool>() && rtxVolumetricsTransmittanceMeasurementDistanceMeters)
			{
				val.value = map_range(fog_color_density.w, 0.0f, 0.9f, 200.0f, 0.6f)
					* gs->timecycle_fogdensity_influence_scalar.get_as<float>()
					+ map_range(atmos_height, 0.0f, 1000.0f,
						gs->timecycle_skyhorizonheight_low_transmittance_offset.get_as<float>(),
						gs->timecycle_skyhorizonheight_high_transmittance_offset.get_as<float>());

				if (val.value < 0.6f) {
					val.value = 0.6f;
				}

				vars->add_queue_entry(rtxVolumetricsTransmittanceMeasurementDistanceMeters, val);
				im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters = val.value;
			}

			if (using_atmos && !im->m_dbg_manual_atmos_system)
			{
				const auto weather_type_to_preset = [](game::eWeatherType wt) -> const char*
				{
					switch (wt)
					{
					default:
					case game::WEATHER_EXTRASUNNY:  return "clear";
					case game::WEATHER_SUNNY:       return "smoggy";
					case game::WEATHER_SUNNY_WINDY: return "partlyCloudy";
					case game::WEATHER_CLOUDY:      return "overcast";
					case game::WEATHER_RAIN:        return "rainstorm";
					case game::WEATHER_DRIZZLE:     return "drizzle";
					case game::WEATHER_FOGGY:       return "foggy";
					case game::WEATHER_LIGHTNING:   return "thunderstorm";
					}
				};

				const char* const game_prev = weather_type_to_preset(*game::weather_type_prev);
				const char* const game_new  = weather_type_to_preset(*game::weather_type_new);

				static char game_val_buff[256] = {};
				uint32_t game_val_str_size = 0u;

				const auto bridge = shared::common::remix_api::get().m_bridge;

				bridge.GetGameValue("__weather.previous", game_val_buff, sizeof(game_val_buff), &game_val_str_size);
				const auto remix_prev = std::string_view(game_val_buff);

				bridge.GetGameValue("__weather.target", game_val_buff, sizeof(game_val_buff), &game_val_str_size);
				const auto remix_target = std::string_view(game_val_buff);

				const bool mismatch =	remix_prev.empty() || remix_prev == "(initial)" ||
										remix_prev != game_prev || remix_target != game_new;

				bridge.SetGameValue("__weather.target", game_new);
				bridge.SetGameValue("__weather.blend_absolute", std::to_string(*game::weather_change_value).c_str());

				if (mismatch) {
					bridge.SetGameValue("__weather.previous_target", game_prev);
				} else {
					bridge.SetGameValue("__weather.previous_target", "");
				}
			}
		}
	}

	timecycle::timecycle()
	{
		p_this = this;

		// -----
		m_initialized = true;
		shared::common::log("Timecycle", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
