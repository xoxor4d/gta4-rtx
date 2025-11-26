#pragma once
#include "shared/utils/utils.hpp"

namespace gta4
{
	extern int g_is_instance_rendering;
	extern int g_is_sky_rendering;
	extern int g_is_water_rendering;
	extern int g_is_rendering_mirror;
	extern int g_is_rendering_fx_instance;
	extern int g_is_rendering_fx;
	extern int g_is_rendering_static;
	extern int g_is_rendering_vehicle;
	extern bool g_is_rendering_phone;

	extern bool g_rendered_first_primitive;
	extern bool g_applied_hud_hack;

	namespace tex_addons
	{
		extern bool initialized;
		extern LPDIRECT3DTEXTURE9 sky;
		extern LPDIRECT3DTEXTURE9 white01;
		extern LPDIRECT3DTEXTURE9 white02;
		extern LPDIRECT3DTEXTURE9 veh_light_ems_glass;
		extern LPDIRECT3DTEXTURE9 berry;
		extern LPDIRECT3DTEXTURE9 mirror;
		extern void init_texture_addons(bool release = false);
	}

	enum class RemixModifier : std::uint32_t
	{
		None = 0,
		EmissiveScalar = 1 << 0,
		RoughnessScalar = 1 << 1,
		EnableVertexColor = 1 << 2,
		DecalDirt = 1 << 3,
		RemoveVertexColorKeepAlpha = 1 << 4,
		VehicleDecalDirt = 1 << 5,
	};

	constexpr RemixModifier operator|(RemixModifier lhs, RemixModifier rhs) {
		return static_cast<RemixModifier>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
	}

	constexpr RemixModifier& operator|=(RemixModifier& lhs, RemixModifier rhs) {
		lhs = static_cast<RemixModifier>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
		return lhs;
	}

	constexpr RemixModifier operator&(RemixModifier lhs, RemixModifier rhs) {
		return static_cast<RemixModifier>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
	}

	constexpr RemixModifier& operator&=(RemixModifier& lhs, RemixModifier rhs) {
		lhs = static_cast<RemixModifier>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
		return lhs;
	}

	// can't use remixapi_InstanceCategoryFlags as they don't match up with InstanceCategories
	enum class InstanceCategories : uint32_t
	{
		WorldUI = 1 << 0,
		WorldMatte = 1 << 1,
		Sky = 1 << 2,
		Ignore = 1 << 3,
		IgnoreLights = 1 << 4,
		IgnoreAntiCulling = 1 << 5,
		IgnoreMotionBlur = 1 << 6,
		IgnoreOpacityMicromap = 1 << 7,
		IgnoreAlphaChannel = 1 << 8,
		Hidden = 1 << 9,
		Particle = 1 << 10,
		Beam = 1 << 11,
		DecalStatic = 1 << 12,
		DecalDynamic = 1 << 13,
		DecalSingleOffset = 1 << 14,
		DecalNoOffset = 1 << 15,
		AlphaBlendToCutout = 1 << 16,
		Terrain = 1 << 17,
		AnimatedWater = 1 << 18,
		ThirdPersonPlayerModel = 1 << 19,
		ThirdPersonPlayerBody = 1 << 20,
		IgnoreBakedLighting = 1 << 21,
		IgnoreTransparencyLayer = 1 << 22,
		ParticleEmitter = 1 << 23,
		DisableBackfaceCulling = 1 << 24,
		Count = 24,
		None = 0u
	};

