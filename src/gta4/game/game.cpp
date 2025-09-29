#include "std_include.hpp"
#include "structs.hpp"
#include "shared/common/flags.hpp"

namespace gta4::game
{
	// helper vars
	bool was_loadscreen_active = false;
	bool is_in_game = false;


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

	//CLightSource* m_renderLights = nullptr;
	//std::uint32_t* m_numRenderLights = nullptr;
	DWORD* m_renderLights_addr = nullptr;
	DWORD* m_numRenderLights_addr = nullptr;

	int* systemMetrics_xRight = nullptr;
	int* systemMetrics_yBottom = nullptr;

	// --------------
	// game functions

	FindPlayerCentreOfWorld_t FindPlayerCentreOfWorld = (FindPlayerCentreOfWorld_t)nullptr;


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

	uint32_t import_addr__SetRect = 0u;
	uint32_t import_addr__CreateWindowExA = 0u;

	uint32_t nop_addr__disable_postfx_drawing = 0u;

	// --------------

	// init any adresses here
	void init_game_addresses()
	{
		std::cout << "[INIT] Getting offsets ...\n";
		const bool use_pattern = !shared::common::flags::has_flag("no_pattern");

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

		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 05 ? ? ? ? 56 57 ? ? ? C1 E7", 4, "pTimeCycleCurrentWetness", use_pattern, 0x986CBC); offset) {
			pTimeCycleWetnessChange = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 10 15 ? ? ? ? 0F 57 DB", 4, "pTimeCycleWetness", use_pattern, 0xAEFBF1); offset) {
			pTimeCycleWetness = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		if (const auto offset = shared::utils::mem::find_pattern("F3 0F 10 0D ? ? ? ? F3 0F 59 C8 0F 2F D9", 4, "pTimeCycleSpecularOffset", use_pattern, 0xAEFC16); offset) {
			pTimeCycleSpecularOffset = (float*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0x103EED0
		if (const auto offset = shared::utils::mem::find_pattern("89 35 ? ? ? ? 8B 49", 2, "m_renderLights_addr", use_pattern, 0xABECED); offset) {
			m_renderLights_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0x154DFD0
		if (const auto offset = shared::utils::mem::find_pattern("89 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 5E", 2, "m_numRenderLights_addr", use_pattern, 0xABED16); offset) {
			m_numRenderLights_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		DWORD* m_renderLights_addr = nullptr;
		DWORD* m_numRenderLights_addr = nullptr;


		// 0x105C888
		if (const auto offset = shared::utils::mem::find_pattern("0F 44 15 ? ? ? ? 66 0F 6E C2", 3, "systemMetrics_xRight", use_pattern, 0x422FF8); offset) {
			systemMetrics_xRight = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;

		// 0x105C87C
		if (const auto offset = shared::utils::mem::find_pattern("0F 44 0D ? ? ? ? 66 0F 6E C1 0F 5B C0 68", 3, "systemMetrics_yBottom", use_pattern, 0x423072); offset) {
			systemMetrics_yBottom = (int*)*(DWORD*)offset; found_pattern_count++;
		} total_pattern_count++;


		// end GAME_VARIABLES
#pragma endregion

		// ---

#pragma region GAME_FUNCTIONS

		if (const auto offset = shared::utils::mem::find_pattern("E8 ? ? ? ? F3 0F 10 44 24 ? F3 0F 10 64 24 ? F3 0F 5C 44 24", 0, "FindPlayerCentreOfWorld", use_pattern, 0x94C1F2); offset) {
			FindPlayerCentreOfWorld = (FindPlayerCentreOfWorld_t)shared::utils::mem::resolve_relative_call_address(offset); found_pattern_count++;
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

		// end GAME_ASM_OFFSETS
#pragma endregion

		if (found_pattern_count == total_pattern_count) 
		{
			shared::common::set_console_color_green(true);
			std::cout << "[INIT] Found all '" << std::to_string(total_pattern_count) << "' Patterns.\n";
			shared::common::set_console_color_default();
		}
		else
		{
			shared::common::set_console_color_red(true);
			std::cout << "[!][INIT] Only found '" << std::to_string(found_pattern_count) << "' out of '" << std::to_string(total_pattern_count) << "' Patterns.\n";
			shared::common::set_console_color_blue(true);
			std::cout << ">> Please create an issue on GitHub and attach this console log and information about your game (version, platform etc.)\n";
			shared::common::set_console_color_default();
		}
	}

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
