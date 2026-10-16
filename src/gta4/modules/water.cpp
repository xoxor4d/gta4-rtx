#include "std_include.hpp"
#include "water.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"

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
		constexpr float GRAVITY = 9.81f;

		constexpr std::uint32_t RENDER_PHASE_FAR_DISTANCE = 0x374;

		DETOUR_TYPEDEF(CWater_ScanThroughBlocks, void, __cdecl, int phase_index);
		DETOUR_TYPEDEF(CWater_BlockHit, void, __cdecl, int block_x, int block_y);
		DETOUR_TYPEDEF(CWater_CalculateWavesForCoordinate, int, __cdecl, int x, int y, int unused1, int unused2, float* z, float* normal, DWORD* color);
		DETOUR_TYPEDEF(CWater_RenderHighDetailWaterRectangle, int, __cdecl, int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);

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

			bool lod = false;
			int lod_center_x = 0;
			int lod_center_y = 0;
			int lod_radius[2] = {};
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

		void build_waves(const decltype(comp_settings::get()) gs)
		{
			const float base_length = std::max(gs->water_far_wave_length.get_as<float>(), 0.5f);
			const float speed = gs->water_far_wave_speed.get_as<float>();
			const float direction = gs->water_far_wave_direction.get_as<float>();

			float rms_unit = 0.0f;
			for (int i = 0; i < WAVE_COUNT; i++) {
				const float length = base_length * WAVE_LENGTH_RATIO[i];
				rms_unit += length * length * 0.5f;
			}
			rms_unit = std::sqrt(rms_unit);

			g_far_wave_rms = g_sim_rms * std::max(gs->water_far_wave_scale.get_as<float>(), 0.0f);
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

		sample_s sample_combined(const int x, const int y, DWORD* color)
		{
			float sim_weight = 0.0f;
			sample_s s = sample_sim(x, y, color, sim_weight);

			if (g_params.far_waves && sim_weight < 1.0f)
			{
				const float weight = (1.0f - sim_weight) * smoothstep_to(g_params.far_wave_fade, static_cast<float>(window_edge_distance(x, y)));
				if (weight > 0.0f)
				{
					const auto farwave = sample_far_waves(x, y);
					s.h += farwave.h * weight;
					s.gx += farwave.gx * weight;
					s.gy += farwave.gy * weight;
				}
			}

			return s;
		}

		// height on the lattice of a cell size, bilinear in between: along axis aligned edges it is linear, so pieces of
		// different cell sizes meet without cracks
		float lattice_height(const int x, const int y, const int step)
		{
			const int x0 = floor_to(x, step);
			const int y0 = floor_to(y, step);

			if (x0 == x && y0 == y) {
				return sample_combined(x, y, nullptr).h;
			}

			const float tx = static_cast<float>(x - x0) / step;
			const float ty = static_cast<float>(y - y0) / step;
			const float h00 = sample_combined(x0, y0, nullptr).h;
			const float h10 = tx > 0.0f ? sample_combined(x0 + step, y0, nullptr).h : h00;
			const float h01 = ty > 0.0f ? sample_combined(x0, y0 + step, nullptr).h : h00;
			const float h11 = tx > 0.0f && ty > 0.0f ? sample_combined(x0 + step, y0 + step, nullptr).h : (tx > 0.0f ? h10 : h01);

			return (h00 * (1.0f - tx) + h10 * tx) * (1.0f - ty) + (h01 * (1.0f - tx) + h11 * tx) * ty;
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

		float vertex_height(const int x, const int y, const float fine_height)
		{
			if (!g_params.lod) {
				return fine_height;
			}

			const int cheb = lod_cheb_distance(x, y);
			const int ring = lod_ring(cheb);
			const int step = lod_step(ring);

			float h = (x % step || y % step) ? lattice_height(x, y, step) : fine_height;

			if (ring < 2)
			{
				// towards the outer border the height follows the next ring's lattice, on the border it matches it exactly
				const int next_step = lod_step(ring + 1);
				const float t = 1.0f - smoothstep_to(static_cast<float>(2 * next_step), static_cast<float>(g_params.lod_radius[ring] - cheb));

				if (t > 0.0f) {
					h += (lattice_height(x, y, next_step) - h) * t;
				}
			}

			return h;
		}

		int __cdecl CWater_CalculateWavesForCoordinate_hk(int x, int y, int unused1, int unused2, float* z, float* normal, DWORD* color)
		{
			if (!g_params.valid || bypassed()) {
				return CWater_CalculateWavesForCoordinate_og(x, y, unused1, unused2, z, normal, color);
			}

			const auto s = sample_combined(x, y, color);
			*z += vertex_height(x, y, s.h);

			const float gx = s.gx * g_params.normal_scale;
			const float gy = s.gy * g_params.normal_scale;
			const float inv_length = 1.0f / std::sqrt(gx * gx + gy * gy + 1.0f);

			normal[0] = gx * inv_length;
			normal[1] = gy * inv_length;
			normal[2] = inv_length;
			return 0;
		}

		void draw_piece(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4);

		// x / y split of a piece, corner heights (x1 y1, x2 y1, x1 y2, x2 y2) interpolated like CWater::SplitWaterRectangleAlong*Line
		void split_x(const int s, const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const float t = static_cast<float>(s - x1) / static_cast<float>(x2 - x1);
			const float za = z1 + (z2 - z1) * t;
			const float zb = z3 + (z4 - z3) * t;

			draw_piece(x1, s, y1, y2, z1, za, z3, zb);
			draw_piece(s, x2, y1, y2, za, z2, zb, z4);
		}

		void split_y(const int s, const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const float t = static_cast<float>(s - y1) / static_cast<float>(y2 - y1);
			const float za = z1 + (z3 - z1) * t;
			const float zb = z2 + (z4 - z2) * t;

			draw_piece(x1, x2, y1, s, z1, z2, za, zb);
			draw_piece(x1, x2, s, y2, za, zb, z3, z4);
		}

		bool strictly_inside(const int v, const int a, const int b) {
			return v > std::min(a, b) && v < std::max(a, b);
		}

		// replaces CWater::RenderHighDetailWaterRectangle while the LOD is on: pieces are split at the ring borders and aligned to
		// the cell size of their ring, so neighbouring pieces share their border vertices or the lattice the heights follow
		void draw_piece(const int x1, const int x2, const int y1, const int y2, const float z1, const float z2, const float z3, const float z4)
		{
			const float half_x = static_cast<float>(x2 - x1) * 0.5f;
			const float half_y = static_cast<float>(y2 - y1) * 0.5f;
			const auto is_sphere_visible = reinterpret_cast<IsSphereVisible_t>(game::func_addr__grcViewport_IsSphereVisible);

			if (!is_sphere_visible(game::pCurrentViewport->wp, static_cast<float>(x1 + x2) * 0.5f, static_cast<float>(y1 + y2) * 0.5f, z1,
				std::sqrt(half_x * half_x + half_y * half_y), nullptr))
			{
				g_pieces_culled++;
				return;
			}

			for (int r = 0; r < 2; r++)
			{
				const int lines[2] = { g_params.lod_center_x - g_params.lod_radius[r], g_params.lod_center_x + g_params.lod_radius[r] };
				for (const int line : lines)
				{
					if (strictly_inside(line, x1, x2))
					{
						split_x(line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}

				const int lines_y[2] = { g_params.lod_center_y - g_params.lod_radius[r], g_params.lod_center_y + g_params.lod_radius[r] };
				for (const int line : lines_y)
				{
					if (strictly_inside(line, y1, y2))
					{
						split_y(line, x1, x2, y1, y2, z1, z2, z3, z4);
						return;
					}
				}
			}

			const int ring = lod_ring(lod_cheb_distance((x1 + x2) / 2, (y1 + y2) / 2));
			int step = lod_step(ring);

			// align to the lattice of the ring, the remainder strips get a finer cell size
			const int min_x = std::min(x1, x2);
			const int max_x = std::max(x1, x2);
			const int min_y = std::min(y1, y2);
			const int max_y = std::max(y1, y2);

			if (const int a = ceil_to(min_x, step); strictly_inside(a, x1, x2) && a != min_x)
			{
				split_x(a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = floor_to(max_x, step); strictly_inside(a, x1, x2) && a != max_x)
			{
				split_x(a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = ceil_to(min_y, step); strictly_inside(a, y1, y2) && a != min_y)
			{
				split_y(a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			if (const int a = floor_to(max_y, step); strictly_inside(a, y1, y2) && a != max_y)
			{
				split_y(a, x1, x2, y1, y2, z1, z2, z3, z4);
				return;
			}

			while (step > 2 && (min_x % step || max_x % step || min_y % step || max_y % step)) {
				step >>= 1;
			}

			const int polys_x = std::max((max_x - min_x) / step, 1);
			const int polys_y = std::max((max_y - min_y) / step, 1);

			if (polys_x > MAX_POLYS_PER_SIDE || polys_y > MAX_POLYS_PER_SIDE)
			{
				if (polys_x >= polys_y) {
					split_x(min_x + (polys_x / 2) * step, x1, x2, y1, y2, z1, z2, z3, z4);
				} else {
					split_y(min_y + (polys_y / 2) * step, x1, x2, y1, y2, z1, z2, z3, z4);
				}

				return;
			}

			g_pieces++;
			g_vertices += static_cast<std::uint32_t>((polys_x + 1) * (polys_y + 1));

			const auto draw = reinterpret_cast<DrawHighDetailPrimitive_t>(game::func_addr__CWater_DrawHighDetailPrimitive);
			draw(x1, x2, y1, y2, z1, z2, z3, z4, 2 * polys_x * polys_y, (polys_x + 1) * (polys_y + 1), polys_x, polys_y);
		}

		int __cdecl CWater_RenderHighDetailWaterRectangle_hk(int x1, int x2, int y1, int y2, float z1, float z2, float z3, float z4)
		{
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
			static const auto gs = comp_settings::get();

			if (bypassed())
			{
				*game::pWaterDetailDist = GAME_DETAIL_RADIUS;
				CWater_ScanThroughBlocks_og(phase_index);
				return;
			}

			*game::pWaterDetailDist = gs->water_lod.get_as<bool>()
				? std::clamp(static_cast<int>(gs->water_detail_radius.get_as<float>()), GAME_DETAIL_RADIUS, 600)
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

			far_distance = saved * std::clamp(gs->water_block_distance_scale.get_as<float>(), 0.1f, 10.0f);
			CWater_ScanThroughBlocks_og(phase_index);
			far_distance = saved;

			g_stats.blocks_map = g_blocks_map;
			g_stats.blocks_sea = g_blocks_sea;
		}
	}

	void water::on_render_begin()
	{
		static const auto gs = comp_settings::get();

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
		g_params.sim_edge_fade = std::max(gs->water_sim_edge_fade.get_as<float>(), 1.0f);
		g_params.normal_scale = std::max(gs->water_vertex_normal_scale.get_as<float>(), 0.0f);

		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		if (!g_qpc_frequency.QuadPart)
		{
			QueryPerformanceFrequency(&g_qpc_frequency);
			g_qpc_start = now;
		}
		g_params.time = static_cast<double>(now.QuadPart - g_qpc_start.QuadPart) / static_cast<double>(g_qpc_frequency.QuadPart);

		update_sim_rms(buffer);
		g_params.far_waves = gs->water_far_waves.get_as<bool>();
		g_params.far_wave_fade = std::max(gs->water_far_wave_fade.get_as<float>(), 0.0f);
		build_waves(gs);

		g_params.lod = gs->water_lod.get_as<bool>();
		g_params.lod_center_x = floor_to(static_cast<int>(cam_x), LOD_SNAP);
		g_params.lod_center_y = floor_to(static_cast<int>(cam_y), LOD_SNAP);
		g_params.lod_radius[0] = std::max(LOD_SNAP, floor_to(static_cast<int>(gs->water_lod_radius_4.get_as<float>()), LOD_SNAP));
		g_params.lod_radius[1] = std::max(g_params.lod_radius[0] + LOD_SNAP, floor_to(static_cast<int>(gs->water_lod_radius_8.get_as<float>()), LOD_SNAP));

		g_params.valid = true;

		g_stats.detail_radius = *game::pWaterDetailDist;
		g_stats.sim_rms = g_sim_rms;
		g_stats.far_wave_rms = g_params.far_waves ? g_far_wave_rms : 0.0f;
	}

	water::stats_s water::get_stats() {
		return g_stats;
	}

	void water::devgui_stats()
	{
		const auto s = get_stats();

		ImGui::Text("Tessellated radius %d, last render: %u pieces (%u culled), %u vertices", s.detail_radius, s.pieces, s.pieces_culled, s.vertices);
		ImGui::Text("Wave height RMS: game simulation %.3f, procedural waves %.3f", s.sim_rms, s.far_wave_rms);
		ImGui::Text("Last water block scan: map blocks %u, open sea blocks %u (the game keeps at most 400)", s.blocks_map, s.blocks_sea);
	}

	water::water()
	{
		if (!game::func_addr__CWater_ScanThroughBlocks || !game::func_addr__CWater_BlockHit || !game::func_addr__CWater_CalculateWavesForCoordinate
			|| !game::func_addr__CWater_RenderHighDetailWaterRectangle || !game::func_addr__CWater_DrawHighDetailPrimitive
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

		p_this = this;

		shared::common::log("Water", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
