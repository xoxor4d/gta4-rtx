#pragma once
#include "water_physics.hpp"

// Interactive water and wind sea (physics in water_physics.hpp)
// - game thread: update() collects what disturbs the water: vehicles, peds and objects near the player (entity pools),
//   the player's body, explosions, bullets and the wind
// - simulation thread: wave fields that follow the camera and the FFT wind sea, stepped by game time. Its heights are
//   also written into the game's wave grid, so boats and bodies float on the simulated surface
// - render thread: render_step() once per water render, then frame() for the heights of this render

namespace gta4::water_dynamics
{
	/// Installs the hooks. Called once from the water module.
	void init();

	/// Game thread, once per frame.
	void update();

	/// Render thread, before the water mesh is built: hands over the camera and takes the newest heights.
	/// @param bake_radius	half size of the area around the camera drawn with cells below 2 units, 0 without one
	void render_step(float cam_x, float cam_y, float bake_radius);

	// what the cells below 2 units add around the camera, baked by the simulation thread at 0.5 unit cells
	struct detail_s
	{
		// 0 - 2: wakes, ripples and rotor wash (height, slope x, slope y)
		// 3 - 5: short wind waves (height, slope x, slope y)
		// 6 - 7: horizontal displacement of the short wind waves (x, y)
		static constexpr int CHANNELS = 8;
		static constexpr float CELL = 0.5f;

		int n = 0;
		int ox = 0; // world cell index of cell [0][0]
		int oy = 0;
		const float* d = nullptr;

		bool valid() const {
			return d && n > 0;
		}

		/// Bilinear sample of all channels.
		/// @return false outside of the baked area
		bool sample(const float x, const float y, float out[CHANNELS]) const
		{
			if (!valid()) {
				return false;
			}

			const float fx = x / CELL - static_cast<float>(ox);
			const float fy = y / CELL - static_cast<float>(oy);
			const int i = static_cast<int>(std::floor(fx));
			const int j = static_cast<int>(std::floor(fy));

			if (i < 0 || j < 0 || i >= n - 1 || j >= n - 1) {
				return false;
			}

			const float tx = fx - static_cast<float>(i);
			const float ty = fy - static_cast<float>(j);
			const float* c00 = d + (static_cast<size_t>(j) * n + i) * CHANNELS;
			const float* c10 = c00 + CHANNELS;
			const float* c01 = c00 + static_cast<size_t>(n) * CHANNELS;
			const float* c11 = c01 + CHANNELS;

			for (int c = 0; c < CHANNELS; c++)
			{
				const float a = c00[c] + (c10[c] - c00[c]) * tx;
				const float b = c01[c] + (c11[c] - c01[c]) * tx;
				out[c] = a + (b - a) * ty;
			}

			return true;
		}
	};

	// the wind sea at whole unit points, one periodic 128 x 128 tile per band: height, slope x, slope y
	struct ocean_nodes_s
	{
		static constexpr int N = 128;
		static constexpr int CHANNELS = 3;

		const float* long_band = nullptr; // waves of 16 units and longer
		const float* mid_band = nullptr;  // 4 - 16 units

		bool valid() const {
			return long_band && mid_band;
		}

		static const float* at(const float* band, const int x, const int y) {
			return band + (static_cast<size_t>(y & (N - 1)) * N + (x & (N - 1))) * CHANNELS;
		}
	};

	struct frame_s
	{
		water_physics::grid_view_s wake_lattice[3]; // wakes and blast waves as cells of 2, 4 and 8 units can show them
		water_physics::grid_view_s shore;           // 1 - share of the wind sea left near shores, 96 units around the camera
		water_physics::grid_view_s shore_far;       // the same at 4 unit cells, 384 units around the camera
		water_physics::grid_view_s beach;           // how far the surface sinks into the sand on beaches
		detail_s detail;
		ocean_nodes_s ocean_nodes;
		bool wake_on = false;
		bool ocean_on = false;
		float long_scale = 1.0f;                    // water_wind_wave_long_scale
		float short_scale = 1.0f;                   // water_wind_wave_short_scale
		float ocean_hs = 0.0f;                      // significant wave height of the wind sea (m)
		float wake_rms = 0.0f;
		float ripple_rms = 0.0f;
	};

	/// Render thread: the heights of this render (empty until the simulation published its first update).
	const frame_s& frame();

	/// Share of the wind sea left at a point: above 1 where waves steepen near a shore, towards 0 where they break,
	/// 1 on open water. The close map is used where it has data, the wide one around it.
	inline float shore_wave_share(const water_physics::grid_view_s& close_map, const water_physics::grid_view_s& wide_map, const float x, const float y)
	{
		// the value of a cell belongs to its centre
		constexpr float BLEND = 8.0f;
		const float close_x = x - 0.5f * close_map.dx;
		const float close_y = y - 0.5f * close_map.dx;
		const float close_weight = water_physics::clamp01(close_map.edge_distance(close_x, close_y) / BLEND);
		float lost = 0.0f;
		float unused_dx, unused_dy;

		if (close_weight > 0.0f)
		{
			close_map.sample(close_x, close_y, lost, unused_dx, unused_dy);
			lost *= close_weight;
		}

		if (close_weight < 1.0f)
		{
			float wide = 0.0f;
			wide_map.sample(x - 0.5f * wide_map.dx, y - 0.5f * wide_map.dx, wide, unused_dx, unused_dy);
			lost += wide * (1.0f - close_weight);
		}

		return 1.0f - lost;
	}

	/// True while the simulated heights are written into the game's wave grid. The game's own simulation is replaced
	/// then and must not be drawn on top.
	bool coupling_active();

	struct stats_s
	{
		int hulls = 0;
		int bodies = 0;
		int helis = 0;          // low enough over water for rotor wash
		int explosions = 0;     // near water, since start
		int splashes = 0;       // since start
		int vehicles_seen = 0;  // live vehicles in the pool
		float wind = 0.0f;
		float ocean_hs = 0.0f;
		float wake_rms = 0.0f;
		float ripple_rms = 0.0f;
		float sim_ms = 0.0f;    // wall time of the last simulation update
		bool pool_ok = false;   // vehicle pool slots match the script handles
	};

	stats_s get_stats();
}
