#pragma once
#include "structs.hpp"

namespace gta4::game
{
	// helper vars
	extern bool was_loadscreen_active;
	extern bool is_in_game;

	extern float helper_timecycle_current_fog_density;

	extern HMODULE hmodule_fusionfix;
	extern RECT game_rect;

	// --------------
	// game variables

	extern DWORD* d3d_dev_addr;
	extern IDirect3DVertexShader9** g_currentVertexShader;
	extern IDirect3DPixelShader9** g_currentPixelShader;

	inline IDirect3DDevice9* get_d3d_device() {
		return reinterpret_cast<IDirect3DDevice9*>(*d3d_dev_addr);
	}

	extern bool* CMenuManager__m_MenuActive;
	extern bool* CMenuManager__m_LoadscreenActive;

	/*extern CLightSource* g_lightList;
	extern std::uint32_t* g_lightCount;*/

	extern CLightSource* g_lightListSrc;
	extern std::uint32_t* g_lightListSrcCount;

	extern CDirectionalLight* g_directionalLights;

	extern grmShaderInfo_Parameter* pGlobalShaderParameters;
	extern uint32_t* pGlobalShaderParameterCount;
	extern grmShaderInfo_Parameter* getGlobalShaderInfoParam(const char* name);

	extern char* pShaderConstFloatCountMap;
	extern int* pRenderStateIndexMap;
	extern g_viewports2* pViewports;
	extern currentViewport_ptr* pCurrentViewport;
	extern D3DXMATRIX* pCurrentWorldTransform;

	extern eWeatherType* weather_type_prev;
	extern eWeatherType* weather_type_new;
	extern float* weather_change_value;
	extern float* pTimeCycleWetnessChange;
	extern float* pTimeCycleWetness;
	extern float* pTimeCycleSpecularOffset;

	extern TimeCycleParams* m_pCurrentTimeCycleParams_01;
	extern TimeCycleParams* m_pCurrentTimeCycleParams_02;
	extern TimeCycleParams* m_pCurrentTimeCycleParams_Cutscene;
	extern uint8_t* m_game_clock_hours;
	extern uint8_t* m_game_clock_minutes;

	//extern CLightSource* m_renderLights;
	//extern std::uint32_t* m_numRenderLights;
	extern DWORD* m_renderLights_addr;
	extern DWORD* m_numRenderLights_addr;

	inline CLightSource* get_renderLights() {
		return reinterpret_cast<CLightSource*>(*m_renderLights_addr);
	}

	inline std::uint32_t get_renderLightsCount() {
		return *reinterpret_cast<std::uint32_t*>(m_numRenderLights_addr);
	}

	extern int* systemMetrics_xRight;
	extern int* systemMetrics_yBottom;

	extern uint32_t* ms_dwNativeTableSize;
	extern uint32_t** ms_pNatives;

	extern uint8_t* m_CodePause;
	extern int* m_dwCutsceneState;
	extern bool* ms_bNoBlockOnLostFocus;
	extern bool* ms_bFocusLost;
	extern bool* ms_bWindowed;

	extern settings_cfg_s* loaded_settings_cfg;
	extern resolution_modes_ptr* avail_game_resolutions;
	extern uint32_t* d3d9_adapter_index;

	extern bool* m_bMobilePhoneActive;

	// --------------
	// game functions

	typedef Vector(__cdecl* FindPlayerCentreOfWorld_t)(Vector*);
		extern FindPlayerCentreOfWorld_t FindPlayerCentreOfWorld;

	typedef	void* (__stdcall* getNativeAddress_t)(uint32_t);
		extern getNativeAddress_t getNativeAddress;

	typedef	void* (__cdecl* PopulateAvailResolutionsArray_t)(uint32_t);
		extern PopulateAvailResolutionsArray_t PopulateAvailResolutionsArray;

	typedef	void* (__cdecl* AddSingleVehicleLight_t)(D3DXMATRIX* some_mtx, float* light_pos, float* light_dir, float* color, float intensity, float radius, float inner_cone_angle, float outer_cone_angle, int interIndex, int roomindex, int shadowRelIndex, char light_flag_0x4, char light_flag_0x400);
		extern AddSingleVehicleLight_t AddSingleVehicleLight;

	typedef	void* (__cdecl* AddSceneLight_t)(int unk_flag, game::eLightType type, int flag, float* dir, float* otherdir, float* pos, float* color, float intensity, int tex_hash, int txd_hash, float radius, float inner_cone, float outer_cone, int inter_index, int room_index, int shadow_rel_index);
		extern AddSceneLight_t AddSceneLight;

