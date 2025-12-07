#pragma once
#include "shared/imgui/imgui_helper.hpp"

namespace gta4
{
	class imgui final : public shared::common::loader::component_module
	{
	public:
		imgui();

		static inline imgui* p_this = nullptr;
		static imgui* get() { return p_this; }

		static void on_present();

		void draw_debug();
		void devgui();
		bool input_message(UINT message_type, WPARAM wparam, LPARAM lparam);

		bool m_initialized_device = false;

		void style_xo();

		ImVec4 ImGuiCol_ButtonGreen = ImVec4(0.3f, 0.4f, 0.05f, 0.7f);
		ImVec4 ImGuiCol_ButtonYellow = ImVec4(0.4f, 0.3f, 0.1f, 0.8f);
		ImVec4 ImGuiCol_ButtonRed = ImVec4(0.48f, 0.15f, 0.15f, 1.00f);
		ImVec4 ImGuiCol_ContainerBackground = ImVec4(0.220f, 0.220f, 0.220f, 0.863f);
		ImVec4 ImGuiCol_ContainerBorder = ImVec4(0.099f, 0.099f, 0.099f, 0.901f);

		bool m_do_not_pause_on_lost_focus = false;
		bool m_do_not_pause_on_lost_focus_changed = false;

		bool m_screenshot_mode = false;
		bool m_screenshot_mode_hide_player = true;
		bool m_freecam_mode = false;
		float m_freecam_fwd_speed = 0.5f;
		float m_freecam_rt_speed = 0.2f;
		float m_freecam_up_speed = 0.2f;
		float m_freecam_up_offset = -0.004f;

		Vector m_debug_vector = { 0.0f, 0.0f, 0.0f };
		Vector m_debug_vector2 = { 0.0f, 0.0f, 0.0f };

		Vector m_debug_vector3 = { 0.0f, 0.0f, 0.0f };
		Vector m_debug_vector4 = { 0.0f, 0.0f, 0.0f };
		Vector m_debug_vector5 = { 0.0f, 0.0f, 0.0f };

		D3DXMATRIX m_dbg_phone_projection_matrix_offset;
		D3DXMATRIX m_debug_mtx02;
		D3DXMATRIX m_debug_mtx03;

		bool m_dbg_debug_bool01 = false;
		bool m_dbg_debug_bool02 = false;
		bool m_dbg_debug_bool03 = false;
		bool m_dbg_debug_bool04 = false;
		bool m_dbg_debug_bool05 = false;
		int m_dbg_int_01 = 0;
		int m_dbg_int_02 = 0;

		bool m_dbg_only_render_static = false;
		bool m_dbg_do_not_render_static = false;
		bool m_dbg_do_not_render_vehicle = false;
		bool m_dbg_do_not_render_instances = false;
		bool m_dbg_do_not_render_stencil_zero = false;
		bool m_dbg_do_not_render_tree_foliage = false;
		bool m_dbg_do_not_render_fx = false;
		bool m_dbg_do_not_render_ff = false;
		bool m_dbg_do_not_render_prims_with_vertexshader = false;
		bool m_dbg_do_not_render_indexed_prims_with_vertexshader = false;
		bool m_dbg_do_not_render_water = false;
		bool m_dbg_do_not_render_tri_surface = false;
		bool m_dbg_toggle_ff = false;
		bool m_dbg_do_not_restore_drawcall_context = false;
		bool m_dbg_do_not_restore_drawcall_context_on_early_out = false;
		bool m_dbg_disable_ps_for_static = false;
		int m_dbg_tag_static_emissive_as_index = -1;
		bool m_dbg_emissive_ff_with_alphablend = false;
		bool m_dbg_emissive_ff_worldui_ignore_alpha = true;
		bool m_dbg_emissive_ff_do_not_render = false;
		bool m_dbg_emissive_ff_alphablend_do_not_render = false;
		bool m_dbg_emissive_ff_alphablend_test1 = false;
		bool m_dbg_emissive_ff_alphablend_enable_alphablend = true;
		bool m_dbg_render_emissives_with_shaders = false; // was game setting 'render_emissive_surfaces_using_shaders'
		bool m_dbg_render_emissives_with_shaders_tag_as_decal = true; // was game setting 'assign_decal_category_to_emissive_surfaces'



