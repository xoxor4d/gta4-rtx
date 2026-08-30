#include "std_include.hpp"
#include "game_lights.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "remix_lights.hpp"
#include "remix_vars.hpp"
#include "timecycle.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	uint64_t calculate_light_hash(const game::CLightSource& def)
	{
		std::uint32_t hash = 0u;

		hash = shared::utils::hash32_combine(hash, def.mPosition.x);
		hash = shared::utils::hash32_combine(hash, def.mPosition.y);
		hash = shared::utils::hash32_combine(hash, def.mPosition.z);
		hash = shared::utils::hash32_combine(hash, def.mFlags);
		hash = shared::utils::hash32_combine(hash, def.mRoomIndex);
		//hash = shared::utils::hash32_combine(hash, def.mInteriorIndex); // this changes when starting a new game from an old save
		hash = shared::utils::hash32_combine(hash, def.mTxdHash);

		//hash = shared::utils::hash32_combine(hash, def.mVolumeSize);
		//hash = shared::utils::hash32_combine(hash, def.mVolumeScale);
		//hash = shared::utils::hash32_combine(hash, def.mIntensity);

		return hash;
	}

	bool compare_dynamic_light_without_position(const game::CLightSource& l1, const game::CLightSource& l2, float eps = 1.e-6f)
	{
		if (shared::utils::float_equal(l1.mIntensity, l2.mIntensity, eps)) {
			if (shared::utils::float_equal(l1.mInnerConeAngle, l2.mInnerConeAngle, eps)) {
				if (shared::utils::float_equal(l1.mRadius, l2.mRadius, eps)) {
					return true;
				}
			}
		}

		return false;
	}

	void game_lights::draw_debug()
	{
		const auto im = imgui::get();
		if (im->m_dbg_visualize_api_lights)
		{
			Vector player_pos;
			player_pos = game::FindPlayerCentreOfWorld(&player_pos);

			game::CLightSource* list = game::get_renderLights();
			const auto count = game::get_renderLightsCount();

			for (auto i = 0u; count; i++)
			{
				auto& def = list[i];
				if (def.mDirection.LengthSqr() == 0.0f) {
					break;
				}

				const Vector circle_pos = def.mPosition;
				if (fabs(circle_pos.DistToSqr(player_pos)) > im->m_dbg_visualize_api_lights_3d_distance * im->m_dbg_visualize_api_lights_3d_distance) {
					continue;
				}

				const float radius = def.mRadius * (comp_settings::get()->translate_game_light_radius_scalar.get_as<float>() * 0.01f);
				auto& remixapi = shared::common::remix_api::get();

				remixapi.add_debug_circle(circle_pos, Vector(0.0f, 0.0f, 1.0f), radius, radius * 0.5f, def.mColor, false);
				remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 0, 90), Vector(1.0f, 1.0f, 1.0f));
				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 90, 0), Vector(1.0f, 1.0f, 1.0f));
				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(90, 0, 90), Vector(1.0f, 1.0f, 1.0f));
			}
		}
	}


	/**
	 * Adds an actual game light via 'game::AddSceneLight' which is later translated to a remixApi light
	 * @param type				sphere/spot
	 * @param pos				position of light	
	 * @param color 			color of light (game scale)
	 * @param intensity 		intensity of light (game scale)
	 * @param radius 			radius of light (game scale)
	 * @param inner_cone 		inner_cone of light (game scale)
	 * @param outer_cone 		outer_cone of light (game scale)
	 * @param dir 				direction of light - spot light only (game scale)
	 * @param always_update 	update translated remix light every frame - code tries to re-use lights of previous frames when false
	 */
	void add_game_light(const game::eLightType& type, Vector pos, Vector color, const float& intensity, const float& radius, const float& inner_cone = 0.0f, const float& outer_cone = 0.0f, Vector dir = { 0, 1, 0 }, bool always_update = true)
	{
		game::AddSceneLight(0, type,
			always_update ? 0x400 : 0x0, // constantly update lights with 0x400 flag (remix_lights.cpp)
			&dir.x, &dir.x, &pos.x, &color.x,
			intensity, 0, 0, radius, inner_cone, outer_cone, 0, 0, 0);
	}


	/**
	 * Iterate all game lights this frame 
	 */
	void game_lights::iterate_all_game_lights()
	{
		GTA4_PERF_SCOPE(performance_section::GameLights);
		const auto im = imgui::get();
		const auto gs = comp_settings::get();
		const auto rml = remix_lights::get();
		const auto& api = shared::common::remix_api::get();

		auto& active_lights = rml->get_active_lights();
		const auto& updateframe = rml->get_updateframe();

		// sun without remix atmosphere system
		if ((!gs->timecycle_use_remix_atmos_system._bool() || im->m_dbg_force_distant_light_translation) && game::g_directionalLights)
		{
			auto& def = game::g_directionalLights[0];
			auto& l = rml->get_distant_light();

			if (l.m_handle)
			{
				api.m_bridge.DestroyLight(l.m_handle);
				l.m_handle = nullptr;
			}

			l.m_updateframe = updateframe;

			l.m_ext.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
			l.m_ext.pNext = nullptr;

			auto dir = def.mDirection; dir.Normalize();
			l.m_ext.direction = dir.ToRemixFloat3D();

			l.m_ext.angularDiameterDegrees = gs->translate_sunlight_angular_diameter_degrees.get_as<float>();
			l.m_ext.volumetricRadianceScale = gs->translate_sunlight_volumetric_radiance_base.get_as<float>();

			if (gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.get_as<bool>())
			{
				l.m_ext.volumetricRadianceScale +=
					game::helper_timecycle_current_fog_density * gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar.get_as<float>();
			}

			// calculate time-based intensity multiplier for moonlight
			float intensity_multiplier = 1.0f;
			if (game::m_game_clock_hours && game::m_game_clock_minutes)
			{
				const int& hour = *game::m_game_clock_hours;
				const int& minute = *game::m_game_clock_minutes;
				const float moon_scalar = std::clamp(gs->translate_moonlight_intensity_scalar._float(), 0.0f, 1.0f);
				
				if (hour >= 22 || hour < 5) 
				{
					// full moon: 22:00-05:00
					intensity_multiplier = moon_scalar; 
				}
				else if (hour == 21)
				{
					// fade from sun to moon: 21:00-22:00
					const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 21:00, 1.0 at 22:00
					intensity_multiplier = 1.0f + fade_progress * (moon_scalar - 1.0f); // lerp from 1.0 to moon_scalar
				}
				else if (hour == 5)
				{
					// fade from moon to sun: 05:00-06:00
					const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 05:00, 1.0 at 06:00
					intensity_multiplier = moon_scalar + fade_progress * (1.0f - moon_scalar); // lerp from moon_scalar to 1.0
				}
			}

			l.m_info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
			l.m_info.pNext = &l.m_ext;
			l.m_info.hash = shared::utils::string_hash64("apilight_distant");

			Vector rad = (def.mIntensity * Vector(def.mColor) * gs->translate_sunlight_intensity_scalar._float() * intensity_multiplier);

			const auto weather_factor = 1.0f - timecycle::get_bad_weather_factor() * std::clamp(gs->translate_sunlight_intensity_bad_weather_influence._float(), 0.0f, 1.0f);
			rad *= weather_factor;

			l.m_info.radiance = rad.ToRemixFloat3D();

			if (api.m_bridge.CreateLight(&l.m_info, &l.m_handle) == REMIXAPI_ERROR_CODE_SUCCESS && l.m_handle) {
				api.m_bridge.DrawLightInstance(l.m_handle);
			}
		}
		else if (game::g_directionalLights)
		{
			auto& def = game::g_directionalLights[0];
			auto dir = def.mDirection; dir.Normalize();
				 dir = -dir;

			const static auto rtx_atmosphere_sunElevation = remix_vars::get_option("rtx.atmosphere.sunElevation");
			const static auto rtx_atmosphere_sunRotation = remix_vars::get_option("rtx.atmosphere.sunRotation");
			const static auto rtx_atmosphere_moon0_elevation = remix_vars::get_option("rtx.atmosphere.moon0.elevation0");
			const static auto rtx_atmosphere_moon0_rotation = remix_vars::get_option("rtx.atmosphere.moon0.rotation0");

			// notes:
			// - game combines sun + moon into a single light
			// - game light never reaches horizon, instead starts to rotate, changes color (moon) and increase elevation again
			// - we need to manually animate this timeframe for sunrise and sunset and pull down the sun elevation to reach night state
			// - we re-use game light direction for the moon more or less directly

			if (rtx_atmosphere_sunElevation && rtx_atmosphere_sunRotation && rtx_atmosphere_moon0_elevation && rtx_atmosphere_moon0_rotation)
			{
				float game_sun_elevation = RAD2DEG(std::asin(dir.z));
				float game_sun_rotation = RAD2DEG(std::atan2(dir.x, dir.y));
				if (game_sun_rotation < 0.0f) {
					game_sun_rotation += 360.0f;
				}

				const int& hour = *game::m_game_clock_hours;
				const int& minute = *game::m_game_clock_minutes;
				const int& seconds = *game::m_game_clock_seconds;

				// sunset normal
				constexpr auto t_sunset_start = 20.0f;					// t when we start pushing sun down towards horizon
				constexpr auto t_sunset_end = 21.00f;					// t when sun reaches slow sunset mode
				constexpr auto deg_sunset_end = 7.0f;					// deg when t = t_sunset_end

				// sunset slow
				constexpr auto t_sunset_slowdown_until = 21.75f;		// t when slow sunset ends
				constexpr auto deg_sunset_slowdown_end = -5.0f;			// deg when t = t_sunset_slowdown_until
				constexpr auto t_sunset_post = 23.0f;					// t when sun reaches -90deg

				// sunrise 
				constexpr auto t_sunrise_prepare = 3.0f;				// t when sun starts to move from -90deg towards horizon

				// sunrise slow
				constexpr auto t_sunrise_start = 5.0f;					// t when slow sunrise starts
				constexpr auto deg_sunrise_start = -2.0f;				// deg when t = t_sunrise_start
				constexpr auto t_sunrise_slowdown_until = 6.0f;			// t when slow sunrise ends
				constexpr auto t_sunrise_end = 8.0f;					// t when sun equals game sun altitude
				constexpr auto deg_sunrise_slowdown_end = 8.0f;			// deg when t = t_sunrise_slowdown_until

				// --

				// save position where moon is at lowest possible altitude before it contintues to only move horizontally
				static float moon_sunrise_5h15m_altitude = 0.0f;
				if (hour == 5 && minute == 15 && seconds < 2) {
					moon_sunrise_5h15m_altitude = game_sun_elevation;
				}

				// save sun rotation val when we hold the sun near the horizon at sunset (t_sunset_end)
				static float sun_sunset_21h00m_rotation = 0.0f;
				if (hour == 21 && minute == 00 && seconds < 2) {
					sun_sunset_21h00m_rotation = game_sun_rotation;
				}

				auto lerp = [](const float& a, const float& b, const float& t) -> float {
						return a + (b - a) * t;
					};

				auto get_sun_elevation = [lerp](const float& t, const float daytime_return_val)
					{
						// sunset: pull sun towards horizon
						if (t >= t_sunset_start && t < t_sunset_end) {
							return lerp(daytime_return_val, deg_sunset_end, (t - t_sunset_start) / (t_sunset_end - t_sunset_start));
						}

						// sunset: slow down
						if (t >= t_sunset_end && t < t_sunset_slowdown_until) {
							return lerp(deg_sunset_end, deg_sunset_slowdown_end, (t - t_sunset_end) / (t_sunset_slowdown_until - t_sunset_end));
						}

						// pull down sun elevation to -90
						if (t >= t_sunset_slowdown_until && t < t_sunset_post) {
							return lerp(deg_sunset_slowdown_end, -90.0f, (t - t_sunset_slowdown_until) / (t_sunset_post - t_sunset_slowdown_until));
						}

						// plateau at -90
						if (t >= t_sunset_post || t < t_sunrise_prepare) {
							return -90.0f;
						}

						// sunrise prepare
						if (t >= t_sunrise_prepare && t < t_sunrise_start) {
							return lerp(-90.0f, deg_sunrise_start, (t - t_sunrise_prepare) / (t_sunrise_start - t_sunrise_prepare));
						}

						// sunrise: slow
						if (t >= t_sunrise_start && t < t_sunrise_slowdown_until) {
							return lerp(deg_sunrise_start, deg_sunrise_slowdown_end, (t - t_sunrise_start) / (t_sunrise_slowdown_until - t_sunrise_start));
						}

						// sunrise
						if (t >= t_sunrise_slowdown_until && t < t_sunrise_end) {
							return lerp(deg_sunrise_slowdown_end, daytime_return_val, (t - t_sunrise_slowdown_until) / (t_sunrise_end - t_sunrise_slowdown_until));
						}

						// in = out otherwise
						return daytime_return_val;
					};

				auto get_moon_elevation = [lerp](const float& t, const float elevation_return_val)
					{
						constexpr auto t_moonset_start = 5.25f;
						constexpr auto t_moonset_end = 7.0f;

						// sunrise: t_sunrise_start → t_moonset_end (0 → 30)
						if (t >= t_moonset_start && t < t_moonset_end) {
							return lerp(moon_sunrise_5h15m_altitude, -15.0f, (t - t_moonset_start) / (t_moonset_end - t_moonset_start));
						}

						// in out at all other times
						return elevation_return_val;
					};

				float t = static_cast<float>(hour)
						+ static_cast<float>(minute) / 60.0f
						+ static_cast<float>(seconds) / 3600.0f;

				const float sun_elevation = get_sun_elevation(t, game_sun_elevation);
				float sun_rotation = game_sun_rotation;

				// we hold the numos sun near horizon to get a longer sunset but the game light starts to rotate
				// (because it starts to act as the moon) so we need to lock the rotation here
				if (t >= t_sunset_end && t < t_sunset_slowdown_until) {
					sun_rotation = sun_sunset_21h00m_rotation;
				}

				if (shared::globals::imgui_menu_open)
				{
					im->m_dbg_vis_sun_lerp_t = t;
					im->m_dbg_vis_sun_elevation = sun_elevation;
					im->m_dbg_vis_sun_rotation = sun_rotation;
					im->m_dbg_vis_sun_frame_count++;
				}

				const auto frametime_s = remix_vars::get()->get_frametime() * 0.001f; // ms to s - min delay by frametime
				const auto UPDATE_DELAY = 0.1f + frametime_s;

				const auto& v1 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(sun_elevation));
				remix_vars::get()->add_interpolate_entry(rtx_atmosphere_sunElevation, v1, UPDATE_DELAY);

				const auto& v2 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(sun_rotation));
				remix_vars::get()->add_interpolate_entry(rtx_atmosphere_sunRotation, v2, UPDATE_DELAY);

				if (hour >= 20 || hour <= 6)
				{
					float moon_elevation = get_moon_elevation(t, game_sun_elevation);

					// directional light past 21 = moon
					const auto& v3 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(moon_elevation));
					remix_vars::get()->add_interpolate_entry(rtx_atmosphere_moon0_elevation, v3, UPDATE_DELAY);
					remix_vars::get()->add_interpolate_entry(rtx_atmosphere_moon0_rotation, v2, UPDATE_DELAY);
				}
			}
		}

		const auto light_list = game::get_renderLights();
		const auto light_count = game::get_renderLightsCount();

		const auto& ignored_lights = map_settings::get()->get_map_settings().ignored_lights;
		const auto& allowed_lights = map_settings::get()->get_map_settings().allow_lights;

		if (comp_settings::get()->translate_game_lights.get_as<bool>() && light_count && light_list)
		{
			for (auto i = 0u; i < light_count; i++)
			{
				auto& def = light_list[i];
				const auto hash = calculate_light_hash(def);

				bool is_filler_light = def.mFlags & 0x10;

				bool add_zero_intensity_light = false;
				bool is_allowed_filler_light = false;

				// debug setting to disable ignore logic (performance impact test)
				if (!im->m_dbg_disable_ignore_light_hash_logic)
				{
					if (ignored_lights.contains(hash))
					{
						// we need this light in the active list to visualize it
						if (im->m_dbg_visualize_api_light_hashes) {
							add_zero_intensity_light = true;
						} else {
							continue;
						}
					}
				}

				// dev setting to test light flags
				if (im->m_dbg_ignore_lights_with_flag_logic)
				{
					if (im->m_dbg_ignore_lights_with_flag_add_second_flag)
					{
						if (def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_01)
							&& def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_02)) {
							continue;
						}
					}
					else
					{
						if (def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_01)) {
							continue;
						}
					}
				}

				// ignore filler light game setting
				else if (gs->translate_game_lights_ignore_filler_lights.get_as<bool>())
				{
					if (is_filler_light)
					{
						// check if this filler light is whitelisted
						if (allowed_lights.contains(hash)) {
							is_allowed_filler_light = true;
						}

						// keep vis. working because the user needs to see the hashes to be able to whitelist them
						else if (im->m_dbg_visualize_api_light_hashes) {
							add_zero_intensity_light = true;
						}

						if (!(is_allowed_filler_light || add_zero_intensity_light)) {
							continue;
						}
					}
				}

				

				bool touched_light = false;
				if (auto it = active_lights.find(hash); it != active_lights.end())
				{
					const auto always_update = it->second.m_def.mFlags & 0x400; 
					const auto custom_game_light = it->second.m_def.mFlags & 0x40000;

					bool should_update = im->m_dbg_visualize_api_light_hashes || always_update; // always update if in vis mode -- or light requests it
					it->second.m_is_filler = is_filler_light;

					// check if most important properties changed - position unchanged as matched a hash
					if (!should_update && compare_dynamic_light_without_position(def, it->second.m_def)) {
						it->second.m_updateframe = updateframe; // light is up to date
					}
					else
					{
						it->second.m_def = def;
						should_update = true; // radius/intensity or something else has changed, update
					}

					if (should_update || add_zero_intensity_light)
					{
						if (add_zero_intensity_light)
						{
							it->second.m_def.mIntensity = 0.0f;
							it->second.m_is_ignored = true;
						} else {
							it->second.m_is_ignored = false;
						}

						it->second.m_is_allowed_filler = is_allowed_filler_light;
						rml->spawn_or_update_remix_sphere_light(it->second, true, custom_game_light);
					}

					touched_light = true;
				}
				else
				{
					// search for light with very very similar settings, then check if within a certain distance comp. to last state
					for (auto& l : active_lights)
					{
						// do not try to match an existing light if flag 0x400 (always update)
						if (l.second.m_def.mFlags & 0x400) {
							continue;
						}

						if (l.second.m_updateframe != updateframe && // do not recheck already updated lights
							compare_dynamic_light_without_position(def, l.second.m_def, 0.05f))
						{
							if (def.mPosition.DistToSqr(l.second.m_def.mPosition) < 1.0f) // expose this?
							{
								l.second.m_def = def;
								l.second.m_hash = hash; // can be used to check if a light changed its position
								rml->spawn_or_update_remix_sphere_light(l.second, true);
								touched_light = true;
								break;
							}
						}
					}
				}

				// this is a new light
				if (!touched_light) {
					rml->add_light(def, hash, add_zero_intensity_light, is_filler_light);
				}
			}
		}
		else {
			rml->destroy_and_clear_all_active_lights();
		}

		// delete all untouched lights
		for (auto it = active_lights.begin(); it != active_lights.end(); )
		{
			if (it->second.m_updateframe != updateframe)
			{
				rml->destroy_light(it->second);
				it = active_lights.erase(it);
			}
			else { ++it; }
		}
	}

	// --------------

	// hooked function call that would normally render a singular headlight in the center until one of the two is defect
	// we do not call the original center headlight function and instead call the singular one two times here. Once for with the left light pos and once with the right light pos.
	void veh_center_headlight(D3DXMATRIX* some_mtx, D3DXMATRIX* left_light_pos, D3DXMATRIX* right_light_pos, [[maybe_unused]] float* pos, float* light_dir, float* color, float intensity, float radius, [[maybe_unused]] std::int64_t ee, int inter_index, int room_index, int shadow_rel_index, char some_flag)
	{
		const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = 0.8f * (1.0f * 20.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC54) * *reinterpret_cast<float*>(0x12E23C8))
		const float outer_cone = 0.8f * (1.0f * 50.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC58) * *reinterpret_cast<float*>(0x12E23CC))

		game::AddSingleVehicleLight(some_mtx, &left_light_pos->m[3][0],
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, 1, some_flag);

		game::AddSingleVehicleLight(some_mtx, &right_light_pos->m[3][0],
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, 1, some_flag);

		/*shared::utils::hook::call<void(__cdecl)(float* _some_mtx, float* _headlight_pos, float* some_vec3, float* _a4, float _cc, float _dd, float _h1, float _h2, int _this_p0x48, int _this_p0x40, int _shadowRelIndex, char _one, char _some_flag)>
			(0xA3DE90)(some_mtx, &left_light_pos->m[3][0], light_dir, color, intensity, radius, inner_cone, outer_cone, inter_index, room_index, 0, 1, some_flag);*/
	}

	__declspec (naked) void veh_center_headlight_stub()
	{
		__asm
		{
			// we are at +100 here
			mov     ecx, [esp + 0x50]; // right_light_pos
			mov     eax, [esp + 0x54]; // left_light_pos
			//add     eax, 0x30; // 12th float (base matrix4x4) .. we push the ptr to the matrix

			push	ecx; // r
			push	eax; // l

			push    dword ptr[ebp + 0x24]; // some_mtx (overwritten with hk) .. normally at stack offs + 0x30

			call	veh_center_headlight;
			add     esp, 0x38; // normally + 0x30 but we added two additional pushes

			mov		eax, game::hk_addr__vehicle_center_headlight;
			add		eax, 11; // skip og add esp op 
			jmp		eax; //  0xA3FE19
		}
	}

	// detoured single headlight func to apply same light settings as in dual mode
	void veh_single_headlight_hk(D3DXMATRIX* some_mtx, float* light_pos, float* light_dir, float* color, float intensity, float radius, [[maybe_unused]] float inner_cone_angle, [[maybe_unused]] float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = 0.8f * (1.0f * 20.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC54) * *reinterpret_cast<float*>(0x12E23C8))
		const float outer_cone = 0.8f * (1.0f * 50.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC58) * *reinterpret_cast<float*>(0x12E23CC))

		game::AddSingleVehicleLight(some_mtx, light_pos,
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	// single center headlight to dual headlight
	void veh_center_rearlight(D3DXMATRIX* some_mtx, D3DXMATRIX* left_light_pos, D3DXMATRIX* right_light_pos, [[maybe_unused]] float* some_vec3, float* color, float intensity, float radius, float inner_cone_angle, float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = inner_cone_angle * 0.6f + gs->translate_vehicle_rearlight_inner_cone_angle_offset.get_as<float>();
		const float outer_cone = outer_cone_angle * 0.6f + gs->translate_vehicle_rearlight_outer_cone_angle_offset.get_as<float>();

		Vector light_dir = { 0.0f, -1.0f, 0.0f };
		light_dir += *gs->translate_vehicle_rearlight_direction_offset.get_as<Vector*>();

		game::AddSingleVehicleLight(some_mtx, &left_light_pos->m[3][0],
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);

		game::AddSingleVehicleLight(some_mtx, &right_light_pos->m[3][0],
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	__declspec (naked) void veh_center_rearlight_stub()
	{
		__asm
		{
			// we are at +0xA0 here
			mov     ecx, [esp + 0x58]; // right_light_pos
			mov     eax, [esp + 0x50]; // left_light_pos
			//add     eax, 0x30; // 12th float (base matrix4x4) .. we push the ptr to the matrix

			push	ecx; // r
			push	eax; // l

			push    dword ptr[ebp + 0x2C]; // some_mtx

			call	veh_center_rearlight;
			add     esp, 0x38; // normally + 0x34 but we omit one push (arg set to 0.0 when single light rendering) with the hook and add two additional pushes

			mov		eax, game::hk_addr__vehicle_center_rearlight;
			add		eax, 16; // skip og add esp op 
			jmp		eax; //  0xA4337E
		}
	}

	// detoured single rearlight func to apply same light settings as in dual mode
	void veh_single_rearlight_hk(D3DXMATRIX* some_mtx, float* light_pos, [[maybe_unused]] float* og_light_dir, float* color, float intensity, float radius, float inner_cone_angle, float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = inner_cone_angle * 0.6f + gs->translate_vehicle_rearlight_inner_cone_angle_offset.get_as<float>();
		const float outer_cone = outer_cone_angle * 0.6f + gs->translate_vehicle_rearlight_outer_cone_angle_offset.get_as<float>();

		Vector light_dir = { 0.0f, -1.0f, 0.0f };
		light_dir += *gs->translate_vehicle_rearlight_direction_offset.get_as<Vector*>();

		game::AddSingleVehicleLight(some_mtx, light_pos,
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	// -----

	// handles "fake" sirenlight waay above the vehicle
	void veh_siren_fake_light_hk(int unk_flag, game::eLightType type, int flag, float* dir, float* otherdir, float* pos, float* color, float intensity, int tex_hash, int txd_hash, float radius, float inner_cone, float outer_cone, int inter_index, int room_index, int shadow_rel_index)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		pos[2] += gs->translate_vehicle_fake_siren_z_offset._float();
		intensity += gs->translate_vehicle_fake_siren_intensity_offset._float();
		radius += gs->translate_vehicle_fake_siren_radius_offset._float();

		game::AddSceneLight(unk_flag, type, flag, dir, otherdir, pos, color, intensity, tex_hash, txd_hash, radius, inner_cone, outer_cone, inter_index, room_index, shadow_rel_index);
	}


	// normally adds deferred/2d lights into the actual sirens - we create actual lights instead (AddProjectedLight)
	void veh_siren_vlights_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		float* light_direction)
	{
		const auto gs = comp_settings::get();

		Vector color =
		{
			(float)r / 255.0f,
			(float)g / 255.0f,
			(float)b / 255.0f
		};

		add_game_light(	gs->translate_vehicle_vsirens_make_spotlight._bool() ? game::eLightType::LT_SPOT : game::eLightType::LT_POINT, 
						pos, color,
						ems_scale + gs->translate_vehicle_vsirens_intensity_offset._float(),
						gs->translate_vehicle_vsirens_radius_offset._float(),
						0.0f, 45.0f, light_direction);

		if (gs->translate_vehicle_vsirens_secondary_spherelight_enabled._bool())
		{
			Vector new_pos = pos; // always add a slight offset so code recognizes it as a new light in case z offset gets set to 0
				   new_pos.z += (gs->translate_vehicle_vsirens_secondary_spherelight_z_offset._float() + 0.01f); 

			add_game_light(	game::eLightType::LT_POINT, new_pos, color,
							ems_scale + gs->translate_vehicle_vsirens_secondary_spherelight_intensity_offset._float(), 
							gs->translate_vehicle_vsirens_secondary_spherelight_radius_offset._float());
		}
	}


	// normally adds deferred/2d lights into the actual sirens - we create actual lights instead
	void veh_bar_siren_vlights_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		[[maybe_unused]] float* light_direction /*nullptr*/)
	{
		const auto gs = comp_settings::get();

		const Vector color = { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f };
		add_game_light(game::eLightType::LT_POINT, pos, color, ems_scale * gs->translate_vehicle_barsirens_intensity_scalar._float(), maybe_radius * gs->translate_vehicle_barsirens_radius_scalar._float());
	}


	// normally adds deferred/2d lights at rear window sirens - we create actual lights instead
	void veh_fbi_buffalo_siren_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		[[maybe_unused]] float* light_direction /*nullptr*/)
	{
		// note: can not create remixApi lights directly because this runs in the main thread and seems to be framerate dependent
		// drawing the light instances later in the same thread does not help either
		// so we create proper game lights which are translated to remixApi lights later

		const Vector color = { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f };
		add_game_light(	game::eLightType::LT_POINT, pos, color, 
						ems_scale * (0.6f /*+ im->m_debug_vector2.x*/), 
						maybe_radius * (0.05f /*+ im->m_debug_vector2.y*/));
	}

	game_lights::game_lights()
	{
		p_this = this;

		// draw two headlights instead of a singular center one
		shared::utils::hook::nop(game::hk_addr__vehicle_center_headlight, 8); // nop push so we start at stack+0x100 - 0xA3FE0E
		shared::utils::hook(game::hk_addr__vehicle_center_headlight, veh_center_headlight_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook::nop(game::nop_addr__vehicle_headlight_prevent_override, 6); // overrides right light pos - 0xA3FCBB
		shared::utils::hook::nop(game::nop_addr__vehicle_headlight_prevent_read, 6); // reads overridden right light pos (prob. not needed) - 0xA3FCF2

		// also hook single light drawing because we adjust light parameters and we want single lights to have the same settings as the two above
		shared::utils::hook(game::hk_addr__vehicle_single_headlight, veh_single_headlight_hk, HOOK_CALL).install()->quick(); // single light mode if one is defect (both use the same func) - 0xA3FF0F

		// rear lights
		shared::utils::hook(game::hk_addr__vehicle_center_rearlight, veh_center_rearlight_stub, HOOK_JUMP).install()->quick(); // center light to two lights - 0xA4336E
		shared::utils::hook(game::hk_addr__vehicle_single_rearlight, veh_single_rearlight_hk, HOOK_CALL).install()->quick(); // single light mode if one is defect (both use the same func) - 0xA4342D

		// "fake" light ontop of v-shaped sirens
		shared::utils::hook(game::hk_addr__vehicle_vshaped_sirens_fake_light, veh_siren_fake_light_hk, HOOK_CALL).install()->quick(); // 0xA40D0A - police / ambulance
		shared::utils::hook(game::hk_addr__vehicle_firetruck_sirens_fake_light, veh_siren_fake_light_hk, HOOK_CALL).install()->quick(); // 0xA4126E - firetruck


		// replace defered/2d light inside sirens with actual lights
		shared::utils::hook(game::hk_addr__vehicle_vshaped_sirens_vlight, veh_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA40AAA - police / ambulance
		shared::utils::hook(game::hk_addr__vehicle_firetruck_sirens, veh_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA4100E - firetruck

		// replace defered/2d light of siren bars
		shared::utils::hook(game::hk_addr__vehicle_barshaped_sirens, veh_bar_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA4146F
		shared::utils::hook(game::hk_addr__vehicle_fbibuffalo_sirens, veh_fbi_buffalo_siren_hk, HOOK_CALL).install()->quick(); // 0xA417DF - fbi rear projected lights



		// -----
		m_initialized = true;
		shared::common::log("GameLights", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
