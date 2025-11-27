#include "std_include.hpp"
#include "renderer.hpp"

#include "d3d9ex.hpp"
#include "game_settings.hpp"
#include "imgui.hpp"
#include "natives.hpp"
#include "remix_markers.hpp"
#include "renderer_ff.hpp"

namespace gta4
{
	int g_is_instance_rendering = 0;
	int g_is_sky_rendering = 0;
	int g_is_water_rendering = 0;
	int g_is_rendering_mirror = 0;
	int g_is_rendering_fx_instance = 0;
	int g_is_rendering_fx = 0;
	int  g_is_rendering_static = 0;
	int  g_is_rendering_vehicle = 0;
	bool g_is_rendering_phone = false;

	bool g_rendered_first_primitive = false;
	bool g_applied_hud_hack = false; // was hud "injection" applied this frame

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

			shared::common::log("Renderer", "Loading CompMod Textures ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

			auto load_texture = [](IDirect3DDevice9* dev, const char* path, LPDIRECT3DTEXTURE9* tex)
				{
					HRESULT hr;
					hr = D3DXCreateTextureFromFileA(dev, path, tex);
					if (FAILED(hr)) shared::common::log("Renderer", std::format("Failed to load {}", path), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				};

			const auto dev = shared::globals::d3d_device;
			load_texture(dev, "rtx_comp\\textures\\sky.png", &tex_addons::sky);
			load_texture(dev, "rtx_comp\\textures\\white01.png", &tex_addons::white01);
			load_texture(dev, "rtx_comp\\textures\\white02.png", &tex_addons::white02);
			load_texture(dev, "rtx_comp\\textures\\veh_light_ems_glass.png", &tex_addons::veh_light_ems_glass);
			load_texture(dev, "rtx_comp\\textures\\berry.png", &tex_addons::berry);
			load_texture(dev, "rtx_comp\\textures\\mirror.png", &tex_addons::mirror);
			tex_addons::initialized = true;
		}
	}

	// ----

	enum remix_custom_rs
	{
		RS_42_TEXTURE_CATEGORY		= 42,
		RS_149_REMIX_MODIFIER		= 149,
		RS_150_TEXTURE_HASH			= 150,
		RS_169_EMISSIVE_SCALE		= 169,
		RS_177_DECAL_DIRT_CONTRAST	= 177,
		RS_210_ROUGHNESS_SCALAR		= 210,
		RS_211_ROUGHNESS_ZNORMAL	= 211,
		RS_212_ROUGHNESS_BLEND		= 212,
		RS_213_FREE					= 213,
		RS_214_FREE					= 214,
		RS_215_FREE					= 215,
		RS_216_FREE					= 216,
		RS_217_FREE					= 217,
		RS_218_FREE					= 218,
		RS_219_FREE					= 219,
		RS_220_FREE					= 220,
	};

	// Uses unused Renderstate 149 to set per drawcall modifiers
	// ~ req. runtime changes
	void renderer::set_remix_modifier(IDirect3DDevice9* dev, RemixModifier mod)
	{
		dc_ctx.save_rs(dev, RS_149_REMIX_MODIFIER);
		dc_ctx.modifiers.remix_modifier |= mod;

		dev->SetRenderState((D3DRENDERSTATETYPE)RS_149_REMIX_MODIFIER, static_cast<DWORD>(dc_ctx.modifiers.remix_modifier));
	}

	// Uses unused Renderstate 149 & 169 to tweak the emissive intensity of remix materials (legacy/opaque)
	// ~ currently req. runtime changes --> remixTempFloat01FromD3D
	/// @param no_overrides	will not override any previously set intensity if true
	void renderer::set_remix_emissive_intensity(IDirect3DDevice9* dev, float intensity, bool no_overrides)
	{
		const bool result = dc_ctx.save_rs(dev, RS_169_EMISSIVE_SCALE);
		if (!result && no_overrides) {
			return;
		}

		dc_ctx.info.shaderconst_emissive_intensity = intensity;
		set_remix_modifier(dev, RemixModifier::EmissiveScalar);
		set_remix_temp_float01(dev, intensity);
	}

	// RemixModifier::RoughnessScalar to tweak the roughness of remix materials (legacy/opaque)
	// - uses RS_210_ROUGHNESS_SCALAR
	// - uses RS_211_ROUGHNESS_ZNORMAL
	// - uses RS_212_ROUGHNESS_BLEND
	// ~ req. runtime changes
	/*void renderer::set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar, float max_z, float blend_width)
	{
		set_remix_modifier(dev, RemixModifier::RoughnessScalar);

		dc_ctx.save_rs(dev, RS_210_ROUGHNESS_SCALAR);
		dc_ctx.save_rs(dev, RS_211_ROUGHNESS_ZNORMAL);
		dc_ctx.save_rs(dev, RS_212_ROUGHNESS_BLEND);
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_210_ROUGHNESS_SCALAR, *reinterpret_cast<DWORD*>(&roughness_scalar));
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_211_ROUGHNESS_ZNORMAL, *reinterpret_cast<DWORD*>(&max_z));
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_212_ROUGHNESS_BLEND, *reinterpret_cast<DWORD*>(&blend_width));
	}*/

	//void renderer::set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar, float max_z, float blend_width)
	//{
	//	set_remix_modifier(dev, RemixModifier::RoughnessScalar);

	//	// Pack 3 parameters into one float: scalar + (max_z * 0.001) + (blend_width * 0.000001)
	//	// Clamp values: scalar (0-10), max_z (0-1), blend_width (0-1)
	//	const float clampedScalar = std::clamp(roughness_scalar, 0.0f, 10.0f);
	//	const float clampedMaxZ = std::clamp(max_z, 0.0f, 1.0f);
	//	const float clampedBlendWidth = std::clamp(blend_width, 0.0f, 1.0f);
	//	float packedValue = clampedScalar + (clampedMaxZ * 0.001f) + (clampedBlendWidth * 0.000001f);

	//	dc_ctx.save_rs(dev, RS_210_ROUGHNESS_SCALAR);
	//	dev->SetRenderState((D3DRENDERSTATETYPE)RS_210_ROUGHNESS_SCALAR, *reinterpret_cast<DWORD*>(&packedValue));
	//}

	//void renderer::set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar, float max_z, float blend_width, float param4)
	//{
	//	set_remix_modifier(dev, RemixModifier::RoughnessScalar);

	//	// Bit packing helper: encode a float value into n bits
	//	auto encodeRange = [](float v, int bits, float maxRange) -> uint16_t {
	//		int maxVal = (1 << bits) - 1;
	//		float normalized = std::clamp(v, 0.0f, maxRange) / maxRange;
	//		return uint16_t(std::round(normalized * maxVal));
	//		};

	//	// Pack wetnessParams1 (lower 16 bits): scalar(6) + max_z(5) + blend_width(5) = 16 bits
	//	uint16_t scalarBits = encodeRange(roughness_scalar, 6, 10.0f);      // 6 bits: 0-63 → 0-10
	//	uint16_t maxZBits = encodeRange(max_z, 5, 1.0f);                     // 5 bits: 0-31 → 0-1
	//	uint16_t blendWidthBits = encodeRange(blend_width, 5, 1.0f);         // 5 bits: 0-31 → 0-1
	//	uint16_t wetnessParams1 = (scalarBits << 10) | (maxZBits << 5) | blendWidthBits;

	//	// Pack wetnessParams2 (upper 16 bits): 4th parameter
	//	// Assuming param4 is 0-1 range, using all 16 bits for full precision
	//	// If you need a different range, adjust maxRange accordingly
	//	uint16_t wetnessParams2 = encodeRange(param4, 16, 1.0f); // 16 bits: 0-65535 → 0-1

	//	// Pack into DWORD: lower 16 bits = wetnessParams1, upper 16 bits = wetnessParams2
	//	uint32_t packedDword = (uint32_t(wetnessParams2) << 16) | uint32_t(wetnessParams1);

	//	dc_ctx.save_rs(dev, RS_210_ROUGHNESS_SCALAR);
	//	dev->SetRenderState((D3DRENDERSTATETYPE)RS_210_ROUGHNESS_SCALAR, packedDword);
	//}



	

