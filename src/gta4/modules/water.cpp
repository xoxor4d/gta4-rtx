#include "std_include.hpp"
#include "water.hpp"

#include "renderer_api.hpp"
#include "comp_settings.hpp"
#include "imgui.hpp"
#include "renderer.hpp"
#include "water_dynamics.hpp"

#include <numbers>

namespace gta4
{
	namespace
	{
		// the game's wave simulation: 100 x 100 cells of 2 units around the camera
		constexpr int SIM_CELLS = 100;
		constexpr int SIM_SIZE = 200;
		constexpr int SIM_GAME_FADE = 10;
		constexpr int GAME_DETAIL_RADIUS = 100; // DETAILEDWATERDIST

		constexpr int MAX_POLYS_PER_SIDE = 10; // row buffers of the high detail primitive
		constexpr int LOD_SNAP = 16;
		constexpr int MAX_FINE_RADIUS = 128; // the simulation thread bakes the detail of this area on every update
		constexpr float GRAVITY = 9.81f;

		constexpr std::uint32_t RENDER_PHASE_FAR_DISTANCE = 0x374;

		DETOUR_TYPEDEF(CWater_ScanThroughBlocks, void, __cdecl, int phase_index);
		DETOUR_TYPEDEF(CWater_BlockHit, void, __cdecl, int block_x, int block_y);
		DETOUR_TYPEDEF(CWater_CalculateWavesForCoordinate, int, __cdecl, int x, int y, int unused1, int unused2, float* z, float* normal, DWORD* color);
		DETOUR_TYPEDEF(CWater_RenderHighDetailWaterRectangle, int, __cdecl, int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);
		DETOUR_TYPEDEF(CWater_RenderWaterRectangle, int, __cdecl, int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);

		typedef int (__cdecl* DrawHighDetailPrimitive_t)(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4,
			int triangles, int vertices, int polys_x, int polys_y);

		typedef int (__thiscall* IsSphereVisible_t)(void* viewport, float x, float y, float z, float radius, float* distance);

		struct wave_s
		{
			double kx = 0.0;
			double ky = 0.0;
			double omega = 0.0;
			double phase = 0.0;
			float amplitude = 0.0f;
		};

		constexpr int WAVE_COUNT = 5;
		constexpr float WAVE_LENGTH_RATIO[WAVE_COUNT] = { 1.0f, 0.62f, 0.41f, 0.27f, 0.17f };
		constexpr float WAVE_ANGLE_OFFSET[WAVE_COUNT] = { 0.0f, 28.0f, -33.0f, 61.0f, -74.0f };
		constexpr float WAVE_PHASE[WAVE_COUNT] = { 0.0f, 1.7f, 4.1f, 2.6f, 5.3f };

		// render thread: settings and camera of the current water render
		struct params_s
		{
			bool valid = false;

			int sim_base_x = 0;
			int sim_base_y = 0;
			float sim_edge_fade = 10.0f;

			bool far_waves = false;
			float far_wave_fade = 40.0f;
			wave_s waves[WAVE_COUNT];
			double time = 0.0;

			float normal_scale = 1.0f;

			bool wind = false;          // wind sea of water_dynamics
			float long_scale = 1.0f;    // water_wind_wave_long_scale
			float short_scale = 1.0f;   // water_wind_wave_short_scale
			float inland_level = 0.5f;  // water above it is inland water
			float inland_scale = 1.0f;  // wind waves on inland water
			bool wakes = false;         // interactive wave fields
			float phys_to_game = 1.0f;  // true slopes to the game's normal convention (normal_scale undone)

			bool lod = false;
			int lod_center_x = 0;
			int lod_center_y = 0;
			int lod_radius[2] = {};     // cells of 2 units inside [0], of 4 units inside [1], of 8 units beyond
			float lod_fade[2] = {};     // width of the fade towards the border of each
			bool game_waves = true;     // the game's own wave simulation is drawn (water_dynamics does not replace it)

			// api path: cells of 0.5 units inside [0], of 1 unit inside [1] around lod_center. 0 = none
			int fine_radius[2] = {};
			float fine_fade[2] = {};    // width of the fade towards the border of each
		};

		params_s g_params;
		float g_sim_rms = 0.0f;
		float g_far_wave_rms = 0.0f;
		LARGE_INTEGER g_qpc_start = {};
		LARGE_INTEGER g_qpc_frequency = {};

		water::stats_s g_stats;
		std::uint32_t g_pieces = 0u;
		std::uint32_t g_vertices = 0u;
		std::uint32_t g_pieces_culled = 0u;
		std::uint32_t g_blocks_map = 0u;
		std::uint32_t g_blocks_sea = 0u;
		int g_list_buffer_at_begin = -1;

		// The game swaps its two render list buffers while the main thread builds the next frame, which can be before or in the
		// middle of CWater::Render of the current one. The block scan tags the buffer it fills with the camera of its render phase,
		// the render reads the buffer tagged with the camera it draws with
		constexpr std::uint32_t LIST_BUFFER_SIZE = 0x10C4;
		constexpr std::uint32_t RENDER_PHASE_CAMERA = 0xF0;
		constexpr int MAX_LIST_SCANS = 8; // render phases scanning into one buffer

		struct list_tags_s
		{
			D3DXMATRIX cameras[MAX_LIST_SCANS];
			int count = 0;
		};

		std::mutex g_list_mutex;
		list_tags_s g_list_tags[2];
		int g_list_scan_buffer = -1;
		int g_list_buffer_pinned = -1;

		DETOUR_TYPEDEF(CWater_GetRenderLists, std::uint8_t*, __cdecl);

		void tag_list_buffer()
		{
			const auto phase = static_cast<const std::uint8_t*>(*game::pCRenderPhase_sm_pCurrent);
			if (!phase || !game::pWaterListBufferIndex) {
				return;
			}

			const int buffer = *game::pWaterListBufferIndex & 1;
			std::lock_guard lock(g_list_mutex);
			auto& tags = g_list_tags[buffer];

			if (buffer != g_list_scan_buffer)
			{
				tags.count = 0;
				g_list_scan_buffer = buffer;
			}

			std::memcpy(&tags.cameras[tags.count++ % MAX_LIST_SCANS], phase + RENDER_PHASE_CAMERA, sizeof(D3DXMATRIX));
		}

		float camera_distance(const D3DXMATRIX& a, const D3DXMATRIX& b)
		{
			float d = 0.0f;
			for (int r = 0; r < 4; r++)
			{
				for (int c = 0; c < 3; c++) {
					d += std::abs(a.m[r][c] - b.m[r][c]);
				}
			}
			return d;
		}

		// the buffer the game would read unless the other one was scanned with a closer camera
		int pick_list_buffer()
		{
			const int live = 1 - (*game::pWaterListBufferIndex & 1);
			const auto viewport = game::pCurrentViewport;
			if (!viewport || !viewport->wp) {
				return live;
			}

			float best[2] = { FLT_MAX, FLT_MAX };
			std::lock_guard lock(g_list_mutex);

			for (int b = 0; b < 2; b++)
			{
				for (int i = 0; i < std::min(g_list_tags[b].count, MAX_LIST_SCANS); i++) {
					best[b] = std::min(best[b], camera_distance(g_list_tags[b].cameras[i], viewport->wp->cameraInv));
				}
			}

			return best[1 - live] < best[live] ? 1 - live : live;
		}

		std::uint8_t* __cdecl CWater_GetRenderLists_hk()
		{
			if (g_list_buffer_pinned < 0) {
				return CWater_GetRenderLists_og();
			}

			return game::pWaterListBuffers + static_cast<std::size_t>(g_list_buffer_pinned) * LIST_BUFFER_SIZE;
		}

		// Remix API path: everything CWater::Render emits goes into one mesh, created, drawn and replaced every render
		struct api_s
		{
			bool active = false;
			bool cull_pieces = true;
			float uv_scale = 0.01f;
			float uv_off_x = 0.0f, uv_off_y = 0.0f; // whole texture tiles taken off per piece

			std::vector<remixapi_HardcodedVertex> vertices;
			std::vector<std::uint32_t> indices;
			std::uint32_t detail_pieces = 0u;
			std::uint32_t flat_rectangles = 0u;
			std::uint32_t fine_vertices = 0u;

			remixapi_MaterialHandle material = nullptr;
			remixapi_MaterialHandle stale_material = nullptr; // replaced material, destroyed with the last mesh that uses it
			std::uint32_t material_serial = 0u;
			remixapi_MeshHandle mesh = nullptr; // mesh of the last render, destroyed once the next one is drawn
			std::uint32_t serial = 0u;
		};

		water::material_s g_material;
		water::material_s g_material_in_use;

		api_s g_api;

		// the fixed function water needs the stable hash for its USD material and stays at the game's tessellated radius,
		// the api path reads both as user choices
		void sync_overrides()
		{
			static const auto cs = comp_settings::get();
			const bool forced = !cs->api_water_enabled._bool() || !renderer_api::is_runtime_supported();
			const char* reason = "Forced while water is drawn through the fixed function path";

			cs->override_water_texture_hash.set_temp_override_state(forced, reason);
			cs->water_detail_radius.set_temp_override_state(forced, reason);
			if (forced)
			{
				cs->override_water_texture_hash.set_var(true, true);
				cs->water_detail_radius.set_var(static_cast<float>(GAME_DETAIL_RADIUS), true);
			}
		}

