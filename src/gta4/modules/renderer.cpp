#include "std_include.hpp"
#include "renderer.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"
#include "remix_markers.hpp"

namespace gta4
{
	int g_is_instance_rendering = 0;
	int g_is_sky_rendering = 0;
	int g_is_water_rendering = 0;
	int g_is_rendering_mirror = 0;

	namespace tex_addons
	{
		bool initialized = false;
		LPDIRECT3DTEXTURE9 sky = nullptr;
		LPDIRECT3DTEXTURE9 white01 = nullptr;
		LPDIRECT3DTEXTURE9 white02 = nullptr;
		LPDIRECT3DTEXTURE9 decal_dirt = nullptr;
		LPDIRECT3DTEXTURE9 veh_light_ems_glass = nullptr;
		LPDIRECT3DTEXTURE9 berry = nullptr;
		LPDIRECT3DTEXTURE9 mirror = nullptr;

		void init_texture_addons(bool release)
		{
			if (release)
			{
				if (tex_addons::sky) tex_addons::sky->Release();
				if (tex_addons::white01) tex_addons::white01->Release();
				if (tex_addons::white02) tex_addons::white02->Release();
				if (tex_addons::veh_light_ems_glass) tex_addons::veh_light_ems_glass->Release();
				if (tex_addons::berry) tex_addons::berry->Release();
				if (tex_addons::mirror) tex_addons::mirror->Release();
				return;
			}

			const auto dev = shared::globals::d3d_device;
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\sky.png", &tex_addons::sky);
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\white01.png", &tex_addons::white01); // marker meshes
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\white02.png", &tex_addons::white02);
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\veh_light_ems_glass.png", &tex_addons::veh_light_ems_glass);
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\berry.png", &tex_addons::berry);
			D3DXCreateTextureFromFileA(dev, "rtx_comp\\textures\\mirror.png", &tex_addons::mirror);
			tex_addons::initialized = true;
		}
	}

	// ----


