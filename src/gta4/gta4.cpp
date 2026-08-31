#include "std_include.hpp"

#include "modules/dinput_hook.hpp"
#include "modules/game_lights.hpp"
#include "modules/comp_settings.hpp"
#include "modules/discord.hpp"
#include "modules/imgui.hpp"
#include "modules/map_settings.hpp"
#include "modules/natives.hpp"
#include "modules/remix_lights.hpp"
#include "modules/remix_markers.hpp"
#include "modules/remix_vars.hpp"
#include "modules/renderer.hpp"
#include "modules/renderer_ff.hpp"
#include "modules/timecycle.hpp"
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
		GTA4_PERF_SCOPE(performance_section::BeginScene);
		const auto im = imgui::get();
		const auto& gs = comp_settings::get();

		renderer::get()->m_triggered_remix_injection = false; 
		g_applied_hud_hack = false;
		g_applied_phone_hack = false;

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

		if (gs->limit_option_sliders._bool())
		{
			if (game::loaded_settings_cfg->view_distance > 29u) {
				game::loaded_settings_cfg->view_distance = 29u;
			}

			if (game::loaded_settings_cfg->detail_distance > 34u) {
				game::loaded_settings_cfg->detail_distance = 34u;
			}

			if (game::loaded_settings_cfg->vehicle_density > 29u) {
				game::loaded_settings_cfg->vehicle_density = 29u;
			}
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

		// called in remix_vars::on_client_frame() otherwise
		/*if (!comp_settings::get()->timecycle_set_on_endscene.get_as<bool>()) {
			timecycle::translate_and_apply_timecycle_settings();
		}*/
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
		auto gs = comp_settings::get();

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


	// extended anti culling for objects defined via map_settings
	int extended_anticull_hk(game::CEntity* ent)
	{
		const auto im = imgui::get();
		const auto cs = comp_settings::get();

		if (ent && cs->nocull_extended.get_as<bool>())
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

				if (cs->nocull_extended_auto._bool())
				{
					const float object_radius = shared::utils::hook::call_virtual<22, float>(ent);
					const auto object_mins = shared::utils::hook::call_virtual<24, Vector*>(ent);
					const auto object_maxs = shared::utils::hook::call_virtual<25, Vector*>(ent);

					float object_height = 0.0f;
					if (object_mins && object_maxs) {
						object_height = object_maxs->z - object_mins->z;
					}

					const float& nc_dist = cs->nocull_extended_dist._float();

					// allow 0 dist to consider all meshes
					if (shared::utils::float_equal(nc_dist, 0.0f) || (nc_dist > 0.0f && dist_sqr < nc_dist * nc_dist))
					{
						// 0 "disables" the check, letting it pass. Both need to be fullfilled to not cull the object
						const bool radius_ok = shared::utils::float_equal(object_radius, 0.0f) || object_radius > cs->nocull_extended_radius._float();
						const bool height_ok = shared::utils::float_equal(object_height, 0.0f) || object_height > cs->nocull_extended_height._float();

						if (radius_ok && height_ok) {
							return TRUE;
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
	int __fastcall frustum_planes_check_hk(game::grcViewport* vp, [[maybe_unused]] void* fastcall_arg, float orgX, float orgY, float orgZ, float distanceToObject, float* outAdjustedNearDistance)
	{
		// do not cull if near enough
		if (const auto vpscene = game::pViewports; vpscene->sceneviewport)
		{
			const auto n = natives::get();

			if (const float& anti_cull_dist = comp_settings::get()->nocull_dist_lights._float(); 
				anti_cull_dist > 0.0f)
			{
				const Vector cam_org = &vpscene->sceneviewport->cameraInv.m[3][0];
				const float dist_sqr = fabs(cam_org.DistToSqr(Vector(orgX, orgY, orgZ)));

				bool use_original_code = false;

				// do not cull if near
				if (dist_sqr < anti_cull_dist * anti_cull_dist) 
				{
					// fix issue where cherise mission never ends because char never despawns when anticull dist is too large
					if (fabs(cam_org.DistToSqr(Vector(-328.75f, 1610.53f, 20.43f))) < 4.0f)
					{
						if (n->HaveAnimsLoaded((char*)"misscherise")) {
							use_original_code = true;
						}
					}
					
					if (!use_original_code) {
						return 2;
					}
				}
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

	// --

	int frustum_panes_check_interior_helper(const game::CRenderPhase_frustum* frustum, const Vector* obj_pos)
	{
		if (const float& nocull_dist_sphere_interior = comp_settings::get()->nocull_dist_sphere_interior._float();
			nocull_dist_sphere_interior > 0.0f)
		{
			const float dist_sqr = fabs(frustum->viewpos.DistToSqr(*obj_pos));

			// do not cull if near
			if (dist_sqr < nocull_dist_sphere_interior * nocull_dist_sphere_interior) {
				return 1;
			}
		}

		return 0;
	}

	__declspec(naked) void frustum_planes_check_interior_stub()
	{
		__asm
		{
			pushad;
			push	eax; // obj pos
			push	ecx; // CRenderPhase_frustum
			call	frustum_panes_check_interior_helper;
			add		esp, 8;
			cmp		eax, 1;
			je		SKIP; // jump (force render) if returned 1
			popad;

			movss   xmm0, dword ptr[ecx + 0x10]; // og
			jmp		game::retn_addr__frustum_check_interior_objs; // stock culling check - 0xA0FCF9

		SKIP:
			popad;
			mov     al, 0; // no cull
			retn    8;
		}
	}


	// ---
	// area / sector anti-culling adding the lowest LOD outside the camera frustum

	void add_far_grid_map_sections(const int render_context)
	{
		const auto cs = comp_settings::get();
		const auto sector_count = cs->nocull_map_areas_count._int();

		// reset temp overrides here in case we return early
		if (!cs->nocull_map_areas._bool() || !cs->nocull_map_areas_always_draw_lowest_lod._bool())
		{
			cs->nocull_dist_far_static.set_temp_override_state(false);
			cs->nocull_radius_far_static.set_temp_override_state(false);
			cs->nocull_height_far_static.set_temp_override_state(false);
		}

		if (!cs->nocull_map_areas._bool() || sector_count < 1 || !game::pCurrentRenderPhase || !*game::pCurrentRenderPhase || !game::AddMapSectionsInFrustum) {
			return;
		}

		const auto& vp = game::get_current_renderphase()->viewport_0xB0;
		const float x_of_up = vp.cameraInv.m[1][0];
		const float y_of_up = vp.cameraInv.m[1][1];

		// cameraInv translation = frustum / view origin
		const float frustum_x = vp.cameraInv.m[3][0];
		const float frustum_y = vp.cameraInv.m[3][1];

		float px = 0.0f, py = 0.0f, pz = 0.0f;
		if (vp.isPersp)
		{
			px = -vp.scalex;
			py = -vp.scaley;
			pz = vp.scaley;
		}
		else
		{
			px = vp.stream_local_min_x;
			py = vp.stream_local_min_y;
			pz = vp.stream_local_max_y;
		}

		const float k_scale = 0.005f;
		const float k_bias = 15.0f;

		const float xx = (frustum_x + px) * k_scale + k_bias;
		const float yy = (frustum_y + py) * k_scale + k_bias;
		const float xy = (frustum_x + py) * k_scale + k_bias;
		const float yz = (frustum_y + pz) * k_scale + k_bias;

		const float cx = (xx + xy) * 0.5f;
		const float cy = (yy + yz) * 0.5f;
		const float rx = (xy - xx) * 0.5f + static_cast<float>(sector_count);
		const float ry = (yz - yy) * 0.5f + static_cast<float>(sector_count);

		float poly[8] = {};
		poly[0] = cx - rx; poly[1] = cy - ry;
		poly[2] = cx + rx; poly[3] = cy - ry;
		poly[4] = cx + rx; poly[5] = cy + ry;
		poly[6] = cx - rx; poly[7] = cy + ry;

		game::AddMapSectionsInFrustum(poly, 4, render_context,
									  reinterpret_cast<void(__cdecl*)(int, int, int, int)>(game::fn_addr__mark_render_sector_far_callback),
									  x_of_up >= 0.0f, y_of_up >= 0.0f, fabsf(x_of_up) > fabsf(y_of_up), 0);

		// always add lowest LOD section so that it can be affected by other anti culling code
		// nocull_map_areas_always_draw_lowest_lod will overwrite the far anti culling cascade object radius and distance
		// so that the lowest LOD section is always drawn
		if (cs->nocull_map_areas_always_draw_lowest_lod._bool())
		{
			// basically places frustum high up in the sky looking down
			// poly spans the full sector grid
			constexpr float dbg_x_of_up = 0.0f;
			constexpr float dbg_y_of_up = 1.0f;

			const float map_min = 0.0f;
			const float map_max = 30.0f; // 30 == full XY map in sector space

			float dbg_poly[8] = {};
			dbg_poly[0] = map_min; dbg_poly[1] = map_min;
			dbg_poly[2] = map_max; dbg_poly[3] = map_min;
			dbg_poly[4] = map_max; dbg_poly[5] = map_max;
			dbg_poly[6] = map_min; dbg_poly[7] = map_max;

			game::AddMapSectionsInFrustum(dbg_poly, 4, render_context,
										  reinterpret_cast<void(__cdecl*)(int, int, int, int)>(game::fn_addr__mark_render_sector_far_callback),
										  dbg_x_of_up >= 0.0f, dbg_y_of_up >= 0.0f, fabsf(dbg_x_of_up) > fabsf(dbg_y_of_up), 0);

			// temp comp setting overrides
			cs->nocull_dist_far_static.set_temp_override_state(true, "Override by NoCull Always Draw Lowest LOD logic");
			cs->nocull_dist_far_static.set_var(8000.0f);

			cs->nocull_radius_far_static.set_temp_override_state(true, "Override by NoCull Always Draw Lowest LOD logic");
			cs->nocull_radius_far_static.set_var(200.0f);

			cs->nocull_height_far_static.set_temp_override_state(true, "Override by NoCull Always Draw Lowest LOD logic");
			cs->nocull_height_far_static.set_var(cs->nocull_map_areas_always_draw_lowest_lod_min_size._float());
		}
	}

	DWORD g_far_grid_stub_helper = 0u;
	__declspec(naked) void add_far_grid_map_sections_stub()
	{
		__asm
		{
			call	game::func_addr_AddMapSectionsInFrustum; // og call to 0xD62DC0
			add     esp, 0x20; // og

			mov		g_far_grid_stub_helper, edi;
			pushad;
			push    g_far_grid_stub_helper;
			call    add_far_grid_map_sections;
			add     esp, 4;
			popad;

			jmp     game::retn_addr__add_far_grid_map_sections; // 0xAEA14D
		}
	}


	// ---
	// area / sector anti-culling adding the highest LOD outside the camera frustum

	void add_near_grid_map_sections(const int render_context)
	{
		const auto cs = comp_settings::get();
		const auto sector_count = 1;

		if (!cs->nocull_map_areas_high_lod_logic._bool() || sector_count < 1 || !game::pCurrentRenderPhase || !*game::pCurrentRenderPhase || !game::AddMapSectionsInFrustum) {
			return;
		}

		const auto& vp = game::get_current_renderphase()->viewport_0xB0;
		const float x_of_up = vp.cameraInv.m[1][0];
		const float y_of_up = vp.cameraInv.m[1][1];
		const float behind_distance = cs->nocull_map_areas_high_lod_logic_distance._float();

		const float frustum_x = vp.cameraInv.m[3][0];
		const float frustum_y = vp.cameraInv.m[3][1];

		float half_w = 0.0f;
		if (vp.isPersp) {
			half_w = vp.scalex;
		} else {
			half_w = (vp.stream_local_max_x - vp.stream_local_min_x) * 0.5f;
		}

		const float k_scale = 0.02f; 
		const float k_bias = 60.0f;
		const float expand_world = static_cast<float>(sector_count) / k_scale;
		half_w += expand_world;

		if (behind_distance <= 0.0f) {
			return;
		}

		// HQ band behind the camera - near edge at view origin, far edge at behind_distance
		const float half_d = behind_distance * 0.5f;

		float fwd_x = -vp.view.m[0][2];
		float fwd_y = -vp.view.m[1][2];

		
		if (const float fwd_len_sq = (fwd_x * fwd_x) + (fwd_y * fwd_y); 
						fwd_len_sq > 1e-6f) 
		{
			const float inv_len = 1.0f / sqrtf(fwd_len_sq);
			fwd_x *= inv_len;
			fwd_y *= inv_len;
		}
		else 
		{
			fwd_x = 0.0f;
			fwd_y = 1.0f;
		}

		const float right_x = -fwd_y;
		const float right_y = fwd_x;
		const float center_x = frustum_x - (fwd_x * half_d);
		const float center_y = frustum_y - (fwd_y * half_d);

		const float world_corners[4][2] = 
		{
			{ center_x + (-right_x * half_w) + (-fwd_x * half_d), center_y + (-right_y * half_w) + (-fwd_y * half_d) },
			{ center_x + ( right_x * half_w) + (-fwd_x * half_d), center_y + ( right_y * half_w) + (-fwd_y * half_d) },
			{ center_x + ( right_x * half_w) + ( fwd_x * half_d), center_y + ( right_y * half_w) + ( fwd_y * half_d) },
			{ center_x + (-right_x * half_w) + ( fwd_x * half_d), center_y + (-right_y * half_w) + ( fwd_y * half_d) },
		};

		float poly[8] = {};
		for (int i = 0; i < 4; ++i) 
		{
			poly[i * 2 + 0] = (world_corners[i][0] * k_scale) + k_bias;
			poly[i * 2 + 1] = (world_corners[i][1] * k_scale) + k_bias;
		}

		game::AddMapSectionsInFrustum(poly, 4, render_context,
									  reinterpret_cast<void(__cdecl*)(int, int, int, int)>(game::fn_addr__mark_render_sector_near_callback),
									  x_of_up >= 0.0f, y_of_up >= 0.0f, fabsf(x_of_up) > fabsf(y_of_up), 0);
	}

	__declspec(naked) void add_near_grid_map_sections_stub()
	{
		__asm
		{
			call	game::func_addr_AddMapSectionsInFrustum; // og call to 0xD62DC0
			push    edi;
			call    add_near_grid_map_sections;
			add     esp, 4;
			jmp     game::retn_addr__add_near_grid_map_sections; // 0xAE9EDC
		}
	}
	
	// ---------------------

	__declspec(naked) void veh_nullptr_crash_fix_stub()
	{
		__asm
		{
			push    esi;				// og
			mov     esi, [esp + 0x10];  // og

			test	ecx, ecx; // ecx is moved in to edi after this check and ecx can be a nullptr for some reason
			jz		NULLPTR;
			jmp		game::retn_addr__veh_nullptr_crash_fix

		NULLPTR:
			push    edi; // og
			jmp		game::retn_addr__veh_nullptr_crash_fix_skip
		}
	}

	__declspec(naked) void veh_nullptr_invalid_model_fix_stub()
	{
		__asm
		{
			cmp		ecx, 0xCDCDCDCD;
			jz		INVALID_MODEL;

			// model valid:
			mov     eax, [ecx + 5]; // og
			and		al, 3;			// og
			jmp		game::retn_addr__veh_invalid_model_crash_fix;

		INVALID_MODEL:
			pop     edi;
			pop     esi;
			mov     esp, ebp;
			pop     ebp;
			retn    0xC;

		}
	}

#if 0
	void start_renderlist_perf()
	{
		/*if (const auto p = performance_logger::get(); p) {
			p->begin_frame();
		}

		GTA4_PERF_BEGIN(performance_section::DrawIndexedPrim);*/
	}

	void end_renderlist_perf()
	{
		/*GTA4_PERF_END(performance_section::DrawIndexedPrim);

		if (const auto p = performance_logger::get(); p) {
			p->end_frame(shared::globals::frame_time_ms);
		}*/
	}

	__declspec(naked) void start_renderlists_stub()
	{
		static uint32_t func_addr = 0x942130;
		static uint32_t retn_addr = 0x8D8C36;
		__asm
		{
			pushad;
			call	start_renderlist_perf;
			popad;

			call	func_addr;

			pushad;
			call	end_renderlist_perf;
			popad;

			jmp		retn_addr;

		}
	}
#endif

	// ---

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
		shared::common::loader::module_loader::register_module(std::make_unique<performance_logger>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer_ff>());
		shared::common::loader::module_loader::register_module(std::make_unique<dinput>());
		shared::common::loader::module_loader::register_module(std::make_unique<remix_lights>());
		shared::common::loader::module_loader::register_module(std::make_unique<game_lights>());
		shared::common::loader::module_loader::register_module(std::make_unique<map_settings>());
		shared::common::loader::module_loader::register_module(std::make_unique<timecycle>());
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

		// extended anti culling for objects defined via map_settings - helpful for multi-story buildings where each story is a single obj
		shared::utils::hook(game::retn_addr__extended_anti_culling_check_stub - 5u, extended_anticull_stub, HOOK_JUMP).install()->quick(); // 0xAE8735

		// light culling check 0xABD093 - detour frustum check function - can lead to issues when a script waits for certain objects to culled?
		shared::utils::hook::detour(game::hk_addr__frustum_check, &frustum_planes_check_hk, nullptr); // 0x431E40

		// reduce interior culling
		shared::utils::hook::detour(game::retn_addr__frustum_check_interior_objs - 5u, &frustum_planes_check_interior_stub, nullptr); // 0xA0FCF9

		// area / sector anti-culling - add lowest LOD outside of camera frustum
		shared::utils::hook(game::retn_addr__add_far_grid_map_sections - 8u, add_far_grid_map_sections_stub, HOOK_JUMP).install()->quick(); // 0xAEA145

		// area / sector anti-culling - add highest LOD outside of camera frustum (near player)
		shared::utils::hook(game::retn_addr__add_near_grid_map_sections - 5u, add_near_grid_map_sections_stub, HOOK_JUMP).install()->quick(); // 0xAE9ED7

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

		// fix a nullptr access in certain missions (cop cars?) - only happens with FF enabled
		// might be an FF issue or an issue with siren / vehicle replacements?
		// https://github.com/xoxor4d/gta4-rtx/issues/49
		shared::utils::hook(game::retn_addr__veh_nullptr_crash_fix - 5u, veh_nullptr_crash_fix_stub, HOOK_JUMP).install()->quick();

		// detect invalid mesh / part and prevent accessing it - https://github.com/xoxor4d/gta4-rtx/issues/49
		shared::utils::hook(game::retn_addr__veh_invalid_model_crash_fix - 5u, veh_nullptr_invalid_model_fix_stub, HOOK_JUMP).install()->quick();

		// test
		//shared::utils::hook(0x8D8C31, start_renderlists_stub, HOOK_JUMP).install()->quick();

		MH_EnableHook(MH_ALL_HOOKS);
	}
}
