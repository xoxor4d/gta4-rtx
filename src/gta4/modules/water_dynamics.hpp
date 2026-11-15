#pragma once
#include "water_physics.hpp"

namespace gta4::water_dynamics
{
	/// Interactive water and wind sea (physics in water_physics.hpp):
	/// - game thread: update() collects the sources: vehicles near the player (vehicle pool), the player in the water,
	///   explosions (CExplosion::AddExplosion) and the wind
	/// - simulation thread: wave fields that follow the camera and the FFT wind sea, stepped by game time,
	///   published triple buffered. Our heights are also written into the game's wave grid so buoyancy follows them
	/// - render thread: render_step() once per water render, then frame() for the heights of this render

	/// hooks, once from the water module
	void init();

	/// game thread, once per frame
	void update();

	/// render thread, before the water mesh is built: hands over the camera and takes the newest heights
	void render_step(float cam_x, float cam_y);

	/// the fine mesh's detail around the camera, baked by the simulation thread at the mesh's 0.5 unit cells:
	/// height, slope x, slope y, displacement x, displacement y per cell (one bilinear lookup per vertex)
	struct detail_s
	{
		static constexpr int CHANNELS = 5;
		static constexpr float CELL = 0.5f;
		int n = 0;
		int ox = 0, oy = 0; // world cell index of cell [0][0]
		const float* d = nullptr;

		bool valid() const { return d && n > 0; }

		/// false outside of the baked area
		bool sample(const float x, const float y, float out[CHANNELS]) const
		{
			if (!valid()) {
				return false;
			}

			const float fx = x / CELL - static_cast<float>(ox), fy = y / CELL - static_cast<float>(oy);
			const int i = static_cast<int>(std::floor(fx)), j = static_cast<int>(std::floor(fy));
			if (i < 0 || j < 0 || i >= n - 1 || j >= n - 1) {
				return false;
			}

			const float tx = fx - static_cast<float>(i), ty = fy - static_cast<float>(j);
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

	/// the wind sea at whole unit nodes (the lattice's points), one 128 x 128 tile per band: height, slope x, slope y
	struct ocean_nodes_s
	{
		static constexpr int N = 128;
		static constexpr int CHANNELS = 3;
		const float* long_band = nullptr;
		const float* mid_band = nullptr;

		bool valid() const { return long_band && mid_band; }

		static const float* at(const float* band, const int x, const int y) {
			return band + (static_cast<size_t>(y & (N - 1)) * N + (x & (N - 1))) * CHANNELS;
		}
	};

	struct frame_s
	{
		water_physics::grid_view_s wake;         // 256 m at 1 m: wakes, blast waves, rotor wash (camera centred)
		water_physics::grid_view_s ripple;       // 128 m at 0.5 m: swimming, wading, splashes (camera centred)
		water_physics::grid_view_s ocean_long;   // wind sea tile (128 m, periodic): waves >= 16 m
		water_physics::grid_view_s ocean_mid;    // 4 - 16 m
		water_physics::grid_view_s ocean_short;  // < 4 m (fine mesh only)
		water_physics::grid_view_s ocean_disp_x; // choppy horizontal displacement of the waves below 16 m (fine mesh)
		water_physics::grid_view_s ocean_disp_y;
		water_physics::grid_view_s shore;        // 1 - wind sea factor near shores (shoaling > 1, breaking -> 0), 96 m at 1 m
		water_physics::grid_view_s beach;        // how far the surface sinks into the sand where the bottom is < 0.2 m down
		water_physics::grid_view_s wake_lattice[3]; // the wake as the lattice steps 2, 4 and 8 can show it (1-2-1 filtered)
		detail_s detail;                         // fine mesh detail around the camera
		ocean_nodes_s ocean_nodes;               // wind sea at the lattice's whole unit points
		bool ocean_on = false;
		float ocean_hs = 0.0f;                   // significant wave height of the wind sea (m)
		float wake_rms = 0.0f;
		float ripple_rms = 0.0f;
	};

	/// render thread: the heights of this render (empty views until the first publish)
	const frame_s& frame();

	/// our heights are written into the game's wave grid, the game's own simulation must not be drawn on top
	bool coupling_active();

	struct stats_s
	{
		int hulls = 0;
		int bodies = 0;
		int helis = 0;          // low enough over water for downwash
		int explosions = 0;     // near water, since start
		int splashes = 0;       // since start
		int vehicles_seen = 0;  // live vehicles in the pool
		float wind = 0.0f;
		float ocean_hs = 0.0f;
		float wake_rms = 0.0f;
		float ripple_rms = 0.0f;
		float sim_ms = 0.0f;    // wall time of the last simulation update
		bool pool_ok = false;
	};

	stats_s get_stats();
}
