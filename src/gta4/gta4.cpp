#include "std_include.hpp"

#include "modules/dinput_hook.hpp"
#include "modules/game_settings.hpp"
#include "modules/imgui.hpp"
#include "modules/remix_lights.hpp"
#include "modules/renderer.hpp"
#include "shared/common/flags.hpp"
#include "shared/common/remix_api.hpp"

namespace mods::gta4
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

		//remix_lights::on_client_frame();

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

		//remix_lights::get()->draw_all_active_lights();

		//D3DXMATRIX view_matrix, proj_matrix;
		//if (const auto im = imgui::get(); im && im->m_dbg_use_fake_camera)
		//{
		//	// Construct view matrix
		//	D3DXMATRIX rotation, translation;
		//	D3DXMatrixRotationYawPitchRoll(&rotation,
		//		D3DXToRadian(im->m_dbg_camera_yaw),		// Yaw in radians
		//		D3DXToRadian(im->m_dbg_camera_pitch),	// Pitch in radians
		//		0.0f);									// No roll for simplicity

		//	D3DXMatrixTranslation(&translation,
		//		-im->m_dbg_camera_pos[0],				// Negate for camera (moves world opposite)
		//		-im->m_dbg_camera_pos[1],
		//		-im->m_dbg_camera_pos[2]);

		//	D3DXMatrixMultiply(&view_matrix, &rotation, &translation);

		//	// Construct projection matrix
		//	D3DXMatrixPerspectiveFovLH(&proj_matrix,
		//		D3DXToRadian(im->m_dbg_camera_fov), // FOV in radians
		//		im->m_dbg_camera_aspect,
		//		im->m_dbg_camera_near_plane,
		//		im->m_dbg_camera_far_plane);

		//	shared::globals::d3d_device->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);
		//	shared::globals::d3d_device->SetTransform(D3DTS_VIEW, &view_matrix);
		//	shared::globals::d3d_device->SetTransform(D3DTS_PROJECTION, &proj_matrix);
		//}

		//if (im->m_dbg_use_game_camera)
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
		if (dist_sqr < nc_dist_med * nc_dist_med)
		{
			if (object_radius > nc_radius_med) {
				return TRUE;
			}
		}

		// do not cull if obj is within far dist and larger then far radius setting
		if (dist_sqr < nc_dist_far * nc_dist_far)
		{
			if (object_radius > nc_radius_far) {
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
