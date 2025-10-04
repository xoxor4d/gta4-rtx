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
#include "shared/common/flags.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	int  g_is_rendering_static = 0;
	int  g_is_rendering_vehicle = 0;
	bool g_is_rendering_phone = false;

	void on_begin_scene_cb()
	{
		renderer::get()->m_triggered_remix_injection = false;

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

		{
			shared::globals::d3d_device->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);

			const auto vp = game::pViewports; //reinterpret_cast<game::g_viewports2*>(0x118D7F0);
			if (vp->sceneviewport)
			{
				shared::globals::d3d_device->SetTransform(D3DTS_VIEW, &vp->sceneviewport->view);
				shared::globals::d3d_device->SetTransform(D3DTS_PROJECTION, &vp->sceneviewport->proj);
			}
		}



		auto Float4ToU32 = [](const float* inColor, uint32_t& outColor)
			{
				outColor = 0;
				outColor |= static_cast<uint32_t>(inColor[0] * 255.0f + 0.5f) << 16;
				outColor |= static_cast<uint32_t>(inColor[1] * 255.0f + 0.5f) << 8;
				outColor |= static_cast<uint32_t>(inColor[2] * 255.0f + 0.5f) << 0;
				outColor |= static_cast<uint32_t>(inColor[3] * 255.0f + 0.5f) << 24;
			};

		auto U32ToFloat4 = [](const uint32_t inColor, float* outColor)
			{
				outColor[0] = static_cast<float>(inColor >> 16 & 0xFF) / 255.0f;
				outColor[1] = static_cast<float>(inColor >>  8 & 0xFF) / 255.0f;
				outColor[2] = static_cast<float>(inColor >>  0 & 0xFF) / 255.0f;
				outColor[3] = static_cast<float>(inColor >> 24 & 0xFF) / 255.0f;
			};

		auto mapRange = [](float input, float in_min, float in_max, float out_min, float out_max)
			{
			return out_min + (out_max - out_min) * ((input - in_min) / (in_max - in_min));
			};

		{
			static auto im = imgui::get();
			static auto vars = remix_vars::get();
			static auto rtxSkybrightness = vars->get_option("rtx.skyBrightness");

			if (rtxSkybrightness)
			{
				auto skylight = game::m_pCurrentTimeCycleParams->mSkyLightMultiplier * im->m_dbg_timecyc_debug01_scalar; // 0.08?
				remix_vars::option_value val{ .value = skylight };
				vars->set_option(rtxSkybrightness, val);


				val.value = game::m_pCurrentTimeCycleParams->mBloomIntensity * 1.0f;
				vars->set_option(vars->get_option("rtx.bloom.burnIntensity"), val);

				val.value = game::m_pCurrentTimeCycleParams->mBloomThreshold * 1.0f;
				vars->set_option(vars->get_option("rtx.bloom.luminanceThreshold"), val);


				Vector4D colorCorrection;
				U32ToFloat4(game::m_pCurrentTimeCycleParams->mColorCorrection, &colorCorrection.x);

				val.vector[0] = colorCorrection.x;
				val.vector[1] = colorCorrection.y;
				val.vector[2] = colorCorrection.z;
				vars->set_option(vars->get_option("rtx.tonemap.colorBalance"), val);

				val.value = (1.0f - game::m_pCurrentTimeCycleParams->mDesaturation) * 0.5f;
				vars->set_option(vars->get_option("rtx.tonemap.saturation"), val);

				val.value = -(1.0f - game::m_pCurrentTimeCycleParams->mGamma);
				vars->set_option(vars->get_option("rtx.tonemap.exposureBias"), val);

				val.value = log2(game::m_pCurrentTimeCycleParams->mLumMin * 0.01f) + im->m_dbg_timecyc_debug01_offset; // +6?
				vars->set_option(vars->get_option("rtx.autoExposure.evMinValue"), val);

				val.value = log2(game::m_pCurrentTimeCycleParams->mLumMax * 0.01f) + im->m_dbg_timecyc_debug02_offset; // +6?
				vars->set_option(vars->get_option("rtx.autoExposure.evMaxValue"), val);

				Vector4D fogColorDensity;
				U32ToFloat4(game::m_pCurrentTimeCycleParams->mSkyBottomColorFogDensity, &fogColorDensity.x); 

				val.vector[0] = fogColorDensity.x * im->m_dbg_timecyc_fog_start_scalar;
				val.vector[1] = fogColorDensity.y * im->m_dbg_timecyc_fog_start_scalar;
				val.vector[2] = fogColorDensity.z * im->m_dbg_timecyc_fog_start_scalar;
				vars->set_option(vars->get_option("rtx.volumetrics.singleScatteringAlbedo"), val);

				const float atmosHeight = game::m_pCurrentTimeCycleParams->mSkyHorizonHeight * 100.0f * im->m_dbg_timecyc_debug02_scalar;
				val.value = atmosHeight;
				vars->set_option(vars->get_option("rtx.volumetrics.atmosphereHeightMeters"), val);

				//im->m_dbg_timecyc_fog_start_scalar;
				//val.value = (1.0f - fogColorDensity.w) /*game::m_pCurrentTimeCycleParams->mFogStart*/ * im->m_dbg_timecyc_fog_density_scalar;
				val.value = mapRange(fogColorDensity.w, 0.0f, 1.0f, 20.0f, 200.0f) /*game::m_pCurrentTimeCycleParams->mFogStart*/ * im->m_dbg_timecyc_fog_density_scalar;
				vars->set_option(vars->get_option("rtx.volumetrics.transmittanceMeasurementDistanceMeters"), val);


				
				

				int asd = 0;
			}

			int y = 0;
		}
	}

	

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
		static auto gs = game_settings::get();

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

	void main()
	{
		// init remix api
		shared::common::remix_api::initialize(nullptr, nullptr, nullptr, false);

		shared::common::loader::module_loader::register_module(std::make_unique<imgui>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer>());
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

		MH_EnableHook(MH_ALL_HOOKS);
	}
}
