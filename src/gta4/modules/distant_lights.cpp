#include "std_include.hpp"
#include "distant_lights.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "renderer.hpp"

namespace gta4
{
	namespace
	{
		using namespace remix_draw_state;

		DETOUR_TYPEDEF(CDistantLights_Render, int, __cdecl, float night_fade);
		DETOUR_TYPEDEF(CDistantLights_RenderOutSpriteBuffer, void, __cdecl, const float* positions, const DWORD* colors, int* count, const float* right, const float* up);
		DETOUR_TYPEDEF(VehicleImposters_RenderNight, int, __cdecl);
		DETOUR_TYPEDEF(VehicleImposters_RenderDay, int, __cdecl);
		DETOUR_TYPEDEF(VehicleImposters_FlushInstances, void, __cdecl, const float* records, int* count, const float* right, const float* up, int model);

		constexpr std::uint32_t SPRITE_STRIDE = 4u;      // float4 positions
		constexpr std::uint32_t CAR_RECORD_FLOATS = 20u; // three rotation rows, position, colour (float4 each)
		constexpr int CAR_MODELS = 3;                    // day: string type % 3 picks the model, night: model 0
		constexpr float FADE_STEPS = 8.0f;

		// api material of a game texture; the handle is the hash, so two hashes alternate and the replaced one lives
		// until the last mesh using it is destroyed
		struct material_s
		{
			remixapi_MaterialHandle handle = nullptr;
			remixapi_MaterialHandle stale = nullptr;
			IDirect3DBaseTexture9* texture = nullptr;
			std::uint32_t serial = 0u;
		};

		struct sprites_s
		{
			bool in_render = false;
			bool capturing = false;
			bool warming_up = false; // this render stays on D3D9 so the runtime gets the texture
			std::vector<remixapi_HardcodedVertex> vertices;
			std::vector<std::uint32_t> indices;
			std::uint32_t count = 0u;
			std::uint32_t flushes = 0u;

			bool state_captured = false;
			remixapi_InstanceInfoBlendEXT blend = {};
			float normal[3] = { 0.0f, 0.0f, 1.0f };
			IDirect3DBaseTexture9* last_texture = nullptr; // texture the last render bound, the material follows it a render later

			material_s material;
			remixapi_MeshHandle mesh = nullptr; // mesh of the last render, destroyed once the next one is drawn
			std::uint32_t serial = 0u;
		};

		struct car_mesh_s
		{
			remixapi_MeshHandle mesh = nullptr;
			material_s material;
			IDirect3DVertexBuffer9* vb = nullptr; // identity of the geometry the mesh was built from
			UINT start_index = 0u;
			UINT prim_count = 0u;
			IDirect3DBaseTexture9* warm_texture = nullptr; // texture of the last D3D9 warm up draw
			std::uint32_t warm_render = 0u;
		};

		struct cars_s
		{
			bool rendering = false;  // inside the game's night / day car render
			bool night = false;
			bool api = false;
			bool in_flush = false;   // inside the game's flush
			bool capturing = false;  // inside a flush the api path takes
			bool drawn = false;      // the first D3D9 draw of the flush was replaced, further passes are dropped
			bool green = false;      // the cached materials are the green debug ones
			const float* records = nullptr;
			int count = 0;
			int model = 0;

			std::uint32_t render_serial = 0u;
			std::uint32_t flushes = 0u;
			std::uint32_t warmups = 0u;
			std::uint32_t records_total = 0u;
			std::uint32_t instances = 0u;
			renderer_api::reject_reason reject = renderer_api::reject_reason::none;

			car_mesh_s meshes[2][CAR_MODELS]; // [night][model]
		};

		sprites_s g_sprites;
		cars_s g_cars;
		distant_lights::stats_s g_stats;

		void destroy_handle(remixapi_MaterialHandle& handle)
		{
			if (handle)
			{
				shared::common::remix_api::get().m_bridge.DestroyMaterial(handle);
				handle = nullptr;
			}
		}

		void destroy_mesh(remixapi_MeshHandle& mesh)
		{
			if (mesh)
			{
				shared::common::remix_api::get().m_bridge.DestroyMesh(mesh);
				mesh = nullptr;
			}
		}