	enum ShaderPreset
	{
		GTA_ALPHA = 0,
		GTA_CUTOUT = 2,
		GTA_CUTOUT_FENCE = 3,
		GTA_DECAL = 4,
		GTA_DECAL_DIRT = 6,
		GTA_DECAL_GLUE = 7,
		GTA_DEFAULT = 9,
		GTA_EMISSIVE = 11,
		GTA_EMISSIVENIGHT = 12,
		GTA_EMISSIVENIGHT_ALPHA = 13,
		GTA_EMISSIVESTRONG = 14,
		GTA_EMISSIVESTRONG_ALPHA = 15,
		GTA_EMISSIVE_ALPHA = 16,
		GTA_GLASS = 17,
		GTA_GLASS_EMISSIVE = 18,
		GTA_GLASS_EMISSIVENIGHT = 19,
		GTA_GLASS_REFLECT = 23,
		GTA_GLASS_SPEC = 24,
		GTA_HAIR_SORTED_ALPHA_EXPENSIVE = 26,
		GTA_NORMAL = 29,
		GTA_NORMAL_ALPHA = 30,
		GTA_NORMAL_CUTOUT = 32,
		GTA_NORMAL_DECAL = 33,
		GTA_NORMAL_REFLECT = 34,
		GTA_NORMAL_SPEC = 39,
		GTA_NORMAL_SPEC_ALPHA = 40,
		GTA_NORMAL_SPEC_DECAL = 42,
		GTA_NORMAL_SPEC_REFLECT = 43,
		GTA_PED = 54,
		GTA_PED_ALPHA = 55,
		GTA_PED_REFLECT = 56,
		GTA_PED_SKIN = 58,
		GTA_PED_SKIN_BLENDSHAPE = 59,
		GTA_RADAR = 62,
		GTA_REFLECT = 63,
		GTA_REFLECT_ALPHA = 64,
		GTA_RMPTFX_MESH = 66,
		GTA_SPEC = 67,
		GTA_SPEC_ALPHA = 68,
		GTA_SPEC_CONST = 69,
		GTA_SPEC_DECAL = 70,
		GTA_SPEC_REFLECT = 71,
		GTA_SPEC_REFLECT_ALPHA = 72,
		GTA_TERRAIN_VA_2LYR = 76,
		GTA_TERRAIN_VA_3LYR = 77,
		GTA_TERRAIN_VA_4LYR = 78,
		GTA_TREES = 79,
		GTA_VEHICLE_BADGES = 81,
		GTA_VEHICLE_INTERIOR = 93,
		GTA_VEHICLE_INTERIOR2 = 94,
		GTA_VEHICLE_LIGHTS = 95,
		GTA_VEHICLE_LIGHTSEMISSIVE = 96,
		GTA_VEHICLE_MESH = 97,
		GTA_VEHICLE_NOSPLASH = 98,
		GTA_VEHICLE_PAINT1 = 100,
		GTA_VEHICLE_PAINT2 = 101,
		GTA_VEHICLE_PAINT3 = 102,
		GTA_VEHICLE_SHUTS = 107,
		GTA_VEHICLE_TIRE = 129,
		GTA_VEHICLE_VEHGLASS = 130,
		GTA_WIRE = 131,
	};

	constexpr InstanceCategories operator|(InstanceCategories lhs, InstanceCategories rhs) {
		return static_cast<InstanceCategories>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
	}

	constexpr InstanceCategories& operator|=(InstanceCategories& lhs, InstanceCategories rhs) {
		lhs = static_cast<InstanceCategories>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
		return lhs;
	}

	constexpr InstanceCategories operator&(InstanceCategories lhs, InstanceCategories rhs) {
		return static_cast<InstanceCategories>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
	}

	constexpr InstanceCategories& operator&=(InstanceCategories& lhs, InstanceCategories rhs) {
		lhs = static_cast<InstanceCategories>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
		return lhs;
	}


	class drawcall_mod_context
	{
	public:
		
		// set texture 0 transform
		void set_texture_transform(IDirect3DDevice9* device, const D3DXMATRIX* matrix)
		{
			if (matrix)
			{
				device->SetTransform(D3DTS_TEXTURE0, matrix);
				tex0_transform_set_ = true;
			}
		}

		// save vertex shader
		void save_vs(IDirect3DDevice9* device)
		{
			device->GetVertexShader(&vs_);
			vs_set_ = true;
		}

		// save vertex shader
		void save_ps(IDirect3DDevice9* device)
		{
			device->GetPixelShader(&ps_);
			ps_set_ = true;
		}

		// save texture at stage 0 or 1
		void save_texture(IDirect3DDevice9* device, const bool stage)
		{
			if (!stage)
			{
#if DEBUG
				if (tex0_set_) {
					OutputDebugStringA("save_texture:: tex0 was already saved\n"); return;
				}
#endif

				device->GetTexture(0, &tex0_);
				tex0_set_ = true;
			}
			else
			{
#if DEBUG
				if (tex1_set_) {
					OutputDebugStringA("save_texture:: tex1 was already saved\n"); return;
				}
#endif

				device->GetTexture(1, &tex1_);
				tex1_set_ = true;
			}
		}

		// save render state (e.g. D3DRS_TEXTUREFACTOR) - returns false if rs was previously saved
		bool save_rs(IDirect3DDevice9* device, const D3DRENDERSTATETYPE& state)
		{
			if (saved_render_state_.contains(state)) {
				return false;
			}

			DWORD temp;
			device->GetRenderState(state, &temp);
			saved_render_state_[state] = temp;
			return true;
		}