		bool m_dbg_skip_draw_indexed_checks = false;
		bool m_dbg_disable_ignore_baked_lighting_enforcement = false;
		bool m_dbg_never_cull_statics = false;
		bool m_dbg_extended_anticull_always_true = false;
		bool m_dbg_disable_hud_fixup = false;
		bool m_dbg_disable_global_uv_anims = false;
		bool m_dbg_disable_omm_override_on_alpha_uv_anims = false;

		int m_dbg_tag_exp_hair_as_index = -1;

		int m_dbg_used_timecycle = -1;
		bool m_dbg_debug_single_frame_timecycle_remix_vars = false;

		bool m_dbg_global_wetness_override = false;
		float m_dbg_global_wetness = 0.0f;

		bool m_dbg_custom_veh_headlight_enabled = false;
		Vector m_dbg_custom_veh_headlight_color = { 1.0f, 0.0f, 0.0f };

		// --

		std::unordered_map<int, std::string> preset_list;

		bool m_dbg_visualize_api_lights = false;
		bool m_dbg_visualize_api_light_hashes = false;
		bool m_dbg_visualize_api_light_unstable_hashes = false; // enabling this will also show unstable hashes
		bool m_dbg_disable_ignore_light_hash_logic = false; // disables the map_settings logic that ignores light translation based on a list of hashes
		float m_dbg_visualize_api_light_hashes_distance = 8.0f;

		bool m_dbg_visualize_decal_renderstates = false;

		struct visualized_decal_rs_s
		{
			Vector pos;
			bool rs_alpha_blending = false;
			std::string_view rs_blendop;
			std::string_view rs_srcblend;
			std::string_view rs_destblend;
			std::string_view tss_alphaop;
			std::string_view tss_alphaarg1;
			std::string_view tss_alphaarg2;
		};
		std::vector<visualized_decal_rs_s> visualized_decal_renderstates;

		// --

		float m_dbg_visualize_anti_cull_info_distance = 80.0f;
		float m_dbg_visualize_anti_cull_info_min_radius = 16.0f;
		float m_dbg_visualize_anti_cull_info_highlight_line_width = 0.25f;
		int m_dbg_visualize_anti_cull_highlight = 0;
		bool m_dbg_visualize_anti_cull_info = false;

		struct visualized_anti_cull_s
		{
			Vector pos;
			float radius = 0.0f;
			float height = 0.0f;
			int m_wModelIndex = 0;
			Vector mins;
			Vector maxs;
			bool draw_debug_box = false;
			bool forced_visible = true;
		};
		std::vector<visualized_anti_cull_s> visualized_anti_cull;
		std::mutex visualized_anti_cull_mutex;

		// --

		bool m_dbg_ignore_lights_with_flag_logic = false;
		int m_dbg_ignore_lights_with_flag_01 = 0;
		bool m_dbg_ignore_lights_with_flag_add_second_flag = false;
		int m_dbg_ignore_lights_with_flag_02 = 0;

		struct visualized_api_light_s
		{
			uint64_t hash = 0u;
			Vector pos;
			game::CLightSource m_def_copy;
			bool ignored = false; // ignored via map_settings
			bool allowed_filler = false; // allowed via map_settings (only used with ignore filler lights option)
			bool is_filler = false;
			std::uint32_t m_updateframe = 0u; // increases when light hash was found in the current frame
			std::uint32_t m_frames_since_addition = 0u; // increases on each frame
		};

		std::vector<visualized_api_light_s> visualized_api_lights;

		// --

		float m_timecyc_curr_mSkyLightMultiplier = 0.0f;
		float m_timecyc_curr_mSkyLightMultiplier_final = 0.0f;
		float m_timecyc_curr_mBloomIntensity = 0.0f;
		float m_timecyc_curr_mBloomIntensity_final = 0.0f;
		float m_timecyc_curr_mBloomThreshold = 0.0f;
		float m_timecyc_curr_mBloomThreshold_final = 0.0f;
		float m_timecyc_curr_mTemperature = 0.0f;
		Vector m_timecyc_curr_mTemperature_offset;

