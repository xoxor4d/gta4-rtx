#pragma once
#include "shared/common/remix_api.hpp"

namespace gta4
{
	class drawcall_mod_context;

	namespace game
	{
		struct grmGeometryQB;
		struct grcTextureReference;
	}

	/// Renders static map geometry (everything the engine draws through CDrawEntityDC) via the Remix API.
	/// Phase A: the fixed function draw of a geometry is captured into an api mesh + material and replaced by DrawInstance.
	/// Phase B: once every geometry of a drawable is owned, the engine's entity draw command is replaced by our own
	///          command that only submits instances, so the entity never touches D3D9.
	/// Pass skip: a geometry whose shader passes are all owned skips the engine's pass setup (grmShader::DrawGeometry)
	///            and only submits its instances.
	/// Peds: their vertex shader draws are replaced the same way, skinned geometry as skinned api meshes that get the
	///       bone palette of each draw (no vertex capture).
	class renderer_api final : public shared::common::loader::component_module
	{
	public:
		renderer_api();
		~renderer_api();

		static inline renderer_api* p_this = nullptr;
		static renderer_api* get() { return p_this; }
		static bool is_initialized() { return p_this != nullptr; }

		enum class reject_reason : std::uint8_t
		{
			none = 0,
			no_geometry,
			no_cpu_data,
			primitive_type,
			vertex_format,
			index_range,
			empty,
			material_failed,
			mesh_failed,
			bone_range,
			volatile_texture,
			count
		};

		static constexpr std::uint32_t MAX_BONES = 48u; // gBoneMtx[48] at c64 of the skinned ped / vehicle vertex shaders

		/// placement of an api instance: skinned draws carry their bone palette, the transform then only translates
		struct draw_pose_s
		{
			remixapi_Transform transform = {};
			remixapi_Transform bones[MAX_BONES] = {};
			bool skinned = false;
			bool vehicle = false;  // records keyed by texture too (liveries), overlay passes allowed
			bool overlay = false;  // dirt / livery pass of a vehicle draw

			IDirect3DTexture9* damage_texture = nullptr; // deformation applied in the vertex shader (switchOn)
			std::uint64_t damage_hash = 0u;
			float bound_radius = 0.0f;

			bool flat_tyre = false;       // tyreDeformSwitchOn: flattened against the ground, a new shape every frame
			float tyre_constants[6][4] = {}; // matWheelTransform rows, tyreDeformParams, tyreDeformParams2
			std::uint64_t flat_hash = 0u;
		};

		static const char* reject_reason_name(reject_reason r);

		struct stats_s
		{
			std::uint32_t records_total = 0u;
			std::uint32_t records_owned = 0u;
			std::uint32_t records_hidden = 0u;
			std::uint32_t records_rejected = 0u;
			std::uint32_t reject_counts[static_cast<std::size_t>(reject_reason::count)] = {};
			std::uint32_t meshes_cached = 0u;
			std::uint32_t meshes_with_replacement_hash = 0u;
			std::uint32_t materials = 0u;
			std::uint32_t texture_writes_last_frame = 0u;   // textures the game wrote (LockRect / UpdateTexture)
			std::uint32_t texture_rebuilds_last_frame = 0u; // of them colormaps of api materials, rebuilt with their meshes
			std::uint32_t volatile_textures = 0u;           // colormaps written every frame, their draws stay on D3D9
			std::uint64_t cache_bytes = 0u;
			std::uint32_t creates_this_frame = 0u;
			std::uint32_t cache_hits_this_frame = 0u;
			std::uint32_t instances_this_frame = 0u;
			std::uint32_t entity_skips_this_frame = 0u;
			std::uint32_t entity_fallbacks_this_frame = 0u;

			std::uint32_t creates_last_frame = 0u;
			std::uint32_t instances_last_frame = 0u;
			std::uint32_t batch_groups_last_frame = 0u; // distinct (mesh, draw overrides) among the rigid instances, when counted
			std::uint32_t entity_skips_last_frame = 0u;
			std::uint32_t entity_fallbacks_last_frame = 0u;
			std::uint32_t budget_exhausted_frames = 0u;
			std::uint32_t modifier_skips_last_frame = 0u;

			// static draws by the draw command they came from (last frame)
			std::uint32_t static_draws_entity_dc_last_frame = 0u;
			std::uint32_t static_draws_frag_type_dc_last_frame = 0u;
			std::uint32_t static_draws_frag_object_last_frame = 0u;
			std::uint32_t static_draws_entity_info_last_frame = 0u;
			std::uint32_t static_draws_dynamic_entity_last_frame = 0u;
			std::uint32_t static_draws_lights_in_area_last_frame = 0u;
			std::uint32_t static_draws_ped_last_frame = 0u;
			std::uint32_t static_draws_other_last_frame = 0u;

			// static draws that stayed on D3D9 because the api path could not take them (last frame)
			std::uint32_t skips_no_window_last_frame = 0u; // static draws of unknown origin (not vehicle / ped)
			std::uint32_t skips_vehicle_last_frame = 0u;
			std::uint32_t skips_ped_last_frame = 0u;
			std::uint32_t skips_instanced_last_frame = 0u;
			std::uint32_t skips_target_last_frame = 0u; // colour writes off (depth-only occluders) or not the backbuffer sized target
			std::uint32_t skips_dual_render_last_frame = 0u;
			std::uint32_t skips_no_geometry_last_frame = 0u;