		bool save_rs(IDirect3DDevice9* device, const uint32_t& state)
		{
			return save_rs(device, (D3DRENDERSTATETYPE)state);
		}

		// save sampler state (D3DSAMPLERSTATETYPE)
		void save_ss(IDirect3DDevice9* device, const D3DSAMPLERSTATETYPE& state)
		{
			if (saved_sampler_state_.contains(state)) {
				return;
			}

			DWORD temp;
			device->GetSamplerState(0, state, &temp);
			saved_sampler_state_[state] = temp;
		}

		// save texture stage 0 state (e.g. D3DTSS_ALPHAARG1) - returns false if tss was previously saved
		bool save_tss(IDirect3DDevice9* device, const D3DTEXTURESTAGESTATETYPE& type)
		{
			if (saved_texture_stage_state_.contains(type)) {
				return false;
			}

			DWORD temp;
			device->GetTextureStageState(0, type, &temp);
			saved_texture_stage_state_[type] = temp;
			return true;
		}

		// save D3DTS_VIEW
		void save_view_transform(IDirect3DDevice9* device)
		{
			device->GetTransform(D3DTS_VIEW, &view_transform_);
			view_transform_set_ = true;
		}

		// save D3DTS_PROJECTION
		void save_projection_transform(IDirect3DDevice9* device)
		{
			device->GetTransform(D3DTS_PROJECTION, &projection_transform_);
			projection_transform_set_ = true;
		}

		// restore vertex shader
		void restore_vs(IDirect3DDevice9* device)
		{
			if (vs_set_)
			{
				device->SetVertexShader(vs_);
				vs_set_ = false;
			}
		}

		// restore pixel shader
		void restore_ps(IDirect3DDevice9* device)
		{
			if (ps_set_)
			{
				device->SetPixelShader(ps_);
				ps_set_ = false;
			}
		}

		// restore texture at stage 0 or 1
		void restore_texture(IDirect3DDevice9* device, const bool stage)
		{
			if (!stage)
			{
				if (tex0_set_)
				{
					device->SetTexture(0, tex0_);
					tex0_set_ = false;
				}
			}
			else
			{
				if (tex1_set_)
				{
					device->SetTexture(1, tex1_);
					tex1_set_ = false;
				}
			}
		}

		// restore a specific render state (e.g. D3DRS_TEXTUREFACTOR)
		void restore_render_state(IDirect3DDevice9* device, const D3DRENDERSTATETYPE& state)
		{
			if (saved_render_state_.contains(state)) {
				device->SetRenderState(state, saved_render_state_[state]);
			}
		}

		// restore a specific sampler state (D3DSAMPLERSTATETYPE)
		void restore_sampler_state(IDirect3DDevice9* device, const D3DSAMPLERSTATETYPE& state)
		{
			if (saved_sampler_state_.contains(state)) {
				device->SetSamplerState(0, state, saved_sampler_state_[state]);
			}
		}

		// restore a specific texture stage 0 state (e.g. D3DTSS_ALPHAARG1)
		void restore_texture_stage_state(IDirect3DDevice9* device, const D3DTEXTURESTAGESTATETYPE& type)
		{
			if (saved_texture_stage_state_.contains(type)) {
				device->SetTextureStageState(0, type, saved_texture_stage_state_[type]);
			}
		}

		// restore texture 0 transform to identity
		void restore_texture_transform(IDirect3DDevice9* device)
		{
			device->SetTransform(D3DTS_TEXTURE0, &shared::globals::IDENTITY);
			tex0_transform_set_ = false;
		}

		// restore saved D3DTS_VIEW
		void restore_view_transform(IDirect3DDevice9* device)
		{
			if (view_transform_set_)
			{
				device->SetTransform(D3DTS_VIEW, &view_transform_);
				view_transform_set_ = false;
			}
		}

		// restore saved D3DTS_PROJECTION
		void restore_projection_transform(IDirect3DDevice9* device)
		{
			if (projection_transform_set_)
			{
				device->SetTransform(D3DTS_PROJECTION, &projection_transform_);
				projection_transform_set_ = false;
			}
		}

