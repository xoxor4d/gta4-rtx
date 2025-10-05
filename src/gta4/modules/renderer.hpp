#pragma once
#include "shared/utils/utils.hpp"

namespace gta4
{
	extern bool g_test;

	namespace tex_addons
	{
		extern bool initialized;
		extern LPDIRECT3DTEXTURE9 sky;
		extern LPDIRECT3DTEXTURE9 white01;
		extern LPDIRECT3DTEXTURE9 white02;
		extern LPDIRECT3DTEXTURE9 veh_light_ems_glass;
		extern LPDIRECT3DTEXTURE9 berry;
		extern void init_texture_addons(bool release = false);
	}

	enum class RemixModifier : std::uint32_t
	{
		None = 0,
		EmissiveScalar = 1 << 0,
		RoughnessScalar = 1 << 1,
		EnableVertexColor = 1 << 2,
		DecalDirt = 1 << 3,
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
		None = 0u
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
			bool is_tree_foliage = false;
			bool is_grass_foliage = false;
			bool is_fx = false;

			bool dual_render = false; // render prim a second time with texture within stage 0
			bool dual_render_with_specified_texture = false; // render prim a second time with tex defined in 'dual_render_texture'
			bool dual_render_mode_blend_add = false; // renders second prim using blend mode ADD
			bool dual_render_mode_blend_diffuse = false; // renders second prim using blend mode ADD
			bool dual_render_reset_remix_modifiers = false; // reset all active remix modifiers
			IDirect3DBaseTexture9* dual_render_texture = nullptr; // texture to be used when 'dual_render_with_specified_texture' is set

			InstanceCategories remix_instance_categories = InstanceCategories::None;
			RemixModifier remix_modifier = RemixModifier::None;

			void reset()
			{
				do_not_render = false;
				do_not_render_indexed_primitives = false;
				is_vehicle_paint = false;
				is_vehicle_using_switch_on_state = false;
				is_vehicle_on = false;
				is_tree_foliage = false;
				is_grass_foliage = false;
				is_fx = false;

				dual_render = false;
				dual_render_with_specified_texture = false;
				dual_render_mode_blend_add = false;
				dual_render_mode_blend_diffuse = false;
				dual_render_reset_remix_modifiers = false;
				dual_render_texture = nullptr;

				remix_instance_categories = InstanceCategories::None;
				remix_modifier = RemixModifier::None;
			}
		};

		// special handlers for the next prim/s
		modifiers_s modifiers;

		struct info_s
		{
			std::string_view shader_name;
			IDirect3DDevice9* device_ptr = nullptr;
			bool is_dirty = false; // true when context was not reset in drawprimitive

			void reset()
			{
				shader_name = "";
				device_ptr = nullptr;
				is_dirty = false;
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
		static void set_remix_roughness_scalar(IDirect3DDevice9* dev, float roughness_scalar);
		static void set_remix_temp_float01(IDirect3DDevice9* dev, float value);
		static void set_remix_temp_float02(IDirect3DDevice9* dev,  float value);
		static void set_remix_texture_categories(IDirect3DDevice9* dev, const InstanceCategories& cat);
		static void set_remix_texture_hash(IDirect3DDevice9* dev, const std::uint32_t& hash);

		bool m_triggered_remix_injection = false;
		bool m_modified_draw_prim = false;
		static inline drawcall_mod_context dc_ctx {};

	private:
		bool m_initialized = false;
	};
}