	// uses unused Renderstate 149 to set per drawcall modifiers
	// ~ currently req. runtime changes
	void renderer::set_remix_modifier(IDirect3DDevice9* dev, RemixModifier mod)
	{
		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)149);
		dc_ctx.modifiers.remix_modifier |= mod;

		dev->SetRenderState((D3DRENDERSTATETYPE)149, static_cast<DWORD>(dc_ctx.modifiers.remix_modifier));
	}

	// uses unused Renderstate 149 & 169 to tweak the emissive intensity of remix materials (legacy/opaque)
	// ~ currently req. runtime changes --> remixTempFloat01FromD3D
	/// @param no_overrides	will not override any previously set intensity if true
	void renderer::set_remix_emissive_intensity(IDirect3DDevice9* dev, float intensity, bool no_overrides)
	{
		const bool result = dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)169);
		if (!result && no_overrides) {
			return;
		}

		set_remix_modifier(dev, RemixModifier::EmissiveScalar);
		dev->SetRenderState((D3DRENDERSTATETYPE)169, *reinterpret_cast<DWORD*>(&intensity));
	}

	// uses unused Renderstate 149 & 177 to tweak the roughness of remix materials (legacy/opaque)
	// ~ currently req. runtime changes --> remixTempFloat02FromD3D
	void renderer::set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar)
	{
		set_remix_modifier(dev, RemixModifier::RoughnessScalar);

		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)177);
		dev->SetRenderState((D3DRENDERSTATETYPE)177, *reinterpret_cast<DWORD*>(&roughness_scalar));
	}


	// uses unused Renderstate 169 to pass per drawcall data
	// ~ currently req. runtime changes --> remixTempFloat02FromD3D
	void renderer::set_remix_temp_float01(IDirect3DDevice9* dev, float value)
	{
		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)169);
		dev->SetRenderState((D3DRENDERSTATETYPE)169, *reinterpret_cast<DWORD*>(&value));
	}

	// uses unused Renderstate 177 to pass per drawcall data
	// ~ currently req. runtime changes --> remixTempFloat02FromD3D
	void renderer::set_remix_temp_float02(IDirect3DDevice9* dev, float value)
	{
		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)177);
		dev->SetRenderState((D3DRENDERSTATETYPE)177, *reinterpret_cast<DWORD*>(&value));
	}


	// uses unused Renderstate 42 to set remix texture categories
	// ~ currently req. runtime changes
	void renderer::set_remix_texture_categories(IDirect3DDevice9* dev, const InstanceCategories& cat)
	{
		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)42);
		dc_ctx.modifiers.remix_instance_categories |= cat;
		dev->SetRenderState((D3DRENDERSTATETYPE)42, static_cast<DWORD>(dc_ctx.modifiers.remix_instance_categories));
	}

	// uses unused Renderstate 150 to set custom remix hash
	// ~ currently req. runtime changes
	void renderer::set_remix_texture_hash(IDirect3DDevice9* dev, const std::uint32_t& hash)
	{
		dc_ctx.save_rs(dev, (D3DRENDERSTATETYPE)150);
		dev->SetRenderState((D3DRENDERSTATETYPE)150, hash);
	}

	// ---

	void on_constant_matDiffuseColor(IDirect3DDevice9* dev, const Vector& color)
	{
		auto& ctx = renderer::get()->dc_ctx;

		ctx.save_rs(dev, D3DRS_TEXTUREFACTOR); // prob. not needed
		ctx.save_tss(dev, D3DTSS_COLOROP);
		ctx.save_tss(dev, D3DTSS_COLORARG1);
		ctx.save_tss(dev, D3DTSS_COLORARG2);

		ctx.modifiers.is_vehicle_paint = true;
		
		dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(color.x, color.y, color.z, 1.0f));
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	}

	void on_constant_diffuseCol(IDirect3DDevice9* dev, const Vector4D& color)
	{
		auto& ctx = renderer::get()->dc_ctx;

		ctx.save_rs(dev, D3DRS_TEXTUREFACTOR); // prob. not needed
		ctx.save_tss(dev, D3DTSS_COLOROP);
		ctx.save_tss(dev, D3DTSS_COLORARG1);
		ctx.save_tss(dev, D3DTSS_COLORARG2);

		ctx.save_tss(dev, D3DTSS_ALPHAOP);
		ctx.save_tss(dev, D3DTSS_ALPHAARG1);
		ctx.save_tss(dev, D3DTSS_ALPHAARG2);

		dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(color.x, color.y, color.z, color.w * 1.0f));
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		ctx.save_rs(shared::globals::d3d_device, D3DRS_ALPHABLENDENABLE);
		ctx.save_rs(shared::globals::d3d_device, D3DRS_SRCBLEND);
		ctx.save_rs(shared::globals::d3d_device, D3DRS_DESTBLEND);

		shared::globals::d3d_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		shared::globals::d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		shared::globals::d3d_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	}

	void on_constant_switchOn([[maybe_unused]] IDirect3DDevice9* dev, const bool state)
	{
		auto& ctx = renderer::get()->dc_ctx;

		ctx.modifiers.is_vehicle_using_switch_on_state = true;
		ctx.modifiers.is_vehicle_on = state;

		/*if (state) 
		{
			ctx.save_rs(shared::globals::d3d_device, D3DRS_SRCBLEND);
			shared::globals::d3d_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		}*/
	}

	void on_constant_emissiveMultiplier(IDirect3DDevice9* dev, const float& intensity)
	{
		auto& ctx = renderer::get()->dc_ctx;

		//ctx.save_rs(dev, D3DRS_TEXTUREFACTOR); // prob. not needed
		ctx.save_tss(dev, D3DTSS_COLOROP);
		ctx.save_tss(dev, D3DTSS_COLORARG1);
		ctx.save_tss(dev, D3DTSS_COLORARG2);

		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

		ctx.save_rs(dev, D3DRS_SRCBLEND);
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);

		renderer::set_remix_emissive_intensity(dev, intensity);
	}

	// ----

	bool debug_ignore_shader_logic(const std::string_view& shader)
	{
		const auto& im = imgui::get();
		if (im->m_dbg_enable_ignore_shader_logic)
		{
			if (im->m_dbg_ignore_all) return true;
			if (im->m_dbg_ignore_cascade && shader.ends_with("cascade.fxc")) return true;
			if (im->m_dbg_ignore_deferred_lighting && shader.ends_with("deferred_lighting.fxc")) return true;
			if (im->m_dbg_ignore_gpuptfx_simplerender && shader.ends_with("gpuptfx_simplerender.fxc")) return true;
			if (im->m_dbg_ignore_gta_atmoscatt_clouds && shader.ends_with("gta_atmoscatt_clouds.fxc")) return true;
			if (im->m_dbg_ignore_gta_cubemap_reflect && shader.ends_with("gta_cubemap_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_cutout_fence && shader.ends_with("gta_cutout_fence.fxc")) return true;
			if (im->m_dbg_ignore_gta_decal && shader.ends_with("gta_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_decal_amb_only && shader.ends_with("gta_decal_amb_only.fxc")) return true;
			if (im->m_dbg_ignore_gta_decal_dirt && shader.ends_with("gta_decal_dirt.fxc")) return true;
			if (im->m_dbg_ignore_gta_decal_glue && shader.ends_with("gta_decal_glue.fxc")) return true;
			if (im->m_dbg_ignore_gta_decal_normal_only && shader.ends_with("gta_decal_normal_only.fxc")) return true;
			if (im->m_dbg_ignore_gta_default && shader.ends_with("gta_default.fxc")) return true;
			if (im->m_dbg_ignore_gta_diffuse_instance && shader.ends_with("gta_diffuse_instance.fxc")) return true;
			if (im->m_dbg_ignore_gta_emissive && shader.ends_with("gta_emissive.fxc")) return true;
			if (im->m_dbg_ignore_gta_emissivenight && shader.ends_with("gta_emissivenight.fxc")) return true;
			if (im->m_dbg_ignore_gta_emissivestrong && shader.ends_with("gta_emissivestrong.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass && shader.ends_with("gta_glass.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass_emissive && shader.ends_with("gta_glass_emissive.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass_emissivenight && shader.ends_with("gta_glass_emissivenight.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass_normal_spec_reflect && shader.ends_with("gta_glass_normal_spec_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass_reflect && shader.ends_with("gta_glass_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_glass_spec && shader.ends_with("gta_glass_spec.fxc")) return true;
			if (im->m_dbg_ignore_gta_grass && shader.ends_with("gta_grass.fxc")) return true;
			if (im->m_dbg_ignore_gta_hair_sorted_alpha && shader.ends_with("gta_hair_sorted_alpha.fxc")) return true;
			if (im->m_dbg_ignore_gta_hair_sorted_alpha_exp && shader.ends_with("gta_hair_sorted_alpha_exp.fxc")) return true;
			if (im->m_dbg_ignore_gta_im && shader.ends_with("gta_im.fxc") && !shared::globals::imgui_is_rendering) return true;
			if (im->m_dbg_ignore_gta_normal && shader.ends_with("gta_normal.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_cubemap_reflect && shader.ends_with("gta_normal_cubemap_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_decal && shader.ends_with("gta_normal_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_reflect && shader.ends_with("gta_normal_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_reflect_alpha && shader.ends_with("gta_normal_reflect_alpha.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_reflect_decal && shader.ends_with("gta_normal_reflect_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec && shader.ends_with("gta_normal_spec.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_cubemap_reflect && shader.ends_with("gta_normal_spec_cubemap_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_decal && shader.ends_with("gta_normal_spec_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_reflect && shader.ends_with("gta_normal_spec_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_reflect_decal && shader.ends_with("gta_normal_spec_reflect_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_reflect_emissive && shader.ends_with("gta_normal_spec_reflect_emissive.fxc")) return true;
			if (im->m_dbg_ignore_gta_normal_spec_reflect_emissivenight && shader.ends_with("gta_normal_spec_reflect_emissivenight.fxc")) return true;
			if (im->m_dbg_ignore_gta_parallax && shader.ends_with("gta_parallax.fxc")) return true;
			if (im->m_dbg_ignore_gta_parallax_specmap && shader.ends_with("gta_parallax_specmap.fxc")) return true;
			if (im->m_dbg_ignore_gta_parallax_steep && shader.ends_with("gta_parallax_steep.fxc")) return true;
			if (im->m_dbg_ignore_gta_ped && shader.ends_with("gta_ped.fxc")) return true;
			if (im->m_dbg_ignore_gta_ped_face && shader.ends_with("gta_ped_face.fxc")) return true;
			if (im->m_dbg_ignore_gta_ped_reflect && shader.ends_with("gta_ped_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_ped_skin && shader.ends_with("gta_ped_skin.fxc")) return true;
			if (im->m_dbg_ignore_gta_ped_skin_blendshape && shader.ends_with("gta_ped_skin_blendshape.fxc")) return true;
			if (im->m_dbg_ignore_gta_projtex && shader.ends_with("gta_projtex.fxc")) return true;
			if (im->m_dbg_ignore_gta_projtex_steep && shader.ends_with("gta_projtex_steep.fxc")) return true;
			if (im->m_dbg_ignore_gta_radar && shader.ends_with("gta_radar.fxc")) return true;
			if (im->m_dbg_ignore_gta_reflect && shader.ends_with("gta_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_reflect_decal && shader.ends_with("gta_reflect_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_rmptfx_gpurender && shader.ends_with("gta_rmptfx_gpurender.fxc")) return true;
			if (im->m_dbg_ignore_gta_rmptfx_litsprite && shader.ends_with("gta_rmptfx_litsprite.fxc")) return true;
			if (im->m_dbg_ignore_gta_rmptfx_mesh && shader.ends_with("gta_rmptfx_mesh.fxc")) return true;
			if (im->m_dbg_ignore_gta_rmptfx_raindrops && shader.ends_with("gta_rmptfx_raindrops.fxc")) return true;
			if (im->m_dbg_ignore_gta_spec && shader.ends_with("gta_spec.fxc")) return true;
			if (im->m_dbg_ignore_gta_spec_decal && shader.ends_with("gta_spec_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_spec_reflect && shader.ends_with("gta_spec_reflect.fxc")) return true;
			if (im->m_dbg_ignore_gta_spec_reflect_decal && shader.ends_with("gta_spec_reflect_decal.fxc")) return true;
			if (im->m_dbg_ignore_gta_terrain && shader.ends_with("gta_terrain.fxc")) return true;
			if (im->m_dbg_ignore_gta_trees && shader.ends_with("gta_trees.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_badges && shader.ends_with("gta_vehicle_badges.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_basic && shader.ends_with("gta_vehicle_basic.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_chrome && shader.ends_with("gta_vehicle_chrome.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_disc && shader.ends_with("gta_vehicle_disc.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_generic && shader.ends_with("gta_vehicle_generic.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_interior && shader.ends_with("gta_vehicle_interior.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_interior2 && shader.ends_with("gta_vehicle_interior2.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_lightsemissive && shader.ends_with("gta_vehicle_lightsemissive.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_mesh && shader.ends_with("gta_vehicle_mesh.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_paint1 && shader.ends_with("gta_vehicle_paint1.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_paint2 && shader.ends_with("gta_vehicle_paint2.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_paint3 && shader.ends_with("gta_vehicle_paint3.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_rims1 && shader.ends_with("gta_vehicle_rims1.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_rims2 && shader.ends_with("gta_vehicle_rims2.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_rims3 && shader.ends_with("gta_vehicle_rims3.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_rubber && shader.ends_with("gta_vehicle_rubber.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_shuts && shader.ends_with("gta_vehicle_shuts.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_tire && shader.ends_with("gta_vehicle_tire.fxc")) return true;
			if (im->m_dbg_ignore_gta_vehicle_vehglass && shader.ends_with("gta_vehicle_vehglass.fxc")) return true;
			if (im->m_dbg_ignore_gta_wire && shader.ends_with("gta_wire.fxc")) return true;
			if (im->m_dbg_ignore_mirror && shader.ends_with("mirror.fxc")) return true;
			if (im->m_dbg_ignore_rage_atmoscatt_clouds && shader.ends_with("rage_atmoscatt_clouds.fxc")) return true;
			if (im->m_dbg_ignore_rage_billboard_nobump && shader.ends_with("rage_billboard_nobump.fxc")) return true;
			if (im->m_dbg_ignore_rage_bink && shader.ends_with("rage_bink.fxc")) return true;
			if (im->m_dbg_ignore_rage_default && shader.ends_with("rage_default.fxc")) return true;
			if (im->m_dbg_ignore_rage_fastmipmap && shader.ends_with("rage_fastmipmap.fxc")) return true;
			if (im->m_dbg_ignore_rage_im && shader.ends_with("rage_im.fxc")) return true;
			if (im->m_dbg_ignore_rage_perlinnoise && shader.ends_with("rage_perlinnoise.fxc")) return true;
			if (im->m_dbg_ignore_rage_postfx && shader.ends_with("rage_postfx.fxc")) return true;
			if (im->m_dbg_ignore_rmptfx_collision && shader.ends_with("rmptfx_collision.fxc")) return true;
			if (im->m_dbg_ignore_rmptfx_default && shader.ends_with("rmptfx_default.fxc")) return true;
			if (im->m_dbg_ignore_rmptfx_litsprite && shader.ends_with("rmptfx_litsprite.fxc")) return true;
			if (im->m_dbg_ignore_shadowSmartBlit && shader.ends_with("shadowSmartBlit.fxc")) return true;
			if (im->m_dbg_ignore_shadowZ && shader.ends_with("shadowZ.fxc")) return true;
			if (im->m_dbg_ignore_shadowZDir && shader.ends_with("shadowZDir.fxc")) return true;
			if (im->m_dbg_ignore_water && shader.ends_with("water.fxc")) return true;
			if (im->m_dbg_ignore_waterTex && shader.ends_with("waterTex.fxc")) return true;
		}

		return false;
	}


	enum class EmissiveShaderType
	{
		None,
		Emissive,
		EmissiveStrong,
		EmissiveNight
	};

	EmissiveShaderType get_emissive_shader_type(const std::string_view& shader_name)
	{
		if (shader_name.ends_with("emissivenight.fxc")) {
			return EmissiveShaderType::EmissiveNight;
		}
		if (shader_name.ends_with("emissive.fxc")) {
			return EmissiveShaderType::Emissive;
		}
		if (shader_name.ends_with("strong.fxc")) {
			return EmissiveShaderType::EmissiveStrong;
		}
		return EmissiveShaderType::None;
	}

	// every mesh goes through here (besides in-game ui and particles)
	void SetupVertexShaderAndConstants(game::vs_info_s* info, game::vs_data_s* data, game::shader_info_sub_s* constant_data_struct, game::shader_data_sub_s* sampler_data)
	{
		static auto gs = game_settings::get();
		const auto game_device = game::get_d3d_device();
		auto& ctx = renderer::get()->dc_ctx;

		/*auto y = &game::globalShaderParameters[20];
		if (y && y->pValue) {
			int asd = 0;
		}*/

		if (*game::g_currentVertexShader != data->shader) 
		{
			*game::g_currentVertexShader = data->shader;
			game_device->SetVertexShader(data->shader);
		}

		if (ctx.info.shader_name.empty()) {
			ctx.info.shader_name = constant_data_struct->data->sub.shader_name;
		}

		// imgui dev - ignore shaders
		ctx.modifiers.do_not_render = debug_ignore_shader_logic(ctx.info.shader_name);

#if DEBUG
		if (ctx.modifiers.do_not_render) 
		{
			int break_me = 1; // can be used to break on ignored shaders

			if (!g_is_rendering_static)
			{
				int break_me2 = 1;
			}
		}
#endif

		if (ctx.info.shader_name.ends_with("deferred_lighting.fxc")) {
			ctx.modifiers.do_not_render = true;
		}

		if (info->num_vs_constants > 0)
		{
			bool is_gta_atmoscatt_clouds_shader = false;
			bool is_lightsemissive_shader = false;
			bool is_gta_rmptfx_litsprite_shader = false;
			bool modified_world_matrix = false;

			if (g_is_sky_rendering)
			{
				//if (ctx.info.shader_name.ends_with("_clouds.fxc")) {
					is_gta_atmoscatt_clouds_shader = true;
				//}
			}
			else if (ctx.info.shader_name.ends_with("lightsemissive.fxc")) {
				is_lightsemissive_shader = true;
			}
			else if (ctx.info.shader_name.ends_with("gta_rmptfx_litsprite.fxc")) {
				is_gta_rmptfx_litsprite_shader = true;
			}

			int i = 0;
			do
			{
				std::uint32_t dataPoolIndex = info->vs_constant_stack[i].constant_pool_index;
				game::const_stack_s* vs_const = &info->vs_constant_stack[i];
				std::uint8_t float_count = constant_data_struct->constant_float_count_array[dataPoolIndex];

				if (float_count)
				{
					std::uint32_t register_pool_index = vs_const->register_pool_index;
					std::uint32_t type = data->register_pool[register_pool_index].type;
					std::uint32_t register_num = data->register_pool[register_pool_index].register_num;

					if (type == 1) {
						game_device->SetVertexShaderConstantI(register_num, constant_data_struct->constants[dataPoolIndex].int_ptr, 1u);
					}
					else if (type == 7) 
					{
						if (g_is_rendering_vehicle)
						{
							if (register_num == 8u && is_lightsemissive_shader) {
								on_constant_switchOn(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].bool_ptr);
							}
						}

						game_device->SetVertexShaderConstantB(register_num, constant_data_struct->constants[dataPoolIndex].bool_ptr, 1u);
					}
					else
					{
						//if (register_num == 64u || register_num == 65u || register_num == 69u || register_num == 70u)
						//{
						//	if (ctx.info.shader_name.contains("gta_rmptfx") || ctx.info.shader_name.contains("rmptfx"))
						//	{
						//		/*if (shared::utils::compare_vs_shader_constant_name(shared::globals::d3d_device, register_num, "gSuperAlpha")) {
						//			on_constant_gSuperAlpha(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].float_arr);
						//		}*/
						//	}
						//}

						/*if (shared::utils::compare_vs_shader_constant_name(game_device, register_num, "globalFogParams"))
						{
							if (constant_data_struct->constants[dataPoolIndex].float_arr[0] == 1.337f) 
							{
								int break_me = 0;
							}
						}*/

						/* // can be used to modify sky shader looks 
						if (is_gta_atmoscatt_clouds_shader && !modified_world_matrix)
						{
							auto im = imgui::get();
							D3DXMATRIX mtx;
							game_device->GetVertexShaderConstantF(0, mtx, 4);

							//mtx.m[0][0] *= 1.0f + imgui::get()->m_debug_vector2.x;
							//mtx.m[1][1] *= 1.0f + imgui::get()->m_debug_vector2.y;
							//mtx.m[2][2] *= 1.0f + imgui::get()->m_debug_vector2.z;

							//mtx.m[1][3] = imgui::get()->m_debug_vector2.x;
							//mtx.m[2][3] = imgui::get()->m_debug_vector2.y;
							//mtx.m[3][3] = 1.0f + imgui::get()->m_debug_vector2.z;

							D3DXMATRIX rX, rY, rZ, combined;
							D3DXMatrixRotationX(&rX, D3DXToRadian(im->m_debug_vector2.x));
							D3DXMatrixRotationY(&rY, D3DXToRadian(im->m_debug_vector2.y));
							D3DXMatrixRotationZ(&rZ, D3DXToRadian(im->m_debug_vector2.z));

							// combine rotations
							D3DXMatrixMultiply(&combined, &rZ, &rY);
							D3DXMatrixMultiply(&combined, &combined, &rX);

							// local rotation after scale/translate
							D3DXMATRIX newWorld;
							D3DXMatrixMultiply(&newWorld, &mtx, &combined);


							newWorld.m[0][0] = im->m_debug_mtx02.m[0][0];
							newWorld.m[0][1] += im->m_debug_mtx02.m[0][1];
							newWorld.m[0][2] += im->m_debug_mtx02.m[0][2];
							newWorld.m[0][3] += im->m_debug_mtx02.m[0][3];

							newWorld.m[1][0] += im->m_debug_mtx02.m[1][0];
							newWorld.m[1][1] = im->m_debug_mtx02.m[1][1];
							newWorld.m[1][2] += im->m_debug_mtx02.m[1][2];
							newWorld.m[1][3] += im->m_debug_mtx02.m[1][3];

							newWorld.m[2][0] += im->m_debug_mtx02.m[2][0];
							newWorld.m[2][1] += im->m_debug_mtx02.m[2][1];
							newWorld.m[2][2] = im->m_debug_mtx02.m[2][2];
							newWorld.m[2][3] += im->m_debug_mtx02.m[2][3];

							newWorld.m[3][0] += im->m_debug_mtx02.m[3][0];
							newWorld.m[3][1] += im->m_debug_mtx02.m[3][1];
							newWorld.m[3][2] += im->m_debug_mtx02.m[3][2];
							newWorld.m[3][3] += im->m_debug_mtx02.m[3][3];

							game_device->SetVertexShaderConstantF(0, newWorld, 4);
							modified_world_matrix = true;
						}
						else*/ if ((register_num == 69u || register_num == 70u) && is_gta_rmptfx_litsprite_shader)
						{
							// gSuperAlpha constant
							float constant[4] = { *constant_data_struct->constants[dataPoolIndex].float_arr + gs->gta_rmptfx_litsprite_alpha_scalar.get_as<float>(), 0.0f, 0.0f, 0.0f };
							game_device->SetVertexShaderConstantF(register_num, constant, 1);
						}
						else {
							game_device->SetVertexShaderConstantF(register_num, constant_data_struct->constants[dataPoolIndex].float_arr, float_count * game::pShaderConstFloatCountMap[type]);
						}
					}
				}
				else
				{
					//SetupTextureAndSampler(data->register_pool[psconst->register_pool_index].register_num, constant_data_struct->constants[dataPoolIndex].constant_ptr, (*sampler_data)[2 * dataPoolIndex].unk02 >> 1, *&(*sampler_data)[2 * dataPoolIndex + 1].unk01);

					auto arg1 = (unsigned int)data->register_pool[vs_const->register_pool_index].register_num + 257;
					auto arg2 = constant_data_struct->constants[dataPoolIndex].texture_ref;
					auto arg3 = sampler_data->sampler_constant_data[dataPoolIndex].unk3_lo >> 1;
					auto arg4 = sampler_data->sampler_constant_data[dataPoolIndex].unk9;

					__asm
					{
						pushad;
						push	arg4;
						push	arg3;
						mov		edx, arg2;
						mov		ecx, arg1;
						call	game::func_addr__SetupTextureAndSampler;
						add     esp, 8;
						popad;
					}
				}

				++i;
			} while (i < info->num_vs_constants);
		}
	}

	// every mesh goes through here (besides in-game ui and particles)
	void SetupPixelShaderAndConstants(game::ps_info_s* info, game::ps_data_s* data, game::shader_info_sub_s* constant_data_struct, game::shader_data_sub_s* sampler_data)
	{
		static auto gs = game_settings::get();
		const auto game_device = game::get_d3d_device();
		auto& ctx = renderer::get()->dc_ctx;

		if (*game::g_currentPixelShader != data->shader)
		{
			*game::g_currentPixelShader = data->shader;
			game_device->SetPixelShader(data->shader);
		}

		if (ctx.info.shader_name.empty()) {
			ctx.info.shader_name = constant_data_struct->data->sub.shader_name;
		}

		if (ctx.info.shader_name.contains("gta_trees")) {
			ctx.modifiers.is_tree_foliage = true;
		}
		else if (ctx.info.shader_name.ends_with("gta_grass.fxc")) {
			ctx.modifiers.is_grass_foliage = true;
		}
		else if (ctx.info.shader_name.contains("rmptfx_")) {
			ctx.modifiers.is_fx = true;
		}

		if (info->num_ps_constants > 0)
		{
			bool is_lightsemissive_shader = false;
			bool is_gta_rmptfx_litsprite_shader = false;
			bool is_gta_vehicle_shader = false;
			bool is_gta_radar_shader = false;
			bool is_gta_decal_dirt_shader = false;
			bool is_emissive_shader = false;

			if (ctx.info.shader_name.ends_with("lightsemissive.fxc")) {
				is_lightsemissive_shader = true;
			}
			else if (ctx.info.shader_name.ends_with("gta_rmptfx_litsprite.fxc")) {
				is_gta_rmptfx_litsprite_shader = true;
			}
			else if (ctx.info.shader_name.contains("gta_vehicle_")) {
				is_gta_vehicle_shader = true;
			}
			else if (ctx.info.shader_name.ends_with("gta_radar.fxc")) {
				is_gta_radar_shader = true;
			}
			else if (ctx.info.shader_name.ends_with("gta_decal_dirt.fxc")) {
				is_gta_decal_dirt_shader = true;
			}
			else if (ctx.info.shader_name.contains("emissive")) {
				is_emissive_shader = true;
			}

			int i = 0;
			do
			{
				std::uint32_t dataPoolIndex = info->ps_constant_stack[i].constant_pool_index;
				game::const_stack_s* psconst = &info->ps_constant_stack[i];
				std::uint8_t float_count = constant_data_struct->constant_float_count_array[dataPoolIndex];

				if (float_count)
				{
					std::uint32_t register_pool_index = psconst->register_pool_index;
					std::uint32_t type = data->register_pool[register_pool_index].type;
					std::uint32_t register_num = data->register_pool[register_pool_index].register_num;

					if (type == 1) {
						game_device->SetPixelShaderConstantI(register_num, constant_data_struct->constants[dataPoolIndex].int_ptr, 1u);
					}
					else if (type == 7) 
					{
						if (g_is_rendering_vehicle)
						{
							if (register_num == 8u && is_lightsemissive_shader) {
								on_constant_switchOn(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].bool_ptr);
							}
						}

						game_device->SetPixelShaderConstantB(register_num, constant_data_struct->constants[dataPoolIndex].bool_ptr, 1u);
					}
					else
					{
						if (g_is_rendering_vehicle)
						{
							if (register_num == 66u && is_gta_vehicle_shader && !ctx.info.shader_name.ends_with("sive.fxc")) {
								on_constant_matDiffuseColor(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].float_arr);
							}
							else if (register_num == 72u && is_lightsemissive_shader) // gta_vehicle_lightsemissive
							{
								const float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->vehicle_lights_emissive_scalar.get_as<float>();
								{
									on_constant_emissiveMultiplier(shared::globals::d3d_device, intensity);
									//set_remix_modifier(shared::globals::d3d_device, ctx, RemixModifier::EnableVertexColor); // required when 'isVertexColorBakedLighting' is turned on

									if (gs->vehicle_lights_dual_render_proxy_texture.get_as<bool>())
									{
										ctx.modifiers.dual_render_with_specified_texture = true;
										ctx.modifiers.dual_render_texture = tex_addons::veh_light_ems_glass;
										ctx.modifiers.dual_render_reset_remix_modifiers = true;
										ctx.modifiers.dual_render_mode_blend_diffuse = true;
									}
								}
							}
						}

						else if (register_num == 66u && is_gta_radar_shader) {
							on_constant_diffuseCol(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].float_arr);
						}

						else
						{
							if (register_num == 66u && is_emissive_shader)
							{
								const auto stype = get_emissive_shader_type(ctx.info.shader_name);
								if (stype != EmissiveShaderType::None)
								{
									switch (stype)
									{
									case EmissiveShaderType::EmissiveNight:
										renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
											*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_night_surfaces_emissive_scalar.get_as<float>());
										break;

									case EmissiveShaderType::Emissive:
										renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
											*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_surfaces_emissive_scalar.get_as<float>());
										break;

									case EmissiveShaderType::EmissiveStrong:
										renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
											*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_strong_surfaces_emissive_scalar.get_as<float>(), true);
										break;
									}
								}

								//const float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_strong_surfaces_emissive_scalar.get_as<float>();
								//on_constant_emissiveMultiplier(shared::globals::d3d_device, intensity);
							}
						}


						if (g_is_rendering_static)
						{
							/*if (register_num == 66u && ctx.info.shader_name.contains("emissive"))
							{ 
								const float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_strong_surfaces_emissive_scalar.get_as<float>();
								on_constant_emissiveMultiplier(shared::globals::d3d_device, intensity);
							}
							else*/ if (gs->decal_dirt_shader_usage.get_as<bool>() && register_num == 66u && is_gta_decal_dirt_shader)
							{
								float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->decal_dirt_shader_scalar.get_as<float>();
								renderer::set_remix_temp_float01(shared::globals::d3d_device, intensity);
								renderer::set_remix_temp_float02(shared::globals::d3d_device, gs->decal_dirt_shader_contrast.get_as<float>());
							}
						}

						game_device->SetPixelShaderConstantF(register_num, constant_data_struct->constants[dataPoolIndex].float_arr, float_count * game::pShaderConstFloatCountMap[type]);
					}
				}
				else
				{
					//SetupTextureAndSampler(data->register_pool[psconst->register_pool_index].register_num, constant_data_struct->constants[dataPoolIndex].constant_ptr, (*sampler_data)[2 * dataPoolIndex].unk02 >> 1, *&(*sampler_data)[2 * dataPoolIndex + 1].unk01);

					auto arg1 = (unsigned int)data->register_pool[psconst->register_pool_index].register_num;
					auto arg2 = constant_data_struct->constants[dataPoolIndex].texture_ref;
					auto arg3 = sampler_data->sampler_constant_data[dataPoolIndex].unk3_lo >> 1;
					auto arg4 = sampler_data->sampler_constant_data[dataPoolIndex].unk9;

					if (gs->load_colormaps_only.get_as<bool>())
					{
						// everything that is not 0 is not a colormap (I hope)
						if (arg1) 
						{
							++i;
							continue;
						}
					}
//#if DEBUG
//					if (ctx.info.shader_name.ends_with("deferred_lighting.fxc") && arg2 && (arg2->N00000096 == 2 || arg2->N00000096 == 0)) {
//						int break_me = 1; 
//					}
//
//					if (ctx.info.shader_name.ends_with("deferred_lighting.fxc") && arg2 && (arg2->N00000096 == 2 || arg2->N00000096 == 0)
//						&& std::string_view(arg2->texture_name_no_ext).contains("carposter")) 
//					{
//						int break_me = 1;
//					}
//#endif

					__asm
					{
						pushad;
						push	arg4;
						push	arg3;
						mov		edx, arg2;
						mov		ecx, arg1;
						call	game::func_addr__SetupTextureAndSampler;
						add     esp, 8;
						popad;
					}
				}

				++i;
			} while (i < info->num_ps_constants);
		}
	}

	// -----

	DETOUR_TYPEDEF(SetupVsPsPass, void, __thiscall, game::current_pass_s* pass, game::vs_data_s** vs_data_struct, game::ps_data_s** ps_data_struct, game::shader_info_sub_s* data, game::shader_data_sub_s* sampler_data);

	void __fastcall SetupVsPsPass_hk(game::current_pass_s* pass, [[maybe_unused]] int fastcall, game::vs_data_s** vs_data_struct, game::ps_data_s** ps_data_struct, game::shader_info_sub_s* data, game::shader_data_sub_s* sampler_data)
	{
		// in case we need to compare the rewritten one to the og version
		//return SetupVsPsPass_og(pass, vs_data_struct, ps_data_struct, data, sampler_data);

		const auto game_device = game::get_d3d_device();
		int n_renderstates = pass->num_renderstates;

		for (int i = 0; i < n_renderstates; ++i)
		{
			const auto rs_data = &pass->renderstate_value_stack[i];
			const auto rs = game::pRenderStateIndexMap[rs_data->renderstate_index];

			if (rs != -1) {
				game_device->SetRenderState((D3DRENDERSTATETYPE)rs, rs_data->renderstate_data);
			}
		}

		// SetupVertexShaderAndConstants(&pass->vs_info, &(*vs_data_struct)[pass->vs_info.vs_data_index], data, sampler_data);
		//shared::utils::hook::call<void(__fastcall)(game::vs_info_s* info, void* thiscall_arg, game::vs_data_s* data, game::shader_info_sub_s* constant_pool, game::shader_data_sub_s* sampler_data)>
		//	(0x437170)(&pass->vs_info, nullptr, &(*vs_data_struct)[pass->vs_info.vs_data_index], data, sampler_data);

		SetupVertexShaderAndConstants(&pass->vs_info, &(*vs_data_struct)[pass->vs_info.vs_data_index], data, sampler_data);


		// SetupPixelShaderAndConstants
		//shared::utils::hook::call<void(__fastcall)(game::ps_info_s* info, void* thiscall_arg, game::ps_data_s* data, game::shader_info_sub_s* constant_pool, game::sampler_data_s** sampler_data)>
		//	(0x4372B0)(&pass->ps_info, nullptr, &(*ps_data_struct)[pass->ps_info.ps_data_index], data, sampler_data);

		SetupPixelShaderAndConstants(&pass->ps_info, &(*ps_data_struct)[pass->ps_info.ps_data_index], data, sampler_data);
	}


	// ----



	HRESULT renderer::on_draw_primitive(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const UINT& StartVertex, const UINT& PrimitiveCount)
	{
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		auto& ctx = renderer::dc_ctx;
		ctx.info.device_ptr = dev;
		// info.shader_name can be empty here -> TODO

		bool render_with_ff = false;

		// fixes the skylight
		dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);

		/*if (im->m_dbg_ignore_drawprimitive)
		{
			if (ctx.modifiers.do_not_render) 
			{
				int x = 1;
			}
		}*/

		//if (ctx.info.shader_name.ends_with("water.fxc"))
		//{
		//	set_remix_texture_hash(dev, shared::utils::string_hash32("water_distant"));
		//	ctx.info.is_dirty = true;
		//	//im->m_dbg_ignore_drawprimitive = true;
		//}
		//else if (!ctx.info.shader_name.empty())
		//{
		//	ctx.info.is_dirty = false;
		//}

		if (g_is_rendering_mirror) //ctx.info.shader_name.ends_with("mirror.fxc"))
		{
			set_remix_texture_hash(dev, shared::utils::string_hash32("mirror"));
			ctx.save_texture(dev, 0);
			dev->SetTexture(0, tex_addons::mirror);
		}

		const auto viewport = game::pCurrentViewport;
		if (viewport && viewport->wp)
		{
			//dev->SetTransform(D3DTS_WORLD, &viewport->wp->world);
			dev->SetTransform(D3DTS_VIEW, &viewport->wp->view);
			dev->SetTransform(D3DTS_PROJECTION, &viewport->wp->proj);

			// automatically tag sky surfs as sky
			// + custom sky texture because sky and traffic lights share the same texture
			if (g_is_sky_rendering)
			{
				// we need to rotate the sky by 90 degrees to fix half of the sky being black - does not influence the primary view
				{
					D3DXMATRIX mtx;
					memset(&mtx, 0, sizeof(D3DXMATRIX));
					mtx.m[0][0] = 1.0f /*+ imgui::get()->m_debug_vector2.x*/;
					mtx.m[1][1] = 1.0f /*+ imgui::get()->m_debug_vector2.y*/;
					mtx.m[2][2] = 1.0f /*+ imgui::get()->m_debug_vector2.z*/;
					mtx.m[3][3] = 1.0f;

					D3DXMATRIX rX /*, rY, rZ, combined*/;
					D3DXMatrixRotationX(&rX, D3DXToRadian(-90.0f/*im->m_debug_vector.x*/)); // set to -90 .. thats all thats needed here
					//D3DXMatrixRotationY(&rY, D3DXToRadian(im->m_debug_vector.y));
					//D3DXMatrixRotationZ(&rZ, D3DXToRadian(im->m_debug_vector.z));

					// Combine as Z * Y * X (common Euler order; adjust if needed)
					//D3DXMatrixMultiply(&combined, &rZ, &rY);
					//D3DXMatrixMultiply(&combined, &combined, &rX);

					// Multiply: new_world = world * rot (local rotation after scale/translate)
					D3DXMATRIX newWorld;
					D3DXMatrixMultiply(&newWorld, &mtx, &rX /*&combined*/);

					/*newWorld.m[0][0] += im->m_debug_mtx02.m[0][0];
					newWorld.m[0][1] += im->m_debug_mtx02.m[0][1];
					newWorld.m[0][2] += im->m_debug_mtx02.m[0][2];
					newWorld.m[0][3] += im->m_debug_mtx02.m[0][3];

					newWorld.m[1][0] += im->m_debug_mtx02.m[1][0];
					newWorld.m[1][1] += im->m_debug_mtx02.m[1][1];
					newWorld.m[1][2] += im->m_debug_mtx02.m[1][2];
					newWorld.m[1][3] += im->m_debug_mtx02.m[1][3];

					newWorld.m[2][0] += im->m_debug_mtx02.m[2][0];
					newWorld.m[2][1] += im->m_debug_mtx02.m[2][1];
					newWorld.m[2][2] += im->m_debug_mtx02.m[2][2];
					newWorld.m[2][3] += im->m_debug_mtx02.m[2][3];

					newWorld.m[3][0] += im->m_debug_mtx02.m[3][0];
					newWorld.m[3][1] += im->m_debug_mtx02.m[3][1];
					newWorld.m[3][2] += im->m_debug_mtx02.m[3][2];
					newWorld.m[3][3] += im->m_debug_mtx02.m[3][3];*/

					dev->SetTransform(D3DTS_WORLD, &newWorld);
				}

				set_remix_texture_categories(dev, InstanceCategories::Sky); 

				if (ctx.info.shader_name.ends_with("im.fxc")) {
					dev->SetTexture(0, tex_addons::sky);
				}
			}

			// rendering water via FF is fine and safes a ton of performance
			if (g_is_water_rendering)
			{
				dev->SetTransform(D3DTS_WORLD, &viewport->wp->world);
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
				render_with_ff = true;

				// override texture hash so that it never changes
				if (gs->override_water_texture_hash.get_as<bool>()) {
					set_remix_texture_hash(dev, shared::utils::string_hash32("water"));
				}
			}

			if (ctx.info.shader_name.ends_with("gta_rmptfx_litsprite.fxc"))  
			{
				set_remix_texture_categories(dev, InstanceCategories::Particle);
				ctx.modifiers.is_fx = true;
				ctx.save_ps(dev);
				dev->SetPixelShader(nullptr);

				//ctx.modifiers.do_not_render = true;
				//shared::utils::lookat_vertex_decl(dev);

				/*dev->SetTransform(D3DTS_WORLD, &viewport->wp->world); 
				D3DMATRIX matrix = viewport->wp->proj;
				matrix.m[0][0] += im->m_debug_mtx02.m[0][0]; 
				matrix.m[0][1] += im->m_debug_mtx02.m[0][1];
				matrix.m[0][2] += im->m_debug_mtx02.m[0][2];
				matrix.m[0][3] += im->m_debug_mtx02.m[0][3];

				matrix.m[1][0] += im->m_debug_mtx02.m[1][0];
				matrix.m[1][1] += im->m_debug_mtx02.m[1][1];
				matrix.m[1][2] += im->m_debug_mtx02.m[1][2];
				matrix.m[1][3] += im->m_debug_mtx02.m[1][3];

				matrix.m[2][0] += im->m_debug_mtx02.m[2][0];
				matrix.m[2][1] += im->m_debug_mtx02.m[2][1];
				matrix.m[2][2] += im->m_debug_mtx02.m[2][2];
				matrix.m[2][3] += im->m_debug_mtx02.m[2][3];

				matrix.m[3][0] += im->m_debug_mtx02.m[3][0];
				matrix.m[3][1] += im->m_debug_mtx02.m[3][1];
				matrix.m[3][2] += im->m_debug_mtx02.m[3][2];
				matrix.m[3][3] += im->m_debug_mtx02.m[3][3];
			
				render_with_ff = false;
				dev->SetTransform(D3DTS_PROJECTION, &matrix);*/
			} 

			if (viewport->wp->proj.m[3][3] == 1.0f && !g_is_rendering_phone) {
				manually_trigger_remix_injection(dev);
			}
		}

		if (g_is_water_rendering && im->m_dbg_do_not_render_water) {
			ctx.modifiers.do_not_render = true;
		}

		if (ctx.modifiers.is_fx)
		{
			ctx.save_tss(dev, D3DTSS_COLOROP);
			ctx.save_tss(dev, D3DTSS_COLORARG1);
			ctx.save_tss(dev, D3DTSS_COLORARG2);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

			ctx.save_tss(dev, D3DTSS_ALPHAOP);
			ctx.save_tss(dev, D3DTSS_ALPHAARG2);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		}

		if (render_with_ff)
		{
			ctx.save_vs(dev);
			dev->SetVertexShader(nullptr);
		}


		// ---------
		// draw

		auto hr = S_OK;

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render) {
			hr = dev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
		}


		// ---------
		// post draw

		if (!im->m_dbg_ignore_drawprimitive && !ctx.info.is_dirty)
		{
			ctx.restore_all(dev);
			ctx.reset_context();
		}

		return hr;
	}



	HRESULT renderer::on_draw_indexed_prim(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const INT& BaseVertexIndex, const UINT& MinVertexIndex, const UINT& NumVertices, const UINT& startIndex, const UINT& primCount)
	{
		auto& ctx = renderer::dc_ctx;

		if (ctx.info.is_dirty) {
			ctx.reset_context();
		}

		ctx.info.device_ptr = dev; // for instanced rendering

		static auto im = imgui::get();
		static auto gs = game_settings::get();
		 
		if (!shared::globals::imgui_is_rendering)
		{
			if (im->m_dbg_skip_draw_indexed_checks)
			{
				const auto viewport = game::pCurrentViewport;
				if (viewport && viewport->wp)
				{
					dev->SetTransform(D3DTS_VIEW, &viewport->wp->view);
					dev->SetTransform(D3DTS_PROJECTION, &viewport->wp->proj);
				}

				return dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
			}

			// shared::utils::lookat_vertex_decl(dev);

			bool render_with_ff = g_is_rendering_static;
			bool allow_vertex_colors = false;

			if (ctx.info.shader_name.ends_with("water.fxc")) {
				ctx.modifiers.do_not_render = true;
			}

			else if (ctx.info.shader_name.ends_with("glue.fxc")) 
			{
				//set_remix_modifier(dev, RemixModifier::EnableVertexColor);
				allow_vertex_colors = true;

				//ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				//dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

				ctx.save_rs(dev, D3DRS_SRCBLEND);
				ctx.save_rs(dev, D3DRS_DESTBLEND);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

				ctx.save_tss(dev, D3DTSS_COLOROP);
				ctx.save_tss(dev, D3DTSS_COLORARG1);
				ctx.save_tss(dev, D3DTSS_COLORARG2);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

				ctx.save_tss(dev, D3DTSS_ALPHAOP);
				ctx.save_tss(dev, D3DTSS_ALPHAARG1);
				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

				DWORD alpha_blending = 0;
				dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &alpha_blending);

				if (alpha_blending) {
					set_remix_texture_categories(dev, InstanceCategories::DecalStatic);
				}
			}

			else if (ctx.info.shader_name.ends_with("_decal.fxc")) {
				set_remix_texture_categories(dev, InstanceCategories::DecalStatic);
			}

			//if (g_is_rendering_mirror) //ctx.info.shader_name.ends_with("mirror.fxc"))
			//{
			//	//static auto mirror_hash = shared::utils::string_hash32("mirror");
			//	//set_remix_texture_hash(dev, mirror_hash);
			//	int x = 1;
			//}

			if (g_is_rendering_static)
			{
				if (im->m_dbg_disable_ps_for_static)
				{
					ctx.save_ps(dev);
					dev->SetPixelShader(nullptr);
				}

				// handle gta_decal_dirt shader
				else if (ctx.info.shader_name.ends_with("_dirt.fxc"))
				{
					if (gs->decal_dirt_shader_usage.get_as<bool>())
					{
						set_remix_modifier(dev, RemixModifier::DecalDirt | RemixModifier::EnableVertexColor);
						set_remix_texture_categories(dev, InstanceCategories::DecalStatic);

						// using a texture in slot1 in the opaque shader is semi working but the texture is not always updated/correct and and cause
						// conflicts with other textures
					}
					else {
						set_remix_texture_categories(dev, InstanceCategories::Ignore);
					}
				}
			}

			// 
			const auto stype = get_emissive_shader_type(ctx.info.shader_name);
			if (stype != EmissiveShaderType::None)
			{
				if (gs->render_emissive_surfaces_using_shaders.get_as<bool>())
				{
					if (im->m_dbg_tag_static_emissive_as_index != -1) {
						set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
					}
					else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
						set_remix_texture_categories(dev, InstanceCategories::DecalStatic /*InstanceCategories::IgnoreTransparencyLayer*/); //1 << im->m_dbg_tag_emissivenight_as_index);
					}

					render_with_ff = false;
				}
			}

			// check if trees should be rendered with shaders
			if (ctx.modifiers.is_tree_foliage && !gs->fixed_function_trees.get_as<bool>()) {
				render_with_ff = false;
			}

			if (im->m_dbg_toggle_ff) {
				render_with_ff = false;
			}

			/*const auto lcolr = game::getGlobalShaderInfoParam("globalFogParams");
			if (lcolr) {
				float r = *(float*)lcolr->pValue;
			}*/

			const auto viewport = game::pCurrentViewport;

			// make VS use the correct matrix (if Use World Transforms is enabled in remix)
			dev->SetTransform(D3DTS_WORLD, game::pCurrentWorldTransform);

			if (render_with_ff)
			{
				ctx.save_vs(dev);
				dev->SetVertexShader(nullptr);
			}

			if (viewport && viewport->wp) 
			{
				dev->SetTransform(D3DTS_VIEW, &viewport->wp->view);
				dev->SetTransform(D3DTS_PROJECTION, &viewport->wp->proj);
			}

			if (g_is_rendering_phone)
			{
				ctx.restore_vs(dev);
				render_with_ff = false;

				D3DMATRIX matrix = {};
				matrix.m[0][0] = 300.0f + im->m_dbg_phone_projection_matrix_offset.m[0][0];
				matrix.m[0][1] = 12.0f + im->m_dbg_phone_projection_matrix_offset.m[0][1];
				matrix.m[0][2] += im->m_dbg_phone_projection_matrix_offset.m[0][2];
				matrix.m[0][3] = -40.0f + im->m_dbg_phone_projection_matrix_offset.m[0][3];

				matrix.m[1][0] = -13.0f + im->m_dbg_phone_projection_matrix_offset.m[1][0];
				matrix.m[1][1] = 300.0f + im->m_dbg_phone_projection_matrix_offset.m[1][1];
				matrix.m[1][2] += im->m_dbg_phone_projection_matrix_offset.m[1][2];
				matrix.m[1][3] = -4.0f + im->m_dbg_phone_projection_matrix_offset.m[1][3];

				matrix.m[2][0] = 100.0f + im->m_dbg_phone_projection_matrix_offset.m[2][0];
				matrix.m[2][1] = -5.0f + im->m_dbg_phone_projection_matrix_offset.m[2][1];
				matrix.m[2][2] = (viewport->wp->farclip / (viewport->wp->farclip - viewport->wp->nearclip)) + im->m_dbg_phone_projection_matrix_offset.m[2][2];
				matrix.m[2][3] = -250.0f + im->m_dbg_phone_projection_matrix_offset.m[2][3];

				matrix.m[3][0] += im->m_dbg_phone_projection_matrix_offset.m[3][0];
				matrix.m[3][1] += im->m_dbg_phone_projection_matrix_offset.m[3][1];
				matrix.m[3][2] = ((-viewport->wp->nearclip * viewport->wp->farclip) / (viewport->wp->farclip - viewport->wp->nearclip)) + im->m_dbg_phone_projection_matrix_offset.m[3][2];
				matrix.m[3][3] = 0.01f + im->m_dbg_phone_projection_matrix_offset.m[3][3];

				dev->SetTransform(D3DTS_PROJECTION, &matrix);

				if (gs->phone_emissive_override.get_as<bool>())
				{
					set_remix_texture_categories(dev, InstanceCategories::WorldUI | InstanceCategories::IgnoreAlphaChannel);
					set_remix_modifier(dev, RemixModifier::EmissiveScalar);
					set_remix_emissive_intensity(dev, gs->phone_emissive_scalar.get_as<float>(), false);
				}
			}

			if (g_is_rendering_vehicle) 
			{
				// we have to disable the pixelshader in order for remix to grab the vertexshader color-output
				// it uses the vertexshader color-input otherwise
				ctx.save_ps(dev);
				dev->SetPixelShader(nullptr);

				if (ctx.modifiers.is_vehicle_using_switch_on_state)
				{
					if (!ctx.modifiers.is_vehicle_on) {
						on_constant_emissiveMultiplier(shared::globals::d3d_device, 0.0f);
					}
				}
			}

			if (ctx.modifiers.is_tree_foliage || ctx.modifiers.is_grass_foliage)
			{
				// enable alpha testing
				ctx.save_rs(dev, D3DRS_ALPHATESTENABLE);
				ctx.save_rs(dev, D3DRS_ALPHAFUNC);
				ctx.save_rs(dev, D3DRS_ALPHAREF);

				dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
				dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

				float alpha_setting = 0.7f;
				if (ctx.modifiers.is_tree_foliage) {
					alpha_setting = gs->tree_foliage_alpha_cutout_value.get_as<float>();
				} else if (ctx.modifiers.is_grass_foliage) {
					alpha_setting = gs->grass_foliage_alpha_cutout_value.get_as<float>();
				}

				float alpha_ref = 0.3125f / std::abs(alpha_setting); // compute normalized alpha reference
					  alpha_ref = std::min(alpha_ref, 1.0f); // clamp to [0, 1]

				DWORD alpha_ref_int = static_cast<DWORD>(alpha_ref * 255.0f); // convert to 0-255 range
				dev->SetRenderState(D3DRS_ALPHAREF, alpha_ref_int);

				// disable alpha blending (since we're using alpha testing)
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

				// enable Z-writing and Z-testing
				ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
				ctx.save_rs(dev, D3DRS_ZENABLE);
				ctx.save_rs(dev, D3DRS_ZFUNC);

				dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
				dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
				dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

				// disable culling for two-sided foliage
				ctx.save_rs(dev, D3DRS_CULLMODE);
				dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

				// texture stage states for alpha testing
				ctx.save_tss(dev, D3DTSS_ALPHAOP);
				ctx.save_tss(dev, D3DTSS_ALPHAARG1);

				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); 
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			}

			if (ctx.modifiers.is_fx)
			{
				//ctx.save_tss(dev, D3DTSS_ALPHAOP);
				//dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			}

			if (!allow_vertex_colors){
				set_remix_texture_categories(dev, InstanceCategories::IgnoreBakedLighting);
			}

			if (g_is_rendering_static)
			{
				if (im->m_dbg_do_not_render_static) {
					ctx.modifiers.do_not_render = true;
				}
			}

			if (g_is_rendering_vehicle)
			{
				if (im->m_dbg_do_not_render_vehicle) {
					ctx.modifiers.do_not_render = true;
				}
			}

			if (g_is_instance_rendering)
			{
				if (im->m_dbg_do_not_render_instances) {
					ctx.modifiers.do_not_render = true;
				}
			}

			if (gs->timecycle_wetness_enabled.get_as<bool>())
			{
				// check for stencil
				DWORD stencil_ref = 0u, stencil_enabled = 0u;
				dev->GetRenderState(D3DRS_STENCILREF, &stencil_ref);
				dev->GetRenderState(D3DRS_STENCILENABLE, &stencil_enabled);

				// surface can get wet if stencil = 0
				if (stencil_enabled && stencil_ref == 0
					|| (g_is_rendering_vehicle && !ctx.info.shader_name.ends_with("_interior.fxc"))
					|| ctx.info.shader_name.ends_with("ped_reflect.fxc"))
				{
					auto get_wetness = []()
						{
							const auto wetness_value = game::pTimeCycleWetnessChange; // normally pTimeCycleWetness but we use wetness change
							const auto specularOffset = game::pTimeCycleSpecularOffset;

							float w = *wetness_value;
							if (*wetness_value >= 0.0f)
							{
								if (*wetness_value > 1.0f) {
									w = 1.0f;
								}
							}

							const float s = *specularOffset * w;
							if (*specularOffset * w < 0.0f) {
								return 0.0f;
							}

							if (s <= 1.0f) {
								return s;
							}

							return 1.0f;
						};

					const float wetness_value = get_wetness();
					if (wetness_value > 0.0f)
					{
						const float adjusted_wetness = std::clamp(wetness_value * gs->timecycle_wetness_scalar.get_as<float>(), 0.0f, 1.0f);
						float scalar = 1.0f - adjusted_wetness;
						scalar = std::clamp(scalar * 0.8f, 0.0f, 1.0f);
						scalar += gs->timecycle_wetness_offset.get_as<float>();
						scalar = scalar < 0.0f ? 0.0f : scalar;
						set_remix_roughness_scalar(dev, scalar);
					}

					if (im->m_dbg_do_not_render_stencil_zero) {
						ctx.modifiers.do_not_render = true;
					}
				}
			}

			if (ctx.modifiers.is_tree_foliage)
			{
				if (im->m_dbg_do_not_render_tree_foliage) {
					ctx.modifiers.do_not_render = true;
				}
			}

			if (render_with_ff)
			{
				if (im->m_dbg_do_not_render_ff) {
					ctx.modifiers.do_not_render = true;
				}
			}

			/*if (ctx.info.shader_name.ends_with("strong.fxc")) 
			{
				ctx.modifiers.dual_render = true;
				ctx.modifiers.dual_render_mode_blend_diffuse = true;
				set_remix_emissive_intensity(dev, ctx, gs->emissive_strong_surfaces_emissive_scalar.get_as<float>()); 
			}*/
		}


		// ---------
		// draw

		auto hr = S_OK;

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render && !ctx.modifiers.do_not_render_indexed_primitives) {
			hr = dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		}


		// second drawings

		if (ctx.modifiers.dual_render || ctx.modifiers.dual_render_with_specified_texture)
		{
			if (ctx.modifiers.dual_render_reset_remix_modifiers) {
				ctx.restore_render_state(dev, (D3DRENDERSTATETYPE)149);
			}

			if (ctx.modifiers.dual_render_with_specified_texture && ctx.modifiers.dual_render_texture)
			{
				// save og texture
				ctx.save_texture(dev, 0);

				// set new texture
				dev->SetTexture(0, ctx.modifiers.dual_render_texture);
			}

			// BLEND ADD mode
			if (ctx.modifiers.dual_render_mode_blend_add)
			{
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

				ctx.save_rs(dev, D3DRS_BLENDOP);
				dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

				ctx.save_rs(dev, D3DRS_SRCBLEND);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);

				ctx.save_rs(dev, D3DRS_DESTBLEND);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

				ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
				dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

				ctx.save_rs(dev, D3DRS_ZENABLE);
				dev->SetRenderState(D3DRS_ZENABLE, FALSE);

				set_remix_texture_categories(dev, InstanceCategories::WorldMatte | InstanceCategories::IgnoreOpacityMicromap);
			}

			if (ctx.modifiers.dual_render_mode_blend_diffuse)
			{
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

				ctx.save_rs(dev, D3DRS_BLENDOP);
				dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

				ctx.save_rs(dev, D3DRS_SRCBLEND);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

				ctx.save_rs(dev, D3DRS_DESTBLEND);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

				ctx.save_tss(dev, D3DTSS_COLOROP);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_COLORARG1);
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_COLORARG2);
				dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

				ctx.save_tss(dev, D3DTSS_ALPHAOP);
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG1);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			}

			// re-draw surface
			dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

			if (ctx.modifiers.dual_render_with_specified_texture) {
				ctx.restore_texture(dev, 0);
			}
		}
		


		// ---------
		// post draw

		if (!g_is_instance_rendering)
		{
			ctx.restore_all(dev);
			ctx.reset_context();
		}
		
		return hr;
	}

	void renderer::manually_trigger_remix_injection(IDirect3DDevice9* dev)
	{
		/*if (game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive) {
			return;
		}

		if (game::CMenuManager__m_LoadscreenActive && *game::CMenuManager__m_LoadscreenActive) {
			return;
		}*/

		if (!game::is_in_game) {
			return;
		}

		if (!m_triggered_remix_injection)
		{
			//const auto& dev = shared::globals::d3d_device;
			auto& ctx = dc_ctx;

			dev->SetRenderState(D3DRS_FOGENABLE, FALSE);

			ctx.save_vs(dev);
			dev->SetVertexShader(nullptr);
			ctx.save_ps(dev);
			dev->SetPixelShader(nullptr); // needed

			ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); // required - const bool zWriteEnabled = d3d9State().renderStates[D3DRS_ZWRITEENABLE]; -> if (isOrthographic && !zWriteEnabled)


			struct CUSTOMVERTEX
			{
				float x, y, z, rhw;
				D3DCOLOR color;
			};

			const auto color = D3DCOLOR_COLORVALUE(0, 0, 0, 0);
			const auto w = -0.49f;
			const auto h = -0.495f;

			CUSTOMVERTEX vertices[] =
			{
				{ -0.5f, -0.5f, 0.0f, 1.0f, color }, // tl
				{     w, -0.5f, 0.0f, 1.0f, color }, // tr
				{ -0.5f,     h, 0.0f, 1.0f, color }, // bl
				{     w,     h, 0.0f, 1.0f, color }  // br
			};

			// setting fvf in any way f's up everything??
			//DWORD og_fvf;
			//dev->GetFVF(&og_fvf);
			//dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));
			//dev->SetFVF(og_fvf);

			ctx.restore_vs(dev);
			ctx.restore_ps(dev);
			ctx.restore_render_state(dev, D3DRS_ZWRITEENABLE);
			m_triggered_remix_injection = true;
		}
	}


	//
	//

	__declspec (naked) void on_instanced_render__pre_setup_vs_ps_pass_stub()
	{
		__asm
		{
			mov		g_is_instance_rendering, 1;
			add     ecx, eax;
			lea     eax, [edx + 8];
			jmp		game::retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub;
		}
	}

	void on_instanced_render__post_setup_vs_ps_pass_hk()
	{
		auto& ctx = renderer::get()->dc_ctx;

		assert(ctx.info.device_ptr);
		ctx.restore_all(ctx.info.device_ptr);
		ctx.reset_context();

		g_is_instance_rendering = 0;
	}

	__declspec (naked) void on_instanced_render__post_setup_vs_ps_pass_stub()
	{
		__asm
		{
			pushad;
			call	on_instanced_render__post_setup_vs_ps_pass_hk;
			popad;
			add     eax, 0x20;
			dec     dword ptr[esp + 0x1C];
			jmp		game::retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub;
		}
	}

	__declspec (naked) void on_phone_phase_clear_stub()
	{
		static float custom_z_marker = 0.1337f;
		__asm
		{
			movss   xmm0, dword ptr[custom_z_marker];
			jmp		game::retn_addr__on_phone_phase_clear_stub;
		}
	}

	void post_render_sky()
	{
		//renderer::get()->m_modified_draw_prim = false;
		remix_markers::get()->draw_nocull_markers();
	}

	__declspec (naked) void on_sky_render_stub()
	{
		__asm
		{
			mov		g_is_sky_rendering, 1;
			call	game::func_addr__on_sky_render_stub;
			mov		g_is_sky_rendering, 0;
			pushad;
			call	post_render_sky;
			popad;
			jmp		game::retn_addr__on_sky_render_stub;
		}
	}

	__declspec (naked) void pre_water_render_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF0;
			mov		g_is_water_rendering, 1;
			jmp		game::retn_addr__pre_draw_water;
		}
	}

	__declspec (naked) void post_water_render_stub()
	{
		__asm
		{
			mov		g_is_water_rendering, 0;
			pop     edi;
			pop     esi;
			mov     esp, ebp;
			pop     ebp;
			retn;
		}
	}

	__declspec (naked) void pre_static_render_stub()
	{
		__asm
		{
			mov     ebx, ecx;
			cmp     eax, 0xFFFFFFFF;
			mov		g_is_rendering_static, 1;
			jmp		game::retn_addr__pre_draw_statics;
		}
	}

	__declspec (naked) void post_static_render_stub()
	{
		__asm
		{
			mov		g_is_rendering_static, 0;
			retn    0x10;
		}
	}

	// ---

	__declspec (naked) void pre_draw_mirror_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF0;
			mov		g_is_rendering_mirror, 1;
			jmp		game::retn_addr__pre_draw_mirror;
		}
	}

	__declspec (naked) void post_draw_mirror_stub()
	{
		__asm
		{
			mov		g_is_rendering_mirror, 0;
			mov     esp, ebp;
			pop     ebp;
			retn;
		}
	}

	renderer::renderer()
	{
		p_this = this;

		shared::utils::hook::detour(game::hk_addr__SetupVsPsPass_hk, &SetupVsPsPass_hk, DETOUR_CAST(SetupVsPsPass_og));

		// there is a renderpath that calls SetupVsPsPass once and then renders multiple instances
		// we can not reset the per drawcall context (renderer::dc_ctx) after a drawcall and need to wait until the very last instance was rendered
		shared::utils::hook(game::retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub - 5u, on_instanced_render__pre_setup_vs_ps_pass_stub, HOOK_JUMP).install()->quick(); // 0x69ED69
		shared::utils::hook::nop(game::hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub, 7);
		shared::utils::hook(game::hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub, on_instanced_render__post_setup_vs_ps_pass_stub, HOOK_JUMP).install()->quick();

		// detecting phone rendering is tricky - modify Z argument of 'clear' drawlist-command to 1.337
		// check all clears in 'd3d9ex::D3D9Device::Clear' for modified Z arg -> assume phone rendering
		// until next clear is called (phone gets rendered to another rendertarget)
		shared::utils::hook(game::retn_addr__on_phone_phase_clear_stub - 5u, on_phone_phase_clear_stub, HOOK_JUMP).install()->quick(); // 0x5E1646

		// detect sky rendering
		shared::utils::hook(game::retn_addr__on_sky_render_stub - 5u, on_sky_render_stub, HOOK_JUMP).install()->quick(); // C107C9

		// detect water rendering
		shared::utils::hook(game::retn_addr__pre_draw_water - 5u, pre_water_render_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook(game::hk_addr__post_draw_water, post_water_render_stub, HOOK_JUMP).install()->quick();

		// detect vehicle rendering
		shared::utils::hook(game::retn_addr__pre_draw_statics - 5u, pre_static_render_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook(game::hk_addr__post_draw_statics, post_static_render_stub, HOOK_JUMP).install()->quick();

		// detect mirror rendering
		shared::utils::hook(game::retn_addr__pre_draw_mirror - 5u, pre_draw_mirror_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook(game::hk_addr__post_draw_mirror, post_draw_mirror_stub, HOOK_JUMP).install()->quick();

		// do not render postfx RT infront of the camera
		shared::utils::hook::nop(game::nop_addr__disable_postfx_drawing, 5);


		// -----
		m_initialized = true;
		std::cout << "[RENDERER] loaded\n";
	}
}
