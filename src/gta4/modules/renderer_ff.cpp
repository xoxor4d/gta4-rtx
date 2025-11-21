#include "std_include.hpp"
#include "renderer_ff.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"

namespace gta4
{
	void renderer_ff::on_ff_emissives(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		if (!gs->render_emissive_surfaces_using_shaders.get_as<bool>())
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

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			else if (im->m_dbg_emissive_ff_worldui_ignore_alpha) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::WorldUI | InstanceCategories::IgnoreTransparencyLayer);
			}
			else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreTransparencyLayer /*InstanceCategories::DecalStatic*/);
			}

			//if (!im->m_dbg_debug_bool02)
			//	renderer::set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha); // this breaks traffic lights and removes vertex colors at certain angles ??
			//ctx.modifiers.allow_vertex_colors = true;

			// (nope ^ ) ... this breaks traffic lights and removes vertex colors at certain angles?
			// Only used by EMISSIVENIGHT shader
			if (!ctx.info.shaderconst_uses_emissive_multiplier /*|| im->m_dbg_emissive_nonalpha_override*/)
			{
				renderer::set_remix_modifier(dev, RemixModifier::EmissiveScalar);
				renderer::set_remix_emissive_intensity(dev, gs->emissive_generic_scale.get_as<float>() /*im->m_dbg_emissive_nonalpha_override_scale*/);
			}
		}
	}

	void renderer_ff::on_ff_emissives_alpha(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		static auto im = imgui::get();
		static auto gs = game_settings::get();

		if (!gs->render_emissive_surfaces_using_shaders.get_as<bool>())
		{
			ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

			ctx.save_rs(dev, D3DRS_ZENABLE);
			dev->SetRenderState(D3DRS_ZENABLE, FALSE);

			ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

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

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::IgnoreTransparencyLayer /*InstanceCategories::DecalStatic*/);
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
