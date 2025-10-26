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
			dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

			ctx.save_rs(dev, D3DRS_ZENABLE);
			dev->SetRenderState(D3DRS_ZENABLE, FALSE);


			ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);

			ctx.save_rs(dev, D3DRS_BLENDOP);
			dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

			ctx.save_rs(dev, D3DRS_SRCBLEND);
			dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);

			ctx.save_rs(dev, D3DRS_DESTBLEND);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

			ctx.save_tss(dev, D3DTSS_COLOROP);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_COLORARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_COLORARG2);
			//dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

			/*ctx.save_tss(dev, D3DTSS_ALPHAOP);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG2);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);*/

			float intensity = ctx.info.shaderconst_emissive_intensity;

			ctx.save_rs(dev, D3DRS_TEXTUREFACTOR);
			dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(intensity, intensity, intensity, intensity));

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::DecalStatic /*| InstanceCategories::Terrain*/); //1 << im->m_dbg_tag_emissivenight_as_index);
			}

			//renderer::set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha);
			//ctx.modifiers.allow_vertex_colors = true;
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
			dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);

			ctx.save_rs(dev, D3DRS_DESTBLEND);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

			ctx.save_tss(dev, D3DTSS_COLOROP);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_COLORARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_COLORARG2);
			//dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

			ctx.save_tss(dev, D3DTSS_ALPHAOP);
			dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG1);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			ctx.save_tss(dev, D3DTSS_ALPHAARG2);
			//dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

			
			float intensity = ctx.info.shaderconst_emissive_intensity;

			ctx.save_rs(dev, D3DRS_TEXTUREFACTOR); 
			dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(intensity, intensity, intensity, intensity));

			if (im->m_dbg_tag_static_emissive_as_index != -1) {
				renderer::set_remix_texture_categories(dev, (InstanceCategories)(1 << im->m_dbg_tag_static_emissive_as_index));
			}
			else if (gs->assign_decal_category_to_emissive_surfaces.get_as<bool>()) {
				renderer::set_remix_texture_categories(dev, InstanceCategories::DecalStatic /*| InstanceCategories::Terrain*/); //1 << im->m_dbg_tag_emissivenight_as_index);
			}

			//renderer::set_remix_modifier(dev, RemixModifier::RemoveVertexColorKeepAlpha);
			//ctx.modifiers.allow_vertex_colors = true;
		}
	}

	renderer_ff::renderer_ff()
	{
		p_this = this;

		// -----
		m_initialized = true;
		std::cout << "[RENDERER_FF] loaded\n";
	}
}