		int floor_div(const int v, const int d) {
			return v >= 0 ? v / d : -((-v + d - 1) / d);
		}

		int floor_to(const int v, const int step) {
			return floor_div(v, step) * step;
		}

		int ceil_to(const int v, const int step) {
			return -floor_to(-v, step);
		}

		float smoothstep_to(const float width, const float x)
		{
			if (width <= 0.0f) {
				return x > 0.0f ? 1.0f : 0.0f;
			}

			const float t = std::clamp(x / width, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		bool bypassed() {
			return imgui::get() && imgui::get()->m_dbg_disable_water_module;
		}

		const int* detail_range() {
			return game::pWaterDetailRangeRender;
		}

		// distance (world units) to the closest edge of the tessellated window
		int window_edge_distance(const int x, const int y)
		{
			const auto r = detail_range();
			return std::min(std::min(x - r[0], r[1] - x), std::min(y - r[2], r[3] - y));
		}

		// distance (world units) to the closest edge of the simulation grid, <= 0 outside (same rule as FindDynamicWaterHeight_RT)
		int sim_edge_distance(const int x, const int y)
		{
			const int dx = std::min(x - g_params.sim_base_x, g_params.sim_base_x + SIM_SIZE - x);
			const int dy = std::min(y - g_params.sim_base_y, g_params.sim_base_y + SIM_SIZE - y);
			return std::min(dx, dy);
		}

		void update_sim_rms(const int buffer)
		{
			const float* heights = game::pWaterDynamicHeightRT + static_cast<std::size_t>(buffer) * SIM_CELLS * SIM_CELLS;
			double sum = 0.0;

			for (int i = 0; i < SIM_CELLS * SIM_CELLS; i++) {
				sum += static_cast<double>(heights[i]) * heights[i];
			}

			const float rms = static_cast<float>(std::sqrt(sum / (SIM_CELLS * SIM_CELLS)));
			g_sim_rms = g_sim_rms > 0.0f ? g_sim_rms + (rms - g_sim_rms) * 0.05f : rms;
		}

		void build_waves(const decltype(comp_settings::get()) cs)
		{
			const float base_length = std::max(cs->water_far_wave_length._float(), 0.5f);
			const float speed = cs->water_far_wave_speed._float();
			const float direction = cs->water_far_wave_direction._float();

			float rms_unit = 0.0f;
			for (int i = 0; i < WAVE_COUNT; i++) {
				const float length = base_length * WAVE_LENGTH_RATIO[i];
				rms_unit += length * length * 0.5f;
			}
			rms_unit = std::sqrt(rms_unit);

			// relative to the game's waves while they are drawn, a fixed height while water_dynamics replaces them
			g_far_wave_rms = g_params.game_waves
				? g_sim_rms * std::max(cs->water_far_wave_scale._float(), 0.0f)
				: std::max(cs->water_far_wave_height._float(), 0.0f);

			const float amplitude_scale = rms_unit > 0.0f ? g_far_wave_rms / rms_unit : 0.0f;

			for (int i = 0; i < WAVE_COUNT; i++)
			{
				auto& w = g_params.waves[i];
				const double length = base_length * WAVE_LENGTH_RATIO[i];
				const double k = 2.0 * std::numbers::pi / length;
				const double angle = (direction + WAVE_ANGLE_OFFSET[i]) * std::numbers::pi / 180.0;

				w.kx = k * std::cos(angle);
				w.ky = k * std::sin(angle);
				w.omega = std::sqrt(GRAVITY * k) * speed;
				w.phase = WAVE_PHASE[i];
				w.amplitude = static_cast<float>(length) * amplitude_scale;
			}
		}

		int lod_cheb_distance(int x, int y);

		struct wave3_s
		{
			float h = 0.0f;  // height (m)
			float dx = 0.0f; // true slopes
			float dy = 0.0f;
		};

		void add(wave3_s& d, const water_physics::grid_view_s& v, const float x, const float y, const float weight)
		{
			if (!v.valid() || weight <= 0.0f) {
				return;
			}

			float h, gx, gy;
			v.sample_cubic(x, y, h, gx, gy);
			d.h += h * weight;
			d.dx += gx * weight;
			d.dy += gy * weight;
		}

		// What is left at a point of the waves only finer cells can show: 1 well inside a ring, 0 from its outer border on.
		// A function of the position alone, so pieces of both rings agree on the vertices they share
		struct lod_fade_s
		{
			float cells_2 = 1.0f; // of what needs cells of 2 units
			float cells_4 = 1.0f; // of what needs cells of 4 units or less
		};

		lod_fade_s lod_fade_at(const int x, const int y)
		{
			if (!g_params.lod) {
				return {};
			}

			const auto cheb = static_cast<float>(lod_cheb_distance(x, y));
			return
			{
				smoothstep_to(g_params.lod_fade[0], static_cast<float>(g_params.lod_radius[0]) - cheb),
				smoothstep_to(g_params.lod_fade[1], static_cast<float>(g_params.lod_radius[1]) - cheb)
			};
		}

		/// Height multiplier of the wind waves on water at level z: inland water (ponds, pools) gets water_inland_wave_scale.
		float wind_gain_at_level(const float z) {
			return z > g_params.inland_level ? g_params.inland_scale : 1.0f;
		}

		/// Wakes and blast waves as the cells at this point can show them. The simulation thread filters the wake field for
		/// cells of 2, 4 and 8 units, crests shorter than ~2 cells would alias into spikes.
		void add_wake(wave3_s& d, const int x, const int y, const lod_fade_s& fade)
		{
			const auto& f = water_dynamics::frame();
			const auto fx = static_cast<float>(x);
			const auto fy = static_cast<float>(y);
			add(d, f.wake_lattice[0], fx, fy, fade.cells_2);
			add(d, f.wake_lattice[1], fx, fy, fade.cells_4 - fade.cells_2);
			add(d, f.wake_lattice[2], fx, fy, 1.0f - fade.cells_4);
		}

		/// The wind sea at a whole unit point.
		/// Waves of 16 units and longer are shown everywhere, those of 4 - 16 units fully by cells of 2 units, at 0.4 by
		/// cells of 4 units and not by cells of 8 units. Waves below 4 units are only on the cells below 2 units.
		/// @param wind_gain	see wind_gain_at_level
		/// @param all			gets the wind sea added
		/// @param short_part	gets the 4 - 16 unit part of it added, which the cells below 2 units replace with their own
		void wind_lattice(const int x, const int y, const lod_fade_s& fade, const float wind_gain, wave3_s& all, wave3_s& short_part)
		{
			const auto& f = water_dynamics::frame();
			if (!f.ocean_nodes.valid()) {
				return;
			}

			const float share = water_dynamics::shore_wave_share(f.shore, f.shore_far, static_cast<float>(x), static_cast<float>(y)) * wind_gain;
			const float long_weight = share * g_params.long_scale;
			const float mid_weight = share * g_params.short_scale * (0.6f * fade.cells_2 + 0.4f * fade.cells_4);
			const float* l = water_dynamics::ocean_nodes_s::at(f.ocean_nodes.long_band, x, y);
			const float* m = water_dynamics::ocean_nodes_s::at(f.ocean_nodes.mid_band, x, y);

			all.h += l[0] * long_weight + m[0] * mid_weight;
			all.dx += l[1] * long_weight + m[1] * mid_weight;
			all.dy += l[2] * long_weight + m[2] * mid_weight;
			short_part.h += m[0] * mid_weight;
			short_part.dx += m[1] * mid_weight;
			short_part.dy += m[2] * mid_weight;
		}

		/// The part of a whole unit point's height the cells below 2 units replace with their own detail.
		wave3_s lattice_detail(const int x, const int y, const float wind_gain)
		{
			const auto fade = lod_fade_at(x, y);
			wave3_s d;

			if (g_params.wakes) {
				add_wake(d, x, y, fade);
			}

			if (g_params.wind)
			{
				wave3_s unused_all;
				wind_lattice(x, y, fade, wind_gain, unused_all, d);
			}

			return d;
		}

		struct sample_s
		{
			float h = 0.0f;
			float gx = 0.0f; // slope in the game's normal convention (h[x - 1] - h[x + 1] of 2 unit cells = -4 dh/dx)
			float gy = 0.0f;
		};

		// the game's simulation with its 10 unit edge ramp replaced by sim_edge_fade
		sample_s sample_sim(const int x, const int y, DWORD* color, float& out_weight)
		{
			out_weight = 0.0f;

			DWORD unused_color = 0u;
			float z = 0.0f;
			float n[3] = { 0.0f, 0.0f, 1.0f };
			CWater_CalculateWavesForCoordinate_og(x, y, 0, 0, &z, n, color ? color : &unused_color);

			const int d = sim_edge_distance(x, y);
			if (d <= 0) {
				return {};
			}

			const float game_fade = d < SIM_GAME_FADE ? static_cast<float>(d + 1) * 0.1f : 1.0f;
			out_weight = smoothstep_to(g_params.sim_edge_fade, static_cast<float>(d));
			const float k = out_weight / game_fade;

			if (color)
			{
				const auto foam = static_cast<std::uint32_t>(std::min(static_cast<float>((*color >> 16) & 0xFFu) * k, 255.0f));
				*color = (*color & 0xFF00FFFFu) | (foam << 16);
			}

			// the grid holds the heights of water_dynamics (for buoyancy), drawing it would add them twice
			if (!g_params.game_waves) {
				return {};
			}

			return { z * k, n[0] / n[2] * k, n[1] / n[2] * k };
		}

		sample_s sample_far_waves(const int x, const int y)
		{
			sample_s s;
			for (const auto& w : g_params.waves)
			{
				const double arg = w.kx * x + w.ky * y - w.omega * g_params.time + w.phase;
				const float c = static_cast<float>(std::cos(arg));

				s.h += w.amplitude * static_cast<float>(std::sin(arg));
				s.gx -= 4.0f * w.amplitude * static_cast<float>(w.kx) * c;
				s.gy -= 4.0f * w.amplitude * static_cast<float>(w.ky) * c;
			}
			return s;
		}

		/// @param wind_gain	see wind_gain_at_level
		sample_s sample_combined(const int x, const int y, DWORD* color, const float wind_gain)
		{
			float sim_weight = 0.0f;
			sample_s s = sample_sim(x, y, color, sim_weight);

			if (g_params.far_waves && sim_weight < 1.0f)
			{
				// with a fixed height they are scaled down on inland water like the wind waves
				const float weight = (1.0f - sim_weight) * smoothstep_to(g_params.far_wave_fade, static_cast<float>(window_edge_distance(x, y)))
					* (g_params.game_waves ? 1.0f : wind_gain);

				if (weight > 0.0f)
				{
					const auto farwave = sample_far_waves(x, y);
					s.h += farwave.h * weight;
					s.gx += farwave.gx * weight;
					s.gy += farwave.gy * weight;
				}
			}

			if (g_params.wind || g_params.wakes)
			{
				const auto fade = lod_fade_at(x, y);

				// wind sea on all tessellated water, fading out towards the window edge like the far waves
				if (g_params.wind)
				{
					const float edge = smoothstep_to(g_params.far_wave_fade, static_cast<float>(window_edge_distance(x, y)));
					if (edge > 0.0f)
					{
						wave3_s all, unused_short;
						wind_lattice(x, y, fade, wind_gain, all, unused_short);
						s.h += all.h * edge;
						s.gx -= all.dx * edge * g_params.phys_to_game;
						s.gy -= all.dy * edge * g_params.phys_to_game;
					}
				}

				// wakes and blast waves, the ripples are too short for these cells
				if (g_params.wakes)
				{
					wave3_s w;
					add_wake(w, x, y, fade);
					s.h += w.h;
					s.gx -= w.dx * g_params.phys_to_game;
					s.gy -= w.dy * g_params.phys_to_game;
				}
			}

			return s;
		}

		// height inside a lattice cell, linear over the triangle the point lies in. The cell is split along the
		// (x0 + step, y0) - (x0, y0 + step) diagonal like the quads of a piece (draw_piece orders the corners), so a vertex off
		// the lattice lies on the surface the aligned pieces around it draw: the water quads the game stacks 0.2 mm apart
		// (water.dat, some piers) stay coincident however each of them is tessellated
		template <typename Height>
		float lattice_interpolate(const int x, const int y, const int step, const Height& height)
		{
			const int x0 = floor_to(x, step);
			const int y0 = floor_to(y, step);
			const float tx = static_cast<float>(x - x0) / step;
			const float ty = static_cast<float>(y - y0) / step;

			if (tx + ty >= 1.0f)
			{
				const float h11 = height(x0 + step, y0 + step);
				return h11 + (height(x0, y0 + step) - h11) * (1.0f - tx) + (height(x0 + step, y0) - h11) * (1.0f - ty);
			}

			const float h00 = height(x0, y0);
			return h00 + (height(x0 + step, y0) - h00) * tx + (height(x0, y0 + step) - h00) * ty;
		}

		int lod_cheb_distance(const int x, const int y) {
			return std::max(std::abs(x - g_params.lod_center_x), std::abs(y - g_params.lod_center_y));
		}

		// vertices on a ring border belong to the inner ring
		int lod_ring(const int cheb)
		{
			if (cheb <= g_params.lod_radius[0]) {
				return 0;
			}

			return cheb <= g_params.lod_radius[1] ? 1 : 2;
		}

		int lod_step(const int ring) {
			return 2 << ring;
		}

		float ring_surface_height(int x, int y, int ring, float wave_height, float wind_gain);

		// height of a point on the ring's lattice: the waves, towards the outer border blended into the next ring's surface,
		// on the border it matches it exactly
		float ring_lattice_height(const int x, const int y, const int ring, const float wave_height, const float wind_gain)
		{
			float h = wave_height;

			if (ring < 2)
			{
				const int next_step = lod_step(ring + 1);
				const float t = 1.0f - smoothstep_to(static_cast<float>(2 * next_step), static_cast<float>(g_params.lod_radius[ring] - lod_cheb_distance(x, y)));

				if (t > 0.0f) {
					h += (ring_surface_height(x, y, ring + 1, wave_height, wind_gain) - h) * t;
				}
			}

			return h;
		}

		// the surface of a ring: its lattice heights, linear over the lattice triangles in between
		float ring_surface_height(const int x, const int y, const int ring, const float wave_height, const float wind_gain)
		{
			const int step = lod_step(ring);

			if (x % step == 0 && y % step == 0) {
				return ring_lattice_height(x, y, ring, wave_height, wind_gain);
			}

			return lattice_interpolate(x, y, step, [ring, wind_gain](const int lx, const int ly) {
				return ring_lattice_height(lx, ly, ring, sample_combined(lx, ly, nullptr, wind_gain).h, wind_gain);
			});
		}

		float vertex_height(const int x, const int y, const float fine_height, const float wind_gain)
		{
			if (!g_params.lod) {
				return fine_height;
			}

			return ring_surface_height(x, y, lod_ring(lod_cheb_distance(x, y)), fine_height, wind_gain);
		}

		int __cdecl CWater_CalculateWavesForCoordinate_hk(int x, int y, int unused1, int unused2, float* z, float* normal, DWORD* color)
		{
			if (!g_params.valid || bypassed()) {
				return CWater_CalculateWavesForCoordinate_og(x, y, unused1, unused2, z, normal, color);
			}

			// z comes in as the level of the calm water at this point
			const float wind_gain = wind_gain_at_level(*z);
			const auto s = sample_combined(x, y, color, wind_gain);
			*z += vertex_height(x, y, s.h, wind_gain);

			const float gx = s.gx * g_params.normal_scale;
			const float gy = s.gy * g_params.normal_scale;
			const float inv_length = 1.0f / std::sqrt(gx * gx + gy * gy + 1.0f);

			normal[0] = gx * inv_length;
			normal[1] = gy * inv_length;
			normal[2] = inv_length;
			return 0;
		}

		void api_add_vertex(const float x, const float y, const float z, const float* normal)
		{
			remixapi_HardcodedVertex v = {};
			v.position[0] = x;
			v.position[1] = y;
			v.position[2] = z;

			// the waterline on beaches follows the sand instead of the polygon edge
			if (const auto& b = water_dynamics::frame().beach; b.valid())
			{
				float sink, unused_dx, unused_dy;
				b.sample(x, y, sink, unused_dx, unused_dy);
				v.position[2] -= std::max(sink, 0.0f);
			}

			v.normal[0] = normal[0];
			v.normal[1] = normal[1];
			v.normal[2] = normal[2];
			// texcoords in the hundreds lose their fraction in the runtime
			v.texcoord[0] = x * g_api.uv_scale - g_api.uv_off_x;
			v.texcoord[1] = y * g_api.uv_scale - g_api.uv_off_y;
			v.color = 0xFFFFFFFFu;
			g_api.vertices.push_back(v);
		}

		// two triangles split along the (x2 y1) - (x1 y2) diagonal, like the game's quads
		void api_add_quad(const std::uint32_t v00, const std::uint32_t v10, const std::uint32_t v01, const std::uint32_t v11)
		{
			g_api.indices.insert(g_api.indices.end(), { v00, v10, v01, v10, v11, v01 });
		}

		// a flat rectangle outside the tessellated window, as CWater::RenderFlatWaterRectangle_OneLayer draws it
		void api_flat(const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			g_api.uv_off_x = std::floor(static_cast<float>(x1) * g_api.uv_scale);
			g_api.uv_off_y = std::floor(static_cast<float>(y1) * g_api.uv_scale);
			if (x1 == x2 || y1 == y2) {
				return;
			}

			constexpr float up[3] = { 0.0f, 0.0f, 1.0f };
			const auto base = static_cast<std::uint32_t>(g_api.vertices.size());

			api_add_vertex(static_cast<float>(x1), static_cast<float>(y1), z1, up);
			api_add_vertex(static_cast<float>(x2), static_cast<float>(y1), z2, up);
			api_add_vertex(static_cast<float>(x1), static_cast<float>(y2), z3, up);
			api_add_vertex(static_cast<float>(x2), static_cast<float>(y2), z4, up);
			api_add_quad(base, base + 1u, base + 2u, base + 3u);

			g_api.flat_rectangles++;
		}

		// a tessellated piece as CWater::DrawHighDetailPrimitive emits it: the base height of every lattice vertex comes from the
		// piece corners (two planes split along the diagonal, CalculateWaterHeightsForStrip), the waves from our height hook
		void api_piece(const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4,
			const int polys_x, const int polys_y)
		{
			g_api.uv_off_x = std::floor(static_cast<float>(x1) * g_api.uv_scale);
			g_api.uv_off_y = std::floor(static_cast<float>(y1) * g_api.uv_scale);
			const int dx = (x2 - x1) / polys_x;
			const int dy = (y2 - y1) / polys_y;
			const float inv_x = 1.0f / static_cast<float>(polys_x);
			const float inv_y = 1.0f / static_cast<float>(polys_y);
			const auto base = static_cast<std::uint32_t>(g_api.vertices.size());

			for (int j = 0; j <= polys_y; j++)
			{
				for (int i = 0; i <= polys_x; i++)
				{
					const int x = x1 + i * dx;
					const int y = y1 + j * dy;

					float z = static_cast<float>(j) * inv_y + static_cast<float>(i) * inv_x >= 1.0f
						? z4 + static_cast<float>(polys_x - i) * (z3 - z4) * inv_x + static_cast<float>(polys_y - j) * (z2 - z4) * inv_y
						: z1 + static_cast<float>(i) * (z2 - z1) * inv_x + static_cast<float>(j) * (z3 - z1) * inv_y;

					float normal[3] = { 0.0f, 0.0f, 1.0f };
					DWORD color = 0u;
					CWater_CalculateWavesForCoordinate_hk(x, y, 0, 0, &z, normal, &color);
					api_add_vertex(static_cast<float>(x), static_cast<float>(y), z, normal);
				}
			}

			const auto row = static_cast<std::uint32_t>(polys_x + 1);
			for (int j = 0; j < polys_y; j++)
			{
				for (int i = 0; i < polys_x; i++)
				{
					const auto v00 = base + static_cast<std::uint32_t>(j) * row + static_cast<std::uint32_t>(i);
					api_add_quad(v00, v00 + 1u, v00 + row, v00 + row + 1u);
				}
			}

			g_api.detail_pieces++;
		}

		// what the cells below 2 units add at a point
		struct fine_detail_s
		{
			float weight = 0.0f; // 0 at the border to the cells of 2 units
			float h = 0.0f;
			float gx = 0.0f;     // true slopes
			float gy = 0.0f;
			float px = 0.0f;     // horizontal displacement
			float py = 0.0f;
		};

		/// The detail baked by the simulation thread at a point: wakes, ripples and the short wind waves.
		/// A function of the position alone, so neighbouring pieces agree on the vertices they share.
		/// @param wind_gain	see wind_gain_at_level
		/// @return false without detail there
		bool sample_fine_detail(const float x, const float y, const float wind_gain, fine_detail_s& out)
		{
			constexpr int CHANNELS = water_dynamics::detail_s::CHANNELS;
			const auto& detail = water_dynamics::frame().detail;
			const float cheb = std::max(std::abs(x - static_cast<float>(g_params.lod_center_x)), std::abs(y - static_cast<float>(g_params.lod_center_y)));

			// fades out towards the cells of 2 units, and towards the edge of the tessellated window like the wind sea
			const auto r = detail_range();
			const float window_edge = std::min(std::min(x - static_cast<float>(r[0]), static_cast<float>(r[1]) - x),
				std::min(y - static_cast<float>(r[2]), static_cast<float>(r[3]) - y));

			out.weight = smoothstep_to(g_params.fine_fade[1], static_cast<float>(g_params.fine_radius[1]) - cheb)
				* smoothstep_to(g_params.far_wave_fade, window_edge);

			float d[CHANNELS];
			if (out.weight <= 0.0f || !detail.sample(x, y, d)) {
				return false;
			}

			// Cells of 1 unit can not show waves of 0.5 units: there the detail is averaged over the cell (4 lookups).
			// Cells of 0.5 units fade into that average towards their border
			const float averaged = g_params.fine_radius[0]
				? 1.0f - smoothstep_to(g_params.fine_fade[0], static_cast<float>(g_params.fine_radius[0]) - cheb)
				: 1.0f;

			if (averaged > 0.0f)
			{
				float sum[CHANNELS] = {};
				float e[CHANNELS];
				int found = 0;

				for (const float ox : { -0.25f, 0.25f })
				{
					for (const float oy : { -0.25f, 0.25f })
					{
						if (detail.sample(x + ox, y + oy, e))
						{
							found++;
							for (int c = 0; c < CHANNELS; c++) {
								sum[c] += e[c];
							}
						}
					}
				}

				if (found == 4)
				{
					for (int c = 0; c < CHANNELS; c++) {
						d[c] += (0.25f * sum[c] - d[c]) * averaged;
					}
				}
			}

			out.h = d[0] + wind_gain * d[3];
			out.gx = d[1] + wind_gain * d[4];
			out.gy = d[2] + wind_gain * d[5];
			out.px = wind_gain * d[6];
			out.py = wind_gain * d[7];
			return true;
		}

		/// Where cells of 0.5 units meet cells of 1 unit, the vertices in between two of the coarser ones are put on the
		/// line between them, or the border would show gaps.
		/// @param base		first vertex of the piece, cols x rows vertices of 0.5 unit cells
		void close_fine_border(const std::uint32_t base, const int cols, const int rows, const int x1, const int x2, const int y1, const int y2)
		{
			const int r = g_params.fine_radius[0];
			const auto on_border = [r](const int v, const int center)
				{
					return v == center - r || v == center + r;
				};

			const auto average = [](const remixapi_HardcodedVertex& a, const remixapi_HardcodedVertex& b, remixapi_HardcodedVertex& out)
				{
					float normal[3];
					for (int k = 0; k < 3; k++)
					{
						out.position[k] = 0.5f * (a.position[k] + b.position[k]);
						normal[k] = a.normal[k] + b.normal[k];
					}

					const float inv_len = 1.0f / std::max(std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]), 1e-6f);
					for (int k = 0; k < 3; k++) {
						out.normal[k] = normal[k] * inv_len;
					}

					out.texcoord[0] = 0.5f * (a.texcoord[0] + b.texcoord[0]);
					out.texcoord[1] = 0.5f * (a.texcoord[1] + b.texcoord[1]);
				};

			auto* v = g_api.vertices.data() + base;

			for (const int col : { 0, cols - 1 })
			{
				if (!on_border(col == 0 ? x1 : x2, g_params.lod_center_x)) {
					continue;
				}

				for (int j = 1; j < rows - 1; j += 2) {
					average(v[(j - 1) * cols + col], v[(j + 1) * cols + col], v[j * cols + col]);
				}
			}

			for (const int row : { 0, rows - 1 })
			{
				if (!on_border(row == 0 ? y1 : y2, g_params.lod_center_y)) {
					continue;
				}

				for (int i = 1; i < cols - 1; i += 2) {
					average(v[row * cols + i - 1], v[row * cols + i + 1], v[row * cols + i]);
				}
			}
		}

