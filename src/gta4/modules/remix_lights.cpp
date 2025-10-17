#include "std_include.hpp"
#include "remix_lights.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"
#include "map_settings.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	// ----

	/**
	 * Spawns or updates a remixApi spherelight
	 * @param light			The light
	 * @return			True if successfull
	 */
	bool remix_lights::spawn_or_update_remix_sphere_light(remix_light_def& light)
	{
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		if (light.m_handle) {
			destroy_light(light);
		}

		light.m_updateframe = m_updateframe;

		const auto& def = light.m_def;
		light.m_ext.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
		light.m_ext.pNext = nullptr;
		light.m_ext.position = def.mPosition.ToRemixFloat3D();

		// scale down lights with larger radii (eg. own vehicle headlight (75 rad))
		light.m_ext.radius = 20.0f * (1.0f - exp(-light.m_def.mRadius / 20.0f)) * gs->translate_game_light_radius_scalar.get_as<float>() * 0.01f;
		light.m_ext.shaping_hasvalue = def.mType == game::LT_SPOT;
		light.m_ext.shaping_value = {};
		light.m_ext.shaping_value.direction = def.mDirection.ToRemixFloat3D();

		//const float innerConeDegrees = RAD2DEG(def.mOuterConeAngle);  // "outer" param → actual inner cone (smaller)
		const float outerConeDegrees = RAD2DEG(def.mInnerConeAngle);  // "inner" param → outer cone (larger)
		const float coneSoftness = std::cos(def.mOuterConeAngle * 0.5f) - std::cos(def.mInnerConeAngle * 0.5f);

		light.m_ext.shaping_value.coneAngleDegrees = outerConeDegrees + gs->translate_game_light_angle_offset.get_as<float>();
		light.m_ext.shaping_value.coneSoftness = coneSoftness + gs->translate_game_light_softness_offset.get_as<float>();
		light.m_ext.shaping_value.focusExponent = 0.0f;

		light.m_ext.volumetricRadianceScale = def.mVolumeScale *
			(def.mType == game::LT_SPOT ? 
				  gs->translate_game_light_spotlight_volumetric_radiance_scale.get_as<float>() 
				: gs->translate_game_light_spherelight_volumetric_radiance_scale.get_as<float>());

		light.m_info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
		light.m_info.pNext = &light.m_ext;
		light.m_info.hash = light.m_hash;
		light.m_info.radiance = (def.mIntensity * Vector(def.mColor) * gs->translate_game_light_intensity_scalar.get_as<float>()).ToRemixFloat3D();

		// car headlight = 75

		bool ignore = false;
		/*if (def.mRadius >= 50.0f)
		{
			ignore = true;
		}*/

		const auto& api = shared::common::remix_api::get();

		bool return_val = true;
		if (!ignore) {
			return_val = api.m_bridge.CreateLight(&light.m_info, &light.m_handle) == REMIXAPI_ERROR_CODE_SUCCESS;
		}

		return return_val;
	}


	/**
	 * Adds and immediately spawns a single light
	 * @param def	The game light definition
	 */
	void remix_lights::add_light(const game::CLightSource& def, const uint64_t& hash, const bool add_but_do_not_draw)
	{
		if (def.mType == game::LT_POINT || def.mType == game::LT_SPOT)
		{
			m_active_lights.emplace_back(
				remix_light_def
				{
					.m_def = def,
					.m_hash = hash,
					.m_light_num = m_active_light_spawn_tracker++,
				});

			// add light with 0 intensity (eg. for debug vizualizations)
			if (add_but_do_not_draw)
			{
				if (auto* light = &m_active_lights.back(); light) 
				{
					light->m_def.mIntensity = 0.0f;
					light->m_is_ignored = true;
				}
			}

			// spawn light
			if (auto* light = &m_active_lights.back(); light) {
				get()->spawn_or_update_remix_sphere_light(*light);
			}
		}
	}

	/**
	 * Destroys a light (remixApi light)
	 * @param l		The light to destroy
	 */
	void remix_lights::destroy_light(remix_light_def& l)
	{
		if (l.m_handle)
		{
			shared::common::remix_api::get().m_bridge.DestroyLight(l.m_handle);
			l.m_handle = nullptr;
		}
	}

	/**
	 * Destroys all lights in 'm_map_lights' (remixApi lights)
	 */
	void remix_lights::destroy_all_lights()
	{
		for (auto& l : m_active_lights) {
			destroy_light(l);
		}
	}

	/**
	 * Destroys all lights in 'm_map_lights' (remixApi lights) and clears 'm_map_lights'
	 */
	void remix_lights::destroy_and_clear_all_active_lights()
	{
		destroy_all_lights();
		m_active_lights.clear();
	}

	uint64_t calculate_light_hash(const game::CLightSource& def)
	{
		std::uint32_t hash = 0u;

		hash = shared::utils::hash32_combine(hash, def.mPosition.x);
		hash = shared::utils::hash32_combine(hash, def.mPosition.y);
		hash = shared::utils::hash32_combine(hash, def.mPosition.z);

		/*if (shared::utils::float_equal(def.mPosition.x, 890.579651f))
		{
			int x = 1;
		}*/

		hash = shared::utils::hash32_combine(hash, def.mFlags);
		hash = shared::utils::hash32_combine(hash, def.mRoomIndex);
		//hash = shared::utils::hash32_combine(hash, def.mInteriorIndex); // this changes when starting a new game from an old save
		hash = shared::utils::hash32_combine(hash, def.mTxdHash);

		//hash = shared::utils::hash32_combine(hash, def.mVolumeSize);
		//hash = shared::utils::hash32_combine(hash, def.mVolumeScale);
		//hash = shared::utils::hash32_combine(hash, def.mIntensity);
		
		return hash;
	}

	/**
	 * Updates all lights in 'm_map_lights'
	 * Handles Destroying, choreo trigger spawning, tick advancing and updating of remixApi lights
	 */
	void remix_lights::iterate_all_game_lights()
	{
		static auto im = imgui::get();
		static auto gs = game_settings::get();
		static auto& api = shared::common::remix_api::get();

		destroy_and_clear_all_active_lights();

		//if (true)
		{
			if (game::g_directionalLights)
			{
				auto& def = game::g_directionalLights[0];
				auto& l = m_distant_light;

				if (l.m_handle)
				{
					api.m_bridge.DestroyLight(l.m_handle);
					l.m_handle = nullptr;
				}

				l.m_updateframe = m_updateframe;

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

				l.m_info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
				l.m_info.pNext = &l.m_ext;
				l.m_info.hash = shared::utils::string_hash64("apilight_distant");
				l.m_info.radiance = (def.mIntensity * Vector(def.mColor) * gs->translate_sunlight_intensity_scalar.get_as<float>()).ToRemixFloat3D();

				if (api.m_bridge.CreateLight(&l.m_info, &l.m_handle) == REMIXAPI_ERROR_CODE_SUCCESS && l.m_handle) {
					api.m_bridge.DrawLightInstance(l.m_handle);
				}
			}
		}
#if 1
		const auto light_list = game::get_renderLights();
		const auto light_count = game::get_renderLightsCount();

		if (game_settings::get()->translate_game_lights.get_as<bool>() && light_count && light_list)
		{
			for (auto i = 0u; i < light_count; i++)
			{
				auto& def = light_list[i];
				const auto hash = calculate_light_hash(def);

				bool add_zero_intensity_light = false;

				// debug setting to disable ignore logic (performance impact test)
				if (!im->m_dbg_disable_ignore_light_hash_logic)
				{
					const auto& ignored_lights = map_settings::get()->get_map_settings().ignored_lights;
					if (ignored_lights.contains(hash))
					{
						// we need this light in the active list to visualize it
						if (im->m_dbg_visualize_api_light_hashes) {
							add_zero_intensity_light = true;
						}
						else {
							continue;
						}
					}
				}

				// dev setting to test light flags
				if (im->m_dbg_ignore_lights_with_flag_logic)
				{
					if (im->m_dbg_ignore_lights_with_flag_add_second_flag)
					{
						if (   def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_01)
							&& def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_02) ) {
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
					if (def.mFlags & 0x10) {
						continue;
					}
				}

				add_light(def, hash, add_zero_intensity_light);
			}
		}

#else
		// d3d9 lights test

		if (game::g_lightList && game::g_lightCount && *game::g_lightCount)
		{
			for (auto i = 0u; i < 7; i++) {
				shared::globals::d3d_device->LightEnable(i, FALSE);
			}

			const auto global_scalar = imgui::get()->m_dbg_global_light_intensity_scalar;
			int lindex = 0;

			for (auto i = 0u; i < *game::g_lightCount && i < 256; i++)
			{
				if (lindex > 7) {
					lindex = 0;
				}

				auto& def = game::g_lightList[i];

				D3DLIGHT9 light = {};
				light.Type = (def.mOuterConeAngle > def.mInnerConeAngle) ? D3DLIGHT_SPOT : D3DLIGHT_POINT;

				// Position
				light.Position.x = def.mPosition.x;
				light.Position.y = def.mPosition.y;
				light.Position.z = def.mPosition.z;

				// Direction for spot light
				if (light.Type == D3DLIGHT_SPOT) {
					light.Direction.x = def.mDirection.x;
					light.Direction.y = def.mDirection.y;
					light.Direction.z = def.mDirection.z;

					// Map 0-1 cone angles to radians (assuming fraction of pi for full cone angles)
					light.Theta = def.mInnerConeAngle * M_PI;  // Inner full cone angle
					light.Phi = def.mOuterConeAngle * M_PI;    // Outer full cone angle
					light.Falloff = 1.0f;  // Standard linear falloff in penumbra
				}

				// Ignore mTangent as D3DLIGHT9 does not support oriented area lights directly

				// Color and intensity scaling
				// Assume mColor is RGBA with RGB in [0,1] range, mIntensity is radiance, mRadius is source radius
				// Scale to effective point light intensity: pi * mIntensity * mRadius^2
				// This preserves far-field illuminance when converted to small fixed-radius sphere in Remix
				const float effectiveIntensity = M_PI * def.mIntensity * def.mRadius * def.mRadius * global_scalar;

				light.Diffuse.r = def.mColor.x * effectiveIntensity;
				light.Diffuse.g = def.mColor.y * effectiveIntensity;
				light.Diffuse.b = def.mColor.z * effectiveIntensity;
				light.Diffuse.a = def.mColor.w;  // Usually 1.0

				// Set specular to match diffuse (common for dynamic lights)
				light.Specular.r = light.Diffuse.r;
				light.Specular.g = light.Diffuse.g;
				light.Specular.b = light.Diffuse.b;
				light.Specular.a = 1.0f;

				// Ambient typically 0 for dynamic lights
				light.Ambient.r = 0.0f;
				light.Ambient.g = 0.0f;
				light.Ambient.b = 0.0f;
				light.Ambient.a = 1.0f;

				// Attenuation for inverse-square falloff: simulates point light E = I / d^2
				light.Attenuation0 = 0.0f;
				light.Attenuation1 = 0.0f;
				light.Attenuation2 = 1.0f;

				// Large range to effectively make it infinite (end distance computed from attenuation)
				light.Range = 100000.0f;

				shared::globals::d3d_device->SetLight(lindex, &light);
				shared::globals::d3d_device->LightEnable(lindex, TRUE);

				lindex++;
			}
		}
#endif
	}

	// Draw all active map lights
	void remix_lights::draw_all_active_lights()
	{
		for (auto& l : m_active_lights)
		{
			if (l.m_handle) {
				shared::common::remix_api::get().m_bridge.DrawLightInstance(l.m_handle);
			}
		}
	}

	// #
	// #

	void remix_lights::on_client_frame()
	{
		static auto rml = remix_lights::get();
		static auto im = imgui::get();

		// check if paused
		rml->m_is_paused = *game::CMenuManager__m_MenuActive; //shared::utils::float_equal(shared::globals::frame_time_ms, 0.0f);

		if (!rml->m_is_paused)
		{
			rml->m_updateframe++;
			rml->iterate_all_game_lights();
		}

		rml->draw_all_active_lights();

		Vector player_pos;
		player_pos = game::FindPlayerCentreOfWorld(&player_pos);

		if (im->m_dbg_visualize_api_lights)
		{
			game::CLightSource* list = game::get_renderLights();
			const auto count = game::get_renderLightsCount();

			for (auto i = 0u; count; i++)
			{
				auto& def = list[i];
				if (def.mDirection.LengthSqr() == 0.0f) {
					break;
				}

				const Vector circle_pos = def.mPosition;

				if (fabs(circle_pos.DistTo(player_pos)) > 20.0f) {
					continue;
				}

				const float radius = def.mRadius * (game_settings::get()->translate_game_light_radius_scalar.get_as<float>() * 0.01f);
				auto& remixapi = shared::common::remix_api::get();

				remixapi.add_debug_circle(circle_pos, Vector(0.0f, 0.0f, 1.0f), radius, radius * 0.5f, def.mColor, false);
				remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 0, 90), Vector(1.0f, 1.0f, 1.0f));

				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 90, 0), Vector(1.0f, 1.0f, 1.0f));
				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(90, 0, 90), Vector(1.0f, 1.0f, 1.0f));
			}
		}
	}

	// called before map_settings
	void remix_lights::on_map_load()
	{
		// reset spawn tracker
		m_active_light_spawn_tracker = 0u;
	}


	void on_render_light_list_hk()
	{
		remix_lights::on_client_frame();
	}

	void __declspec(naked) on_render_light_list_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF0;

			pushad;
			call	on_render_light_list_hk;
			popad;

			jmp		game::retn_addr__on_render_light_list_stub;
		}
	}

	remix_lights::remix_lights()
	{
		p_this = this;

		shared::utils::hook(game::retn_addr__on_render_light_list_stub - 5u, on_render_light_list_stub, HOOK_JUMP).install()->quick(); // 0xAC1031

		// -----
		m_initialized = true;
		std::cout << "[REMIX_LIGHTS] loaded\n";
	}
}