		// restore all changes
		void restore_all(IDirect3DDevice9* device)
		{
			restore_vs(device);
			restore_ps(device);
			restore_texture(device, 0);
			restore_texture(device, 1);
			restore_texture_transform(device);
			restore_view_transform(device);
			restore_projection_transform(device);

			for (auto& rs : saved_render_state_) {
				device->SetRenderState(rs.first, rs.second);
			}

			for (auto& ss : saved_sampler_state_) {
				device->SetSamplerState(0, ss.first, ss.second);
			}

			for (auto& tss : saved_texture_stage_state_) {
				device->SetTextureStageState(0, tss.first, tss.second);
			}
		}

		// reset the stored context data
		void reset_context()
		{
			vs_ = nullptr; vs_set_ = false;
			ps_ = nullptr; ps_set_ = false;
			tex0_ = nullptr; tex0_set_ = false;
			tex1_ = nullptr; tex1_set_ = false;
			tex0_transform_set_ = false;
			view_transform_set_ = false;
			projection_transform_set_ = false;
			saved_render_state_.clear();
			saved_sampler_state_.clear();
			saved_texture_stage_state_.clear();
			modifiers.reset();
			info.reset();
		}

		struct modifiers_s
		{
			bool do_not_render = false;
			bool do_not_render_indexed_primitives = false;
			bool is_vehicle_paint = false;
			bool is_vehicle_using_switch_on_state = false;
			bool is_vehicle_on = false;
			bool is_vehicle_wet = false;
			bool is_grass_foliage = false;
			bool is_fx = false;

			bool allow_vertex_colors = false;

			bool dual_render = false; // render prim a second time with texture within stage 0
			bool dual_render_mode_vehicle_livery = false;
			bool dual_render_mode_vehicle_dirt = false;
			bool dual_render_mode_blend_add = false; // renders second prim using blend mode ADD
			bool dual_render_mode_blend_diffuse = false; // renders second prim using blend mode ADD
			bool dual_render_mode_emissive_offset = false; // renders second prim emissive
			bool dual_render_reset_remix_modifiers = false; // reset all active remix modifiers
			IDirect3DBaseTexture9* dual_render_texture = nullptr; // use this texture for dual rendering if set

			bool tri_render = false; 
			IDirect3DBaseTexture9* tri_render_texture = nullptr;

			InstanceCategories remix_instance_categories = InstanceCategories::None;
			RemixModifier remix_modifier = RemixModifier::None;

			void reset()
			{
				do_not_render = false;
				do_not_render_indexed_primitives = false;
				is_vehicle_paint = false;
				is_vehicle_using_switch_on_state = false;
				is_vehicle_on = false;
				is_vehicle_wet = false;
				is_grass_foliage = false;
				is_fx = false;

				allow_vertex_colors = false;

				dual_render = false;
				dual_render_mode_vehicle_livery = false;
				dual_render_mode_vehicle_dirt = false;
				dual_render_mode_blend_add = false;
				dual_render_mode_blend_diffuse = false;
				dual_render_mode_emissive_offset = false;
				dual_render_reset_remix_modifiers = false;
				dual_render_texture = nullptr;

				tri_render = false;
				tri_render_texture = nullptr;

				remix_instance_categories = InstanceCategories::None;
				remix_modifier = RemixModifier::None;
			}
		};

		// special handlers for the next prim/s
		modifiers_s modifiers;

		struct info_s
		{
			std::string_view shader_name;
			std::string_view preset_name;
			int preset_index = 0;
			IDirect3DDevice9* device_ptr = nullptr;
			bool is_dirty = false; // true when context was not reset in drawprimitive
			bool is_gta_rmptfx_litsprite_shader = false;
			bool checked_for_gta_rmptfx_litsprite_shader = false; // true if shader check was performed
			bool is_bink = false;

			float shaderconst_emissive_intensity = 0.0f;
			bool shaderconst_uses_emissive_multiplier = false;

			DWORD rs_alphablendenable = 0u;
			DWORD rs_blendop = 0u;
			DWORD rs_srcblend = 0u;
			DWORD rs_destblend = 0u;
			DWORD tss_alphaop = 0u;
			DWORD tss_alphaarg1 = 0u;
			DWORD tss_alphaarg2 = 0u;

			Vector4D ps_const_72_veh_dirt;
			Vector4D ps_const_73_veh_dirt;
			Vector4D ps_const_74_veh_dirt;

