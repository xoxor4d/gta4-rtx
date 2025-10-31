#include "std_include.hpp"

#include "modules/dinput_hook.hpp"
#include "modules/game_settings.hpp"
#include "modules/imgui.hpp"
#include "modules/map_settings.hpp"
#include "modules/natives.hpp"
#include "modules/remix_lights.hpp"
#include "modules/remix_markers.hpp"
#include "modules/remix_vars.hpp"
#include "modules/renderer.hpp"
#include "modules/renderer_ff.hpp"
#include "shared/common/flags.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	int  g_is_rendering_static = 0;
	int  g_is_rendering_vehicle = 0;
	bool g_is_rendering_phone = false;

	void on_begin_scene_cb()
	{
		static auto im = imgui::get();
		static auto vars = remix_vars::get();
		static auto gs = game_settings::get();

		renderer::get()->m_triggered_remix_injection = false; 
		g_applied_hud_hack = false;

		if (!tex_addons::initialized) {
			tex_addons::init_texture_addons();
		}

		// check if loadscreen was active once
		if (!game::was_loadscreen_active && game::CMenuManager__m_LoadscreenActive && *game::CMenuManager__m_LoadscreenActive) {
			game::was_loadscreen_active = true;
		}

		// check if loadscreen was active but is no longer active -> should be in-game
		if (!game::is_in_game && game::was_loadscreen_active
			&& game::CMenuManager__m_LoadscreenActive && !*game::CMenuManager__m_LoadscreenActive)
		{
			game::is_in_game = true;
		}

		if (game::CMenuManager__m_LoadscreenActive && *game::CMenuManager__m_LoadscreenActive)
		{
			gta4::game::loaded_settings_cfg->nightshadow_quality = 0u;
			gta4::game::loaded_settings_cfg->reflection_quality = 0u;
			gta4::game::loaded_settings_cfg->shadow_quality = 0u;
			gta4::game::loaded_settings_cfg->water_quality = 0u;
		}

		// do not pause if enabled
		if (im->m_do_not_pause_on_lost_focus) {
			*game::ms_bNoBlockOnLostFocus = false;
		}

		if (im->m_do_not_pause_on_lost_focus_changed && !im->m_do_not_pause_on_lost_focus) 
		{
			*game::ms_bNoBlockOnLostFocus = true;
			im->m_do_not_pause_on_lost_focus_changed = false;
		}

		{
			shared::globals::d3d_device->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);

			const auto vp = game::pViewports; //reinterpret_cast<game::g_viewports2*>(0x118D7F0);
			if (vp->sceneviewport)
			{
				shared::globals::d3d_device->SetTransform(D3DTS_VIEW, &vp->sceneviewport->view);
				shared::globals::d3d_device->SetTransform(D3DTS_PROJECTION, &vp->sceneviewport->proj);
			}
		}

		if (game::is_in_game) 
		{
			translate_and_apply_timecycle_settings();

			// Remix sets 'rtx.di.initialSampleCount' to hardcoded values on start
			// and we def. need more then 3 samples to get somewhat good looking vehicle lights
			const auto rtxdi_override_val = gs->remix_override_rtxdi_samplecount.get_as<int>();
			if (rtxdi_override_val) // override if > 0
			{
				static auto rtxdi_samplecount = vars->get_option("rtx.di.initialSampleCount");
				remix_vars::option_value val {.value = (float)rtxdi_override_val };
				vars->set_option(rtxdi_samplecount, val);
			}
		}
	}


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

	void translate_and_apply_timecycle_settings()
	{
		auto unpack_uint32 = [](const uint32_t& in, float* out)
			{
				out[0] = static_cast<float>(in >> 16 & 0xFF) / 255.0f;
				out[1] = static_cast<float>(in >> 8  & 0xFF) / 255.0f;
				out[2] = static_cast<float>(in >> 0  & 0xFF) / 255.0f;
				out[3] = static_cast<float>(in >> 24 & 0xFF) / 255.0f;
			};

		auto mapRange = [](float input, float in_min, float in_max, float out_min, float out_max)
			{
				return out_min + (out_max - out_min) * ((input - in_min) / (in_max - in_min));
			};

		{
			static auto im = imgui::get();
			static auto vars = remix_vars::get();
			static auto gs = game_settings::get();
			remix_vars::option_value val{};


			// TODO! Add defaults for used remix variables in case user disables the saving of ALL remix variables

			//auto first_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8910);
			//auto third_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8B20);

			game::TimeCycleParams* timecycle = game::m_pCurrentTimeCycleParams_01;

			if (game::m_dwCutsceneState && *game::m_dwCutsceneState > 0) {
				timecycle = game::m_pCurrentTimeCycleParams_Cutscene;
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

			static auto rtxSkybrightness = vars->get_option("rtx.skyBrightness");
			if (gs->timecycle_skylight_enabled.get_as<bool>() && rtxSkybrightness)
			{
				val.value = timecycle->mSkyLightMultiplier * gs->timecycle_skylight_scalar.get_as<float>();
				vars->set_option(rtxSkybrightness, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyLightMultiplier);
			}


			static auto rtxBloomBurnIntensity = vars->get_option("rtx.bloom.burnIntensity");
			static auto rtxBloomLuminanceThreshold = vars->get_option("rtx.bloom.luminanceThreshold");
			if (gs->timecycle_bloom_enabled.get_as<bool>() && rtxBloomBurnIntensity && rtxBloomLuminanceThreshold)
			{
				val.value = timecycle->mBloomIntensity * gs->timecycle_bloomintensity_scalar.get_as<float>();
				vars->set_option(rtxBloomBurnIntensity, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomIntensity);

				val.value = timecycle->mBloomThreshold * gs->timecycle_bloomthreshold_scalar.get_as<float>();
				vars->set_option(rtxBloomLuminanceThreshold, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomThreshold);
			}


			static auto rtxTonemapColorBalance = vars->get_option("rtx.tonemap.colorBalance");
			if (gs->timecycle_colorcorrection_enabled.get_as<bool>() && rtxTonemapColorBalance)
			{
				Vector temp_color_offset;
				if (gs->timecycle_colortemp_enabled.get_as<bool>())
				{
					const float nrml_temp = std::clamp(timecycle->mTemperature / 15.0f, -1.0f, 1.0f);
					temp_color_offset.x = nrml_temp * 0.3f;
					temp_color_offset.y = 0.0f;
					temp_color_offset.z = -nrml_temp * 0.3f;
					temp_color_offset *= gs->timecycle_colortemp_influence.get_as<float>();
					im->m_timecyc_curr_mTemperature = timecycle->mTemperature;
					im->m_timecyc_curr_mTemperature_offset = temp_color_offset;
				}

				Vector4D color_correction;
				unpack_uint32(timecycle->mColorCorrection, &color_correction.x);
				val.vector[0] = color_correction.x + temp_color_offset.x;
				val.vector[1] = color_correction.y + temp_color_offset.y;
				val.vector[2] = color_correction.z + temp_color_offset.z;
				vars->set_option(rtxTonemapColorBalance, val);
				ASSIGN_IMGUI_VIS_UNPACKED_COLOR(mColorCorrection, color_correction);
			}


			static auto rtxTonemapSaturation = vars->get_option("rtx.tonemap.saturation");
			if (gs->timecycle_desaturation_enabled.get_as<bool>() && rtxTonemapSaturation)
			{
				const float far_desaturation_influence = gs->timecycle_fardesaturation_influence.get_as<float>() * mapRange(timecycle->mDesaturationFar, 0.0f, 1.0f, 0.0f, 0.4f);
				val.value = 1.0f - ((1.0f - timecycle->mDesaturation) * gs->timecycle_desaturation_influence.get_as<float>());
				val.value -= far_desaturation_influence;
				vars->set_option(rtxTonemapSaturation, val);
				ASSIGN_IMGUI_VIS_FLOAT(mDesaturation);
				im->m_timecyc_curr_mDesaturationFar = timecycle->mDesaturationFar;
				im->m_timecyc_curr_mDesaturationFar_offset = far_desaturation_influence;
			}


			static auto rtxTonemapExposureBias = vars->get_option("rtx.tonemap.exposureBias");
			if (gs->timecycle_gamma_enabled.get_as<bool>() && rtxTonemapExposureBias)
			{
				val.value = -(1.0f - timecycle->mGamma) + gs->timecycle_gamma_offset.get_as<float>();
				vars->set_option(rtxTonemapExposureBias, val);
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
			if (gs->timecycle_fogcolor_enabled.get_as<bool>() && rtxVolumetricsSingleScatteringAlbedo)
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
				
				vars->set_option(rtxVolumetricsSingleScatteringAlbedo, val);
				ASSIGN_IMGUI_VIS_VEC3(singleScatteringAlbedo);
			}

			float atmos_height = 0.0f;
			static auto rtxVolumetricsAtmosphereHeightMeters = vars->get_option("rtx.volumetrics.atmosphereHeightMeters");
			if (gs->timecycle_skyhorizonheight_enabled.get_as<bool>() && rtxVolumetricsAtmosphereHeightMeters)
			{
				atmos_height = timecycle->mSkyHorizonHeight * 100.0f * gs->timecycle_skyhorizonheight_scalar.get_as<float>();
				val.value = atmos_height;
				vars->set_option(rtxVolumetricsAtmosphereHeightMeters, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyHorizonHeight);
			}

			static auto rtxVolumetricsTransmittanceMeasurementDistanceMeters = vars->get_option("rtx.volumetrics.transmittanceMeasurementDistanceMeters");
			if (gs->timecycle_fogdensity_enabled.get_as<bool>() && rtxVolumetricsTransmittanceMeasurementDistanceMeters)
			{
				val.value = mapRange(fog_color_density.w, 0.0f, 0.9f, 200.0f, 0.6f)
					* gs->timecycle_fogdensity_influence_scalar.get_as<float>()
					+ mapRange(atmos_height, 0.0f, 1000.0f,
						gs->timecycle_skyhorizonheight_low_transmittance_offset.get_as<float>(),
						gs->timecycle_skyhorizonheight_high_transmittance_offset.get_as<float>());

				if (val.value < 0.6f) {
					val.value = 0.6f;
				}

				vars->set_option(rtxVolumetricsTransmittanceMeasurementDistanceMeters, val);
				im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters = val.value;
			}
		}
	}

#undef ASSIGN_IMGUI_VIS_FLOAT

	void post_vehicle_rendering()
	{
		g_is_rendering_vehicle = 0;
	}

	/*__declspec (naked) void pre_static_surfs_stub()
	{
		static uint32_t retn_addr = 0x42EC46;
		__asm
		{
			push	ebp;
			shl		eax, 2;
			push	edi;
			mov		g_is_rendering_static, 1;
			jmp		retn_addr;
		}
	}

	__declspec (naked) void post_static_surfs_stub()
	{
		__asm
		{
			mov		g_is_rendering_static, 0;
			retn	0x10;
		}
	}*/


	void reset_world_transform()
	{
		const auto& dev = shared::globals::d3d_device;
		dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);
	}

	__declspec (naked) void pre_entity_surfs_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF0;
			mov		g_is_rendering_static, 1;
			jmp		game::retn_addr__pre_entity_surfs_stub;
		}
	}

	__declspec (naked) void post_entity_surfs_stub()
	{
		__asm
		{
			pushad;
			call	reset_world_transform;
			popad;
			mov		g_is_rendering_static, 0;
			pop     ebx;
			mov     esp, ebp;
			pop     ebp;
			retn;
		}
	}

	// -------------

	__declspec (naked) void pre_vehicle_surfs_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF0;
			mov		g_is_rendering_vehicle, 1;
			jmp		game::retn_addr__pre_vehicle_surfs_stub;
		}
	}

	__declspec (naked) void post_vehicle_surfs_stub()
	{
		__asm
		{
			pushad;
			call	post_vehicle_rendering;
			popad;

			pop     ebx;
			mov     esp, ebp;
			pop     ebp;
			retn;
		}
	}

	// -------

	struct unkown_struct_culling
	{
		BYTE gap0[176];
		game::grcViewport viewport;
	};


	DETOUR_TYPEDEF(static_world_culling_check, BOOL, __thiscall, void* this_ptr, unkown_struct_culling* unk);
	BOOL __fastcall static_world_culling_check_hk(void* this_ptr, [[maybe_unused]] void* fastcall, unkown_struct_culling* unk)
	{
		// this = AVCBuilding : AVCEntity : AUCVirtualBase
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		if (im->m_dbg_never_cull_statics) {
			return TRUE;
		}

		const auto& nc_dist_near = gs->nocull_dist_near_static.get_as<float>();
		const auto& nc_dist_med = gs->nocull_dist_medium_static.get_as<float>();
		const auto& nc_radius_med = gs->nocull_radius_medium_static.get_as<float>();
		const auto& nc_dist_far = gs->nocull_dist_far_static.get_as<float>();
		const auto& nc_radius_far = gs->nocull_radius_far_static.get_as<float>();
		const auto& nc_height_far = gs->nocull_height_far_static.get_as<float>();

		// calculate distance to object
		Vector4D object_origin = {};
		shared::utils::hook::call_virtual<20, void, Vector4D*>(this_ptr, &object_origin);

		const Vector cam_org = &unk->viewport.cameraInv.m[3][0];
		const float dist_sqr = fabs(cam_org.DistToSqr(object_origin)); //

		// do not cull if near
		if (dist_sqr < nc_dist_near * nc_dist_near) {
			return TRUE;
		}

		const float object_radius = shared::utils::hook::call_virtual<22, float>(this_ptr);

		// do not cull if obj is within medium dist and larger then medium radius setting
		if (nc_dist_med > 0.0f && dist_sqr < nc_dist_med * nc_dist_med)
		{
			if (object_radius > nc_radius_med) {
				return TRUE;
			}
		}

		// do not cull if obj is within far dist and larger then far radius setting
		if (nc_dist_far > 0.0f && dist_sqr < nc_dist_far * nc_dist_far)
		{
			if (object_radius > nc_radius_far) {
				return TRUE;
			}

			const auto object_mins = shared::utils::hook::call_virtual<24, Vector*>(this_ptr);
			const auto object_maxs = shared::utils::hook::call_virtual<25, Vector*>(this_ptr);

			float object_height = 0.0f;
			if (object_mins && object_maxs) {
				object_height = object_maxs->z - object_mins->z;
			}

			if (object_height > 0.0f && object_height > nc_height_far) {
				return TRUE;
			}
		}

		return static_world_culling_check_og(this_ptr, unk);
	}

	// A general culling function used by lights, npc's and other? things ..
	int __fastcall FrustumPlanesCheck(game::grcViewport* vp, [[maybe_unused]] void* fastcall_arg, float orgX, float orgY, float orgZ, float distanceToObject, float* outAdjustedNearDistance)
	{
		// do not cull if near enough
		if (const auto vpscene = game::pViewports; vpscene->sceneviewport)
		{
			const float anti_cull_dist = game_settings::get()->nocull_dist_lights.get_as<float>();

			const Vector cam_org = &vpscene->sceneviewport->cameraInv.m[3][0];
			const float dist_sqr = fabs(cam_org.DistToSqr(Vector(orgX, orgY, orgZ)));

			// do not cull if near
			if (dist_sqr < anti_cull_dist * anti_cull_dist) {
				return 2;
			}
		}

		// original code
		if (outAdjustedNearDistance)
		{
			*outAdjustedNearDistance = vp->frustumClipPlane0[0] * orgX + vp->frustumClipPlane0[1] * orgY + vp->frustumClipPlane0[2] * orgZ + vp->frustumClipPlane0[3] + distanceToObject;
			if (*outAdjustedNearDistance >= 0.0f)
			{
				int v8 = 1; // returning 0 stops rendering of player
				for (float* i = &vp->frustumClipPlane1[2]; *(i - 2) * orgX + *(i - 1) * orgY + *i * orgZ + i[1] + distanceToObject >= 0.0f; i += 4)
				{
					if (++v8 > 5) {
						return 1;
					}
				}
			}
		}
		else
		{
			int result = 2; 
			auto plane = &vp->frustumClipPlane0[2];
			int plane_index = 0;
			while (true)
			{
				const float signed_dist = ((((*(plane - 2) * orgX) + (*(plane - 1) * orgY)) + (*plane * orgZ)) + plane[1]) + distanceToObject;
				if (signed_dist < 0.0f) {
					break;
				}

				if (distanceToObject > signed_dist) {
					result = 1;
				}

				++plane_index;
				plane += 4;

				if (plane_index >= 6) {
					return result;
				}
			}
		}

		return 0;
	}

	typedef void(__cdecl ProcessGameInput_t)(bool);
	ProcessGameInput_t* ProcessGameInput_og = nullptr;

	// https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/ea06ae1b55bca037800e6fe74e6ab9925a298f72/source/windowed.ixx#L100
	void process_game_input(bool arg)
	{
		if (*game::ms_bWindowed && *game::ms_bFocusLost) 
		{
			uint32_t counter = 0u;
			while (::ShowCursor(TRUE) < 0 && ++counter < 3) {}
			return;
		}

		ProcessGameInput_og(arg);
	}

	void main()
	{
		// init remix api
		shared::common::remix_api::initialize(nullptr, nullptr, nullptr, false);

		shared::common::loader::module_loader::register_module(std::make_unique<imgui>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer_ff>());
		shared::common::loader::module_loader::register_module(std::make_unique<dinput>());
		shared::common::loader::module_loader::register_module(std::make_unique<remix_lights>());
		shared::common::loader::module_loader::register_module(std::make_unique<map_settings>());
		shared::common::loader::module_loader::register_module(std::make_unique<remix_markers>());
		shared::common::loader::module_loader::register_module(std::make_unique<natives>());
		shared::common::loader::module_loader::register_module(std::make_unique<remix_vars>());

		//shared::utils::hook(0x415648, pre_non_blended_surfs, HOOK_JUMP).install()->quick();

		// 42EC80 draws static stuff?
		//shared::utils::hook(0x42EC41, pre_static_surfs_stub, HOOK_JUMP).install()->quick();
		//shared::utils::hook(0x42ECB7, post_static_surfs_stub, HOOK_JUMP).install()->quick();

		// 8DD4CE to jmp to not render nico
		// 8DCDBB to jmp to not render vehicles
		// 8DCC0B to jmp to not render a bunch of stuff

		// 
		shared::utils::hook(game::retn_addr__pre_entity_surfs_stub - 5u, pre_entity_surfs_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook(game::hk_addr__post_entity_surfs_stub, post_entity_surfs_stub, HOOK_JUMP).install()->quick();

		// CDrawFrag
		shared::utils::hook(game::retn_addr__pre_vehicle_surfs_stub - 5u, pre_vehicle_surfs_stub, HOOK_JUMP).install()->quick(); // 0x8DCDA1
		shared::utils::hook(game::hk_addr__post_vehicle_surfs_stub, post_vehicle_surfs_stub, HOOK_JUMP).install()->quick();

		// reduce culling 01 - low lod variants do not get culled outside frustum
		// 0x431E52 from: 0F 84 B1 00 00 00 (je 0x431F09) -- to: E9 A6 00 00 00 90 (jmp 0x431EFD)
		// ++ 0x431EFD mov eax,00000001 to mov eax,00000002 to disable all frustum culling (still keeps low lod variant)

		// detour function that builds the render list of static objects? -> add distance based check
		shared::utils::hook::detour(game::hk_addr__static_world_culling_check_hk, &static_world_culling_check_hk, DETOUR_CAST(static_world_culling_check_og));
		shared::utils::hook::nop(game::nop_addr__static_world_frustum_patch01, 6); // disable secondary frustum based check for static objects by "returning 2"
		shared::utils::hook::nop(game::nop_addr__static_world_frustum_patch02, 2); // ^

		// light culling check 0xABD093 - detour frustum check function
		shared::utils::hook::detour(game::hk_addr__frustum_check, &FrustumPlanesCheck, nullptr);

		// -----
		// disable unused rendering
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_01, 5); //0xABD872
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_02, 5); //0xADD8E1
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_03, 5); //0xADD938
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_04, 5); //0xADD9C7
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_05, 5); //0xADDA4D
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_06, 5); //0xADDAD2
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_07, 5); //0xADDB17
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_08, 5); //0xADDB5A

		// warp shadow phase
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_09, 2); //0xD781EA
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_01); //0xD781F3
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_02); //0xD77A0D

		// water and mirror reflection phase
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_03); //0xD76C23
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_04); //0xD76964

		// height phase
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_05); //0xD61CFA
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_06); //0xD61BAB

		// reflection phase
		shared::utils::hook::nop(game::nop_addr__disable_unused_rendering_10, 2); //0xD518DD
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_07); //0xD518E6
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_08); //0xD5116B

		// interior reflection phase
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_09); //0xD514FD
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_10); //0xD50F8B

		// cascaded shadow
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__disable_unused_rendering_11); //0x928AE5

		// hk_addr__prevent_game_input_func
		shared::utils::hook::detour(game::hk_addr__prevent_game_input_func, process_game_input, (LPVOID*)&ProcessGameInput_og);

		MH_EnableHook(MH_ALL_HOOKS);
	}
}
