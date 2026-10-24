#include "std_include.hpp"
#include "renderer_api.hpp"

#include "comp_settings.hpp"
#include "d3d9ex.hpp"
#include "imgui.hpp"
#include "natives.hpp"
#include "performance_logger.hpp"
#include "renderer.hpp"
#include "timecycle.hpp"
#include "water.hpp"

#include <mutex>
#include <shared_mutex>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace gta4
{
	namespace
	{
		// #
		// engine structures (PC 1.2.0.59, verified in IDA)

		struct grcVertexBufferD3D
		{
			void* vtbl;
			std::uint16_t vertex_count;
			std::uint8_t locked;
			std::uint8_t dynamic;
			void* lock_ptr;
			std::uint32_t stride;
			void* fvf;
			std::uint32_t lock_timestamp;
			void* resource_data;				// zeroed after the D3D buffer was created
			IDirect3DVertexBuffer9* d3d_buffer;	// created lazily at the first draw
			const std::uint8_t* cpu_data;		// persistent copy, freed in the destructor (null after a defrag re-place)
		};
		static_assert(offsetof(grcVertexBufferD3D, cpu_data) == 0x20);

		struct grcIndexBufferD3D
		{
			void* vtbl;
			std::uint32_t index_count;
			void* resource_data;
			IDirect3DIndexBuffer9* d3d_buffer;
			const std::uint16_t* cpu_data;		// persistent copy
		};
		static_assert(offsetof(grcIndexBufferD3D, cpu_data) == 0x10);

		struct grcVertexDeclaration
		{
			IDirect3DVertexDeclaration9* d3d_decl;	// created lazily
			std::uint32_t ref_count;
			std::uint32_t stream_frequency;
			D3DVERTEXELEMENT9 elements[1];			// terminated with D3DDECL_END()
		};
		static_assert(offsetof(grcVertexDeclaration, elements) == 0x0C);

		struct rmcLod
		{
			struct grmModel** models;
			std::uint16_t model_count;
			std::uint16_t model_capacity;
		};

		struct rmcDrawable
		{
			void* vtbl;
			std::uint32_t pad04;
			void* shader_group;
			void* skeleton_data;
			float lod_center[4];
			float lod_aabb_min[4];
			float lod_aabb_max[4];
			rmcLod* lods[4];
			float lod_thresholds[4];
			std::uint32_t lod_bucket_masks[4];
			float lod_radius;
		};
		static_assert(offsetof(rmcDrawable, lods) == 0x40);

		struct CEntityPC
		{
			void* vtbl;
			std::uint32_t pad04[3];
			float simple_pos[3];	// used when matrix is null
			float simple_heading;
			float (*matrix)[4];		// rage::Matrix34, 4 rows of 4 floats (right, forward, up, position)
			std::uint32_t flags;
			std::uint32_t flags2;	// entity type in bits 6-9
			std::uint16_t random;
			std::int16_t model_index;
			std::uint32_t pad30;
			rmcDrawable** drawable_handle;
			std::uint8_t pad38[0x2B];
			std::uint8_t alpha;
		};
		static_assert(offsetof(CEntityPC, matrix) == 0x20);
		static_assert(offsetof(CEntityPC, model_index) == 0x2E);
		static_assert(offsetof(CEntityPC, drawable_handle) == 0x34);
		static_assert(offsetof(CEntityPC, alpha) == 0x63);

		constexpr std::uint32_t ENTITY_FLAG_FRAGMENT = 0x4000000u;
		constexpr std::uint32_t ENTITY_TYPE_BUILDING = 1u;
		constexpr std::uint32_t RENDER_PHASE_GBUFFER = 0x1Fu;
		constexpr std::uint32_t RENDER_PHASE_DRAW_SCENE = 0xFu;
		constexpr std::uint32_t ENTITY_DC_SIZE = 64u;
		constexpr std::uint32_t MAX_SKIP_PASSES = 4u;

		using namespace remix_draw_state;

		// D3DBLEND (1..11) -> VkBlendFactor
		constexpr std::uint32_t D3D_BLEND_TO_VK[] = { 1u, 0u, 1u, 2u, 3u, 6u, 7u, 8u, 9u, 4u, 5u, 10u };

		std::uint32_t to_vk_blend_factor(const DWORD d3d_blend, const std::uint32_t fallback)
		{
			return d3d_blend && d3d_blend < std::size(D3D_BLEND_TO_VK) ? D3D_BLEND_TO_VK[d3d_blend] : fallback;
		}

		std::uint32_t to_vk_blend_op(const DWORD d3d_op)
		{
			return d3d_op >= D3DBLENDOP_ADD && d3d_op <= D3DBLENDOP_MAX ? d3d_op - 1u : VK_BLEND_OP_ADD;
		}

		std::uint32_t to_vk_compare_op(const DWORD d3d_cmp)
		{
			return d3d_cmp >= D3DCMP_NEVER && d3d_cmp <= D3DCMP_ALWAYS ? d3d_cmp - 1u : VK_COMPARE_ALWAYS;
		}

		std::uint32_t to_texture_arg(const DWORD d3d_arg)
		{
			switch (d3d_arg & D3DTA_SELECTMASK)
			{
			case D3DTA_TEXTURE: return TEXTURE_ARG_TEXTURE;
			case D3DTA_CURRENT:
			case D3DTA_DIFFUSE: return TEXTURE_ARG_VERTEX_COLOR0;
			case D3DTA_TFACTOR: return TEXTURE_ARG_TFACTOR;
			default: return TEXTURE_ARG_NONE;
			}
		}

		std::uint32_t to_texture_op(const DWORD d3d_op)
		{
			switch (d3d_op)
			{
			case D3DTOP_DISABLE: return TEXTURE_OP_DISABLE;
			case D3DTOP_SELECTARG1: return TEXTURE_OP_SELECT_ARG1;
			case D3DTOP_SELECTARG2: return TEXTURE_OP_SELECT_ARG2;
			case D3DTOP_MODULATE2X: return TEXTURE_OP_MODULATE_2X;
			case D3DTOP_MODULATE4X: return TEXTURE_OP_MODULATE_4X;
			case D3DTOP_ADD: return TEXTURE_OP_ADD;
			default: return TEXTURE_OP_MODULATE;
			}
		}

		// #
		// state

		// draw call alpha state as the fixed function path submits it, mirrored into InstanceInfoBlendEXT
		// draw call state as the fixed function path submits it, mirrored into InstanceInfoBlendEXT
		struct draw_state_s
		{
			bool alpha_test = false;
			DWORD alpha_func = D3DCMP_ALWAYS;
			DWORD alpha_ref = 0u;
			bool alpha_blend = false;
			DWORD src_blend = D3DBLEND_ONE;
			DWORD dst_blend = D3DBLEND_ZERO;
			DWORD blend_op = D3DBLENDOP_ADD;
			DWORD color_op = D3DTOP_MODULATE;
			DWORD color_arg1 = D3DTA_TEXTURE;
			DWORD color_arg2 = D3DTA_DIFFUSE;
			DWORD alpha_op = D3DTOP_SELECTARG1;
			DWORD alpha_arg1 = D3DTA_TEXTURE;
			DWORD alpha_arg2 = D3DTA_DIFFUSE;
			DWORD tfactor = 0xFFFFFFFFu; // per instance, not part of the key
			DWORD cull_mode = D3DCULL_CCW;
			bool double_sided = false;
			DWORD uv_set = 0u; // D3DTSS_TEXCOORDINDEX (vehicle dirt / livery passes use the second set)

			// D3D enum values only, so the key is identical across sessions (part of the mesh hash)
			std::uint64_t key() const
			{
				std::uint64_t h = 1469598103934665603ull;
				const auto mix = [&h](const std::uint32_t v) { h ^= v; h *= 1099511628211ull; };
				mix(alpha_test); mix(alpha_func); mix(alpha_ref); mix(alpha_blend); mix(src_blend); mix(dst_blend); mix(blend_op);
				mix(color_op); mix(color_arg1); mix(color_arg2); mix(alpha_op); mix(alpha_arg1); mix(alpha_arg2); mix(double_sided);
				if (uv_set) {
					mix(uv_set);
				}
				// the game draws some geometry twice, culling clockwise and counter clockwise: one record per side
				if (cull_mode == D3DCULL_CW) {
					mix(cull_mode);
				}
				return h;
			}
		};

		struct material_record
		{
			remixapi_MaterialHandle handle = nullptr;
			draw_state_s state = {};
			remixapi_InstanceInfoBlendEXT blend = {};
			std::string colormap_name;
			std::string shader_name;
			IDirect3DBaseTexture9* colormap = nullptr;
		};

		struct mesh_record
		{
			remixapi_MeshHandle mesh = nullptr;
			std::uint64_t material_key = 0u;
			bool vertex_colors_dropped = false;
			std::uint32_t bytes = 0u;
			std::uint32_t vertex_count = 0u;
			std::uint32_t index_count = 0u;
			std::uint32_t last_used_frame = 0u;
			std::uint32_t bone_count = 0u; // skinned mesh: bone transforms every instance has to carry
			bool transient = false;        // a shape of one frame (flat tyre), destroyed once unused
			bool uv_fallback = false;      // built from UV0 because the requested texcoord set was not usable
			std::uint64_t replacement_hash = 0u; // geometry asset hash of the D3D9 draw (USD replacement key), 0 = none
		};

		// one per geometry and draw state (a geometry can be drawn by several passes with different states)
		struct geometry_record
		{
			std::uint64_t state_key = 0u;
			bool owned = false;
			bool hidden = false; // the draw carries the Ignore category: owned but never submitted
			renderer_api::reject_reason reason = renderer_api::reject_reason::none;
			std::uint64_t mesh_hash = 0u; // key into g_meshes, also the api mesh handle
			std::uint32_t last_frame = 0u;
			remixapi_InstanceInfoD3DOverridesEXT overrides = {}; // of the last draw that went through the draw hook
			std::uint64_t vertex_hash = 0u; // double buffered geometry: the vertices the mesh was built from
			DWORD tfactor = 0xFFFFFFFFu;
			IDirect3DBaseTexture9* texture = nullptr; // part of the key of ped / vehicle records
		};

		struct entity_frames_s
		{
			std::uint32_t api_frame = 0u;    // last game frame our command replaced the engine's
			std::uint32_t engine_frame = 0u; // last game frame the engine drew the entity (refreshes the records)
			std::uint32_t generation = 0u;   // g_overrides_generation the engine last drew it under
		};

		// the per draw values captured in records (wetness, raindrops, emissive night window) come from a few globals;
		// geometry the engine no longer draws is drawn once more when they change so its records are captured again
		std::uint32_t g_overrides_generation = 1u;
		std::uint32_t g_overrides_generation_game_frame = 0u;
		std::uint64_t g_overrides_fingerprint = 0u;
		constexpr std::uint32_t OVERRIDES_REFRESH_SPREAD = 8u; // entities refresh over this many frames

		std::uint64_t overrides_fingerprint()
		{
			// EndScene arrives before the modules are up
			const auto gs = comp_settings::get();
			const auto im = imgui::get();

			if (!gs || !im) {
				return g_overrides_fingerprint;
			}

			// the wetness the world draws derive their roughness from (renderer.cpp), 1/64 steps
			const float wetness = im->m_dbg_global_wetness_override ? im->m_dbg_global_wetness : timecycle::get_wetness();

			std::uint64_t h = static_cast<std::uint32_t>(wetness * 64.0f + 0.5f);
			const auto mix = [&h](const std::uint32_t v) {
					h = (h ^ v) * 0x9E3779B97F4A7C15ull;
				};

			mix(im->m_dbg_global_wetness_override ? 1u : 0u);
			mix(should_apply_heavy_raindrops() ? 1u : 0u);
			mix(should_apply_light_raindrops() ? 1u : 0u);
			mix(game::m_game_clock_hours ? *game::m_game_clock_hours : 0u); // emissive night windows are hour based

			// every setting renderer.cpp packs into the wetness render state (flags, normal blend, raindrops), for the
			// world, vehicle and ped branches; records owned by the api path are only recaptured when this changes
			const bool toggles[] =
			{
				gs->timecycle_wetness_enabled._bool(),
				gs->timecycle_wetness_world_variation_enable._bool(), gs->timecycle_wetness_world_puddle_layer_enable._bool(),
				gs->timecycle_wetness_world_raindrop_enable._bool(),
				gs->timecycle_wetness_world_occlusion_check_enable._bool(), gs->timecycle_wetness_world_occlusion_smoothing_enable._bool(),
				gs->timecycle_wetness_vehicle_raindrop_enable._bool(), gs->timecycle_wetness_ped_raindrop_enable._bool(),
				im->m_dbg_global_wetness_occl_allow_on_ignored,
			};

			for (const auto t : toggles) {
				mix(t ? 1u : 0u);
			}

			const float settings[] = 
			{
				gs->timecycle_wetness_world_scalar.get_as<float>(), gs->timecycle_wetness_world_offset.get_as<float>(),
				gs->timecycle_wetness_world_z_normal.get_as<float>(), gs->timecycle_wetness_world_blending.get_as<float>(),
				gs->timecycle_wetness_world_raindrop_scalar.get_as<float>(),
				gs->timecycle_wetness_vehicle_scalar.get_as<float>(), gs->timecycle_wetness_vehicle_z_normal.get_as<float>(),
				gs->timecycle_wetness_vehicle_blending.get_as<float>(), gs->timecycle_wetness_vehicle_raindrop_scalar.get_as<float>(),
				gs->timecycle_wetness_vehicle_dirt_roughness_scalar.get_as<float>(), gs->timecycle_wetness_vehicle_dirt_z_normal.get_as<float>(),
				gs->timecycle_wetness_vehicle_dirt_blending.get_as<float>(), gs->timecycle_wetness_vehicle_dirt_intensity_scalar.get_as<float>(),
				gs->timecycle_wetness_ped_raindrop_scalar.get_as<float>(),
				gs->emissive_night_surfaces_emissive_scalar.get_as<float>(), gs->emissive_generic_scale.get_as<float>(),
			};

			for (const auto f : settings)
			{
				std::uint32_t bits;
				std::memcpy(&bits, &f, sizeof(bits));
				mix(bits);
			}

			return h;
		}

		// one mutex for everything the render thread, the game thread (entity hook) and the streaming code touch
		std::shared_mutex g_mutex;
		std::unordered_map<const void*, std::vector<geometry_record>> g_records; // grmGeometryQB* -> records per draw state
		std::unordered_map<std::uint64_t, mesh_record> g_meshes;    // mesh hash -> api mesh
		std::unordered_map<std::uint64_t, material_record> g_materials; // material key -> api material
		std::uint64_t g_cache_bytes = 0u;
		std::unordered_map<IDirect3DBaseTexture9*, std::vector<std::uint64_t>> g_materials_by_texture; // colormap -> material keys

		// the streaming system writes new content into existing texture objects; the api materials bound to them and the
		// meshes bound to those materials are rebuilt (LockRect / UnlockRect hooks, UpdateTexture / UpdateSurface)
		std::mutex g_written_textures_mutex;
		std::unordered_set<IDirect3DTexture9*> g_write_locked_textures; // level 0 locked for writing, pending until the unlock
		std::unordered_set<IDirect3DBaseTexture9*> g_written_textures;  // written since the last frame start

		struct texture_writes_s
		{
			std::uint32_t last_frame = 0u;
			std::uint32_t streak = 0u; // consecutive frames with a write
			bool is_volatile = false;  // written every frame: its draws stay on D3D9 instead of rebuilding their meshes each frame
		};
		std::unordered_map<IDirect3DBaseTexture9*, texture_writes_s> g_texture_writes;
		constexpr std::uint32_t VOLATILE_TEXTURE_FRAMES = 8u;
		constexpr std::uint32_t VOLATILE_TEXTURE_DECAY_FRAMES = 300u;
		std::uint32_t g_texture_writes_last_frame = 0u;
		std::uint32_t g_texture_rebuilds_last_frame = 0u; // textures whose api materials were destroyed for a rebuild
		std::uint32_t g_volatile_textures = 0u;

		// the passes of a geometry for one shader technique, all owned by the api path (records by state key)
		struct pass_set_s
		{
			std::int32_t technique = -1;
			std::uint64_t signature = 0u; // textures of the shader instance (ped variations, liveries share the geometry)
			std::uint32_t generation = 0u; // g_overrides_generation its records were captured under
			std::uint32_t count = 0u;
			std::uint64_t state_keys[MAX_SKIP_PASSES] = {};
			std::uint32_t refresh_frame = 0u; // from this frame on the engine draws it again (refreshes the per draw values)
		};

		std::unordered_map<const void*, std::vector<pass_set_s>> g_pass_sets; // grmGeometryQB* -> sets per technique

		// passes of the geometry drawn by the current grmShader::DrawGeometry call (render thread)
		struct pass_capture_s
		{
			const void* geometry = nullptr;
			std::int32_t technique = -1;
			std::uint64_t signature = 0u;
			std::uint32_t expected = 0u;
			std::uint32_t owned = 0u;
			bool failed = false;
			std::uint64_t state_keys[MAX_SKIP_PASSES] = {};
		};
		pass_capture_s g_pass_capture;
		std::uint32_t g_pass_skips_this_frame = 0u;
		std::uint32_t g_pass_skips_last_frame = 0u;
		std::uint32_t g_pass_full_draws_this_frame = 0u;
		std::uint32_t g_pass_full_draws_last_frame = 0u;

		enum class pass_stat : std::uint8_t
		{
			full_no_set,
			full_refresh,
			full_stale,
			full_too_many_passes,
			capture_complete,
			capture_missing,
			capture_rejected,
			capture_rejected_ped,
			count
		};

		std::uint32_t g_pass_stats_this_frame[static_cast<std::size_t>(pass_stat::count)] = {};
		std::uint32_t g_pass_stats_last_frame[static_cast<std::size_t>(pass_stat::count)] = {};

		void count_pass_stat(const pass_stat stat) {
			g_pass_stats_this_frame[static_cast<std::size_t>(stat)]++;
		}

		std::uint32_t g_frame = 0u;
		std::uint32_t g_creates_this_frame = 0u;
		std::uint32_t g_cache_hits_this_frame = 0u;
		std::uint32_t g_instances_this_frame = 0u;
		// batching estimate: rigid instances of one frame grouped by mesh and draw overrides (one DrawInstance with
		// GpuInstancingEXT could carry all transforms of a group)
		bool g_count_batch_groups = false;
		std::unordered_set<std::uint64_t> g_batch_groups;
		std::uint32_t g_batch_groups_last_frame = 0u;
		std::uint32_t g_entity_skips_this_frame = 0u;
		std::uint32_t g_entity_fallbacks_this_frame = 0u;
		std::uint32_t g_creates_last_frame = 0u;
		std::uint32_t g_instances_last_frame = 0u;
		std::uint32_t g_entity_skips_last_frame = 0u;
		std::uint32_t g_entity_fallbacks_last_frame = 0u;
		std::uint32_t g_budget_exhausted_frames = 0u;
		bool g_budget_exhausted_this_frame = false;
		std::uint32_t g_modifier_skips_this_frame = 0u;
		std::uint32_t g_modifier_skips_last_frame = 0u;

		// geometry currently drawn by grmGeometryQB::Draw (render thread)
		const game::grmGeometryQB* g_current_geometry = nullptr;

		// RenderInstanced (tyres) draws without grmGeometryQB::Draw, renderer.cpp records its geometry
		const game::grmGeometryQB* current_geometry()
		{
			if (g_current_geometry) {
				return g_current_geometry;
			}
			return g_is_instance_rendering ? static_cast<const game::grmGeometryQB*>(g_instanced_geometry) : nullptr;
		}


		std::vector<remixapi_HardcodedVertex> g_vertices;
		std::vector<std::uint32_t> g_indices;
		std::vector<float> g_blend_weights;          // 4 per vertex
		std::vector<std::uint32_t> g_blend_indices;  // 4 per vertex

		std::uint32_t g_ped_instances_this_frame = 0u;
		std::uint32_t g_ped_pass_skips_this_frame = 0u;
		std::uint32_t g_ped_pass_skips_last_frame = 0u;
		std::uint32_t g_ped_skips_context_this_frame = 0u;
		std::uint32_t g_ped_skips_target_this_frame = 0u;
		std::uint32_t g_ped_skips_stream_this_frame = 0u;
		std::uint32_t g_ped_skips_blendshape_this_frame = 0u;
		std::uint32_t g_ped_skips_rejected_this_frame = 0u;
		std::uint32_t g_ped_instances_last_frame = 0u;
		std::uint32_t g_ped_skips_context_last_frame = 0u;
		std::uint32_t g_ped_skips_target_last_frame = 0u;
		std::uint32_t g_ped_skips_stream_last_frame = 0u;
		std::uint32_t g_ped_skips_blendshape_last_frame = 0u;

		enum class vehicle_stat : std::uint8_t
		{
			instances,
			overlays,
			overlay_uv_fallbacks,
			damaged,
			tyres,
			flat_tyres,
			skips_context,
			skips_overlay,
			skips_target,
			skips_rejected,
			count
		};

		std::uint32_t g_vehicle_stats_this_frame[static_cast<std::size_t>(vehicle_stat::count)] = {};
		std::uint32_t g_vehicle_stats_last_frame[static_cast<std::size_t>(vehicle_stat::count)] = {};

		void count_vehicle_stat(const vehicle_stat stat) {
			g_vehicle_stats_this_frame[static_cast<std::size_t>(stat)]++;
		}

		std::uint32_t g_ped_skips_rejected_last_frame = 0u;

		int g_in_draw_ped_dc = 0;        // inside a ped / player / ped prop / super lod ped draw command
		std::uint32_t g_camera_frame = UINT32_MAX; // frame the camera was last sent for
		bool g_ped_candidate = false;    // the last try_draw_ped call was a ped draw (debug hide)
		bool g_debug_hide_d3d9_peds = false;

		// ped draws kept on D3D9 since the last clear: reason + preset + colormap name (devgui)
		struct kept_ped_draw_s
		{
			std::string reason;
			std::string preset;
			std::string colormap;
		};

		std::vector<kept_ped_draw_s> g_kept_ped_draws;

		// game thread: entity -> frame of its last api submission (dedupes the per bucket pushes)
		std::unordered_map<const void*, entity_frames_s> g_entity_frames;
		std::uint32_t g_current_phase_id = 0u; // id of the render phase whose draw list is being built (game thread)

		// static draws per source, counted in the draw hook
		std::uint32_t g_static_entity_dc_this_frame = 0u;
		std::uint32_t g_static_frag_type_dc_this_frame = 0u;
		std::uint32_t g_static_frag_object_this_frame = 0u;
		std::uint32_t g_static_entity_info_this_frame = 0u;
		std::uint32_t g_static_dynamic_entity_this_frame = 0u;
		std::uint32_t g_static_lights_in_area_this_frame = 0u;
		std::uint32_t g_static_ped_this_frame = 0u;
		std::uint32_t g_static_other_this_frame = 0u;
		std::uint32_t g_static_entity_dc_last_frame = 0u;
		std::uint32_t g_static_frag_type_dc_last_frame = 0u;
		std::uint32_t g_static_frag_object_last_frame = 0u;
		std::uint32_t g_static_entity_info_last_frame = 0u;
		std::uint32_t g_static_dynamic_entity_last_frame = 0u;
		std::uint32_t g_static_lights_in_area_last_frame = 0u;
		std::uint32_t g_static_ped_last_frame = 0u;
		std::uint32_t g_static_other_last_frame = 0u;

		// why a static draw stayed on D3D9
		std::uint32_t g_skips_no_window_this_frame = 0u;
		std::uint32_t g_skips_vehicle_this_frame = 0u;
		std::uint32_t g_skips_ped_this_frame = 0u;
		std::uint32_t g_skips_vehicle_last_frame = 0u;
		std::uint32_t g_skips_ped_last_frame = 0u;

		// static draws of unknown origin seen since the last clear: preset + colormap name (devgui)
		struct unknown_draw_s
		{
			std::string preset;
			std::string colormap;
		};

		std::vector<unknown_draw_s> g_unknown_draws;
		std::uint32_t g_skips_instanced_this_frame = 0u;
		std::uint32_t g_skips_target_this_frame = 0u;
		std::uint32_t g_skips_dual_render_this_frame = 0u;
		std::uint32_t g_skips_no_geometry_this_frame = 0u;
		std::uint32_t g_skips_no_window_last_frame = 0u;
		std::uint32_t g_skips_instanced_last_frame = 0u;
		std::uint32_t g_skips_target_last_frame = 0u;
		std::uint32_t g_skips_dual_render_last_frame = 0u;
		std::uint32_t g_skips_no_geometry_last_frame = 0u;

		// debugging: what the api path renders (owned draws are not submitted / everything else is hidden)
		bool g_debug_hide_api = false;
		bool g_debug_hide_d3d9_static = false;
		bool g_debug_tyres_on_d3d9 = false;
		bool g_debug_log_hashes = false; // also sets rtx.debugLogGeometryHashes in the runtime
		bool g_send_d3d9_hashes = true;  // api meshes keyed for USD replacements by their D3D9 geometry hash
		std::uint32_t g_game_frame = 0u;

		// #
		// helpers

		// cheap hash for vertex data that is compared every draw (blendshape morphs)
		std::uint64_t hash_words(const void* data, const std::size_t size)
		{
			const auto* p = static_cast<const std::uint8_t*>(data);
			std::uint64_t h = 0x9E3779B97F4A7C15ull ^ size;
			std::size_t i = 0u;

			for (; i + 8u <= size; i += 8u)
			{
				std::uint64_t w;
				std::memcpy(&w, p + i, sizeof(w));
				h = (h ^ w) * 0xBF58476D1CE4E5B9ull;
				h ^= h >> 31;
			}

			for (; i < size; i++) {
				h = (h ^ p[i]) * 1099511628211ull;
			}

			return h;
		}

		std::uint64_t fnv1a64(const void* data, const std::size_t size, std::uint64_t h = 1469598103934665603ull)
		{
			const auto* p = static_cast<const std::uint8_t*>(data);
			for (std::size_t i = 0u; i < size; i++)
			{
				h ^= p[i];
				h *= 1099511628211ull;
			}
			return h;
		}

		float half_to_float(const std::uint16_t h)
		{
			const std::uint32_t sign = (h >> 15) & 1u;
			const std::uint32_t exp = (h >> 10) & 0x1Fu;
			const std::uint32_t mant = h & 0x3FFu;
			std::uint32_t bits;

			if (exp == 0u)
			{
				if (mant == 0u) {
					bits = sign << 31;
				}
				else
				{
					// subnormal
					std::uint32_t e = 127u - 15u + 1u;
					std::uint32_t m = mant;
					while ((m & 0x400u) == 0u) { m <<= 1; e--; }
					bits = (sign << 31) | (e << 23) | ((m & 0x3FFu) << 13);
				}
			}
			else if (exp == 31u) {
				bits = (sign << 31) | 0x7F800000u | (mant << 13);
			} else {
				bits = (sign << 31) | ((exp + 127u - 15u) << 23) | (mant << 13);
			}

			float f;
			std::memcpy(&f, &bits, sizeof(f));
			return f;
		}

		// decodes a D3DDECLTYPE element into up to 4 floats, returns false for unsupported types
		bool decode_element(const std::uint8_t* src, const std::uint8_t type, float* out, std::uint32_t& out_count)
		{
			const auto f32 = [src](const std::uint32_t i) { float v; std::memcpy(&v, src + 4u * i, sizeof(v)); return v; };
			const auto u16 = [src](const std::uint32_t i) { std::uint16_t v; std::memcpy(&v, src + 2u * i, sizeof(v)); return v; };
			const auto s16 = [src](const std::uint32_t i) { std::int16_t v; std::memcpy(&v, src + 2u * i, sizeof(v)); return v; };

			switch (type)
			{
			case D3DDECLTYPE_FLOAT1: out[0] = f32(0); out_count = 1u; return true;
			case D3DDECLTYPE_FLOAT2: out[0] = f32(0); out[1] = f32(1); out_count = 2u; return true;
			case D3DDECLTYPE_FLOAT3: out[0] = f32(0); out[1] = f32(1); out[2] = f32(2); out_count = 3u; return true;
			case D3DDECLTYPE_FLOAT4: out[0] = f32(0); out[1] = f32(1); out[2] = f32(2); out[3] = f32(3); out_count = 4u; return true;
			case D3DDECLTYPE_D3DCOLOR: // b, g, r, a bytes
				out[0] = src[2] / 255.0f; out[1] = src[1] / 255.0f; out[2] = src[0] / 255.0f; out[3] = src[3] / 255.0f; out_count = 4u; return true;
			case D3DDECLTYPE_UBYTE4:
				out[0] = src[0]; out[1] = src[1]; out[2] = src[2]; out[3] = src[3]; out_count = 4u; return true;
			case D3DDECLTYPE_UBYTE4N:
				out[0] = src[0] / 255.0f; out[1] = src[1] / 255.0f; out[2] = src[2] / 255.0f; out[3] = src[3] / 255.0f; out_count = 4u; return true;
			case D3DDECLTYPE_SHORT2: out[0] = s16(0); out[1] = s16(1); out_count = 2u; return true;
			case D3DDECLTYPE_SHORT4: out[0] = s16(0); out[1] = s16(1); out[2] = s16(2); out[3] = s16(3); out_count = 4u; return true;
			case D3DDECLTYPE_SHORT2N: out[0] = s16(0) / 32767.0f; out[1] = s16(1) / 32767.0f; out_count = 2u; return true;
			case D3DDECLTYPE_SHORT4N: out[0] = s16(0) / 32767.0f; out[1] = s16(1) / 32767.0f; out[2] = s16(2) / 32767.0f; out[3] = s16(3) / 32767.0f; out_count = 4u; return true;
			case D3DDECLTYPE_USHORT2N: out[0] = u16(0) / 65535.0f; out[1] = u16(1) / 65535.0f; out_count = 2u; return true;
			case D3DDECLTYPE_USHORT4N: out[0] = u16(0) / 65535.0f; out[1] = u16(1) / 65535.0f; out[2] = u16(2) / 65535.0f; out[3] = u16(3) / 65535.0f; out_count = 4u; return true;
			case D3DDECLTYPE_FLOAT16_2: out[0] = half_to_float(u16(0)); out[1] = half_to_float(u16(1)); out_count = 2u; return true;
			case D3DDECLTYPE_FLOAT16_4: out[0] = half_to_float(u16(0)); out[1] = half_to_float(u16(1)); out[2] = half_to_float(u16(2)); out[3] = half_to_float(u16(3)); out_count = 4u; return true;
			case D3DDECLTYPE_UDEC3:
			{
				std::uint32_t v; std::memcpy(&v, src, sizeof(v));
				out[0] = static_cast<float>(v & 0x3FFu); out[1] = static_cast<float>((v >> 10) & 0x3FFu); out[2] = static_cast<float>((v >> 20) & 0x3FFu); out_count = 3u; return true;
			}
			case D3DDECLTYPE_DEC3N:
			{
				std::uint32_t v; std::memcpy(&v, src, sizeof(v));
				const auto sn10 = [](const std::uint32_t bits) { const auto s = static_cast<std::int32_t>(bits << 22) >> 22; return std::max(s / 511.0f, -1.0f); };
				out[0] = sn10(v & 0x3FFu); out[1] = sn10((v >> 10) & 0x3FFu); out[2] = sn10((v >> 20) & 0x3FFu); out_count = 3u; return true;
			}
			default:
				return false;
			}
		}

		std::uint32_t pack_color(const float* rgba)
		{
			const auto c = [](const float v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return c(rgba[2]) | (c(rgba[1]) << 8) | (c(rgba[0]) << 16) | (c(rgba[3]) << 24);
		}

		remixapi_Transform to_transform(const float (*m)[4])
		{
			remixapi_Transform t = {};
			for (auto r = 0; r < 3; r++)
			{
				for (auto c = 0; c < 3; c++) {
					t.matrix[r][c] = m[c][r];
				}
				t.matrix[r][3] = m[3][r];
			}
			return t;
		}

		remixapi_Transform to_transform(const D3DXMATRIX& m)
		{
			return to_transform(reinterpret_cast<const float (*)[4]>(m.m));
		}

		remixapi_InstanceInfoBlendEXT make_blend(const draw_state_s& s)
		{
			return
			{
				.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BLEND_EXT,
				.pNext = nullptr,
				.alphaTestEnabled = s.alpha_test,
				.alphaTestReferenceValue = static_cast<std::uint8_t>(s.alpha_ref & 0xFFu),
				.alphaTestCompareOp = s.alpha_test ? to_vk_compare_op(s.alpha_func) : VK_COMPARE_ALWAYS,
				.alphaBlendEnabled = s.alpha_blend,
				.srcColorBlendFactor = to_vk_blend_factor(s.src_blend, VK_BLEND_FACTOR_ONE),
				.dstColorBlendFactor = to_vk_blend_factor(s.dst_blend, VK_BLEND_FACTOR_ZERO),
				.colorBlendOp = to_vk_blend_op(s.blend_op),
				.textureColorArg1Source = to_texture_arg(s.color_arg1),
				.textureColorArg2Source = to_texture_arg(s.color_arg2),
				.textureColorOperation = to_texture_op(s.color_op),
				.textureAlphaArg1Source = to_texture_arg(s.alpha_arg1),
				.textureAlphaArg2Source = to_texture_arg(s.alpha_arg2),
				.textureAlphaOperation = to_texture_op(s.alpha_op),
				.tFactor = s.tfactor,
				.isTextureFactorBlend = FALSE,
				.srcAlphaBlendFactor = to_vk_blend_factor(s.src_blend, VK_BLEND_FACTOR_ONE),
				.dstAlphaBlendFactor = to_vk_blend_factor(s.dst_blend, VK_BLEND_FACTOR_ZERO),
				.alphaBlendOp = to_vk_blend_op(s.blend_op),
				.writeMask = 0xFu,
				.isVertexColorBakedLighting = FALSE,
			};
		}

		draw_state_s capture_draw_state(IDirect3DDevice9* dev)
		{
			draw_state_s s = {};
			DWORD v = 0u;
			dev->GetRenderState(D3DRS_ALPHATESTENABLE, &v); s.alpha_test = v != 0u;
			dev->GetRenderState(D3DRS_ALPHAFUNC, &v); s.alpha_func = v;
			dev->GetRenderState(D3DRS_ALPHAREF, &v); s.alpha_ref = v;
			dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &v); s.alpha_blend = v != 0u;
			dev->GetRenderState(D3DRS_SRCBLEND, &v); s.src_blend = v;
			dev->GetRenderState(D3DRS_DESTBLEND, &v); s.dst_blend = v;
			dev->GetRenderState(D3DRS_BLENDOP, &v); s.blend_op = v;
			dev->GetRenderState(D3DRS_TEXTUREFACTOR, &v); s.tfactor = v;
			dev->GetRenderState(D3DRS_CULLMODE, &v); s.cull_mode = v; s.double_sided = v == D3DCULL_NONE;
			dev->GetTextureStageState(0, D3DTSS_COLOROP, &v); s.color_op = v;
			dev->GetTextureStageState(0, D3DTSS_COLORARG1, &v); s.color_arg1 = v;
			dev->GetTextureStageState(0, D3DTSS_COLORARG2, &v); s.color_arg2 = v;
			dev->GetTextureStageState(0, D3DTSS_ALPHAOP, &v); s.alpha_op = v;
			dev->GetTextureStageState(0, D3DTSS_ALPHAARG1, &v); s.alpha_arg1 = v;
			dev->GetTextureStageState(0, D3DTSS_ALPHAARG2, &v); s.alpha_arg2 = v;
			dev->GetTextureStageState(0, D3DTSS_TEXCOORDINDEX, &v); s.uv_set = v & 0xFFFFu;
			return s;
		}

		// shader pass texture objects (grcTexturePC, grcTextureReference, grcRenderTargetPC) differ in layout,
		// the virtual GetName (vtable slot 5) is the only safe way to the name
		const char* texture_name(const game::grcTextureReference* ref)
		{
			if (!ref) {
				return nullptr;
			}

			const auto vtbl = *reinterpret_cast<void* const* const*>(ref);
			if (!vtbl) {
				return nullptr;
			}

			const auto get_name = reinterpret_cast<const char*(__thiscall*)(const void*)>(vtbl[5]);
			const auto name = get_name(ref);
			return name && *name ? name : nullptr;
		}

		const game::grcTextureReference* colormap_ref(const drawcall_mod_context& ctx)
		{
			for (auto i = 0u; i < ctx.info.pass_texture_count; i++)
			{
				if (ctx.info.pass_textures[i].slot == 0u) {
					return ctx.info.pass_textures[i].ref;
				}
			}
			return nullptr;
		}

		// the per draw values the fixed function path wrote to the unused render states, taken from the draw context
		// (never read back from the device, so nothing from another draw can leak in)
		remixapi_InstanceInfoD3DOverridesEXT make_overrides(const drawcall_mod_context& ctx)
		{
			remixapi_InstanceInfoD3DOverridesEXT o = {};
			o.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_D3D_OVERRIDES_EXT;
			o.categoryFlags = static_cast<std::uint32_t>(ctx.modifiers.remix_instance_categories);
			o.modifierFlags = static_cast<std::uint32_t>(ctx.modifiers.remix_modifier);
			o.hashOverride = ctx.info.remix_hash_override;
			o.hashSeed = ctx.info.remix_hash_seed;
			o.emissiveScale = ctx.info.remix_temp_float01;
			o.packedWetness = ctx.info.remix_wetness_packed;

			for (auto i = 0u; i < 6u; i++) {
				o.multiUse[i] = ctx.info.remix_multi_use[i];
			}

			return o;
		}

		// the runtime drops the color stream of D3D9 draws tagged IgnoreBakedLighting (unless RemoveVertexColorKeepAlpha is set),
		// api meshes carry their colors, so the same draws get white vertex colors baked in
		bool wants_vertex_colors_dropped(const drawcall_mod_context& ctx)
		{
			const auto categories = static_cast<std::uint32_t>(ctx.modifiers.remix_instance_categories);
			const auto modifiers = static_cast<std::uint32_t>(ctx.modifiers.remix_modifier);
			
			return (categories & static_cast<std::uint32_t>(InstanceCategories::IgnoreBakedLighting))
				&& !(modifiers & static_cast<std::uint32_t>(RemixModifier::RemoveVertexColorKeepAlpha));
		}

		bool is_ignored_draw(const drawcall_mod_context& ctx)
		{
			return (static_cast<std::uint32_t>(ctx.modifiers.remix_instance_categories) & static_cast<std::uint32_t>(InstanceCategories::Ignore)) != 0u;
		}

		// the draw categories (D3D render state layout) as instance categories: the runtime only reads the D3D categories of
		// the overrides on the D3D9 path, an api instance is categorized by its own flags (other bit layout)
		std::uint32_t to_api_categories(const std::uint32_t d3d)
		{
			static constexpr std::uint32_t API_BITS[] =
			{
				REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_UI,
				REMIXAPI_INSTANCE_CATEGORY_BIT_WORLD_MATTE,
				REMIXAPI_INSTANCE_CATEGORY_BIT_SKY,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_LIGHTS,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ANTI_CULLING,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_MOTION_BLUR,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_OPACITY_MICROMAP,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_ALPHA_CHANNEL,
				REMIXAPI_INSTANCE_CATEGORY_BIT_HIDDEN,
				REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE,
				REMIXAPI_INSTANCE_CATEGORY_BIT_BEAM,
				REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_STATIC,
				REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_DYNAMIC,
				REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_SINGLE_OFFSET,
				REMIXAPI_INSTANCE_CATEGORY_BIT_DECAL_NO_OFFSET,
				REMIXAPI_INSTANCE_CATEGORY_BIT_ALPHA_BLEND_TO_CUTOUT,
				REMIXAPI_INSTANCE_CATEGORY_BIT_TERRAIN,
				REMIXAPI_INSTANCE_CATEGORY_BIT_ANIMATED_WATER,
				REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_MODEL,
				REMIXAPI_INSTANCE_CATEGORY_BIT_THIRD_PERSON_PLAYER_BODY,
				REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_BAKED_LIGHTING,
				REMIXAPI_INSTANCE_CATEGORY_BIT_PARTICLE_EMITTER,
				REMIXAPI_INSTANCE_CATEGORY_BIT_SMOOTH_NORMALS,
				REMIXAPI_INSTANCE_CATEGORY_BIT_HAIR_CARDS,
				0u, // DisableBackfaceCulling: doubleSided
			};
			static_assert(std::size(API_BITS) == static_cast<std::size_t>(InstanceCategories::Count));

			std::uint32_t api = 0u;
			for (auto i = 0u; i < std::size(API_BITS); i++)
			{
				if (d3d & (1u << i)) {
					api |= API_BITS[i];
				}
			}
			return api;
		}

		// blend ext copy with the instance's texture factor and the overrides chained
		const void* chain_overrides(remixapi_InstanceInfoBlendEXT& blend, remixapi_InstanceInfoD3DOverridesEXT& overrides, const DWORD tfactor)
		{
			blend.tFactor = tfactor;
			blend.pNext = &overrides;
			return &blend;
		}

		/// one api material per (colormap texture, draw state); the colormap is the material's albedo and replacement key
		/// @note caller holds the unique lock
		material_record* find_or_create_material(IDirect3DDevice9* dev, const drawcall_mod_context& ctx, const draw_state_s& state, std::uint64_t& out_key)
		{
			IDirect3DBaseTexture9* color_base = nullptr;
			dev->GetTexture(0, &color_base);

			if (color_base) {
				color_base->Release(); // the game keeps its own reference
			}

			std::uint64_t key = state.key();
			key ^= reinterpret_cast<std::uintptr_t>(color_base) * 0x9E3779B97F4A7C15ull;
			out_key = key;

			if (const auto it = g_materials.find(key); it != g_materials.end()) {
				return &it->second;
			}

			material_record rec = {};
			rec.state = state;
			rec.blend = make_blend(state);
			rec.shader_name = std::string(ctx.info.shader_name);
			rec.colormap = color_base;

			if (const auto n = texture_name(colormap_ref(ctx)); n) {
				rec.colormap_name = n;
			} else {
				rec.colormap_name = std::format("tex_{:08X}", reinterpret_cast<std::uintptr_t>(color_base));
			}

			IDirect3DTexture9* colormap = nullptr;
			if (color_base && color_base->GetType() == D3DRTYPE_TEXTURE) {
				colormap = static_cast<IDirect3DTexture9*>(color_base);
			}

			remixapi_MaterialInfoGameTexturesEXT game_textures =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_GAME_TEXTURES_EXT,
				.pNext = nullptr,
				.colorTexture = colormap,
			};

			remixapi_MaterialInfoOpaqueEXT opaque = {};
			opaque.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
			opaque.pNext = colormap ? &game_textures : nullptr;
			opaque.roughnessTexture = L"";
			opaque.metallicTexture = L"";
			opaque.heightTexture = L"";
			opaque.albedoConstant = { 1.0f, 1.0f, 1.0f };
			opaque.opacityConstant = 1.0f;
			opaque.roughnessConstant = 0.7f;
			opaque.metallicConstant = 0.1f;
			// alpha test / blending come from the per instance blend ext, exactly like the fixed function draw
			opaque.useDrawCallAlphaState = TRUE;
			opaque.alphaTestType = static_cast<int>(VK_COMPARE_ALWAYS);
			opaque.alphaReferenceValue = 0u;
			opaque.enableDlssControlMask = TRUE;
			opaque.dlssControlMaskIntensity = 1.0f;
			opaque.dlssControlMaskToneStrength = 1.0f;
			opaque.dlssControlMaskStructuralStrength = 1.0f;

			// empty albedo: the runtime uses the colormap as albedo and its image hash as the replacement key
			const remixapi_MaterialInfo info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,
				.pNext = &opaque,
				.hash = key ? key : 1u,
				.albedoTexture = L"",
				.normalTexture = L"",
				.tangentTexture = L"",
				.emissiveTexture = L"",
				.emissiveIntensity = 0.0f,
				.emissiveColorConstant = { 0.0f, 0.0f, 0.0f },
				.filterMode = 1, // linear
				.wrapModeU = 1, // repeat
				.wrapModeV = 1,
			};

			if (shared::common::remix_api::get().m_bridge.CreateMaterial(&info, &rec.handle) != REMIXAPI_ERROR_CODE_SUCCESS || !rec.handle) {
				return nullptr;
			}

			if (color_base) {
				g_materials_by_texture[color_base].push_back(key);
			}
			return &g_materials.emplace(key, std::move(rec)).first->second;
		}

		// byte size of a vertex element, the element size the runtime hashes (DecodeDecltype -> VkFormat)
		std::uint32_t decltype_size(const BYTE type)
		{
			switch (type)
			{
			case D3DDECLTYPE_FLOAT1: return 4u;
			case D3DDECLTYPE_FLOAT2: return 8u;
			case D3DDECLTYPE_FLOAT3: return 12u;
			case D3DDECLTYPE_FLOAT4: return 16u;
			case D3DDECLTYPE_SHORT4: case D3DDECLTYPE_SHORT4N: case D3DDECLTYPE_USHORT4N: case D3DDECLTYPE_FLOAT16_4: return 8u;
			case D3DDECLTYPE_D3DCOLOR: case D3DDECLTYPE_UBYTE4: case D3DDECLTYPE_UBYTE4N: case D3DDECLTYPE_SHORT2: case D3DDECLTYPE_SHORT2N:
			case D3DDECLTYPE_USHORT2N: case D3DDECLTYPE_UDEC3: case D3DDECLTYPE_DEC3N: case D3DDECLTYPE_FLOAT16_2: return 4u;
			default: return 0u;
			}
		}

		struct geometry_bytes_s
		{
			const std::uint8_t* vertices = nullptr;
			std::uint32_t vertex_count = 0u;
			std::uint32_t stride = 0u;
			const std::uint16_t* indices = nullptr;
			std::uint32_t index_count = 0u;
			const D3DVERTEXELEMENT9* elements = nullptr;

			// read-only locks of the D3D buffers (bridge shadow copies) when the engine freed its copies
			IDirect3DVertexBuffer9* locked_vb = nullptr;
			IDirect3DIndexBuffer9* locked_ib = nullptr;

			~geometry_bytes_s()
			{
				if (locked_vb) { locked_vb->Unlock(); locked_vb->Release(); }
				if (locked_ib) { locked_ib->Unlock(); locked_ib->Release(); }
			}
		};

		/// @param bound_buffers read the buffers bound for the draw instead of the engine copies: double buffered geometry
		///                      (blendshape morphs) draws the pair grmGeometryQB::Draw picked this frame
		bool read_geometry_bytes(IDirect3DDevice9* dev, const game::grmGeometryQB* geom, geometry_bytes_s& out, const bool bound_buffers = false)
		{
			const auto vb = reinterpret_cast<const grcVertexBufferD3D*>(geom->vertex_buffers[0]);
			const auto ib = reinterpret_cast<const grcIndexBufferD3D*>(geom->index_buffers[0]);
			const auto decl = reinterpret_cast<const grcVertexDeclaration*>(geom->vertex_declaration);

			if (!vb || !ib || !decl) {
				return false;
			}

			out.vertex_count = vb->vertex_count;
			out.stride = vb->stride;
			out.index_count = geom->index_count ? geom->index_count : ib->index_count;
			out.elements = decl->elements;

			if (!bound_buffers && vb->cpu_data && ib->cpu_data)
			{
				out.vertices = vb->cpu_data;
				out.indices = ib->cpu_data;
				return true;
			}

			// defrag re-placed buffers lose the engine copy; the bridge client keeps a shadow of every buffer
			IDirect3DVertexBuffer9* d3d_vb = nullptr;
			UINT offset = 0u, stride = 0u;

			if (FAILED(dev->GetStreamSource(0, &d3d_vb, &offset, &stride)) || !d3d_vb) {
				return false;
			}

			void* vdata = nullptr;
			if (FAILED(d3d_vb->Lock(0, 0, &vdata, D3DLOCK_READONLY)) || !vdata) {
				d3d_vb->Release();
				return false;
			}

			out.locked_vb = d3d_vb;
			out.vertices = static_cast<const std::uint8_t*>(vdata) + offset;

			IDirect3DIndexBuffer9* d3d_ib = nullptr;
			if (FAILED(dev->GetIndices(&d3d_ib)) || !d3d_ib) {
				return false;
			}

			void* idata = nullptr;
			if (FAILED(d3d_ib->Lock(0, 0, &idata, D3DLOCK_READONLY)) || !idata) {
				d3d_ib->Release();
				return false;
			}

			out.locked_ib = d3d_ib;
			out.indices = static_cast<const std::uint16_t*>(idata);
			return true;
		}

		bool has_blend_elements(const D3DVERTEXELEMENT9* elements)
		{
			bool weights = false, indices = false;
			for (const auto* e = elements; e->Stream != 0xFF; e++)
			{
				if (e->Stream == 0u && e->UsageIndex == 0u)
				{
					weights |= e->Usage == D3DDECLUSAGE_BLENDWEIGHT;
					indices |= e->Usage == D3DDECLUSAGE_BLENDINDICES;
				}
			}
			return weights && indices;
		}

		// blend indices select the bone register triplet (index * 3 + 64); normalized formats carry them as byte / 255
		std::uint32_t decode_bone_index(const float v, const std::uint8_t type)
		{
			const float scale = type == D3DDECLTYPE_D3DCOLOR || type == D3DDECLTYPE_UBYTE4N ? 255.0f : 1.0f;
			return static_cast<std::uint32_t>(std::max(0.0f, v * scale + 0.5f));
		}

		struct texture_hash_entry_s
		{
			UINT width = 0u;
			UINT height = 0u;
			D3DFORMAT format = D3DFMT_UNKNOWN;
			std::uint64_t hash = 0u;
		};
		std::unordered_map<const void*, texture_hash_entry_s> g_texture_hashes;

		// session stable identity of a texture: size, format and top mip (read-only lock of the bridge's copy), cached per texture.
		// Peds of different models share component meshes and texture names (skin tones, clothing), the name is not enough.
		std::uint64_t texture_content_hash(IDirect3DBaseTexture9* base)
		{
			if (!base || base->GetType() != D3DRTYPE_TEXTURE) {
				return 0u;
			}
			const auto tex = static_cast<IDirect3DTexture9*>(base);
			D3DSURFACE_DESC desc = {};
			if (FAILED(tex->GetLevelDesc(0, &desc))) {
				return 0u;
			}

			auto& s_cache = g_texture_hashes;
			if (const auto it = s_cache.find(tex); it != s_cache.end() && it->second.width == desc.Width && it->second.height == desc.Height && it->second.format == desc.Format) {
				return it->second.hash;
			}

			D3DLOCKED_RECT locked = {};
			if (FAILED(tex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)) || !locked.pBits) {
				return 0u;
			}
			const bool dxt1 = desc.Format == D3DFMT_DXT1;
			const bool block = dxt1 || desc.Format == D3DFMT_DXT2 || desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT4 || desc.Format == D3DFMT_DXT5;
			const UINT rows = block ? (desc.Height + 3u) / 4u : desc.Height;
			const auto row_bytes = block ? std::min<std::size_t>(locked.Pitch, ((desc.Width + 3u) / 4u) * (dxt1 ? 8u : 16u)) : static_cast<std::size_t>(locked.Pitch);

			std::uint64_t h = (static_cast<std::uint64_t>(desc.Width) << 32 | desc.Height) * 0x9E3779B97F4A7C15ull ^ desc.Format;
			for (UINT y = 0u; y < rows; y++) {
				h = (h ^ hash_words(static_cast<const std::uint8_t*>(locked.pBits) + static_cast<std::size_t>(y) * locked.Pitch, row_bytes)) * 0xBF58476D1CE4E5B9ull;
			}
			tex->UnlockRect(0);

			h = h ? h : 1u;
			s_cache[tex] = { desc.Width, desc.Height, desc.Format, h };
			return h;
		}

		// the 128x128 R32F damage texture of a vehicle, every texel packs three bytes (x low, z high) into a float
		struct damage_map_s
		{
			const std::uint8_t* bits = nullptr;
			std::uint32_t pitch = 0u;
			std::uint32_t width = 0u;
			std::uint32_t height = 0u;

			// point sampled, clamped like the vertex texture fetch
			void decoded(const float u, const float v, float out[3]) const
			{
				const auto x = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(u * static_cast<float>(width))), 0, static_cast<int>(width) - 1));
				const auto y = static_cast<std::uint32_t>(std::clamp(static_cast<int>(std::floor(v * static_cast<float>(height))), 0, static_cast<int>(height) - 1));
				float packed;
				std::memcpy(&packed, bits + static_cast<std::size_t>(y) * pitch + x * sizeof(float), sizeof(float));
				const float hi = std::trunc(packed / 65536.0f);
				const float rest = packed - hi * 65536.0f;
				const float mid = std::trunc(rest / 256.0f);
				out[0] = rest - mid * 256.0f;
				out[1] = mid;
				out[2] = hi;
			}
		};

		// direction on the damage sphere for a texture position (a, b) = 2 * (uv - 0.5), inverse of the mapping in apply_vehicle_damage
		void damage_direction(const float a, const float b, float out[3])
		{
			const float r2 = a * a + b * b;
			const float s = r2 > 0.0f ? std::sqrt(r2) : 0.0f;
			const float z = std::max(1.0f - 2.0f * s, -1.0f);
			const bool valid = z < 1.0f && z > -1.0f && s > 0.0f;
			const float w = valid ? std::sqrt(1.0f - z * z) / s : 0.0f;
			out[0] = a * w;
			out[1] = b * w;
			out[2] = z;
		}

		// the deformation every damaged gta_vehicle_* vertex shader applies in model space before skinning (switchOn)
		void apply_vehicle_damage(const damage_map_s& map, const float bound_radius, float pos[3], float nrm[3], const bool has_normal)
		{
			constexpr float K = 126.732674f;
			constexpr float k = 0.00789062493f;

			const float len = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
			const float inv = 1.0f / (len + 1e-5f);
			const float t = (1.0f - pos[2] * inv) * 0.5f;
			float dx = pos[0] * inv + 1e-5f;
			float dy = pos[1] * inv + 1e-5f;
			const float dl = 1.0f / std::sqrt(dx * dx + dy * dy);
			dx *= dl;
			dy *= dl;

			const float u = dx * t * 0.5f + 0.5f;
			const float v = dy * t * 0.5f + 0.5f;
			const float fx = u * K - std::trunc(u * K);
			const float fy = v * K - std::trunc(v * K);
			const float u0 = u - fx * k;
			const float v0 = v - fy * k;

			float d00[3], d10[3], d01[3], d11[3];
			map.decoded(u, v, d00);
			map.decoded(u0 + k, v0, d10);
			map.decoded(u0, v0 + k, d01);
			map.decoded(u0 + k, v0 + k, d11);

			const float scale = bound_radius > 0.0f ? std::min(len / bound_radius, 1.0f) : 1.0f;
			for (auto c = 0; c < 3; c++)
			{
				const float b = (d00[c] * (1.0f - fx) + d10[c] * fx) * (1.0f - fy) + (d01[c] * (1.0f - fx) + d11[c] * fx) * fy;
				pos[c] += (b - 128.0f) / 128.0f * scale * 0.5f;
			}

			if (!has_normal) {
				return;
			}
			if (nrm[0] == 1.0f && nrm[1] == 1.0f && nrm[2] == 1.0f)
			{
				nrm[0] = 0.0f;
				nrm[1] = 0.0f;
				return;
			}

			// the displacement gradient along the two texture axes bends the normal
			float p0[3], p1[3], p2[3];
			damage_direction((u0 - 0.5f) * 2.0f, (v0 - 0.5f) * 2.0f, p0);
			damage_direction((u0 + k - 0.5f) * 2.0f, (v0 - 0.5f) * 2.0f, p1);
			damage_direction((u0 - 0.5f) * 2.0f, (v0 + k - 0.5f) * 2.0f, p2);

			float acc[3] = { nrm[0], nrm[1], nrm[2] };
			const auto bend = [&](const float dir[3], const float sample[3], const float epsilon)
			{
				float e[3], d[3];
				for (auto c = 0; c < 3; c++)
				{
					e[c] = dir[c] + epsilon - p0[c];
					d[c] = ((sample[c] - 128.0f) * scale - (d00[c] - 128.0f) * scale) / 256.0f;
				}
				const float ee = e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
				if (ee > 0.0f)
				{
					const float f = -(d[0] * nrm[0] + d[1] * nrm[1] + d[2] * nrm[2]) / ee;
					for (auto c = 0; c < 3; c++) {
						acc[c] += e[c] * f;
					}
				}
			};
			bend(p1, d10, 1e-8f);
			bend(p2, d01, 0.0f);

			const float al = std::sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
			if (al > 0.0f)
			{
				for (auto c = 0; c < 3; c++) {
					nrm[c] = acc[c] / al;
				}
			}
		}

		// the tyreDeformSwitchOn block of gta_vehicle_tire: flattens the part of the tyre below the ground. c[0..3] = matWheelTransform
		// rows (world space), c[4] = tyreDeformParams, c[5] = tyreDeformParams2 (x: radius, y: ground height, z: falloff, w: amount)
		void apply_flat_tyre(const float (*c)[4], float p[3])
		{
			const float* params = c[4];
			const float* params2 = c[5];
			const float world_z = p[0] * c[0][2] + p[1] * c[1][2] + p[2] * c[2][2] + c[3][2];
			const float r2 = p[1] * p[1] + p[2] * p[2];

			const float outside = params2[0] * params2[0] < r2 ? 1.0f : 0.0f;
			float q[3] = { params2[3] * p[0], params2[3] * p[1], params2[3] * p[2] };
			const float limit = params2[0] * 1.1f;
			const float beyond = limit * limit < r2 * params2[3] ? 1.0f : 0.0f;
			const float ground = params2[1] + 0.03f;
			const float below = world_z < ground ? 1.0f : 0.0f;
			const float depth = std::max(params2[1] - world_z, 0.0f);
			const float scaled = (ground - world_z) * params2[2];
			const float t = scaled > 1.0f ? 1.0f : scaled;

			float o[3];
			o[0] = params[0] * p[0] * t + (q[0] - params[3] * t);
			o[1] = params[1] * depth + q[1];
			o[2] = params[2] * depth + q[2];
			for (auto i = 0; i < 3; i++)
			{
				o[i] = (o[i] - p[i] * params2[3]) * below;
				q[i] += beyond * o[i];
				p[i] = outside * q[i] + (1.0f - outside) * p[i];
			}
		}

		// read-only lock of a damage texture (the bridge client keeps a shadow of every locked texture, nothing goes to the server)
		struct damage_lock_s
		{
			IDirect3DTexture9* texture = nullptr;
			damage_map_s map = {};

			explicit damage_lock_s(IDirect3DTexture9* tex)
			{
				D3DSURFACE_DESC desc = {};
				D3DLOCKED_RECT locked = {};
				if (!tex || FAILED(tex->GetLevelDesc(0, &desc)) || desc.Format != D3DFMT_R32F
					|| FAILED(tex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)) || !locked.pBits) {
					return;
				}
				texture = tex;
				map = { static_cast<const std::uint8_t*>(locked.pBits), static_cast<std::uint32_t>(locked.Pitch), desc.Width, desc.Height };
			}

			~damage_lock_s()
			{
				if (texture) {
					texture->UnlockRect(0);
				}
			}

			bool valid() const { return texture != nullptr; }
		};

		struct asset_hash_parts_s
		{
			std::uint64_t positions = 0u;
			std::uint64_t indices = 0u;
			std::uint64_t descriptor = 0u;
			std::uint32_t index_count = 0u;
			std::uint32_t vertex_count = 0u;
			std::uint32_t min_index = 0u;
			BYTE position_type = 0u;
			WORD position_offset = 0u;
			const char* failure = nullptr;
		};

		// The geometry asset hash the runtime gives the D3D9 draw of this geometry, the key existing USD replacements use.
		// Mirrors rtx.geometryAssetHashRuleString = positions,indices,geometrydescriptor: D3D9Rtx::processIndexBuffer /
		// computeHash (d3d9_rtx.cpp, d3d9_rtx_geometry.cpp), hashGeometryDescriptor / hashVertexRegionIndexed (rtx_hashing.cpp)
		// and GeometryHashes::getHashForRuleImpl (rtx_hashing.h). 0 when the draw cannot be hashed like that (parts.failure says why).
		std::uint64_t d3d9_asset_hash(const geometry_bytes_s& bytes, const INT base_vertex, const UINT start_index, const UINT prim_count, asset_hash_parts_s& parts)
		{
			const std::uint32_t index_count = prim_count * 3u;
			parts.index_count = index_count;

			if (!bytes.vertices || !bytes.indices || !bytes.elements || !index_count)
			{
				parts.failure = "no geometry data";
				return 0u;
			}

			if (start_index + index_count > bytes.index_count)
			{
				parts.failure = "index range outside the index buffer";
				return 0u;
			}

			const D3DVERTEXELEMENT9* position = nullptr;
			for (auto* e = bytes.elements; e->Stream != 0xFF; e++)
			{
				if (e->Stream == 0u && e->Usage == D3DDECLUSAGE_POSITION && e->UsageIndex == 0u)
				{
					position = e;
					break;
				}
			}

			if (!position)
			{
				parts.failure = "no position element on stream 0";
				return 0u;
			}

			parts.position_type = position->Type;
			parts.position_offset = position->Offset;

			const auto position_size = decltype_size(position->Type);
			if (!position_size)
			{
				parts.failure = "unknown position type";
				return 0u;
			}

			const std::uint16_t* indices = bytes.indices + start_index;
			const auto [min_it, max_it] = std::minmax_element(indices, indices + index_count);
			const std::uint32_t min_index = *min_it;
			const std::uint32_t max_index = *max_it;
			parts.min_index = min_index;

			if (max_index == min_index)
			{
				parts.failure = "degenerate index range";
				return 0u;
			}

			if (static_cast<std::int64_t>(base_vertex) + max_index >= bytes.vertex_count || base_vertex + static_cast<std::int64_t>(min_index) < 0)
			{
				parts.failure = "vertex range outside the vertex buffer";
				return 0u;
			}

			// indices rebased to the lowest one, the vertex region starts there
			static std::vector<std::uint16_t> rebased;
			rebased.resize(index_count);
			for (std::uint32_t i = 0u; i < index_count; i++) {
				rebased[i] = static_cast<std::uint16_t>(indices[i] - min_index);
			}

			const std::uint32_t vertex_count = max_index - min_index + 1u;
			parts.vertex_count = vertex_count;
			const std::uint8_t* region = bytes.vertices + static_cast<std::size_t>(base_vertex + min_index) * bytes.stride + position->Offset;

			// positions of the referenced vertices in ascending index order
			static std::vector<std::uint8_t> used;
			used.assign(vertex_count, 0u);
			for (const auto i : rebased) {
				used[i] = 1u;
			}

			XXH64_hash_t position_hash = 0u;
			for (std::uint32_t i = 0u; i < vertex_count; i++)
			{
				if (used[i]) {
					position_hash = XXH3_64bits_withSeed(region + static_cast<std::size_t>(i) * bytes.stride, position_size, position_hash);
				}
			}

			const XXH64_hash_t index_hash = XXH3_64bits(rebased.data(), index_count * sizeof(std::uint16_t));

			const std::uint32_t topology = 3u;   // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
			const std::uint32_t index_type = 0u; // VK_INDEX_TYPE_UINT16
			XXH64_hash_t descriptor_hash = XXH3_64bits_withSeed(&index_count, sizeof(index_count), 0u);
			descriptor_hash = XXH3_64bits_withSeed(&vertex_count, sizeof(vertex_count), descriptor_hash);
			descriptor_hash = XXH3_64bits_withSeed(&topology, sizeof(topology), descriptor_hash);
			descriptor_hash = XXH3_64bits_withSeed(&index_type, sizeof(index_type), descriptor_hash);

			parts.positions = position_hash;
			parts.indices = index_hash;
			parts.descriptor = descriptor_hash;

			// combined in HashComponents order: VertexPosition, Indices, GeometryDescriptor
			XXH64_hash_t hash = position_hash;
			hash = XXH64(&index_hash, sizeof(index_hash), hash);
			return XXH64(&descriptor_hash, sizeof(descriptor_hash), hash);
		}

		// api meshes are clockwise front faced (D3DCULL_CCW); draws with D3DCULL_CW need their winding flipped.
		// skinned: 4 normalized weights + indices per vertex into g_blend_weights / g_blend_indices, out_bone_count = highest used index + 1
		// uv0_fallback (vertex shader draws): like D3D9Rtx, a texcoord set it would not take from the declaration is replaced by
		// the shader's output texcoord, which is UV0 for the game's shaders
		renderer_api::reject_reason build_vertices(const geometry_bytes_s& bytes, const UINT prim_count, const bool flip_winding, const bool drop_vertex_colors,
			const bool skinned, const std::uint32_t uv_set, const bool uv0_fallback, const damage_map_s* damage, const float bound_radius,
			const float (*flat_tyre)[4], std::uint32_t& out_bone_count, bool& out_uv_fallback)
		{
			const D3DVERTEXELEMENT9* pos = nullptr;
			const D3DVERTEXELEMENT9* normal = nullptr;
			const D3DVERTEXELEMENT9* color = nullptr;
			const D3DVERTEXELEMENT9* uv = nullptr;
			const D3DVERTEXELEMENT9* uv0 = nullptr;
			const D3DVERTEXELEMENT9* weights = nullptr;
			const D3DVERTEXELEMENT9* bone_indices = nullptr;

			for (const auto* e = bytes.elements; e->Stream != 0xFF; e++)
			{
				if (e->Stream != 0u) {
					continue;
				}

				if (e->Usage == D3DDECLUSAGE_POSITION && e->UsageIndex == 0u) pos = e;
				else if (e->Usage == D3DDECLUSAGE_NORMAL && e->UsageIndex == 0u) normal = e;
				else if (e->Usage == D3DDECLUSAGE_COLOR && e->UsageIndex == 0u) color = e;
				else if (e->Usage == D3DDECLUSAGE_TEXCOORD)
				{
					if (e->UsageIndex == uv_set) uv = e;
					if (e->UsageIndex == 0u) uv0 = e;
				}
				else if (e->Usage == D3DDECLUSAGE_BLENDWEIGHT && e->UsageIndex == 0u) weights = e;
				else if (e->Usage == D3DDECLUSAGE_BLENDINDICES && e->UsageIndex == 0u) bone_indices = e;
			}

			// the formats of RtxGeometryUtils::isTexcoordFormatValid
			const bool uv_usable = uv 
				&& (uv->Type == D3DDECLTYPE_FLOAT2 || uv->Type == D3DDECLTYPE_FLOAT3
				 || uv->Type == D3DDECLTYPE_FLOAT4 || uv->Type == D3DDECLTYPE_FLOAT16_2);

			out_uv_fallback = uv0_fallback && uv_set != 0u && !uv_usable;

			if (out_uv_fallback) {
				uv = uv0;
			}

			if (!pos || (pos->Type != D3DDECLTYPE_FLOAT3 && pos->Type != D3DDECLTYPE_FLOAT4)) {
				return renderer_api::reject_reason::vertex_format;
			}

			// rigid vehicle skinning has indices only (one bone, weight 1)
			if (skinned && !bone_indices) {
				return renderer_api::reject_reason::vertex_format;
			}

			const auto index_count = std::min<std::uint32_t>(bytes.index_count, prim_count * 3u);
			if (!bytes.vertex_count || !index_count || !bytes.stride) {
				return renderer_api::reject_reason::empty;
			}

			g_vertices.clear();
			g_vertices.resize(bytes.vertex_count);
			g_blend_weights.clear();
			g_blend_indices.clear();

			if (skinned)
			{
				g_blend_weights.resize(static_cast<std::size_t>(bytes.vertex_count) * 4u);
				g_blend_indices.resize(static_cast<std::size_t>(bytes.vertex_count) * 4u);
			}

			std::uint32_t max_bone = 0u;
			float v[4]; std::uint32_t n;

			for (auto i = 0u; i < bytes.vertex_count; i++)
			{
				const auto* src = bytes.vertices + static_cast<std::size_t>(i) * bytes.stride;
				auto& dst = g_vertices[i];
				dst = {};

				decode_element(src + pos->Offset, pos->Type, v, n);
				dst.position[0] = v[0]; dst.position[1] = v[1]; dst.position[2] = v[2];

				if (normal && decode_element(src + normal->Offset, normal->Type, v, n) && n >= 3u)
				{
					if (normal->Type == D3DDECLTYPE_UBYTE4N || normal->Type == D3DDECLTYPE_D3DCOLOR) {
						v[0] = v[0] * 2.0f - 1.0f; v[1] = v[1] * 2.0f - 1.0f; v[2] = v[2] * 2.0f - 1.0f;
					}
					dst.normal[0] = v[0]; dst.normal[1] = v[1]; dst.normal[2] = v[2];
				} else {
					dst.normal[2] = 1.0f;
				}

				if (damage) {
					apply_vehicle_damage(*damage, bound_radius, dst.position, dst.normal, normal != nullptr);
				}

				if (flat_tyre) {
					apply_flat_tyre(flat_tyre, dst.position);
				}

				if (uv && decode_element(src + uv->Offset, uv->Type, v, n) && n >= 2u) {
					dst.texcoord[0] = v[0]; dst.texcoord[1] = v[1];
				}

				if (!drop_vertex_colors && color && decode_element(src + color->Offset, color->Type, v, n) && n >= 4u) {
					dst.color = pack_color(v);
				} else {
					dst.color = 0xFFFFFFFFu;
				}

				if (skinned)
				{
					float w[4] = {};
					if (weights && decode_element(src + weights->Offset, weights->Type, v, n)) {
						std::copy_n(v, std::min(n, 4u), w);
					}

					std::uint32_t idx[4] = {};
					if (decode_element(src + bone_indices->Offset, bone_indices->Type, v, n))
					{
						for (auto k = 0u; k < std::min(n, 4u); k++) {
							idx[k] = decode_bone_index(v[k], bone_indices->Type);
						}
					}

					const float sum = w[0] + w[1] + w[2] + w[3];
					if (sum <= 0.0f) {
						w[0] = 1.0f; w[1] = w[2] = w[3] = 0.0f;
					}
					else
					{
						for (auto& wk : w) {
							wk /= sum;
						}
					}
					// the runtime derives the last weight from the others
					w[3] = std::max(0.0f, 1.0f - w[0] - w[1] - w[2]);

					for (auto k = 0u; k < 4u; k++)
					{
						if (w[k] > 0.0f) {
							max_bone = std::max(max_bone, idx[k]);
						} else {
							idx[k] = idx[0]; // unused slots must still address a valid bone
						}

						g_blend_weights[i * 4u + k] = w[k];
						g_blend_indices[i * 4u + k] = idx[k];
					}
				}
			}

			if (skinned)
			{
				if (max_bone >= renderer_api::MAX_BONES) {
					return renderer_api::reject_reason::bone_range;
				}
				out_bone_count = max_bone + 1u;
			}

			g_indices.clear();
			g_indices.resize(index_count);

			for (auto i = 0u; i < index_count; i++)
			{
				const std::uint32_t idx = bytes.indices[i];
				if (idx >= bytes.vertex_count) {
					return renderer_api::reject_reason::index_range;
				}

				const auto dst_i = flip_winding && (i % 3u) ? (i - (i % 3u)) + (3u - (i % 3u)) : i;
				g_indices[dst_i] = idx;
			}

			return renderer_api::reject_reason::none;
		}

		void destroy_mesh_locked(const std::uint64_t hash)
		{
			const auto it = g_meshes.find(hash);
			if (it == g_meshes.end()) {
				return;
			}

			if (it->second.mesh) {
				shared::common::remix_api::get().m_bridge.DestroyMesh(it->second.mesh);
			}

			g_cache_bytes -= it->second.bytes;
			g_meshes.erase(it);

			for (auto& [geom, records] : g_records)
			{
				for (auto& rec : records)
				{
					if (rec.owned && rec.mesh_hash == hash) {
						rec = { .state_key = rec.state_key };
					}
				}
			}
		}

		void enforce_cache_budget_locked()
		{
			const auto budget = static_cast<std::uint64_t>(std::max(0, comp_settings::get()->api_static_cache_mb.get_as<int>())) << 20;
			while (g_cache_bytes > budget && !g_meshes.empty())
			{
				auto oldest = g_meshes.begin();
				for (auto it = g_meshes.begin(); it != g_meshes.end(); ++it)
				{
					if (it->second.last_used_frame < oldest->second.last_used_frame) {
						oldest = it;
					}
				}

				if (oldest->second.last_used_frame == g_frame) {
					break; // everything left was used this frame
				}

				destroy_mesh_locked(oldest->first);
			}
		}

		bool in_api_draw_window()
		{
			return g_in_draw_entity_dc || g_in_draw_frag_type_dc || g_in_draw_frag_dc_object || g_in_draw_entity_info || g_in_draw_dynamic_entity_dc || g_in_draw_lights_in_area
				|| g_in_draw_ped_dc; // rigid ped parts (props, unskinned lods) are drawn like static geometry
		}

		// the runtime reads the camera only from ray traced D3D9 draws or SetupCamera; a frame drawn entirely through
		// the api needs it from here, once per frame before the first instance
		void submit_camera_once()
		{
			if (g_camera_frame == g_frame || g_is_rendering_mirror || g_is_rendering_phone) {
				return;
			}

			const auto viewport = game::pCurrentViewport;
			const auto& bridge = shared::common::remix_api::get().m_bridge;
			if (!viewport || !viewport->wp || !bridge.SetupCamera) {
				return;
			}

			g_camera_frame = g_frame;

			remixapi_CameraInfo info = {};
			info.sType = REMIXAPI_STRUCT_TYPE_CAMERA_INFO;
			info.type = REMIXAPI_CAMERA_TYPE_WORLD;
			std::memcpy(info.view, &viewport->wp->view, sizeof(info.view));
			std::memcpy(info.projection, &viewport->wp->proj, sizeof(info.projection));
			bridge.SetupCamera(&info);
		}

		/// submits one captured pass of a geometry; skinned meshes need the bone palette of the draw
		/// @note caller holds the lock
		void submit_record_locked(const geometry_record& rec, const remixapi_Transform& transform, const remixapi_Transform* bones = nullptr, const std::uint64_t mesh_hash = 0u)
		{
			const auto mesh_it = g_meshes.find(mesh_hash ? mesh_hash : rec.mesh_hash);
			if (mesh_it == g_meshes.end()) {
				return;
			}

			const auto mat_it = g_materials.find(mesh_it->second.material_key);
			if (mat_it == g_materials.end()) {
				return;
			}

			// dev tab: the engine draw of an owned geometry is skipped, so the ignore shader logic is applied here
			if (debug_ignore_shader_logic(mat_it->second.shader_name)) {
				return;
			}

			if (mesh_it->second.bone_count && !bones) {
				return;
			}

			submit_camera_once();
			mesh_it->second.last_used_frame = g_frame;
			remixapi_InstanceInfoD3DOverridesEXT overrides = rec.overrides;
			remixapi_InstanceInfoBlendEXT blend = mat_it->second.blend;
			// D3D9Rtx turns Beam (vehicle paint / rims with 'vehicle_force_vertex_colors') into texture x vertex color x texture factor
			if (overrides.categoryFlags & static_cast<std::uint32_t>(InstanceCategories::Beam))
			{
				blend.textureColorOperation = TEXTURE_OP_MODULATE;
				blend.textureColorArg1Source = TEXTURE_ARG_TEXTURE;
				blend.textureColorArg2Source = TEXTURE_ARG_VERTEX_COLOR0;
				blend.isTextureFactorBlend = TRUE;
			}
			const remixapi_InstanceInfoBoneTransformsEXT bone_ext =
			{
				.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_BONE_TRANSFORMS_EXT,
				.pNext = nullptr,
				.boneTransforms_values = bones,
				.boneTransforms_count = mesh_it->second.bone_count,
			};

			if (mesh_it->second.bone_count) {
				overrides.pNext = const_cast<remixapi_InstanceInfoBoneTransformsEXT*>(&bone_ext);
			}

			const remixapi_InstanceInfo inst =
			{
				.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO,
				.pNext = const_cast<void*>(chain_overrides(blend, overrides, rec.tfactor)),
				.categoryFlags = to_api_categories(overrides.categoryFlags),
				.mesh = mesh_it->second.mesh,
				.transform = transform,
				.doubleSided = mat_it->second.state.double_sided,
			};

			shared::common::remix_api::get().m_bridge.DrawInstance(&inst);
			g_instances_this_frame++;

			if (g_count_batch_groups && !bones)
			{
				auto key_overrides = rec.overrides;
				key_overrides.pNext = nullptr;
				std::uint64_t key = hash_words(&key_overrides, sizeof(key_overrides));
				key = (key ^ reinterpret_cast<std::uintptr_t>(mesh_it->second.mesh)) * 0x9E3779B97F4A7C15ull;
				key = (key ^ rec.tfactor ^ (mat_it->second.state.double_sided ? 0x5555u : 0u)) * 0xBF58476D1CE4E5B9ull;
				g_batch_groups.insert(key);
			}
		}

		// the runtime only ray traces draws into a backbuffer sized target that write color, everything else is rasterized
		bool draws_to_primary_target(IDirect3DDevice9* dev)
		{
			DWORD write_mask = 0u;
			dev->GetRenderState(D3DRS_COLORWRITEENABLE, &write_mask);
			if ((write_mask & 7u) != 7u) {
				return false;
			}

			bool primary = false;
			IDirect3DSurface9* target = nullptr;
			IDirect3DSurface9* back_buffer = nullptr;
			if (SUCCEEDED(dev->GetRenderTarget(0, &target)) && target && SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer)) && back_buffer)
			{
				D3DSURFACE_DESC target_desc = {}, back_buffer_desc = {};
				primary = SUCCEEDED(target->GetDesc(&target_desc)) && SUCCEEDED(back_buffer->GetDesc(&back_buffer_desc))
					&& target_desc.Width == back_buffer_desc.Width && target_desc.Height == back_buffer_desc.Height;
			}

			if (target) {
				target->Release();
			}

			if (back_buffer) {
				back_buffer->Release();
			}

			return primary;
		}

		// the skinned variant of a technique is the vertex shader that declares a BLENDINDICES input; a geometry
		// can carry blend data and still be drawn rigid. Render thread only, cached per shader.
		bool vertex_shader_is_skinned(IDirect3DDevice9* dev)
		{
			IDirect3DVertexShader9* vs = nullptr;
			if (FAILED(dev->GetVertexShader(&vs)) || !vs) {
				return false;
			}

			struct entry_s
			{
				UINT size = 0u;
				bool skinned = false;
			};
			static std::unordered_map<const void*, entry_s> s_cache;

			UINT size = 0u;
			vs->GetFunction(nullptr, &size);

			auto it = s_cache.find(vs);
			if (it == s_cache.end() || it->second.size != size)
			{
				entry_s entry = { .size = size };
				std::vector<DWORD> code(size / sizeof(DWORD));
				if (!code.empty() && SUCCEEDED(vs->GetFunction(code.data(), &size)))
				{
					for (std::size_t i = 1u; i < code.size();)
					{
						const auto token = code[i];
						const auto opcode = token & 0xFFFFu;
						if (opcode == 0xFFFFu) { // end
							break;
						}

						if (opcode == 0xFFFEu) // comment
						{
							i += 1u + ((token >> 16) & 0x7FFFu);
							continue;
						}

						if (opcode == 0x1Fu && i + 2u < code.size()) // dcl usage, register
						{
							const auto usage = code[i + 1u] & 0x1Fu;
							const auto reg = code[i + 2u];
							const auto reg_type = ((reg >> 28) & 0x7u) | ((reg >> 8) & 0x18u);
							if (usage == D3DDECLUSAGE_BLENDINDICES && reg_type == D3DSPR_INPUT)
							{
								entry.skinned = true;
								break;
							}
						}

						i += 1u + ((token >> 24) & 0x0Fu);
					}
				}

				it = s_cache.insert_or_assign(vs, entry).first;
			}

			const bool skinned = it->second.skinned;
			vs->Release();
			return skinned;
		}

		// hash of a damage texture's content, once per frame and texture
		std::uint64_t damage_texture_hash(IDirect3DTexture9* tex)
		{
			struct entry_s
			{
				std::uint32_t frame = 0u;
				std::uint64_t hash = 0u;
			};
			static std::unordered_map<const void*, entry_s> s_cache;

			auto& entry = s_cache[tex];
			if (entry.hash && entry.frame == g_frame) {
				return entry.hash;
			}

			const damage_lock_s lock(tex);
			if (!lock.valid()) {
				return 0u;
			}

			std::uint64_t h = 0x9E3779B97F4A7C15ull;
			for (auto y = 0u; y < lock.map.height; y++) {
				h ^= hash_words(lock.map.bits + static_cast<std::size_t>(y) * lock.map.pitch, lock.map.width * sizeof(float)) + (h << 6) + (h >> 2);
			}

			entry = { g_frame, h ? h : 1u };

			if ((g_frame & 0x3FFu) == 0u) {
				std::erase_if(s_cache, [](const auto& e) { return g_frame - e.second.frame > 600u; });
			}

			return entry.hash;
		}

		// rigid ped draws use the full world matrix (c0 - c3); skinned ones get world oriented bones (c64, three registers each)
		// and the vertex shader only adds the world translation (c3)
		bool read_ped_pose(IDirect3DDevice9* dev, const game::grmGeometryQB* geom, renderer_api::draw_pose_s& pose)
		{
			float world[4][4];
			if (FAILED(dev->GetVertexShaderConstantF(0, &world[0][0], 4))) {
				return false;
			}

			pose.skinned = vertex_shader_is_skinned(dev);
			if (!pose.skinned)
			{
				pose.transform = to_transform(world);
				return true;
			}

			const auto decl = static_cast<const grcVertexDeclaration*>(geom->vertex_declaration);
			if (!decl || !has_blend_elements(decl->elements)) {
				return false;
			}

			pose.transform = {};
			for (auto r = 0; r < 3; r++)
			{
				pose.transform.matrix[r][r] = 1.0f;
				pose.transform.matrix[r][3] = world[3][r];
			}

			float registers[renderer_api::MAX_BONES * 3u][4];
			if (FAILED(dev->GetVertexShaderConstantF(64, &registers[0][0], renderer_api::MAX_BONES * 3u))) {
				return false;
			}

			for (auto b = 0u; b < renderer_api::MAX_BONES; b++) {
				std::memcpy(pose.bones[b].matrix, registers[b * 3u], sizeof(pose.bones[b].matrix));
			}

			return true;
		}

		// vehicles skin rigidly (one bone index, the palette at c64 like peds) or draw with the full world matrix; with
		// switchOn the vertex shader first deforms the model space vertex with the damage texture
		bool read_vehicle_pose(IDirect3DDevice9* dev, const drawcall_mod_context& ctx, const game::grmGeometryQB* geom, renderer_api::draw_pose_s& pose)
		{
			pose = {};
			pose.vehicle = true;

			float world[4][4];
			if (FAILED(dev->GetVertexShaderConstantF(0, &world[0][0], 4))) {
				return false;
			}

			pose.skinned = vertex_shader_is_skinned(dev);
			if (pose.skinned)
			{
				const auto decl = static_cast<const grcVertexDeclaration*>(geom->vertex_declaration);
				bool has_indices = false;
				for (const auto* e = decl ? decl->elements : nullptr; e && e->Stream != 0xFF; e++) {
					has_indices |= e->Stream == 0u && e->Usage == D3DDECLUSAGE_BLENDINDICES && e->UsageIndex == 0u;
				}
				if (!has_indices) {
					return false;
				}

				for (auto r = 0; r < 3; r++)
				{
					pose.transform.matrix[r][r] = 1.0f;
					pose.transform.matrix[r][3] = world[3][r];
				}

				float registers[renderer_api::MAX_BONES * 3u][4];
				if (FAILED(dev->GetVertexShaderConstantF(64, &registers[0][0], renderer_api::MAX_BONES * 3u))) {
					return false;
				}

				for (auto b = 0u; b < renderer_api::MAX_BONES; b++) {
					std::memcpy(pose.bones[b].matrix, registers[b * 3u], sizeof(pose.bones[b].matrix));
				}
			}
			else {
				pose.transform = to_transform(world);
			}

			const auto& info = ctx.info;
			if (info.vs_switch_on_register < 0 || info.vs_bound_radius_register < 0 || info.vs_damage_sampler < 0) {
				return true;
			}

			BOOL switch_on = FALSE;
			if (FAILED(dev->GetVertexShaderConstantB(static_cast<UINT>(info.vs_switch_on_register), &switch_on, 1)) || !switch_on) {
				return true;
			}

			IDirect3DBaseTexture9* base = nullptr;
			if (FAILED(dev->GetTexture(D3DVERTEXTEXTURESAMPLER0 + static_cast<DWORD>(info.vs_damage_sampler), &base)) || !base) {
				return false;
			}

			const bool is_texture = base->GetType() == D3DRTYPE_TEXTURE;
			base->Release(); // the vehicle keeps its own reference

			if (!is_texture) {
				return false;
			}

			float radius[4] = {};
			dev->GetVertexShaderConstantF(static_cast<UINT>(info.vs_bound_radius_register), radius, 1);
			pose.damage_texture = static_cast<IDirect3DTexture9*>(base);
			pose.bound_radius = radius[0];
			pose.damage_hash = damage_texture_hash(pose.damage_texture);
			return pose.damage_hash != 0u;
		}

		// tyres (gta_vehicle_tire, mostly drawn by RenderInstanced): the wheel matrix (matWheelTransform, rows) comes before
		// the world matrix (c0 - c3)
		bool read_tyre_pose(IDirect3DDevice9* dev, const drawcall_mod_context& ctx, renderer_api::draw_pose_s& pose)
		{
			pose = {};
			pose.vehicle = true;

			const auto reg = ctx.info.vs_wheel_transform_register >= 0 ? ctx.info.vs_wheel_transform_register : 64;
			float world[4][4], wheel[4][4];

			if (FAILED(dev->GetVertexShaderConstantF(0, &world[0][0], 4)) || FAILED(dev->GetVertexShaderConstantF(static_cast<UINT>(reg), &wheel[0][0], 4))) {
				return false;
			}

			for (auto r = 0; r < 3; r++) {
				wheel[r][3] = 0.0f;
			}

			wheel[3][3] = 1.0f;

			float m[4][4];
			for (auto r = 0; r < 4; r++)
			{
				for (auto c = 0; c < 4; c++) {
					m[r][c] = wheel[r][0] * world[0][c] + wheel[r][1] * world[1][c] + wheel[r][2] * world[2][c] + wheel[r][3] * world[3][c];
				}
			}

			pose.transform = to_transform(m);
			return true;
		}

		// a damaged vehicle draws its own variant of the shared undamaged mesh
		std::uint64_t mesh_variant_hash(const std::uint64_t base, const renderer_api::draw_pose_s* pose)
		{
			if (!pose || (!pose->damage_hash && !pose->flat_hash)) {
				return base;
			}

			std::uint32_t radius_bits;
			std::memcpy(&radius_bits, &pose->bound_radius, sizeof(radius_bits));

			const auto h = base ^ (pose->damage_hash * 0xD6E8FEB86659FD93ull) 
								^ (static_cast<std::uint64_t>(radius_bits) * 0xA0761D6478BD642Full)
								^ (pose->flat_hash * 0x8EBC6AF09C88C6E3ull);

			return h ? h : 1u;
		}

		renderer_api::draw_pose_s g_vehicle_pose;

		// #
		// phase B: entity level

		DETOUR_TYPEDEF(CEntity_AddToDrawListBuckets, int, __thiscall, CEntityPC* ent, std::uint32_t bucket, std::uint32_t render_mode, std::uint8_t alpha, int lod);
		DETOUR_TYPEDEF(grmGeometryQB_Draw, void, __thiscall, const game::grmGeometryQB* geom);
		DETOUR_TYPEDEF(grmGeometryQB_dtor, void, __thiscall, const game::grmGeometryQB* geom);
		DETOUR_TYPEDEF(CRenderPhase_BuildDrawListWrapper, void, __thiscall, void* phase);
		DETOUR_TYPEDEF(CEntityDrawInfo_RenderBuckets, int, __thiscall, void* info, int bucket, int render_mode);
		DETOUR_TYPEDEF(CDrawSkinnedEntityDC_Execute, int, __thiscall, void* dc);
		DETOUR_TYPEDEF(CRenderer_RenderDrawableGeometriesWithLightsInArea, int, __cdecl, const void* drawable, const float* matrix, int bucket, int lights_a, int lights_b, int lod);
		DETOUR_TYPEDEF(grmShader_DrawGeometry, void, __thiscall, void* shader, const game::grmModel* model, int geometry_index, int lod, std::uint32_t is_last);
		DETOUR_TYPEDEF(grmShader_DrawSkinned, void, __thiscall, void* shader, const game::grmModel* model, int geometry_index, const void* matrix_set, int lod, std::uint32_t is_last);
		DETOUR_TYPEDEF(CDrawPedDC_Execute, int, __thiscall, void* dc);
		DETOUR_TYPEDEF(CDrawPlayerDC_Execute, int, __thiscall, void* dc);
		DETOUR_TYPEDEF(CDrawPedPropDC_Execute, int, __thiscall, void* dc);
		DETOUR_TYPEDEF(CDrawPedPropsDC_Execute, int, __thiscall, void* dc);
		DETOUR_TYPEDEF(CDrawSLODPedDC_Execute, int, __thiscall, void* dc);

		typedef void* (__cdecl* CBaseDC_operator_new_t)(std::uint32_t size, int flags);
		typedef void (__cdecl* CBaseDC_Init_t)(void* dc);
		typedef void (__thiscall* CDrawListMgr_AddModelInfoReference_t)(void* mgr, int model_index);
		typedef void (__thiscall* CGtaDrawable_DrawListHook_t)(void* handle, int render_mode);

		// our draw list command: submits the api instances of a drawable at the position the engine command would have drawn it
		struct api_entity_dc
		{
			void** vtbl;
			std::uint32_t header;	// command index in bits 0-13, size / 16 in bits 14-24 (written by CBaseDC::Init)
			float matrix[3][4];		// remix transform
			const rmcDrawable* drawable;
			std::uint32_t pad;
		};
		static_assert(sizeof(api_entity_dc) == ENTITY_DC_SIZE);

		void submit_drawable_instances(const rmcDrawable* drawable, const remixapi_Transform& transform)
		{
			const auto lod = drawable->lods[0];
			if (!lod) {
				return;
			}

			std::shared_lock lock(g_mutex);
			for (auto m = 0u; m < lod->model_count; m++)
			{
				const auto model = reinterpret_cast<const game::grmModel*>(lod->models[m]);
				if (!model) {
					continue;
				}

				const auto geometries = reinterpret_cast<game::grmGeometryQB* const*>(model->m_geometries.pData);
				for (auto g = 0; g < model->m_geometries.wCount; g++)
				{
					const auto rec_it = g_records.find(geometries[g]);
					if (rec_it == g_records.end()) {
						continue;
					}

					// every pass the geometry was captured in (opaque, emissive, ...)
					for (const auto& rec : rec_it->second)
					{
						if (rec.owned && !rec.hidden && !g_debug_hide_api) {
							submit_record_locked(rec, transform);
						}
					}
				}
			}
		}

		void __fastcall api_entity_dc_dtor([[maybe_unused]] api_entity_dc* self, [[maybe_unused]] void* edx) {}

		void __fastcall api_entity_dc_execute(api_entity_dc* self, [[maybe_unused]] void* edx)
		{
			remixapi_Transform transform = {};
			std::memcpy(transform.matrix, self->matrix, sizeof(transform.matrix));
			submit_drawable_instances(self->drawable, transform);
		}

		int __fastcall api_entity_dc_get_size([[maybe_unused]] api_entity_dc* self, [[maybe_unused]] void* edx) { return static_cast<int>(ENTITY_DC_SIZE); }
		int __fastcall api_entity_dc_unk3([[maybe_unused]] api_entity_dc* self, [[maybe_unused]] void* edx) { return 0; }

		void* g_api_entity_dc_vtable[4] =
		{
			reinterpret_cast<void*>(&api_entity_dc_dtor),
			reinterpret_cast<void*>(&api_entity_dc_execute),
			reinterpret_cast<void*>(&api_entity_dc_get_size),
			reinterpret_cast<void*>(&api_entity_dc_unk3),
		};

		// CEntityDrawInfo::RenderBuckets draws the sorted alpha entities (alpha, glass, wire shaders). Skinned entries
		// (matrix set at +0x27) use bone matrices the api path cannot carry, they stay on D3D9.
		int __fastcall CEntityDrawInfo_RenderBuckets_hk(void* info, [[maybe_unused]] void* edx, const int bucket, const int render_mode)
		{
			const bool skinned = *(static_cast<const std::uint8_t*>(info) + 0x27) != 0u;
			g_in_draw_entity_info = skinned ? 0 : 1;
			const auto result = CEntityDrawInfo_RenderBuckets_og(info, bucket, render_mode);
			g_in_draw_entity_info = 0;
			reset_world_transform();
			return result;
		}

		// CDrawSkinnedEntityDC draws dynamic entities (CObject: boxes, barriers, ...); only entries with the skinning
		// flags at +0x1C use bone matrices, the rest draw with one world matrix like static entities
		int __fastcall CDrawSkinnedEntityDC_Execute_hk(void* dc, [[maybe_unused]] void* edx)
		{
			const bool skinned = *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(dc) + 0x1C) != 0u;
			g_in_draw_dynamic_entity_dc = skinned ? 0 : 1;
			const auto result = CDrawSkinnedEntityDC_Execute_og(dc);
			g_in_draw_dynamic_entity_dc = 0;
			reset_world_transform();
			return result;
		}

		// CRenderer::RenderDrawableGeometriesWithLightsInArea draws large alpha entities and wires geometry by geometry
		// with the area lights of each geometry (T_CB_Generic_6Args draw command callback); the world matrix is set per model
		int __cdecl CRenderer_RenderDrawableGeometriesWithLightsInArea_hk(const void* drawable, const float* matrix, const int bucket, const int lights_a, const int lights_b, const int lod)
		{
			g_in_draw_lights_in_area = 1;
			const auto result = CRenderer_RenderDrawableGeometriesWithLightsInArea_og(drawable, matrix, bucket, lights_a, lights_b, lod);
			g_in_draw_lights_in_area = 0;
			reset_world_transform();
			return result;
		}

		// ped draw commands: everything they draw is ped geometry, whatever shader it uses (the super lod ped uses gta_default)
		template <typename Fn>
		int run_in_ped_window(void* dc, Fn original)
		{
			g_in_draw_ped_dc++;
			const auto result = original(dc);
			g_in_draw_ped_dc--;
			return result;
		}

		int __fastcall CDrawPedDC_Execute_hk(void* dc, [[maybe_unused]] void* edx) { return run_in_ped_window(dc, CDrawPedDC_Execute_og); }
		int __fastcall CDrawPlayerDC_Execute_hk(void* dc, [[maybe_unused]] void* edx) { return run_in_ped_window(dc, CDrawPlayerDC_Execute_og); }
		int __fastcall CDrawPedPropDC_Execute_hk(void* dc, [[maybe_unused]] void* edx) { return run_in_ped_window(dc, CDrawPedPropDC_Execute_og); }
		int __fastcall CDrawPedPropsDC_Execute_hk(void* dc, [[maybe_unused]] void* edx) { return run_in_ped_window(dc, CDrawPedPropsDC_Execute_og); }
		int __fastcall CDrawSLODPedDC_Execute_hk(void* dc, [[maybe_unused]] void* edx) { return run_in_ped_window(dc, CDrawSLODPedDC_Execute_og); }

		void note_kept_ped_draw(const char* reason, const drawcall_mod_context& ctx)
		{
			if (g_kept_ped_draws.size() >= 32u) {
				return;
			}

			const auto name = texture_name(colormap_ref(ctx));
			kept_ped_draw_s entry = { reason, std::string(ctx.info.preset_name), name ? name : "?" };

			const auto known = std::any_of(g_kept_ped_draws.begin(), g_kept_ped_draws.end(), [&entry](const kept_ped_draw_s& k) {
					return k.reason == entry.reason && k.preset == entry.preset && k.colormap == entry.colormap;
				});

			if (!known) {
				g_kept_ped_draws.push_back(std::move(entry));
			}
		}

		// the technique grmShader::DrawGeometry runs: the forced one, else the default or (grmRenderState1 set) the alternative
		std::int32_t current_technique()
		{
			if (const auto forced = *game::pForcedTechnique; forced != -1) {
				return forced;
			}

			return *game::pGrmRenderState1 ? *game::pTechniqueAlt : *game::pTechniqueDefault;
		}

		// number of passes grmShader::DrawGeometry (DrawSkinned: skinned) runs for the technique (0 if the shader has none for it)
		std::uint32_t technique_pass_count(const void* shader, const std::int32_t technique, const bool skinned = false)
		{
			if (technique < 0) {
				return 0u;
			}

			const auto base = static_cast<const std::uint8_t*>(shader);
			const auto technique_map = *reinterpret_cast<const std::uint8_t* const*>(base + 0x40); // 3 bytes per technique: rigid, skinned, ?
			const auto group_index = technique_map[3 * technique + (skinned ? 1 : 0)];
			
			if (!group_index) {
				return 0u;
			}

			const auto effect = *reinterpret_cast<const std::uint8_t* const*>(base + 0x18);
			const auto groups = *reinterpret_cast<const std::uint8_t* const*>(effect);
			return *reinterpret_cast<const std::uint16_t*>(groups - 0x10 + 0x10 * group_index + 0x0C);
		}

		// the textures the pass setup would bind, from the shader instance's variables (the same data the setup reads). A
		// variable usually holds a grcTextureReference that stays while its target changes (CPedVariationMgr sets the
		// component textures of every ped through grmShaderGroup::SetVar before the draw), so the resolved texture
		// (vtbl slot 15: a texture returns itself, a reference its target) is what tells ped variations apart
		std::uint64_t shader_texture_signature(const void* shader)
		{
			const auto data = reinterpret_cast<const game::shader_info_sub_s*>(static_cast<const std::uint8_t*>(shader) + 0x14);
			std::uint64_t h = 0x9E3779B97F4A7C15ull;
			if (!data->constants || !data->constant_float_count_array) {
				return h;
			}

			for (auto i = 0u; i < data->mCount; i++)
			{
				if (data->constant_float_count_array[i]) {
					continue;
				}

				const void* texture = data->constants[i].constant_ptr;
				if (texture)
				{
					const auto vtbl = *static_cast<void* const* const*>(texture);
					const auto resolve = reinterpret_cast<const void*(__thiscall*)(const void*)>(vtbl[15]);
					texture = resolve(texture);
				}

				h = (h ^ reinterpret_cast<std::uintptr_t>(texture)) * 0xBF58476D1CE4E5B9ull;
			}
			return h;
		}

		void drop_pass_set_locked(const void* geom, const std::int32_t technique, const std::uint64_t signature)
		{
			const auto sets_it = g_pass_sets.find(geom);
			if (sets_it == g_pass_sets.end()) {
				return;
			}

			std::erase_if(sets_it->second, [technique, signature](const pass_set_s& set) { return set.technique == technique && set.signature == signature; });
			if (sets_it->second.empty()) {
				g_pass_sets.erase(sets_it);
			}
		}

		/// submits the captured passes of a geometry instead of letting the engine set up and draw them
		/// @param bones palette of a skinned geometry (the transform then only translates)
		/// @return false if the engine has to draw the geometry (no complete owned pass set, or its refresh is due)
		bool try_skip_passes(const void* geom, const std::int32_t technique, const std::uint64_t signature, const remixapi_Transform& transform, const remixapi_Transform* bones)
		{
			static const auto gs = comp_settings::get();
			std::unique_lock lock(g_mutex);

			const auto sets_it = g_pass_sets.find(geom);
			if (sets_it == g_pass_sets.end())
			{
				count_pass_stat(pass_stat::full_no_set);
				return false;
			}

			const auto set_it = std::find_if(sets_it->second.begin(), sets_it->second.end(),
				[technique, signature](const pass_set_s& set) { return set.technique == technique && set.signature == signature; });
			if (set_it == sets_it->second.end())
			{
				count_pass_stat(pass_stat::full_no_set);
				return false;
			}

			if (gs->api_static_pass_refresh_frames.get_as<int>() > 0 && static_cast<std::int32_t>(g_frame - set_it->refresh_frame) >= 0)
			{
				count_pass_stat(pass_stat::full_refresh); // the engine draws it once, the capture renews the set
				return false;
			}

			if (set_it->generation != g_overrides_generation)
			{
				count_pass_stat(pass_stat::full_refresh);
				return false;
			}

			const auto rec_it = g_records.find(geom);
			if (rec_it == g_records.end())
			{
				count_pass_stat(pass_stat::full_stale);
				drop_pass_set_locked(geom, technique, signature);
				return false;
			}

			// everything is checked before anything is submitted so a stale set never draws half a geometry
			const geometry_record* records[MAX_SKIP_PASSES] = {};
			for (auto i = 0u; i < set_it->count; i++)
			{
				const auto key = set_it->state_keys[i];
				const auto it = std::find_if(rec_it->second.begin(), rec_it->second.end(), [key](const geometry_record& rec) { return rec.state_key == key; });
				
				if (it == rec_it->second.end() || !it->owned || (!it->hidden && !g_meshes.contains(it->mesh_hash)))
				{
					count_pass_stat(pass_stat::full_stale);
					drop_pass_set_locked(geom, technique, signature);
					return false;
				}

				records[i] = &*it;
			}

			submit_camera_once(); // also while api draws are hidden
			for (auto i = 0u; i < set_it->count; i++)
			{
				if (!records[i]->hidden && !g_debug_hide_api) {
					submit_record_locked(*records[i], transform, bones);
				}
			}
			return true;
		}

		// called by try_draw for every pass of the geometry the engine draws inside grmShader_DrawGeometry_hk
		void note_captured_pass(const bool owned, const std::uint64_t state_key, const drawcall_mod_context& ctx)
		{
			auto& c = g_pass_capture;
			if (!c.geometry || c.geometry != g_current_geometry || c.failed) {
				return;
			}

			// animated uv offsets change every frame, these passes stay with the engine
			const bool animated_uvs = static_cast<std::uint32_t>(ctx.modifiers.remix_modifier) & static_cast<std::uint32_t>(RemixModifier::UseGlobalUVs);
			if (!owned || animated_uvs || c.owned >= MAX_SKIP_PASSES)
			{
				c.failed = true;
				return;
			}

			c.state_keys[c.owned++] = state_key;
		}

		void end_pass_capture()
		{
			static const auto gs = comp_settings::get();
			const auto c = g_pass_capture;
			g_pass_capture = {};

			std::unique_lock lock(g_mutex);
			if (c.failed || c.owned != c.expected)
			{
				count_pass_stat(c.failed ? pass_stat::capture_rejected : pass_stat::capture_missing);
				if (c.failed && g_in_draw_ped_dc) {
					count_pass_stat(pass_stat::capture_rejected_ped);
				}
				drop_pass_set_locked(c.geometry, c.technique, c.signature);
				return;
			}

			count_pass_stat(pass_stat::capture_complete);

			auto& sets = g_pass_sets[c.geometry];
			auto set_it = std::find_if(sets.begin(), sets.end(), [&c](const pass_set_s& set) {
				return set.technique == c.technique && set.signature == c.signature;
			});

			if (set_it == sets.end()) {
				set_it = sets.emplace(sets.end());
			}

			set_it->technique = c.technique;
			set_it->signature = c.signature;
			set_it->generation = g_overrides_generation;
			set_it->count = c.owned;
			std::copy_n(c.state_keys, c.owned, set_it->state_keys);

			// spread over the second half of the interval so geometry captured in the same frame does not refresh in the same frame
			const auto refresh = static_cast<std::uint32_t>(std::max(0, gs->api_static_pass_refresh_frames.get_as<int>()));
			const auto spread = refresh > 1u ? static_cast<std::uint32_t>((reinterpret_cast<std::uintptr_t>(c.geometry) >> 4) % (refresh / 2u)) : 0u;
			set_it->refresh_frame = g_frame + refresh - spread;
		}

		// grmShader::DrawGeometry runs every pass of the shader's technique for one geometry: pass setup (render states,
		// shader constants, textures) followed by grmGeometryQB::Draw. Geometry whose passes are all owned by the api path
		// skips all of it and only submits its instances.
		bool ped_pass_skip_enabled()
		{
			static const auto gs = comp_settings::get();
			return renderer_api::is_enabled() && gs->api_peds_enabled.get_as<bool>() && gs->api_peds_skip_pass_setup.get_as<bool>();
		}

		void __fastcall grmShader_DrawGeometry_hk(void* shader, [[maybe_unused]] void* edx, const game::grmModel* model, const int geometry_index, const int lod, const std::uint32_t is_last)
		{
			const bool enabled = g_in_draw_ped_dc ? ped_pass_skip_enabled() : renderer_api::is_pass_skip_enabled();
			if (!enabled || !in_api_draw_window() || g_is_instance_rendering || imgui::get()->m_dbg_do_not_render_static)
			{
				grmShader_DrawGeometry_og(shader, model, geometry_index, lod, is_last);
				return;
			}

			const auto technique = current_technique();
			const auto passes = technique_pass_count(shader, technique);
			const auto geom = reinterpret_cast<const game::grmGeometryQB* const*>(model->m_geometries.pData)[geometry_index];
			// morphed geometry is compared against its record every draw
			if (!passes || passes > MAX_SKIP_PASSES || !geom || geom->double_buffered)
			{
				if (passes > MAX_SKIP_PASSES) {
					count_pass_stat(pass_stat::full_too_many_passes);
				}

				grmShader_DrawGeometry_og(shader, model, geometry_index, lod, is_last);
				return;
			}

			const auto signature = shader_texture_signature(shader);
			{
				GTA4_PERF_SCOPE(performance_section::ApiPassSkip);
				if (try_skip_passes(geom, technique, signature, to_transform(*game::pCurrentWorldTransform), nullptr))
				{
					g_pass_skips_this_frame++;
					if (g_in_draw_ped_dc) {
						g_ped_pass_skips_this_frame++;
					}

					return;
				}
			}

			// the engine resets the pass render states only after the last geometry of a shader; the next geometry
			// may be skipped, so they are reset after every pass here
			g_pass_full_draws_this_frame++;
			g_pass_capture = { .geometry = geom, .technique = technique, .signature = signature, .expected = passes };
			grmShader_DrawGeometry_og(shader, model, geometry_index, lod, 1u);
			end_pass_capture();
		}

		// grmShader::DrawSkinned: the same pass loop, the geometry is drawn by grmGeometryQB::DrawSkinned which uploads the bone
		// palette (c64, three registers per bone) from the matrix set. Ped geometry the api path owns builds that palette itself.
		void __fastcall grmShader_DrawSkinned_hk(void* shader, [[maybe_unused]] void* edx, const game::grmModel* model, const int geometry_index, const void* matrix_set,
			const int lod, const std::uint32_t is_last)
		{
			if (!ped_pass_skip_enabled() || !g_in_draw_ped_dc || g_is_instance_rendering || !game::pBikeHelmetDrawFlag || *game::pBikeHelmetDrawFlag)
			{
				grmShader_DrawSkinned_og(shader, model, geometry_index, matrix_set, lod, is_last);
				return;
			}

			const auto technique = current_technique();
			const auto passes = technique_pass_count(shader, technique, true);
			const auto geom = reinterpret_cast<const game::grmGeometryQB* const*>(model->m_geometries.pData)[geometry_index];
			if (!passes || passes > MAX_SKIP_PASSES || !geom || geom->double_buffered || geom->second_vertex_buffer || !matrix_set)
			{
				grmShader_DrawSkinned_og(shader, model, geometry_index, matrix_set, lod, is_last);
				return;
			}

			// grmGeometryQB +0x38: bone ids (WORD) of the palette, +0x3E: their count; the matrix set holds 48 byte matrices from +0x20
			const auto g = reinterpret_cast<const std::uint8_t*>(geom);
			const auto bone_ids = *reinterpret_cast<const std::uint16_t* const*>(g + 0x38);
			const auto bone_count = *reinterpret_cast<const std::uint16_t*>(g + 0x3E);
			if (!bone_ids || !bone_count || bone_count > renderer_api::MAX_BONES)
			{
				grmShader_DrawSkinned_og(shader, model, geometry_index, matrix_set, lod, is_last);
				return;
			}

			const auto signature = shader_texture_signature(shader);
			{
				GTA4_PERF_SCOPE(performance_section::ApiPassSkip);

				remixapi_Transform bones[renderer_api::MAX_BONES] = {};
				const auto matrices = static_cast<const std::uint8_t*>(matrix_set) + 0x20;
				for (auto b = 0u; b < bone_count; b++) {
					std::memcpy(bones[b].matrix, matrices + 48u * bone_ids[b], sizeof(bones[b].matrix));
				}

				remixapi_Transform transform = {};
				for (auto r = 0; r < 3; r++)
				{
					transform.matrix[r][r] = 1.0f;
					transform.matrix[r][3] = game::pCurrentWorldTransform->m[3][r];
				}

				if (try_skip_passes(geom, technique, signature, transform, bones))
				{
					g_pass_skips_this_frame++;
					g_ped_pass_skips_this_frame++;
					return;
				}
			}

			g_pass_full_draws_this_frame++;
			g_pass_capture = { .geometry = geom, .technique = technique, .signature = signature, .expected = passes };
			grmShader_DrawSkinned_og(shader, model, geometry_index, matrix_set, lod, 1u);
			end_pass_capture();
		}

		// CRenderPhase::BuildDrawListWrapper: the phase object is valid for the whole BuildDrawList, so its id
		// (vtable slot 9) is read here once instead of trusting CRenderPhase::sm_pCurrent elsewhere
		void __fastcall CRenderPhase_BuildDrawListWrapper_hk(void* phase, [[maybe_unused]] void* edx)
		{
			const auto vtbl = *reinterpret_cast<void***>(phase);
			const auto get_id = reinterpret_cast<std::uint32_t(__thiscall*)(void*)>(vtbl[0x24 / 4]);
			g_current_phase_id = get_id(phase);
			CRenderPhase_BuildDrawListWrapper_og(phase);
			g_current_phase_id = 0u;
		}

		bool drawable_fully_owned(const rmcDrawable* drawable)
		{
			const auto lod = drawable->lods[0];
			if (!lod || !lod->model_count) {
				return false;
			}

			std::shared_lock lock(g_mutex);
			bool any = false;
			for (auto m = 0u; m < lod->model_count; m++)
			{
				const auto model = reinterpret_cast<const game::grmModel*>(lod->models[m]);
				if (!model) {
					return false;
				}

				const auto geometries = reinterpret_cast<game::grmGeometryQB* const*>(model->m_geometries.pData);
				for (auto g = 0; g < model->m_geometries.wCount; g++)
				{
					const auto it = g_records.find(geometries[g]);
					if (it == g_records.end() || it->second.empty()) {
						return false;
					}

					for (const auto& rec : it->second)
					{
						if (!rec.owned) {
							return false;
						}
					}
					any = true;
				}
			}
			return any;
		}

		int __fastcall CEntity_AddToDrawListBuckets_hk(CEntityPC* ent, [[maybe_unused]] void* edx, std::uint32_t bucket, std::uint32_t render_mode, std::uint8_t alpha, int lod)
		{
			// the engine returns 0 unconditionally and ignores lod
			const auto fallback = [&]() { return CEntity_AddToDrawListBuckets_og(ent, bucket, render_mode, alpha, lod); };

			if (!renderer_api::is_entity_skip_enabled() || render_mode != 0u || alpha != 255u) {
				return fallback();
			}

			const auto type = (ent->flags2 >> 6) & 0xFu;
			// entities without a matrix get one allocated by the engine on their first draw
			if (type != ENTITY_TYPE_BUILDING || (ent->flags & ENTITY_FLAG_FRAGMENT) || !ent->matrix || !ent->drawable_handle || !*ent->drawable_handle) {
				return fallback();
			}

			if (g_current_phase_id != RENDER_PHASE_GBUFFER && g_current_phase_id != RENDER_PHASE_DRAW_SCENE) {
				return fallback();
			}

			const auto drawable = *ent->drawable_handle;
			if (!drawable_fully_owned(drawable))
			{
				g_entity_fallbacks_this_frame++;
				return fallback();
			}

			// the entity is pushed once per bucket it uses; our command covers all captured passes at once.
			// every refresh interval the engine draws the entity again so the draw hook refreshes the per draw values.
			{
				static const auto gs = comp_settings::get();
				const auto refresh = static_cast<std::uint32_t>(std::max(0, gs->api_static_entity_refresh_frames.get_as<int>()));

				std::unique_lock lock(g_mutex);
				auto& frames = g_entity_frames[ent];

				if (frames.api_frame == g_game_frame || frames.engine_frame == g_game_frame)
				{
					const bool engine_draws = frames.engine_frame == g_game_frame;
					lock.unlock();
					return engine_draws ? fallback() : 0;
				}

				const bool due = refresh && g_game_frame - frames.engine_frame >= refresh;
				
				const bool stale = 
					frames.generation != g_overrides_generation
					&& g_game_frame - g_overrides_generation_game_frame >= (reinterpret_cast<std::uintptr_t>(ent) >> 4) % OVERRIDES_REFRESH_SPREAD;
				
				if (due || stale)
				{
					frames.engine_frame = g_game_frame;
					frames.generation = g_overrides_generation;
					lock.unlock();
					return fallback();
				}

				frames.api_frame = g_game_frame;
			}

			// same bookkeeping on the drawable handle as the engine around its own command
			reinterpret_cast<CGtaDrawable_DrawListHook_t>(game::func_addr__CGtaDrawable_BeforeAddToDrawList)(ent->drawable_handle, render_mode);
			const auto after_draw = [&]() { reinterpret_cast<CGtaDrawable_DrawListHook_t>(game::func_addr__CGtaDrawable_AfterDraw)(ent->drawable_handle, render_mode); };

			const auto dc = static_cast<api_entity_dc*>(reinterpret_cast<CBaseDC_operator_new_t>(game::func_addr__CBaseDC_operator_new)(ENTITY_DC_SIZE, 0));
			if (!dc)
			{
				after_draw();
				return 0;
			}

			// keeps the model (and with it the drawable) alive until the render thread consumed the command
			reinterpret_cast<CDrawListMgr_AddModelInfoReference_t>(game::func_addr__CDrawListMgr_AddModelInfoReference)(game::pgDrawListMgr, ent->model_index);

			dc->vtbl = g_api_entity_dc_vtable;
			dc->header ^= ((*game::pDrawCommandIndexCounter)++ ^ dc->header) & 0x3FFFu;

			const auto transform = to_transform(ent->matrix);
			std::memcpy(dc->matrix, transform.matrix, sizeof(dc->matrix));
			dc->drawable = drawable;
			dc->pad = 0u;

			reinterpret_cast<CBaseDC_Init_t>(game::func_addr__CBaseDC_Init)(dc);
			after_draw();
			g_entity_skips_this_frame++;
			return 0;
		}

		void __fastcall grmGeometryQB_Draw_hk(const game::grmGeometryQB* geom, [[maybe_unused]] void* edx)
		{
			g_current_geometry = geom;
			grmGeometryQB_Draw_og(geom);
			g_current_geometry = nullptr;
		}

		// api materials bind the D3D texture (the runtime keeps its image), records and the content hash cache are keyed by
		// it; a texture created later at the same address, or new content written into it, must not find any of them
		// @note caller holds the unique lock
		// @return true if anything was bound to the texture
		bool invalidate_texture_locked(IDirect3DBaseTexture9* texture)
		{
			g_texture_hashes.erase(texture);

			std::vector<std::uint64_t> material_keys;
			if (const auto it = g_materials_by_texture.find(texture); it != g_materials_by_texture.end())
			{
				material_keys = std::move(it->second);
				g_materials_by_texture.erase(it);
			}

			if (!material_keys.empty())
			{
				std::vector<std::uint64_t> mesh_hashes;
				for (const auto& [hash, mesh] : g_meshes)
				{
					if (std::find(material_keys.begin(), material_keys.end(), mesh.material_key) != material_keys.end()) {
						mesh_hashes.push_back(hash);
					}
				}

				for (const auto hash : mesh_hashes) {
					destroy_mesh_locked(hash);
				}

				const auto& bridge = shared::common::remix_api::get().m_bridge;
				for (const auto key : material_keys)
				{
					if (const auto it = g_materials.find(key); it != g_materials.end())
					{
						if (it->second.handle) {
							bridge.DestroyMaterial(it->second.handle);
						}
						g_materials.erase(it);
					}
				}
			}

			bool had_records = false;
			for (auto& [geom, records] : g_records) {
				had_records |= std::erase_if(records, [texture](const geometry_record& rec) { return rec.texture == texture; }) != 0u;
			}
			return !material_keys.empty() || had_records;
		}

		void on_texture_destroyed(IDirect3DBaseTexture9* texture)
		{
			std::unique_lock lock(g_mutex);
			invalidate_texture_locked(texture);
			g_texture_writes.erase(texture);
		}

		// the texture of stage 0 (the colormap of a fixed function draw)
		IDirect3DBaseTexture9* bound_colormap(IDirect3DDevice9* dev)
		{
			IDirect3DBaseTexture9* texture = nullptr;
			if (SUCCEEDED(dev->GetTexture(0, &texture)) && texture) {
				texture->Release(); // the game keeps its own reference
			}
			return texture;
		}

		// @note caller holds the lock
		bool is_volatile_texture_locked(IDirect3DBaseTexture9* texture)
		{
			const auto it = g_texture_writes.find(texture);
			if (it == g_texture_writes.end() || !it->second.is_volatile) {
				return false;
			}
			if (g_frame - it->second.last_frame > VOLATILE_TEXTURE_DECAY_FRAMES)
			{
				it->second.is_volatile = false;
				g_volatile_textures--;
				return false;
			}
			return true;
		}

		// frame start: rebuild what was bound to the textures the game wrote since the last frame
		void process_texture_writes()
		{
			std::vector<IDirect3DBaseTexture9*> written;
			{
				std::lock_guard guard(g_written_textures_mutex);
				written.assign(g_written_textures.begin(), g_written_textures.end());
				g_written_textures.clear();
			}

			g_texture_writes_last_frame = static_cast<std::uint32_t>(written.size());
			g_texture_rebuilds_last_frame = 0u;
			if (written.empty()) {
				return;
			}

			std::unique_lock lock(g_mutex);
			for (const auto texture : written)
			{
				auto& writes = g_texture_writes[texture];
				writes.streak = writes.last_frame + 1u == g_frame ? writes.streak + 1u : 1u;
				writes.last_frame = g_frame;
				if (!writes.is_volatile && writes.streak >= VOLATILE_TEXTURE_FRAMES)
				{
					writes.is_volatile = true;
					g_volatile_textures++;
				}
				if (invalidate_texture_locked(texture)) {
					g_texture_rebuilds_last_frame++;
				}
			}
		}

		DETOUR_TYPEDEF(IDirect3DTexture9_LockRect, HRESULT, __stdcall, IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* locked, const RECT* rect, DWORD flags);
		DETOUR_TYPEDEF(IDirect3DTexture9_UnlockRect, HRESULT, __stdcall, IDirect3DTexture9* self, UINT level);

		// a level 0 lock without D3DLOCK_READONLY is a write; the content is complete at the unlock
		HRESULT __stdcall IDirect3DTexture9_LockRect_hk(IDirect3DTexture9* self, const UINT level, D3DLOCKED_RECT* locked, const RECT* rect, const DWORD flags)
		{
			const auto hr = IDirect3DTexture9_LockRect_og(self, level, locked, rect, flags);
			if (SUCCEEDED(hr) && level == 0u && !(flags & D3DLOCK_READONLY))
			{
				std::lock_guard guard(g_written_textures_mutex);
				g_write_locked_textures.insert(self);
			}
			return hr;
		}

		HRESULT __stdcall IDirect3DTexture9_UnlockRect_hk(IDirect3DTexture9* self, const UINT level)
		{
			const auto hr = IDirect3DTexture9_UnlockRect_og(self, level);
			if (level == 0u)
			{
				std::lock_guard guard(g_written_textures_mutex);
				if (g_write_locked_textures.erase(self)) {
					g_written_textures.insert(self);
				}
			}
			return hr;
		}

		DETOUR_TYPEDEF(grcTexturePC_dtor, void, __thiscall, void* texture);

		void __fastcall grcTexturePC_dtor_hk(void* texture, [[maybe_unused]] void* edx)
		{
			if (const auto handle = *reinterpret_cast<IDirect3DBaseTexture9* const*>(static_cast<const std::uint8_t*>(texture) + 0x18); handle) {
				on_texture_destroyed(handle);
			}

			grcTexturePC_dtor_og(texture);
		}

		void __fastcall grmGeometryQB_dtor_hk(const game::grmGeometryQB* geom, [[maybe_unused]] void* edx)
		{
			{
				std::unique_lock lock(g_mutex);
				g_records.erase(geom); // the mesh itself stays cached by content
				g_pass_sets.erase(geom);
			}
			grmGeometryQB_dtor_og(geom);
		}

		void on_game_frame()
		{
			g_game_frame++;
			if ((g_game_frame & 0x3FFu) == 0u)
			{
				// entities are pool slots that get reused, drop stale entries now and then
				std::unique_lock lock(g_mutex);
				std::erase_if(g_entity_frames, [](const auto& e) { return g_game_frame - std::max(e.second.api_frame, e.second.engine_frame) > 4u; });
			}
		}
	}

	// #
	// public

	const char* renderer_api::reject_reason_name(const reject_reason r)
	{
		switch (r)
		{
		case reject_reason::none: return "none";
		case reject_reason::no_geometry: return "no geometry";
		case reject_reason::no_cpu_data: return "no vertex data";
		case reject_reason::primitive_type: return "primitive type";
		case reject_reason::vertex_format: return "vertex format";
		case reject_reason::index_range: return "index range";
		case reject_reason::empty: return "empty";
		case reject_reason::material_failed: return "material create failed";
		case reject_reason::mesh_failed: return "mesh create failed";
		case reject_reason::bone_range: return "too many bones";
		case reject_reason::volatile_texture: return "colormap written every frame";
		default: return "?";
		}
	}

	std::uint32_t renderer_api::rage_hash(const std::string_view str)
	{
		std::uint32_t h = 0u;
		for (auto c : str)
		{
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c + 32);
			} else if (c == '\\') {
				c = '/';
			}

			h += static_cast<std::uint8_t>(c);
			h += h << 10;
			h ^= h >> 6;
		}

		h += h << 3;
		h ^= h >> 11;
		h += h << 15;
		return h;
	}

	bool renderer_api::is_runtime_supported()
	{
		const auto& api = shared::common::remix_api::get();
		return api.is_initialized() && api.m_bridge.CreateMaterial != nullptr && api.m_bridge.CreateMesh != nullptr && api.m_bridge.DrawInstance != nullptr;
	}

	bool renderer_api::is_enabled()
	{
		return is_initialized() && comp_settings::get()->api_static_enabled.get_as<bool>() && is_runtime_supported();
	}

	bool renderer_api::is_entity_skip_enabled()
	{
		return is_enabled() && comp_settings::get()->api_static_entity_skip.get_as<bool>();
	}

	bool renderer_api::is_pass_skip_enabled()
	{
		return is_enabled() && comp_settings::get()->api_static_skip_pass_setup.get_as<bool>();
	}

	bool renderer_api::try_draw(IDirect3DDevice9* dev, drawcall_mod_context& ctx, const D3DPRIMITIVETYPE type, const UINT prim_count)
	{
		if (!is_enabled()) {
			return false;
		}

		if (!in_api_draw_window())
		{
			if (g_is_rendering_vehicle) {
				g_skips_vehicle_this_frame++;
			} else if (ctx.info.preset_index >= GTA_PED && ctx.info.preset_index <= GTA_PED_SKIN_BLENDSHAPE) {
				g_skips_ped_this_frame++;
			}
			else
			{
				g_skips_no_window_this_frame++;
				if (g_unknown_draws.size() < 32u)
				{
					const auto name = texture_name(colormap_ref(ctx));
					unknown_draw_s entry = { std::string(ctx.info.preset_name), name ? name : "?" };
					
					const auto known = std::any_of(g_unknown_draws.begin(), g_unknown_draws.end(), [&entry](const unknown_draw_s& u) {
							return u.preset == entry.preset && u.colormap == entry.colormap;
						});

					if (!known) {
						g_unknown_draws.push_back(std::move(entry));
					}
				}
			}
			return false;
		}

		// depth-only draws (colour writes off: the interior occluders of some buildings) and draws into other targets: the
		// runtime drops them on the D3D9 path, the api would draw them
		if (!draws_to_primary_target(dev))
		{
			g_skips_target_this_frame++;
			return false;
		}

		std::uint64_t state_key = 0u;
		const bool owned = try_draw_impl(dev, ctx, type, prim_count, nullptr, state_key);
		note_captured_pass(owned, state_key, ctx);
		return owned;
	}

	bool renderer_api::try_draw_ped(IDirect3DDevice9* dev, drawcall_mod_context& ctx, const D3DPRIMITIVETYPE type, const UINT prim_count)
	{
		static const auto gs = comp_settings::get();
		g_ped_candidate = false;

		if (!is_enabled() || !gs->api_peds_enabled.get_as<bool>()) {
			return false;
		}

		const auto pidx = ctx.info.preset_index;

		const bool ped_preset = pidx == GTA_PED || pidx == GTA_PED_ALPHA || pidx == GTA_PED_REFLECT || pidx == GTA_PED_SKIN
			|| pidx == GTA_PED_SKIN_BLENDSHAPE || pidx == GTA_HAIR_SORTED_ALPHA || pidx == GTA_HAIR_SORTED_ALPHA_EXPENSIVE;
		
		if (!g_in_draw_ped_dc && !ped_preset) {
			return false;
		}

		g_ped_candidate = true;

		if (g_is_rendering_mirror || g_is_rendering_phone || g_is_rendering_vehicle || g_is_instance_rendering)
		{
			g_ped_skips_context_this_frame++;
			note_kept_ped_draw("mirror / phone / vehicle / instanced", ctx);
			return false;
		}

		const auto geom = g_current_geometry;
		if (!geom)
		{
			note_kept_ped_draw("no geometry", ctx);
			return false;
		}

		if (geom->second_vertex_buffer)
		{
			g_ped_skips_stream_this_frame++;
			note_kept_ped_draw("second vertex stream", ctx);
			return false;
		}

		if (geom->double_buffered && !gs->api_peds_blendshapes.get_as<bool>())
		{
			g_ped_skips_blendshape_this_frame++;
			note_kept_ped_draw("blendshape heads off", ctx);
			return false;
		}

		if (!draws_to_primary_target(dev))
		{
			g_ped_skips_target_this_frame++;
			note_kept_ped_draw("other render target", ctx);
			return false;
		}

		draw_pose_s pose;
		if (!read_ped_pose(dev, geom, pose))
		{
			g_ped_skips_rejected_this_frame++;
			note_kept_ped_draw("skinned shader without blend data", ctx);
			return false;
		}

		std::uint64_t state_key = 0u;
		const bool owned = try_draw_impl(dev, ctx, type, prim_count, &pose, state_key);
		note_captured_pass(owned, state_key, ctx);
		
		if (!owned)
		{
			g_ped_skips_rejected_this_frame++;
			const char* reason = "pending (create budget, dual render, ...)";
			{
				std::shared_lock lock(g_mutex);
				if (const auto it = g_records.find(geom); it != g_records.end())
				{
					for (const auto& rec : it->second)
					{
						if (rec.state_key == state_key && rec.reason != reject_reason::none) {
							reason = reject_reason_name(rec.reason);
						}
					}
				}
			}
			note_kept_ped_draw(reason, ctx);
			return false;
		}

		g_ped_instances_this_frame++;
		return true;
	}

	bool renderer_api::debug_hide_d3d9_ped()
	{
		return g_debug_hide_d3d9_peds && g_ped_candidate;
	}

	bool renderer_api::try_draw_vehicle(IDirect3DDevice9* dev, drawcall_mod_context& ctx, const D3DPRIMITIVETYPE type, const UINT prim_count, const bool overlay)
	{
		static const auto gs = comp_settings::get();
		const bool is_tyre = ctx.info.preset_index == GTA_VEHICLE_TIRE;
		
		if (!is_enabled() || !gs->api_vehicles_enabled.get_as<bool>() || (!g_is_rendering_vehicle && !is_tyre) || (is_tyre && g_debug_tyres_on_d3d9)) {
			return false;
		}

		std::uint64_t record_key = 0u;
		if (overlay)
		{
			g_vehicle_pose.overlay = true;
			const bool owned = try_draw_impl(dev, ctx, type, prim_count, &g_vehicle_pose, record_key);
			count_vehicle_stat(owned ? vehicle_stat::overlays : vehicle_stat::skips_rejected);
			return owned;
		}

		if (g_is_rendering_mirror || g_is_rendering_phone || (g_is_instance_rendering && !is_tyre))
		{
			count_vehicle_stat(vehicle_stat::skips_context);
			return false;
		}

		// flat tyres are flattened against the ground in the vertex shader, the shape changes while the wheel turns
		BOOL flat = FALSE;
		if (is_tyre && ctx.info.vs_tyre_deform_register >= 0) {
			dev->GetVertexShaderConstantB(static_cast<UINT>(ctx.info.vs_tyre_deform_register), &flat, 1);
		}

		const auto geom = current_geometry();
		if (!geom || geom->second_vertex_buffer || geom->double_buffered)
		{
			count_vehicle_stat(vehicle_stat::skips_rejected);
			return false;
		}

		// light proxies offset the world matrix or blend additively on top, only dirt and livery passes are mirrored
		if (ctx.modifiers.dual_render && (ctx.modifiers.dual_render_mode_blend_add || ctx.modifiers.dual_render_mode_blend_diffuse || ctx.modifiers.dual_render_mode_emissive_offset))
		{
			count_vehicle_stat(vehicle_stat::skips_overlay);
			return false;
		}

		if (!draws_to_primary_target(dev))
		{
			count_vehicle_stat(vehicle_stat::skips_target);
			return false;
		}

		const bool rigid_tyre = is_tyre && !vertex_shader_is_skinned(dev);
		bool pose_read = rigid_tyre ? read_tyre_pose(dev, ctx, g_vehicle_pose) : read_vehicle_pose(dev, ctx, geom, g_vehicle_pose);
		if (pose_read && rigid_tyre && flat)
		{
			// the per wheel block RenderInstanced uploads: matWheelTransform, tyreDeformParams, tyreDeformParams2
			const auto reg = ctx.info.vs_wheel_transform_register >= 0 ? ctx.info.vs_wheel_transform_register : 64;
			pose_read = SUCCEEDED(dev->GetVertexShaderConstantF(static_cast<UINT>(reg), &g_vehicle_pose.tyre_constants[0][0], 6));
			g_vehicle_pose.flat_tyre = true;
			g_vehicle_pose.flat_hash = hash_words(g_vehicle_pose.tyre_constants, sizeof(g_vehicle_pose.tyre_constants));
		}

		if (!pose_read || !try_draw_impl(dev, ctx, type, prim_count, &g_vehicle_pose, record_key))
		{
			count_vehicle_stat(vehicle_stat::skips_rejected);
			return false;
		}

		count_vehicle_stat(g_vehicle_pose.flat_tyre ? vehicle_stat::flat_tyres : is_tyre ? vehicle_stat::tyres : vehicle_stat::instances);
		
		if (g_vehicle_pose.damage_hash) {
			count_vehicle_stat(vehicle_stat::damaged);
		}

		return true;
	}

	bool renderer_api::try_draw_impl(IDirect3DDevice9* dev, drawcall_mod_context& ctx, const D3DPRIMITIVETYPE type, const UINT prim_count, const draw_pose_s* pose, std::uint64_t& out_state_key)
	{
		if (g_is_instance_rendering && !(pose && pose->vehicle))
		{
			g_skips_instanced_this_frame++;
			return false;
		}

		if (!current_geometry())
		{
			g_skips_no_geometry_this_frame++;
			return false;
		}

		if ((ctx.modifiers.dual_render || ctx.modifiers.tri_render) && !(pose && pose->vehicle))
		{
			g_skips_dual_render_this_frame++;
			return false;
		}

		const auto gs = comp_settings::get();
		if (gs->api_static_require_plain_draw.get_as<bool>()
			&& (ctx.modifiers.remix_modifier != RemixModifier::None || ctx.modifiers.remix_instance_categories != InstanceCategories::None))
		{
			g_modifier_skips_this_frame++;
			return false;
		}

		GTA4_PERF_SCOPE(performance_section::RendererApi);

		const auto geom = current_geometry();
		const bool double_buffered = geom->double_buffered != 0u;
		const auto state = capture_draw_state(dev);
		const auto state_key = state.key();

		// peds and vehicles of one model share their geometry but not their textures (clothing / head variations,
		// liveries, dirt), each texture gets its own record
		IDirect3DBaseTexture9* color_texture = nullptr;
		if (pose && SUCCEEDED(dev->GetTexture(0, &color_texture)) && color_texture) {
			color_texture->Release();
		}

		const auto record_key = state_key ^ (reinterpret_cast<std::uintptr_t>(color_texture) * 0x9FB21C651E98DF25ull);
		out_state_key = record_key;
		const auto transform = pose ? pose->transform : to_transform(*game::pCurrentWorldTransform);
		const auto bones = pose && pose->skinned ? pose->bones : nullptr;
		const bool beam = static_cast<std::uint32_t>(ctx.modifiers.remix_instance_categories) & static_cast<std::uint32_t>(InstanceCategories::Beam);
		const bool drop_vertex_colors = wants_vertex_colors_dropped(ctx) && !beam;
		const bool count_uv_fallback = pose && pose->overlay;
		const bool hidden = is_ignored_draw(ctx);

		const auto find_record = [geom, record_key]() -> geometry_record*
		{
			const auto it = g_records.find(geom);
			if (it == g_records.end()) {
				return nullptr;
			}

			for (auto& rec : it->second)
			{
				if (rec.state_key == record_key) {
					return &rec;
				}
			}

			return nullptr;
		};

		// fast path: already owned
		{
			std::unique_lock lock(g_mutex);
			const auto rec = find_record();

			// not owned without a reason: its mesh was destroyed (cache budget, morph, vertex color change), build again below
			if (rec && !rec->owned && rec->reason != reject_reason::none)
			{
				// a colormap written every frame is checked again now and then, the texture may be gone or calm by now
				if (rec->reason != reject_reason::volatile_texture || g_frame - rec->last_frame < VOLATILE_TEXTURE_DECAY_FRAMES
					|| is_volatile_texture_locked(color_texture ? color_texture : bound_colormap(dev))) {
					return false;
				}
				*rec = { .state_key = rec->state_key };
			}

			const auto mesh_it = rec && rec->owned && !rec->hidden ? g_meshes.find(mesh_variant_hash(rec->mesh_hash, pose)) : g_meshes.end();
			if (rec && rec->owned && (rec->hidden || g_debug_hide_api))
			{
				submit_camera_once();
				return true;
			}

			if (rec && mesh_it != g_meshes.end())
			{
				const auto mat_it = g_materials.find(mesh_it->second.material_key);
				if (mat_it == g_materials.end()) {
					return false;
				}

				// blendshape morphs rewrite the vertices, the mesh is rebuilt whenever they changed
				bool vertices_changed = false;
				if (double_buffered)
				{
					geometry_bytes_s current;
					if (!read_geometry_bytes(dev, geom, current, true)) {
						return false;
					}
					vertices_changed = hash_words(current.vertices, static_cast<std::size_t>(current.vertex_count) * current.stride) != rec->vertex_hash;
				}

				if (vertices_changed)
				{
					destroy_mesh_locked(mesh_it->first);
					lock.unlock();
					return try_draw_impl(dev, ctx, type, prim_count, pose, out_state_key);
				}

				if (mesh_it->second.vertex_colors_dropped != drop_vertex_colors)
				{
					// the mesh content depends on the draw's vertex color handling (hash stays the same), rebuild below
					destroy_mesh_locked(mesh_it->first);
					lock.unlock();
					return try_draw_impl(dev, ctx, type, prim_count, pose, out_state_key);
				}

				rec->overrides = make_overrides(ctx);
				rec->tfactor = state.tfactor;
				rec->last_frame = g_frame;
				submit_record_locked(*rec, transform, bones, mesh_it->first);

				if (count_uv_fallback && mesh_it->second.uv_fallback) {
					count_vehicle_stat(vehicle_stat::overlay_uv_fallbacks);
				}

				return true;
			}
		}

		std::unique_lock lock(g_mutex);
		const auto new_record = [geom, record_key]() -> geometry_record&
		{
			auto& records = g_records[geom];
			for (auto& rec : records)
			{
				if (rec.state_key == record_key)
				{
					rec = { .state_key = record_key };
					return rec;
				}
			}
			records.push_back({ .state_key = record_key });
			return records.back();
		};

		// the runtime drops D3D9 draws with the Ignore category, the api path never submits them
		if (hidden)
		{
			auto& rec = new_record();
			rec.owned = true;
			rec.hidden = true;
			rec.texture = color_texture;
			rec.last_frame = g_frame;
			return true;
		}

		// build (budgeted per frame so streaming spikes are spread out; the draw stays on D3D9 meanwhile)
		if (type != D3DPT_TRIANGLELIST)
		{
			new_record().reason = reject_reason::primitive_type;
			return false;
		}

		// morphed geometry is rebuilt every time it changes, the budget would leave it on D3D9 for frames
		const auto budget = std::max(0, gs->api_static_creates_per_frame.get_as<int>());
		if (!double_buffered && !(pose && pose->flat_tyre) && static_cast<int>(g_creates_this_frame) >= budget)
		{
			g_budget_exhausted_this_frame = true;
			return false;
		}

		geometry_record& rec = new_record();

		geometry_bytes_s bytes;
		if (!read_geometry_bytes(dev, geom, bytes, double_buffered))
		{
			rec.reason = reject_reason::no_cpu_data;
			return false;
		}

		if (is_volatile_texture_locked(color_texture ? color_texture : bound_colormap(dev)))
		{
			rec.reason = reject_reason::volatile_texture;
			rec.last_frame = g_frame;
			return false;
		}

		std::uint64_t material_key = 0u;
		const auto material = find_or_create_material(dev, ctx, state, material_key);
		if (!material)
		{
			rec.reason = reject_reason::material_failed;
			return false;
		}

		// stable across sessions: geometry bytes + colormap name + draw state (an api mesh binds its material, the same
		// geometry drawn by the opaque and the emissive pass needs one mesh per pass)
		const auto index_bytes = static_cast<std::size_t>(std::min<std::uint32_t>(bytes.index_count, prim_count * 3u)) * sizeof(std::uint16_t);
		std::uint64_t hash = fnv1a64(bytes.indices, index_bytes);
		hash = fnv1a64(bytes.vertices, static_cast<std::size_t>(bytes.vertex_count) * bytes.stride, hash);
		hash = fnv1a64(&bytes.stride, sizeof(bytes.stride), hash);
		hash ^= static_cast<std::uint64_t>(rage_hash(material->colormap_name)) * 0x9E3779B97F4A7C15ull;
		hash ^= (state_key + 0x9E3779B97F4A7C15ull) * 0xBF58476D1CE4E5B9ull;
		const bool flip_winding = state.cull_mode == D3DCULL_CW;
		
		if (flip_winding) {
			hash ^= 0xC2B2AE3D27D4EB4Full;
		}

		const bool skinned = bones != nullptr;
		if (skinned) {
			hash ^= 0x94D049BB133111EBull;
		}

		// overlay textures (livery, dirt) differ per car while the colormap name stays the paint's (not session stable)
		if (pose && pose->overlay) {
			hash ^= material_key * 0xE7037ED1A0B428DBull;
		}

		// ped models share component meshes and texture names, the texture itself tells them apart
		if (pose && !pose->vehicle)
		{
			const auto texture_hash = texture_content_hash(color_texture);
			hash ^= (texture_hash ? texture_hash : material_key) * 0x94D049BB133111EBull;
		}
		if (!hash) {
			hash = 1u;
		}
		const auto draw_hash = mesh_variant_hash(hash, pose);

		auto mesh_it = g_meshes.find(draw_hash);
		if (mesh_it != g_meshes.end())
		{
			g_cache_hits_this_frame++;
		}
		else
		{
			const damage_lock_s damage(pose && pose->damage_hash ? pose->damage_texture : nullptr);
			if (pose && pose->damage_hash && !damage.valid()) {
				return false;
			}

			std::uint32_t bone_count = 0u;
			bool uv_fallback = false;
			const auto reason = build_vertices(bytes, prim_count, flip_winding, drop_vertex_colors, skinned, state.uv_set, pose != nullptr,
				damage.valid() ? &damage.map : nullptr, pose ? pose->bound_radius : 0.0f, pose && pose->flat_tyre ? pose->tyre_constants : nullptr,
				bone_count, uv_fallback);
			
			if (reason != reject_reason::none)
			{
				rec.reason = reason;
				return false;
			}

			remixapi_MeshInfoSurfaceTriangles triangles =
			{
				.vertices_values = g_vertices.data(),
				.vertices_count = static_cast<std::uint32_t>(g_vertices.size()),
				.indices_values = g_indices.data(),
				.indices_count = static_cast<std::uint32_t>(g_indices.size()),
				.skinning_hasvalue = skinned,
				.skinning_value =
				{
					.bonesPerVertex = 4u,
					.blendWeights_values = g_blend_weights.data(),
					.blendWeights_count = static_cast<std::uint32_t>(g_blend_weights.size()),
					.blendIndices_values = g_blend_indices.data(),
					.blendIndices_count = static_cast<std::uint32_t>(g_blend_indices.size()),
				},
				.material = material->handle,
			};

			// existing USD replacements are keyed by the geometry hash of the D3D9 draw
			asset_hash_parts_s parts;
			std::uint64_t replacement_hash = 0u;

			if (g_send_d3d9_hashes) {
				replacement_hash = d3d9_asset_hash(bytes, ctx.info.draw_base_vertex, ctx.info.draw_start_index, prim_count, parts);
			} else {
				parts.failure = "D3D9 geometry hashes disabled";
			}

			if (g_debug_log_hashes)
			{
				const auto inputs = std::format("index count {} vertex count {} base vertex {} start index {} min index {} stride {} position type {} offset {} from {} colormap {}",
					parts.index_count, parts.vertex_count, ctx.info.draw_base_vertex, ctx.info.draw_start_index, parts.min_index, bytes.stride,
					static_cast<unsigned>(parts.position_type), parts.position_offset, bytes.locked_vb ? "bound buffers" : "engine copy", material->colormap_name);

				if (replacement_hash)
				{
					shared::common::log("RendererApi", std::format("mesh 0x{:x} key 0x{:x} positions 0x{:x} indices 0x{:x} descriptor 0x{:x} {}",
						draw_hash, replacement_hash, parts.positions, parts.indices, parts.descriptor, inputs));
				}
				else {
					shared::common::log("RendererApi", std::format("mesh 0x{:x} no key: {} {}", draw_hash, parts.failure ? parts.failure : "", inputs), shared::common::LOG_TYPE::LOG_TYPE_WARN);
				}
			}

			remixapi_MeshInfoReplacementHashEXT replacement_ext =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO_REPLACEMENT_HASH_EXT,
				.pNext = nullptr,
				.replacementHash = replacement_hash,
			};

			const remixapi_MeshInfo info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
				.pNext = replacement_hash ? &replacement_ext : nullptr,
				.hash = draw_hash,
				.surfaces_values = &triangles,
				.surfaces_count = 1u,
			};

			remixapi_MeshHandle mesh = nullptr;
			if (shared::common::remix_api::get().m_bridge.CreateMesh(&info, &mesh) != REMIXAPI_ERROR_CODE_SUCCESS || !mesh)
			{
				rec.reason = reject_reason::mesh_failed;
				return false;
			}

			mesh_record mrec = {};
			mrec.mesh = mesh;
			mrec.material_key = material_key;
			mrec.vertex_colors_dropped = drop_vertex_colors;
			mrec.transient = pose && pose->flat_tyre;
			mrec.uv_fallback = uv_fallback;
			mrec.bone_count = bone_count;
			mrec.replacement_hash = replacement_hash;
			mrec.vertex_count = static_cast<std::uint32_t>(g_vertices.size());
			mrec.index_count = static_cast<std::uint32_t>(g_indices.size());
			mrec.bytes = mrec.vertex_count * sizeof(remixapi_HardcodedVertex) + mrec.index_count * sizeof(std::uint32_t);
			mrec.bytes += static_cast<std::uint32_t>(g_blend_weights.size() * sizeof(float) + g_blend_indices.size() * sizeof(std::uint32_t));
			g_cache_bytes += mrec.bytes;
			mesh_it = g_meshes.emplace(draw_hash, mrec).first;
			g_creates_this_frame++;
		}

		mesh_it->second.last_used_frame = g_frame;
		rec.owned = true;
		rec.mesh_hash = hash;
		rec.vertex_hash = double_buffered ? hash_words(bytes.vertices, static_cast<std::size_t>(bytes.vertex_count) * bytes.stride) : 0u;
		rec.texture = color_texture;
		rec.last_frame = g_frame;
		rec.overrides = make_overrides(ctx);
		rec.tfactor = state.tfactor;
		submit_record_locked(rec, transform, bones, draw_hash);

		if (count_uv_fallback && mesh_it->second.uv_fallback) {
			count_vehicle_stat(vehicle_stat::overlay_uv_fallbacks);
		}

		return true;
	}

	void renderer_api::submit_camera() {
		submit_camera_once();
	}

	remixapi_InstanceInfoBlendEXT renderer_api::capture_blend(IDirect3DDevice9* dev, bool* out_double_sided)
	{
		const auto state = capture_draw_state(dev);
		if (out_double_sided) {
			*out_double_sided = state.double_sided;
		}
		return make_blend(state);
	}

	remixapi_MeshHandle renderer_api::create_mesh_from_bound_draw(IDirect3DDevice9* dev, const INT base_vertex, const UINT min_index, const UINT num_vertices, const UINT start_index,
		const UINT prim_count, const std::uint64_t hash, const remixapi_MaterialHandle material, reject_reason* out_reason, float* out_bounds)
	{
		const auto fail = [out_reason](const reject_reason r) { if (out_reason) { *out_reason = r; } return static_cast<remixapi_MeshHandle>(nullptr); };

		IDirect3DVertexDeclaration9* decl = nullptr;
		if (FAILED(dev->GetVertexDeclaration(&decl)) || !decl) {
			return fail(reject_reason::vertex_format);
		}

		D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH + 1] = {};
		UINT element_count = 0u;
		const auto decl_ok = SUCCEEDED(decl->GetDeclaration(elements, &element_count)) && element_count;
		decl->Release();
		if (!decl_ok) {
			return fail(reject_reason::vertex_format);
		}

		IDirect3DIndexBuffer9* ib = nullptr;
		D3DINDEXBUFFER_DESC ib_desc = {};
		if (FAILED(dev->GetIndices(&ib)) || !ib) {
			return fail(reject_reason::no_cpu_data);
		}

		if (FAILED(ib->GetDesc(&ib_desc)) || ib_desc.Format != D3DFMT_INDEX16)
		{
			ib->Release();
			return fail(reject_reason::vertex_format);
		}

		geometry_bytes_s bytes;
		bytes.elements = elements;
		bytes.vertex_count = min_index + num_vertices;
		bytes.index_count = prim_count * 3u;

		IDirect3DVertexBuffer9* vb = nullptr;
		UINT offset = 0u, stride = 0u;
		void* vdata = nullptr;
		if (FAILED(dev->GetStreamSource(0, &vb, &offset, &stride)) || !vb || !stride)
		{
			ib->Release();
			return fail(reject_reason::no_cpu_data);
		}

		if (FAILED(vb->Lock(0, 0, &vdata, D3DLOCK_READONLY)) || !vdata)
		{
			vb->Release();
			ib->Release();
			return fail(reject_reason::no_cpu_data);
		}

		bytes.locked_vb = vb;
		bytes.stride = stride;
		bytes.vertices = static_cast<const std::uint8_t*>(vdata) + offset + static_cast<std::size_t>(base_vertex) * stride;

		void* idata = nullptr;
		if (FAILED(ib->Lock(0, 0, &idata, D3DLOCK_READONLY)) || !idata)
		{
			ib->Release();
			return fail(reject_reason::no_cpu_data);
		}

		bytes.locked_ib = ib;
		bytes.indices = static_cast<const std::uint16_t*>(idata) + start_index;

		std::unique_lock lock(g_mutex);
		std::uint32_t bone_count = 0u;
		bool uv_fallback = false;
		const auto reason = build_vertices(bytes, prim_count, false, false, false, 0u, false, nullptr, 0.0f, nullptr, bone_count, uv_fallback);
		if (reason != reject_reason::none) {
			return fail(reason);
		}

		if (out_bounds)
		{
			out_bounds[0] = out_bounds[1] = out_bounds[2] = FLT_MAX;
			out_bounds[3] = out_bounds[4] = out_bounds[5] = -FLT_MAX;
			for (const auto& v : g_vertices)
			{
				for (auto i = 0; i < 3; i++)
				{
					out_bounds[i] = std::min(out_bounds[i], v.position[i]);
					out_bounds[3 + i] = std::max(out_bounds[3 + i], v.position[i]);
				}
			}
		}

		const remixapi_MeshInfoSurfaceTriangles triangles =
		{
			.vertices_values = g_vertices.data(),
			.vertices_count = static_cast<std::uint32_t>(g_vertices.size()),
			.indices_values = g_indices.data(),
			.indices_count = static_cast<std::uint32_t>(g_indices.size()),
			.skinning_hasvalue = FALSE,
			.skinning_value = {},
			.material = material,
		};

		const remixapi_MeshInfo info =
		{
			.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
			.pNext = nullptr,
			.hash = hash ? hash : 1u,
			.surfaces_values = &triangles,
			.surfaces_count = 1u,
		};

		remixapi_MeshHandle mesh = nullptr;
		if (shared::common::remix_api::get().m_bridge.CreateMesh(&info, &mesh) != REMIXAPI_ERROR_CODE_SUCCESS || !mesh) {
			return fail(reject_reason::mesh_failed);
		}

		if (out_reason) {
			*out_reason = reject_reason::none;
		}
		return mesh;
	}

	void renderer_api::note_static_draw()
	{
		if (g_in_draw_entity_dc) {
			g_static_entity_dc_this_frame++;
		} else if (g_in_draw_frag_type_dc) {
			g_static_frag_type_dc_this_frame++;
		} else if (g_in_draw_frag_dc_object) {
			g_static_frag_object_this_frame++;
		} else if (g_in_draw_entity_info) {
			g_static_entity_info_this_frame++;
		} else if (g_in_draw_dynamic_entity_dc) {
			g_static_dynamic_entity_this_frame++;
		} else if (g_in_draw_lights_in_area) {
			g_static_lights_in_area_this_frame++;
		} else if (g_in_draw_ped_dc) {
			g_static_ped_this_frame++;
		} else {
			g_static_other_this_frame++;
		}
	}

	bool renderer_api::debug_hide_d3d9_static()
	{
		return is_enabled() && g_debug_hide_d3d9_static;
	}

	void renderer_api::on_texture_created(IDirect3DTexture9* texture)
	{
		static bool hooked = false;
		if (hooked || !texture) {
			return;
		}
		hooked = true;

		// every texture of the bridge client shares one vtable: LockRect slot 19, UnlockRect slot 20
		const auto vtbl = *reinterpret_cast<void** const*>(texture);
		shared::utils::hook::detour(reinterpret_cast<DWORD>(vtbl[19]), &IDirect3DTexture9_LockRect_hk, DETOUR_CAST(IDirect3DTexture9_LockRect_og));
		shared::utils::hook::detour(reinterpret_cast<DWORD>(vtbl[20]), &IDirect3DTexture9_UnlockRect_hk, DETOUR_CAST(IDirect3DTexture9_UnlockRect_og));
	}

	void renderer_api::on_texture_written(IDirect3DBaseTexture9* texture)
	{
		if (!texture) {
			return;
		}
		std::lock_guard guard(g_written_textures_mutex);
		g_written_textures.insert(texture);
	}

	void renderer_api::on_begin_scene()
	{
		g_frame++;
		process_texture_writes();
		g_creates_this_frame = 0u;
		g_cache_hits_this_frame = 0u;
		g_instances_this_frame = 0u;
		g_batch_groups.clear();
		g_entity_skips_this_frame = 0u;
		g_entity_fallbacks_this_frame = 0u;
		g_budget_exhausted_this_frame = false;
		g_modifier_skips_this_frame = 0u;
		g_static_entity_dc_this_frame = 0u;
		g_static_frag_type_dc_this_frame = 0u;
		g_static_frag_object_this_frame = 0u;
		g_static_entity_info_this_frame = 0u;
		g_static_dynamic_entity_this_frame = 0u;
		g_static_lights_in_area_this_frame = 0u;
		g_static_ped_this_frame = 0u;
		g_static_other_this_frame = 0u;
		g_skips_no_window_this_frame = 0u;
		g_skips_vehicle_this_frame = 0u;
		g_skips_ped_this_frame = 0u;
		g_skips_instanced_this_frame = 0u;
		g_skips_target_this_frame = 0u;
		g_skips_dual_render_this_frame = 0u;
		g_skips_no_geometry_this_frame = 0u;
		g_pass_skips_this_frame = 0u;
		g_pass_full_draws_this_frame = 0u;
		std::fill(std::begin(g_pass_stats_this_frame), std::end(g_pass_stats_this_frame), 0u);
		g_ped_instances_this_frame = 0u;
		g_ped_pass_skips_this_frame = 0u;
		g_ped_skips_context_this_frame = 0u;
		g_ped_skips_target_this_frame = 0u;
		g_ped_skips_stream_this_frame = 0u;
		g_ped_skips_blendshape_this_frame = 0u;
		g_ped_skips_rejected_this_frame = 0u;
		std::fill(std::begin(g_vehicle_stats_this_frame), std::end(g_vehicle_stats_this_frame), 0u);
	}

	void renderer_api::on_end_scene()
	{
		{
			const auto fingerprint = overrides_fingerprint();
			if (fingerprint != g_overrides_fingerprint)
			{
				std::unique_lock lock(g_mutex);
				g_overrides_fingerprint = fingerprint;
				g_overrides_generation++;
				g_overrides_generation_game_frame = g_game_frame;
			}
		}

		g_creates_last_frame = g_creates_this_frame;
		g_instances_last_frame = g_instances_this_frame;
		g_batch_groups_last_frame = static_cast<std::uint32_t>(g_batch_groups.size());
		g_entity_skips_last_frame = g_entity_skips_this_frame;
		g_entity_fallbacks_last_frame = g_entity_fallbacks_this_frame;
		g_modifier_skips_last_frame = g_modifier_skips_this_frame;
		g_static_entity_dc_last_frame = g_static_entity_dc_this_frame;
		g_static_frag_type_dc_last_frame = g_static_frag_type_dc_this_frame;
		g_static_frag_object_last_frame = g_static_frag_object_this_frame;
		g_static_entity_info_last_frame = g_static_entity_info_this_frame;
		g_static_dynamic_entity_last_frame = g_static_dynamic_entity_this_frame;
		g_static_lights_in_area_last_frame = g_static_lights_in_area_this_frame;
		g_static_ped_last_frame = g_static_ped_this_frame;
		g_static_other_last_frame = g_static_other_this_frame;
		g_skips_no_window_last_frame = g_skips_no_window_this_frame;
		g_skips_vehicle_last_frame = g_skips_vehicle_this_frame;
		g_skips_ped_last_frame = g_skips_ped_this_frame;
		g_skips_instanced_last_frame = g_skips_instanced_this_frame;
		g_skips_target_last_frame = g_skips_target_this_frame;
		g_skips_dual_render_last_frame = g_skips_dual_render_this_frame;
		g_skips_no_geometry_last_frame = g_skips_no_geometry_this_frame;
		g_pass_skips_last_frame = g_pass_skips_this_frame;
		g_pass_full_draws_last_frame = g_pass_full_draws_this_frame;
		std::copy(std::begin(g_pass_stats_this_frame), std::end(g_pass_stats_this_frame), std::begin(g_pass_stats_last_frame));
		g_ped_instances_last_frame = g_ped_instances_this_frame;
		g_ped_pass_skips_last_frame = g_ped_pass_skips_this_frame;
		g_ped_skips_context_last_frame = g_ped_skips_context_this_frame;
		g_ped_skips_target_last_frame = g_ped_skips_target_this_frame;
		g_ped_skips_stream_last_frame = g_ped_skips_stream_this_frame;
		g_ped_skips_blendshape_last_frame = g_ped_skips_blendshape_this_frame;
		std::copy(std::begin(g_vehicle_stats_this_frame), std::end(g_vehicle_stats_this_frame), std::begin(g_vehicle_stats_last_frame));
		g_ped_skips_rejected_last_frame = g_ped_skips_rejected_this_frame;
		
		if (g_budget_exhausted_this_frame) {
			g_budget_exhausted_frames++;
		}

		{
			std::unique_lock lock(g_mutex);
			std::vector<std::uint64_t> unused;

			for (const auto& [hash, mesh] : g_meshes)
			{
				if (mesh.transient && g_frame - mesh.last_used_frame > 1u) {
					unused.push_back(hash);
				}
			}

			for (const auto hash : unused) {
				destroy_mesh_locked(hash);
			}

			if ((g_frame & 0x3Fu) == 0u) {
				enforce_cache_budget_locked();
			}
		}
	}

	void renderer_api::destroy_all()
	{
		std::unique_lock lock(g_mutex);
		auto& bridge = shared::common::remix_api::get().m_bridge;

		for (auto& [hash, mesh] : g_meshes)
		{
			if (mesh.mesh) {
				bridge.DestroyMesh(mesh.mesh);
			}
		}

		for (auto& [key, mat] : g_materials)
		{
			if (mat.handle) {
				bridge.DestroyMaterial(mat.handle);
			}
		}

		g_meshes.clear();
		g_materials.clear();
		g_materials_by_texture.clear();
		g_records.clear();
		g_pass_sets.clear();
		g_entity_frames.clear();
		g_cache_bytes = 0u;
	}

	renderer_api::stats_s renderer_api::get_stats()
	{
		stats_s s = {};
		std::shared_lock lock(g_mutex);

		for (const auto& [geom, records] : g_records)
		{
			for (const auto& rec : records)
			{
				s.records_total++;
				if (rec.owned)
				{
					s.records_owned++;
					s.records_hidden += rec.hidden;
				}
				else if (rec.reason != reject_reason::none)
				{
					s.records_rejected++;
					s.reject_counts[static_cast<std::size_t>(rec.reason)]++;
				}
			}
		}
		s.meshes_cached = static_cast<std::uint32_t>(g_meshes.size());
		for (const auto& [hash, mesh] : g_meshes)
		{
			if (mesh.replacement_hash) {
				s.meshes_with_replacement_hash++;
			}
		}
		s.materials = static_cast<std::uint32_t>(g_materials.size());
		s.texture_writes_last_frame = g_texture_writes_last_frame;
		s.texture_rebuilds_last_frame = g_texture_rebuilds_last_frame;
		s.volatile_textures = g_volatile_textures;
		s.cache_bytes = g_cache_bytes;
		s.creates_this_frame = g_creates_this_frame;
		s.cache_hits_this_frame = g_cache_hits_this_frame;
		s.instances_this_frame = g_instances_this_frame;
		s.entity_skips_this_frame = g_entity_skips_this_frame;
		s.entity_fallbacks_this_frame = g_entity_fallbacks_this_frame;
		s.creates_last_frame = g_creates_last_frame;
		s.instances_last_frame = g_instances_last_frame;
		s.batch_groups_last_frame = g_batch_groups_last_frame;
		s.entity_skips_last_frame = g_entity_skips_last_frame;
		s.entity_fallbacks_last_frame = g_entity_fallbacks_last_frame;
		s.budget_exhausted_frames = g_budget_exhausted_frames;
		s.modifier_skips_last_frame = g_modifier_skips_last_frame;
		s.static_draws_entity_dc_last_frame = g_static_entity_dc_last_frame;
		s.static_draws_frag_type_dc_last_frame = g_static_frag_type_dc_last_frame;
		s.static_draws_frag_object_last_frame = g_static_frag_object_last_frame;
		s.static_draws_entity_info_last_frame = g_static_entity_info_last_frame;
		s.static_draws_dynamic_entity_last_frame = g_static_dynamic_entity_last_frame;
		s.static_draws_lights_in_area_last_frame = g_static_lights_in_area_last_frame;
		s.static_draws_ped_last_frame = g_static_ped_last_frame;
		s.static_draws_other_last_frame = g_static_other_last_frame;
		s.skips_no_window_last_frame = g_skips_no_window_last_frame;
		s.skips_vehicle_last_frame = g_skips_vehicle_last_frame;
		s.skips_ped_last_frame = g_skips_ped_last_frame;
		s.skips_instanced_last_frame = g_skips_instanced_last_frame;
		s.skips_target_last_frame = g_skips_target_last_frame;
		s.skips_dual_render_last_frame = g_skips_dual_render_last_frame;
		s.skips_no_geometry_last_frame = g_skips_no_geometry_last_frame;
		s.pass_skips_last_frame = g_pass_skips_last_frame;
		s.pass_full_draws_last_frame = g_pass_full_draws_last_frame;

		const auto stat = [](const pass_stat st) {
			return g_pass_stats_last_frame[static_cast<std::size_t>(st)];
		};

		s.pass_full_no_set_last_frame = stat(pass_stat::full_no_set);
		s.pass_full_refresh_last_frame = stat(pass_stat::full_refresh);
		s.pass_full_stale_last_frame = stat(pass_stat::full_stale);
		s.pass_full_too_many_passes_last_frame = stat(pass_stat::full_too_many_passes);
		s.pass_capture_complete_last_frame = stat(pass_stat::capture_complete);
		s.pass_capture_missing_last_frame = stat(pass_stat::capture_missing);
		s.pass_capture_rejected_last_frame = stat(pass_stat::capture_rejected);
		s.pass_capture_rejected_ped_last_frame = stat(pass_stat::capture_rejected_ped);
		s.ped_instances_last_frame = g_ped_instances_last_frame;
		s.ped_pass_skips_last_frame = g_ped_pass_skips_last_frame;
		s.ped_skips_context_last_frame = g_ped_skips_context_last_frame;
		s.ped_skips_target_last_frame = g_ped_skips_target_last_frame;
		s.ped_skips_stream_last_frame = g_ped_skips_stream_last_frame;
		s.ped_skips_blendshape_last_frame = g_ped_skips_blendshape_last_frame;

		const auto vstat = [](const vehicle_stat st) {
			return g_vehicle_stats_last_frame[static_cast<std::size_t>(st)];
		};

		s.vehicle_instances_last_frame = vstat(vehicle_stat::instances);
		s.vehicle_overlays_last_frame = vstat(vehicle_stat::overlays);
		s.vehicle_overlay_uv_fallbacks_last_frame = vstat(vehicle_stat::overlay_uv_fallbacks);
		s.vehicle_damaged_last_frame = vstat(vehicle_stat::damaged);
		s.vehicle_tyres_last_frame = vstat(vehicle_stat::tyres);
		s.vehicle_skips_context_last_frame = vstat(vehicle_stat::skips_context);
		s.vehicle_flat_tyres_last_frame = vstat(vehicle_stat::flat_tyres);
		s.vehicle_skips_overlay_last_frame = vstat(vehicle_stat::skips_overlay);
		s.vehicle_skips_target_last_frame = vstat(vehicle_stat::skips_target);
		s.vehicle_skips_rejected_last_frame = vstat(vehicle_stat::skips_rejected);
		s.ped_skips_rejected_last_frame = g_ped_skips_rejected_last_frame;

		for (const auto& [geom, sets] : g_pass_sets) {
			s.pass_sets += static_cast<std::uint32_t>(sets.size());
		}

		return s;
	}

	void renderer_api::devgui()
	{
		static const auto gs = comp_settings::get();

		if (!is_runtime_supported()) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Remix API not available - api path unavailable");
		}

		ImGui::SeparatorText("Settings");
		ImGui::TextDisabled("Draw through the Remix API: the mod builds api meshes from the geometry and submits instances, the engine's D3D9 draw is dropped.");
		ImGui::TextDisabled("Skip the engine's pass setup: the engine's render state / shader constant / texture calls of such geometry are dropped as well.");
		ImGui::TextDisabled("Skip the engine's entity draw: the engine does not even walk the entity's drawable (phase B, static entities only).");

		ImGui::SeparatorText("Static geometry");
		ImGui::Checkbox("Draw through the Remix API##static", gs->api_static_enabled.get_as<bool*>());
		ImGui::Checkbox("Skip the engine's pass setup of api drawn geometry##static", gs->api_static_skip_pass_setup.get_as<bool*>());
		ImGui::SliderInt("Pass setup refresh interval (frames, 0 = never)##static", gs->api_static_pass_refresh_frames.get_as<int*>(), 0, 600);
		ImGui::Checkbox("Skip the engine's entity draw of fully api drawn entities##static", gs->api_static_entity_skip.get_as<bool*>());
		ImGui::SliderInt("Entity draw refresh interval (frames, 0 = never)##static", gs->api_static_entity_refresh_frames.get_as<int*>(), 0, 1024);

		ImGui::SeparatorText("Peds");
		ImGui::Checkbox("Draw through the Remix API (skinned, no vertex capture)##peds", gs->api_peds_enabled.get_as<bool*>());
		ImGui::Checkbox("Blendshape heads too (mesh rebuilt per morph)##peds", gs->api_peds_blendshapes.get_as<bool*>());
		ImGui::Checkbox("Skip the engine's pass setup of api drawn peds##peds", gs->api_peds_skip_pass_setup.get_as<bool*>());

		ImGui::SeparatorText("Vehicles");
		ImGui::Checkbox("Draw through the Remix API (bones per draw, damage on the CPU)##vehicles", gs->api_vehicles_enabled.get_as<bool*>());
		ImGui::Checkbox("Debug: tyres stay on D3D9##vehicles", &g_debug_tyres_on_d3d9);

		ImGui::Spacing(0, 4);
		if (ImGui::TreeNode("Water material##api_water"))
		{
			ImGui::TextDisabled("Used while water is drawn through the Remix API and no USD water material replaces it. Not saved.");

			ImGui::BeginDisabled(gs->override_water_texture_hash.get_temp_override_state());
			ImGui::Checkbox("Stabilize Water Texture Hash (USD water material)##api_water", gs->override_water_texture_hash.get_as<bool*>());
			ImGui::EndDisabled();

			auto& m = water::material();
			ImGui::DragFloat("Index of refraction##api_water", &m.ior, 0.001f, 1.0f, 3.0f, "%.3f");
			ImGui::ColorEdit3("Transmittance color##api_water", m.transmittance_color, ImGuiColorEditFlags_Float);
			ImGui::DragFloat("Transmittance distance##api_water", &m.transmittance_distance, 0.05f, 0.01f, 1000.0f, "%.2f");
			ImGui::TreePop();
		}

		ImGui::SeparatorText("Mesh cache");
		ImGui::SliderInt("Mesh creates per frame", gs->api_static_creates_per_frame.get_as<int*>(), 0, 256);
		ImGui::SliderInt("Mesh cache budget (MB)", gs->api_static_cache_mb.get_as<int*>(), 0, 8192);
		ImGui::Checkbox("Only plain draws (no render state modifiers / categories)", gs->api_static_require_plain_draw.get_as<bool*>());
		
		if (ImGui::Button("Destroy all api meshes and materials")) {
			destroy_all();
		}

		const auto s = get_stats();
		const auto& calls = d3d9ex::get_calls_last_frame();
		std::uint32_t total_calls = 0u;

		for (const auto c : calls) {
			total_calls += c;
		}

		ImGui::SeparatorText("Overview (last frame)");
		ImGui::Text("Instances %u, ped instances %u (pass setup skipped %u), vehicle instances %u, mesh creates %u, D3D9 calls through the device proxy %u",
			s.instances_last_frame, s.ped_instances_last_frame + s.ped_pass_skips_last_frame, s.ped_pass_skips_last_frame,
			s.vehicle_instances_last_frame + s.vehicle_tyres_last_frame + s.vehicle_flat_tyres_last_frame + s.vehicle_overlays_last_frame,
			s.creates_last_frame, total_calls);
		
		ImGui::Text("Geometry passes %u: owned %u (hidden by Ignore %u), rejected %u", s.records_total, s.records_owned, s.records_hidden, s.records_rejected);
		ImGui::Checkbox("Count identical rigid instances (mesh + draw state), estimate for GPU instancing", &g_count_batch_groups);
		
		if (g_count_batch_groups) {
			ImGui::Text("Rigid instances fall into %u groups of identical mesh and draw state", s.batch_groups_last_frame);
		}

		ImGui::Text("Meshes %u (%.1f MB), materials %u, frames with exhausted create budget %u",
			s.meshes_cached, static_cast<double>(s.cache_bytes) / (1024.0 * 1024.0), s.materials, s.budget_exhausted_frames);
		ImGui::Text("Meshes keyed for USD replacements by their D3D9 geometry hash %u of %u", s.meshes_with_replacement_hash, s.meshes_cached);
		ImGui::Text("Textures the game wrote last frame %u, of them api colormaps (materials and meshes rebuilt) %u, colormaps written every frame (kept on D3D9) %u",
			s.texture_writes_last_frame, s.texture_rebuilds_last_frame, s.volatile_textures);

		if (ImGui::TreeNode("Static geometry##api_static_stats"))
		{
			ImGui::Text("Draws by source: entity %u, fragment type %u, fragment object %u, sorted alpha %u", s.static_draws_entity_dc_last_frame,
				s.static_draws_frag_type_dc_last_frame, s.static_draws_frag_object_last_frame, s.static_draws_entity_info_last_frame);
			
			ImGui::Text("                 dynamic entity %u, area lights %u, rigid ped parts %u, other %u", s.static_draws_dynamic_entity_last_frame,
				s.static_draws_lights_in_area_last_frame, s.static_draws_ped_last_frame, s.static_draws_other_last_frame);
			
			ImGui::Text("Kept on D3D9: vehicle %u, ped %u, unknown draw command %u, instanced %u, colour write off / other target %u", s.skips_vehicle_last_frame,
				s.skips_ped_last_frame, s.skips_no_window_last_frame, s.skips_instanced_last_frame, s.skips_target_last_frame);
			
			ImGui::Text("              dual / tri render %u, no geometry %u, modifiers / categories %u", s.skips_dual_render_last_frame,
				s.skips_no_geometry_last_frame, s.modifier_skips_last_frame);
			
			ImGui::Text("Phase B: entity skips %u, entity fallbacks %u", s.entity_skips_last_frame, s.entity_fallbacks_last_frame);

			if (ImGui::TreeNode("Draws of unknown origin (preset / colormap)"))
			{
				for (const auto& u : g_unknown_draws) {
					ImGui::Text("%s / %s", u.preset.c_str(), u.colormap.c_str());
				}

				if (ImGui::Button("Clear##unknown_draws")) {
					g_unknown_draws.clear();
				}

				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Pass skip##api_static_stats"))
		{
			ImGui::Text("Skipped %u, drawn by the engine in api windows %u, skippable geometry passes %u",
				s.pass_skips_last_frame, s.pass_full_draws_last_frame, s.pass_sets);
			
			ImGui::Text("Engine draws: no pass set %u, refresh %u, stale set %u, more than %u passes %u",
				s.pass_full_no_set_last_frame, s.pass_full_refresh_last_frame, s.pass_full_stale_last_frame, MAX_SKIP_PASSES, s.pass_full_too_many_passes_last_frame);
			
			ImGui::Text("Captures: complete %u, passes missing %u, pass not owned / animated uvs %u (of these in ped draws %u)",
				s.pass_capture_complete_last_frame, s.pass_capture_missing_last_frame, s.pass_capture_rejected_last_frame, s.pass_capture_rejected_ped_last_frame);
			
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Peds##api_static_stats"))
		{
			ImGui::Text("Api: drawn through the engine's draw %u, engine pass setup skipped %u", s.ped_instances_last_frame, s.ped_pass_skips_last_frame);
			
			ImGui::Text("Kept on D3D9: mirror / phone / vehicle / instanced %u, other render target %u",
				s.ped_skips_context_last_frame, s.ped_skips_target_last_frame);
			
			ImGui::Text("                   second vertex stream %u, blendshape heads off %u, rejected %u",
				s.ped_skips_stream_last_frame, s.ped_skips_blendshape_last_frame, s.ped_skips_rejected_last_frame);

			if (ImGui::TreeNode("Draws kept on D3D9 (reason / preset / colormap)"))
			{
				for (const auto& k : g_kept_ped_draws) {
					ImGui::Text("%s / %s / %s", k.reason.c_str(), k.preset.c_str(), k.colormap.c_str());
				}

				if (ImGui::Button("Clear##kept_ped_draws")) {
					g_kept_ped_draws.clear();
				}
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Vehicles##api_static_stats"))
		{
			ImGui::Text("Api: base passes %u (damaged %u), tyres %u (flat %u), dirt / livery passes %u (UV0 for a missing set %u)",
				s.vehicle_instances_last_frame, s.vehicle_damaged_last_frame, s.vehicle_tyres_last_frame + s.vehicle_flat_tyres_last_frame,
				s.vehicle_flat_tyres_last_frame, s.vehicle_overlays_last_frame, s.vehicle_overlay_uv_fallbacks_last_frame);
			
			ImGui::Text("Kept on D3D9: mirror / phone / instanced %u, light proxies %u, other render target %u, rejected %u",
				s.vehicle_skips_context_last_frame, s.vehicle_skips_overlay_last_frame, s.vehicle_skips_target_last_frame, s.vehicle_skips_rejected_last_frame);
			
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("D3D9 calls through the device proxy##api_static_stats"))
		{
			ImGui::TextDisabled("draws are counted before the mod drops or replaces them");
			
			for (auto i = 0u; i < calls.size(); i++) {
				ImGui::Text("%s: %u", d3d9ex::call_type_name(static_cast<d3d9ex::call_type>(i)), calls[i]);
			}

			ImGui::TreePop();
		}

		if (s.records_rejected && ImGui::TreeNode("Reject reasons##api_static_stats"))
		{
			for (auto i = 1u; i < static_cast<std::uint32_t>(reject_reason::count); i++)
			{
				if (s.reject_counts[i]) {
					ImGui::Text("%s: %u", reject_reason_name(static_cast<reject_reason>(i)), s.reject_counts[i]);
				}
			}
			ImGui::TreePop();
		}

		ImGui::SeparatorText("Debug");
		ImGui::Checkbox("Hide api drawn geometry (static and peds)", &g_debug_hide_api);
		ImGui::Checkbox("Hide static geometry not drawn by the api", &g_debug_hide_d3d9_static);
		ImGui::Checkbox("Hide ped draws not drawn by the api", &g_debug_hide_d3d9_peds);

		// rebuilds every api mesh so all of them are logged
		if (ImGui::Checkbox("Log geometry hashes (rtx_comp/logfile.txt and the runtime log)", &g_debug_log_hashes))
		{
			if (const auto& bridge = shared::common::remix_api::get().m_bridge; bridge.SetConfigVariable) {
				bridge.SetConfigVariable("rtx.debugLogGeometryHashes", g_debug_log_hashes ? "True" : "False");
			}

			if (g_debug_log_hashes) {
				destroy_all();
			}
		}

		// meshes are rebuilt with / without the key
		if (ImGui::Checkbox("Send D3D9 geometry hashes (USD replacements of api meshes)", &g_send_d3d9_hashes)) {
			destroy_all();
		}
	}

	renderer_api::renderer_api()
	{
		p_this = this;

		shared::utils::hook::detour(game::func_addr__grmGeometryQB_Draw, &grmGeometryQB_Draw_hk, DETOUR_CAST(grmGeometryQB_Draw_og));
		shared::utils::hook::detour(game::func_addr__grmGeometryQB_dtor, &grmGeometryQB_dtor_hk, DETOUR_CAST(grmGeometryQB_dtor_og));
		shared::utils::hook::detour(game::func_addr__grcTexturePC_dtor, &grcTexturePC_dtor_hk, DETOUR_CAST(grcTexturePC_dtor_og));
		shared::utils::hook::detour(game::func_addr__CEntity_AddToDrawListBuckets, &CEntity_AddToDrawListBuckets_hk, DETOUR_CAST(CEntity_AddToDrawListBuckets_og));
		shared::utils::hook::detour(game::func_addr__CRenderPhase_BuildDrawListWrapper, &CRenderPhase_BuildDrawListWrapper_hk, DETOUR_CAST(CRenderPhase_BuildDrawListWrapper_og));
		shared::utils::hook::detour(game::func_addr__CEntityDrawInfo_RenderBuckets, &CEntityDrawInfo_RenderBuckets_hk, DETOUR_CAST(CEntityDrawInfo_RenderBuckets_og));
		shared::utils::hook::detour(game::func_addr__CDrawSkinnedEntityDC_Execute, &CDrawSkinnedEntityDC_Execute_hk, DETOUR_CAST(CDrawSkinnedEntityDC_Execute_og));
		shared::utils::hook::detour(game::func_addr__CRenderer_RenderDrawableGeometriesWithLightsInArea, &CRenderer_RenderDrawableGeometriesWithLightsInArea_hk, DETOUR_CAST(CRenderer_RenderDrawableGeometriesWithLightsInArea_og));

		shared::utils::hook::detour(game::func_addr__CDrawPedDC_Execute, &CDrawPedDC_Execute_hk, DETOUR_CAST(CDrawPedDC_Execute_og));
		shared::utils::hook::detour(game::func_addr__CDrawPlayerDC_Execute, &CDrawPlayerDC_Execute_hk, DETOUR_CAST(CDrawPlayerDC_Execute_og));
		shared::utils::hook::detour(game::func_addr__CDrawPedPropDC_Execute, &CDrawPedPropDC_Execute_hk, DETOUR_CAST(CDrawPedPropDC_Execute_og));
		shared::utils::hook::detour(game::func_addr__CDrawPedPropsDC_Execute, &CDrawPedPropsDC_Execute_hk, DETOUR_CAST(CDrawPedPropsDC_Execute_og));
		shared::utils::hook::detour(game::func_addr__CDrawSLODPedDC_Execute, &CDrawSLODPedDC_Execute_hk, DETOUR_CAST(CDrawSLODPedDC_Execute_og));

		if (game::func_addr__grmShader_DrawGeometry && game::pForcedTechnique && game::pTechniqueDefault && game::pTechniqueAlt && game::pGrmRenderState1) {
			shared::utils::hook::detour(game::func_addr__grmShader_DrawGeometry, &grmShader_DrawGeometry_hk, DETOUR_CAST(grmShader_DrawGeometry_og));
			shared::utils::hook::detour(game::func_addr__grmShader_DrawSkinned, &grmShader_DrawSkinned_hk, DETOUR_CAST(grmShader_DrawSkinned_og));
		}

		natives::on_game_frame_callback = on_game_frame;

		shared::common::log("RendererApi", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}

	renderer_api::~renderer_api()
	{
		p_this = nullptr;
	}
}