	// --------------
	// game asm offsets

	extern uint32_t retn_addr__special_SetupVsPsPass_handling;
	extern uint32_t hk_addr__SetupVsPsPass_hk;
	extern uint32_t func_addr__SetupTextureAndSampler;
	extern uint32_t hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub;
	extern uint32_t retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub;
	extern uint32_t retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub;
	extern uint32_t retn_addr__on_phone_phase_clear_stub;
	extern uint32_t func_addr__on_sky_render_stub;
	extern uint32_t retn_addr__on_sky_render_stub;

	extern uint32_t hk_addr__on_cgame_process_hk;

	extern uint32_t retn_addr__on_add_frontendhelpertext_stub;
	extern uint32_t func_addr__add_renderfontbufferdc;
	extern uint32_t func_addr__frontendhelpertext_add_drawcmd;

	extern uint32_t retn_addr__pre_entity_surfs_stub;
	extern uint32_t hk_addr__post_entity_surfs_stub;

	extern uint32_t retn_addr__pre_vehicle_surfs_stub;
	extern uint32_t hk_addr__post_vehicle_surfs_stub;

	extern uint32_t hk_addr__static_world_culling_check_hk;
	extern uint32_t nop_addr__static_world_frustum_patch01;
	extern uint32_t nop_addr__static_world_frustum_patch02;

	extern uint32_t retn_addr__extended_anti_culling_check_stub;
	extern uint32_t jmp_addr__extended_anti_culling_check_stub;

	extern uint32_t hk_addr__on_update_light_list_stub;
	extern uint32_t retn_addr__on_render_light_list_stub;

	extern uint32_t nop_addr__allow_commandline01;
	extern uint32_t jmp_addr__allow_commandline02;
	extern uint32_t hk_addr__commandlinearg_get_int;

	extern uint32_t hk_addr__on_create_game_window_hk;
	extern uint32_t retn_addr__on_create_game_window_hk;
	extern uint32_t import_addr__SetRect;
	extern uint32_t import_addr__CreateWindowExA;

	extern uint32_t nop_addr__disable_postfx_drawing;

	extern uint32_t retn_addr__pre_draw_water;
	extern uint32_t hk_addr__post_draw_water;

	extern uint32_t retn_addr__pre_draw_statics;
	extern uint32_t hk_addr__post_draw_statics;

	extern uint32_t retn_addr__pre_draw_mirror;
	extern uint32_t hk_addr__post_draw_mirror;

	extern uint32_t retn_addr__pre_draw_fx_instance;
	extern uint32_t hk_addr__post_draw_fx_instance;
	extern uint32_t retn_addr__pre_draw_fx;
	extern uint32_t hk_addr__post_draw_fx;

	extern uint32_t hk_addr__vehicle_center_headlight;
	extern uint32_t nop_addr__vehicle_headlight_prevent_override;
	extern uint32_t nop_addr__vehicle_headlight_prevent_read;
	extern uint32_t hk_addr__vehicle_single_headlight;

	extern uint32_t hk_addr__vehicle_center_rearlight;
	extern uint32_t hk_addr__vehicle_single_rearlight;
	extern uint32_t hk_addr__vehicle_vshaped_sirens_fake_light;
	extern uint32_t hk_addr__vehicle_vshaped_sirens_vlight;

	extern uint32_t hk_addr__frustum_check;

	extern uint32_t hk_addr__prevent_game_input_func;

	extern uint32_t nop_addr__always_draw_game_in_menus;

	extern uint32_t nop_addr__disable_unused_rendering_01;
	extern uint32_t nop_addr__disable_unused_rendering_02;
	extern uint32_t nop_addr__disable_unused_rendering_03;
	extern uint32_t nop_addr__disable_unused_rendering_04;
	extern uint32_t nop_addr__disable_unused_rendering_05;
	extern uint32_t nop_addr__disable_unused_rendering_06;
	extern uint32_t nop_addr__disable_unused_rendering_07;
	extern uint32_t nop_addr__disable_unused_rendering_08;
	extern uint32_t nop_addr__disable_unused_rendering_09;
	extern uint32_t nop_addr__disable_unused_rendering_10;

	extern uint32_t cond_jmp_addr__disable_unused_rendering_01;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_02;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_03;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_04;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_05;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_06;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_07;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_08;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_09;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_10;
	extern uint32_t cond_jmp_addr__disable_unused_rendering_11;

	extern uint32_t cond_jmp_addr__skip_deferred_light_rendering01;
	extern uint32_t cond_jmp_addr__skip_deferred_light_rendering02;

	// ---

	extern void init_game_addresses();
}