	void renderer::set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar, float max_z, float blend_width, float param4, uint8_t flags)
	{
		set_remix_modifier(dev, RemixModifier::RoughnessScalar);

		// Bit packing helper: encode a float value into n bits
		auto encodeRange = [](float v, int bits, float maxRange) -> uint16_t {
			int maxVal = (1 << bits) - 1;
			float normalized = std::clamp(v, 0.0f, maxRange) / maxRange;
			return uint16_t(std::round(normalized * maxVal));
			};

		// Pack wetnessParams1 (lower 16 bits): scalar(6) + max_z(5) + blend_width(5) = 16 bits
		uint16_t scalarBits = encodeRange(roughness_scalar, 6, 10.0f);      // 6 bits: 0-63 → 0-10
		uint16_t maxZBits = encodeRange(max_z, 5, 1.0f);                     // 5 bits: 0-31 → 0-1
		uint16_t blendWidthBits = encodeRange(blend_width, 5, 1.0f);         // 5 bits: 0-31 → 0-1
		uint16_t wetnessParams1 = (scalarBits << 10) | (maxZBits << 5) | blendWidthBits;

		// Pack wetnessParams2 (upper 16 bits): lower 8 bits = param4 (0-10), upper 8 bits = flags
		// Lower 8 bits: param4 as 8-bit value (0-255 → 0-10 range, used as raindrop scale multiplier)
		// Don't scale on client side - send 0-10 range directly, shader will decode it
		uint16_t param4Bits = encodeRange(param4, 8, 10.0f); // 8 bits: 0-255 → 0-10
		// Upper 8 bits: flags (directly pack uint8_t flags into upper 8 bits)
		uint16_t flagsBits = uint16_t(flags) << 8; // Shift flags to upper 8 bits
		uint16_t wetnessParams2 = param4Bits | flagsBits;

		// Pack into DWORD: lower 16 bits = wetnessParams1, upper 16 bits = wetnessParams2
		uint32_t packedDword = (uint32_t(wetnessParams2) << 16) | uint32_t(wetnessParams1);

		dc_ctx.save_rs(dev, RS_210_ROUGHNESS_SCALAR);
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_210_ROUGHNESS_SCALAR, packedDword);
	}


	// ---

	// uses unused Renderstate 169 to pass per drawcall data
	// - used by emissive scalar mod
	// ~ req. runtime changes --> remixTempFloat01FromD3D
	void renderer::set_remix_temp_float01(IDirect3DDevice9* dev, float value)
	{
		dc_ctx.save_rs(dev, RS_169_EMISSIVE_SCALE);
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_169_EMISSIVE_SCALE, *reinterpret_cast<DWORD*>(&value));
	}

	
	// ---

	// Uses unused Renderstate 177 to pass per drawcall data
	// - used by decal dirt (contrast)
	// ~ req. runtime changes --> remixTempFloat02FromD3D
	void renderer::set_remix_temp_float02(IDirect3DDevice9* dev, float value)
	{
		dc_ctx.save_rs(dev, RS_177_DECAL_DIRT_CONTRAST);
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_177_DECAL_DIRT_CONTRAST, *reinterpret_cast<DWORD*>(&value));
	}

	// ---

	// Uses unused Renderstate 42 to set remix texture categories
	// ~ req. runtime changes
	void renderer::set_remix_texture_categories(IDirect3DDevice9* dev, const InstanceCategories& cat)
	{
		dc_ctx.save_rs(dev, RS_42_TEXTURE_CATEGORY);
		dc_ctx.modifiers.remix_instance_categories |= cat;
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_42_TEXTURE_CATEGORY, static_cast<DWORD>(dc_ctx.modifiers.remix_instance_categories));
	}

	// Uses unused Renderstate 150 to set custom remix hash
	// ~ req. runtime changes
	void renderer::set_remix_texture_hash(IDirect3DDevice9* dev, const std::uint32_t& hash)
	{
		dc_ctx.save_rs(dev, RS_150_TEXTURE_HASH);
		dev->SetRenderState((D3DRENDERSTATETYPE)RS_150_TEXTURE_HASH, hash);
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

	void handle_vehicle_roughness_when_wet(IDirect3DDevice9* dev, [[maybe_unused]] const drawcall_mod_context& ctx)
	{
		const auto gs = game_settings::get();

		uint8_t rain_flags = renderer::WETNESS_FLAG_NONE;
		if (gs->timecycle_wetness_vehicle_raindrop_enable.get_as<bool>())
		{
			if (*game::weather_type_prev == game::eWeatherType::WEATHER_RAIN || *game::weather_type_new == game::eWeatherType::WEATHER_LIGHTNING) {
				rain_flags |= renderer::WETNESS_FLAG_ENABLE_EXP_RAINSDROPS | renderer::WETNESS_FLAG_USE_TEXTURE_COORDINATES;
			}
		}

		renderer::set_remix_roughness_scalar(dev,
			gs->timecycle_wetness_vehicle_scalar.get_as<float>(),
			gs->timecycle_wetness_vehicle_z_normal.get_as<float>(),
			gs->timecycle_wetness_vehicle_blending.get_as<float>(),
			gs->timecycle_wetness_vehicle_raindrop_scalar.get_as<float>(),
			rain_flags);
	}

	void handle_vehicle_dirt_roughness_when_wet(IDirect3DDevice9* dev, [[maybe_unused]] const drawcall_mod_context& ctx)
	{
		const auto gs = game_settings::get();
		renderer::set_remix_roughness_scalar(dev,
			gs->timecycle_wetness_vehicle_dirt_roughness_scalar.get_as<float>(),
			gs->timecycle_wetness_vehicle_dirt_z_normal.get_as<float>(),
			gs->timecycle_wetness_vehicle_dirt_blending.get_as<float>());
	}

	void handle_vehicle_dirt_roughness_when_dry(IDirect3DDevice9* dev, const drawcall_mod_context& ctx)
	{
		const auto gs = game_settings::get();

		// not wet: slightly scale roughness wi* imgui::get()->m_debug_vector2.xth dirt
		const float adjusted = powf(ctx.info.ps_const_73_veh_dirt.x, gs->vehicle_dirt_expo.get_as<float>());

		renderer::set_remix_roughness_scalar(dev,
			0.0f - adjusted,
			gs->vehicle_dirt_roughness_z_normal.get_as<float>(),
			gs->vehicle_dirt_roughness_blending.get_as<float>());
	}

	void handle_vehicle_dirt_decal(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		const auto gs = game_settings::get();

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
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

		ctx.save_tss(dev, D3DTSS_ALPHAOP);
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

		ctx.save_tss(dev, D3DTSS_ALPHAARG1);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		ctx.save_tss(dev, D3DTSS_ALPHAARG2);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		ctx.save_rs(dev, D3DRS_TEXTUREFACTOR);

		float dirtscale = powf(ctx.info.ps_const_73_veh_dirt.x, gs->vehicle_dirt_expo.get_as<float>());
		if (ctx.modifiers.is_vehicle_wet) {
			dirtscale *= gs->timecycle_wetness_vehicle_dirt_intensity_scalar.get_as<float>();
		}

		if (gs->vehicle_dirt_custom_color_enabled.get_as<bool>())
		{
			const auto& col = gs->vehicle_dirt_custom_color.get_as<Vector*>();
			dev->SetRenderState(D3DRS_TEXTUREFACTOR,
				D3DCOLOR_COLORVALUE(
					col->x,
					col->y,
					col->z,
					dirtscale));
		}
		else
		{
			dev->SetRenderState(D3DRS_TEXTUREFACTOR,
				D3DCOLOR_COLORVALUE(
					ctx.info.ps_const_74_veh_dirt.x,
					ctx.info.ps_const_74_veh_dirt.y,
					ctx.info.ps_const_74_veh_dirt.z,
					dirtscale));
		}

		ctx.save_tss(dev, D3DTSS_TEXCOORDINDEX);
		dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 1);

		if (ctx.modifiers.is_vehicle_wet)
		{
			// dirt roughness when wet
			handle_vehicle_dirt_roughness_when_wet(dev, ctx);
		}
		else
		{
			// not wet: slightly scale roughness with dirt
			handle_vehicle_dirt_roughness_when_dry(dev, ctx);
		}

		renderer::set_remix_modifier(dev, RemixModifier::VehicleDecalDirt);
		renderer::set_remix_texture_categories(dev, InstanceCategories::DecalStatic);
	}
	

	// ----

	bool debug_ignore_shader_logic(const std::string_view& shader)
	{
		const auto& im = imgui::get();
		if (im->m_dbg_enable_ignore_shader_logic)
		{
			if (im->m_dbg_ignore_all) {
				return true;
			}

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
			if (im->m_dbg_ignore_gta_terrain_va_2lyr && shader.ends_with("gta_terrain_va_2lyr.fxc")) return true;
			if (im->m_dbg_ignore_gta_terrain_va_3lyr && shader.ends_with("gta_terrain_va_3lyr.fxc")) return true;
			if (im->m_dbg_ignore_gta_terrain_va_4lyr && shader.ends_with("gta_terrain_va_4lyr.fxc")) return true;
			if (im->m_dbg_ignore_gta_trees && shader.ends_with("gta_trees.fxc")) return true;
			if (im->m_dbg_ignore_gta_trees_extended && shader.ends_with("gta_trees_extended.fxc")) return true;
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

	drawcall_mod_context& setup_context(IDirect3DDevice9* dev)
	{
		auto& ctx = renderer::dc_ctx;

		if (ctx.info.is_dirty) {
			ctx.reset_context();
		}

		ctx.info.device_ptr = dev;

		dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &ctx.info.rs_alphablendenable);
		dev->GetRenderState(D3DRS_BLENDOP, &ctx.info.rs_blendop);
		dev->GetRenderState(D3DRS_SRCBLEND, &ctx.info.rs_srcblend);
		dev->GetRenderState(D3DRS_DESTBLEND, &ctx.info.rs_destblend);
		dev->GetTextureStageState(0, D3DTSS_ALPHAOP, &ctx.info.tss_alphaop);
		dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &ctx.info.tss_alphaarg1);
		dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &ctx.info.tss_alphaarg2);

		return ctx;
	}

	// every mesh goes through here (besides in-game ui and particles)
	void SetupVertexShaderAndConstants(game::vs_info_s* info, game::vs_data_s* data, game::shader_info_sub_s* constant_data_struct, game::shader_data_sub_s* sampler_data)
	{
		static auto im = imgui::get();
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

		const auto& pidx = ctx.info.preset_index;

		// no longer needed because of patch @ game::cond_jmp_addr__skip_deferred_light_rendering02
		/*if (ctx.info.shader_name.ends_with("deferred_lighting.fxc")) {
			ctx.modifiers.do_not_render = true;
		}*/

		if (info->num_vs_constants > 0)
		{
			bool is_gta_atmoscatt_clouds_shader = false;
			bool is_lightsemissive_shader = false;
			//bool is_gta_rmptfx_litsprite_shader = false;
			//bool modified_world_matrix = false;

			if (g_is_sky_rendering)
			{
				//if (ctx.info.shader_name.ends_with("_clouds.fxc")) {
					is_gta_atmoscatt_clouds_shader = true;
				//}
			}
			else if (pidx == GTA_VEHICLE_LIGHTSEMISSIVE /*ctx.info.shader_name.ends_with("lightsemissive.fxc")*/) {
				is_lightsemissive_shader = true;
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
						else*/ if ((register_num == 69u || register_num == 70u) && ctx.info.is_gta_rmptfx_litsprite_shader)
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
		static auto im = imgui::get();
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

		if (g_is_rendering_fx_instance || g_is_rendering_fx) {
			ctx.modifiers.is_fx = true;
		}

		if (info->num_ps_constants > 0)
		{
			//bool is_gta_rmptfx_litsprite_shader = false;
			bool is_gta_vehicle_shader = false;
			bool is_emissive_shader = false;
			//bool is_projtex_shader = false;
			//bool is_gta_im_shader = false;
			//bool modified_projtex_shader = false;

			const auto& pidx = ctx.info.preset_index;

			bool is_vehicle_paint = pidx == GTA_VEHICLE_PAINT1 || pidx == GTA_VEHICLE_PAINT2 || pidx == GTA_VEHICLE_PAINT3;

			// shader_name.contains("gta_vehicle_")
			if (   pidx == GTA_VEHICLE_BADGES || pidx == GTA_VEHICLE_INTERIOR || pidx == GTA_VEHICLE_INTERIOR2 || pidx == GTA_VEHICLE_LIGHTS
				|| pidx == GTA_VEHICLE_MESH || pidx == GTA_VEHICLE_NOSPLASH || pidx == GTA_VEHICLE_PAINT1 || pidx == GTA_VEHICLE_PAINT2 || pidx == GTA_VEHICLE_PAINT3
				|| pidx == GTA_VEHICLE_SHUTS || pidx == GTA_VEHICLE_TIRE || pidx == GTA_VEHICLE_VEHGLASS) {
				is_gta_vehicle_shader = true; 
			}

			else if (  pidx == GTA_EMISSIVE || pidx == GTA_EMISSIVENIGHT || pidx == GTA_EMISSIVESTRONG ||
					   pidx == GTA_GLASS_EMISSIVE || pidx == GTA_GLASS_EMISSIVENIGHT
					|| pidx == GTA_EMISSIVENIGHT_ALPHA || pidx == GTA_EMISSIVESTRONG_ALPHA || pidx == GTA_EMISSIVE_ALPHA
					|| pidx == GTA_GLASS_EMISSIVE || pidx == GTA_GLASS_EMISSIVENIGHT)/*if (ctx.info.shader_name.contains("emissive"))*/ {
				is_emissive_shader = true;
			}

			else if ((g_is_rendering_fx_instance || g_is_rendering_fx) 
				&& !ctx.info.checked_for_gta_rmptfx_litsprite_shader)
			{
				ctx.info.checked_for_gta_rmptfx_litsprite_shader = true;

				if (im->m_stats._gta_rmptfx_litsprite_shader_name_checks.track_check() && ctx.info.shader_name.ends_with("gta_rmptfx_litsprite.fxc"))
				{
					im->m_stats._gta_rmptfx_litsprite_shader_name_checks.track_check(true);
					ctx.info.is_gta_rmptfx_litsprite_shader = true;
				}
			}

#if 0
			else if (im->m_stats._projtex_shader_name_checks.track() && ctx.info.shader_name.ends_with("projtex.fxc")) 
			{
				im->m_stats._projtex_shader_name_checks.track(imgui::StatModeSucess);
				is_projtex_shader = true;
			}


			else if (im->m_stats._gta_im_shader_name_checks.track() && ctx.info.shader_name.ends_with("_im.fxc")) 
			{
				im->m_stats._gta_im_shader_name_checks.track(imgui::StatModeSucess);
				is_gta_im_shader = true;
			}
#endif

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
							if (register_num == 8u && pidx == GTA_VEHICLE_LIGHTSEMISSIVE) {
								on_constant_switchOn(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].bool_ptr);
							}
						}

						//if (g_is_sky_rendering || is_gta_im_shader)
							game_device->SetPixelShaderConstantB(register_num, constant_data_struct->constants[dataPoolIndex].bool_ptr, 1u);
					}
					else
					{
						if (g_is_rendering_vehicle)
						{
							if (register_num == 66u && is_gta_vehicle_shader /*&& !ctx.info.shader_name.ends_with("sive.fxc")*/) {
								on_constant_matDiffuseColor(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].float_arr); 
							}
							else if (register_num == 72u && pidx == GTA_VEHICLE_LIGHTSEMISSIVE)
							{
								const float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->vehicle_lights_emissive_scalar.get_as<float>();
								{
									on_constant_emissiveMultiplier(shared::globals::d3d_device, intensity); 
									//renderer::set_remix_modifier(shared::globals::d3d_device, RemixModifier::RemoveVertexColorKeepAlpha);
									//set_remix_modifier(shared::globals::d3d_device, ctx, RemixModifier::EnableVertexColor); // required when 'isVertexColorBakedLighting' is turned on

									/*if (gs->vehicle_lights_dual_render_proxy_texture.get_as<bool>())
									{
										ctx.modifiers.dual_render_with_specified_texture = true;
										ctx.modifiers.dual_render_texture = tex_addons::veh_light_ems_glass;
										ctx.modifiers.dual_render_reset_remix_modifiers = true;
										ctx.modifiers.dual_render_mode_blend_diffuse = true;
									}*/
								}
							}
							else if (register_num == 72u && is_vehicle_paint)
							{
								ctx.info.ps_const_72_veh_dirt.x = constant_data_struct->constants[dataPoolIndex].float_arr[0];
								ctx.info.ps_const_72_veh_dirt.y = constant_data_struct->constants[dataPoolIndex].float_arr[1];
								ctx.info.ps_const_72_veh_dirt.z = constant_data_struct->constants[dataPoolIndex].float_arr[2];
								ctx.info.ps_const_72_veh_dirt.w = constant_data_struct->constants[dataPoolIndex].float_arr[3];
							}
							else if (register_num == 73u && is_vehicle_paint)
							{
								// dirt level
								ctx.info.ps_const_73_veh_dirt.x = constant_data_struct->constants[dataPoolIndex].float_arr[0];
								ctx.info.ps_const_73_veh_dirt.y = constant_data_struct->constants[dataPoolIndex].float_arr[1];
								ctx.info.ps_const_73_veh_dirt.z = constant_data_struct->constants[dataPoolIndex].float_arr[2];
								ctx.info.ps_const_73_veh_dirt.w = constant_data_struct->constants[dataPoolIndex].float_arr[3];
							}
							else if (register_num == 74u && is_vehicle_paint)
							{
								// dirt color?
								// variant 5 ...
								ctx.info.ps_const_74_veh_dirt.x = constant_data_struct->constants[dataPoolIndex].float_arr[0];
								ctx.info.ps_const_74_veh_dirt.y = constant_data_struct->constants[dataPoolIndex].float_arr[1];
								ctx.info.ps_const_74_veh_dirt.z = constant_data_struct->constants[dataPoolIndex].float_arr[2];
								ctx.info.ps_const_74_veh_dirt.w = constant_data_struct->constants[dataPoolIndex].float_arr[3];
							}
						}

						else if (register_num == 66u && pidx == GTA_RADAR) {
							on_constant_diffuseCol(shared::globals::d3d_device, constant_data_struct->constants[dataPoolIndex].float_arr);
						}

#if 0
						else if (is_projtex_shader && register_num == 66u)
						{
							//modified_projtex_shader = true;

							/*D3DXMATRIX mtx;
							game_device->GetVertexShaderConstantF(0, mtx, 4);

							mtx.m[0][0] *= dat[0] + imgui::get()->m_debug_vector2.x;
							mtx.m[1][1] *= dat[0] + imgui::get()->m_debug_vector2.y;
							mtx.m[2][2] *= dat[0] + imgui::get()->m_debug_vector2.z;
							mtx.m[3][3] *= dat[0] + imgui::get()->m_debug_vector2.z;

							game_device->SetVertexShaderConstantF(0, mtx, 4);*/
						}
#endif

						else
						{
							if (register_num == 66u && is_emissive_shader)
							{
								ctx.info.shaderconst_uses_emissive_multiplier = true;

								switch (pidx)
								{
								case GTA_EMISSIVENIGHT_ALPHA:
								case GTA_EMISSIVENIGHT:

									if (*game::m_game_clock_hours <= 6 || *game::m_game_clock_hours >= 19) {
										renderer::set_remix_emissive_intensity(shared::globals::d3d_device, *constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_night_surfaces_emissive_scalar.get_as<float>());
									}
									else {
										renderer::set_remix_emissive_intensity(shared::globals::d3d_device, 0.0f);
									}

									//renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
									//	*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_night_surfaces_emissive_scalar.get_as<float>());

									break;

								default:
								//case GTA_EMISSIVE:
								//case GTA_GLASS_EMISSIVE:
								//case GTA_GLASS_EMISSIVENIGHT:
									renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
										*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_surfaces_emissive_scalar.get_as<float>());
									break;

								case GTA_EMISSIVESTRONG_ALPHA:
								case GTA_EMISSIVESTRONG:
									renderer::set_remix_emissive_intensity(shared::globals::d3d_device,
										*constant_data_struct->constants[dataPoolIndex].float_arr * gs->emissive_strong_surfaces_emissive_scalar.get_as<float>()/*, true*/);
									break;
								}
							}
							else if (register_num == 51u && (pidx == GTA_EMISSIVENIGHT || pidx == GTA_EMISSIVENIGHT_ALPHA))
							{
								ctx.info.shaderconst_uses_emissive_multiplier = true;

								Vector4D color = constant_data_struct->constants[dataPoolIndex].float_arr;

								ctx.info.device_ptr->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(color.x, color.y, color.z, color.w));
								ctx.info.device_ptr->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
								ctx.info.device_ptr->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
								ctx.info.device_ptr->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
							}
						}

						if (g_is_rendering_static)
						{
							if (gs->decal_dirt_shader_usage.get_as<bool>() && register_num == 66u && pidx == GTA_DECAL_DIRT)
							{
								float intensity = *constant_data_struct->constants[dataPoolIndex].float_arr * gs->decal_dirt_shader_scalar.get_as<float>();
								renderer::set_remix_temp_float01(shared::globals::d3d_device, intensity);
								renderer::set_remix_temp_float02(shared::globals::d3d_device, gs->decal_dirt_shader_contrast.get_as<float>());
							}
						}

						//if (g_is_sky_rendering || is_gta_im_shader)
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

					bool is_livery = false;
					bool is_dirt = false;
					bool is_dirt_s2 = false;

					if (pidx == GTA_VEHICLE_PAINT1 || pidx == GTA_VEHICLE_PAINT2) { 
						is_dirt = arg3 == 6 && arg1 == 1;
					}