		Vector4D m_timecyc_curr_mColorCorrection;
		Vector4D m_timecyc_curr_mColorCorrection_final;
		float m_timecyc_curr_mDesaturation = 0.0f;
		float m_timecyc_curr_mDesaturation_final = 0.0f;
		float m_timecyc_curr_mDesaturationFar = 0.0f;
		float m_timecyc_curr_mDesaturationFar_offset = 0.0f;

		float m_timecyc_curr_mGamma = 0.0f;
		float m_timecyc_curr_mGamma_final = 0.0f;
		Vector4D m_timecyc_curr_mSkyBottomColorFogDensity;

		Vector m_timecyc_curr_singleScatteringAlbedo;
		float m_timecyc_curr_mSkyHorizonHeight = 0.0f;
		float m_timecyc_curr_mSkyHorizonHeight_final = 0.0f;
		float m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters = 0.0f;
		
		// --

		bool m_dbg_enable_ignore_shader_logic = false;
		bool m_dbg_ignore_all = false;
		bool m_dbg_ignore_drawprimitive = false;
		bool m_dbg_ignore_cascade = false;
		bool m_dbg_ignore_deferred_lighting = false;
		bool m_dbg_ignore_gpuptfx_simplerender = false;
		bool m_dbg_ignore_gta_atmoscatt_clouds = false;
		bool m_dbg_ignore_gta_cubemap_reflect = false;
		bool m_dbg_ignore_gta_cutout_fence = false;
		bool m_dbg_ignore_gta_decal = false;
		bool m_dbg_ignore_gta_decal_amb_only = false;
		bool m_dbg_ignore_gta_decal_dirt = false;
		bool m_dbg_ignore_gta_decal_glue = false;
		bool m_dbg_ignore_gta_decal_normal_only = false;
		bool m_dbg_ignore_gta_default = false;
		bool m_dbg_ignore_gta_diffuse_instance = false;
		bool m_dbg_ignore_gta_emissive = false;
		bool m_dbg_ignore_gta_emissivenight = false;
		bool m_dbg_ignore_gta_emissivestrong = false;
		bool m_dbg_ignore_gta_glass = false;
		bool m_dbg_ignore_gta_glass_emissive = false;
		bool m_dbg_ignore_gta_glass_emissivenight = false;
		bool m_dbg_ignore_gta_glass_normal_spec_reflect = false;
		bool m_dbg_ignore_gta_glass_reflect = false;
		bool m_dbg_ignore_gta_glass_spec = false;
		bool m_dbg_ignore_gta_grass = false;
		bool m_dbg_ignore_gta_hair_sorted_alpha = false;
		bool m_dbg_ignore_gta_hair_sorted_alpha_exp = false;
		bool m_dbg_ignore_gta_im = false;
		bool m_dbg_ignore_gta_normal = false;
		bool m_dbg_ignore_gta_normal_cubemap_reflect = false;
		bool m_dbg_ignore_gta_normal_decal = false;
		bool m_dbg_ignore_gta_normal_reflect = false;
		bool m_dbg_ignore_gta_normal_reflect_alpha = false;
		bool m_dbg_ignore_gta_normal_reflect_decal = false;
		bool m_dbg_ignore_gta_normal_spec = false;
		bool m_dbg_ignore_gta_normal_spec_cubemap_reflect = false;
		bool m_dbg_ignore_gta_normal_spec_decal = false;
		bool m_dbg_ignore_gta_normal_spec_reflect = false;
		bool m_dbg_ignore_gta_normal_spec_reflect_decal = false;
		bool m_dbg_ignore_gta_normal_spec_reflect_emissive = false;
		bool m_dbg_ignore_gta_normal_spec_reflect_emissivenight = false;
		bool m_dbg_ignore_gta_parallax = false;
		bool m_dbg_ignore_gta_parallax_specmap = false;
		bool m_dbg_ignore_gta_parallax_steep = false;
		bool m_dbg_ignore_gta_ped = false;
		bool m_dbg_ignore_gta_ped_face = false;
		bool m_dbg_ignore_gta_ped_reflect = false;
		bool m_dbg_ignore_gta_ped_skin = false;
		bool m_dbg_ignore_gta_ped_skin_blendshape = false;
		bool m_dbg_ignore_gta_projtex = false;
		bool m_dbg_ignore_gta_projtex_steep = false;
		bool m_dbg_ignore_gta_radar = false;
		bool m_dbg_ignore_gta_reflect = false;
		bool m_dbg_ignore_gta_reflect_decal = false;
		bool m_dbg_ignore_gta_rmptfx_gpurender = false;
		bool m_dbg_ignore_gta_rmptfx_litsprite = false;
		bool m_dbg_ignore_gta_rmptfx_mesh = false;
		bool m_dbg_ignore_gta_rmptfx_raindrops = false;
		bool m_dbg_ignore_gta_spec = false;
		bool m_dbg_ignore_gta_spec_decal = false;
		bool m_dbg_ignore_gta_spec_reflect = false;
		bool m_dbg_ignore_gta_spec_reflect_decal = false;
		bool m_dbg_ignore_gta_terrain_va_2lyr = false;
		bool m_dbg_ignore_gta_terrain_va_3lyr = false;
		bool m_dbg_ignore_gta_terrain_va_4lyr = false;
		bool m_dbg_ignore_gta_trees = false;
		bool m_dbg_ignore_gta_trees_extended = false;
		bool m_dbg_ignore_gta_vehicle_badges = false;
		bool m_dbg_ignore_gta_vehicle_basic = false;
		bool m_dbg_ignore_gta_vehicle_chrome = false;
		bool m_dbg_ignore_gta_vehicle_disc = false;
		bool m_dbg_ignore_gta_vehicle_generic = false;
		bool m_dbg_ignore_gta_vehicle_interior = false;
		bool m_dbg_ignore_gta_vehicle_interior2 = false;
		bool m_dbg_ignore_gta_vehicle_lightsemissive = false;
		bool m_dbg_ignore_gta_vehicle_mesh = false;
		bool m_dbg_ignore_gta_vehicle_paint1 = false;
		bool m_dbg_ignore_gta_vehicle_paint2 = false;
		bool m_dbg_ignore_gta_vehicle_paint3 = false;
		bool m_dbg_ignore_gta_vehicle_rims1 = false;
		bool m_dbg_ignore_gta_vehicle_rims2 = false;
		bool m_dbg_ignore_gta_vehicle_rims3 = false;
		bool m_dbg_ignore_gta_vehicle_rubber = false;
		bool m_dbg_ignore_gta_vehicle_shuts = false;
		bool m_dbg_ignore_gta_vehicle_tire = false;
		bool m_dbg_ignore_gta_vehicle_vehglass = false;
		bool m_dbg_ignore_gta_wire = false;
		bool m_dbg_ignore_mirror = false;
		bool m_dbg_ignore_rage_atmoscatt_clouds = false;
		bool m_dbg_ignore_rage_billboard_nobump = false;
		bool m_dbg_ignore_rage_bink = false;
		bool m_dbg_ignore_rage_default = false;
		bool m_dbg_ignore_rage_fastmipmap = false;
		bool m_dbg_ignore_rage_im = false;
		bool m_dbg_ignore_rage_perlinnoise = false;
		bool m_dbg_ignore_rage_postfx = false;
		bool m_dbg_ignore_rmptfx_collision = false;
		bool m_dbg_ignore_rmptfx_default = false;
		bool m_dbg_ignore_rmptfx_litsprite = false;
		bool m_dbg_ignore_shadowSmartBlit = false;
		bool m_dbg_ignore_shadowZ = false;
		bool m_dbg_ignore_shadowZDir = false;
		bool m_dbg_ignore_water = false;
		bool m_dbg_ignore_waterTex = false;

