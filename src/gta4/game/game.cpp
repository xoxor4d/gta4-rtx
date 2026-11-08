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
	RECT game_rect = {};



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
	void** pCurrentRenderPhase = nullptr;

	eWeatherType* weather_type_prev = nullptr;
	eWeatherType* weather_type_new = nullptr;
	float* weather_change_value = nullptr;
	float* pTimeCycleWetnessChange = nullptr;
	float* pTimeCycleWetness = nullptr;
	float* pTimeCycleSpecularOffset = nullptr;

	TimeCycleParams* m_pCurrentTimeCycleParams_01 = nullptr;
	TimeCycleParams* m_pCurrentTimeCycleParams_02 = nullptr;
	TimeCycleParams* m_pCurrentTimeCycleParams_Cutscene = nullptr;
	uint8_t* m_game_clock_hours = nullptr;
	uint8_t* m_game_clock_minutes = nullptr;
	uint8_t* m_game_clock_seconds = nullptr;
	uint32_t* m_game_timer_length = nullptr;

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

	DWORD g_modelPointersAddr = 0u;
	CBaseModelInfo** g_modelPointers = nullptr;

	CPool** ppBuildingPool = nullptr;
	render_cell_grid_near_s* pRenderCellGridNear = nullptr;
	float* pShaderGlobalScalars = nullptr;

	// --------------
	// game functions

	FindPlayerCentreOfWorld_t FindPlayerCentreOfWorld = nullptr;
	getNativeAddress_t getNativeAddress = nullptr;
	PopulateAvailResolutionsArray_t PopulateAvailResolutionsArray = nullptr;
	AddSingleVehicleLight_t AddSingleVehicleLight = nullptr;
	AddSceneLight_t AddSceneLight = nullptr;
	SetupTextureAndSampler_t SetupTextureAndSampler = nullptr;
	AddMapSectionsInFrustum_t AddMapSectionsInFrustum = nullptr;
	uint32_t func_addr_AddMapSectionsInFrustum = 0u;



	// --------------
	// game asm offsets

	uint32_t retn_addr__special_SetupVsPsPass_handling = 0u;
	uint32_t hk_addr__SetupVsPsPass_hk = 0u;
	uint32_t func_addr__SetupTextureAndSampler = 0u;
	uint32_t hk_addr__on_instanced_render__post_setup_vs_ps_pass_stub = 0u;
	uint32_t retn_addr__on_instanced_render__pre_setup_vs_ps_pass_stub = 0u;
	uint32_t retn_addr__on_instanced_render__post_setup_vs_ps_pass_stub = 0u;
	uint32_t hk_addr__on_instanced_render__geometry_stub = 0u;
	uint32_t retn_addr__on_instanced_render__geometry_stub = 0u;
	uint32_t retn_addr__on_phone_phase_clear_stub = 0u;
	uint32_t func_addr__on_sky_render_stub = 0u;
	uint32_t retn_addr__on_sky_render_stub = 0u;

	uint32_t hk_addr__on_cgame_process_hk = 0u;

	uint32_t retn_addr__on_add_frontendhelpertext_stub = 0u;
	uint32_t func_addr__add_renderfontbufferdc = 0u;
	uint32_t func_addr__frontendhelpertext_add_drawcmd = 0u;
	uint32_t retn_addr__radar_background_flicker_fix = 0u;

	uint32_t retn_addr__pre_entity_surfs_stub = 0u;
	uint32_t hk_addr__post_entity_surfs_stub = 0u;
	uint32_t hk_addr__post_entity_surfs_stub2 = 0u;

	uint32_t func_addr__CEntity_AddToDrawListBuckets = 0u;
	uint32_t func_addr__CRenderPhase_BuildDrawListWrapper = 0u;
	uint32_t func_addr__CEntityDrawInfo_RenderBuckets = 0u;
	uint32_t func_addr__CDrawSkinnedEntityDC_Execute = 0u;
	uint32_t func_addr__CRenderer_RenderDrawableGeometriesWithLightsInArea = 0u;
	uint32_t hk_addr__frag_type_dc_draw = 0u;
	uint32_t func_addr__fragType_Draw = 0u;
	void** pModelInfoPtrs = nullptr;
	uint32_t func_addr__CBaseDC_operator_new = 0u;
	uint32_t func_addr__CBaseDC_Init = 0u;
	uint32_t func_addr__CDrawListMgr_AddModelInfoReference = 0u;
	uint32_t func_addr__grmGeometryQB_Draw = 0u;
	uint32_t func_addr__grmGeometryQB_dtor = 0u;
	uint32_t func_addr__grcTexturePC_dtor = 0u;
	uint32_t func_addr__grmShader_DrawGeometry = 0u;
	uint32_t func_addr__grmShader_DrawSkinned = 0u;
	int* pForcedTechnique = nullptr;
	std::uint8_t* pBikeHelmetDrawFlag = nullptr;
	int* pTechniqueDefault = nullptr;
	int* pTechniqueAlt = nullptr;
	int* pGrmRenderState1 = nullptr;
	uint32_t func_addr__CDrawPedDC_Execute = 0u;
	uint32_t func_addr__CDrawPlayerDC_Execute = 0u;
	uint32_t func_addr__CDrawPedPropDC_Execute = 0u;
	uint32_t func_addr__CDrawPedPropsDC_Execute = 0u;
	uint32_t func_addr__CDrawSLODPedDC_Execute = 0u;
	uint32_t func_addr__CCustomShaderEffectPedBoneDamageFX_SetShaderVariables = 0u;
	DWORD func_addr__CGtaDrawable_BeforeAddToDrawList = 0u;
	DWORD func_addr__CGtaDrawable_AfterDraw = 0u;
	void* pgDrawListMgr = nullptr;
	uint32_t* pDrawCommandIndexCounter = nullptr;
	void** pCRenderPhase_sm_pCurrent = nullptr;

	uint32_t retn_addr__pre_vehicle_surfs_stub = 0u;
	uint32_t hk_addr__post_vehicle_surfs_stub = 0u;

	uint32_t hk_addr__on_update_light_list_stub = 0u;
	uint32_t retn_addr__on_render_light_list_stub = 0u;

	uint32_t nop_addr__allow_commandline01 = 0u;
	uint32_t jmp_addr__allow_commandline02 = 0u;
	uint32_t hk_addr__commandlinearg_get_int = 0u;

	uint32_t hk_addr__on_create_game_window_hk = 0u;
	uint32_t retn_addr__on_create_game_window_hk = 0u;
	uint32_t import_addr__SetRect = 0u;
	uint32_t import_addr__CreateWindowExA = 0u;

	uint32_t nop_addr__disable_postfx_drawing = 0u;

	uint32_t retn_addr__pre_draw_water = 0u;
	uint32_t hk_addr__post_draw_water = 0u;

	uint32_t func_addr__CWater_ScanThroughBlocks = 0u;
	uint32_t func_addr__CWater_BlockHit = 0u;
	uint32_t func_addr__CWater_CalculateWavesForCoordinate = 0u;
	uint32_t func_addr__CWater_RenderHighDetailWaterRectangle = 0u;
	uint32_t func_addr__CWater_DrawHighDetailPrimitive = 0u;
	uint32_t func_addr__CWater_RenderWaterRectangle = 0u;
	uint32_t func_addr__grcViewport_IsSphereVisible = 0u;
	int* pWaterDetailDist = nullptr;
	int* pWaterRenderBufferIndex = nullptr;
	float* pWaterCameraCoordsRT = nullptr;
	int* pWaterDetailRangeRender = nullptr;
	float* pWaterDynamicHeightRT = nullptr;
	std::uint8_t* pWaterRectFrustumCull = nullptr;

	uint32_t func_addr__CDistantLights_Render = 0u;
	uint32_t func_addr__CDistantLights_RenderOutSpriteBuffer = 0u;
	uint32_t func_addr__VehicleImposters_RenderNight = 0u;
	uint32_t func_addr__VehicleImposters_RenderDay = 0u;
	uint32_t func_addr__VehicleImposters_FlushInstances = 0u;
	void** pCurrentImTexture = nullptr;

	uint32_t retn_addr__pre_draw_statics = 0u;
	uint32_t hk_addr__post_draw_statics = 0u;

	uint32_t retn_addr__pre_draw_mirror = 0u;
	uint32_t hk_addr__post_draw_mirror = 0u;

	uint32_t retn_addr__pre_draw_fx_instance = 0u;
	uint32_t hk_addr__post_draw_fx_instance = 0u;
	uint32_t retn_addr__pre_draw_fx = 0u;
	uint32_t hk_addr__post_draw_fx = 0u;

	uint32_t hk_addr__vehicle_center_headlight = 0u;
	uint32_t nop_addr__vehicle_headlight_prevent_override = 0u;
	uint32_t nop_addr__vehicle_headlight_prevent_read = 0u;
	uint32_t hk_addr__vehicle_single_headlight = 0u;

	uint32_t hk_addr__vehicle_center_rearlight = 0u;
	uint32_t hk_addr__vehicle_single_rearlight = 0u;

	uint32_t hk_addr__vehicle_vshaped_sirens_fake_light = 0u;
	uint32_t hk_addr__vehicle_firetruck_sirens_fake_light = 0u;

	uint32_t hk_addr__vehicle_vshaped_sirens_vlight = 0u;
	uint32_t hk_addr__vehicle_firetruck_sirens = 0u;

	uint32_t hk_addr__vehicle_barshaped_sirens = 0u;
	uint32_t hk_addr__vehicle_fbibuffalo_sirens = 0u;

	uint32_t hk_addr__static_world_culling_check_hk = 0u;
	uint32_t nop_addr__static_world_frustum_patch01 = 0u;
	uint32_t nop_addr__static_world_frustum_patch02 = 0u;

	uint32_t retn_addr__add_far_grid_map_sections = 0u;
	uint32_t fn_addr__mark_render_sector_far_callback = 0u;

	uint32_t retn_addr__add_near_grid_map_sections = 0u;
	uint32_t fn_addr__mark_render_sector_near_callback = 0u;

	uint32_t retn_addr__extended_anti_culling_check_stub = 0u;
	uint32_t jmp_addr__extended_anti_culling_check_stub = 0u;
	uint32_t hk_addr__lod_fade_in_stub = 0u;
	uint32_t hk_addr__frustum_check = 0u;
	uint32_t retn_addr__frustum_check_interior_objs = 0u;

	uint32_t hk_addr__prevent_game_input_func = 0u;
	uint32_t nop_addr__always_draw_game_in_menus = 0u;

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
	uint32_t nop_addr__disable_add_projected_light = 0u;

	uint32_t hk_addr__on_draw_entities_mod_fn_args = 0u;
	uint32_t hk_addr__rmcdrawable_grab_model_hash = 0u;
	uint32_t hk_addr__rmcdrawable_grab_model_hash_retn_addr = 0u;

	uint32_t hk_addr__dyn_obj_grab_model_hash_retn_addr = 0u;
	uint32_t hk_addr__dyn_obj_clear_hash01 = 0u;
	uint32_t hk_addr__dyn_obj_clear_hash02 = 0u;
	uint32_t hk_addr__dyn_obj_clear_hash03 = 0u;

	uint32_t retn_addr__veh_nullptr_crash_fix = 0u;
	uint32_t retn_addr__veh_nullptr_crash_fix_skip = 0u;

	uint32_t retn_addr__veh_invalid_model_crash_fix = 0u;

	uint32_t retn_addr__draw_phonescreen_bg_fix = 0u;
	uint32_t fn_addr__draw_prim_wrapper = 0u; // for phone screen#
	uint32_t retn_addr__draw_phonescreen_bg_fix2 = 0u;

	// --------------

