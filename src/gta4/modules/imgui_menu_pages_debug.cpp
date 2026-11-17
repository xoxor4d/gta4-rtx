#include "std_include.hpp"
#include "imgui_menu.hpp"

#include "distant_lights.hpp"
#include "game_lights.hpp"
#include "imgui.hpp"
#include "renderer.hpp"
#include "renderer_api.hpp"
#include "timecycle.hpp"
#include "shared/common/flags.hpp"
#include "shared/common/remix_api.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/imgui_helper.hpp"

namespace gta4::menu
{
	void dev_shader_container()
	{
		static const auto& im = imgui::get();

		ImGui::Checkbox("Enable Ignore Shader Logic", &im->m_dbg_enable_ignore_shader_logic);
		ImGui::BeginDisabled(!im->m_dbg_enable_ignore_shader_logic);
		{
			ImGui::Checkbox("Also Ignore on DrawPrimitive calls", &im->m_dbg_ignore_drawprimitive);
			ImGui::Checkbox("Ignore ALL", &im->m_dbg_ignore_all);

			static bool toggle_all_state = false;
			if(ImGui::Checkbox("Toggle All", &toggle_all_state))
			{
				im->m_dbg_ignore_cascade = toggle_all_state;
				im->m_dbg_ignore_deferred_lighting = toggle_all_state;
				im->m_dbg_ignore_gpuptfx_simplerender = toggle_all_state;
				im->m_dbg_ignore_gta_atmoscatt_clouds = toggle_all_state;
				im->m_dbg_ignore_gta_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_cutout_fence = toggle_all_state;
				im->m_dbg_ignore_gta_decal = toggle_all_state;
				im->m_dbg_ignore_gta_decal_amb_only = toggle_all_state;
				im->m_dbg_ignore_gta_decal_dirt = toggle_all_state;
				im->m_dbg_ignore_gta_decal_glue = toggle_all_state;
				im->m_dbg_ignore_gta_decal_normal_only = toggle_all_state;
				im->m_dbg_ignore_gta_default = toggle_all_state;
				im->m_dbg_ignore_gta_diffuse_instance = toggle_all_state;
				im->m_dbg_ignore_gta_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_emissivestrong = toggle_all_state;
				im->m_dbg_ignore_gta_glass = toggle_all_state;
				im->m_dbg_ignore_gta_glass_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_glass_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_glass_normal_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_glass_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_glass_spec = toggle_all_state;
				im->m_dbg_ignore_gta_grass = toggle_all_state;
				im->m_dbg_ignore_gta_hair_sorted_alpha = toggle_all_state;
				im->m_dbg_ignore_gta_hair_sorted_alpha_exp = toggle_all_state;
				im->m_dbg_ignore_gta_im = toggle_all_state;
				im->m_dbg_ignore_gta_normal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect_alpha = toggle_all_state;
				im->m_dbg_ignore_gta_normal_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_cubemap_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_emissive = toggle_all_state;
				im->m_dbg_ignore_gta_normal_spec_reflect_emissivenight = toggle_all_state;
				im->m_dbg_ignore_gta_parallax = toggle_all_state;
				im->m_dbg_ignore_gta_parallax_specmap = toggle_all_state;
				im->m_dbg_ignore_gta_parallax_steep = toggle_all_state;
				im->m_dbg_ignore_gta_ped = toggle_all_state;
				im->m_dbg_ignore_gta_ped_face = toggle_all_state;
				im->m_dbg_ignore_gta_ped_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_ped_skin = toggle_all_state;
				im->m_dbg_ignore_gta_ped_skin_blendshape = toggle_all_state;
				im->m_dbg_ignore_gta_projtex = toggle_all_state;
				im->m_dbg_ignore_gta_projtex_steep = toggle_all_state;
				im->m_dbg_ignore_gta_radar = toggle_all_state;
				im->m_dbg_ignore_gta_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_gpurender = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_litsprite = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_mesh = toggle_all_state;
				im->m_dbg_ignore_gta_rmptfx_raindrops = toggle_all_state;
				im->m_dbg_ignore_gta_spec = toggle_all_state;
				im->m_dbg_ignore_gta_spec_decal = toggle_all_state;
				im->m_dbg_ignore_gta_spec_reflect = toggle_all_state;
				im->m_dbg_ignore_gta_spec_reflect_decal = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_2lyr = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_3lyr = toggle_all_state;
				im->m_dbg_ignore_gta_terrain_va_4lyr = toggle_all_state;
				im->m_dbg_ignore_gta_trees = toggle_all_state;
				im->m_dbg_ignore_gta_trees_extended = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_badges = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_basic = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_chrome = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_disc = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_generic = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_interior = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_interior2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_lightsemissive = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_mesh = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint1 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_paint3 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims1 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims2 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rims3 = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_rubber = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_shuts = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_tire = toggle_all_state;
				im->m_dbg_ignore_gta_vehicle_vehglass = toggle_all_state;
				im->m_dbg_ignore_gta_wire = toggle_all_state;
				im->m_dbg_ignore_mirror = toggle_all_state;
				im->m_dbg_ignore_rage_atmoscatt_clouds = toggle_all_state;
				im->m_dbg_ignore_rage_billboard_nobump = toggle_all_state;
				im->m_dbg_ignore_rage_bink = toggle_all_state;
				im->m_dbg_ignore_rage_default = toggle_all_state;
				im->m_dbg_ignore_rage_fastmipmap = toggle_all_state;
				im->m_dbg_ignore_rage_im = toggle_all_state;
				im->m_dbg_ignore_rage_perlinnoise = toggle_all_state;
				im->m_dbg_ignore_rage_postfx = toggle_all_state;
				im->m_dbg_ignore_rmptfx_collision = toggle_all_state;
				im->m_dbg_ignore_rmptfx_default = toggle_all_state;
				im->m_dbg_ignore_rmptfx_litsprite = toggle_all_state;
				im->m_dbg_ignore_shadowSmartBlit = toggle_all_state;
				im->m_dbg_ignore_shadowZ = toggle_all_state;
				im->m_dbg_ignore_shadowZDir = toggle_all_state;
				im->m_dbg_ignore_water = toggle_all_state;
				im->m_dbg_ignore_waterTex = toggle_all_state;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("Uncategorized ..."))
			{
				ImGui::Checkbox("cascade.fxc", &im->m_dbg_ignore_cascade);
				ImGui::Checkbox("deferred_lighting.fxc", &im->m_dbg_ignore_deferred_lighting);
				ImGui::Checkbox("gpuptfx_simplerender.fxc", &im->m_dbg_ignore_gpuptfx_simplerender);
				ImGui::Checkbox("gta_atmoscatt_clouds.fxc", &im->m_dbg_ignore_gta_atmoscatt_clouds);
				ImGui::Checkbox("gta_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_cubemap_reflect);
				ImGui::Checkbox("gta_cutout_fence.fxc", &im->m_dbg_ignore_gta_cutout_fence);
				ImGui::Checkbox("gta_default.fxc", &im->m_dbg_ignore_gta_default);
				ImGui::Checkbox("gta_diffuse_instance.fxc", &im->m_dbg_ignore_gta_diffuse_instance);
				ImGui::Checkbox("gta_grass.fxc", &im->m_dbg_ignore_gta_grass);
				ImGui::Checkbox("gta_hair_sorted_alpha.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha);
				ImGui::Checkbox("gta_hair_sorted_alpha_exp.fxc", &im->m_dbg_ignore_gta_hair_sorted_alpha_exp);
				ImGui::Checkbox("gta_im.fxc", &im->m_dbg_ignore_gta_im);
				ImGui::Checkbox("gta_projtex.fxc", &im->m_dbg_ignore_gta_projtex);
				ImGui::Checkbox("gta_projtex_steep.fxc", &im->m_dbg_ignore_gta_projtex_steep);
				ImGui::Checkbox("gta_radar.fxc", &im->m_dbg_ignore_gta_radar);
				ImGui::Checkbox("gta_reflect.fxc", &im->m_dbg_ignore_gta_reflect);
				ImGui::Checkbox("gta_reflect_decal.fxc", &im->m_dbg_ignore_gta_reflect_decal);
				ImGui::Checkbox("gta_terrain_va_2lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_2lyr);
				ImGui::Checkbox("gta_terrain_va_3lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_3lyr);
				ImGui::Checkbox("gta_terrain_va_4lyr.fxc", &im->m_dbg_ignore_gta_terrain_va_4lyr);
				ImGui::Checkbox("gta_trees.fxc", &im->m_dbg_ignore_gta_trees);
				ImGui::Checkbox("gta_trees_extended.fxc", &im->m_dbg_ignore_gta_trees_extended);
				ImGui::Checkbox("gta_wire.fxc", &im->m_dbg_ignore_gta_wire);
				ImGui::Checkbox("mirror.fxc", &im->m_dbg_ignore_mirror);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_DECAL ..."))
			{
				ImGui::Checkbox("gta_decal.fxc", &im->m_dbg_ignore_gta_decal);
				ImGui::Checkbox("gta_decal_amb_only.fxc", &im->m_dbg_ignore_gta_decal_amb_only);
				ImGui::Checkbox("gta_decal_dirt.fxc", &im->m_dbg_ignore_gta_decal_dirt);
				ImGui::Checkbox("gta_decal_glue.fxc", &im->m_dbg_ignore_gta_decal_glue);
				ImGui::Checkbox("gta_decal_normal_only.fxc", &im->m_dbg_ignore_gta_decal_normal_only);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_EMISSIVE ..."))
			{
				ImGui::Checkbox("gta_emissive.fxc", &im->m_dbg_ignore_gta_emissive);
				ImGui::Checkbox("gta_emissivenight.fxc", &im->m_dbg_ignore_gta_emissivenight);
				ImGui::Checkbox("gta_emissivestrong.fxc", &im->m_dbg_ignore_gta_emissivestrong);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_GLASS ..."))
			{
				ImGui::Checkbox("gta_glass.fxc", &im->m_dbg_ignore_gta_glass);
				ImGui::Checkbox("gta_glass_emissive.fxc", &im->m_dbg_ignore_gta_glass_emissive);
				ImGui::Checkbox("gta_glass_emissivenight.fxc", &im->m_dbg_ignore_gta_glass_emissivenight);
				ImGui::Checkbox("gta_glass_normal_spec_reflect.fxc", &im->m_dbg_ignore_gta_glass_normal_spec_reflect);
				ImGui::Checkbox("gta_glass_reflect.fxc", &im->m_dbg_ignore_gta_glass_reflect);
				ImGui::Checkbox("gta_glass_spec.fxc", &im->m_dbg_ignore_gta_glass_spec);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_NORMAL ..."))
			{
				ImGui::Checkbox("gta_normal.fxc", &im->m_dbg_ignore_gta_normal);
				ImGui::Checkbox("gta_normal_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_normal_cubemap_reflect);
				ImGui::Checkbox("gta_normal_decal.fxc", &im->m_dbg_ignore_gta_normal_decal);
				ImGui::Checkbox("gta_normal_reflect.fxc", &im->m_dbg_ignore_gta_normal_reflect);
				ImGui::Checkbox("gta_normal_reflect_alpha.fxc", &im->m_dbg_ignore_gta_normal_reflect_alpha);
				ImGui::Checkbox("gta_normal_reflect_decal.fxc", &im->m_dbg_ignore_gta_normal_reflect_decal);
				ImGui::Checkbox("gta_normal_spec.fxc", &im->m_dbg_ignore_gta_normal_spec);
				ImGui::Checkbox("gta_normal_spec_cubemap_reflect.fxc", &im->m_dbg_ignore_gta_normal_spec_cubemap_reflect);
				ImGui::Checkbox("gta_normal_spec_decal.fxc", &im->m_dbg_ignore_gta_normal_spec_decal);
				ImGui::Checkbox("gta_normal_spec_reflect.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect);
				ImGui::Checkbox("gta_normal_spec_reflect_decal.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_decal);
				ImGui::Checkbox("gta_normal_spec_reflect_emissive.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_emissive);
				ImGui::Checkbox("gta_normal_spec_reflect_emissivenight.fxc", &im->m_dbg_ignore_gta_normal_spec_reflect_emissivenight);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_PARALLAX ..."))
			{
				ImGui::Checkbox("gta_parallax.fxc", &im->m_dbg_ignore_gta_parallax);
				ImGui::Checkbox("gta_parallax_specmap.fxc", &im->m_dbg_ignore_gta_parallax_specmap);
				ImGui::Checkbox("gta_parallax_steep.fxc", &im->m_dbg_ignore_gta_parallax_steep);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_PED ..."))
			{
				ImGui::Checkbox("gta_ped.fxc", &im->m_dbg_ignore_gta_ped);
				ImGui::Checkbox("gta_ped_face.fxc", &im->m_dbg_ignore_gta_ped_face);
				ImGui::Checkbox("gta_ped_reflect.fxc", &im->m_dbg_ignore_gta_ped_reflect);
				ImGui::Checkbox("gta_ped_skin.fxc", &im->m_dbg_ignore_gta_ped_skin);
				ImGui::Checkbox("gta_ped_skin_blendshape.fxc", &im->m_dbg_ignore_gta_ped_skin_blendshape);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_RMPTFX ..."))
			{
				ImGui::Checkbox("gta_rmptfx_gpurender.fxc", &im->m_dbg_ignore_gta_rmptfx_gpurender);
				ImGui::Checkbox("gta_rmptfx_litsprite.fxc", &im->m_dbg_ignore_gta_rmptfx_litsprite);
				ImGui::Checkbox("gta_rmptfx_mesh.fxc", &im->m_dbg_ignore_gta_rmptfx_mesh);
				ImGui::Checkbox("gta_rmptfx_raindrops.fxc", &im->m_dbg_ignore_gta_rmptfx_raindrops);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_SPEC ..."))
			{
				ImGui::Checkbox("gta_spec.fxc", &im->m_dbg_ignore_gta_spec);
				ImGui::Checkbox("gta_spec_decal.fxc", &im->m_dbg_ignore_gta_spec_decal);
				ImGui::Checkbox("gta_spec_reflect.fxc", &im->m_dbg_ignore_gta_spec_reflect);
				ImGui::Checkbox("gta_spec_reflect_decal.fxc", &im->m_dbg_ignore_gta_spec_reflect_decal);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("GTA_VEHICLE ..."))
			{
				ImGui::Checkbox("gta_vehicle_badges.fxc", &im->m_dbg_ignore_gta_vehicle_badges);
				ImGui::Checkbox("gta_vehicle_basic.fxc", &im->m_dbg_ignore_gta_vehicle_basic);
				ImGui::Checkbox("gta_vehicle_chrome.fxc", &im->m_dbg_ignore_gta_vehicle_chrome);
				ImGui::Checkbox("gta_vehicle_disc.fxc", &im->m_dbg_ignore_gta_vehicle_disc);
				ImGui::Checkbox("gta_vehicle_generic.fxc", &im->m_dbg_ignore_gta_vehicle_generic);
				ImGui::Checkbox("gta_vehicle_interior.fxc", &im->m_dbg_ignore_gta_vehicle_interior);
				ImGui::Checkbox("gta_vehicle_interior2.fxc", &im->m_dbg_ignore_gta_vehicle_interior2);
				ImGui::Checkbox("gta_vehicle_lightsemissive.fxc", &im->m_dbg_ignore_gta_vehicle_lightsemissive);
				ImGui::Checkbox("gta_vehicle_mesh.fxc", &im->m_dbg_ignore_gta_vehicle_mesh);
				ImGui::Checkbox("gta_vehicle_paint1.fxc", &im->m_dbg_ignore_gta_vehicle_paint1);
				ImGui::Checkbox("gta_vehicle_paint2.fxc", &im->m_dbg_ignore_gta_vehicle_paint2);
				ImGui::Checkbox("gta_vehicle_paint3.fxc", &im->m_dbg_ignore_gta_vehicle_paint3);
				ImGui::Checkbox("gta_vehicle_rims1.fxc", &im->m_dbg_ignore_gta_vehicle_rims1);
				ImGui::Checkbox("gta_vehicle_rims2.fxc", &im->m_dbg_ignore_gta_vehicle_rims2);
				ImGui::Checkbox("gta_vehicle_rims3.fxc", &im->m_dbg_ignore_gta_vehicle_rims3);
				ImGui::Checkbox("gta_vehicle_rubber.fxc", &im->m_dbg_ignore_gta_vehicle_rubber);
				ImGui::Checkbox("gta_vehicle_shuts.fxc", &im->m_dbg_ignore_gta_vehicle_shuts);
				ImGui::Checkbox("gta_vehicle_tire.fxc", &im->m_dbg_ignore_gta_vehicle_tire);
				ImGui::Checkbox("gta_vehicle_vehglass.fxc", &im->m_dbg_ignore_gta_vehicle_vehglass);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("RAGE ..."))
			{
				ImGui::Checkbox("rage_atmoscatt_clouds.fxc", &im->m_dbg_ignore_rage_atmoscatt_clouds);
				ImGui::Checkbox("rage_billboard_nobump.fxc", &im->m_dbg_ignore_rage_billboard_nobump);
				ImGui::Checkbox("rage_bink.fxc", &im->m_dbg_ignore_rage_bink);
				ImGui::Checkbox("rage_default.fxc", &im->m_dbg_ignore_rage_default);
				ImGui::Checkbox("rage_fastmipmap.fxc", &im->m_dbg_ignore_rage_fastmipmap);
				ImGui::Checkbox("rage_im.fxc", &im->m_dbg_ignore_rage_im);
				ImGui::Checkbox("rage_perlinnoise.fxc", &im->m_dbg_ignore_rage_perlinnoise);
				ImGui::Checkbox("rage_postfx.fxc", &im->m_dbg_ignore_rage_postfx);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("RMPTFX ..."))
			{
				ImGui::Checkbox("rmptfx_collision.fxc", &im->m_dbg_ignore_rmptfx_collision);
				ImGui::Checkbox("rmptfx_default.fxc", &im->m_dbg_ignore_rmptfx_default);
				ImGui::Checkbox("rmptfx_litsprite.fxc", &im->m_dbg_ignore_rmptfx_litsprite);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("SHADOW ..."))
			{
				ImGui::Checkbox("shadowSmartBlit.fxc", &im->m_dbg_ignore_shadowSmartBlit);
				ImGui::Checkbox("shadowZ.fxc", &im->m_dbg_ignore_shadowZ);
				ImGui::Checkbox("shadowZDir.fxc", &im->m_dbg_ignore_shadowZDir);
				ImGui::TreePop();
			}

			ImGui::Spacing(0, TREENODE_SPACING);
			if (ImGui::TreeNode("WATER ..."))
			{
				ImGui::Checkbox("water.fxc", &im->m_dbg_ignore_water);
				ImGui::Checkbox("waterTex.fxc", &im->m_dbg_ignore_waterTex);
				ImGui::TreePop();
			}
			ImGui::EndDisabled();

			ImGui::Spacing(0.0f, 4.0f);
		}
	}

	void dev_debug_container()
	{
		const auto& im = imgui::get();
		//const auto& gs = comp_settings::get();

#ifdef LOG_SHADERPRESETS
		if (ImGui::Button("Copy Shader PresetLog to Clipboard", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
		{
			auto log_str = [&]()
				{
					std::string out;
					for (auto& p : im->preset_list) {
						out += p.second + " = " + std::to_string(p.first) + ",\n";
					}
					return out;
				};

			ImGui::LogToClipboard();
			ImGui::LogText("%s", log_str().c_str());
			ImGui::LogFinish();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
#endif

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Statistics ..."))
		{
			im->m_stats.enable_tracking(true);
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			im->m_stats.draw_stats();
			ImGui::TreePop();
		}
		else {
			im->m_stats.enable_tracking(false);
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Disable Functionalities ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::Checkbox("Toggle Shader/FF Rendering (On: Shader)", &im->m_dbg_toggle_ff);
			ImGui::Checkbox("Disable Pixelshader for Static Objects Rendered via FF", &im->m_dbg_disable_ps_for_static);
			TT(	"Remix only grabs VS output when no pixel shader is bound.\n"
				"If this makes a difference somewhere, please report it on Discord or GitHub!")

			ImGui::Checkbox("Skip DrawIndexedPrim Logic", &im->m_dbg_skip_draw_indexed_checks); TT("Disables all checks in DrawIndexedPrim wrapper and renders via Shaders");
			ImGui::Checkbox("Do not restore Drawcall Context", &im->m_dbg_do_not_restore_drawcall_context);
			ImGui::Checkbox("Do not restore Drawcall Context on Early Out", &im->m_dbg_do_not_restore_drawcall_context_on_early_out);

			ImGui::Spacing(0, 4);

			ImGui::Checkbox("Never Cull Statics", &im->m_dbg_never_cull_statics); TT("No distance/radii checks for custom anti culling code.");
			ImGui::Checkbox("Extended Anticull Always TRUE", &im->m_dbg_extended_anticull_always_true); TT("Always return true when extended anticull is on.");
			
			ImGui::SameLine();
			ImGui::Text(" ---- %d tracked, %d marked", im->m_dbg_anticull_offscreen_tracked, im->m_dbg_anticull_offscreen_marked);
			TT("Tracked: anti culling mesh instances found in the building pool.\nMarked: instances within category distance whose map cell got forced this frame (last phase).");

			ImGui::Checkbox("Disable Scene Injection Trigger", &im->m_dbg_disable_scene_injection); TT("Disables the remix injection trigger at the end of the scene phase (behind the 3D markers) and in the pause menu. Remix then injects on the first game UI draw instead (radar background flicker, missing rooms behind api meshes).");

			ImGui::Checkbox("Disable Phone Hack", &im->m_dbg_disable_phone_fixup); TT("Disables phone hack that clears the RT before drawing the first elem on the phone screen. Fixes smearing.");

			
			ImGui::Checkbox("Disable IgnoreBackedLighting Enforcement", &im->m_dbg_disable_ignore_baked_lighting_enforcement);
			TT("CompMod forces the IgnoreBakedLighting category for almost every mesh. This disables that")

			ImGui::Checkbox("Disable Global UV Animations", &im->m_dbg_disable_global_uv_anims);
			ImGui::Checkbox("Disable OMM Override on Alphatested UV Anims", &im->m_dbg_disable_omm_override_on_alpha_uv_anims);
			TT("Disables automatic ignore OMM tagging on alphatested emissive surfaces with animated UVs");

			ImGui::Checkbox("Disable Water Worldpos Logic", &im->m_dbg_disable_water_worldpos_logic);
			ImGui::Checkbox("Disable Water Module", &im->m_dbg_disable_water_module);
			TT("All water module hooks behave like the game: no tessellation changes, far waves, simulation edge fade or block scan changes.");
			ImGui::Checkbox("Disable Water Rectangle Culling", &im->m_dbg_disable_water_rect_culling);
			TT("The water block scan keeps rectangles outside the camera frustum instead of dropping them.");

			//ImGui::Checkbox("Disable Alphablend On VEHGLASS", &im->m_dbg_vehglass_disable_alphablend);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Remix Api: World Meshes ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			renderer_api::devgui();
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Remix Api: Distant Imposters ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			distant_lights::devgui();
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Do not render / render only ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Do not render Static", &im->m_dbg_do_not_render_static);
			ImGui::Checkbox("Only render Static", &im->m_dbg_only_render_static);
			ImGui::Checkbox("Do not render Vehicle", &im->m_dbg_do_not_render_vehicle);
			ImGui::Checkbox("Do not render Instances", &im->m_dbg_do_not_render_instances);
			ImGui::Checkbox("Do not render Stencil 0", &im->m_dbg_do_not_render_stencil_zero);
			ImGui::Checkbox("Do not render Tree Foliage", &im->m_dbg_do_not_render_tree_foliage);
			ImGui::Checkbox("Do not render FX", &im->m_dbg_do_not_render_fx);
			ImGui::Checkbox("Do not render FF", &im->m_dbg_do_not_render_ff);
			ImGui::Checkbox("Do not render Prims with VS", &im->m_dbg_do_not_render_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Indexed Prims with VS", &im->m_dbg_do_not_render_indexed_prims_with_vertexshader);
			ImGui::Checkbox("Do not render Water", &im->m_dbg_do_not_render_water);
			ImGui::Checkbox("Do not render Tri Surfaces", &im->m_dbg_do_not_render_tri_surface);
			ImGui::Checkbox("Do not render Map markers", &im->m_dbg_do_not_render_map_markers);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Light related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Api Lights 3D", &im->m_dbg_visualize_api_lights); TT("Visualize all spawned api lights");
			ImGui::DragFloat("Vis. 3D Light Distance", &im->m_dbg_visualize_api_lights_3d_distance, 0.01f, 1.0f, 50.0f, "%.0f");
			ImGui::Checkbox("Skip Ignore Light Hash Logic", &im->m_dbg_disable_ignore_light_hash_logic); TT("For performance impact testing");

			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Ignore Lights with certain flags ...");
			ImGui::Spacing(0, 4);

			{
				ImGui::Checkbox("Enable Flag Logic", &im->m_dbg_ignore_lights_with_flag_logic);

				ImGui::BeginDisabled(!im->m_dbg_ignore_lights_with_flag_logic);
				{
					static const char* bit_list[] = {
						"Bit 0:  0x1",
						"Bit 1:  0x2",
						"Bit 2:  0x4",
						"Bit 3:  0x8",
						"Bit 4:  0x10",
						"Bit 5:  0x20",
						"Bit 6:  0x40",
						"Bit 7:  0x80",
						"Bit 8:  0x100",
						"Bit 9:  0x200",
						"Bit 10: 0x400",
						"Bit 11: 0x800",
						"Bit 12: 0x1000",
						"Bit 13: 0x2000",
						"Bit 14: 0x4000",
						"Bit 15: 0x8000",
						"Bit 16: 0x10000"
					};
					static const int num_bit_list = std::size(bit_list);

					uint32_t current_selection_flag1 = im->m_dbg_ignore_lights_with_flag_01;
					if (ImGui::BeginCombo("Ignore Lights with Flag:", bit_list[current_selection_flag1]))
					{
						for (auto i = 0u; i < num_bit_list; ++i)
						{
							const bool is_selected = (current_selection_flag1 == i);
							if (ImGui::Selectable(bit_list[i], is_selected)) {
								current_selection_flag1 = i;
							}

							if (is_selected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					if (current_selection_flag1 != (uint32_t)im->m_dbg_ignore_lights_with_flag_01) {
						im->m_dbg_ignore_lights_with_flag_01 = static_cast<int>(current_selection_flag1);
					}

					ImGui::Checkbox("ADD Second Flag", &im->m_dbg_ignore_lights_with_flag_add_second_flag);
					TT("This will add a second flag the light has to have to ignore it.\n"
						"In short, both flags have to be set for a light to be ignored.")

						ImGui::BeginDisabled(!im->m_dbg_ignore_lights_with_flag_add_second_flag);
					{
						uint32_t current_selection_flag2 = im->m_dbg_ignore_lights_with_flag_02;
						if (ImGui::BeginCombo("Additional Flag:", bit_list[current_selection_flag2]))
						{
							for (auto i = 0u; i < num_bit_list; ++i)
							{
								const bool is_selected = (current_selection_flag2 == i);
								if (ImGui::Selectable(bit_list[i], is_selected)) {
									current_selection_flag2 = i;
								}

								if (is_selected) {
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}

						if (current_selection_flag2 != (uint32_t)im->m_dbg_ignore_lights_with_flag_02) {
							im->m_dbg_ignore_lights_with_flag_02 = static_cast<int>(current_selection_flag2);
						}

						ImGui::EndDisabled();
					}

					ImGui::EndDisabled();
				}
			}

			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Directional Light Info");
			ImGui::Spacing(0, 4);

			ImGui::Checkbox("Force Directional Light Translation", &im->m_dbg_force_distant_light_translation);
			TT("Force Translation even when Remix Atmosphere System is active.");

			for (auto i = 0u; i < 2; i++)
			{
				auto& def = game::g_directionalLights[i];
				ImGui::Text("Directional Light #%d", i);
				ImGui::Text("mDirection: [%.2f, %.2f, %.2f]", def.mDirection.x, def.mDirection.y, def.mDirection.z);
				ImGui::Text("mColor: [%.2f, %.2f, %.2f, %.2f]", def.mColor.x, def.mColor.y, def.mColor.z, def.mColor.w);
				ImGui::Text("mIntensity: [%.2f]", def.mIntensity);
				ImGui::Spacing(0, 4);
				ImGui::Separator();
				ImGui::Spacing(0, 4);
			}

			ImGui::Spacing(0, 8);
			ImGui::SeparatorText("Fun");
			ImGui::Spacing(0, 4);

			ImGui::Checkbox("Enable Custom Vehicle Headlight Color", &im->m_dbg_custom_veh_headlight_enabled);
			ImGui::BeginDisabled(!im->m_dbg_custom_veh_headlight_enabled);
			{
				ImGui::ColorEdit3("Custom Vehicle Headlight Color", &im->m_dbg_custom_veh_headlight_color.x);
				ImGui::EndDisabled();
			}

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Emissive Related"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SliderInt("Tag EmissiveNight Surfaces as Category ..", &im->m_dbg_tag_static_emissive_as_index, -1, (int)InstanceCategories::Count, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Checkbox("FF Emissive: Enable Alphablend on non alpha Emissives", &im->m_dbg_emissive_ff_with_alphablend);
			ImGui::Checkbox("FF Emissive: Tag as WorldUI + IgnoreTransparencyLayer", &im->m_dbg_emissive_ff_worldui_ignore_alpha);

			ImGui::Checkbox("Render Emissives using Shaders", &im->m_dbg_render_emissives_with_shaders);
			ImGui::Checkbox("Tag Emissives drawn by Shaders as Decal/WorldUI", &im->m_dbg_render_emissives_with_shaders_tag_as_decal);

			ImGui::Checkbox("Do Not Render FF Emissives", &im->m_dbg_emissive_ff_do_not_render);
			ImGui::Checkbox("Do Not Render FF Alphablended Emissives", &im->m_dbg_emissive_ff_alphablend_do_not_render);

			ImGui::Checkbox("FF Alphablended Emissives Assign IgnoreTransparency", &im->m_dbg_emissive_ff_alphablend_test1);
			ImGui::Checkbox("FF Alphablended Emissives Alphablend", &im->m_dbg_emissive_ff_alphablend_enable_alphablend);

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Emissive Intensity Debug Switches");

			ImGui::Widget_CategoryWithVerticalLabel("Renderer", [&]() 
			{
				ImGui::PushID("intens");
				ImGui::Checkbox("Disable Veh SwitchOn", &im->m_dbg_emissive_disable_veh_switch_on);
				ImGui::Checkbox("Disable OnEmissiveMultiplier", &im->m_dbg_emissive_disable_on_emissive_multi);
				ImGui::Checkbox("Disable PsConst72 VehLightsEmissive", &im->m_dbg_emissive_disable_const77_lightsemissive);
				ImGui::Checkbox("Disable PsConst66 EmissiveNight", &im->m_dbg_emissive_disable_const66_emissivenight);
				ImGui::Checkbox("Disable PsConst66 Default", &im->m_dbg_emissive_disable_const66_default);
				ImGui::Checkbox("Disable PsConst66 EmissiveStrong", &im->m_dbg_emissive_disable_const66_emissivestrong);
				ImGui::Checkbox("Disable PsConst51 TFACTOR EmsMulti", &im->m_dbg_emissive_disable_const51_tfactor_ems_multi);
				ImGui::PopID();
			});

			ImGui::Spacing(0, SEPARATOR_SPACING);

			ImGui::Widget_CategoryWithVerticalLabel("Renderer FF", [&]()
				{
					ImGui::PushID("intensFF");
					ImGui::Checkbox("Disable FF EmissiveNight (Night)", &im->m_dbg_emissive_disable_ff_emissivenight_nighttime);
					ImGui::Checkbox("Disable FF EmissiveNight (Day 0)", &im->m_dbg_emissive_disable_ff_emissivenight_daytime);
					ImGui::Checkbox("Disable FF NOT EmissiveNight", &im->m_dbg_emissive_disable_ff_not_emissivenight);
					ImGui::Checkbox("Disable AlphaFF", &im->m_dbg_emissive_disable_alpha_ff);
					ImGui::PopID();
				});

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Visualizations / Rendering related ..."))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::Checkbox("Visualize Decal Renderstates", &im->m_dbg_visualize_decal_renderstates); TT("Visualize renderstates of nearby decal surfaces.");
			ImGui::Checkbox("Visualize Stencil States", &im->m_dbg_visualize_stencil_state); TT("Visualize stencil states of nearby surfaces.");
			ImGui::DragFloat("Visualize Dist", &im->m_dbg_visualize_distance, 0.1f);

			if (static auto debug_model_hash = shared::common::flags::has_flag("debug_model_hash"); debug_model_hash)
			{
				ImGui::Spacing(0, 4);

				ImGui::Checkbox("Visualize Model Hashes", &im->m_dbg_visualize_model_hashes_info);
				ImGui::DragFloat("Visualize Model Hashes Dist", &im->m_dbg_visualize_model_hashes_distance, 0.1f);
				ImGui::DragFloat("Visualize Model Hashes Min Radius", &im->m_dbg_visualize_model_hashes_min_radius, 0.1f);
				ImGui::DragFloat("Visualize Model Hashes Max Radius", &im->m_dbg_visualize_model_hashes_max_radius, 0.1f);
			}

			ImGui::Spacing(0, 4);
			ImGui::SliderInt("Tag Exp Hair Surfaces as Category ..", &im->m_dbg_tag_exp_hair_as_index, -1, 23, "%d", ImGuiSliderFlags_AlwaysClamp);

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Timecycle related ..."))
		{
			const auto bridge = shared::common::remix_api::get().m_bridge;
			constexpr float end_padding = 12.0f;
			const float width = ImGui::GetContentRegionAvail().x - end_padding;
			const float half_width = width * 0.5f;

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Game weather and clock");

			const auto weather_name = [](const game::eWeatherType* type) -> const char* {
				return type && *type < game::WEATHER_NONE ? game::eWeatherTypeStr[*type] + 8 : "unknown"; // without the WEATHER_ prefix
			};

			ImGui::Text("Weather: %s -> %s, transition %.2f", weather_name(game::weather_type_prev), weather_name(game::weather_type_new),
				game::weather_change_value ? *game::weather_change_value : 0.0f);
			ImGui::Text("Clock: %02d:%02d", *game::m_game_clock_hours, *game::m_game_clock_minutes);

			ImGui::SetNextItemWidth(half_width);
			ImGui::SliderInt("Timecycle used for the Remix translation", &im->m_dbg_used_timecycle, -1, 2, "%d", ImGuiSliderFlags_AlwaysClamp);
			TT("Sets the Timecycle to be used to translate its settings to fitting remix variables.\n"
				"-1: No override\n0: Timecycle 1 (World/Interior)\n1: Timecycle 2 (World/Interior)\n3: Timecycle 3 (Cutscenes)");

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Remix atmos system");

			ImGui::Checkbox("Manual mode", &im->m_dbg_manual_atmos_system);
			TT("The mod stops driving the atmos preset and blend, the controls below set them directly.");

			ImGui::BeginDisabled(!im->m_dbg_manual_atmos_system);
			{
				/* clear, partlyCloudy, overcast, hazy, foggy, drizzle, rainstorm, thunderstorm, snow, blizzard,
				 * sandstorm, smoggy. Empty string to return to dormant. */

				struct preset_s { const char* name; const char* game_weather; };
				constexpr preset_s PRESETS[] =
				{
					{ "clear", "EXTRASUNNY" }, { "smoggy", "SUNNY" }, { "partlyCloudy", "SUNNY_WINDY" }, { "overcast", "CLOUDY" },
					{ "drizzle", "DRIZZLE" }, { "foggy", "FOGGY" }, { "rainstorm", "RAIN" }, { "thunderstorm", "LIGHTNING" },
				};

				const float preset_width = (width - ImGui::GetStyle().ItemSpacing.x * 3.0f) * 0.25f;
				for (std::size_t i = 0u; i < std::size(PRESETS); i++)
				{
					if (i % 4u) {
						ImGui::SameLine();
					}

					if (ImGui::Button(PRESETS[i].name, ImVec2(preset_width, 0.0f))) {
						bridge.SetGameValue("__weather.target", PRESETS[i].name);
					}
					TT(PRESETS[i].game_weather);
				}

				ImGui::Spacing(0, TREENODE_SPACING_INSIDE);

				static float trans_val = 1.0f;
				ImGui::SetNextItemWidth(half_width);
				ImGui::SliderFloat("##blend_absolute", &trans_val, 0.0f, 1.0f, "absolute blend %.2f");
				ImGui::SameLine();
				if (ImGui::Button("Apply##absolute")) {
					bridge.SetGameValue("__weather.blend_absolute", std::to_string(trans_val).c_str());
				}

				ImGui::SameLine();
				if (ImGui::Button("Disable absolute mode"))
				{
					bridge.SetGameValue("__weather.blend_absolute", "-1");
					trans_val = -1.0f;
				}

				static float trans_blend_val = 1.0f;
				ImGui::SetNextItemWidth(half_width);
				ImGui::SliderFloat("##blend_seconds", &trans_blend_val, 0.0f, 10.0f, "transition %.1f s");
				ImGui::SameLine();
				if (ImGui::Button("Apply##seconds"))
				{
					bridge.SetGameValue("__weather.blend_absolute", "-1.0");
					bridge.SetGameValue("__weather.blend_seconds", std::to_string(trans_blend_val).c_str());
				}

				ImGui::EndDisabled();
			}

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			if (ImGui::BeginTable("##atmos_state", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX, ImVec2(width, 0.0f)))
			{
				static char str_buff[512] = {};
				static uint32_t str_len = 0u;
				constexpr const char* VALUES[] = { "__weather.current", "__weather.target", "__weather.previous", "__weather.blend_absolute", "__weather.blend_progress" };

				for (const auto value : VALUES)
				{
					bridge.GetGameValue(value, str_buff, ARRAYSIZE(str_buff), &str_len);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(value);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(str_buff);
				}

				ImGui::EndTable();
			}

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Atmos sun");
			game_lights::devgui_atmos_sun();

			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::SeparatorText("Wetness");

			ImGui::Checkbox("Override global wetness", &im->m_dbg_global_wetness_override);
			ImGui::SameLine();
			ImGui::BeginDisabled(!im->m_dbg_global_wetness_override);
			ImGui::SetNextItemWidth(half_width);
			ImGui::SliderFloat("##global_wetness", &im->m_dbg_global_wetness, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::EndDisabled();

			ImGui::Checkbox("Occlusion test allowed on ignored surfaces", &im->m_dbg_global_wetness_occl_allow_on_ignored);
			ImGui::SetNextItemWidth(half_width);
			ImGui::DragFloat("Rain marker height offset", &im->m_dbg_global_wetness_rain_marker_height_offset, 0.05f, -100.0f, 100.0f, "%.0f");

			const auto& wet = timecycle::get_wetness_state();
			if (ImGui::BeginTable("##wetness_state", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX, ImVec2(width, 0.0f)))
			{
				const auto row = [](const char* name, const std::string& value)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(name);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(value.c_str());
				};

				row("Game wetness", std::format("{:.3f}", wet.game));
				row("Effective wetness", std::format("{:.3f}", wet.effective));
				row("Phase", wet.phase == timecycle::wetness_phase::after_rain ? "After rain" : "Follow game");
				row("After-rain level (peak)", std::format("{:.3f} ({:.3f})", wet.level, wet.peak));
				row("After-rain timer", std::format("{:.1f} s", wet.timer));
				row("Ripple strength", std::format("{:.4f}", wet.ripple_strength));
				row("Vehicle wetness", std::format("{:.3f}", wet.vehicle));
				ImGui::EndTable();
			}

			if (ImGui::Button("Reset after-rain wetness")) {
				timecycle::reset_wetness();
			}

			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Temp Debug Values"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat3("Debug Vector", &im->m_debug_vector.x, 0.01f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 2", &im->m_debug_vector2.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 3", &im->m_debug_vector3.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 4", &im->m_debug_vector4.x, 0.1f, 0, 0, "%.6f");
			ImGui::DragFloat3("Debug Vector 5", &im->m_debug_vector5.x, 0.1f, 0, 0, "%.6f");

			ImGui::Checkbox("Debug Bool 1", &im->m_dbg_debug_bool01);
			ImGui::Checkbox("Debug Bool 2", &im->m_dbg_debug_bool02);
			ImGui::Checkbox("Debug Bool 3", &im->m_dbg_debug_bool03);
			ImGui::Checkbox("Debug Bool 4", &im->m_dbg_debug_bool04);
			ImGui::Checkbox("Debug Bool 5", &im->m_dbg_debug_bool05);
			ImGui::Checkbox("Debug Bool 6", &im->m_dbg_debug_bool06);
			ImGui::Checkbox("Debug Bool 7", &im->m_dbg_debug_bool07);
			ImGui::Checkbox("Debug Bool 8", &im->m_dbg_debug_bool08);
			ImGui::Checkbox("Debug Bool 9", &im->m_dbg_debug_bool09);

			ImGui::DragInt("Debug Int 1", &im->m_dbg_int_01, 0.01f);
			ImGui::DragInt("Debug Int 2", &im->m_dbg_int_02, 0.01f);
			ImGui::DragInt("Debug Int 3", &im->m_dbg_int_03, 0.01f);
			ImGui::DragInt("Debug Int 4", &im->m_dbg_int_04, 0.01f);
			ImGui::DragInt("Debug Int 5", &im->m_dbg_int_05, 0.01f);
			
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Phone Debug"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Phone Proj Offset Row0", im->m_dbg_phone_projection_matrix_offset.m[0], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row1", im->m_dbg_phone_projection_matrix_offset.m[1], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row2", im->m_dbg_phone_projection_matrix_offset.m[2], 0.01f);
			ImGui::DragFloat4("##Phone Proj Offset Row3", im->m_dbg_phone_projection_matrix_offset.m[3], 0.01f);

			ImGui::Spacing(0, 8);
			ImGui::Separator();

			ImGui::ColorEdit3("Phone Clear Color", &im->m_dbg_phone_clear_hack_color.x, ImGuiColorEditFlags_InputRGB);
			TT("Color used for the clear hack used on phones");

			ImGui::Spacing(0, 8);
			if (ImGui::Button("Log Rasterized Phone Draws")) {
				im->m_dbg_phone_log_draws = true;
			} TT("Logs shaders, shader constants, textures and render states of the phone draws of the next frame. Needs 'phone_mode' 1 and the phone on screen.");


			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Offset Matrix 2"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Debug Mtx02 Row0", im->m_debug_mtx02.m[0], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row1", im->m_debug_mtx02.m[1], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row2", im->m_debug_mtx02.m[2], 0.01f);
			ImGui::DragFloat4("##Debug Mtx02 Row3", im->m_debug_mtx02.m[3], 0.01f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Debug Offset Matrix 3"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			ImGui::DragFloat4("##Debug Mtx03 Row0", im->m_debug_mtx03.m[0], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row1", im->m_debug_mtx03.m[1], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row2", im->m_debug_mtx03.m[2], 0.01f);
			ImGui::DragFloat4("##Debug Mtx03 Row3", im->m_debug_mtx03.m[3], 0.01f);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::TreeNode("Custom ImGui Colors"))
		{
			ImGui::Spacing(0, TREENODE_SPACING_INSIDE);
			const auto coloredit_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_Float;

			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonGreen", &im->ImGuiCol_ButtonGreen.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonYellow", &im->ImGuiCol_ButtonYellow.x, coloredit_flags);
			SET_CHILD_WIDGET_WIDTH_MAN(140.0f); ImGui::ColorEdit4("ButtonRed", &im->ImGuiCol_ButtonRed.x, coloredit_flags);
			ImGui::TreePop();
		}

		ImGui::Spacing(0, TREENODE_SPACING);
	}

#if DEBUG
	void dev_other_container()
	{
		static const auto& im = imgui::get();

		ImGui::Spacing(0, TREENODE_SPACING);
		if (ImGui::Button("Timecycle Vars - Debug Single Frame", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			im->m_dbg_debug_single_frame_timecycle_remix_vars = true;
		}

		ImGui::Spacing(0.0f, 4.0f);
		if (ImGui::Button("Emissive Intensity - Debug Single Frame", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			im->m_dbg_debug_single_frame_emissive_intensity_vars = true;
		}

		//ImGui::Checkbox("Log Shader Float Constants", &im->m_dbg_debug_shader_float_constants);
		ImGui::Spacing(0.0f, 4.0f);
	}
#endif

	void pages::debug()
	{
		group("Shaders", ICON_FA_ELLIPSIS_H, dev_shader_container);
		group("DEBUG Section", ICON_FA_TERMINAL, dev_debug_container, true);
#if DEBUG
		group("Other Settings", ICON_FA_MEMORY, dev_other_container);
#endif
	}
}