			std::uint32_t pass_skips_last_frame = 0u;      // geometries whose engine pass setup was skipped
			std::uint32_t pass_full_draws_last_frame = 0u; // geometries in an api draw window the engine drew
			std::uint32_t pass_sets = 0u;                  // geometry / technique pairs with skippable passes

			// why geometries in an api draw window were drawn by the engine (last frame)
			std::uint32_t pass_full_no_set_last_frame = 0u;
			std::uint32_t pass_full_refresh_last_frame = 0u;
			std::uint32_t pass_full_stale_last_frame = 0u;
			std::uint32_t pass_full_too_many_passes_last_frame = 0u;

			// what the captures of these engine draws found (last frame)
			std::uint32_t pass_capture_complete_last_frame = 0u;
			std::uint32_t pass_capture_missing_last_frame = 0u;  // fewer owned draws than passes (pass not drawn / not reaching the api path)
			std::uint32_t pass_capture_rejected_last_frame = 0u; // a pass was not owned or has animated uvs
			std::uint32_t pass_capture_rejected_ped_last_frame = 0u; // of these, captures in the ped draw window

			// ped draws (last frame)
			std::uint32_t ped_instances_last_frame = 0u;   // drawn through the engine's draw (captured or re-submitted)
			std::uint32_t ped_pass_skips_last_frame = 0u;  // ped geometries whose engine pass setup and draw were skipped
			std::uint32_t ped_skips_context_last_frame = 0u; // mirror, phone, vehicle or instanced rendering
			std::uint32_t ped_skips_target_last_frame = 0u;  // not drawn to the backbuffer sized target (the runtime rasterizes these)
			std::uint32_t ped_skips_stream_last_frame = 0u;  // geometry with a second vertex stream
			std::uint32_t ped_skips_blendshape_last_frame = 0u; // blendshape heads while 'api_peds_blendshapes' is off
			std::uint32_t ped_skips_rejected_last_frame = 0u;

			// vehicle draws (last frame)
			std::uint32_t vehicle_instances_last_frame = 0u;
			std::uint32_t vehicle_overlays_last_frame = 0u;  // dirt / livery passes
			std::uint32_t vehicle_overlay_uv_fallbacks_last_frame = 0u; // of these, drawn with UV0 because their texcoord set was not usable
			std::uint32_t vehicle_damaged_last_frame = 0u;   // base passes with a damage mesh
			std::uint32_t vehicle_tyres_last_frame = 0u;
			std::uint32_t vehicle_flat_tyres_last_frame = 0u;     // deformed on the CPU, a mesh per frame

			std::uint32_t vehicle_skips_context_last_frame = 0u;  // mirror, phone, instanced (not tyres)
			std::uint32_t vehicle_skips_overlay_last_frame = 0u;  // overlay modes the api path does not mirror (lights)
			std::uint32_t vehicle_skips_target_last_frame = 0u;
			std::uint32_t vehicle_skips_rejected_last_frame = 0u;
		};

		static bool is_runtime_supported();
		static bool is_enabled();
		static bool is_entity_skip_enabled();
		static bool is_pass_skip_enabled();

		/// Called right before the fixed function draw of a static geometry.
		/// @return true if the draw was submitted through the api (the D3D9 draw must be skipped)
		static bool try_draw(IDirect3DDevice9* dev, drawcall_mod_context& ctx, D3DPRIMITIVETYPE type, UINT prim_count);

		/// Called right before every D3D9 draw of a vehicle geometry: the base pass, then (overlay) the dirt / livery passes.
		/// Overlay passes only reuse the pose of a base pass the api path took.
		/// @return true if the draw was submitted through the api (the D3D9 draw must be skipped)
		static bool try_draw_vehicle(IDirect3DDevice9* dev, drawcall_mod_context& ctx, D3DPRIMITIVETYPE type, UINT prim_count, bool overlay);

		/// Called right before the vertex shader draw of a ped geometry.
		/// @return true if the draw was submitted through the api (the D3D9 draw must be skipped)
		static bool try_draw_ped(IDirect3DDevice9* dev, drawcall_mod_context& ctx, D3DPRIMITIVETYPE type, UINT prim_count);

		/// passes the game camera to the runtime once per frame (frames without ray traced D3D9 draws have none otherwise)
		static void submit_camera();

		/// counts a static fixed function draw by the draw command it came from (devgui)
		static void note_static_draw();

		/// debugging: hide every static draw that is not owned by the api path (shows what the api renders)
		static bool debug_hide_d3d9_static();

		/// debugging: hide the ped draw try_draw_ped just kept on D3D9
		static bool debug_hide_d3d9_ped();

		static void on_begin_scene();
		static void on_end_scene();

		/// the first texture of the device installs the LockRect / UnlockRect hooks (texture writes rebuild the api materials bound to them)
		static void on_texture_created(IDirect3DTexture9* texture);

		/// the game wrote new content into a texture (UpdateTexture / UpdateSurface destination)
		static void on_texture_written(IDirect3DBaseTexture9* texture);

		/// destroys all api meshes and materials (settings change / rebuild button)
		static void destroy_all();

		static stats_s get_stats();
		static void devgui();

		/// RAGE string hash (rage::atStringHash), used for shader parameter names
		static std::uint32_t rage_hash(std::string_view str);

	private:
		static bool try_draw_impl(IDirect3DDevice9* dev, drawcall_mod_context& ctx, D3DPRIMITIVETYPE type, UINT prim_count, const draw_pose_s* pose, std::uint64_t& out_state_key);
	};
}