		void destroy_material(material_s& m)
		{
			destroy_handle(m.stale);
			destroy_handle(m.handle);
			m.texture = nullptr;
		}

		// the game texture is the albedo and the replacement key (renderer_api's material recipe): the runtime draws the
		// legacy defaults material of that texture, keyed by its image hash like a D3D9 draw (rtx.conf texture lists apply).
		// emissive: the texture is the emission as well (unit intensity, scaled per instance by the emissive scalar modifier).
		// green: a solid emissive material without a texture, nothing of it is keyed
		bool create_material(material_s& m, IDirect3DBaseTexture9* texture, const std::uint64_t base_hash, const bool emissive, const bool green)
		{
			const auto colormap = !green && texture && texture->GetType() == D3DRTYPE_TEXTURE ? static_cast<IDirect3DTexture9*>(texture) : nullptr;

			remixapi_MaterialInfoGameTexturesEXT game_textures =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_GAME_TEXTURES_EXT,
				.pNext = nullptr,
				.colorTexture = colormap,
				.colorIsEmissive = emissive,
			};

			remixapi_MaterialInfoOpaqueEXT opaque = {};
			opaque.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
			opaque.pNext = colormap ? &game_textures : nullptr;
			opaque.roughnessTexture = L"";
			opaque.metallicTexture = L"";
			opaque.heightTexture = L"";
			opaque.albedoConstant = green ? remixapi_Float3D{ 0.0f, 1.0f, 0.0f } : remixapi_Float3D{ 1.0f, 1.0f, 1.0f };
			opaque.opacityConstant = 1.0f;
			opaque.roughnessConstant = 0.7f;
			opaque.metallicConstant = 0.1f;
			opaque.useDrawCallAlphaState = TRUE;
			opaque.alphaTestType = static_cast<int>(VK_COMPARE_ALWAYS);
			opaque.alphaReferenceValue = 0u;
			opaque.enableDlssControlMask = FALSE;
			opaque.dlssControlMaskIntensity = 1.0f;
			opaque.dlssControlMaskToneStrength = 1.0f;
			opaque.dlssControlMaskStructuralStrength = 1.0f;

			destroy_handle(m.stale);
			m.stale = m.handle;
			m.handle = nullptr;

			const remixapi_MaterialInfo info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,
				.pNext = &opaque,
				.hash = base_hash + (m.serial++ & 1u),
				.albedoTexture = L"",
				.normalTexture = L"",
				.tangentTexture = L"",
				.emissiveTexture = L"",
				.emissiveIntensity = green ? 20.0f : (emissive ? 1.0f : 0.0f),
				.emissiveColorConstant = green ? remixapi_Float3D{ 0.0f, 1.0f, 0.0f } : (emissive ? remixapi_Float3D{ 1.0f, 1.0f, 1.0f } : remixapi_Float3D{ 0.0f, 0.0f, 0.0f }),
				.filterMode = 1,
				.wrapModeU = 1,
				.wrapModeV = 1,
			};

			if (shared::common::remix_api::get().m_bridge.CreateMaterial(&info, &m.handle) != REMIXAPI_ERROR_CODE_SUCCESS || !m.handle)
			{
				m.handle = nullptr;
				return false;
			}