#if DEBUG
					if (pidx == GTA_VEHICLE_PAINT1 || pidx == GTA_VEHICLE_PAINT2 || pidx == GTA_VEHICLE_PAINT3)
					{
						if (arg3 == 6 && arg1 == 0) 
						{
							int y = 0;
						}

						if (arg3 == 6 && arg1 == 1)
						{
							int y = 0;
						}

						if (arg3 == 6 && arg1 == 2)
						{
							int y = 0;
						}

						if (arg3 == 6 && arg1 == 3)
						{
							int y = 0;
						}
					}
#endif

					if (pidx == GTA_VEHICLE_PAINT3) 
					{
						is_livery = arg3 == 6 && arg1 == 1;
						is_dirt_s2 = arg3 == 6 && arg1 == 2;
					}

					if (gs->load_colormaps_only.get_as<bool>() && !g_is_sky_rendering)
					{
						// everything that is not 0 is not a colormap (I hope)
						if (arg1 && !is_livery && !is_dirt && !is_dirt_s2)
						{
							++i;
							continue;
						}
					}

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

					if (is_livery && gs->vehicle_livery_enabled.get_as<bool>())
					{
						if (arg3 == 6 && arg1 == 1)
						{
							IDirect3DBaseTexture9* tex1ptr = nullptr;
							if (game_device->GetTexture(1, &tex1ptr); tex1ptr)
							{
								ctx.modifiers.dual_render = true;
								ctx.modifiers.dual_render_texture = tex1ptr;
								ctx.modifiers.dual_render_mode_vehicle_livery = true;
							}
						}
					}

					if (gs->vehicle_dirt_enabled.get_as<bool>())
					{
						if (is_dirt && arg3 == 6 && arg1 == 1)
						{
							IDirect3DBaseTexture9* tex1ptr = nullptr;
							if (game_device->GetTexture(1, &tex1ptr); tex1ptr)
							{
								ctx.modifiers.dual_render = true;
								ctx.modifiers.dual_render_texture = tex1ptr;
								ctx.modifiers.dual_render_mode_vehicle_dirt = true;
							}
						}

						if (is_dirt_s2 && arg3 == 6 && arg1 == 2)
						{
							IDirect3DBaseTexture9* tex2ptr = nullptr;
							if (game_device->GetTexture(2, &tex2ptr); tex2ptr)
							{
								ctx.modifiers.tri_render = true;
								ctx.modifiers.tri_render_texture = tex2ptr;
							}
						}
					}
				}

				++i;
			} while (i < info->num_ps_constants);
		}
	}

	// -----

	// 1 if "special" SetupVsPsPass call is used (used by bink)
	int g_is_special_vspspass = 0;

	__declspec (naked) void handle_special_setupvspspass_stub()
	{
		__asm
		{
			mov		g_is_special_vspspass, 1;
			call	game::hk_addr__SetupVsPsPass_hk;
			mov		g_is_special_vspspass, 0;
			jmp		game::retn_addr__special_SetupVsPsPass_handling;
		}
	}

	DETOUR_TYPEDEF(SetupVsPsPass, void, __thiscall, game::current_pass_s* pass, game::vs_data_s** vs_data_struct, game::ps_data_s** ps_data_struct, game::shader_info_sub_s* data, game::shader_data_sub_s* sampler_data);

	void __fastcall SetupVsPsPass_hk(game::current_pass_s* pass, [[maybe_unused]] int fastcall, game::vs_data_s** vs_data_struct, game::ps_data_s** ps_data_struct, game::shader_info_sub_s* data, game::shader_data_sub_s* sampler_data)
	{
		// in case we need to compare the rewritten one to the og version
		//return SetupVsPsPass_og(pass, vs_data_struct, ps_data_struct, data, sampler_data);

		// the "base" object is not a function arg so we get it by subtracting 0x14 from the data arg
		//auto* parent_addr = reinterpret_cast<std::byte*>(data) - 0x14;
		//const auto* shader_obj = reinterpret_cast<game::shaderfx_base*>(parent_addr);

		const auto game_device = game::get_d3d_device();
		int n_renderstates = pass->num_renderstates;

		auto& ctx = renderer::get()->dc_ctx;
		ctx.info.shader_name = data->data->sub.shader_name;

		// special case because the data around the function args seems to be drastically different for bink passes?
		// invalid data preset index and name locations which can result in a crash in mission "Lure" (Francis)
		if (g_is_special_vspspass && ctx.info.shader_name.ends_with("bink.fxc"))
		{
			ctx.info.is_bink = true;
			ctx.info.preset_index = -1;
			return SetupVsPsPass_og(pass, vs_data_struct, ps_data_struct, data, sampler_data);
		}

		// we actually read past the substruct into the preset index here (part of grmShader?)
		ctx.info.preset_index = data->preset_index;

		if (ctx.info.preset_index >= 0 && data->preset_name) {
			ctx.info.preset_name = data->preset_name;
		}

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
		// needed for comp mod initialization 
		if (!g_rendered_first_primitive) {
			g_rendered_first_primitive = true;
		}

		if (!is_initialized() || shared::globals::imgui_is_rendering) {
			return dev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
		}

		static auto im = imgui::get();
		static auto gs = game_settings::get();

		im->m_stats._drawcall_prim_incl_ignored.track_single();

		auto& ctx = setup_context(dev);
		// info.shader_name can be empty here -> TODO

		bool render_with_ff = false;
		bool disable_vertex_colors = false;

		// fixes the skylight
		dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);

		if (g_is_rendering_mirror)
		{
			set_remix_texture_hash(dev, shared::utils::string_hash32("mirror"));
			ctx.save_texture(dev, 0);
			dev->SetTexture(0, tex_addons::mirror);
		}
		else if (im->m_dbg_do_not_render_fx && (g_is_rendering_fx || g_is_rendering_fx_instance)) {
			ctx.modifiers.do_not_render = true;
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

				// ~ 2 checks per frame
				if (ctx.info.shader_name.ends_with("im.fxc")) {
					dev->SetTexture(0, tex_addons::sky);
				}
			}
			
			// rendering water via FF is fine and safes a ton of performance
			else if (g_is_water_rendering)
			{
				dev->SetTransform(D3DTS_WORLD, &viewport->wp->world);
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
				render_with_ff = true;

				// override texture hash so that it never changes
				if (gs->override_water_texture_hash.get_as<bool>()) {
					set_remix_texture_hash(dev, shared::utils::string_hash32("water"));
				}

#if 0			// test - water UV's are always 0 - could be fixed in 0xAD8960 but no impr.
				{
					D3DXMATRIX texMatrix;
					D3DXMatrixIdentity(&texMatrix);  // Start with identity
					D3DXMatrixScaling(&texMatrix, imgui::get()->m_debug_vector2.x, imgui::get()->m_debug_vector2.y, 1.0f);
					ctx.set_texture_transform(shared::globals::d3d_device, &texMatrix);

					ctx.save_tss(dev, D3DTSS_TEXTURETRANSFORMFLAGS);
					dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

					ctx.save_ss(dev, D3DSAMP_ADDRESSU);
					ctx.save_ss(dev, D3DSAMP_ADDRESSV);
					dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
					dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

					ctx.save_texture(dev, 0);
					dev->SetTexture(0, tex_addons::veh_light_ems_glass);
				}
#endif

				disable_vertex_colors = true;

				if (im->m_dbg_do_not_render_water) {
					ctx.modifiers.do_not_render = true;
				}
			}

			else if ((g_is_rendering_fx_instance || g_is_rendering_fx)
				&& !ctx.info.checked_for_gta_rmptfx_litsprite_shader)
			{
				ctx.info.checked_for_gta_rmptfx_litsprite_shader = true;

				if (im->m_stats._gta_rmptfx_litsprite_shader_name_checks.track_check() && ctx.info.shader_name.ends_with("gta_rmptfx_litsprite.fxc"))
				{
					im->m_stats._gta_rmptfx_litsprite_shader_name_checks.track_check(true);
					ctx.info.is_gta_rmptfx_litsprite_shader = true;
				}
			}

			// --

			if (ctx.info.is_gta_rmptfx_litsprite_shader)
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

		if (disable_vertex_colors) {
			set_remix_texture_categories(dev, InstanceCategories::IgnoreBakedLighting);
		}

		// TODO
		if (ctx.info.is_bink)
		{
			ctx.modifiers.do_not_render = true; // does not work anyway

			/*ctx.save_texture(dev, 0);

			IDirect3DBaseTexture9* tex1 = nullptr;
			dev->GetTexture(0, &tex1);

			if (tex1)
			{
				dev->SetTexture(0, tex1);
			}*/
		}

		// ---------
		// draw

		auto hr = S_OK;

		if (!render_with_ff && !shared::globals::imgui_is_rendering && im->m_dbg_do_not_render_prims_with_vertexshader) {
			return hr;
		}

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render) 
		{
			hr = dev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
			im->m_stats._drawcall_prim.track_single();

			if (!render_with_ff) {
				im->m_stats._drawcall_using_vs.track_single();
			}
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
		if (!is_initialized() || shared::globals::imgui_is_rendering) {
			return dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		}

		auto& ctx = setup_context(dev);
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		bool render_with_ff = false;

		if (!shared::globals::imgui_is_rendering)
		{
			im->m_stats._drawcall_indexed_prim_incl_ignored.track_single();

			if (ctx.modifiers.do_not_render) 
			{
				if (!g_is_instance_rendering) 
				{
					if (!im->m_dbg_do_not_restore_drawcall_context_on_early_out) {
						ctx.restore_all(dev);
					}

					ctx.reset_context();
				}
				
				return S_OK;
			}

			if (im->m_dbg_skip_draw_indexed_checks)
			{
				if (!g_is_instance_rendering)
				{
					if (!im->m_dbg_do_not_restore_drawcall_context_on_early_out) {
						ctx.restore_all(dev);
					}

					ctx.reset_context();
				}
				
				return S_OK;

				/*const auto viewport = game::pCurrentViewport;
				if (viewport && viewport->wp)
				{
					dev->SetTransform(D3DTS_VIEW, &viewport->wp->view);
					dev->SetTransform(D3DTS_PROJECTION, &viewport->wp->proj);
				}

				return dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);*/
			}

			// shared::utils::lookat_vertex_decl(dev);

			render_with_ff = g_is_rendering_static;
			//bool allow_vertex_colors = false;

#ifdef LOG_SHADERPRESETS
			auto it = im->preset_list.find(ctx.info.preset_index);
			if (it == im->preset_list.end() || it->second != ctx.info.preset_name) {
				im->preset_list[ctx.info.preset_index] = ctx.info.preset_name;
			}
#endif

			const auto viewport = game::pCurrentViewport;

			// make VS use the correct matrix (if Use World Transforms is enabled in remix)
			dev->SetTransform(D3DTS_WORLD, game::pCurrentWorldTransform);

			if (viewport && viewport->wp)
			{
				dev->SetTransform(D3DTS_VIEW, &viewport->wp->view);
				dev->SetTransform(D3DTS_PROJECTION, &viewport->wp->proj);
			}

			// true if none of the below checks were true
			bool not_part_of_large_if_check = false;

			const auto& pidx = ctx.info.preset_index;

			if ( (  pidx == GTA_ALPHA || pidx == GTA_DECAL || pidx == GTA_DECAL_GLUE || pidx == GTA_NORMAL_ALPHA || pidx == GTA_NORMAL_DECAL 
				 || pidx == GTA_NORMAL_SPEC_ALPHA || pidx == GTA_NORMAL_SPEC_DECAL || pidx == GTA_SPEC_ALPHA || pidx == GTA_SPEC_DECAL ) 
				&& ctx.info.rs_alphablendenable && ctx.info.tss_alphaarg2 == D3DTA_CURRENT && ctx.info.tss_alphaop == D3DTOP_SELECTARG1)
			{
				if (im->m_dbg_visualize_decal_renderstates) 
				{
					imgui::visualized_decal_rs_s vis = {};
					vis.pos.x = game::pCurrentWorldTransform->m[3][0];
					vis.pos.y = game::pCurrentWorldTransform->m[3][1];
					vis.pos.z = game::pCurrentWorldTransform->m[3][2];

					vis.rs_alpha_blending = ctx.info.rs_alphablendenable;

					switch (ctx.info.rs_blendop)
					{
					case D3DBLENDOP_ADD:
						vis.rs_blendop = "D3DBLENDOP_ADD";
						break;
					case D3DBLENDOP_SUBTRACT:
						vis.rs_blendop = "D3DBLENDOP_SUBTRACT";
						break;
					case D3DBLENDOP_REVSUBTRACT:
						vis.rs_blendop = "D3DBLENDOP_REVSUBTRACT";
						break;
					case D3DBLENDOP_MIN:
						vis.rs_blendop = "D3DBLENDOP_MIN";
						break;
					case D3DBLENDOP_MAX:
						vis.rs_blendop = "D3DBLENDOP_MAX";
						break;
					default:
						vis.rs_blendop = "INVALID";
						break;
					}

					switch (ctx.info.rs_srcblend)
					{
					case D3DBLEND_ZERO:
						vis.rs_srcblend = "D3DBLEND_ZERO";
						break;
					case D3DBLEND_ONE:
						vis.rs_srcblend = "D3DBLEND_ONE";
						break;
					case D3DBLEND_SRCCOLOR:
						vis.rs_srcblend = "D3DBLEND_SRCCOLOR";
						break;
					case D3DBLEND_INVSRCCOLOR:
						vis.rs_srcblend = "D3DBLEND_INVSRCCOLOR";
						break;
					case D3DBLEND_SRCALPHA:
						vis.rs_srcblend = "D3DBLEND_SRCALPHA";
						break;
					case D3DBLEND_INVSRCALPHA:
						vis.rs_srcblend = "D3DBLEND_INVSRCALPHA";
						break;
					case D3DBLEND_DESTALPHA:
						vis.rs_srcblend = "D3DBLEND_DESTALPHA";
						break;
					case D3DBLEND_INVDESTALPHA:
						vis.rs_srcblend = "D3DBLEND_INVDESTALPHA";
						break;
					case D3DBLEND_DESTCOLOR:
						vis.rs_srcblend = "D3DBLEND_DESTCOLOR";
						break;
					case D3DBLEND_INVDESTCOLOR:
						vis.rs_srcblend = "D3DBLEND_INVDESTCOLOR";
						break;
					case D3DBLEND_SRCALPHASAT:
						vis.rs_srcblend = "D3DBLEND_SRCALPHASAT";
						break;
					case D3DBLEND_BOTHSRCALPHA:
						vis.rs_srcblend = "D3DBLEND_BOTHSRCALPHA";
						break;
					case D3DBLEND_BOTHINVSRCALPHA:
						vis.rs_srcblend = "D3DBLEND_BOTHINVSRCALPHA";
						break;
					case D3DBLEND_BLENDFACTOR:
						vis.rs_srcblend = "D3DBLEND_BLENDFACTOR";
						break;
					case D3DBLEND_INVBLENDFACTOR:
						vis.rs_srcblend = "D3DBLEND_INVBLENDFACTOR";
						break;
					default:
						vis.rs_srcblend = "INVALID";
						break;
					}

					switch (ctx.info.rs_destblend)
					{
					case D3DBLEND_ZERO:
						vis.rs_destblend = "D3DBLEND_ZERO";
						break;
					case D3DBLEND_ONE:
						vis.rs_destblend = "D3DBLEND_ONE";
						break;
					case D3DBLEND_SRCCOLOR:
						vis.rs_destblend = "D3DBLEND_SRCCOLOR";
						break;
					case D3DBLEND_INVSRCCOLOR:
						vis.rs_destblend = "D3DBLEND_INVSRCCOLOR";
						break;
					case D3DBLEND_SRCALPHA:
						vis.rs_destblend = "D3DBLEND_SRCALPHA";
						break;
					case D3DBLEND_INVSRCALPHA:
						vis.rs_destblend = "D3DBLEND_INVSRCALPHA";
						break;
					case D3DBLEND_DESTALPHA:
						vis.rs_destblend = "D3DBLEND_DESTALPHA";
						break;
					case D3DBLEND_INVDESTALPHA:
						vis.rs_destblend = "D3DBLEND_INVDESTALPHA";
						break;
					case D3DBLEND_DESTCOLOR:
						vis.rs_destblend = "D3DBLEND_DESTCOLOR";
						break;
					case D3DBLEND_INVDESTCOLOR:
						vis.rs_destblend = "D3DBLEND_INVDESTCOLOR";
						break;
					case D3DBLEND_SRCALPHASAT:
						vis.rs_destblend = "D3DBLEND_SRCALPHASAT";
						break;
					case D3DBLEND_BOTHSRCALPHA:
						vis.rs_destblend = "D3DBLEND_BOTHSRCALPHA";
						break;
					case D3DBLEND_BOTHINVSRCALPHA:
						vis.rs_destblend = "D3DBLEND_BOTHINVSRCALPHA";
						break;
					case D3DBLEND_BLENDFACTOR:
						vis.rs_destblend = "D3DBLEND_BLENDFACTOR";
						break;
					case D3DBLEND_INVBLENDFACTOR:
						vis.rs_destblend = "D3DBLEND_INVBLENDFACTOR";
						break;
					default:
						vis.rs_destblend = "INVALID";
						break;
					}

					switch (ctx.info.tss_alphaop)
					{
					case D3DTOP_DISABLE:
						vis.tss_alphaop = "D3DTOP_DISABLE";
						break;
					case D3DTOP_SELECTARG1:
						vis.tss_alphaop = "D3DTOP_SELECTARG1";
						break;
					case D3DTOP_SELECTARG2:
						vis.tss_alphaop = "D3DTOP_SELECTARG2";
						break;
					case D3DTOP_MODULATE:
						vis.tss_alphaop = "D3DTOP_MODULATE";
						break;
					case D3DTOP_MODULATE2X:
						vis.tss_alphaop = "D3DTOP_MODULATE2X";
						break;
					case D3DTOP_MODULATE4X:
						vis.tss_alphaop = "D3DTOP_MODULATE4X";
						break;
					case D3DTOP_ADD:
						vis.tss_alphaop = "D3DTOP_ADD";
						break;
					case D3DTOP_ADDSIGNED:
						vis.tss_alphaop = "D3DTOP_ADDSIGNED";
						break;
					case D3DTOP_ADDSIGNED2X:
						vis.tss_alphaop = "D3DTOP_ADDSIGNED2X";
						break;
					case D3DTOP_SUBTRACT:
						vis.tss_alphaop = "D3DTOP_SUBTRACT";
						break;
					case D3DTOP_ADDSMOOTH:
						vis.tss_alphaop = "D3DTOP_ADDSMOOTH";
						break;
					default:
						vis.tss_alphaop = "UNMAPPED or UNKOWN";
						break;
					}

					switch (ctx.info.tss_alphaarg1)
					{
					case 0x00000000:
						vis.tss_alphaarg1 = "D3DTA_DIFFUSE";
						break;
					case 0x00000001:
						vis.tss_alphaarg1 = "D3DTA_CURRENT";
						break;
					case 0x00000002:
						vis.tss_alphaarg1 = "D3DTA_TEXTURE";
						break;
					case 0x00000003:
						vis.tss_alphaarg1 = "D3DTA_TFACTOR";
						break;
					case 0x00000004:
						vis.tss_alphaarg1 = "D3DTA_SPECULAR";
						break;
					case 0x00000005:
						vis.tss_alphaarg1 = "D3DTA_TEMP";
						break;
					case 0x00000006:
						vis.tss_alphaarg1 = "D3DTA_CONSTANT";
						break;
					default:
						vis.tss_alphaarg1 = "UNMAPPED or UNKOWN";
						break;
					}

					switch (ctx.info.tss_alphaarg2)
					{
					case 0x00000000:
						vis.tss_alphaarg2 = "D3DTA_DIFFUSE";
						break;
					case 0x00000001:
						vis.tss_alphaarg2 = "D3DTA_CURRENT";
						break;
					case 0x00000002:
						vis.tss_alphaarg2 = "D3DTA_TEXTURE";
						break;
					case 0x00000003:
						vis.tss_alphaarg2 = "D3DTA_TFACTOR";
						break;
					case 0x00000004:
						vis.tss_alphaarg2 = "D3DTA_SPECULAR";
						break;
					case 0x00000005:
						vis.tss_alphaarg2 = "D3DTA_TEMP";
						break;
					case 0x00000006:
						vis.tss_alphaarg2 = "D3DTA_CONSTANT";
						break;
					default:
						vis.tss_alphaarg2 = "UNMAPPED or UNKOWN";
						break;
					}

					im->visualized_decal_renderstates.emplace_back(std::move(vis));
				}

				//allow_vertex_colors = true;
				ctx.save_tss(dev, D3DTSS_ALPHAOP);  
				ctx.save_tss(dev, D3DTSS_ALPHAARG2); 
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
				set_remix_texture_categories(dev, InstanceCategories::DecalStatic);
				set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha);
			}

			else if (pidx == GTA_VEHICLE_LIGHTSEMISSIVE) 
			{
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

				ctx.save_rs(dev, D3DRS_BLENDOP);
				dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

				ctx.save_rs(dev, D3DRS_SRCBLEND);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

				ctx.save_rs(dev, D3DRS_DESTBLEND);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

				ctx.save_tss(dev, D3DTSS_COLOROP);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

				ctx.save_tss(dev, D3DTSS_COLORARG1);
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_ALPHAOP);
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG1);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

				ctx.save_rs(dev, D3DRS_TEXTUREFACTOR);
				dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(0, 0, 0, 1.0f)); 

				// i swear this did something at one point?!
				//D3DXMATRIX mat = *game::pCurrentWorldTransform;
				//mat.m[0][0] *= 0.999f; //im->m_debug_vector2.y + 1.0f; //0.99f; 0.999f
				//mat.m[1][1] *= 0.999f; //im->m_debug_vector2.y + 1.0f;
				//mat.m[2][2] *= 0.999f; //im->m_debug_vector2.y + 1.0f; 
				//dev->SetTransform(D3DTS_WORLD, &mat);

				ctx.modifiers.dual_render = true;
				ctx.modifiers.dual_render_mode_emissive_offset = true;

				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreOpacityMicromap);
			}

			// check if trees should be rendered with shaders
			else if (pidx == GTA_TREES || ctx.modifiers.is_grass_foliage /*ctx.modifiers.is_tree_foliage*/)
			{
				if (im->m_dbg_do_not_render_tree_foliage) {
					ctx.modifiers.do_not_render = true;
				}

				if (!gs->fixed_function_trees.get_as<bool>()) {
					render_with_ff = false;
				}

				// enable alpha testing
				ctx.save_rs(dev, D3DRS_ALPHATESTENABLE);
				ctx.save_rs(dev, D3DRS_ALPHAFUNC);
				ctx.save_rs(dev, D3DRS_ALPHAREF);

				dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
				dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

				float alpha_setting = 0.7f;
				if (pidx == GTA_TREES /*ctx.modifiers.is_tree_foliage*/) {
					alpha_setting = gs->tree_foliage_alpha_cutout_value.get_as<float>();
				}
				else if (ctx.modifiers.is_grass_foliage) {
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

			// handle gta_decal_dirt shader
			else if (pidx == GTA_DECAL_DIRT /*ctx.info.shader_name.ends_with("_dirt.fxc")*/)
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

			else if (pidx == GTA_HAIR_SORTED_ALPHA_EXPENSIVE)
			{
				if (im->m_dbg_tag_exp_hair_as_index != -1) {
					set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_exp_hair_as_index));
				}

				if (gs->npc_expensive_hair_alpha_testing.get_as<bool>())
				{
					// enable alpha testing
					ctx.save_rs(dev, D3DRS_ALPHATESTENABLE);
					ctx.save_rs(dev, D3DRS_ALPHAFUNC);
					ctx.save_rs(dev, D3DRS_ALPHAREF);

					dev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
					dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

					const float alpha_ref_val = std::clamp(gs->npc_expensive_hair_alpha_cutout_value.get_as<float>(), 0.0f, 1.0f);
					DWORD alpha_ref_int = static_cast<DWORD>(alpha_ref_val * 255.0f); // 0-255 range
					dev->SetRenderState(D3DRS_ALPHAREF, alpha_ref_int);

					// disable alpha blending (since we're using alpha testing)
					ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
					dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

					// enable Z-writing and Z-testing
					ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
					ctx.save_rs(dev, D3DRS_ZENABLE);
					ctx.save_rs(dev, D3DRS_ZFUNC);

					dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); // TODO
					dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
					dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

					// disable culling for two-sided foliage
					ctx.save_rs(dev, D3DRS_CULLMODE);
					dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

					// texture stage states for alpha testing
					/*ctx.save_tss(dev, D3DTSS_ALPHAOP);
					ctx.save_tss(dev, D3DTSS_ALPHAARG1);

					dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
					dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);*/

					ctx.save_tss(dev, D3DTSS_ALPHAOP);
					dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

					ctx.save_tss(dev, D3DTSS_ALPHAARG1);
					dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

					ctx.save_tss(dev, D3DTSS_ALPHAARG2);
					dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

					//ctx.modifiers.allow_vertex_colors = true;
					set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha);
					set_remix_texture_categories(dev, InstanceCategories::DisableBackfaceCulling);
				}
			}

			else if (pidx == GTA_PED_SKIN || pidx == GTA_PED_SKIN_BLENDSHAPE)
			{
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);

				//set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha);

				// fix alpha issues around hair
				set_remix_texture_categories(dev, InstanceCategories::IgnoreAlphaChannel | InstanceCategories::DisableBackfaceCulling);
			}

			else {
				not_part_of_large_if_check = true;
			}

			if (g_is_rendering_static)
			{
				if (im->m_dbg_do_not_render_static) {
					ctx.modifiers.do_not_render = true;
				}

				if (im->m_dbg_disable_ps_for_static)
				{
					ctx.save_ps(dev);
					dev->SetPixelShader(nullptr);
				}
			}

			// water - why do I even check this here? - never triggers?
			else if (not_part_of_large_if_check && !g_is_rendering_phone && !g_is_rendering_mirror && !g_is_rendering_vehicle &&
				im->m_stats._water_shader_name_checks.track_check() && ctx.info.shader_name.ends_with("water.fxc"))
			{
				im->m_stats._water_shader_name_checks.track_check(true);
				ctx.modifiers.do_not_render = true;
			}

			else if (im->m_dbg_do_not_render_fx && (g_is_rendering_fx || g_is_rendering_fx_instance)) {
				ctx.modifiers.do_not_render = true;
			}

			if (gs->render_emissive_surfaces_using_shaders.get_as<bool>())
			{
				if (pidx == GTA_EMISSIVE			 || pidx == GTA_EMISSIVENIGHT		|| pidx == GTA_EMISSIVESTRONG ||
					pidx == GTA_GLASS_EMISSIVE		 || pidx == GTA_GLASS_EMISSIVENIGHT	|| pidx == GTA_EMISSIVENIGHT_ALPHA ||
					pidx == GTA_EMISSIVESTRONG_ALPHA || pidx == GTA_EMISSIVE_ALPHA)
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
			else
			{
				if (pidx == GTA_EMISSIVE || pidx == GTA_EMISSIVENIGHT || pidx == GTA_EMISSIVESTRONG || pidx == GTA_GLASS_EMISSIVE || pidx == GTA_GLASS_EMISSIVENIGHT)  {
					renderer_ff::on_ff_emissives(dev, ctx);
				}
				else if (pidx == GTA_EMISSIVENIGHT_ALPHA || pidx == GTA_EMISSIVESTRONG_ALPHA || pidx == GTA_EMISSIVE_ALPHA) {
					renderer_ff::on_ff_emissives_alpha(dev, ctx);
				}
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
				if (im->m_dbg_do_not_render_vehicle) {
					ctx.modifiers.do_not_render = true;
				}

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

				if (pidx == GTA_VEHICLE_VEHGLASS)
				{
					//if (im->m_dbg_vehglass_disable_alphablend)
					{
						ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
						dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
					}
				}
			}

			

			if (ctx.modifiers.is_fx)
			{
				//ctx.save_tss(dev, D3DTSS_ALPHAOP);
				//dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			}

			if (!ctx.modifiers.allow_vertex_colors && !im->m_dbg_disable_ignore_baked_lighting_enforcement) {
				set_remix_texture_categories(dev, InstanceCategories::IgnoreBakedLighting);
			}

			if (g_is_instance_rendering)
			{
				if (im->m_dbg_do_not_render_instances) {
					ctx.modifiers.do_not_render = true;
				}
			}

			if ((gs->timecycle_wetness_enabled.get_as<bool>() && *game::pTimeCycleWetnessChange > 0.0f) || im->m_dbg_global_wetness_override)
			{
				// check for stencil
				DWORD stencil_ref = 0u, stencil_enabled = 0u;
				dev->GetRenderState(D3DRS_STENCILREF, &stencil_ref);
				dev->GetRenderState(D3DRS_STENCILENABLE, &stencil_enabled);

				// surface can get wet if stencil = 0
				if (stencil_enabled && stencil_ref == 0
					|| (g_is_rendering_vehicle && !(pidx == GTA_VEHICLE_INTERIOR || pidx == GTA_VEHICLE_INTERIOR2))
					|| pidx == GTA_PED_REFLECT)
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
					if (wetness_value > 0.0f || im->m_dbg_global_wetness_override)
					{
						const float adjusted_wetness = std::clamp(wetness_value * gs->timecycle_wetness_world_scalar.get_as<float>(), 0.0f, 1.0f);
						float scalar = 1.0f - adjusted_wetness;
						scalar += gs->timecycle_wetness_world_offset.get_as<float>();
						scalar = std::clamp(scalar * 0.8f, 0.0f, 1.0f);
						scalar = scalar < 0.0f ? 0.0f : scalar;

						if (im->m_dbg_global_wetness_override) {
							scalar = std::clamp((1.0f - im->m_dbg_global_wetness), 0.0f, 1.0f);
						}

						if (g_is_rendering_vehicle)
						{
							ctx.modifiers.is_vehicle_wet = true;
							handle_vehicle_roughness_when_wet(dev, ctx);
						}
						else 
						{
							const float rain_size = pidx == GTA_PED_REFLECT ? gs->timecycle_wetness_ped_raindrop_scalar.get_as<float>() : gs->timecycle_wetness_world_raindrop_scalar.get_as<float>();
							uint8_t rain_flags = WETNESS_FLAG_NONE;

							if (pidx == GTA_PED_REFLECT)
							{
								if (gs->timecycle_wetness_ped_raindrop_enable.get_as<bool>()) 
								{
									if (*game::weather_type_prev == game::eWeatherType::WEATHER_RAIN || *game::weather_type_new == game::eWeatherType::WEATHER_LIGHTNING) {
										rain_flags |= renderer::WETNESS_FLAG_ENABLE_EXP_RAINSDROPS | renderer::WETNESS_FLAG_USE_TEXTURE_COORDINATES;
									}
								}
							}
							else // world surfs
							{
								if (gs->timecycle_wetness_world_raindrop_enable.get_as<bool>()) 
								{
									if (*game::weather_type_prev == game::eWeatherType::WEATHER_RAIN || *game::weather_type_new == game::eWeatherType::WEATHER_LIGHTNING) {
										rain_flags |= WETNESS_FLAG_ENABLE_RAINDROPS;
									}
								}

								if (gs->timecycle_wetness_world_puddles_enable.get_as<bool>()) {
									rain_flags |= WETNESS_FLAG_ENABLE_PUDDLES;
								}
							}
							
							set_remix_roughness_scalar(dev, 
								scalar, 
								gs->timecycle_wetness_world_z_normal.get_as<float>(), 
								gs->timecycle_wetness_world_blending.get_as<float>(),
								rain_size, rain_flags);
						}
					}

					if (im->m_dbg_do_not_render_stencil_zero) {
						ctx.modifiers.do_not_render = true;
					}
				}
			}

			if (im->m_dbg_toggle_ff) {
				render_with_ff = false;
			}

			if (render_with_ff)
			{
				if (im->m_dbg_do_not_render_ff) {
					ctx.modifiers.do_not_render = true;
				}

				ctx.save_vs(dev);
				dev->SetVertexShader(nullptr);
			}

			if (im->m_dbg_only_render_static && !g_is_rendering_static) 
			{
				ctx.modifiers.do_not_render = true;
				ctx.modifiers.dual_render = false;
			}

		} // end imgui-is-rendering

		// ---------
		// draw

		auto hr = S_OK;

		if (!render_with_ff && !shared::globals::imgui_is_rendering && im->m_dbg_do_not_render_indexed_prims_with_vertexshader) {
			return hr;
		}

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render && !ctx.modifiers.do_not_render_indexed_primitives) 
		{
			hr = dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

			if (!shared::globals::imgui_is_rendering) {
				im->m_stats._drawcall_indexed_prim.track_single();
			}

			if (!render_with_ff) {
				im->m_stats._drawcall_indexed_prim_using_vs.track_single();
			}
		}


		// second drawings

		if (ctx.modifiers.dual_render)
		{
			bool redrew = false;

			if (ctx.modifiers.dual_render_reset_remix_modifiers) {
				ctx.restore_render_state(dev, (D3DRENDERSTATETYPE)149);
			}

			if (ctx.modifiers.dual_render_texture)
			{
				// save og texture
				ctx.save_texture(dev, 0);

				// set new texture
				dev->SetTexture(0, ctx.modifiers.dual_render_texture);
			}

			if (ctx.modifiers.dual_render_mode_vehicle_livery)
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

				ctx.save_tss(dev, D3DTSS_TEXCOORDINDEX);
				dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 1);

				set_remix_texture_categories(dev, InstanceCategories::DecalStatic);

				// livery
				if (ctx.modifiers.is_vehicle_wet) 
				{
					// same as car paint
					handle_vehicle_roughness_when_wet(dev, ctx);
				}
			}

			if (ctx.modifiers.dual_render_mode_vehicle_dirt)
			{
				handle_vehicle_dirt_decal(dev, ctx);

				// re-draw surface
				//dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount /*- 1*/);
				//redrew = true; 
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

			// BLEND emissive mode
			if (ctx.modifiers.dual_render_mode_emissive_offset)
			{
				//ctx.restore_render_state(dev, (D3DRENDERSTATETYPE)42); // InstanceCategories

				ctx.restore_texture_stage_state(dev, D3DTSS_COLOROP);
				ctx.restore_texture_stage_state(dev, D3DTSS_COLORARG1);

				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE); 
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);  

				//dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY); 

				// i swear this did something at one point?!
				D3DXMATRIX mat = *game::pCurrentWorldTransform;
				mat.m[0][0] = 1.001f; //im->m_debug_vector2.z + 1.00f; //  1.005f
				mat.m[1][1] = 1.001f; //im->m_debug_vector2.z + 1.00f;
				mat.m[2][2] = 1.001f; //im->m_debug_vector2.z + 1.00f;
				dev->SetTransform(D3DTS_WORLD, &mat);

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

				ctx.save_tss(dev, D3DTSS_COLOROP);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_COLORARG1); 
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

				// fix issue with merging material?
				IDirect3DBaseTexture9* currTex = nullptr; 
				if (dev->GetTexture(0, &currTex); currTex)
				{
					DWORD hash = (DWORD)(uintptr_t)currTex;
					set_remix_texture_hash(dev, hash);
				}

				// re-draw surface
				//dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount/* - 1*/);
				//redrew = true;
			}

			if (!redrew)
			{
				// re-draw surface
				dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

			}

			// re-drawing proxy but remix makes it emissive ...
			if (ctx.modifiers.dual_render_mode_emissive_offset)
			{
				if (gs->vehicle_lights_dual_render_proxy_texture.get_as<bool>())
				{
					//dev->SetTransform(D3DTS_WORLD, game::pCurrentWorldTransform);

					D3DXMATRIX mat = *game::pCurrentWorldTransform;
					mat.m[0][0] *= 1.01f;
					mat.m[1][1] *= 1.01f;
					mat.m[2][2] *= 1.01f;

					dev->SetTransform(D3DTS_WORLD, &mat);

					ctx.save_texture(dev, 0);
					dev->SetTexture(0, tex_addons::veh_light_ems_glass); 

					ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
					dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); 

					ctx.save_rs(dev, D3DRS_BLENDOP);
					dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

					ctx.save_rs(dev, D3DRS_SRCBLEND);
					dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

					ctx.save_rs(dev, D3DRS_DESTBLEND);
					dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

					ctx.save_tss(dev, D3DTSS_COLOROP);
					dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

					ctx.save_tss(dev, D3DTSS_COLORARG1);
					dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

					ctx.save_tss(dev, D3DTSS_COLORARG2);
					dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);

					ctx.save_tss(dev, D3DTSS_ALPHAOP);
					dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

					ctx.save_tss(dev, D3DTSS_ALPHAARG1);
					dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

					ctx.save_tss(dev, D3DTSS_ALPHAARG2);
					dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT); 

					renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreOpacityMicromap | InstanceCategories::Terrain | InstanceCategories::IgnoreBakedLighting);
					renderer::set_remix_emissive_intensity(dev, 0); 

					// re-draw surface
					dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
				}
			}
			

			if (ctx.modifiers.dual_render_texture) {
				ctx.restore_texture(dev, 0);
			}
		}



		if (ctx.modifiers.tri_render)
		{
			if (ctx.modifiers.tri_render_texture)
			{
				ctx.restore_texture(dev, 0);

				// save og texture
				ctx.save_texture(dev, 0);

				// set new texture
				dev->SetTexture(0, ctx.modifiers.tri_render_texture);

				handle_vehicle_dirt_decal(dev, ctx);

				// re-draw surface
				dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount /*- 1*/);
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

			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));

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

	// ---

	void post_render_sky()
	{
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

	// ---

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

	// ---

	__declspec (naked) void post_static_render_stub()
	{
		__asm
		{
			mov		g_is_rendering_static, 0;
			retn    0x10;
		}
	}

	// ---

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

	// ---

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
			mov		g_is_rendering_vehicle, 0;
			pop     ebx;
			mov     esp, ebp;
			pop     ebp;
			retn;
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

	// ---

	void on_add_frontendhelpertext_hk()
	{
		const auto im = imgui::get();

		if (!game::is_in_game) {
			return;
		}

		if (im->m_screenshot_mode) {
			natives::get()->HideHudAndRadarThisFrame();
		}

		// reset in gta4::on_begin_scene_cb
		if (g_applied_hud_hack || *game::m_bMobilePhoneActive) {
			return;
		}

		if (im->m_dbg_disable_hud_fixup) {
			return;
		}

		g_applied_hud_hack = true;

		// get drawlist and add CRenderFontBufferDC
		shared::utils::hook::call<void(__cdecl)()>(game::func_addr__add_renderfontbufferdc)();

		struct pos_s
		{
			float x = 0.0f;
			float y = 0.0f;
		};
		const pos_s p = {};

		// add actual text/img drawcmd
		shared::utils::hook::call<void(__cdecl)(pos_s, char*, int, int)>(game::func_addr__frontendhelpertext_add_drawcmd)(p, (char*)".", 0xFFFFFFFF, 0xFFFFFFFF);
	}

	__declspec (naked) void on_add_frontendhelpertext_stub()
	{
		__asm
		{
			mov     ebp, esp;
			and		esp, 0xFFFFFFF8;

			pushad;
			call	on_add_frontendhelpertext_hk;
			popad;

			jmp		game::retn_addr__on_add_frontendhelpertext_stub;
		}
	}

	__declspec (naked) void pre_draw_fx_instance_stub()
	{
		__asm
		{
			mov     esi, ecx;
			mov     ecx, [esi + 8];
			mov		g_is_rendering_fx_instance, 1;
			jmp		game::retn_addr__pre_draw_fx_instance;
		}
	}

	__declspec (naked) void post_draw_fx_instance_stub()
	{
		__asm
		{
			mov		g_is_rendering_fx_instance, 0;
			pop     esi;
			add     esp, 8;
			retn;
		}
	}


	// ----


	__declspec (naked) void pre_draw_fx_stub()
	{
		__asm
		{
			mov		g_is_rendering_fx, 1;
			cmp     ax, [edi + 0x270];
			jmp		game::retn_addr__pre_draw_fx;
		}
	}

	__declspec (naked) void post_draw_fx_stub()
	{
		__asm
		{
			mov		g_is_rendering_fx, 0;
			pop     edi;
			pop     ebx;
			retn	8;
		}
	}

	renderer::renderer()
	{
		p_this = this;

		shared::utils::hook(game::retn_addr__special_SetupVsPsPass_handling - 5u, handle_special_setupvspspass_stub, HOOK_JUMP).install()->quick();
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

		// detect static rendering
		shared::utils::hook(game::retn_addr__pre_draw_statics - 5u, pre_static_render_stub, HOOK_JUMP).install()->quick(); // 0x42EBEC
		shared::utils::hook(game::hk_addr__post_draw_statics, post_static_render_stub, HOOK_JUMP).install()->quick();

		// also considered static
		shared::utils::hook(game::retn_addr__pre_entity_surfs_stub - 5u, pre_entity_surfs_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook(game::hk_addr__post_entity_surfs_stub, post_entity_surfs_stub, HOOK_JUMP).install()->quick();

		// detect vehicle rendering - CDrawFrag
		shared::utils::hook(game::retn_addr__pre_vehicle_surfs_stub - 5u, pre_vehicle_surfs_stub, HOOK_JUMP).install()->quick(); // 0x8DCDA1
		shared::utils::hook(game::hk_addr__post_vehicle_surfs_stub, post_vehicle_surfs_stub, HOOK_JUMP).install()->quick();


		// detect mirror rendering
		shared::utils::hook(game::retn_addr__pre_draw_mirror - 5u, pre_draw_mirror_stub, HOOK_JUMP).install()->quick(); // 0xB59906
		shared::utils::hook(game::hk_addr__post_draw_mirror, post_draw_mirror_stub, HOOK_JUMP).install()->quick(); // 0xB59911

		// do not render postfx RT infront of the camera
		shared::utils::hook::nop(game::nop_addr__disable_postfx_drawing, 5);

		// hack to properly detect UI - we need to "inject" frontend helper text to trigger remix injection asap
		// doing this fixes the sniper scope + shadow background of radar
		shared::utils::hook(game::retn_addr__on_add_frontendhelpertext_stub - 5u, on_add_frontendhelpertext_stub, HOOK_JUMP).install()->quick(); // 0x8B6C81

		// skip og light rendering
		shared::utils::hook::conditional_jump_to_jmp(game::cond_jmp_addr__skip_deferred_light_rendering01); // 0x928AE5
		shared::utils::hook::set(game::cond_jmp_addr__skip_deferred_light_rendering02, 0xC3); // 0x8DCBC0 - do not exec code inside CDrawDefLight::Draw

		shared::utils::hook(game::retn_addr__pre_draw_fx_instance - 5u, pre_draw_fx_instance_stub, HOOK_JUMP).install()->quick(); // 0x8DD5A4 --- retn to (0x8DD5A9 .. sig addr)
		shared::utils::hook(game::hk_addr__post_draw_fx_instance, post_draw_fx_instance_stub, HOOK_JUMP).install()->quick(); // 0x8DD659

		shared::utils::hook::nop(game::retn_addr__pre_draw_fx - 7u, 7); // 0x6035BD
		shared::utils::hook(game::retn_addr__pre_draw_fx - 7u, pre_draw_fx_stub, HOOK_JUMP).install()->quick(); // 0x6035BD --- retn to (0x6035C4 .. sig addr)
		shared::utils::hook(game::hk_addr__post_draw_fx, post_draw_fx_stub, HOOK_JUMP).install()->quick(); // 0x60361C

		// -----
		m_initialized = true;
		shared::common::log("Renderer", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
