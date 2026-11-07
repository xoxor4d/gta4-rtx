#include "std_include.hpp"

#include "modules/dinput_hook.hpp"
#include "modules/game_lights.hpp"
#include "modules/comp_settings.hpp"
#include "modules/renderer_api.hpp"
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
#include "modules/water.hpp"
#include "modules/distant_lights.hpp"
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

		renderer_api::on_begin_scene();
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
			if (game::loaded_settings_cfg->view_distance > 59u) {
				game::loaded_settings_cfg->view_distance = 59u;
			}

			if (game::loaded_settings_cfg->detail_distance > 64u) {
				game::loaded_settings_cfg->detail_distance = 64u;
			}

			if (game::loaded_settings_cfg->vehicle_density > 59u) {
				game::loaded_settings_cfg->vehicle_density = 59u;
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


	// ---
	// anti culling mesh inspector (mapsettings ui)
	// the snapshot runs on the game thread before the scene traversal, so it sees the finished culling state of the previous frame

	namespace anticull_inspector
	{
		using frustum_reason = imgui::anticull_inspector_s::frustum_reason;
		using lod_view_mode = imgui::anticull_inspector_s::lod_view_mode;
		using instance_s = imgui::anticull_inspector_s::instance_s;
		using trace_s = imgui::anticull_inspector_s::trace_s;

		constexpr std::size_t MAX_INSTANCES = 1500u;
		constexpr std::uint32_t SNAPSHOT_INTERVAL_FRAMES = 4u;

		std::vector<instance_s> snapshot;
		std::uint32_t frame_counter = 0u;

		std::uint32_t phase_bit(const void* phase) {
			return 1u << *reinterpret_cast<const int*>(static_cast<const std::uint8_t*>(phase) + game::RENDER_PHASE_OFFSET_INDEX);
		}

		Vector entity_position(const game::CEntity* ent) {
			return ent->worldTransform ? Vector(&ent->worldTransform->m[3][0]) : ent->m_pos;
		}

		// temporarily hidden meshes and the lod level view (job threads)
		bool hides(const game::CEntity* ent)
		{
			const auto& ins = imgui::get()->m_anticull_inspector;
			switch (ins.active_lod_view.load(std::memory_order_relaxed))
			{
				case lod_view_mode::lods:
					if (ent->m_pLod && !(ent->m_dwFlags & 0x40)) {
						return true;
					}
					break;

				case lod_view_mode::lowest:
					if (ent->m_pLod) {
						return true;
					}
					break;

				default:
					break;
			}

			const auto count = ins.hidden_count.load(std::memory_order_acquire);
			for (auto i = 0u; i < count; ++i)
			{
				const auto& slot = ins.hidden_slots[i];

				if (slot.entity.load(std::memory_order_relaxed) == ent && slot.model_index.load(std::memory_order_relaxed) == ent->m_wModelIndex) {
					return true;
				}
			}

			return false;
		}

		// called for every perspective render phase while it marks its map cells
		void capture_primary_phase(const game::CRenderPhase_struct* phase)
		{
			if (phase && phase->is_primary)
			{
				auto& ins = imgui::get()->m_anticull_inspector;
				ins.primary_phase_bit.store(phase_bit(phase), std::memory_order_relaxed);
				ins.primary_lod_scale.store(*reinterpret_cast<const float*>(reinterpret_cast<const std::uint8_t*>(phase) + game::RENDER_PHASE_OFFSET_LOD_SCALE), std::memory_order_relaxed);
			}
		}

		// returns the trace of the entity for its current visit, resets it when the entity got visited again. Needs the inspector mutex.
		trace_s& trace_for_visit(imgui::anticull_inspector_s& ins, const game::CEntity* ent)
		{
			auto& t = ins.trace;
			if (t.entity != ent || t.stamp != ent->m_dwVisibilityStamp) {
				t = { .entity = ent, .stamp = ent->m_dwVisibilityStamp };
			}

			return t;
		}

		// frustum stage of the traced entity (job threads)
		void trace_frustum(const game::CEntity* ent, const void* phase, const BOOL visible, const frustum_reason reason)
		{
			auto& ins = imgui::get()->m_anticull_inspector;
			if (!ins.enabled || !ent || ent != ins.traced_entity.load(std::memory_order_relaxed)) {
				return;
			}

			const auto bit = phase_bit(phase);
			std::lock_guard lock(ins.mutex);
			auto& t = trace_for_visit(ins, ent);

			t.frustum_tested |= bit;
			if (visible) {
				t.frustum_passed |= bit;
			}

			if (bit & ins.primary_phase_bit.load(std::memory_order_relaxed)) {
				t.primary_reason = reason;
			}
		}

		// occlusion stage (job threads). Runs for every visited entity that is not past its lod distance, so it also remembers the traversal stamp.
		void trace_occlusion(const game::CEntity* ent, const bool forced)
		{
			auto& ins = imgui::get()->m_anticull_inspector;
			if (!ins.enabled || !ent) {
				return;
			}

			if (ins.traversal_stamp.load(std::memory_order_relaxed) != ent->m_dwVisibilityStamp) {
				ins.traversal_stamp.store(ent->m_dwVisibilityStamp, std::memory_order_relaxed);
			}

			if (ent != ins.traced_entity.load(std::memory_order_relaxed)) {
				return;
			}

			std::lock_guard lock(ins.mutex);
			auto& t = trace_for_visit(ins, ent);
			t.occlusion_tested = true;
			t.occlusion_forced = forced;
		}

		// collects building instances around the camera and publishes them to the ui (game thread)
		void update()
		{
			auto& ins = imgui::get()->m_anticull_inspector;
			if (!ins.enabled || ++frame_counter % SNAPSHOT_INTERVAL_FRAMES) {
				return;
			}

			PROFILE_ZONE();
			const auto pool = game::ppBuildingPool ? *game::ppBuildingPool : nullptr;
			const auto vp = game::pViewports;

			if (!pool || !pool->m_pObjects || !pool->m_pFlags || !vp || !vp->sceneviewport || !game::g_modelPointers) {
				return;
			}

			const Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];
			const auto stamp = ins.traversal_stamp.load(std::memory_order_relaxed);
			const auto primary_bit = ins.primary_phase_bit.load(std::memory_order_relaxed);
			const auto lod_scale = ins.primary_lod_scale.load(std::memory_order_relaxed);

			snapshot.clear();
			for (auto slot = 0; slot < pool->m_nSize; ++slot)
			{
				if (pool->m_pFlags[slot] & 0x80) {
					continue;
				}

				const auto ent = reinterpret_cast<const game::CEntity*>(pool->m_pObjects + slot * pool->m_nEntrySize);

				// cheap reject via the map cells the entity is linked into, before touching its model info
				{
					const bool lod = ent->m_dwFlags & 0x40;
					const float cell_size = lod ? 200.0f : 50.0f;
					const float cam_x = cam_org.x / cell_size + (lod ? 15.0f : 60.0f);
					const float cam_y = cam_org.y / cell_size + (lod ? 15.0f : 60.0f);

					const auto rect = ent->m_dwCellRect;
					const float min_x = static_cast<float>(static_cast<int>(rect & 0x7FF) - 1024);
					const float min_y = static_cast<float>(static_cast<int>((rect >> 11) & 0x7FF) - 1024);
					const float max_x = min_x + static_cast<float>((rect >> 22) & 0x1F) + 1.0f;
					const float max_y = min_y + static_cast<float>(rect >> 27) + 1.0f;

					const float dx = std::max({ min_x - cam_x, 0.0f, cam_x - max_x }) * cell_size;
					const float dy = std::max({ min_y - cam_y, 0.0f, cam_y - max_y }) * cell_size;
					
					if (dx * dx + dy * dy > ins.radius * ins.radius) {
						continue;
					}
				}

				const auto model = ent->m_wModelIndex >= 0 ? game::g_modelPointers[ent->m_wModelIndex] : nullptr;
				if (!model) {
					continue;
				}

				const float height = model->m_vAabbMax.z - model->m_vAabbMin.z;
				if (model->m_fRadius < ins.min_radius || height < ins.min_height) {
					continue;
				}

				const auto pos = entity_position(ent);
				const float reach = ins.radius + model->m_fRadius + model->m_vCenter.Length();
				
				if (cam_org.DistToSqr(pos) > reach * reach) {
					continue;
				}

				instance_s inst = {};
				if (ent->worldTransform)
				{
					inst.world = *ent->worldTransform;
					inst.world.m[0][3] = 0.0f;
					inst.world.m[1][3] = 0.0f;
					inst.world.m[2][3] = 0.0f;
					inst.world.m[3][3] = 1.0f;
				}
				else
				{
					D3DXMatrixRotationZ(&inst.world, ent->m_fHeading);
					inst.world.m[3][0] = pos.x;
					inst.world.m[3][1] = pos.y;
					inst.world.m[3][2] = pos.z;
				}

				const auto local_center = ((model->m_vAabbMin + model->m_vAabbMax) * 0.5f).ToD3DXVector();
				
				D3DXVECTOR3 center;
				D3DXVec3TransformCoord(&center, &local_center, &inst.world);

				inst.center = { center.x, center.y, center.z };
				inst.distance = cam_org.DistTo(inst.center);
				
				if (inst.distance > ins.radius + model->m_fRadius) {
					continue;
				}

				inst.entity = ent;
				inst.lod_entity = ent->m_pLod;
				inst.local_mins = model->m_vAabbMin;
				inst.local_maxs = model->m_vAabbMax;
				inst.radius = model->m_fRadius;
				inst.height = height;
				inst.lod_distance = ent->m_fLodDistance;
				inst.model_index = ent->m_wModelIndex;
				inst.lod_model_index = ent->m_pLod ? ent->m_pLod->m_wModelIndex : -1;
				inst.lod_children = ent->m_nLodChildren;
				inst.alpha = ent->m_alpha;
				inst.is_lod = (ent->m_dwFlags & 0x40) != 0;
				inst.model_loaded = ent->m_pDrawableReference != nullptr;
				inst.visited = stamp && ent->m_dwVisibilityStamp == stamp;
				inst.visible = inst.visited && (ent->m_dwPhaseVisible & ~ent->m_dwPhaseCulled & primary_bit) != 0u;

				// the game measures an HD entity from its lod parent
				if (!inst.is_lod && ent->m_fLodDistance > 0.0f) {
					inst.past_lod_distance = cam_org.DistTo(ent->m_pLod ? entity_position(ent->m_pLod) : pos) > ent->m_fLodDistance * lod_scale;
				}

				if (const auto it = game::g_modelHashToName.find(static_cast<std::uint32_t>(model->m_dwModel)); it != game::g_modelHashToName.end()) {
					inst.model_name = it->second;
				}

				snapshot.emplace_back(inst);
			}

			if (snapshot.size() > MAX_INSTANCES)
			{
				std::nth_element(snapshot.begin(), snapshot.begin() + MAX_INSTANCES, snapshot.end(), [](const instance_s& a, const instance_s& b) {
					return a.distance < b.distance;
				});

				snapshot.resize(MAX_INSTANCES);
			}

			std::lock_guard lock(ins.mutex);
			ins.published.swap(snapshot);
			++ins.published_generation;
		}
	}


	DETOUR_TYPEDEF(static_world_culling_check, BOOL, __thiscall, void* this_ptr, unkown_struct_culling* unk);

	// anti culling rules for static world objects, falls back to the stock frustum test
	std::pair<BOOL, anticull_inspector::frustum_reason> static_world_culling_verdict(game::CEntity* this_ptr, unkown_struct_culling* unk)
	{
		using reason = anticull_inspector::frustum_reason;

		if (anticull_inspector::hides(this_ptr)) {
			return { FALSE, reason::inspector };
		}

		// this = AVCBuilding : AVCEntity : AUCVirtualBase
		const auto im = imgui::get();
		const auto gs = comp_settings::get();

		if (im->m_dbg_never_cull_statics) {
			return { TRUE, reason::never_cull };
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
			return { TRUE, reason::near_dist };
		}

		const float object_radius = shared::utils::hook::call_virtual<22, float>(this_ptr);
		const auto object_mins = shared::utils::hook::call_virtual<24, Vector*>(this_ptr);
		const auto object_maxs = shared::utils::hook::call_virtual<25, Vector*>(this_ptr);

		float object_height = 0.0f;
		if (object_mins && object_maxs) {
			object_height = object_maxs->z - object_mins->z;
		}

		// do not cull if obj is within medium dist and larger then medium radius setting
		if (nc_dist_med > 0.0f && dist_sqr < nc_dist_med * nc_dist_med)
		{
			if (object_radius > nc_radius_med) {
				return { TRUE, reason::medium_radius };
			}
		}

		// do not cull if obj is within far dist and larger then far radius setting
		if (nc_dist_far > 0.0f && dist_sqr < nc_dist_far * nc_dist_far)
		{
			if (object_radius > nc_radius_far) {
				return { TRUE, reason::far_radius };
			}

			if (object_height > 0.0f && object_height > nc_height_far) {
				return { TRUE, reason::far_height };
			}
		}

		if (const auto& ms = map_settings::get_map_settings(); !ms.anticull_meshes.empty())
		{
			for (const auto& cat : ms.anticull_meshes)
			{
				// dist 0 = always active
				if (!cat.distance || (int)dist_sqr < cat.distance * cat.distance)
				{
					if (cat.indices.contains(this_ptr->m_wModelIndex)) {
						return { TRUE, reason::anticull_mesh };
					}
				}
			}
		}

		return { static_world_culling_check_og(this_ptr, unk), reason::stock };
	}

	BOOL __fastcall static_world_culling_check_hk(game::CEntity* this_ptr, [[maybe_unused]] void* fastcall, unkown_struct_culling* unk)
	{
		PROFILE_ZONE_N("static world culling check");
		PROFILE_ZONE_VALUE(this_ptr->m_wModelIndex);

		const auto [visible, reason] = static_world_culling_verdict(this_ptr, unk);
		anticull_inspector::trace_frustum(this_ptr, unk, visible, reason);
		return visible;
	}


	// extended anti culling for objects defined via map_settings
	int extended_anticull_verdict(game::CEntity* ent)
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

	int extended_anticull_hk(game::CEntity* ent)
	{
		PROFILE_ZONE_N("extended anti culling check");
		const int forced = extended_anticull_verdict(ent);
		anticull_inspector::trace_occlusion(ent, forced == TRUE);
		return forced;
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
	// anti culling meshes (mapsettings) outside of the camera frustum
	// the scene traversal only walks map cells a render phase marked as visible, so meshes behind the camera never reach
	// static_world_culling_check_hk or extended_anticull_hk. Their cells get marked here so the game processes them.

	namespace anticull_offscreen
	{
		struct tracked_instance_s
		{
			std::int32_t slot;
			std::uint8_t flag;
		};

		constexpr std::int32_t POOL_SLOTS_PER_SCAN = 4096;

		/// max squared distance per model index, negative = not an anti culling mesh
		std::vector<float> model_dist_sqr(0x10000, -1.0f);
		std::vector<std::uint16_t> model_dist_sqr_used;

		std::vector<tracked_instance_s> tracked;
		std::vector<tracked_instance_s> scan_results;
		std::int32_t scan_cursor = 0;
		int marked_near = 0, marked_lod = 0;

		void rebuild_model_lut()
		{
			for (const auto index : model_dist_sqr_used) {
				model_dist_sqr[index] = -1.0f;
			}

			model_dist_sqr_used.clear();

			for (const auto& cat : map_settings::get_map_settings().anticull_meshes)
			{
				// dist 0 = always active
				const float dist_sqr = cat.distance > 0 ? static_cast<float>(cat.distance) * static_cast<float>(cat.distance) : FLT_MAX;

				for (const auto index : cat.indices)
				{
					const auto lut_index = static_cast<std::uint16_t>(index);

					if (model_dist_sqr[lut_index] < 0.0f) {
						model_dist_sqr_used.push_back(lut_index);
					}

					model_dist_sqr[lut_index] = std::max(model_dist_sqr[lut_index], dist_sqr);
				}
			}
		}

		// walks a slice of the building pool per call and swaps in the found instances once a full pass is done.
		// The model lut is rebuilt on every pass so mapsetting edits apply without explicit invalidation.
		void scan_building_pool(const game::CPool* pool)
		{
			if (!scan_cursor)
			{
				rebuild_model_lut();
				scan_results.clear();
			}

			const auto end = std::min(scan_cursor + POOL_SLOTS_PER_SCAN, pool->m_nSize);
			for (auto slot = scan_cursor; slot < end; ++slot)
			{
				const auto flag = pool->m_pFlags[slot];
				if (flag & 0x80) {
					continue;
				}

				const auto ent = reinterpret_cast<const game::CEntity*>(pool->m_pObjects + slot * pool->m_nEntrySize);
				if (model_dist_sqr[static_cast<std::uint16_t>(ent->m_wModelIndex)] >= 0.0f) {
					scan_results.push_back({ slot, flag });
				}
			}

			scan_cursor = end;
			if (scan_cursor >= pool->m_nSize)
			{
				tracked.swap(scan_results);
				scan_cursor = 0;
			}
		}

		// marks one map cell per tracked instance within category distance for the current render phase.
		// The game links an entity into every cell its bounds touch, so the cell closest to the camera is enough.
		void mark_cells(const int render_context, const bool lod_grid)
		{
			PROFILE_ZONE();
			const auto cs = comp_settings::get();
			const auto im = imgui::get();
			const auto pool = game::ppBuildingPool ? *game::ppBuildingPool : nullptr;
			const auto phase = game::get_current_renderphase();
			const auto grid = game::pRenderCellGridNear;

			if (!cs->nocull_extended._bool() || !cs->nocull_extended_offscreen._bool() || map_settings::get_map_settings().anticull_meshes.empty()
				|| !pool || !pool->m_pObjects || !pool->m_pFlags || !phase || !grid)
			{
				tracked.clear();
				scan_cursor = 0;
				marked_near = marked_lod = 0;

				im->m_dbg_anticull_offscreen_tracked = im->m_dbg_anticull_offscreen_marked = 0;
				return;
			}

			if (!lod_grid) {
				scan_building_pool(pool);
			}

			const Vector cam_org = &phase->viewport_0xB0.cameraInv.m[3][0];
			const float cell_scale = lod_grid ? 0.005f : 0.02f;
			const float cell_bias = lod_grid ? 15.0f : 60.0f;
			const int cam_cell_x = static_cast<int>(floorf(cam_org.x * cell_scale + cell_bias));
			const int cam_cell_y = static_cast<int>(floorf(cam_org.y * cell_scale + cell_bias));

			const auto mark_cell = reinterpret_cast<void(__cdecl*)(int, int, int, int)>(
				lod_grid ? game::fn_addr__mark_render_sector_far_callback : game::fn_addr__mark_render_sector_near_callback);

			int marked = 0;
			for (const auto& t : tracked)
			{
				if (t.slot >= pool->m_nSize || pool->m_pFlags[t.slot] != t.flag) {
					continue;
				}

				const auto ent = reinterpret_cast<game::CEntity*>(pool->m_pObjects + t.slot * pool->m_nEntrySize);
				if (((ent->m_dwFlags & 0x40) != 0) != lod_grid) {
					continue;
				}

				const float max_dist_sqr = model_dist_sqr[static_cast<std::uint16_t>(ent->m_wModelIndex)];
				if (max_dist_sqr < 0.0f) {
					continue;
				}

				// same origin as the anti culling checks in static_world_culling_check_hk and extended_anticull_hk
				Vector4D object_origin = {};
				shared::utils::hook::call_virtual<20, void, Vector4D*>(ent, &object_origin);

				if (fabs(cam_org.DistToSqr(object_origin)) > max_dist_sqr) {
					continue;
				}

				const auto rect = ent->m_dwCellRect;
				const int min_x = static_cast<int>(rect & 0x7FF) - 1024;
				const int min_y = static_cast<int>((rect >> 11) & 0x7FF) - 1024;
				const int x = std::clamp(cam_cell_x, min_x, min_x + static_cast<int>((rect >> 22) & 0x1F));
				const int y = std::clamp(cam_cell_y, min_y, min_y + static_cast<int>(rect >> 27));

				// the near grid clamps cells outside of its window onto the border, which would mark an unrelated cell
				if (!lod_grid && (static_cast<unsigned>(x - grid->origin_x) >= 48u || static_cast<unsigned>(y - grid->origin_y) >= 48u)) {
					continue;
				}

				mark_cell(x, y, render_context, reinterpret_cast<int>(phase));
				++marked;
			}

			(lod_grid ? marked_lod : marked_near) = marked;
			im->m_dbg_anticull_offscreen_tracked = static_cast<int>(tracked.size());
			im->m_dbg_anticull_offscreen_marked = marked_near + marked_lod;
		}
	}


	// ---
	// area / sector anti-culling adding the lowest LOD outside the camera frustum

	void add_far_grid_map_sections(const int render_context)
	{
		PROFILE_ZONE();
		anticull_offscreen::mark_cells(render_context, true);

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
		PROFILE_ZONE();
		anticull_inspector::capture_primary_phase(game::get_current_renderphase());
		anticull_offscreen::mark_cells(render_context, false);

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

	// the record can be the uninitialized fill pattern or any garbage value (seen as a float bit pattern in TLAD "Shifting Weight")
	bool is_invalid_model_record(const void* record)
	{
		return record == reinterpret_cast<const void*>(0xCDCDCDCD) || shared::utils::memory::is_bad_read_ptr(record);
	}

	__declspec(naked) void veh_nullptr_invalid_model_fix_stub()
	{
		__asm
		{
			pushad;
			sub		esp, 0x80;
			movups	[esp + 0x00], xmm0;
			movups	[esp + 0x10], xmm1;
			movups	[esp + 0x20], xmm2;
			movups	[esp + 0x30], xmm3;
			movups	[esp + 0x40], xmm4;
			movups	[esp + 0x50], xmm5;
			movups	[esp + 0x60], xmm6;
			movups	[esp + 0x70], xmm7;

			push	ecx;
			call	is_invalid_model_record;
			add		esp, 4;
			movzx	eax, al;
			mov		[esp + 0x80 + 0x1C], eax; // pushad eax slot

			movups	xmm0, [esp + 0x00];
			movups	xmm1, [esp + 0x10];
			movups	xmm2, [esp + 0x20];
			movups	xmm3, [esp + 0x30];
			movups	xmm4, [esp + 0x40];
			movups	xmm5, [esp + 0x50];
			movups	xmm6, [esp + 0x60];
			movups	xmm7, [esp + 0x70];
			add		esp, 0x80;
			popad;

			test	eax, eax;
			jnz		INVALID_MODEL;

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
		shared::common::remix_api::initialize(nullptr, nullptr, nullptr);

		shared::common::loader::module_loader::register_module(std::make_unique<imgui>());
		shared::common::loader::module_loader::register_module(std::make_unique<performance_logger>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer_ff>());
		shared::common::loader::module_loader::register_module(std::make_unique<renderer_api>());
		shared::common::loader::module_loader::register_module(std::make_unique<water>());
		shared::common::loader::module_loader::register_module(std::make_unique<distant_lights>());
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