			m.texture = texture;
			return true;
		}

		IDirect3DBaseTexture9* bound_texture(IDirect3DDevice9* dev)
		{
			IDirect3DBaseTexture9* texture = nullptr;
			dev->GetTexture(0, &texture);
			if (texture) {
				texture->Release(); // the game keeps its reference
			}
			return texture;
		}

		// the game bound the corona texture for its im shader (rage::grcBindTexture); the skipped grcBegin never puts it on sampler 0
		IDirect3DBaseTexture9* im_texture(IDirect3DDevice9* dev)
		{
			if (game::pCurrentImTexture && *game::pCurrentImTexture) {
				return static_cast<const game::grcTexturePC*>(*game::pCurrentImTexture)->d3d_texture_handle;
			}
			return bound_texture(dev);
		}

		void set_emissive_scale(remixapi_InstanceInfoD3DOverridesEXT& overrides, const float emissive_scale)
		{
			overrides.modifierFlags |= static_cast<std::uint32_t>(RemixModifier::EmissiveScalar);
			overrides.emissiveScale = std::max(emissive_scale, 0.0f);
		}

		// additive blending: the runtime treats the instance as emissive (its albedo texture is the emission) and draws it
		// unordered, like a particle: fine for the static sprites, moving geometry would lose its motion vectors
		void make_additive_emissive(remixapi_InstanceInfoBlendEXT& blend, remixapi_InstanceInfoD3DOverridesEXT& overrides, const float emissive_scale)
		{
			blend.alphaBlendEnabled = TRUE;
			blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blend.colorBlendOp = VK_BLEND_OP_ADD;
			blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blend.alphaBlendOp = VK_BLEND_OP_ADD;
			set_emissive_scale(overrides, emissive_scale);
		}

		// ----- sprites

		void destroy_sprites()
		{
			destroy_mesh(g_sprites.mesh);
			destroy_material(g_sprites.material);
		}

		// the billboards face the camera: their normal is the camera's back axis, pointing from the sprites to the camera
		void capture_sprite_state(IDirect3DDevice9* dev, const float* first_position)
		{
			auto& s = g_sprites;
			s.blend = renderer_api::capture_blend(dev);
			s.state_captured = true;

			const auto viewport = game::pCurrentViewport;
			if (!viewport || !viewport->wp) {
				return;
			}

			const auto& cam = viewport->wp->cameraInv;
			const float to_camera[3] = { cam.m[3][0] - first_position[0], cam.m[3][1] - first_position[1], cam.m[3][2] - first_position[2] };
			const float dot = cam.m[2][0] * to_camera[0] + cam.m[2][1] * to_camera[1] + cam.m[2][2] * to_camera[2];
			const float sign = dot < 0.0f ? -1.0f : 1.0f;

			for (auto i = 0; i < 3; i++) {
				s.normal[i] = cam.m[2][i] * sign;
			}
		}

		void add_sprite_vertex(const float* p, const float* offset, const float sign, const float u, const float v, const DWORD color)
		{
			remixapi_HardcodedVertex vert = {};
			vert.position[0] = p[0] + offset[0] * sign;
			vert.position[1] = p[1] + offset[1] * sign;
			vert.position[2] = p[2] + offset[2] * sign;
			vert.normal[0] = g_sprites.normal[0];
			vert.normal[1] = g_sprites.normal[1];
			vert.normal[2] = g_sprites.normal[2];
			vert.texcoord[0] = u;
			vert.texcoord[1] = v;
			vert.color = color;
			g_sprites.vertices.push_back(vert);
		}

		// size multiplier of the sprites under a weather type: the game enlarges them in fog
		float sprite_size_scale(const game::eWeatherType weather)
		{
			static const auto gs = comp_settings::get();
			const auto& setting = weather == game::WEATHER_FOGGY ? gs->api_distant_lights_size_scale_foggy : gs->api_distant_lights_size_scale;
			return std::max(setting._float(), 0.0f);
		}

		// the multiplier of the current weather: the game blends its previous weather into the new one with weather_change_value
		// (0 = previous, 1 = new), the sprite size follows the same blend
		float current_sprite_size_scale()
		{
			if (!game::weather_type_prev || !game::weather_type_new || !game::weather_change_value) {
				return sprite_size_scale(game::WEATHER_NONE);
			}

			const float blend = std::clamp(*game::weather_change_value, 0.0f, 1.0f);
			return std::lerp(sprite_size_scale(*game::weather_type_prev), sprite_size_scale(*game::weather_type_new), blend);
		}

		// CDistantLights::RenderOutSpriteBuffer: 1820 sprites at most per flush, one grcBegin / grcEnd draw each
		void __cdecl CDistantLights_RenderOutSpriteBuffer_hk(const float* positions, const DWORD* colors, int* count, const float* right, const float* up)
		{
			PROFILE_ZONE_N("CDistantLights::RenderOutSpriteBuffer");
			auto& s = g_sprites;

			if (!s.capturing)
			{
				if (s.in_render && *count > 0) {
					s.flushes++;
				}
				return CDistantLights_RenderOutSpriteBuffer_og(positions, colors, count, right, up);
			}

			const int n = *count;
			*count = 0;
			if (n <= 0) {
				return;
			}

			s.flushes++;

			if (!s.state_captured) {
				capture_sprite_state(shared::globals::d3d_device, positions);
			}

			const float scale = current_sprite_size_scale();
			const float r_minus_u[3] = { (right[0] - up[0]) * scale, (right[1] - up[1]) * scale, (right[2] - up[2]) * scale };
			const float r_plus_u[3] = { (right[0] + up[0]) * scale, (right[1] + up[1]) * scale, (right[2] + up[2]) * scale };

			s.vertices.reserve(s.vertices.size() + static_cast<std::size_t>(n) * 4u);
			s.indices.reserve(s.indices.size() + static_cast<std::size_t>(n) * 6u);

			// the two triangles of the game: (p-(r-u), p+(r-u), p-(r+u)) and (p+(r-u), p+(r+u), p-(r-u))
			for (int i = 0; i < n; i++)
			{
				const float* p = positions + static_cast<std::size_t>(i) * SPRITE_STRIDE;
				const DWORD color = colors[i];
				const auto base = static_cast<std::uint32_t>(s.vertices.size());

				add_sprite_vertex(p, r_minus_u, -1.0f, 0.0f, 1.0f, color);
				add_sprite_vertex(p, r_minus_u, 1.0f, 1.0f, 0.0f, color);
				add_sprite_vertex(p, r_plus_u, -1.0f, 0.0f, 0.0f, color);
				add_sprite_vertex(p, r_plus_u, 1.0f, 1.0f, 1.0f, color);
				s.indices.insert(s.indices.end(), { base, base + 1u, base + 2u, base + 1u, base + 3u, base });
			}

			s.count += static_cast<std::uint32_t>(n);
		}

		// one mesh for all sprites of the render, the last render's mesh is destroyed once this one is drawn
		void submit_sprites()
		{
			static const auto gs = comp_settings::get();
			auto& s = g_sprites;
			const auto& bridge = shared::common::remix_api::get().m_bridge;

			if (s.indices.empty() || !s.material.handle)
			{
				destroy_mesh(s.mesh);
				return;
			}

			const remixapi_MeshInfoSurfaceTriangles triangles =
			{
				.vertices_values = s.vertices.data(),
				.vertices_count = static_cast<std::uint32_t>(s.vertices.size()),
				.indices_values = s.indices.data(),
				.indices_count = static_cast<std::uint32_t>(s.indices.size()),
				.skinning_hasvalue = FALSE,
				.skinning_value = {},
				.material = s.material.handle,
			};

			const remixapi_MeshInfo mesh_info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
				.pNext = nullptr,
				.hash = shared::utils::string_hash64("api_distant_lights_mesh") + (s.serial++ & 1u),
				.surfaces_values = &triangles,
				.surfaces_count = 1u,
			};

			remixapi_MeshHandle mesh = nullptr;
			if (bridge.CreateMesh(&mesh_info, &mesh) != REMIXAPI_ERROR_CODE_SUCCESS || !mesh)
			{
				destroy_mesh(s.mesh);
				return;
			}

			// the game's im shader: texture x vertex colour, the colour carries fade and flicker per sprite
			remixapi_InstanceInfoD3DOverridesEXT overrides = {};
			overrides.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_D3D_OVERRIDES_EXT;

			remixapi_InstanceInfoBlendEXT blend = s.blend;
			blend.textureColorOperation = TEXTURE_OP_MODULATE;
			blend.textureColorArg1Source = TEXTURE_ARG_TEXTURE;
			blend.textureColorArg2Source = TEXTURE_ARG_VERTEX_COLOR0;
			blend.textureAlphaOperation = TEXTURE_OP_MODULATE;
			blend.textureAlphaArg1Source = TEXTURE_ARG_TEXTURE;
			blend.textureAlphaArg2Source = TEXTURE_ARG_VERTEX_COLOR0;
			blend.isTextureFactorBlend = FALSE;
			blend.isVertexColorBakedLighting = FALSE;
			make_additive_emissive(blend, overrides, gs->api_distant_lights_emissive_scale.get_as<float>());
			blend.pNext = &overrides;

			remixapi_InstanceInfo instance = {};
			instance.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
			instance.pNext = &blend;
			instance.mesh = mesh;
			instance.transform.matrix[0][0] = 1.0f;
			instance.transform.matrix[1][1] = 1.0f;
			instance.transform.matrix[2][2] = 1.0f;
			instance.doubleSided = TRUE;

			renderer_api::submit_camera();
			bridge.DrawInstance(&instance);

			destroy_mesh(s.mesh);
			destroy_handle(s.material.stale);
			s.mesh = mesh;
		}

		int __cdecl CDistantLights_Render_hk(float night_fade)
		{
			PROFILE_ZONE_N("CDistantLights::Render");
			static const auto gs = comp_settings::get();
			auto& s = g_sprites;
			auto& st = g_stats;

			if (!gs->distant_lights_enabled.get_as<bool>())
			{
				st.sprites_rendered = false;
				destroy_sprites();
				return 0;
			}

			const bool want_api = gs->api_distant_lights_enabled.get_as<bool>() && renderer_api::is_runtime_supported();

			// the runtime only has a game texture once a D3D9 draw uploaded it: the first render with a new texture stays
			// on D3D9 (warm up), its api material is created the render after
			if (want_api && s.last_texture && (!s.material.handle || s.material.texture != s.last_texture)) {
				create_material(s.material, s.last_texture, shared::utils::string_hash64("api_distant_lights_material"), false, false);
			}

			s.in_render = true;
			s.capturing = want_api && s.material.handle && s.material.texture == s.last_texture;
			s.warming_up = want_api && !s.capturing;
			s.vertices.clear();
			s.indices.clear();
			s.count = 0u;
			s.flushes = 0u;
			s.state_captured = false;

			const auto result = CDistantLights_Render_og(night_fade);

			const bool api = s.capturing;
			s.capturing = false;
			s.in_render = false;

			const auto texture = im_texture(shared::globals::d3d_device);
			if (texture != s.last_texture)
			{
				// a new texture: sprites captured with the old material are dropped, the next render warms it up
				s.last_texture = texture;
				destroy_sprites();
			}
			else if (api) {
				submit_sprites();
			}

			if (!want_api) {
				destroy_sprites();
			}

			st.sprites_rendered = true;
			st.sprites_api = api;
			st.sprites_warming_up = s.warming_up;
			st.sprites = s.count;
			st.sprite_flushes = s.flushes;
			st.sprite_vertices = static_cast<std::uint32_t>(s.vertices.size());
			st.sprite_triangles = static_cast<std::uint32_t>(s.indices.size() / 3u);
			return result;
		}

		// ----- cars

		void destroy_car_mesh(car_mesh_s& m)
		{
			destroy_mesh(m.mesh);
			destroy_material(m.material);
			m.vb = nullptr;
		}

		void destroy_cars()
		{
			for (auto& row : g_cars.meshes)
			{
				for (auto& m : row) {
					destroy_car_mesh(m);
				}
			}
		}

		std::uint32_t car_mesh_count()
		{
			std::uint32_t n = 0u;
			for (const auto& row : g_cars.meshes)
			{
				for (const auto& m : row) {
					n += m.mesh ? 1u : 0u;
				}
			}
			return n;
		}

		DWORD pack_color(const float* rgb, const float alpha)
		{
			const auto channel = [](const float v) { return static_cast<DWORD>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return (channel(alpha) << 24) | (channel(rgb[0]) << 16) | (channel(rgb[1]) << 8) | channel(rgb[2]);
		}

		int render_cars(const bool night, const VehicleImposters_RenderNight_t og)
		{
			static const auto gs = comp_settings::get();
			const auto im = imgui::get();
			auto& c = g_cars;
			auto& st = g_stats;

			if (!gs->distant_cars_enabled.get_as<bool>())
			{
				st.cars_rendered = false;
				destroy_cars();
				return 0;
			}

			if (c.green != im->m_dbg_distant_cars_green_material)
			{
				c.green = im->m_dbg_distant_cars_green_material;
				destroy_cars();
			}

			c.rendering = true;
			c.night = night;
			c.api = gs->api_distant_cars_enabled.get_as<bool>() && renderer_api::is_runtime_supported();
			c.render_serial++;
			c.flushes = 0u;
			c.warmups = 0u;
			c.records_total = 0u;
			c.instances = 0u;
			c.reject = renderer_api::reject_reason::none;

			const auto result = og();

			c.rendering = false;
			c.in_flush = false;
			c.capturing = false;

			if (!c.api) {
				destroy_cars();
			}

			st.cars_rendered = true;
			st.cars_night = night;
			st.cars_api = c.api;
			st.car_records = c.records_total;
			st.car_flushes = c.flushes;
			st.car_warmups = c.warmups;
			st.car_instances = c.instances;
			st.car_meshes = car_mesh_count();
			st.car_reject = c.reject;
			return result;
		}

		int __cdecl VehicleImposters_RenderNight_hk()
		{
			PROFILE_ZONE_N("VehicleImposters::RenderNight");
			return render_cars(true, VehicleImposters_RenderNight_og);
		}

		int __cdecl VehicleImposters_RenderDay_hk()
		{
			PROFILE_ZONE_N("VehicleImposters::RenderDay");
			// no sprite render by day, nothing references the last sprite mesh anymore
			destroy_mesh(g_sprites.mesh);
			return render_cars(false, VehicleImposters_RenderDay_og);
		}

		// uploads up to 1820 records into the instance vertex buffer and issues the instanced draw (try_draw_cars replaces it)
		void __cdecl VehicleImposters_FlushInstances_hk(const float* records, int* count, const float* right, const float* up, int model)
		{
			PROFILE_ZONE_N("VehicleImposters::FlushInstances");
			auto& c = g_cars;

			if (!c.rendering || *count <= 0) {
				return VehicleImposters_FlushInstances_og(records, count, right, up, model);
			}

			c.flushes++;
			c.records_total += static_cast<std::uint32_t>(*count);

			c.in_flush = true;
			c.capturing = c.api && model >= 0 && model < CAR_MODELS;
			c.drawn = false;
			c.records = records;
			c.count = *count;
			c.model = model;

			VehicleImposters_FlushInstances_og(records, count, right, up, model);

			c.in_flush = false;
			c.capturing = false;
			c.records = nullptr;
			c.count = 0;
		}
	}

	bool distant_lights::try_draw_cars(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE type, const INT base_vertex, const UINT min_index, const UINT num_vertices, const UINT start_index, const UINT prim_count)
	{
		static const auto gs = comp_settings::get();
		auto& c = g_cars;

		if (!c.capturing || !c.records) {
			return false;
		}

		// every pass of the technique draws the same instances, the first one is enough
		if (c.drawn) {
			return true;
		}

		if (type != D3DPT_TRIANGLELIST) {
			return false;
		}

		auto& m = c.meshes[c.night ? 1 : 0][c.model];
		const auto texture = bound_texture(dev);
		const auto index = static_cast<std::uint64_t>((c.night ? CAR_MODELS : 0) + c.model);

		IDirect3DVertexBuffer9* vb = nullptr;
		UINT offset = 0u, stride = 0u;
		if (SUCCEEDED(dev->GetStreamSource(0, &vb, &offset, &stride)) && vb) {
			vb->Release();
		}

		// the mesh binds its material: a new material needs a new mesh
		if (!m.material.handle || m.material.texture != texture)
		{
			// the runtime only has a game texture once a D3D9 draw uploaded it: this flush stays on D3D9 (warm up),
			// the api material is created the render after
			if (!c.green && (m.warm_texture != texture || m.warm_render == c.render_serial))
			{
				m.warm_texture = texture;
				m.warm_render = c.render_serial;
				c.warmups++;
				return false;
			}

			destroy_mesh(m.mesh);
			if (!create_material(m.material, texture, shared::utils::string_hash64("api_distant_cars_material") + index * 2u, c.night, c.green))
			{
				c.reject = renderer_api::reject_reason::material_failed;
				return false;
			}
		}

		if (m.mesh && (m.vb != vb || m.start_index != start_index || m.prim_count != prim_count)) {
			destroy_mesh(m.mesh);
		}

		if (!m.mesh)
		{
			m.mesh = renderer_api::create_mesh_from_bound_draw(dev, base_vertex, min_index, num_vertices, start_index, prim_count,
				shared::utils::string_hash64("api_distant_cars_mesh") + index, m.material.handle, &c.reject);

			if (!m.mesh) {
				return false;
			}

			m.vb = vb;
			m.start_index = start_index;
			m.prim_count = prim_count;
			destroy_handle(m.material.stale);
		}

		// the colour of every car rides in the texture factor (the instanced shader multiplies it in), the model's vertex
		// colours stay out of it. No alpha at all: the game's distance fade would make the opaque day cars disappear, so it
		// scales the brightness of the night headlights instead
		auto blend = renderer_api::capture_blend(dev);
		blend.textureColorOperation = TEXTURE_OP_SELECT_ARG1;
		blend.textureColorArg1Source = TEXTURE_ARG_TEXTURE;
		blend.textureColorArg2Source = TEXTURE_ARG_TFACTOR;
		blend.isTextureFactorBlend = !c.green;
		blend.textureAlphaOperation = TEXTURE_OP_SELECT_ARG2;
		blend.textureAlphaArg1Source = TEXTURE_ARG_TEXTURE;
		blend.textureAlphaArg2Source = TEXTURE_ARG_TFACTOR;
		blend.alphaTestEnabled = FALSE;
		blend.alphaBlendEnabled = FALSE;
		blend.isVertexColorBakedLighting = FALSE;

		remixapi_InstanceInfoD3DOverridesEXT overrides = {};
		overrides.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_D3D_OVERRIDES_EXT;
		overrides.categoryFlags = static_cast<std::uint32_t>(InstanceCategories::IgnoreAlphaChannel);

		// the game draws the headlights unlit; their material emits its texture (colorIsEmissive), scaled per instance
		if (c.night && !c.green) {
			set_emissive_scale(overrides, gs->api_distant_cars_emissive_scale.get_as<float>());
		}
		blend.pNext = &overrides;

		remixapi_InstanceInfo instance = {};
		instance.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
		instance.pNext = &blend;
		instance.categoryFlags = REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ALPHA_CHANNEL;
		instance.mesh = m.mesh;
		instance.doubleSided = TRUE; // flat headlight quads, seen from both sides

		const auto& bridge = shared::common::remix_api::get().m_bridge;
		renderer_api::submit_camera();

		for (int i = 0; i < c.count; i++)
		{
			const float* rec = c.records + static_cast<std::size_t>(i) * CAR_RECORD_FLOATS;

			// rows are the basis vectors (row vector convention), the api transform wants them as columns
			for (auto r = 0; r < 3; r++)
			{
				for (auto col = 0; col < 3; col++) {
					instance.transform.matrix[r][col] = rec[col * 4 + r];
				}
				instance.transform.matrix[r][3] = rec[12 + r];
			}

			// the texture factor is part of the material hash the runtime matches instances with between frames (motion
			// vectors), so the night fade is quantized: a car only re-matches as a new instance when it crosses a step
			const float fade = c.night ? std::round(std::clamp(rec[19], 0.0f, 1.0f) * FADE_STEPS) / FADE_STEPS : 1.0f;
			const float rgb[3] = { rec[16] * fade, rec[17] * fade, rec[18] * fade };
			blend.tFactor = pack_color(rgb, 1.0f);
			bridge.DrawInstance(&instance);
		}

		c.instances += static_cast<std::uint32_t>(c.count);
		c.drawn = true;
		return true;
	}

	void distant_lights::destroy_all()
	{
		destroy_sprites();
		destroy_cars();
	}

	distant_lights::stats_s distant_lights::get_stats() {
		return g_stats;
	}

	void distant_lights::devgui()
	{
		const auto im = imgui::get();
		const auto s = get_stats();

		if (!renderer_api::is_runtime_supported()) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Remix API not available - api path unavailable");
		}

		ImGui::TextDisabled("The game's 'vehicle imposters': street light / aircraft sprites and fake cars along the road light strings at night, fake cars by day.");
		ImGui::TextDisabled("Sprites: one api mesh per frame. Cars: the instanced model as cached api mesh, one api instance per car.");
		ImGui::TextDisabled("A material of a game texture is keyed by the texture's image hash: rtx.conf texture lists and replacements apply like on the D3D9 path.");

		ImGui::Spacing(0, 4);
		ImGui::PushID("devimpost");

		ImGui::SeparatorText("Debug");
		ImGui::Checkbox("Cars: solid green emissive material (no texture, no key)", &im->m_dbg_distant_cars_green_material);

		ImGui::Spacing(0, 8);
		ImGui::SeparatorText("Last render");

		if (!s.sprites_rendered) {
			ImGui::TextUnformatted("Sprites: not rendered (day, disabled or -disableimposters)");
		} else if (s.sprites_api) {
			ImGui::Text("Sprites: %u in %u game flushes -> Remix API, one mesh with %u vertices, %u triangles", s.sprites, s.sprite_flushes, s.sprite_vertices, s.sprite_triangles);
		} else {
			ImGui::Text("Sprites: %u game flushes drawn through D3D9%s", s.sprite_flushes, s.sprites_warming_up ? " (texture warm up, api from the next render)" : "");
		}

		ImGui::Text("Sprite size scale: %.2f (weather blend %.2f of previous -> new)", current_sprite_size_scale(),
			game::weather_change_value ? std::clamp(*game::weather_change_value, 0.0f, 1.0f) : 1.0f);

		if (!s.cars_rendered) {
			ImGui::TextUnformatted("Cars: not rendered (disabled or -disableimposters)");
		}
		else
		{
			ImGui::Text("Cars (%s): %u records in %u game flushes (%u texture warm ups) -> %s, %u api instances, %u cached meshes",
				s.cars_night ? "night headlights" : "day models", s.car_records, s.car_flushes, s.car_warmups, s.cars_api ? "Remix API" : "D3D9", s.car_instances, s.car_meshes);

			if (s.car_reject != renderer_api::reject_reason::none) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Cars stayed on D3D9: %s", renderer_api::reject_reason_name(s.car_reject));
			}
		}

		ImGui::PopID();
		ImGui::Spacing(0, 4);
	}

	distant_lights::distant_lights()
	{
		if (!game::func_addr__CDistantLights_Render || !game::func_addr__CDistantLights_RenderOutSpriteBuffer || !game::func_addr__VehicleImposters_RenderNight
			|| !game::func_addr__VehicleImposters_RenderDay || !game::func_addr__VehicleImposters_FlushInstances || !game::pCurrentViewport)
		{
			shared::common::log("DistantLights", "Missing game addresses, module disabled.", shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return;
		}

		shared::utils::hook::detour(game::func_addr__CDistantLights_Render, &CDistantLights_Render_hk, DETOUR_CAST(CDistantLights_Render_og));
		shared::utils::hook::detour(game::func_addr__CDistantLights_RenderOutSpriteBuffer, &CDistantLights_RenderOutSpriteBuffer_hk, DETOUR_CAST(CDistantLights_RenderOutSpriteBuffer_og));
		shared::utils::hook::detour(game::func_addr__VehicleImposters_RenderNight, &VehicleImposters_RenderNight_hk, DETOUR_CAST(VehicleImposters_RenderNight_og));
		shared::utils::hook::detour(game::func_addr__VehicleImposters_RenderDay, &VehicleImposters_RenderDay_hk, DETOUR_CAST(VehicleImposters_RenderDay_og));
		shared::utils::hook::detour(game::func_addr__VehicleImposters_FlushInstances, &VehicleImposters_FlushInstances_hk, DETOUR_CAST(VehicleImposters_FlushInstances_og));

		p_this = this;
		shared::common::log("DistantLights", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
