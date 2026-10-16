#pragma once

namespace gta4
{
	/// Water surface tweaks below CWater::Render (fixed function water):
	/// - a larger tessellated area around the camera (DETAILEDWATERDIST) with coarser cells further out
	/// - a wider fade at the edge of the game's wave simulation (100 x 100 cells of 2 units around the camera)
	/// - procedural waves that continue the simulation and fade out towards the edge of the tessellated area
	/// - a longer reach of the water block scan (500 x 500 unit blocks in front of the camera)
	/// - optionally the whole water surface as one Remix API mesh per frame instead of the fixed function draw
	class water final : public shared::common::loader::component_module
	{
	public:
		water();

		static inline water* p_this = nullptr;
		static water* get() { return p_this; }

		struct stats_s
		{
			int detail_radius = 0;
			float sim_rms = 0.0f;           // measured height RMS of the game's simulation
			float far_wave_rms = 0.0f;      // target RMS of the procedural waves
			std::uint32_t pieces = 0u;      // tessellated pieces of the last water render
			std::uint32_t vertices = 0u;
			std::uint32_t pieces_culled = 0u;
			std::uint32_t blocks_map = 0u;  // blocks of the last water block scan
			std::uint32_t blocks_sea = 0u;  // open sea blocks (the game keeps at most 400)

			bool api_active = false;        // last water render went through the Remix API
			std::uint32_t api_vertices = 0u;
			std::uint32_t api_triangles = 0u;
			std::uint32_t api_detail_pieces = 0u;
			std::uint32_t api_flat_rectangles = 0u;
		};

		/// render thread, right before CWater::Render draws
		static void on_render_begin();

		/// render thread, when CWater::Render returns: submits the water mesh built during the render (Remix API path)
		static void on_render_end();

		/// translucent material of the Remix API water while no USD water material replaces it (dev tab, not saved)
		struct material_s
		{
			float ior = 1.33f;
			float transmittance_color[3] = { 0.62f, 0.8f, 0.8f };
			float transmittance_distance = 4.0f;

			bool operator==(const material_s&) const = default;
		};

		static material_s& material();

		static stats_s get_stats();
		static void devgui_stats();
	};
}