		/// A piece of cells below 2 units: the whole unit vertices as api_piece computes them, every cell split further.
		/// fine vertex = lattice surface + weight * (detail - the part of it the lattice shows already). The weight is 0 at the
		/// border to the cells of 2 units, so that border matches the pieces outside.
		/// @param cells_per_unit	2 for cells of 0.5 units, 1 for cells of 1 unit
		void api_piece_fine(const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4,
			const int polys_x, const int polys_y, const int cells_per_unit)
		{
			g_api.uv_off_x = std::floor(static_cast<float>(x1) * g_api.uv_scale);
			g_api.uv_off_y = std::floor(static_cast<float>(y1) * g_api.uv_scale);
			const int dx = (x2 - x1) / polys_x;
			const int dy = (y2 - y1) / polys_y;
			const int div_x = dx * cells_per_unit;
			const int div_y = dy * cells_per_unit;
			const float inv_x = 1.0f / static_cast<float>(polys_x);
			const float inv_y = 1.0f / static_cast<float>(polys_y);
			const int cols = polys_x + 1;

			struct lattice_s
			{
				float z, gx, gy;     // surface height, slopes in the game's normal convention (normal scale applied)
				float dh, dgx, dgy;  // its detail part (true slopes)
				float wind_gain;     // see wind_gain_at_level
			};

			thread_local std::vector<lattice_s> lat;
			lat.resize(static_cast<size_t>(cols) * (polys_y + 1));

			for (int j = 0; j <= polys_y; j++)
			{
				for (int i = 0; i <= polys_x; i++)
				{
					const int x = x1 + i * dx;
					const int y = y1 + j * dy;

					float z = static_cast<float>(j) * inv_y + static_cast<float>(i) * inv_x >= 1.0f
						? z4 + static_cast<float>(polys_x - i) * (z3 - z4) * inv_x + static_cast<float>(polys_y - j) * (z2 - z4) * inv_y
						: z1 + static_cast<float>(i) * (z2 - z1) * inv_x + static_cast<float>(j) * (z3 - z1) * inv_y;

					const float wind_gain = wind_gain_at_level(z);
					float normal[3] = { 0.0f, 0.0f, 1.0f };
					DWORD color = 0u;
					CWater_CalculateWavesForCoordinate_hk(x, y, 0, 0, &z, normal, &color);

					const auto d = lattice_detail(x, y, wind_gain);
					auto& l = lat[static_cast<size_t>(j) * cols + i];
					l.wind_gain = wind_gain;
					l.z = z;
					l.gx = normal[0] / normal[2];
					l.gy = normal[1] / normal[2];
					l.dh = d.h;
					l.dgx = d.dx;
					l.dgy = d.dy;
				}
			}

			const int fcols = polys_x * div_x + 1;
			const int frows = polys_y * div_y + 1;
			const auto base = static_cast<std::uint32_t>(g_api.vertices.size());
			const float fine_step_x = static_cast<float>(dx) / static_cast<float>(div_x);
			const float fine_step_y = static_cast<float>(dy) / static_cast<float>(div_y);
			const float p2g = g_params.phys_to_game * g_params.normal_scale; // true slope to the final normal convention

			for (int fj = 0; fj < frows; fj++)
			{
				const int cj = std::min(fj / div_y, polys_y - 1);
				const float ty = static_cast<float>(fj - cj * div_y) / static_cast<float>(div_y);
				const float y = static_cast<float>(y1) + static_cast<float>(fj) * fine_step_y;

				for (int fi = 0; fi < fcols; fi++)
				{
					const int ci = std::min(fi / div_x, polys_x - 1);
					const float tx = static_cast<float>(fi - ci * div_x) / static_cast<float>(div_x);
					const float x = static_cast<float>(x1) + static_cast<float>(fi) * fine_step_x;

					// the lattice triangle the point lies in, split like lattice_interpolate and the piece quads
					const auto& l00 = lat[static_cast<size_t>(cj) * cols + ci];
					const auto& l10 = lat[static_cast<size_t>(cj) * cols + ci + 1];
					const auto& l01 = lat[static_cast<size_t>(cj + 1) * cols + ci];
					const auto& l11 = lat[static_cast<size_t>(cj + 1) * cols + ci + 1];
					const bool upper = tx + ty >= 1.0f;

					const auto tri = [&](float lattice_s::* m)
						{
							if (upper)
							{
								const float a = l11.*m;
								return a + (l01.*m - a) * (1.0f - tx) + (l10.*m - a) * (1.0f - ty);
							}

							const float a = l00.*m;
							return a + (l10.*m - a) * tx + (l01.*m - a) * ty;
						};

					float z = tri(&lattice_s::z);
					float gx = tri(&lattice_s::gx);
					float gy = tri(&lattice_s::gy);
					float px = x;
					float py = y;

					fine_detail_s d;
					if (sample_fine_detail(x, y, tri(&lattice_s::wind_gain), d))
					{
						z += d.weight * (d.h - tri(&lattice_s::dh));
						gx -= d.weight * (d.gx - tri(&lattice_s::dgx)) * p2g;
						gy -= d.weight * (d.gy - tri(&lattice_s::dgy)) * p2g;
						px += d.weight * d.px;
						py += d.weight * d.py;
					}

					const float inv_len = 1.0f / std::sqrt(gx * gx + gy * gy + 1.0f);
					const float normal[3] = { gx * inv_len, gy * inv_len, inv_len };
					api_add_vertex(px, py, z, normal);
				}
			}

			if (cells_per_unit == 2) {
				close_fine_border(base, fcols, frows, x1, x2, y1, y2);
			}

			const auto row = static_cast<std::uint32_t>(fcols);
			for (int j = 0; j < frows - 1; j++)
			{
				for (int i = 0; i < fcols - 1; i++)
				{
					const auto v00 = base + static_cast<std::uint32_t>(j) * row + static_cast<std::uint32_t>(i);
					api_add_quad(v00, v00 + 1u, v00 + row, v00 + row + 1u);
				}
			}

			g_api.detail_pieces++;
			g_api.fine_vertices += static_cast<std::uint32_t>(fcols * frows);
		}