		// -----

		class ImGuiStats
		{
		private:
			static inline bool m_tracking_enabled = false;

		public:
			bool is_tracking_enabled() const {
				return m_tracking_enabled;
			}

			void enable_tracking(const bool state)
			{
				// reset stats once when tracking gets disabled
				if (!state && m_tracking_enabled) {
					this->reset_stats();
				}

				m_tracking_enabled = state;
			}

			class StatObj
			{
			public:
				enum class Mode
				{
					Single = 0,
					ConditionalCheck = 1,
				};

				StatObj(Mode mode) : m_mode(mode) {};

				auto& get_mode() const { return m_mode; }
				auto& get_total() const { return m_num_total; }
				auto& get_successful() const { return m_num_successful; }

				void track_single()
				{
					if (m_tracking_enabled) {
						++m_num_total;
					}
				}

				bool track_check(const bool is_success = false)
				{
					if (m_tracking_enabled) 
					{
						if (!is_success) {
							++m_num_total;
						}
						else {
							++m_num_successful;
						}
					}

					return true;
				}

				void reset()
				{
					m_num_total = 0u;
					m_num_successful = 0u;
				}

				Mode m_mode = Mode::Single;

			private:
				std::uint32_t m_num_total{ 0 };
				std::uint32_t m_num_successful{ 0 };
			};

