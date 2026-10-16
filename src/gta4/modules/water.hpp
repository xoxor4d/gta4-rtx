#pragma once

namespace gta4
{
	/// Water surface tweaks below CWater::Render (fixed function water):
	/// - a larger tessellated area around the camera (DETAILEDWATERDIST) with coarser cells further out
	/// - a wider fade at the edge of the game's wave simulation (100 x 100 cells of 2 units around the camera)
	/// - procedural waves that continue the simulation and fade out towards the edge of the tessellated area
	/// - a longer reach of the water block scan (500 x 500 unit blocks in front of the camera)
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
		};

		/// render thread, right before CWater::Render draws
		static void on_render_begin();

		static stats_s get_stats();
		static void devgui_stats();
	};
}
