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
	void force_graphic_settings()
	{
		if (gta4::game::loaded_settings_cfg)
		{
			gta4::game::loaded_settings_cfg->nightshadow_quality = 0u;
			gta4::game::loaded_settings_cfg->reflection_quality = 0u;
			gta4::game::loaded_settings_cfg->shadow_quality = 0u;
			gta4::game::loaded_settings_cfg->water_quality = 0u;
			gta4::game::loaded_settings_cfg->sharpness = 0u; // fix cutscene crashing issue on amd cards
		}
	}

	void on_begin_scene_cb()
	{
		static auto im = imgui::get();

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


		// force graphic settings when menu is active
		if (game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive) {
			force_graphic_settings();
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


		// camera setup
		{
			shared::globals::d3d_device->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);

			const auto vp = game::pViewports; //reinterpret_cast<game::g_viewports2*>(0x118D7F0);
			if (vp->sceneviewport)
			{
				shared::globals::d3d_device->SetTransform(D3DTS_VIEW, &vp->sceneviewport->view);
				shared::globals::d3d_device->SetTransform(D3DTS_PROJECTION, &vp->sceneviewport->proj);
			}
		}
	}

	// ----

	struct unkown_struct_culling
	{
		BYTE gap0[176];
		game::grcViewport viewport;
	};

	DETOUR_TYPEDEF(static_world_culling_check, BOOL, __thiscall, void* this_ptr, unkown_struct_culling* unk);
	BOOL __fastcall static_world_culling_check_hk(game::CEntity* this_ptr, [[maybe_unused]] void* fastcall, unkown_struct_culling* unk)
	{
		// this = AVCBuilding : AVCEntity : AUCVirtualBase
		auto im = imgui::get();
		auto gs = game_settings::get();

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

		const auto object_mins = shared::utils::hook::call_virtual<24, Vector*>(this_ptr);
		const auto object_maxs = shared::utils::hook::call_virtual<25, Vector*>(this_ptr);

		float object_height = 0.0f;
		if (object_mins && object_maxs) {
			object_height = object_maxs->z - object_mins->z;
		}

		bool dbg_vis_added = false, dbg_vis_forced_visible = false;
		const float dbg_vis_draw_dist = im->m_dbg_visualize_anti_cull_info_distance;

		if (im->m_dbg_visualize_anti_cull_info
			&& im->visualized_anti_cull.size() < 160
			&& object_radius > im->m_dbg_visualize_anti_cull_info_min_radius
			&& dist_sqr < dbg_vis_draw_dist * dbg_vis_draw_dist)
		{
			std::lock_guard lock(im->visualized_anti_cull_mutex); // multi thread
			imgui::visualized_anti_cull_s vis = { {object_origin.x, object_origin.y, object_origin.z}, object_radius, object_height, this_ptr->m_wModelIndex };

			if (im->m_dbg_visualize_anti_cull_highlight && im->m_dbg_visualize_anti_cull_highlight == this_ptr->m_wModelIndex)
			{
				// transform mins/maxs 
				if (this_ptr->worldTransform)
				{
					D3DXVECTOR3 corners[8];
					corners[0] = object_mins->ToD3DXVector();
					corners[1] = D3DXVECTOR3(object_maxs->x, object_mins->y, object_mins->z);
					corners[2] = D3DXVECTOR3(object_mins->x, object_maxs->y, object_mins->z);
					corners[3] = D3DXVECTOR3(object_maxs->x, object_maxs->y, object_mins->z);
					corners[4] = D3DXVECTOR3(object_mins->x, object_mins->y, object_maxs->z);
					corners[5] = D3DXVECTOR3(object_maxs->x, object_mins->y, object_maxs->z);
					corners[6] = D3DXVECTOR3(object_mins->x, object_maxs->y, object_maxs->z);
					corners[7] = object_maxs->ToD3DXVector();

					D3DXVECTOR3 world_mins(FLT_MAX, FLT_MAX, FLT_MAX), world_maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);

					D3DXMATRIX world = *this_ptr->worldTransform;
					world.m[0][3] = 0.0f;
					world.m[1][3] = 0.0f;
					world.m[2][3] = 0.0f;
					world.m[3][3] = 1.0f;

					for (auto i = 0u; i < 8u; ++i)
					{
						D3DXVec3TransformCoord(&corners[i], &corners[i], &world);
						world_mins.x = std::min(world_mins.x, corners[i].x);
						world_mins.y = std::min(world_mins.y, corners[i].y);
						world_mins.z = std::min(world_mins.z, corners[i].z);
						world_maxs.x = std::max(world_maxs.x, corners[i].x);
						world_maxs.y = std::max(world_maxs.y, corners[i].y);
						world_maxs.z = std::max(world_maxs.z, corners[i].z);
					}

					vis.mins.x = world_mins.x;
					vis.mins.y = world_mins.y;
					vis.mins.z = world_mins.z;

					vis.maxs.x = world_maxs.x;
					vis.maxs.y = world_maxs.y;
					vis.maxs.z = world_maxs.z;
				}
				else
				{
					vis.mins = *object_mins + object_origin;
					vis.maxs = *object_maxs + object_origin;
				}

				vis.draw_debug_box = true;
				dbg_vis_forced_visible = true;
			}

			im->visualized_anti_cull.emplace_back(vis);
			dbg_vis_added = true;
		}

		// perfect -> Map Settings
		/*if (im->m_dbg_int_01)
		{
			if (this_ptr->m_wModelIndex == im->m_dbg_int_01) {
				return TRUE;
			}
		}*/

		// do not cull if obj is within medium dist and larger then medium radius setting
		if (nc_dist_med > 0.0f && dist_sqr < nc_dist_med * nc_dist_med)
		{
			if (object_radius > nc_radius_med) {
				return TRUE;
			}

			/*if (im->m_debug_vector3.x > 0.0f && object_origin.z + object_height >= im->m_debug_vector3.x) {
				return TRUE;
			}*/
		}

		// do not cull if obj is within far dist and larger then far radius setting
		if (nc_dist_far > 0.0f && dist_sqr < nc_dist_far * nc_dist_far)
		{
			if (object_radius > nc_radius_far) {
				return TRUE;
			}

			if (object_height > 0.0f && object_height > nc_height_far) {
				return TRUE;
			}

			/*if (im->m_debug_vector3.y > 0.0f && object_origin.z + object_height >= im->m_debug_vector3.y) {
				return TRUE;
			}*/
		}

		if (const auto& ms = map_settings::get_map_settings(); !ms.anticull_meshes.empty())
		{
			for (const auto& cat : ms.anticull_meshes)
			{
				// dist 0 = always active
				if (!cat.distance || (int)dist_sqr < cat.distance * cat.distance)
				{
					if (cat.indices.contains(this_ptr->m_wModelIndex)) {
						return TRUE;
					}
				}
			}
		}

		// if vis was added but we did not return (to force vis) - draw as white text
		if (dbg_vis_added && !dbg_vis_forced_visible
			&& im->m_dbg_visualize_anti_cull_info && !im->visualized_anti_cull.empty())
		{
			std::lock_guard lock(im->visualized_anti_cull_mutex); // multi thread
			im->visualized_anti_cull.back().forced_visible = false;
		}

		return static_world_culling_check_og(this_ptr, unk);
	}



	int extended_anticull_hk(game::CEntity* ent)
	{
		const auto im = imgui::get();
		const auto gs = game_settings::get();

		if (ent && gs->nocull_extended.get_as<bool>())
		{
			if (im->m_dbg_extended_anticull_always_true) {
				return TRUE;
			}

			const auto viewport = game::pViewports;
			if (viewport && viewport->sceneviewport)
			{
				// calculate distance to object
				Vector4D object_origin = {};
				shared::utils::hook::call_virtual<20, void, Vector4D*>(ent, &object_origin);

				const Vector cam_org = &viewport->sceneviewport->cameraInv.m[3][0];
				const float dist_sqr = fabs(cam_org.DistToSqr(object_origin)); //

				if (const auto& ms = map_settings::get_map_settings(); !ms.anticull_meshes.empty())
				{
					for (const auto& cat : ms.anticull_meshes)
					{
						// dist 0 = always active
						if (!cat.distance || (int)dist_sqr < cat.distance * cat.distance)
						{
							if (cat.indices.contains(ent->m_wModelIndex)) {
								return TRUE;
							}
						}
					}
				}
			}
		}

		return FALSE;
	}
	
	__declspec(naked) void extended_anticull_stub()
	{
		__asm
		{
			pushad;
			push	esi; // ent
			call	extended_anticull_hk;
			add		esp, 4;
			cmp		eax, 1; // check if returned 1
			je		SKIP;
			popad;

			push    ebp;				// og
			mov     ebp, [esp + 0x18];	// og
			jmp		game::retn_addr__extended_anti_culling_check_stub;

		SKIP:
			popad;
			pop		ebx; // there is one push in front of our hook
			jmp		game::jmp_addr__extended_anti_culling_check_stub; // 0xAE8735
		}
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


	// hooked function call that would normally render a singular headlight in the center until one of the two is defect
	// we do not call the original center headlight function and instead call the singular one two times here. Once for with the left light pos and once with the right light pos.
	void veh_center_headlight(D3DXMATRIX* some_mtx, D3DXMATRIX* left_light_pos, D3DXMATRIX* right_light_pos, [[maybe_unused]] float* pos, float* light_dir, float* color, float intensity, float radius, [[maybe_unused]] std::int64_t ee, int inter_index, int room_index, int shadow_rel_index, char some_flag)
	{
		const auto im = imgui::get();
		const auto gs = game_settings::get();

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
		const auto gs = game_settings::get();

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
		const auto gs = game_settings::get();

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
		const auto gs = game_settings::get();

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

		// reduce culling 01 - low lod variants do not get culled outside frustum
		// 0x431E52 from: 0F 84 B1 00 00 00 (je 0x431F09) -- to: E9 A6 00 00 00 90 (jmp 0x431EFD)
		// ++ 0x431EFD mov eax,00000001 to mov eax,00000002 to disable all frustum culling (still keeps low lod variant)

		// detour function that builds the render list of static objects? -> add distance based check
		shared::utils::hook::detour(game::hk_addr__static_world_culling_check_hk, &static_world_culling_check_hk, DETOUR_CAST(static_world_culling_check_og));
		shared::utils::hook::nop(game::nop_addr__static_world_frustum_patch01, 6); // disable secondary frustum based check for static objects by "returning 2"
		shared::utils::hook::nop(game::nop_addr__static_world_frustum_patch02, 2); // ^


		shared::utils::hook(game::retn_addr__extended_anti_culling_check_stub - 5u, extended_anticull_stub, HOOK_JUMP).install()->quick();


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

		// (c) https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix/blob/fcb91f0c9629a25de4941ce55312798d591d109c/source/settings.ixx#L772
		// FF places a jmp here to allow game vis in certain menus - we always want the game to draw -> nop FF hook
		shared::utils::hook::nop(game::nop_addr__always_draw_game_in_menus, 5);

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

		MH_EnableHook(MH_ALL_HOOKS);
	}
}