			StatObj _water_shader_name_checks = { StatObj::Mode::ConditionalCheck };
			StatObj _gta_rmptfx_litsprite_shader_name_checks = { StatObj::Mode::ConditionalCheck };

			StatObj _drawcall_prim = { StatObj::Mode::Single };
			StatObj _drawcall_prim_incl_ignored = { StatObj::Mode::Single };
			StatObj _drawcall_using_vs = { StatObj::Mode::Single };

			StatObj _drawcall_indexed_prim{ StatObj::Mode::Single };
			StatObj _drawcall_indexed_prim_incl_ignored = { StatObj::Mode::Single };

			StatObj _drawcall_indexed_prim_using_vs = { StatObj::Mode::Single };


			ImGuiStats()
			{
				m_stat_list.emplace_back("Water Shader Name Checks", &_water_shader_name_checks);
				m_stat_list.emplace_back("RMPTFX Litsprite Shader Name Checks", &_gta_rmptfx_litsprite_shader_name_checks);

				m_stat_list.emplace_back();
				m_stat_list.emplace_back("DrawPrim Calls", &_drawcall_prim);
				m_stat_list.emplace_back("DrawPrim +Ignored", &_drawcall_prim_incl_ignored);
				m_stat_list.emplace_back("DrawPrim VS", &_drawcall_using_vs);

				m_stat_list.emplace_back();
				m_stat_list.emplace_back("DrawIndexedPrim Calls", &_drawcall_indexed_prim);
				m_stat_list.emplace_back("DrawIndexedPrim +Ignored", &_drawcall_indexed_prim_incl_ignored);
				m_stat_list.emplace_back("DrawIndexedPrim VS", &_drawcall_indexed_prim_using_vs);
			}

			void draw_stats();

			void reset_stats()
			{
				for (auto& p : m_stat_list) 
				{
					if (p.second) {
						p.second->reset();
					}
				}
			}

		private:
			std::vector<std::pair<const char*, StatObj*>> m_stat_list;

			void display_single_stat(const char* name, const StatObj& stat);
		};

		ImGuiStats m_stats = {};
		


		static bool is_initialized()
		{
			if (const auto im = imgui::get(); im && im->m_initialized){
				return true;
			}
			return false;
		}

	private:
		void tab_about();
		void tab_dev();
		void tab_wip();
		void tab_utilities();
		void tab_gamesettings();
		void tab_map_settings();
		bool m_im_window_focused = false;
		bool m_im_window_hovered = false;
		std::string m_devgui_custom_footer_content;

		bool m_initialized = false;

		static void questionmark(const char* desc)
		{
			ImGui::TextDisabled("(?)");
			if (ImGui::BeginItemTooltip())
			{
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted(desc);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
	};
}