			void reset()
			{
				shader_name = "";
				preset_name = "";
				preset_index = 0;
				device_ptr = nullptr;
				is_dirty = false;
				is_gta_rmptfx_litsprite_shader = false;
				checked_for_gta_rmptfx_litsprite_shader = false;
				is_bink = false;

				shaderconst_emissive_intensity = 0.0f;
				shaderconst_uses_emissive_multiplier = false;

				rs_alphablendenable = 0u;
				rs_blendop = 0u;
				rs_srcblend = 0u;
				rs_destblend = 0u;
				tss_alphaop = 0u;
				tss_alphaarg1 = 0u;
				tss_alphaarg2 = 0u;

				ps_const_72_veh_dirt.Zero();
				ps_const_73_veh_dirt.Zero();
				ps_const_74_veh_dirt.Zero();
			}
		};

		// holds information about the current pass
		info_s info;

		// constructor for singleton
		drawcall_mod_context() = default;

	private:
		// Render states to save
		IDirect3DVertexShader9* vs_ = nullptr;
		IDirect3DPixelShader9* ps_ = nullptr;
		IDirect3DBaseTexture9* tex0_ = nullptr;
		IDirect3DBaseTexture9* tex1_ = nullptr;
		bool vs_set_ = false;
		bool ps_set_ = false;
		bool tex0_set_ = false;
		bool tex1_set_ = false;
		bool tex0_transform_set_ = false;
		D3DMATRIX view_transform_ = {};
		D3DMATRIX projection_transform_ = {};
		bool view_transform_set_ = false;
		bool projection_transform_set_ = false;

		// store saved render states (with the type as the key)
		std::unordered_map<D3DRENDERSTATETYPE, DWORD> saved_render_state_;

		// store saved render states (with the type as the key)
		std::unordered_map<D3DSAMPLERSTATETYPE, DWORD> saved_sampler_state_;

		// store saved texture stage states (with type as the key)
		std::unordered_map<D3DTEXTURESTAGESTATETYPE, DWORD> saved_texture_stage_state_;
	};

	// ----

	class renderer final : public shared::common::loader::component_module
	{
	public:
		renderer();

		static inline renderer* p_this = nullptr;
		static renderer* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		void manually_trigger_remix_injection(IDirect3DDevice9* dev);
		HRESULT on_draw_primitive(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const UINT& StartVertex, const UINT& PrimitiveCount);
		HRESULT on_draw_indexed_prim(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const INT& BaseVertexIndex, const UINT& MinVertexIndex, const UINT& NumVertices, const UINT& startIndex, const UINT& primCount);

		static void set_remix_modifier(IDirect3DDevice9* dev, RemixModifier mod);
		static void set_remix_emissive_intensity(IDirect3DDevice9* dev, float intensity, bool no_overrides = false);

		enum eWetnessFlags : uint8_t
		{
			WETNESS_FLAG_ENABLE_PUDDLES = 1 << 0,
			WETNESS_FLAG_ENABLE_RAINDROPS = 1 << 1,				// either expensive or normal raindrops
			WETNESS_FLAG_USE_TEXTURE_COORDINATES = 1 << 2,
			WETNESS_FLAG_ENABLE_EXP_RAINSDROPS = 1 << 3,		// either expensive or normal raindrops
			WETNESS_FLAG_TEMP_04 = 1 << 4,
			WETNESS_FLAG_TEMP_05 = 1 << 5,
			WETNESS_FLAG_TEMP_06 = 1 << 6,
			WETNESS_FLAG_TEMP_07 = 1 << 7,
			WETNESS_FLAG_NONE = 0u
		};

		static void set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar, float max_z = 0.35f, float blend_width = 0.65f, float param4 = 0.0f, uint8_t flags = WETNESS_FLAG_NONE);

		static void set_remix_temp_float01(IDirect3DDevice9* dev, float value);
		static void set_remix_temp_float01_pack_two_halfs(IDirect3DDevice9* dev, float value_low, float value_high);
		static void set_remix_temp_float02(IDirect3DDevice9* dev,  float value);
		static void set_remix_temp_float02_pack_two_halfs(IDirect3DDevice9* dev, float value_low, float value_high);
		static void set_remix_temp_float03_pack_four(IDirect3DDevice9* dev, float v1, float v2, float v3, float v4);
		static void set_remix_texture_categories(IDirect3DDevice9* dev, const InstanceCategories& cat);
		static void set_remix_texture_hash(IDirect3DDevice9* dev, const std::uint32_t& hash);

		bool m_triggered_remix_injection = false;
		bool m_modified_draw_prim = false;
		static inline drawcall_mod_context dc_ctx {};

	private:
		bool m_initialized = false;
	};
}
