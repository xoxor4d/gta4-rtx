#include "std_include.hpp"
#include "renderer_ff.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"

namespace gta4
{
	void handle_global_uv_anims(IDirect3DDevice9* dev, drawcall_mod_context& ctx, bool is_alpha_blended = false)
	{
		const auto im = imgui::get();

		if (ctx.info.has_global_anim_uv1 && ctx.info.has_global_anim_uv2 && !im->m_dbg_disable_global_uv_anims)
		{
			renderer::set_remix_modifier(dev, RemixModifier::UseGlobalUVs);

			ctx.save_rs(dev, RS_211_FREE);
			ctx.save_rs(dev, RS_212_FREE);
			ctx.save_rs(dev, RS_213_FREE);
			ctx.save_rs(dev, RS_214_FREE);
			ctx.save_rs(dev, RS_215_FREE);
			ctx.save_rs(dev, RS_216_FREE);

			dev->SetRenderState((D3DRENDERSTATETYPE)RS_211_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv0.x));
			dev->SetRenderState((D3DRENDERSTATETYPE)RS_212_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv0.y));
			dev->SetRenderState((D3DRENDERSTATETYPE)RS_213_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv0.z));

			dev->SetRenderState((D3DRENDERSTATETYPE)RS_214_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv1.x));
			dev->SetRenderState((D3DRENDERSTATETYPE)RS_215_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv1.y));
			dev->SetRenderState((D3DRENDERSTATETYPE)RS_216_FREE, *reinterpret_cast<DWORD*>(&ctx.info.global_anim_uv1.z));

			if (   !im->m_dbg_disable_omm_override_on_alpha_uv_anims && is_alpha_blended
				&& ctx.info.global_anim_uv0 != Vector(1.0f, 0.0f, 0.0f) 
				&& ctx.info.global_anim_uv1 != Vector(0.0f, 1.0f, 0.0f)) 
			{
				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreOpacityMicromap); // OMM prevents animation
			}
		}
	}

	void renderer_ff::on_ff_emissives(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		const auto im = imgui::get();
		const auto gs = game_settings::get();

		if (im->m_dbg_emissive_ff_do_not_render) 
		{
			ctx.modifiers.do_not_render = true;
			return;
		}

		if (!im->m_dbg_render_emissives_with_shaders)
		{
			ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, im->m_dbg_emissive_ff_worldui_ignore_alpha);

			ctx.save_rs(dev, D3DRS_ZENABLE);
			dev->SetRenderState(D3DRS_ZENABLE, im->m_dbg_emissive_ff_worldui_ignore_alpha);

			ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE); 
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, im->m_dbg_emissive_ff_with_alphablend);

			/*ctx.save_rs(dev, D3DRS_SEPARATEALPHABLENDENABLE);
			dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, true);

			ctx.save_rs(dev, D3DRS_BLENDOPALPHA);
			dev->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
			ctx.save_rs(dev, D3DRS_SRCBLENDALPHA);
			dev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_SRCALPHA);
			ctx.save_rs(dev, D3DRS_DESTBLENDALPHA);
			dev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);*/

			ctx.save_rs(dev, D3DRS_BLENDOP);
			dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

			ctx.save_rs(dev, D3DRS_SRCBLEND);
			dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE); // D3DBLEND_ONE

			ctx.save_rs(dev, D3DRS_DESTBLEND);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

			ctx.save_tss(dev, D3DTSS_COLOROP);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_COLORARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_COLORARG2);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			//dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

			ctx.save_tss(dev, D3DTSS_ALPHAOP);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, im->m_dbg_emissive_ff_worldui_ignore_alpha ? D3DTOP_DISABLE : D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG2);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			//float intensity = ctx.info.shaderconst_emissive_intensity;
			//ctx.save_rs(dev, D3DRS_TEXTUREFACTOR);
			//dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(intensity, intensity, intensity, intensity));

			// animate UVs if mesh has correct shader params
			handle_global_uv_anims(dev, ctx);

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			else if (im->m_dbg_emissive_ff_worldui_ignore_alpha) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::WorldUI | InstanceCategories::IgnoreTransparencyLayer);
			}
			else if (im->m_dbg_render_emissives_with_shaders_tag_as_decal) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::WorldUI /*InstanceCategories::DecalStatic*/);
			}

			//if (!im->m_dbg_debug_bool02)
			//	renderer::set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha); // this breaks traffic lights and removes vertex colors at certain angles ??
			//ctx.modifiers.allow_vertex_colors = true;

			// (nope ^ ) ... this breaks traffic lights and removes vertex colors at certain angles?
			// Only used by EMISSIVENIGHT shader
			if (!ctx.info.shaderconst_uses_emissive_multiplier /*|| im->m_dbg_emissive_nonalpha_override*/)
			{
				renderer::set_remix_modifier(dev, RemixModifier::EmissiveScalar);

				if (ctx.info.preset_index == GTA_EMISSIVENIGHT || ctx.info.preset_index == GTA_EMISSIVENIGHT_ALPHA)
				{
					if (*game::m_game_clock_hours <= 6 || *game::m_game_clock_hours >= 19) {
						renderer::set_remix_emissive_intensity(dev, gs->emissive_generic_scale.get_as<float>());
					} else {
						renderer::set_remix_emissive_intensity(dev, 0.0f);
					}
				}
				else {
					renderer::set_remix_emissive_intensity(dev, gs->emissive_generic_scale.get_as<float>() /*im->m_dbg_emissive_nonalpha_override_scale*/);
				}
			}
		}
	}

	void renderer_ff::on_ff_emissives_alpha(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		const auto im = imgui::get();
		const auto gs = game_settings::get();

		if (im->m_dbg_emissive_ff_alphablend_do_not_render)
		{
			ctx.modifiers.do_not_render = true;
			return;
		}

		if (!im->m_dbg_render_emissives_with_shaders)
		{
			ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

			ctx.save_rs(dev, D3DRS_ZENABLE);
			dev->SetRenderState(D3DRS_ZENABLE, FALSE);

			ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, im->m_dbg_emissive_ff_alphablend_enable_alphablend); // TRUE

			ctx.save_rs(dev, D3DRS_BLENDOP);
			dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

			ctx.save_rs(dev, D3DRS_SRCBLEND);
			dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

			ctx.save_rs(dev, D3DRS_DESTBLEND);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

			ctx.save_tss(dev, D3DTSS_COLOROP);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_COLORARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_COLORARG2);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			//dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

			ctx.save_tss(dev, D3DTSS_ALPHAOP);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG2);
			//dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			
			//float intensity = ctx.info.shaderconst_emissive_intensity;

			//ctx.save_rs(dev, D3DRS_TEXTUREFACTOR); 
			//dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(intensity, intensity, intensity, intensity));

			// animate UVs if mesh has correct shader params
			handle_global_uv_anims(dev, ctx, true);

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			/*else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreTransparencyLayer);
			}*/

			else if (gs->emissive_alpha_blend_hack._bool())
			{
				renderer::set_remix_texture_categories(dev, InstanceCategories::WorldUI | InstanceCategories::DecalStatic);

				renderer::set_remix_modifier(dev, RemixModifier::EmissiveScalar);
				renderer::set_remix_emissive_intensity(dev, ctx.info.shaderconst_emissive_intensity * gs->emissive_alpha_blend_hack_scale._float());
			}

			else if (im->m_dbg_emissive_ff_alphablend_test1) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreTransparencyLayer);
			}

			renderer::set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha); 
			ctx.modifiers.allow_vertex_colors = true;
		}
	}

	renderer_ff::renderer_ff()
	{
		p_this = this;

		// -----
		m_initialized = true;
		shared::common::log("RendererFF", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
