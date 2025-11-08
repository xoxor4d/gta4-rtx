#include "std_include.hpp"
#include "structs.hpp"
#include "shared/common/flags.hpp"

namespace gta4::game
{
	// helper vars
	bool was_loadscreen_active = false;
	bool is_in_game = false;

	float helper_timecycle_current_fog_density = 0.0f;

	HMODULE hmodule_fusionfix = nullptr;


	// --------------
	// game variables

	DWORD* d3d_dev_addr = nullptr;
	IDirect3DVertexShader9** g_currentVertexShader = nullptr;
	IDirect3DPixelShader9** g_currentPixelShader = nullptr;

	bool* CMenuManager__m_MenuActive = nullptr;
	bool* CMenuManager__m_LoadscreenActive = nullptr;

	/*CLightSource* g_lightList = nullptr;
	std::uint32_t* g_lightCount = nullptr;*/

	CLightSource* g_lightListSrc = nullptr;
	std::uint32_t* g_lightListSrcCount = nullptr;

	CDirectionalLight* g_directionalLights = nullptr;

	grmShaderInfo_Parameter* pGlobalShaderParameters = nullptr;
	uint32_t* pGlobalShaderParameterCount = nullptr;

	char* pShaderConstFloatCountMap = nullptr;
	int* pRenderStateIndexMap = nullptr;

	g_viewports2* pViewports = nullptr;
	currentViewport_ptr* pCurrentViewport = nullptr;
	D3DXMATRIX* pCurrentWorldTransform = nullptr;

	float* pTimeCycleWetnessChange = nullptr;
	float* pTimeCycleWetness = nullptr;
	float* pTimeCycleSpecularOffset = nullptr;

	TimeCycleParams* m_pCurrentTimeCycleParams_01 = nullptr;
	TimeCycleParams* m_pCurrentTimeCycleParams_02 = nullptr;
	TimeCycleParams* m_pCurrentTimeCycleParams_Cutscene = nullptr;

	//CLightSource* m_renderLights = nullptr;
	//std::uint32_t* m_numRenderLights = nullptr;
	DWORD* m_renderLights_addr = nullptr;
	DWORD* m_numRenderLights_addr = nullptr;

	int* systemMetrics_xRight = nullptr;
	int* systemMetrics_yBottom = nullptr;

	uint32_t* ms_dwNativeTableSize = nullptr;
	uint32_t** ms_pNatives = nullptr;

	uint8_t* m_CodePause = nullptr;
	int* m_dwCutsceneState = nullptr;
	bool* ms_bNoBlockOnLostFocus = nullptr;
	bool* ms_bFocusLost = nullptr;
	bool* ms_bWindowed = nullptr;

	settings_cfg_s* loaded_settings_cfg = nullptr;
	resolution_modes_ptr* avail_game_resolutions = nullptr;
	uint32_t* d3d9_adapter_index = nullptr;

	bool* m_bMobilePhoneActive = nullptr;

	// --------------
	// game functions

	FindPlayerCentreOfWorld_t FindPlayerCentreOfWorld = nullptr;
	getNativeAddress_t getNativeAddress = nullptr;
	PopulateAvailResolutionsArray_t PopulateAvailResolutionsArray = nullptr;



	// --------------
	// game asm offsets

	uint32_t hk_addr__SetupVsPsPass_hk = 0u;
	uint32_t func_addr__SetupTextureAndSampler = 0u;
	uint32_t hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub = 0u;
	uint32_t retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub = 0u;
	uint32_t retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub = 0u;
	uint32_t retn_addr__on_phone_phase_clear_stub = 0u;
	uint32_t func_addr__on_sky_render_stub = 0u;
	uint32_t retn_addr__on_sky_render_stub = 0u;

	uint32_t retn_addr__on_add_frontendhelpertext_stub = 0u;
	uint32_t func_addr__add_renderfontbufferdc = 0u;
	uint32_t func_addr__frontendhelpertext_add_drawcmd = 0u;

	uint32_t retn_addr__pre_entity_surfs_stub = 0u;
	uint32_t hk_addr__post_entity_surfs_stub = 0u;