		/// True for a piece inside the square of this radius around the lod center.
		bool inside_radius(const int radius, const int x1, const int x2, const int y1, const int y2)
		{
			return radius > 0
				&& x1 >= g_params.lod_center_x - radius && x2 <= g_params.lod_center_x + radius
				&& y1 >= g_params.lod_center_y - radius && y2 <= g_params.lod_center_y + radius;
		}

		using piece_fn = void(*)(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);

		void draw_piece(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);

		// x / y split of a piece, corner heights (x1 y1, x2 y1, x1 y2, x2 y2) interpolated like CWater::SplitWaterRectangleAlong*Line
		void split_x(const piece_fn fn, const int s, const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const float t = static_cast<float>(s - x1) / static_cast<float>(x2 - x1);
			const float za = z1 + (z2 - z1) * t;
			const float zb = z3 + (z4 - z3) * t;

			fn(x1, s, y1, y2, z1, za, z3, zb);
			fn(s, x2, y1, y2, za, z2, zb, z4);
		}

		void split_y(const piece_fn fn, const int s, const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const float t = static_cast<float>(s - y1) / static_cast<float>(y2 - y1);
			const float za = z1 + (z3 - z1) * t;
			const float zb = z2 + (z4 - z2) * t;

			fn(x1, x2, y1, s, z1, z2, za, zb);
			fn(x1, x2, s, y2, za, zb, z3, z4);
		}