#define PATTERN_OFFSET_SIMPLE(var, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = offset; found_pattern_count++; \
		} total_pattern_count++;

#define PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(var, type, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = (type)*(DWORD*)offset; found_pattern_count++; \
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

		if (auto offset = shared::utils::mem::find_pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0", 2, "CMenuManager__m_MenuActive", use_pattern, 0x47D0A1); offset) {
			CMenuManager__m_MenuActive = (bool*)*(DWORD*)offset; found_pattern_count++;
		}
		// fallback
		else if (offset = shared::utils::mem::find_pattern("80 3D ? ? ? ? ? 74 ? E8 ? ? ? ? 6A ? 6A", 2, "CMenuManager__m_MenuActive", use_pattern, 0x481BA3); offset) {
			CMenuManager__m_MenuActive = (bool*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 80 3D ? ? ? ? ? 74 ? E8 ? ? ? ? 6A ? 6A

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
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(weather_type_prev, eWeatherType*, "A1 ? ? ? ? 8B 0D ? ? ? ? F3 0F 10 1D", 1, 0x9AFB6F);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(weather_type_new, eWeatherType*, "8B 0D ? ? ? ? F3 0F 10 1D ? ? ? ? F3 0F 5C 1D", 2, 0x9AFB74);

		if (const auto offset = shared::utils::mem::find_pattern("A3 ? ? ? ? 8B 44 24 ? A3 ? ? ? ? 8B 44 24 ? A3 ? ? ? ? A1", 0, "m_game_clock_hours", use_pattern, 0x9CB46D); offset) 
		{
			found_pattern_count += 2u;
			m_game_clock_hours = (uint8_t*)*(DWORD*)(offset + 1u);
			m_game_clock_minutes = (uint8_t*)*(DWORD*)(offset + 10u);
			m_game_clock_seconds = (uint8_t*)*(DWORD*)(offset + 19u);
		} total_pattern_count += 2u;

		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(m_game_timer_length, uint32_t*, "8B 3D ? ? ? ? 2B C1 3B C7 7E", 2, 0x9CB785);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(weather_change_value, float*, "F3 0F 10 0D ? ? ? ? 8B 15 ? ? ? ? 0F 2F C1", 4, 0xA2E7E8);

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

		if (auto offset = shared::utils::mem::find_pattern("0F 94 05 ? ? ? ? FF 15", 3, "ms_bNoBlockOnLostFocus", use_pattern, 0x4241BC); offset) {
			ms_bNoBlockOnLostFocus = (bool*)*(DWORD*)offset; found_pattern_count++;
		} // fallback
		else if (offset = shared::utils::mem::find_pattern("A2 ? ? ? ? C6 05 ? ? ? ? ? FF 15", 3, "ms_bNoBlockOnLostFocus", use_pattern, 0x424417); offset) {
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

		if (const auto offset = shared::utils::mem::find_pattern("8B 04 85 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B 78", 3, "g_modelPointers", use_pattern, 0x8DCC02); offset) 
		{
			g_modelPointersAddr = *(DWORD*)offset;
			g_modelPointers = (CBaseModelInfo**)*(DWORD*)offset; 
			found_pattern_count++;
		} total_pattern_count++;

		// pCurrentRenderPhase = (void**)0x12FB1B8;
		if (const auto offset = shared::utils::mem::find_pattern("89 3D ? ? ? ? E8 ? ? ? ? 8B F0 83 C4 ? 85 F6 74 ? ? ? 8B CF 8B 52", 2, "pCurrentRenderPhase", use_pattern, 0x928B47); offset)
		{
			pCurrentRenderPhase = (void**)*(DWORD*)offset;
			found_pattern_count++;
		} total_pattern_count++;

		// ppBuildingPool = (CPool**)0x12BD0E8;
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(ppBuildingPool, CPool**, "81 FB ? ? ? ? 7C ? 8B 0D ? ? ? ? E8 ? ? ? ? 8B 0D ? ? ? ? E8 ? ? ? ? 5F 5E 5D B9", 10, 0x94171B);

		// pRenderCellGridNear = (render_cell_grid_near_s*)0x15C1610;
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pRenderCellGridNear, render_cell_grid_near_s*, "F6 40 24 40 56 51 57 74 ? B9 ? ? ? ? E8 ? ? ? ? 5F 5E C3 B9 ? ? ? ? E8", 23, 0xAE5E78);

		// pShaderGlobalScalars = (float*)0x103F260;
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pShaderGlobalScalars, float*, "F3 0F 10 0D ? ? ? ? F3 0F 59 4C 24 04 F3 0F 10 05", 18, 0xAC6EF0);

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

		if (const auto offset = shared::utils::mem::find_pattern("55 8B EC 83 E4 ? 83 EC ? 80 7D ? ? 8B 4D", 0, "AddSingleVehicleLight", use_pattern, 0xA3DE90); offset) {
			AddSingleVehicleLight = (AddSingleVehicleLight_t)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("55 8B EC 83 E4 ? F3 0F 10 05 ? ? ? ? 81 EC ? ? ? ? 0F 2F 45 ? 56 57 0F 83", 0, "AddSceneLight", use_pattern, 0xABCCD0); offset) {
			AddSceneLight = (AddSceneLight_t)offset; found_pattern_count++;
		} total_pattern_count++;

		// end GAME_FUNCTIONS
#pragma endregion

		// ---

#pragma region GAME_ASM_OFFSETS

		PATTERN_OFFSET_SIMPLE(retn_addr__special_SetupVsPsPass_handling, "C7 05 ? ? ? ? ? ? ? ? B0 ? 5F C3", 0, 0x6091F1);

		if (const auto offset = shared::utils::mem::find_pattern("53 55 8B E9 56 0F B7 5D ? 33 F6", 0, "hk_addr__SetupVsPsPass_hk", use_pattern, 0x4373E0); offset) {
			hk_addr__SetupVsPsPass_hk = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("83 EC ?? 53 57 8B FA", 0, "func_addr__SetupTextureAndSampler", use_pattern, 0x437C40); offset) {
			func_addr__SetupTextureAndSampler = offset; found_pattern_count++;
			SetupTextureAndSampler = (SetupTextureAndSampler_t)offset;
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

		// RenderInstanced: mov eax, [edi+4]; push 1; mov esi, [eax+ebp*4] (the geometry); its draws bypass grmGeometryQB::Draw
		if (const auto offset = shared::utils::mem::find_pattern("8B 47 04 6A 01 8B 34 A8 8B CE 8B 06 FF 50 28", 0, "hk_addr__on_instanced_render__geometry_stub", use_pattern, 0x69ED83); offset) {
			hk_addr__on_instanced_render__geometry_stub = offset; found_pattern_count++;
			retn_addr__on_instanced_render__geometry_stub = offset + 5u;
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


		PATTERN_OFFSET_SIMPLE(hk_addr__on_cgame_process_hk, "E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 84 DB", 0, 0x59D79F);

		PATTERN_OFFSET_SIMPLE(retn_addr__on_add_frontendhelpertext_stub, "81 EC ? ? ? ? 53 56 8B 35 ? ? ? ? 57 85 F6", 0, 0x8B6C86);
		PATTERN_OFFSET_SIMPLE(func_addr__add_renderfontbufferdc, "56 57 E8 ? ? ? ? 8B F0 8B CE", 0, 0x923950);
		PATTERN_OFFSET_SIMPLE(func_addr__frontendhelpertext_add_drawcmd, "55 8B EC 83 E4 ? 83 EC ? A1 ? ? ? ? 33 C4 89 44 24 ? 56 57 8B 7D ? 85 FF", 0, 0x921DA0);
		PATTERN_OFFSET_SIMPLE(retn_addr__radar_background_flicker_fix, "83 3D ? ? ? ? ? 0F 84 ? ? ? ? ? ? ? ? ? ? ? ? ? ? 89 44 24", 0, 0x902E6E);

		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? 53 56 8B F1 57 0F B7 46 ? 8B 04 85", 0, "retn_addr__pre_entity_surfs_stub", use_pattern, 0x8DCBF6); offset) {
			retn_addr__pre_entity_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("5B 8B E5 5D C3 8A 46", 0, "hk_addr__post_entity_surfs_stub", use_pattern, 0x8DCD66); offset) {
			hk_addr__post_entity_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("FF 52 2C 6A 00 E8 ? ? ? ? 83 C4 04 5F 5E 5B 8B E5 5D C3", 15, "hk_addr__post_entity_surfs_stub2", use_pattern, 0x8DCD88); offset) {
			hk_addr__post_entity_surfs_stub2 = offset; found_pattern_count++;
		} total_pattern_count++;

		// remix api static geometry path
		PATTERN_OFFSET_SIMPLE(func_addr__CEntity_AddToDrawListBuckets, "55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 8B 4F 34 85 C9 0F 84", 0, 0xA2FE70);
		PATTERN_OFFSET_SIMPLE(func_addr__CRenderPhase_BuildDrawListWrapper, "89 0D ? ? ? ? 8B 01 FF 50 0C C7 05 ? ? ? ? 00 00 00 00 C3", 0, 0xA87A20);
		PATTERN_OFFSET_SIMPLE(func_addr__CEntityDrawInfo_RenderBuckets, "55 8B EC 83 E4 F0 83 EC 58 56 57 8B F1 E8 ? ? ? ? 0F B6 46 25 83 F8", 0, 0xAE02A0);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawSkinnedEntityDC_Execute, "51 56 57 8B F9 0F B7 47 08 8B 34 85 ? ? ? ? 8B 46 08 85", 0, 0x8DD920);
		PATTERN_OFFSET_SIMPLE(func_addr__CRenderer_RenderDrawableGeometriesWithLightsInArea, "55 8B EC 83 E4 F0 83 EC 78 8B 45 08 8B 55 1C 33 C9 8B 54 90 40", 0, 0xAC1900);
		PATTERN_OFFSET_SIMPLE(hk_addr__frag_type_dc_draw, "8B CF E8 ? ? ? ? C7 05 ? ? ? ? FF 00 00 00", 2, 0x8DD146);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pModelInfoPtrs, void**, "0F B7 46 46 8B 3C 85 ? ? ? ? 85 FF", 7, 0x8DCDAE);
		PATTERN_OFFSET_SIMPLE(func_addr__CBaseDC_operator_new, "53 56 57 8B 7C 24 10 FF 74 24 14 8B F7 F7 DE 83 E6", 0, 0x8DC3A0);
		PATTERN_OFFSET_SIMPLE(func_addr__CBaseDC_Init, "56 57 8B 7C 24 0C 8B CF 8B 07 FF 50 08 25", 0, 0x499E30);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawListMgr_AddModelInfoReference, "56 57 8B 7C 24 0C 8B F1 8B 0C BD ? ? ? ? E8 ? ? ? ? 8B 86 40 20", 0, 0x942000);
		PATTERN_OFFSET_SIMPLE(func_addr__grmGeometryQB_Draw, "57 8B F9 8B 57 04 85 D2 0F 84 ? ? ? ? 53 33 DB 39 5F 48", 0, 0x463490);
		PATTERN_OFFSET_SIMPLE(func_addr__grmGeometryQB_dtor, "53 56 8B D9 57 FF 73 40 C7 03 ? ? ? ? E8 ? ? ? ? 83 C4 ? 8D 73", 0, 0x45F540);
		PATTERN_OFFSET_SIMPLE(func_addr__grcTexturePC_dtor, "56 57 8B F9 C7 07 ? ? ? ? E8 ? ? ? ? 29 05 ? ? ? ? 8A 47 4D 0F B6 C8", 0, 0x43AE40);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawPedDC_Execute, "55 8B EC 83 E4 F0 81 EC 88 03 00 00 A1 ? ? ? ? 33 C4 89 84 24 84 03 00 00 56 57 8B F9 0F B7", 0, 0x8DD1B0);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawPlayerDC_Execute, "51 56 8B F1 57 0F B7 46 08 8B 3C 85 ? ? ? ? 8B 47 08 85 C0 74 08 8B 80 B4 00 00 00", 0, 0x8DD4A0);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawPedPropDC_Execute, "51 56 8B F1 51 0F B6 86 EF 00 00 00 66 0F 6E C0 0F B6 86 EE 00 00 00", 0, 0x8DD310);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawPedPropsDC_Execute, "51 56 8B F1 80 BE AD 00 00 00 00 0F 84", 0, 0x8DD3B0);
		PATTERN_OFFSET_SIMPLE(func_addr__CDrawSLODPedDC_Execute, "51 56 8B F1 51 0F B6 46 15 66 0F 6E C0 0F B6 46 14", 0, 0x8DD8B0);
		PATTERN_OFFSET_SIMPLE(func_addr__CCustomShaderEffectPedBoneDamageFX_SetShaderVariables, "55 8B EC 83 E4 F0 81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 8B 45 0C 56 8B 75 08 57 8B F9 89 7C 24 10 89 74 24 14 89 44 24 0C", 0, 0xAC6180);
		PATTERN_OFFSET_SIMPLE(func_addr__grmShader_DrawGeometry, "A1 ? ? ? ? 53 56 8B D9 83 F8 FF 75 ? 83 3D ? ? ? ? 00 A1 ? ? ? ? 0F 45 05 ? ? ? ? 8D 0C 40 8B 43 40 0F B6 0C 01", 0, 0x42EBE0);
		PATTERN_OFFSET_SIMPLE(func_addr__grmShader_DrawSkinned, "80 3D ? ? ? ? 00 53 55 8B 6C 24 0C 56 57 8B 7C 24 18 8B D9 74", 0, 0x42ECC0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pBikeHelmetDrawFlag, std::uint8_t*, "80 3D ? ? ? ? 00 53 55 8B 6C 24 0C 56 57 8B 7C 24 18 8B D9 74", 2, 0x42ECC0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pForcedTechnique, int*, "A1 ? ? ? ? 53 56 8B D9 83 F8 FF 75 ? 83 3D ? ? ? ? 00 A1 ? ? ? ? 0F 45 05 ? ? ? ? 8D 0C 40 8B 43 40 0F B6 0C 01", 1, 0x42EBE0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pGrmRenderState1, int*, "A1 ? ? ? ? 53 56 8B D9 83 F8 FF 75 ? 83 3D ? ? ? ? 00 A1 ? ? ? ? 0F 45 05 ? ? ? ? 8D 0C 40 8B 43 40 0F B6 0C 01", 16, 0x42EBE0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pTechniqueDefault, int*, "A1 ? ? ? ? 53 56 8B D9 83 F8 FF 75 ? 83 3D ? ? ? ? 00 A1 ? ? ? ? 0F 45 05 ? ? ? ? 8D 0C 40 8B 43 40 0F B6 0C 01", 22, 0x42EBE0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pTechniqueAlt, int*, "A1 ? ? ? ? 53 56 8B D9 83 F8 FF 75 ? 83 3D ? ? ? ? 00 A1 ? ? ? ? 0F 45 05 ? ? ? ? 8D 0C 40 8B 43 40 0F B6 0C 01", 29, 0x42EBE0);
		PATTERN_OFFSET_SIMPLE(func_addr__CGtaDrawable_BeforeAddToDrawList, "8B 49 04 85 C9 74 0C 83 7C 24 04 00 75 05 8B 01 FF 50 1C C2 04 00", 0, 0xAF4790);
		PATTERN_OFFSET_SIMPLE(func_addr__CGtaDrawable_AfterDraw, "FF 71 08 B9 ? ? ? ? E8 ? ? ? ? C2 04 00", 0, 0xAF4700);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pgDrawListMgr, void*, "B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? B8", 1, 0x8D8C2C);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pDrawCommandIndexCounter, uint32_t*, "89 15 ? ? ? ? C7 07 ? ? ? ? E8", 2, 0x5D4657);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pCRenderPhase_sm_pCurrent, void**, "A1 ? ? ? ? 85 C0 74 ? F3 0F 7E 80 E8 08 00", 1, 0x9423A9);


		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? 53 56 8B F1 57 0F B7 46 ? 8B 3C 85", 0, "retn_addr__pre_vehicle_surfs_stub", use_pattern, 0x8DCDA6); offset) {
			retn_addr__pre_vehicle_surfs_stub = offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 83 C4 ? 5F 5E 5B", 20, "hk_addr__post_vehicle_surfs_stub", use_pattern, 0x8DD095); offset) {
			hk_addr__post_vehicle_surfs_stub = offset; found_pattern_count++;
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

		PATTERN_OFFSET_SIMPLE(hk_addr__commandlinearg_get_int, "8B 41 ? 85 C0 74 ? ? ? ? 74 ? 6A ? 6A ? 50", 0, 0x4102D0);

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

		PATTERN_OFFSET_SIMPLE(func_addr__CWater_ScanThroughBlocks, "55 8B EC 83 E4 F0 81 EC B0 00 00 00 A1 ? ? ? ? 33 C4 89 84 24 AC 00 00 00 8B 15 ? ? ? ?", 0, 0xAD99D0);
		PATTERN_OFFSET_SIMPLE(func_addr__CWater_BlockHit, "56 8B 74 24 0C 57 8B 7C 24 0C 85 FF 78 ? 85 F6", 0, 0xAD4A70);
		PATTERN_OFFSET_SIMPLE(func_addr__CWater_CalculateWavesForCoordinate, "8B 0D ? ? ? ? 64 A1 ? ? ? ? 56 8B 74 24 1C 8B 04 88", 0, 0xAD4D30);
		PATTERN_OFFSET_SIMPLE(func_addr__CWater_RenderHighDetailWaterRectangle, "51 8B 54 24 08 8B 4C 24 14 F3 0F 10 15 ? ? ? ? 56 8B 74 24 10", 0, 0xAD8FA0);
		PATTERN_OFFSET_SIMPLE(func_addr__CWater_DrawHighDetailPrimitive, "83 EC 08 8B 4C 24 34 53 55 56 8B 74 24 1C 57 8B 7C 24 1C 8B C6", 0, 0xAD9130);
		PATTERN_OFFSET_SIMPLE(func_addr__CWater_RenderWaterRectangle, "8B 44 24 0C 8B 4C 24 10 53 55 56 57 3B C1 7D ? 8B D8 8B F9 EB ? 8B D9 8B F8 8B 54 24 14 8B 2D", 0, 0xAD97C0);
		PATTERN_OFFSET_SIMPLE(func_addr__grcViewport_IsSphereVisible, "55 8B EC 83 E4 F8 51 8B 45 18 56 8B F1 0F 57 F6 85 C0 0F 84", 0, 0x431E40);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pWaterDetailDist, int*, "66 0F 6E 05 ? ? ? ? F3 0F 10 27 0F 5B C0 F3 0F 5C E0", 4, 0xAD9E58);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pWaterRectFrustumCull, std::uint8_t*, "80 3D ? ? ? ? 00 0F BF 8F ? ? ? ? 0F BF 04 CD", 2, 0xAD7204);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pWaterDynamicHeightRT, float*, "F3 0F 10 A7 ? ? ? ? F3 0F 59 E2 89 44 24 18", 4, 0xAD5683);

		// vehicle imposters (distant lights)
		PATTERN_OFFSET_SIMPLE(func_addr__CDistantLights_Render, "55 8B EC 83 E4 F0 81 EC ? ? 00 00 F3 0F 10 45 08 F3 0F 59 05 ? ? ? ? 56 57 F3 0F 2C C0 C7 44 24 3C 00 00 00 00", 0, 0xD65650);
		PATTERN_OFFSET_SIMPLE(func_addr__CDistantLights_RenderOutSpriteBuffer, "55 8B EC 83 E4 F0 81 EC ? ? 00 00 8B 4D 14 8B 45 18 ? ? ? ? F3 0F 10 18 F3 0F 10 60 04 F3 0F 10 68 08", 0, 0xD670B0);
		PATTERN_OFFSET_SIMPLE(func_addr__VehicleImposters_RenderNight, "55 8B EC 83 E4 F0 81 EC B8 0A 00 00 A1 ? ? ? ? 33 C4 89 84 24 B4 0A 00 00 56 57", 0, 0xD66070);
		PATTERN_OFFSET_SIMPLE(func_addr__VehicleImposters_RenderDay, "55 8B EC 83 E4 F0 81 EC A8 0A 00 00 A1 ? ? ? ? 33 C4 89 84 24 A4 0A 00 00 56 57", 0, 0xD64970);
		PATTERN_OFFSET_SIMPLE(func_addr__VehicleImposters_FlushInstances, "55 8B EC 83 E4 F0 83 EC 58 56 57 8B 7D 0C 83 3F 00 0F 8E A6 03 00 00 A1 ? ? ? ? C7 44 24 20 00 00 80 3F", 0, 0xD66CE0);
		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(pCurrentImTexture, void**, "8B 44 24 04 85 C0 0F 44 05 ? ? ? ? 50 FF 35 ? ? ? ? B9 ? ? ? ? 68 ? ? ? ? A3", 31, 0x432B8F); // rage::grcBindTexture

		// CWater::GetCameraRangeForRender
		if (const auto offset = shared::utils::mem::find_pattern("8B 0D ? ? ? ? C1 E1 04 F3 0F 2C 81 ? ? ? ? A3 ? ? ? ?", 0, "CWater::GetCameraRangeForRender", use_pattern, 0xAD5AB0); offset)
		{
			pWaterRenderBufferIndex = (int*)*(DWORD*)(offset + 2);
			pWaterCameraCoordsRT = (float*)*(DWORD*)(offset + 13);
			pWaterDetailRangeRender = (int*)*(DWORD*)(offset + 48);
			found_pattern_count++;
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

		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_center_headlight, "FF 75 ? E8 ? ? ? ? 83 C4 ? 5F 5E 8B E5 5D C2 ? ? 84 C0 0F 85 ? ? ? ? 80 7D", 0, 0xA3FE0E);
		PATTERN_OFFSET_SIMPLE(nop_addr__vehicle_headlight_prevent_override, "F3 0F 11 44 24 ? E8 ? ? ? ? 8D 44 24 ? 50 8B 44 24 ? 50 8B CE E8 ? ? ? ? F3 0F 10 64 24 ? F3 0F 58 64 24 ? F3 0F 10 6C 24 ? 8B 46 ? F3 0F 58 6C 24 ? F3 0F 10 74 24 ? F3 0F 59 25 ? ? ? ? F3 0F 11 74 24 ? F3 0F 59 2D ? ? ? ? F3 0F 11 64 24 ? 6A", 0, 0xA3FCBB);
		PATTERN_OFFSET_SIMPLE(nop_addr__vehicle_headlight_prevent_read, "F3 0F 10 74 24 ? F3 0F 59 25 ? ? ? ? F3 0F 11 74 24 ? F3 0F 59 2D ? ? ? ? F3 0F 11 64 24 ? 6A", 0, 0xA3FCF2);
		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_single_headlight, "F3 0F 11 44 24 ? F3 0F 10 44 24 ? ? ? ? ? ? 68 ? ? ? ? 50 8B 44 24 ? 83 C0", 0x22, 0xA3FEED); // dest addr: 0xA3FF0F

		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_center_rearlight, "8D 44 24 ? 50 FF 75 ? E8 ? ? ? ? 83 C4 ? 5F 5E 8B E5 5D C2 ? ? 83 FF ? 0F 84 ? ? ? ? 80 7D ? ? 0F 85 ? ? ? ? F3 0F 10 05", 0, 0xA4336E);
		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_single_rearlight, "F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? ? ? ? ? ? 68", 0x26, 0xA43407); // dest addr: 0xA4342D

		if (const auto offset = shared::utils::mem::find_pattern("8B 44 24 ? 48 46", 0x22F, "hk_addr__vehicle_vshaped_sirens_fake_light", use_pattern, 0xA40ADB); offset) 
		{
			// making sure that this a call
			if (*reinterpret_cast<BYTE*>(offset) == 0xE8)
			{
				hk_addr__vehicle_vshaped_sirens_fake_light = offset;
				found_pattern_count++;
			}
		} total_pattern_count++;

		if (auto offset = shared::utils::mem::find_pattern("0F 84 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 59 0D ? ? ? ? F3 0F 58 0D ? ? ? ? 0F 28 C1 E8 ? ? ? ? 0F 57 C9 0F 2F C8 76 ? F3 0F 10 64 24 ? F3 0F 59 05 ? ? ? ? F3 0F 59 25 ? ? ? ? F3 0F 10 6C 24 ? F3 0F 10 74 24 ? F3 0F 10 7C 24 ? F3 0F 59 E0 F3 0F 10 05 ? ? ? ? F3 0F 5E 44 24 ? B1", 0x0, "hk_addr__vehicle_firetruck_sirens_fake_light", use_pattern, 0xA4105E); offset)
		{
			offset = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u) - 8u;
			// making sure that this a call
			if (*reinterpret_cast<BYTE*>(offset) == 0xE8)
			{
				hk_addr__vehicle_firetruck_sirens_fake_light = offset;
				found_pattern_count++;
			}
		} total_pattern_count++;


		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_vshaped_sirens_vlight, "E8 ? ? ? ? 83 C4 ? EB ? 8B 7C 24 ? 51", 0x0, 0xA40AAA);
		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_firetruck_sirens, "E8 ? ? ? ? 83 C4 ? EB ? 51 8D 47", 0x0, 0xA4100E);

		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_barshaped_sirens, "E8 ? ? ? ? 83 C4 ? EB ? 51 83 C0 ? ? ? ? ? ? ? ? 50 8B 44 24", 0x0, 0xA4146F);
		PATTERN_OFFSET_SIMPLE(hk_addr__vehicle_fbibuffalo_sirens, "E8 ? ? ? ? F3 0F 10 4C 24 ? 83 C4 ? 0F 2F 0D", 0x0, 0xA417DF);
		

		PATTERN_OFFSET_SIMPLE(hk_addr__static_world_culling_check_hk, "55 8B EC 83 E4 ? 83 EC ? 56 8B F1 ? ? 8B 40 ? FF D0 ? ? ? ? ? ? 8D 4C 24 ? 51 8B CE FF 50 ? F3 0F 10 44 24 ? 8B 4D", 0, 0xA31C20);
		PATTERN_OFFSET_SIMPLE(nop_addr__static_world_frustum_patch01, "0F 84 ? ? ? ? 66 83 7F ? ? 75 ? B9", 0, 0x444C8B);
		PATTERN_OFFSET_SIMPLE(nop_addr__static_world_frustum_patch02, "75 ? B9 ? ? ? ? 89 4C 24 ? 32 C0", 0, 0x444C96);
		PATTERN_OFFSET_SIMPLE(retn_addr__extended_anti_culling_check_stub, "33 DB 85 ED 0F 8E ? ? ? ? 57", 0, 0xAE8698);

		if (const auto offset = shared::utils::mem::find_pattern("0F 85 ? ? ? ? 53 55 8B 6C 24 ? 33 DB", 0, "jmp_addr__extended_anti_culling_check_stub", use_pattern, 0xAE868C); offset) {
			jmp_addr__extended_anti_culling_check_stub = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;

		PATTERN_OFFSET_SIMPLE(hk_addr__lod_fade_in_stub, "C6 46 63 FF 83 3D ? ? ? ? 00 74 ? 85 D2", 0, 0xAE82C0);

		PATTERN_OFFSET_SIMPLE(hk_addr__frustum_check, "55 8B EC 83 E4 ? 51 8B 45 ? 56 8B F1 0F 57 F6", 0, 0x431E40);
		PATTERN_OFFSET_SIMPLE(retn_addr__frustum_check_interior_objs, "? ? ? ? F3 0F 10 50 ? ? ? ? ? F3 0F 5C 51 ? F3 0F 10 49", 0, 0xA0FCF9);

		PATTERN_OFFSET_SIMPLE(retn_addr__add_far_grid_map_sections, "8B 8C 24 ? ? ? ? 5F 5E 33 CC C6 05", 0, 0xAEA14D);
		PATTERN_OFFSET_SIMPLE(fn_addr__mark_render_sector_far_callback, "8B 54 24 ? 85 D2 78 ? 56", 0, 0xAE93C0);

		PATTERN_OFFSET_SIMPLE(retn_addr__add_near_grid_map_sections, "F3 0F 10 BC 24 ? ? ? ? F3 0F 10 9C 24 ? ? ? ? 8A 0D ? ? ? ? A1", 0, 0xAE9EDC);
		PATTERN_OFFSET_SIMPLE(fn_addr__mark_render_sector_near_callback, "8B 4C 24 ? B8 ? ? ? ? 8B 89 ? ? ? ? D3 E0", 0, 0xAE95B0);


		if (const auto offset = shared::utils::mem::find_pattern("83 EC ? A1 ? ? ? ? 33 C4 89 44 24 ? 80 7C 24 ? ? 8B 44 24", 0, "AddMapSectionsInFrustum", use_pattern, 0xD62DC0); offset) 
		{
			AddMapSectionsInFrustum = (AddMapSectionsInFrustum_t)offset; 
			func_addr_AddMapSectionsInFrustum = offset;
			found_pattern_count++;
		} total_pattern_count++;

		PATTERN_OFFSET_SIMPLE(hk_addr__prevent_game_input_func, "53 8A 5C 24 ? 8A CB", 0, 0x69F0C0);
		PATTERN_OFFSET_SIMPLE(nop_addr__always_draw_game_in_menus, "83 FE ? 75 ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 ? 85 C0 79", 0, 0x5C278A);

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
		PATTERN_OFFSET_SIMPLE(nop_addr__disable_add_projected_light, "8B 15 ? ? ? ? 56 8D 72", 0, 0xC1F020);

		PATTERN_OFFSET_SIMPLE(hk_addr__on_draw_entities_mod_fn_args, "50 50 0F B6 46 ? 50 8D 44 24 ? 50 8B CF", 0, 0x8DCD49);
		if (const auto offset = shared::utils::mem::find_pattern("? ? ? ? ? ? 33 C0 C7 86 ? ? ? ? ? ? ? ? 89 86 ? ? ? ? C7 86", 2, "hk_addr__rmcdrawable_grab_model_hash", use_pattern, 0x633994); offset) 
		{
			// offset = fragDrawable vtbl
			uint8_t* vtbl_addr = (uint8_t*)*(DWORD*)offset; // fragDrawable vtbl (should be 0xFE2534)
				     vtbl_addr += 0x20;						// draw fn
			hk_addr__rmcdrawable_grab_model_hash = *(DWORD*)vtbl_addr; // get addr of draw fn (should end up as 0x6A1D10)
			hk_addr__rmcdrawable_grab_model_hash_retn_addr = hk_addr__rmcdrawable_grab_model_hash + 0xB;
			found_pattern_count++;
		} total_pattern_count++;

		
		PATTERN_OFFSET_SIMPLE(hk_addr__dyn_obj_grab_model_hash_retn_addr, "0F 84 ? ? ? ? 53 8B 9D ? ? ? ? 56", 0, 0x8DE1CD);
		PATTERN_OFFSET_SIMPLE(hk_addr__dyn_obj_clear_hash01, "5D 83 C4 ? C3 8B 4C 24 ? F3 0F 10 49", 0, 0x8DE2C3);
		PATTERN_OFFSET_SIMPLE(hk_addr__dyn_obj_clear_hash02, "5D 83 C4 ? C3 ? ? ? ? A1", 0, 0x8DE298);

		if (hk_addr__dyn_obj_grab_model_hash_retn_addr) {
			hk_addr__dyn_obj_clear_hash03 = shared::utils::mem::resolve_relative_jump_address(hk_addr__dyn_obj_grab_model_hash_retn_addr, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;


		// tire deflation related issue
		PATTERN_OFFSET_SIMPLE(retn_addr__veh_nullptr_crash_fix, "57 F6 86 ? ? ? ? ? 8B F9 0F 85", 0, 0xAC7AD5);
		
		if (const auto offset = shared::utils::mem::find_pattern("0F 85 ? ? ? ? F3 0F 10 87 ? ? ? ? 0F 2E 05", 0, "retn_addr__veh_nullptr_crash_fix_skip", use_pattern, 0xAC7ADF); offset) {
			retn_addr__veh_nullptr_crash_fix_skip = shared::utils::mem::resolve_relative_jump_address(offset, 6u, 2u); found_pattern_count++;
		} total_pattern_count++;

		// accessing wrong mesh? 
		PATTERN_OFFSET_SIMPLE(retn_addr__veh_invalid_model_crash_fix, "3C ? 0F 85 ? ? ? ? E8 ? ? ? ? F3 0F 59 05", 0, 0x9795CA);

		// retn_addr__draw_phonescreen_bg_fix
		if (const auto offset = shared::utils::mem::find_pattern("A1 ? ? ? ? 69 C0 ? ? ? ? 83 C4 ? F6 84 30 ? ? ? ? ? 0F 84 ? ? ? ? 80 7C 24 ? ? 0F 84", 0, "retn_addr__draw_phonescreen_bg_fix", use_pattern, 0x94A643); offset) 
		{
			retn_addr__draw_phonescreen_bg_fix = offset; found_pattern_count++;

			// func used to draw the screen
			fn_addr__draw_prim_wrapper = shared::utils::mem::resolve_relative_call_address(retn_addr__draw_phonescreen_bg_fix - 5u);
		} total_pattern_count++;

		PATTERN_OFFSET_SIMPLE(retn_addr__draw_phonescreen_bg_fix2, "83 C4 ? E8 ? ? ? ? E9 ? ? ? ? F3 0F 10 44 24", 0, 0x94A1BD);

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

	CRenderPhase_struct* get_current_renderphase()
	{
		if (!game::pCurrentRenderPhase || !*game::pCurrentRenderPhase) {
			return nullptr;
		}

		return reinterpret_cast<game::CRenderPhase_struct*>(*game::pCurrentRenderPhase);
	}
}