	uint32_t retn_addr__pre_vehicle_surfs_stub = 0u;
	uint32_t hk_addr__post_vehicle_surfs_stub = 0u;

	uint32_t hk_addr__static_world_culling_check_hk = 0u;
	uint32_t nop_addr__static_world_frustum_patch01 = 0u;
	uint32_t nop_addr__static_world_frustum_patch02 = 0u;

	uint32_t hk_addr__on_update_light_list_stub = 0u;
	uint32_t retn_addr__on_render_light_list_stub = 0u;

	uint32_t nop_addr__allow_commandline01 = 0u;
	uint32_t jmp_addr__allow_commandline02 = 0u;

	uint32_t hk_addr__on_create_game_window_hk = 0u;
	uint32_t retn_addr__on_create_game_window_hk = 0u;
	uint32_t import_addr__SetRect = 0u;
	uint32_t import_addr__CreateWindowExA = 0u;

	uint32_t nop_addr__disable_postfx_drawing = 0u;

	uint32_t retn_addr__pre_draw_water = 0u;
	uint32_t hk_addr__post_draw_water = 0u;

	uint32_t retn_addr__pre_draw_statics = 0u;
	uint32_t hk_addr__post_draw_statics = 0u;

	uint32_t retn_addr__pre_draw_mirror = 0u;
	uint32_t hk_addr__post_draw_mirror = 0u;

	uint32_t retn_addr__pre_draw_fx_instance = 0u;
	uint32_t hk_addr__post_draw_fx_instance = 0u;
	uint32_t retn_addr__pre_draw_fx = 0u;
	uint32_t hk_addr__post_draw_fx = 0u;

	uint32_t hk_addr__frustum_check = 0u;

	uint32_t hk_addr__prevent_game_input_func = 0u;

	uint32_t nop_addr__disable_unused_rendering_01 = 0u;
	uint32_t nop_addr__disable_unused_rendering_02 = 0u;
	uint32_t nop_addr__disable_unused_rendering_03 = 0u;
	uint32_t nop_addr__disable_unused_rendering_04 = 0u;
	uint32_t nop_addr__disable_unused_rendering_05 = 0u;
	uint32_t nop_addr__disable_unused_rendering_06 = 0u;
	uint32_t nop_addr__disable_unused_rendering_07 = 0u;
	uint32_t nop_addr__disable_unused_rendering_08 = 0u;
	uint32_t nop_addr__disable_unused_rendering_09 = 0u;
	uint32_t nop_addr__disable_unused_rendering_10 = 0u;

	uint32_t cond_jmp_addr__disable_unused_rendering_01 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_02 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_03 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_04 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_05 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_06 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_07 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_08 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_09 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_10 = 0u;
	uint32_t cond_jmp_addr__disable_unused_rendering_11 = 0u;

	uint32_t cond_jmp_addr__skip_deferred_light_rendering01 = 0u;
	uint32_t cond_jmp_addr__skip_deferred_light_rendering02 = 0u;