		bool strictly_inside(const int v, const int a, const int b) {
			return v > std::min(a, b) && v < std::max(a, b);
		}

		// replaces CWater::RenderHighDetailWaterRectangle while the LOD or the api path is on: pieces are split at the ring borders and
		// aligned to the cell size of their ring, so neighbouring pieces share their border vertices or the lattice the heights follow.
		// The corners are ordered x1 < x2, y1 < y2 first: the game lists most quads reversed, and the quads of a piece split along
		// the diagonal of its corner order, which has to be the one lattice_interpolate uses
		void draw_piece(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4)
		{
			if (x1 > x2)
			{
				std::swap(x1, x2);
				std::swap(z1, z2);
				std::swap(z3, z4);
			}

			if (y1 > y2)
			{
				std::swap(y1, y2);
				std::swap(z1, z3);
				std::swap(z2, z4);
			}

			const float half_x = static_cast<float>(x2 - x1) * 0.5f;
			const float half_y = static_cast<float>(y2 - y1) * 0.5f;
			const auto is_sphere_visible = reinterpret_cast<IsSphereVisible_t>(game::func_addr__grcViewport_IsSphereVisible);

			if ((!g_api.active || g_api.cull_pieces) && !is_sphere_visible(game::pCurrentViewport->wp, static_cast<float>(x1 + x2) * 0.5f,
				static_cast<float>(y1 + y2) * 0.5f, z1, std::sqrt(half_x * half_x + half_y * half_y), nullptr))
			{
				g_pieces_culled++;
				return;
			}

			for (int r = 0; g_params.lod && r < 2; r++)
			{
				const int lines[2] = { g_params.lod_center_x - g_params.lod_radius[r], g_params.lod_center_x + g_params.lod_radius[r] };
				for (const int line : lines)
				{
					if (strictly_inside(line, x1, x2))
					{
						split_x(draw_piece, line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}

				const int lines_y[2] = { g_params.lod_center_y - g_params.lod_radius[r], g_params.lod_center_y + g_params.lod_radius[r] };
				for (const int line : lines_y)
				{
					if (strictly_inside(line, y1, y2))
					{
						split_y(draw_piece, line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}
			}

			// the borders of the cells below 2 units (api path)
			for (int level = 0; g_api.active && level < 2; level++)
			{
				const int r = g_params.fine_radius[level];
				if (!r) {
					continue;
				}

				for (const int line : { g_params.lod_center_x - r, g_params.lod_center_x + r })
				{
					if (strictly_inside(line, x1, x2))
					{
						split_x(draw_piece, line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}

				for (const int line : { g_params.lod_center_y - r, g_params.lod_center_y + r })
				{
					if (strictly_inside(line, y1, y2))
					{
						split_y(draw_piece, line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}
			}

			const int ring = g_params.lod ? lod_ring(lod_cheb_distance((x1 + x2) / 2, (y1 + y2) / 2)) : 0;
			int step = lod_step(ring);

			// align to the lattice of the ring, the remainder strips get a finer cell size
			if (const int a = ceil_to(x1, step); strictly_inside(a, x1, x2))
			{
				split_x(draw_piece, a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = floor_to(x2, step); strictly_inside(a, x1, x2))
			{
				split_x(draw_piece, a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = ceil_to(y1, step); strictly_inside(a, y1, y2))
			{
				split_y(draw_piece, a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = floor_to(y2, step); strictly_inside(a, y1, y2))
			{
				split_y(draw_piece, a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			while (step > 2 && (x1 % step || x2 % step || y1 % step || y2 % step)) {
				step >>= 1;
			}

			const int polys_x = std::max((x2 - x1) / step, 1);
			const int polys_y = std::max((y2 - y1) / step, 1);

			if (polys_x > MAX_POLYS_PER_SIDE || polys_y > MAX_POLYS_PER_SIDE)
			{
				if (polys_x >= polys_y) {
					split_x(draw_piece, x1 + (polys_x / 2) * step, x1, x2, y1, y2, z1, z2, z3, z4);
				} else {
					split_y(draw_piece, y1 + (polys_y / 2) * step, x1, x2, y1, y2, z1, z2, z3, z4);
				}

				return;
			}

			g_pieces++;
			g_vertices += static_cast<std::uint32_t>((polys_x + 1) * (polys_y + 1));

			if (g_api.active)
			{
				if (inside_radius(g_params.fine_radius[0], x1, x2, y1, y2)) {
					api_piece_fine(x1, x2, y1, y2, z1, z2, z3, z4, polys_x, polys_y, 2);
				}
				else if (inside_radius(g_params.fine_radius[1], x1, x2, y1, y2)) {
					api_piece_fine(x1, x2, y1, y2, z1, z2, z3, z4, polys_x, polys_y, 1);
				}
				else {
					api_piece(x1, x2, y1, y2, z1, z2, z3, z4, polys_x, polys_y);
				}

				return;
			}

			const auto draw = reinterpret_cast<DrawHighDetailPrimitive_t>(game::func_addr__CWater_DrawHighDetailPrimitive);
			draw(x1, x2, y1, y2, z1, z2, z3, z4, 2 * polys_x * polys_y, (polys_x + 1) * (polys_y + 1), polys_x, polys_y);
		}

		// CWater::RenderWaterRectangle on the api path: the same split along the tessellated window, the parts outside stay flat
		void api_rectangle(const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const auto r = detail_range();
			const int min_y = std::min(y1, y2);
			const int max_y = std::max(y1, y2);

			if (x1 >= r[1] || x2 <= r[0] || min_y >= r[3] || max_y <= r[2]) {
				api_flat(x1, x2, y1, y2, z1, z2, z3, z4);
			}
			else if (x2 > r[1]) {
				split_x(api_rectangle, r[1], x1, x2, y1, y2, z1, z2, z3, z4);
			}
			else if (x1 < r[0]) {
				split_x(api_rectangle, r[0], x1, x2, y1, y2, z1, z2, z3, z4);
			}
			else if (min_y < r[2]) {
				split_y(api_rectangle, r[2], x1, x2, y1, y2, z1, z2, z3, z4);
			}
			else if (max_y > r[3]) {
				split_y(api_rectangle, r[3], x1, x2, y1, y2, z1, z2, z3, z4);
			}
			else {
				draw_piece(x1, x2, y1, y2, z1, z2, z3, z4);
			}
		}

		int __cdecl CWater_RenderWaterRectangle_hk(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4)
		{
			PROFILE_ZONE_N("CWater::RenderWaterRectangle");
			if (!g_api.active) {
				return CWater_RenderWaterRectangle_og(x1, x2, y1, y2, z1, z2, z3, z4);
			}

			api_rectangle(x1, x2, y1, y2, z1, z2, z3, z4);
			return 0;
		}

		bool api_create_material(const water::material_s& p)
		{
			const remixapi_MaterialInfoTranslucentEXT translucent =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT,
				.pNext = nullptr,
				.transmittanceTexture = L"",
				.refractiveIndex = p.ior,
				.transmittanceColor = { p.transmittance_color[0], p.transmittance_color[1], p.transmittance_color[2] },
				.transmittanceMeasurementDistance = p.transmittance_distance,
				.thinWallThickness_hasvalue = FALSE,
				.thinWallThickness_value = 0.0f,
				.useDiffuseLayer = FALSE,
			};

			// used without a USD water material; the override hash of the instance picks the replacement.
			// The handle is the hash, the material it replaces lives until the last mesh using it is gone: two hashes alternate
			const remixapi_MaterialInfo info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,
				.pNext = const_cast<remixapi_MaterialInfoTranslucentEXT*>(&translucent),
				.hash = shared::utils::string_hash64("api_water_material") + (g_api.material_serial++ & 1u),
				.albedoTexture = L"",
				.normalTexture = L"",
				.tangentTexture = L"",
				.emissiveTexture = L"",
				.emissiveIntensity = 0.0f,
				.emissiveColorConstant = { 0.0f, 0.0f, 0.0f },
				.filterMode = 1,
				.wrapModeU = 1,
				.wrapModeV = 1,
			};

			return shared::common::remix_api::get().m_bridge.CreateMaterial(&info, &g_api.material) == REMIXAPI_ERROR_CODE_SUCCESS && g_api.material;
		}

		void api_destroy_material(remixapi_MaterialHandle& material)
		{
			if (material)
			{
				shared::common::remix_api::get().m_bridge.DestroyMaterial(material);
				material = nullptr;
			}
		}

		void api_destroy_mesh()
		{
			if (g_api.mesh)
			{
				shared::common::remix_api::get().m_bridge.DestroyMesh(g_api.mesh);
				g_api.mesh = nullptr;
			}
		}

		// one mesh for the whole water render; the last render's mesh is destroyed once this one is drawn
		bool api_submit()
		{
			static const auto cs = comp_settings::get();
			const auto& bridge = shared::common::remix_api::get().m_bridge;

			if (!g_api.material || g_material != g_material_in_use)
			{
				api_destroy_material(g_api.stale_material);
				g_api.stale_material = g_api.material;
				g_api.material = nullptr;

				if (!api_create_material(g_material)) {
					return false;
				}

				g_material_in_use = g_material;
			}

			const remixapi_MeshInfoSurfaceTriangles triangles =
			{
				.vertices_values = g_api.vertices.data(),
				.vertices_count = g_api.vertices.size(),
				.indices_values = g_api.indices.data(),
				.indices_count = g_api.indices.size(),
				.skinning_hasvalue = FALSE,
				.skinning_value = {},
				.material = g_api.material,
			};

			const remixapi_MeshInfo mesh_info =
			{
				.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
				.pNext = nullptr,
				.hash = shared::utils::string_hash64("api_water_mesh") + (g_api.serial++ & 1u),
				.surfaces_values = &triangles,
				.surfaces_count = 1u,
			};

			remixapi_MeshHandle mesh = nullptr;
			if (bridge.CreateMesh(&mesh_info, &mesh) != REMIXAPI_ERROR_CODE_SUCCESS || !mesh) {
				return false;
			}

			// the values the fixed function water branch passes through render states (renderer.cpp)
			remixapi_InstanceInfoD3DOverridesEXT overrides = {};
			overrides.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO_D3D_OVERRIDES_EXT;

			std::uint32_t modifiers = 0u;
			if (!imgui::get()->m_dbg_disable_water_worldpos_logic)
			{
				modifiers |= static_cast<std::uint32_t>(RemixModifier::Translucent_WorldposAsUVs);
				overrides.multiUse[0] = 0.01f * cs->water_texture_uv_scale._float();
			}

			if (cs->water_texture_normal_fadeout_distance._float() > 0.0f)
			{
				modifiers |= static_cast<std::uint32_t>(RemixModifier::Translucent_FadeNormalUntilDist);
				overrides.multiUse[1] = cs->water_texture_normal_fadeout_distance._float();
			}

			if (cs->water_apply_shoreline_modifier._bool()) {
				modifiers |= static_cast<std::uint32_t>(RemixModifier::WaterShoreline);
			}

			overrides.modifierFlags = modifiers;

			if (cs->override_water_texture_hash._bool()) {
				overrides.hashOverride = shared::utils::string_hash32("water");
			}

			const bool animated_water = cs->water_apply_animated_water_category._bool();
			if (animated_water) {
				overrides.categoryFlags = static_cast<std::uint32_t>(InstanceCategories::AnimatedWater);
			}

			remixapi_InstanceInfo instance = {};
			instance.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
			instance.pNext = &overrides;
			instance.categoryFlags = animated_water ? REMIXAPI_INSTANCE_CATEGORY_BIT_ANIMATED_WATER : 0u;
			instance.mesh = mesh;
			instance.transform.matrix[0][0] = 1.0f;
			instance.transform.matrix[1][1] = 1.0f;
			instance.transform.matrix[2][2] = 1.0f;
			instance.doubleSided = TRUE; // seen from below when the camera is under water

			renderer_api::submit_camera();
			bridge.DrawInstance(&instance);

			api_destroy_mesh();
			api_destroy_material(g_api.stale_material);
			g_api.mesh = mesh;
			return true;
		}

		int __cdecl CWater_RenderHighDetailWaterRectangle_hk(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4)
		{
			PROFILE_ZONE_N("CWater::RenderHighDetailWaterRectangle");
			if (!g_params.valid || !g_params.lod || bypassed()) {
				return CWater_RenderHighDetailWaterRectangle_og(x1, x2, y1, y2, z1, z2, z3, z4);
			}

			draw_piece(x1, x2, y1, y2, z1, z2, z3, z4);
			return 0;
		}

		void __cdecl CWater_BlockHit_hk(int block_x, int block_y)
		{
			if (block_x >= 0 && block_y >= 0 && block_x < 12 && block_y < 12) {
				g_blocks_map++;
			}

			if (block_x <= 0 || block_y <= 0 || block_x >= 11 || block_y >= 11) {
				g_blocks_sea++;
			}

			CWater_BlockHit_og(block_x, block_y);
		}

		// the block footprint reaches out to the render phase's far distance, scaled for the water scan only
		void __cdecl CWater_ScanThroughBlocks_hk(int phase_index)
		{
			PROFILE_ZONE_N("CWater::ScanThroughBlocks");
			static const auto cs = comp_settings::get();
			tag_list_buffer();

			if (bypassed())
			{
				*game::pWaterDetailDist = GAME_DETAIL_RADIUS;
				CWater_ScanThroughBlocks_og(phase_index);
				return;
			}

			*game::pWaterDetailDist = cs->water_lod._bool()
				? std::clamp(static_cast<int>(cs->water_detail_radius._float()), GAME_DETAIL_RADIUS, 600)
				: GAME_DETAIL_RADIUS;

			g_blocks_map = 0u;
			g_blocks_sea = 0u;

			const auto phase = static_cast<std::uint8_t*>(*game::pCRenderPhase_sm_pCurrent);
			if (!phase)
			{
				CWater_ScanThroughBlocks_og(phase_index);
				return;
			}

			auto& far_distance = *reinterpret_cast<float*>(phase + RENDER_PHASE_FAR_DISTANCE);
			const float saved = far_distance;

			far_distance = saved * std::clamp(cs->water_block_distance_scale._float(), 0.1f, 10.0f);
			CWater_ScanThroughBlocks_og(phase_index);
			far_distance = saved;

			g_stats.blocks_map = g_blocks_map;
			g_stats.blocks_sea = g_blocks_sea;
		}
	}

	void water::on_render_begin()
	{
		PROFILE_ZONE();
		static const auto cs = comp_settings::get();
		g_api.active = false;
		sync_overrides();
		g_list_buffer_at_begin = game::pWaterListBufferIndex ? *game::pWaterListBufferIndex : -1;
		g_list_buffer_pinned = -1;

		if (get() && CWater_GetRenderLists_og && !bypassed())
		{
			g_list_buffer_pinned = pick_list_buffer();
			if (g_list_buffer_pinned != 1 - (g_list_buffer_at_begin & 1)) {
				g_stats.list_repicks++;
			}
		}

		if (!get() || !game::pWaterDetailDist || !game::pWaterRenderBufferIndex || !game::pWaterCameraCoordsRT || !game::pWaterDetailRangeRender
			|| !game::pWaterDynamicHeightRT)
		{
			return;
		}

		if (bypassed())
		{
			g_params.valid = false;
			return;
		}

		g_stats.pieces = g_pieces;
		g_stats.vertices = g_vertices;
		g_stats.pieces_culled = g_pieces_culled;
		g_pieces = 0u;
		g_vertices = 0u;
		g_pieces_culled = 0u;

		const int buffer = *game::pWaterRenderBufferIndex;
		const float cam_x = game::pWaterCameraCoordsRT[buffer * 4 + 0];
		const float cam_y = game::pWaterCameraCoordsRT[buffer * 4 + 1];

		g_params.sim_base_x = 2 * ((static_cast<int>(cam_x) - SIM_CELLS) / 2);
		g_params.sim_base_y = 2 * ((static_cast<int>(cam_y) - SIM_CELLS) / 2);
		g_params.sim_edge_fade = std::max(cs->water_sim_edge_fade._float(), 1.0f);
		g_params.normal_scale = std::max(cs->water_vertex_normal_scale._float(), 0.0f);

		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		if (!g_qpc_frequency.QuadPart)
		{
			QueryPerformanceFrequency(&g_qpc_frequency);
			g_qpc_start = now;
		}
		g_params.time = static_cast<double>(now.QuadPart - g_qpc_start.QuadPart) / static_cast<double>(g_qpc_frequency.QuadPart);

		// while water_dynamics writes the game's wave grid there are no waves of the game to measure
		g_params.game_waves = !water_dynamics::coupling_active();
		if (g_params.game_waves) {
			update_sim_rms(buffer);
		}

		const bool wind_waves = cs->water_wind_waves._bool();
		const bool interactive = cs->water_interactive._bool();
		g_params.far_waves = cs->water_far_waves._bool();
		g_params.far_wave_fade = std::max(cs->water_far_wave_fade._float(), 0.0f);
		build_waves(cs);

		g_api.active = cs->api_water_enabled._bool() && renderer_api::is_runtime_supported();

		g_params.lod = cs->water_lod._bool();

		// the water's camera coordinates follow the gameplay camera, scripted cameras (cutscenes) use the render viewport's
		float view_x = cam_x;
		float view_y = cam_y;
		if (game::pCurrentViewport && game::pCurrentViewport->wp)
		{
			view_x = game::pCurrentViewport->wp->cameraInv.m[3][0];
			view_y = game::pCurrentViewport->wp->cameraInv.m[3][1];
		}

		// the rings follow the view camera while it is inside the tessellated window
		{
			const auto r = detail_range();
			const bool inside = view_x > static_cast<float>(r[0] + 64) && view_x < static_cast<float>(r[1] - 64)
				&& view_y > static_cast<float>(r[2] + 64) && view_y < static_cast<float>(r[3] - 64);

			g_params.lod_center_x = floor_to(static_cast<int>(inside ? view_x : cam_x), LOD_SNAP);
			g_params.lod_center_y = floor_to(static_cast<int>(inside ? view_y : cam_y), LOD_SNAP);
		}

		// Cells below 2 units show the short wind waves and the interactive fields, without them they are only vertices.
		// Each cell size needs a ring of at least LOD_SNAP units
		const int fine_cells = g_api.active && (wind_waves || interactive) ? std::clamp(cs->water_lod_fine_cells._int(), 0, 2) : 0;
		const int radius_2 = std::clamp(floor_to(static_cast<int>(cs->water_lod_radius_2._float()), LOD_SNAP), fine_cells == 2 ? 2 * LOD_SNAP : LOD_SNAP, MAX_FINE_RADIUS);
		const int radius_1 = std::clamp(floor_to(static_cast<int>(cs->water_lod_radius_1._float()), LOD_SNAP), LOD_SNAP, std::max(LOD_SNAP, radius_2 - LOD_SNAP));

		g_params.fine_radius[0] = fine_cells == 2 ? radius_1 : 0;
		g_params.fine_radius[1] = fine_cells >= 1 ? radius_2 : 0;

		const int ring0_min = g_params.fine_radius[1] ? g_params.fine_radius[1] + LOD_SNAP : LOD_SNAP;
		g_params.lod_radius[0] = std::max(ring0_min, floor_to(static_cast<int>(cs->water_lod_radius_4._float()), LOD_SNAP));
		g_params.lod_radius[1] = std::max(g_params.lod_radius[0] + LOD_SNAP, floor_to(static_cast<int>(cs->water_lod_radius_8._float()), LOD_SNAP));

		// A fade is never wider than the ring it runs in. The detail of the cells below 2 units fades out over the cells
		// of 1 unit, or over the outer half without them
		const float lod_blend = std::max(cs->water_lod_blend._float(), 0.0f);
		g_params.lod_fade[0] = std::min(lod_blend, static_cast<float>(g_params.lod_radius[0] - g_params.fine_radius[1]));
		g_params.lod_fade[1] = std::min(lod_blend, static_cast<float>(g_params.lod_radius[1] - g_params.lod_radius[0]));
		g_params.fine_fade[0] = std::min(lod_blend, 0.5f * static_cast<float>(g_params.fine_radius[0]));
		g_params.fine_fade[1] = std::min(lod_blend, g_params.fine_radius[0]
			? static_cast<float>(g_params.fine_radius[1] - g_params.fine_radius[0])
			: 0.5f * static_cast<float>(g_params.fine_radius[1]));

		water_dynamics::render_step(view_x, view_y, static_cast<float>(g_params.fine_radius[1]));

		const auto& frame = water_dynamics::frame();
		g_params.wakes = frame.wake_on;
		g_params.wind = frame.ocean_on;
		g_params.long_scale = frame.long_scale;
		g_params.short_scale = frame.short_scale;
		g_params.inland_level = cs->water_inland_level._float();
		g_params.inland_scale = std::max(cs->water_inland_wave_scale._float(), 0.0f);
		g_params.phys_to_game = 1.0f / std::max(g_params.normal_scale, 0.05f);

		g_params.valid = true;

		g_api.cull_pieces = cs->api_water_cull_pieces._bool();
		g_api.uv_scale = 0.01f * cs->water_texture_uv_scale._float();
		g_api.vertices.clear();
		g_api.indices.clear();
		g_api.detail_pieces = 0u;
		g_api.flat_rectangles = 0u;
		g_api.fine_vertices = 0u;

		g_stats.detail_radius = *game::pWaterDetailDist;
		g_stats.sim_rms = g_sim_rms;
		g_stats.far_wave_rms = g_params.far_waves ? g_far_wave_rms : 0.0f;
	}

	void water::on_render_end()
	{
		PROFILE_ZONE();
		if (!get()) {
			return;
		}

		g_list_buffer_pinned = -1;
		g_stats.renders++;

		if (g_list_buffer_at_begin >= 0 && *game::pWaterListBufferIndex != g_list_buffer_at_begin) {
			g_stats.list_flips++;
		}

		const bool submitted = g_api.active && !g_api.indices.empty() && !imgui::get()->m_dbg_do_not_render_water && api_submit();
		if (!submitted) {
			api_destroy_mesh();
		}

		// the api path was switched off: nothing references the materials anymore
		if (!g_api.active)
		{
			api_destroy_material(g_api.stale_material);
			api_destroy_material(g_api.material);
		}

		g_stats.api_active = g_api.active;
		g_stats.api_vertices = static_cast<std::uint32_t>(g_api.vertices.size());
		g_stats.api_triangles = static_cast<std::uint32_t>(g_api.indices.size() / 3u);
		g_stats.api_detail_pieces = g_api.detail_pieces;
		g_stats.api_flat_rectangles = g_api.flat_rectangles;
		g_stats.api_fine_vertices = g_api.fine_vertices;
		g_api.active = false;
	}

	water::material_s& water::material() {
		return g_material;
	}

	water::stats_s water::get_stats() {
		return g_stats;
	}

	void water::devgui_stats()
	{
		const auto s = get_stats();
		const auto d = water_dynamics::get_stats();

		const auto group = [](const char* name)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(name);
			};

		const auto cell = [](const char* label, const char* format, const auto... args)
			{
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", label);
				ImGui::SameLine(0.0f, 6.0f);
				ImGui::Text(format, args...);
			};

		if (ImGui::BeginTable("##water_stats", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
		{
			group("Mesh");
			cell("Radius", "%d", s.detail_radius);
			cell("Pieces", "%u", s.pieces);
			cell("Culled", "%u", s.pieces_culled);
			cell("Vertices", "%u", s.vertices);

			if (s.api_active)
			{
				group("Remix API");
				cell("Vertices", "%u", s.api_vertices);
				cell("Triangles", "%u", s.api_triangles);
				cell("Fine Vertices", "%u", s.api_fine_vertices);
				cell("Flat Rectangles", "%u", s.api_flat_rectangles);
			}

			group("Wave Height RMS");
			cell("Game", "%.3f", s.sim_rms);
			cell("Procedural", "%.3f", s.far_wave_rms);
			cell("Wake", "%.4f", d.wake_rms);
			cell("Ripple", "%.4f", d.ripple_rms);

			group("Wave Sources");
			cell("Vehicles", "%d (pool %s)", d.vehicles_seen, d.pool_ok ? "ok" : "-");
			cell("Hulls", "%d", d.hulls);
			cell("Bodies", "%d", d.bodies);
			cell("Helis", "%d", d.helis);

			group("Simulation");
			cell("Thread", "%.2f ms", d.sim_ms);
			cell("Wind", "%.2f", d.wind);
			cell("Splashes", "%d", d.splashes);
			cell("Explosions", "%d", d.explosions);

			ImGui::EndTable();
		}

		if (!s.api_active) {
			ImGui::TextUnformatted("Drawn through the fixed function path");
		}
	}

	void water::devgui_overview()
	{
		static const auto cs = comp_settings::get();
		static imgui::debug_plot sim_history;
		constexpr float MIN_PLOT_RANGE = 2.0f; // ms

		const auto s = get_stats();
		const bool simulated = cs->water_wind_waves._bool() || cs->water_interactive._bool();
		const float sim_ms = simulated ? water_dynamics::get_stats().sim_ms : 0.0f;
		sim_history.add(sim_ms);

		const float start_x = ImGui::GetCursorPosX();
		const float value_x = start_x + ImGui::CalcTextSize("Tessellated Radius").x + 24.0f;

		const auto label = [&](const char* name)
			{
				ImGui::TextDisabled("%s", name);
				ImGui::SameLine(value_x);
			};

		ImGui::BeginGroup();
		{
			label("Tessellated Radius");
			ImGui::Text("%d", s.detail_radius);

			label("Triangles");
			if (s.api_active) {
				ImGui::Text("%u", s.api_triangles);
			} else {
				ImGui::TextUnformatted("-");
			}

			label("Simulation Time");
			if (simulated) {
				ImGui::Text("%.2f ms", sim_ms);
			} else {
				ImGui::TextUnformatted("off");
			}

			ImGui::EndGroup();
		}

		const float plot_height = ImGui::GetItemRectSize().y;
		const float plot_max = std::max(*std::max_element(std::begin(sim_history.values), std::end(sim_history.values)) * 1.25f, MIN_PLOT_RANGE);

		ImGui::SameLine(value_x + ImGui::CalcTextSize("000.00 ms").x + 24.0f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.02f, 0.02f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::PlotLines("##water_sim_time", sim_history.values, imgui::debug_plot::history_size, sim_history.offset, nullptr, 0.0f, plot_max,
			ImVec2(ImGui::GetContentRegionAvail().x, plot_height));
		ImGui::PopStyleColor(2);

		char scale_top[32] = {};
		snprintf(scale_top, sizeof(scale_top), "%.1f ms", plot_max);

		const ImVec2 rect_min = ImGui::GetItemRectMin();
		const ImVec2 rect_max = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddText(ImVec2(rect_max.x - ImGui::CalcTextSize(scale_top).x - 4.0f, rect_min.y + 2.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), scale_top);
	}

	void water::devgui_debug()
	{
		const auto s = get_stats();

		ImGui::Text("Last water block scan: map blocks %u, open sea blocks %u (the game keeps at most 400)", s.blocks_map, s.blocks_sea);
		ImGui::Spacing(0, 4);

		if (CWater_GetRenderLists_og)
		{
			ImGui::Text("List buffers of %u renders: swapped during %u, swapped before %u", s.renders, s.list_flips, s.list_repicks);
			ImGui::TextDisabled("The game swaps its two water list buffers while the next frame is built.\n"
				"Each render reads the buffer that was scanned with its camera.");
		} else {
			ImGui::TextDisabled("List buffers are not pinned (address not found), water can cut out while the camera turns.");
		}
	}

	water::water()
	{
		if (!game::func_addr__CWater_ScanThroughBlocks || !game::func_addr__CWater_BlockHit || !game::func_addr__CWater_CalculateWavesForCoordinate
			|| !game::func_addr__CWater_RenderHighDetailWaterRectangle || !game::func_addr__CWater_DrawHighDetailPrimitive || !game::func_addr__CWater_RenderWaterRectangle
			|| !game::func_addr__grcViewport_IsSphereVisible || !game::pWaterDetailDist || !game::pWaterRenderBufferIndex
			|| !game::pWaterCameraCoordsRT || !game::pWaterDetailRangeRender || !game::pWaterDynamicHeightRT || !game::pCRenderPhase_sm_pCurrent)
		{
			shared::common::log("Water", "Missing game addresses, module disabled.", shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return;
		}

		shared::utils::hook::detour(game::func_addr__CWater_ScanThroughBlocks, &CWater_ScanThroughBlocks_hk, DETOUR_CAST(CWater_ScanThroughBlocks_og));
		shared::utils::hook::detour(game::func_addr__CWater_BlockHit, &CWater_BlockHit_hk, DETOUR_CAST(CWater_BlockHit_og));
		shared::utils::hook::detour(game::func_addr__CWater_CalculateWavesForCoordinate, &CWater_CalculateWavesForCoordinate_hk, DETOUR_CAST(CWater_CalculateWavesForCoordinate_og));
		shared::utils::hook::detour(game::func_addr__CWater_RenderHighDetailWaterRectangle, &CWater_RenderHighDetailWaterRectangle_hk, DETOUR_CAST(CWater_RenderHighDetailWaterRectangle_og));
		shared::utils::hook::detour(game::func_addr__CWater_RenderWaterRectangle, &CWater_RenderWaterRectangle_hk, DETOUR_CAST(CWater_RenderWaterRectangle_og));
		water_dynamics::init();

		if (game::func_addr__CWater_GetRenderLists && game::pWaterListBufferIndex && game::pWaterListBuffers) {
			shared::utils::hook::detour(game::func_addr__CWater_GetRenderLists, &CWater_GetRenderLists_hk, DETOUR_CAST(CWater_GetRenderLists_og));
		}

		p_this = this;
		sync_overrides();

		shared::common::log("Water", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