	// --------------

#define PATTERN_OFFSET_SIMPLE(var, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = offset; found_pattern_count++; \
		} total_pattern_count++;

	// init any adresses here
	void init_game_addresses()
	{
		
		const bool use_pattern = !shared::common::flags::has_flag("no_pattern");
		if (use_pattern) {
			shared::common::log("Game", "Getting offsets ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}

		std::uint32_t total_pattern_count = 0u;
		std::uint32_t found_pattern_count = 0u;

#pragma region GAME_VARIABLES

		if (const auto offset = shared::utils::mem::find_pattern("FF 35 ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0", 2, "d3d_dev_addr", use_pattern, 0x4208FD); offset) {
			d3d_dev_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("39 15 ? ? ? ? 74 ? A1 ? ? ? ? 52 ? ? 50 89 15 ? ? ? ? FF 91 ? ? ? ? 0F B7 43", 2, "g_currentVertexShader", use_pattern, 0x437183); offset) {
			g_currentVertexShader = (IDirect3DVertexShader9**)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("39 15 ? ? ? ? 74 ? A1 ? ? ? ? 52 ? ? 50 89 15 ? ? ? ? FF 91 ? ? ? ? 8B 44 24", 2, "g_currentPixelShader", use_pattern, 0x4372CB); offset) {
			g_currentPixelShader = (IDirect3DPixelShader9**)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0", 2, "CMenuManager__m_MenuActive", use_pattern, 0x47D0A1); offset) {
			CMenuManager__m_MenuActive = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("80 3D ? ? ? ? ? 53 56 8A FA", 2, "CMenuManager__m_LoadscreenActive", use_pattern, 0x5CDBC3); offset) {
			CMenuManager__m_LoadscreenActive = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		/*if (const auto offset = shared::utils::mem::find_pattern("89 35 ? ? ? ? 8B 49", 2, "g_lightList", use_pattern, 0xABECED); offset) {
			g_lightList = (CLightSource*)*(DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("89 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 5E", 2, "g_lightCount", use_pattern, 0xABED16); offset) {
			g_lightCount = (std::uint32_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;*/

		if (const auto offset = shared::utils::mem::find_pattern("81 C6 ? ? ? ? 89 BC 24 ? ? ? ? ? ? ? ? F3 0F 10 6E", 2, "g_directionalLights", use_pattern, 0xAC3576); offset) {
			g_directionalLights = (CDirectionalLight*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("A3 ? ? ? ? 8B C1 69 C0", 1, "g_lightListSrc", use_pattern, 0xAC2DE4); offset) {
			g_lightListSrc = (CLightSource*)*(DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("A1 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 89 0D", 1, "g_lightListSrcCount", use_pattern, 0xAC2E2D); offset) {
			g_lightListSrcCount = (std::uint32_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("81 C6 ? ? ? ? 42 57", 2, "pGlobalShaderParameters", use_pattern, 0x435FF9); offset) {
			pGlobalShaderParameters = (game::grmShaderInfo_Parameter*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 15 ? ? ? ? 8D 34 52", 2, "pGlobalShaderParameterCount", use_pattern, 0x435FEC); offset) {
			pGlobalShaderParameterCount = (std::uint32_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("0F B6 88 ? ? ? ? 8B 54 24", 3, "pShaderConstFloatCountMap", use_pattern, 0x43723C); offset) {
			pShaderConstFloatCountMap = (char*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 3C 85 ? ? ? ? 83 FF ? 74", 3, "pRenderStateIndexMap", use_pattern, 0x4373F8); offset) {
			pRenderStateIndexMap = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("B9 ? ? ? ? 53 56 FF 35", 1, "pViewports", use_pattern, 0x5C2679); offset) {
			pViewports = (g_viewports2*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 0D ? ? ? ? E8 ? ? ? ? 33 C0", 2, "pCurrentViewport", use_pattern, 0x6A432F); offset) {
			pCurrentViewport = (currentViewport_ptr*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("C7 80 ? ? ? ? ? ? ? ? C7 80 ? ? ? ? ? ? ? ? C7 80 ? ? ? ? ? ? ? ? C7 80 ? ? ? ? ? ? ? ? C7 80 ? ? ? ? ? ? ? ? 83 C0 ? 3D", 
			2, "pCurrentWorldTransform", use_pattern, 0x42C422); offset) 
		{
			pCurrentWorldTransform = (D3DXMATRIX*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		//
		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 05 ? ? ? ? 56 57 ? ? ? C1 E7", 4, "pTimeCycleCurrentWetness", use_pattern, 0x986CBC); offset) {
			pTimeCycleWetnessChange = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 10 15 ? ? ? ? 0F 57 DB", 4, "pTimeCycleWetness", use_pattern, 0xAEFBF1); offset) {
			pTimeCycleWetness = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 0D ? ? ? ? F3 0F 59 C8 0F 2F D9", 4, "pTimeCycleSpecularOffset", use_pattern, 0xAEFC16); offset) {
			pTimeCycleSpecularOffset = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("B9 ? ? ? ? ? ? ? ? ? ? ? 56 E8 ? ? ? ? B9", 1, "m_pCurrentTimeCycleParams_01", use_pattern, 0x5B9498); offset) {
			m_pCurrentTimeCycleParams_01 = (TimeCycleParams*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("B9 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 6A ? 51", 1, "m_pCurrentTimeCycleParams_02", use_pattern, 0x5B94AA); offset) {
			m_pCurrentTimeCycleParams_02 = (TimeCycleParams*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("68 ? ? ? ? E8 ? ? ? ? 0F B6 05 ? ? ? ? 8B 0D", 1, "m_pCurrentTimeCycleParams_Cutscene", use_pattern, 0xAF4306); offset) {
			m_pCurrentTimeCycleParams_Cutscene = (TimeCycleParams*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		// 0x103EED0
		if (const auto offset = shared::utils::mem::find_pattern("89 35 ? ? ? ? 8B 49", 2, "m_renderLights_addr", use_pattern, 0xABECED); offset) {
			m_renderLights_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0x154DFD0
		if (const auto offset = shared::utils::mem::find_pattern("89 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 5E", 2, "m_numRenderLights_addr", use_pattern, 0xABED16); offset) {
			m_numRenderLights_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		// 0x105C888
		if (const auto offset = shared::utils::mem::find_pattern("0F 44 15 ? ? ? ? 66 0F 6E C2", 3, "systemMetrics_xRight", use_pattern, 0x422FF8); offset) {
			systemMetrics_xRight = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0x105C87C
		if (const auto offset = shared::utils::mem::find_pattern("0F 44 0D ? ? ? ? 66 0F 6E C1 0F 5B C0 68", 3, "systemMetrics_yBottom", use_pattern, 0x423072); offset) {
			systemMetrics_yBottom = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("89 1D ? ? ? ? ? ? F7 D9 0B C8 51 8B CE FF 57 ? 33 C9 A3 ? ? ? ? 85 DB 7E ? ? ? ? ? ? ? ? 41 3B CB 7D ? A1 ? ? ? ? EB ? 5F 5E C7 05 ? ? ? ? ? ? ? ? 5B C2 ? ? 5F 5E 89 0D ? ? ? ? 5B C2 ? ? 56", 
			2, "ms_dwNativeTableSize", use_pattern, 0x86FC94); offset) {
			ms_dwNativeTableSize = (uint32_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 15 ? ? ? ? 53 64 8B 1D ? ? ? ? 85 D2", 2, "ms_pNatives", use_pattern, 0x86E730); offset) {
			ms_pNatives = (uint32_t**)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("0A 05 ? ? ? ? 75 ? F6 43", 2, "m_CodePause", use_pattern, 0x4C4D4F); offset) {
			m_CodePause = (uint8_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("A1 ? ? ? ? 83 EC ? 85 C0 74 ? 83 F8", 1, "m_dwCutsceneState", use_pattern, 0xAF42D6); offset) {
			m_dwCutsceneState = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("0F 94 05 ? ? ? ? FF 15", 3, "ms_bNoBlockOnLostFocus", use_pattern, 0x4241BC); offset) {
			ms_bNoBlockOnLostFocus = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("C6 05 ? ? ? ? ? 85 C0 74 ? FF D0 E8", 2, "ms_bFocusLost", use_pattern, 0x420A90); offset) {
			ms_bFocusLost = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("38 05 ? ? ? ? 74 ? B8 ? ? ? ? A3", 2, "ms_bWindowed", use_pattern, 0x4208B1); offset) {
			ms_bWindowed = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("A3 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 85 C0 74 ? 0F 57 C0", 1, "loaded_settings_cfg", use_pattern, 0x59E41B); offset) {
			loaded_settings_cfg = (settings_cfg_s*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 0D ? ? ? ? ? ? 83 C1", 2, "avail_game_resolutions", use_pattern, 0x4238B6); offset) {
			avail_game_resolutions = (resolution_modes_ptr*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("8B 2D ? ? ? ? E8 ? ? ? ? 83 E8", 2, "d3d9_adapter_index", use_pattern, 0x41F511); offset) {
			d3d9_adapter_index = (uint32_t*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("C6 05 ? ? ? ? ? C6 05 ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? ? ? 6A", 2, "m_bMobilePhoneActive", use_pattern, 0x5BF958); offset) {
			m_bMobilePhoneActive = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// end GAME_VARIABLES
#pragma endregion

		// ---

#pragma region GAME_FUNCTIONS

		if (const auto offset = shared::utils::mem::find_pattern("E8 ? ? ? ? F3 0F 10 44 24 ? F3 0F 10 64 24 ? F3 0F 5C 44 24", 0, "FindPlayerCentreOfWorld", use_pattern, 0x94C1F2); offset) {
			FindPlayerCentreOfWorld = (FindPlayerCentreOfWorld_t)shared::utils::mem::resolve_relative_call_address(offset); found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("E8 ? ? ? ? 8B D8 85 DB 75 ? 50", 0, "getNativeAddress", use_pattern, 0x86E508); offset) {
			getNativeAddress = (getNativeAddress_t)shared::utils::mem::resolve_relative_call_address(offset); found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? 53 55 56 FF 35", 0, "PopulateAvailResolutionsArray", use_pattern, 0x8C4530); offset) {
			PopulateAvailResolutionsArray = (PopulateAvailResolutionsArray_t)offset; found_pattern_count++;
		} total_pattern_count++;

		// end GAME_FUNCTIONS
#pragma endregion

		// ---

#pragma region GAME_ASM_OFFSETS

		if (const auto offset = shared::utils::mem::find_pattern("53 55 8B E9 56 0F B7 5D ? 33 F6", 0, "hk_addr__SetupVsPsPass_hk", use_pattern, 0x4373E0); offset) {
			hk_addr__SetupVsPsPass_hk = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("83 EC ?? 53 57 8B FA", 0, "func_addr__SetupTextureAndSampler", use_pattern, 0x437C40); offset) {
			func_addr__SetupTextureAndSampler = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("83 C0 ? FF 4C 24 ? C7 05", 0, "hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub", use_pattern, 0x69EEFE); offset) {
			hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("50 56 8D 42 ? 50 8D 42", 0, "retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub", use_pattern, 0x69ED6E); offset) {
			retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("C7 05 ? ? ? ? ? ? ? ? 89 44 24 ? 74", 0, "retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub", use_pattern, 0x69EF05); offset) {
			retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("6A ? 6A ? 51 8B C8 ? ? ? ? ? 6A ? 6A ? 6A ? E8 ? ? ? ? 50", 0, "retn_addr__on_phone_phase_clear_stub", use_pattern, 0x5E164B); offset) {
			retn_addr__on_phone_phase_clear_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("55 8B EC 83 E4 ? A1 ? ? ? ? 81 EC ? ? ? ? C7 80", 0, "func_addr__on_sky_render_stub", use_pattern, 0xDBC170); offset) {
			func_addr__on_sky_render_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		// final offset 0xC107CE
		if (const auto offset = shared::utils::mem::find_pattern("80 3D ? ? ? ? ? 74 ? E8 ? ? ? ? 56", 14, "retn_addr__on_sky_render_stub", use_pattern, 0xC107C0); offset) {
			retn_addr__on_sky_render_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		PATTERN_OFFSET_SIMPLE(retn_addr__on_add_frontendhelpertext_stub, "81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 57 85 F6", 0, 0x8B6C86);
		PATTERN_OFFSET_SIMPLE(func_addr__add_renderfontbufferdc, "56 57 E8 ? ? ? ? 8B F0 8B CE", 0, 0x923950);
		PATTERN_OFFSET_SIMPLE(func_addr__frontendhelpertext_add_drawcmd, "55 8B EC 83 E4 ? 83 EC ? A1 ? ? ? ? 33 C4 89 44 24 ? 56 57 8B 7D ? 85 FF", 0, 0x921DA0);


		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? 53 56 8B F1 57 0F B7 46 ? 8B 04 85", 0, "retn_addr__pre_entity_surfs_stub", use_pattern, 0x8DCBF6); offset) {
			retn_addr__pre_entity_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("5B 8B E5 5D C3 8A 46", 0, "hk_addr__post_entity_surfs_stub", use_pattern, 0x8DCD66); offset) {
			hk_addr__post_entity_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? 53 56 8B F1 57 0F B7 46 ? 8B 3C 85", 0, "retn_addr__pre_vehicle_surfs_stub", use_pattern, 0x8DCDA6); offset) {
			retn_addr__pre_vehicle_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 C4 ? 5F 5E 5B", 20, "hk_addr__post_vehicle_surfs_stub", use_pattern, 0x8DD095); offset) {
			hk_addr__post_vehicle_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 8B F1 ? ? 8B 40 ? FF D0 ? ? ? ? ? ? 8D 4C 24 ? 51 8B CE FF 50 ? F3 0F 10 44 24 ? 8B 4D", 
			0, "hk_addr__static_world_culling_check_hk", use_pattern, 0xA31C20); offset) {
			hk_addr__static_world_culling_check_hk = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("0F 84 ? ? ? ? 66 83 7F ? ? 75 ? B9", 0, "nop_addr__static_world_frustum_patch01", use_pattern, 0x444C8B); offset) {
			nop_addr__static_world_frustum_patch01 = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("75 ? B9 ? ? ? ? 89 4C 24 ? 32 C0", 0, "nop_addr__static_world_frustum_patch02", use_pattern, 0x444C96); offset) {
			nop_addr__static_world_frustum_patch02 = offset; found_pattern_count++;
		} total_pattern_count++;


		/*if (const auto offset = shared::utils::mem::find_pattern("B9 ? ? ? ? 89 15 ? ? ? ? A3", 21, "hk_addr__on_update_light_list_stub", use_pattern, 0xABED11); offset) {
			hk_addr__on_update_light_list_stub = offset; found_pattern_count++;
		} total_pattern_count++;*/

		if (const auto offset = shared::utils::mem::find_pattern("81 EC ? ? ? ? A1 ? ? ? ? 56 F3 0F 10 80", 0, "retn_addr__on_render_light_list_stub", use_pattern, 0xAC1036); offset) {
			retn_addr__on_render_light_list_stub = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("0F 8F ? ? ? ? 6A ? 68", 0, "nop_addr__allow_commandline01", use_pattern, 0x401CAA); offset) {
			nop_addr__allow_commandline01 = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("7C ? ? ? 8B 40 ? ? ? ? 75", 0, "jmp_addr__allow_commandline02", use_pattern, 0x401C8E); offset) {
			jmp_addr__allow_commandline02 = offset; found_pattern_count++;
		} total_pattern_count++;


		// 
		if (const auto offset = shared::utils::mem::find_pattern("6A ? 0F 45 DF", 0, "hk_addr__on_create_game_window_hk", use_pattern, 0x420D91); offset) {
			hk_addr__on_create_game_window_hk = offset; found_pattern_count++;
			retn_addr__on_create_game_window_hk = hk_addr__on_create_game_window_hk + 5u;
		} total_pattern_count++;

		// 0xE733C4
		if (const auto offset = shared::utils::mem::find_pattern("FF 15 ? ? ? ? 80 3D ? ? ? ? ? 74 ? 6A", 2, "import_addr__SetRect", use_pattern, 0x4209C5); offset) {
			import_addr__SetRect = *(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0xE733C0
		if (const auto offset = shared::utils::mem::find_pattern("FF 15 ? ? ? ? 8B F0 6A", 2, "import_addr__CreateWindowExA", use_pattern, 0x420E5B); offset) {
			import_addr__CreateWindowExA = *(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("A2 ? ? ? ? E8 ? ? ? ? 8B CE E8 ? ? ? ? 8B CE", 12, "nop_addr__disable_postfx_drawing", use_pattern, 0x92F547); offset) {
			nop_addr__disable_postfx_drawing = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("81 EC ? ? ? ? 56 57 E8 ? ? ? ? ? ? ? 0F 84", 0, "retn_addr__pre_draw_water", use_pattern, 0xAD7EF6); offset) {
			retn_addr__pre_draw_water = offset; found_pattern_count++;
		} total_pattern_count++;

		// can't create signature at the end of the function so we get the offset from a relative jump instruction
		if (const auto offset = shared::utils::mem::find_pattern("0F 84 ? ? ? ? A1 ? ? ? ? 83 F8 ? 74 ? 83 F8 ? 75", 0, "hk_addr__post_draw_water", use_pattern, 0xAD7F06); offset) {
			hk_addr__post_draw_water = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("75 ? 83 3D ? ? ? ? ? A1 ? ? ? ? 0F 45 05 ? ? ? ? ? ? ? 8B 43 ? ? ? ? ? 85 C9", 0, "retn_addr__pre_draw_statics", use_pattern, 0x42EBEC); offset) {
			retn_addr__pre_draw_statics = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("EB ? 5F 5D 5E C7 05", 16, "hk_addr__post_draw_statics", use_pattern, 0x42ECA7); offset) {
			hk_addr__post_draw_statics = offset; found_pattern_count++;
		} total_pattern_count++;


		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? E8 ? ? ? ? ? ? ? 0F 84", 0, "retn_addr__pre_draw_mirror", use_pattern, 0xB59906); offset) {
			retn_addr__pre_draw_mirror = offset; found_pattern_count++;
		} total_pattern_count++;

		// can't create signature at the end of the function so we get the offset from a relative jump instruction
		if (const auto offset = shared::utils::mem::find_pattern("0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 84 ? ? ? ? FF 35", 0, "hk_addr__post_draw_mirror", use_pattern, 0xB59911); offset) {
			hk_addr__post_draw_mirror = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;

		// --

		PATTERN_OFFSET_SIMPLE(retn_addr__pre_draw_fx_instance, "80 B9 ? ? ? ? ? 74 ? 80 B9 ? ? ? ? ? 0F 84", 0, 0x8DD5A9);

		// can't create signature at the end of the function so we get the offset from a relative jump instruction
		if (const auto offset = shared::utils::mem::find_pattern("0F 84 ? ? ? ? 83 3D ? ? ? ? ? 74 ? A1 ? ? ? ? 3B 05 ? ? ? ? 75 ? 83 3D ? ? ? ? ? 75", 0, "hk_addr__post_draw_fx_instance", use_pattern, 0x8DD5B9); offset) {
			hk_addr__post_draw_fx_instance = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;

		{
			PATTERN_OFFSET_SIMPLE(retn_addr__pre_draw_fx, "73 ? 8B 44 24 ? 55", 0, 0x6035C4);

			// retn addr ^ is a relative jmp to the end of the func - resolve offset
			if (retn_addr__pre_draw_fx) {
				hk_addr__post_draw_fx = shared::utils::mem::resolve_relative_jump_address(retn_addr__pre_draw_fx, 2u, 1u); found_pattern_count++;
			} total_pattern_count++;
		}
		

		if (const auto offset = shared::utils::mem::find_pattern("55 8B EC 83 E4 ? 51 8B 45 ? 56 8B F1 0F 57 F6", 0, "hk_addr__frustum_check", use_pattern, 0x431E40); offset) {
			hk_addr__frustum_check = offset; found_pattern_count++;
		} total_pattern_count++;

		PATTERN_OFFSET_SIMPLE(hk_addr__prevent_game_input_func, "53 8A 5C 24 ? 8A CB", 0, 0x69F0C0);

		// --

		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_01, "25 ? ? ? ? ? ? ? ? ? ? 50 68 ? ? ? ? E8", 17, 0xABD861);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_02, "E8 ? ? ? ? 6A ? E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 8B D0 83 C4 ? 85 D2 74 ? 8B 4A ? ? ? ? ? ? ? 33 0D ? ? ? ? 81 E1 ? ? ? ? 31 4A ? FF 05 ? ? ? ? ? ? ? ? ? ? C7 42 ? ? ? ? ? 8B 87 ? ? ? ? 89 42 ? EB ? 33 D2 52 E8 ? ? ? ? E8", 0, 0xADD8E1);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_03, "E8 ? ? ? ? E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 48 ? ? ? ? ? ? ? 33 0D ? ? ? ? 81 E1 ? ? ? ? 31 48 ? FF 05 ? ? ? ? ? ? ? ? ? ? C7 40", 0, 0xADD938);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_04, "E8 ? ? ? ? 83 C4 ? 6A ? 6A ? FF B7 ? ? ? ? E8 ? ? ? ? 83 C4 ? 50 E8 ? ? ? ? 6A", 0, 0xADD9C7);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_05, "E8 ? ? ? ? 83 C4 ? F7 87 ? ? ? ? ? ? ? ? 0F 84 ? ? ? ? 8B 35", 0, 0xADDA4D);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_06, "E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 48 ? ? ? ? ? ? ? 33 0D ? ? ? ? 81 E1 ? ? ? ? 31 48 ? FF 05 ? ? ? ? ? ? ? ? ? ? C7 40 ? ? ? ? ? EB ? 33 C0 50 E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 0D", 0, 0xADDAD2);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_07, "E8 ? ? ? ? 6A ? 6A ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8B 0D ? ? ? ? 85 C9 74 ? 8A 89 ? ? ? ? EB ? 32 C9 88 4C 24 ? 8D 4C 24 ? 51 68 ? ? ? ? 8B C8 E8 ? ? ? ? EB", 0, 0xADDB17);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_08, "E8 ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0 74 ? 8D 8F ? ? ? ? 51 8B C8 E8 ? ? ? ? EB ? 33 C0 50 E8 ? ? ? ? 83 C4 ? F7 87", 0, 0xADDB5A);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_09, "75 ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 84", 0, 0xD781EA);
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_unused_rendering_10, "75 ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 75", 0, 0xD518DD);

		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_01, "0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 57 6A", 0, 0xD781F3);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_02, "0F 84 ? ? ? ? E8 ? ? ? ? 84 C0 0F 84 ? ? ? ? 8B 86 ? ? ? ? 83 F8", 0, 0xD77A0D);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_03, "0F 84 ? ? ? ? 80 3D ? ? ? ? ? 74 ? 8B 0D", 0, 0xD76C23);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_04, "0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 83 BF", 0, 0xD76964);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_05, "0F 84 ? ? ? ? 80 BE ? ? ? ? ? 0F 84 ? ? ? ? 57", 0, 0xD61CFA);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_06, "0F 84 ? ? ? ? 6A ? 6A ? C6 86", 0, 0xD61BAB);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_07, "0F 85 ? ? ? ? E8 ? ? ? ? 84 C0 75 ? 8B CF", 0, 0xD518E6);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_08, "0F 84 ? ? ? ? 57 E8 ? ? ? ? 83 EC", 0, 0xD5116B);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_09, "0F 84 ? ? ? ? 53 55 56 6A", 0, 0xD514FD);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_10, "0F 84 ? ? ? ? 57 83 EC", 0, 0xD50F8B);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__disable_unused_rendering_11, "0F 84 ? ? ? ? 80 3D ? ? ? ? ? 0F 84 ? ? ? ? 8B 0D ? ? ? ? 64 A1", 0, 0xAC10AA);

		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__skip_deferred_light_rendering01, "0F 8E ? ? ? ? 83 C7 ? 89 7C 24 ? 8B 47", 0, 0x928AE5);
		PATTERN_OFFSET_SIMPLE(cond_jmp_addr__skip_deferred_light_rendering02, "56 8B F1 FF 76 ? FF 76 ? E8", 0, 0x8DCBC0);

		// end GAME_ASM_OFFSETS
#pragma endregion

		if (use_pattern)
		{
			if (found_pattern_count == total_pattern_count) {
				shared::common::log("Game", std::format("Found all '{:d}' Patterns.", total_pattern_count), shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
			}
			else
			{
				shared::common::log("Game", std::format("Only found '{:d}' out of '{:d}' Patterns.", found_pattern_count, total_pattern_count), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				shared::common::log("Game", ">> Please create an issue on GitHub and attach this console log and information about your game (version, platform etc.)\n", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
			}
		}
	}

#undef PATTERN_OFFSET_SIMPLE


	grmShaderInfo_Parameter* getGlobalShaderInfoParam(const char* name)
	{
		//auto x = pGlobalShaderParameterCount;
		//auto y = &pGlobalShaderParameters[20];
		for (uint32_t i = 0; i < *pGlobalShaderParameterCount; i++)
		{
			if (std::string_view(name) == std::string_view(pGlobalShaderParameters[i].pszName))
				return &pGlobalShaderParameters[i];
		}
		return nullptr;
	}
}
