#include "std_include.hpp"
#include "water_dynamics.hpp"

#include "comp_settings.hpp"
#include "natives.hpp"
#include "remix_vars.hpp"

#include <array>

namespace gta4::water_dynamics
{
	namespace
	{
		// the wave fields are propagated exactly, so 30 Hz shows the same waves as 60 Hz
		constexpr float STEP = 1.0f / 30.0f;
		constexpr float PI = 3.14159265f;
		constexpr float DEG_TO_RAD = PI / 180.0f;
		constexpr float RHO_AIR = 1.225f; // kg/m3

		// comp settings the simulation thread needs, read on the render thread
		struct settings_s
		{
			bool wind_waves = false;
			bool interactive = false;
			float wind_scale = 1.0f;
			float long_scale = 1.0f;
			float short_scale = 1.0f;
			float swell_base = 1.0f;
			float swell_scale = 1.0f;
			float wind_direction = 0.0f; // radians
			float inland_level = 0.5f;
			float inland_scale = 1.0f;
			float heli_strength = 1.0f;  // rotor wash
			float bake_radius = 0.0f;    // area drawn with cells below 2 units, 0 without one

			/// Height multiplier of the wind waves on water at this level.
			float wind_gain(const float water_level) const {
				return water_level > inland_level ? inland_scale : 1.0f;
			}
		};

		settings_s read_settings(const float bake_radius)
		{
			static const auto cs = comp_settings::get();

			settings_s s;
			s.wind_waves = cs->water_wind_waves._bool();
			s.interactive = cs->water_interactive._bool();
			s.wind_scale = std::max(cs->water_wind_wave_scale._float(), 0.0f);
			s.long_scale = std::max(cs->water_wind_wave_long_scale._float(), 0.0f);
			s.short_scale = std::max(cs->water_wind_wave_short_scale._float(), 0.0f);
			s.swell_base = std::max(cs->water_wind_swell_base._float(), 0.0f);
			s.swell_scale = std::max(cs->water_wind_swell_scale._float(), 0.0f);
			s.wind_direction = cs->water_wind_wave_direction._float() * DEG_TO_RAD;
			s.inland_level = cs->water_inland_level._float();
			s.inland_scale = std::max(cs->water_inland_wave_scale._float(), 0.0f);
			s.heli_strength = std::clamp(cs->water_interactive_strength._float() * cs->water_interactive_heli_scale._float(), 0.0f, 4.0f);
			s.bake_radius = s.wind_waves || s.interactive ? std::max(bake_radius, 0.0f) : 0.0f;
			return s;
		}

		// what disturbs the water, handed from the game thread to the simulation thread (metres, m/s)
		namespace sources
		{
			// footprint of something that floats. The game's buoyancy must not see the dent the hull presses itself
			struct hull_s
			{
				float x, y, dirx, diry, length, beam;
				float water_level;
			};

			// surface pressure of something in the water, given as the depth of water it holds down
			struct body_s
			{
				float x, y, radius, depth;
				bool wake = false; // on the 1 m wake field, otherwise on the 0.5 m ripple field
			};

			struct heli_s
			{
				float x, y;     // where the rotor's axis meets the water
				float altitude; // of the skids above the water, along the axis
				float radius;   // rotor
				float mass;     // kg, times the share of its weight the rotor carries right now
			};

			// the surface is struck down at speed over a patch of radius
			struct splash_s
			{
				float x, y, radius, speed;
			};

			struct blast_s
			{
				float x, y, radius, height;
			};
		}

		// depth map scanned on the game thread, see depth::grid_s
		struct depth_map_s
		{
			std::vector<float> depth;
			int ox = 0;
			int oy = 0;
			std::uint32_t version = 0u;
		};

		// everything the simulation thread takes over at the start of an update
		struct input_s
		{
			std::vector<sources::hull_s> hulls;       // latest game frame
			std::vector<sources::body_s> bodies;      // latest game frame
			std::vector<sources::heli_s> helis;       // latest game frame
			std::vector<sources::splash_s> splashes;  // queued until the simulation thread takes them
			std::vector<sources::blast_s> blasts;
			float game_dt = 0.0f;                     // game time not yet simulated (0 while paused)
			float cam_x = 0.0f;
			float cam_y = 0.0f;
			bool have_cam = false;
			bool reset = false;                       // interactive water was switched: start calm
			float player_x = 0.0f;
			float player_y = 0.0f;
			float water_level = 0.0f;                 // level of the water around the player
			settings_s settings;
			depth_map_s depth_close;                  // 1 unit cells
			depth_map_s depth_wide;                   // 4 unit cells
		};

		std::mutex g_input_mutex;
		std::condition_variable g_input_cv;
		input_s g_input;
		bool g_input_pending = false; // a water render asked for an update

		std::atomic<float> g_wind = 0.0f;
		std::atomic<float> g_sim_ms = 0.0f;
		stats_s g_stats;
		std::mutex g_stats_mutex;

		// The game's 100 x 100 wave grid (2 unit cells around the local player) is what buoyancy reads. The simulated heights
		// are written into it after each of its steps, the game's own simulation is replaced while that happens
		namespace coupling
		{
			constexpr int CELLS = 100;
			constexpr int CELL_SIZE = 2;

			struct grid_s
			{
				std::array<float, CELLS * CELLS> heights = {};
				int base_x = 0; // world position of the grid when it was filled
				int base_y = 0;
			};

			std::mutex mutex;
			grid_s grid;
			bool hooked = false;
			std::atomic<bool> active = false;

			/// Index of the cell at world position (x, y), the game wraps its grid around.
			size_t cell_index(const int x, const int y)
			{
				const int ix = (x / CELL_SIZE + 100000) % CELLS;
				const int iy = (y / CELL_SIZE + 100000) % CELLS;
				return static_cast<size_t>(ix) * CELLS + iy;
			}

			DETOUR_TYPEDEF(CWater_UpdateDynamicWater, void, __cdecl, void* params);

			/// Runs as a task on a worker thread, the game waits for it before the frame ends.
			void __cdecl CWater_UpdateDynamicWater_hk(void* params)
			{
				CWater_UpdateDynamicWater_og(params);

				if (!active.load()) {
					return;
				}

				std::lock_guard lock(mutex);
				const int base_x = *game::pWaterDynamicBaseX;
				const int base_y = *game::pWaterDynamicBaseY;

				if (base_x == grid.base_x && base_y == grid.base_y) {
					std::memcpy(game::pWaterDynamicHeights, grid.heights.data(), sizeof(float) * grid.heights.size());
				}
				else
				{
					// the grid moved with the player since it was filled: cells that came in have no height yet
					for (int kx = 0; kx < CELLS; kx++)
					{
						const int x = base_x + CELL_SIZE * kx;
						const bool x_known = x >= grid.base_x && x < grid.base_x + CELL_SIZE * CELLS;

						for (int ky = 0; ky < CELLS; ky++)
						{
							const int y = base_y + CELL_SIZE * ky;
							const bool known = x_known && y >= grid.base_y && y < grid.base_y + CELL_SIZE * CELLS;
							const size_t idx = cell_index(x, y);
							game::pWaterDynamicHeights[idx] = known ? grid.heights[idx] : 0.0f;
						}
					}
				}

				// the simulated fields own the motion, nothing is left for the game's step
				std::memset(game::pWaterDynamicVelocities, 0, sizeof(float) * grid.heights.size());
			}

			/// Game thread: height of the waves in the game's grid at a world position, 0 outside of the grid.
			/// This is the surface the game floats things on.
			float wave_height(const float x, const float y)
			{
				if (!hooked) {
					return 0.0f;
				}

				const int base_x = *game::pWaterDynamicBaseX;
				const int base_y = *game::pWaterDynamicBaseY;
				const float fx = (x - static_cast<float>(base_x)) / CELL_SIZE;
				const float fy = (y - static_cast<float>(base_y)) / CELL_SIZE;
				const int i = static_cast<int>(std::floor(fx));
				const int j = static_cast<int>(std::floor(fy));

				if (i < 0 || j < 0 || i >= CELLS - 1 || j >= CELLS - 1) {
					return 0.0f;
				}

				const auto at = [&](const int ci, const int cj)
					{
						return game::pWaterDynamicHeights[cell_index(base_x + CELL_SIZE * ci, base_y + CELL_SIZE * cj)];
					};

				const float tx = fx - static_cast<float>(i);
				const float ty = fy - static_cast<float>(j);
				const float low = at(i, j) + (at(i + 1, j) - at(i, j)) * tx;
				const float high = at(i, j + 1) + (at(i + 1, j + 1) - at(i, j + 1)) * tx;
				return low + (high - low) * ty;
			}

			void init()
			{
				if (!game::func_addr__CWater_UpdateDynamicWater || !game::pWaterDynamicHeights || !game::pWaterDynamicVelocities
					|| !game::pWaterDynamicBaseX || !game::pWaterDynamicBaseY)
				{
					return;
				}

				shared::utils::hook::detour(game::func_addr__CWater_UpdateDynamicWater, &CWater_UpdateDynamicWater_hk, DETOUR_CAST(CWater_UpdateDynamicWater_og));
				hooked = CWater_UpdateDynamicWater_og != nullptr;
			}
		}

		// what the simulation thread publishes for the render thread. Three buffers: one being read, the newest complete
		// one and one to write into
		namespace output
		{
			struct buffer_s
			{
				std::vector<float> wake_lattice[3];
				std::vector<float> nodes_long;
				std::vector<float> nodes_mid;
				std::vector<float> shore;
				std::vector<float> shore_far;
				std::vector<float> beach;
				std::vector<float> detail;
				frame_s frame;
			};

			buffer_s buffers[3];
			std::mutex mutex;
			int latest = -1; // newest complete buffer
			int front = -1;  // read by the render thread this frame
			frame_s front_frame;

			/// @return index of a buffer the render thread does not use, -1 if there is none
			int pick_free()
			{
				std::lock_guard lock(mutex);
				for (int i = 0; i < 3; i++)
				{
					if (i != latest && i != front) {
						return i;
					}
				}

				return -1;
			}

			void publish(const int idx)
			{
				std::lock_guard lock(mutex);
				latest = idx;
			}

			/// Render thread: holds the newest buffer until the next call.
			void take_newest()
			{
				std::lock_guard lock(mutex);
				front = latest;
				front_frame = front >= 0 ? buffers[front].frame : frame_s {};
			}
		}

		// simulation thread: owns the wave fields and the wind sea
		namespace sim
		{
			using water_physics::grid_view_s;
			using water_physics::clamp01;

			/// 1-2-1 filter along both axes of an n x n grid: removes the waves of two cells, which a mesh of the same cell
			/// size shows as spikes (response cos^2(k dx / 2): 0 at 2 cells, 0.5 at 4, 0.85 at 8).
			/// @param s	distance of the filter taps in cells
			void smooth_121(std::vector<float>& v, const int n, const bool periodic, const int s = 1)
			{
				if (v.size() != static_cast<size_t>(n) * n) {
					return;
				}

				static thread_local std::vector<float> tmp;
				tmp.resize(v.size());

				const auto at = [&](const int i)
					{
						return periodic ? (i + n) % n : std::clamp(i, 0, n - 1);
					};

				// along the rows into tmp
				for (int j = 0; j < n; j++)
				{
					const float* r = v.data() + static_cast<size_t>(j) * n;
					float* d = tmp.data() + static_cast<size_t>(j) * n;

					for (int i = 0; i < std::min(s, n); i++) {
						d[i] = 0.25f * r[at(i - s)] + 0.5f * r[i] + 0.25f * r[at(i + s)];
					}

					for (int i = s; i < n - s; i++) {
						d[i] = 0.25f * r[i - s] + 0.5f * r[i] + 0.25f * r[i + s];
					}

					for (int i = std::max(n - s, s); i < n; i++) {
						d[i] = 0.25f * r[at(i - s)] + 0.5f * r[i] + 0.25f * r[at(i + s)];
					}
				}

				// along the columns back into v
				for (int j = 0; j < n; j++)
				{
					const float* a = tmp.data() + static_cast<size_t>(at(j - s)) * n;
					const float* b = tmp.data() + static_cast<size_t>(j) * n;
					const float* c = tmp.data() + static_cast<size_t>(at(j + s)) * n;
					float* d = v.data() + static_cast<size_t>(j) * n;

					for (int i = 0; i < n; i++) {
						d[i] = 0.25f * a[i] + 0.5f * b[i] + 0.25f * c[i];
					}
				}
			}

			grid_view_s ocean_view(const std::vector<float>& h)
			{
				constexpr float CELL = water_physics::ocean_s::L / water_physics::ocean_s::N;
				return { water_physics::ocean_s::N, CELL, 0, 0, true, h.data() };
			}

			// What a depth map does to the wind sea near shores.
			// Depth values: -1 = no water, 0 = dry (land or a deck at the water line), NaN = not scanned yet, else metres
			struct shore_map_s
			{
				static constexpr int N = 96;
				static constexpr float NO_WATER = -1.0f;

				float cell = 1.0f;       // world units per cell
				bool with_beach = false; // also lowers the surface into the sand on beaches

				std::vector<float> depth;
				int ox = 0;
				int oy = 0;
				std::uint32_t version = 0u;
				bool dirty = false;
				float built_hs = -1.0f;

				// share of the wind sea lost (1 - share left) and how far the surface sinks into the sand:
				// what the depth map asks for, and what is shown (eased towards it)
				std::vector<float> lost_target;
				std::vector<float> beach_target;
				std::vector<float> lost;
				std::vector<float> beach;
				int shown_ox = 0;
				int shown_oy = 0;

				grid_view_s lost_view() const {
					return lost.empty() ? grid_view_s {} : grid_view_s { N, cell, shown_ox, shown_oy, false, lost.data() };
				}

				/// Depth at a world position, NaN outside of the map.
				float depth_at(const float x, const float y) const
				{
					if (depth.empty()) {
						return NAN;
					}

					const int i = static_cast<int>(std::floor(x / cell)) - ox;
					const int j = static_cast<int>(std::floor(y / cell)) - oy;
					if (i < 0 || j < 0 || i >= N || j >= N) {
						return NAN;
					}

					return depth[static_cast<size_t>(j) * N + i];
				}

				/// Takes a newer depth map from the game thread. Follow with despeckle() once the lock is released.
				/// @return true when the map was newer
				bool take(const depth_map_s& map)
				{
					if (map.version == version) {
						return false;
					}

					depth = map.depth;
					ox = map.ox;
					oy = map.oy;
					version = map.version;
					dirty = true;
					return true;
				}

				/// The game's water lookup fails now and then in open water, and finds piles, buoys or rocks as dry land. Left
				/// alone such a cell sinks a hole into the surface or flattens the waves around it, so a cell most of its
				/// neighbours contradict takes their median (two passes, for pairs of them).
				void despeckle()
				{
					if (depth.empty()) {
						return;
					}

					for (int pass = 0; pass < 2; pass++)
					{
						std::vector<float> out = depth;
						for (int j = 1; j < N - 1; j++)
						{
							for (int i = 1; i < N - 1; i++)
							{
								const int c = j * N + i;
								const float d = depth[c];
								if (!(d == NO_WATER || (d >= 0.0f && d < 0.3f))) {
									continue;
								}

								float wet[8];
								int n_wet = 0;
								int n_deep = 0;

								for (int dj = -1; dj <= 1; dj++)
								{
									for (int di = -1; di <= 1; di++)
									{
										const float e = depth[c + dj * N + di];
										if ((di || dj) && e >= 0.0f)
										{
											wet[n_wet++] = e;
											n_deep += e > 1.0f ? 1 : 0;
										}
									}
								}

								// no water amid water (6 of 8: a lookup that failed), dry or shallow amid deep water (7 of 8: a pile
								// or a buoy, a pier of one cell keeps its two neighbours on the line)
								if ((d == NO_WATER && n_wet >= 6) || (d >= 0.0f && n_deep >= 7))
								{
									std::nth_element(wet, wet + n_wet / 2, wet + n_wet);
									out[c] = wet[n_wet / 2];
								}
							}
						}

						depth.swap(out);
					}
				}

				/// True with land (dry or no water) within reach cells of cell (i, j).
				bool land_within(const int i, const int j, const int reach) const
				{
					for (int dj = -reach; dj <= reach; dj++)
					{
						for (int di = -reach; di <= reach; di++)
						{
							const int ii = i + di;
							const int jj = j + dj;
							if (ii < 0 || jj < 0 || ii >= N || jj >= N || di * di + dj * dj > reach * reach) {
								continue;
							}

							const float e = depth[static_cast<size_t>(jj) * N + ii];
							if (e == NO_WATER || e == 0.0f) {
								return true;
							}
						}
					}

					return false;
				}

				/// Distance in cells to the closest cell without water within reach cells, 99 if there is none.
				float no_water_distance(const int i, const int j, const int reach) const
				{
					float nearest = 99.0f;
					for (int dj = -reach; dj <= reach; dj++)
					{
						for (int di = -reach; di <= reach; di++)
						{
							const int ii = i + di;
							const int jj = j + dj;
							if (ii < 0 || jj < 0 || ii >= N || jj >= N) {
								continue;
							}

							if (depth[static_cast<size_t>(jj) * N + ii] == NO_WATER) {
								nearest = std::min(nearest, std::sqrt(static_cast<float>(di * di + dj * dj)));
							}
						}
					}

					return nearest;
				}

				void build_beach()
				{
					// the surface sinks into the sand where the bottom is < 0.2 m down next to land (within 6 cells): the waterline
					// follows the beach. A sandbar out in the water keeps the flat surface over it
					for (size_t c = 0; c < depth.size(); c++)
					{
						const float d = depth[c];
						if (d >= 0.0f && d < 0.2f && land_within(static_cast<int>(c % N), static_cast<int>(c / N), 6))
						{
							const float t = 1.0f - d / 0.2f;
							beach_target[c] = 0.17f * t * t * (3.0f - 2.0f * t);
						}
					}

					// water polygons end in straight lines across beaches: near such an edge (no water within 4 cells)
					// shallow water (< 0.6 m) is lowered under the sand, so the waterline follows the sand instead
					constexpr int EDGE_REACH = 4;
					for (int j = 0; j < N; j++)
					{
						for (int i = 0; i < N; i++)
						{
							const size_t c = static_cast<size_t>(j) * N + i;
							const float d = depth[c];
							if (!(d >= 0.0f && d < 0.6f)) {
								continue;
							}

							const float nearest = no_water_distance(i, j, EDGE_REACH);
							if (nearest <= static_cast<float>(EDGE_REACH))
							{
								const float k = clamp01(1.0f - (nearest - 1.0f) / static_cast<float>(EDGE_REACH));
								beach_target[c] = std::max(beach_target[c], (d + 0.08f) * k * k * (3.0f - 2.0f * k));
							}
						}
					}

					// cells without water take the largest sink around them: the vertices on the mesh edge sample them
					std::vector<float> edge = beach_target;
					for (int j = 1; j < N - 1; j++)
					{
						for (int i = 1; i < N - 1; i++)
						{
							const int c = j * N + i;
							if (depth[c] != NO_WATER) {
								continue;
							}

							float m = 0.0f;
							for (int dj = -1; dj <= 1; dj++)
							{
								for (int di = -1; di <= 1; di++) {
									m = std::max(m, beach_target[c + dj * N + di]);
								}
							}

							edge[c] = m;
						}
					}

					beach_target.swap(edge);
				}

				/// Rebuilds what the shore asks for after new depth data or a change of the wave height hs (m).
				/// Shoaling follows Green's law (~depth^-1/4), the waves break where the water is shallower than ~1.3 x hs.
				void rebuild(const float hs)
				{
					if (depth.empty() || (!dirty && std::abs(hs - built_hs) <= 0.03f)) {
						return;
					}

					dirty = false;
					built_hs = hs;
					lost_target.assign(depth.size(), 0.0f);
					beach_target.assign(depth.size(), 0.0f);
					const float breaking_depth = std::max(1.3f * std::max(hs, 0.12f), 0.15f);

					for (size_t c = 0; c < depth.size(); c++)
					{
						// not scanned or no water
						const float d = depth[c];
						if (!(d >= 0.0f)) {
							continue;
						}

						const float shoal = d < 8.0f ? std::min(std::pow(8.0f / std::max(d, 0.5f), 0.25f), 1.5f) : 1.0f;
						lost_target[c] = 1.0f - shoal * clamp01(d / breaking_depth);
					}

					if (with_beach) {
						build_beach();
					}

					// hard steps between the cells read as bumps and pits on a finer mesh
					smooth_121(lost_target, N, false);
					smooth_121(beach_target, N, false);
				}

				/// What is shown eases towards what the shore asks for (~0.6 s), so new map data never makes the waterline jump.
				void ease(const float dt)
				{
					if (lost_target.empty()) {
						return;
					}

					if (lost.size() != lost_target.size())
					{
						lost = lost_target;
						beach = beach_target;
						shown_ox = ox;
						shown_oy = oy;
					}
					else if (shown_ox != ox || shown_oy != oy)
					{
						// the map moved with the camera: what is shown moves along, new cells start at what the shore asks for
						std::vector<float> moved_lost = lost_target;
						std::vector<float> moved_beach = beach_target;

						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const int si = i + ox - shown_ox;
								const int sj = j + oy - shown_oy;
								if (si >= 0 && sj >= 0 && si < N && sj < N)
								{
									moved_lost[static_cast<size_t>(j) * N + i] = lost[static_cast<size_t>(sj) * N + si];
									moved_beach[static_cast<size_t>(j) * N + i] = beach[static_cast<size_t>(sj) * N + si];
								}
							}
						}

						lost.swap(moved_lost);
						beach.swap(moved_beach);
						shown_ox = ox;
						shown_oy = oy;
					}

					const float k = 1.0f - std::exp(-std::clamp(dt, 0.0f, 0.25f) / 0.6f);
					for (size_t c = 0; c < lost.size(); c++)
					{
						lost[c] += (lost_target[c] - lost[c]) * k;
						beach[c] += (beach_target[c] - beach[c]) * k;
					}
				}
			};

			// Water ruffled by rotor wash. What a helicopter low over water does: no waves to speak of, but its outwash ruffles
			// the surface into a sharp edged patch of fine wind ripples, streaked outward and several rotor diameters wide. The
			// patch spreads within a few seconds, follows the helicopter and stays for ~10 s after it has left.
			// - momentum theory: hover induced velocity vh = sqrt(m g / (2 rho_air A)), ~9 m/s for a light helicopter
			// - at the surface the downwash turns into a radial wall jet, ~2 vh just outside the disk, falling off ~1/r
			// - wind ripples form where that wind is above ~3 m/s, the faster the wind the sooner
			struct ruffle_s
			{
				static constexpr int N = 128;  // 1 unit cells around the camera
				static constexpr int WAVES = 12;

				// the wind ripples: short waves (1 - 2.2 m) in all directions, each running at its own deep water speed.
				// Together a slope of ~0.22 rms where the water is fully ruffled
				struct wave_s
				{
					float kx, ky, omega, amp, phase;
				};

				std::vector<float> cover = std::vector<float>(static_cast<size_t>(N) * N, 0.0f); // share of the surface covered with ripples
				std::vector<float> moved;
				wave_s waves[WAVES] = {};
				int ox = 0;
				int oy = 0;
				bool placed = false;
				float peak = 0.0f;       // largest cover on the grid
				float time = 0.0f;       // s the ripples have been running
				float wind_time = 0.0f;  // s since the wind was last applied
				std::uint32_t tick = 0u;

				// cos / sin of kx x per column and of ky y + phase per row of the current detail bake
				std::vector<float> cos_x, sin_x, cos_y, sin_y;

				void init()
				{
					std::uint32_t seed = 0x9E3779B9u;
					const auto rnd = [&]
						{
							seed ^= seed << 13;
							seed ^= seed >> 17;
							seed ^= seed << 5;
							return static_cast<float>(seed >> 8) * (1.0f / 16777216.0f);
						};

					for (auto& w : waves)
					{
						const float lambda = 1.0f + 1.2f * rnd() * rnd();
						const float k = water_physics::TWO_PI / lambda;
						const float dir = water_physics::TWO_PI * rnd();
						const float slope = 0.22f * std::sqrt(2.0f / WAVES) * (0.6f + 0.8f * rnd());
						w = { k * std::cos(dir), k * std::sin(dir), std::sqrt(water_physics::G * k), slope / k, water_physics::TWO_PI * rnd() };
					}
				}

				bool visible() const {
					return peak > 0.01f;
				}

				void recenter(const float cam_x, const float cam_y)
				{
					const int nox = static_cast<int>(std::floor(cam_x)) - N / 2;
					const int noy = static_cast<int>(std::floor(cam_y)) - N / 2;

					if (!placed || peak <= 0.0f)
					{
						ox = nox;
						oy = noy;
						placed = true;
						return;
					}

					if (std::abs(nox - ox) <= 8 && std::abs(noy - oy) <= 8) {
						return;
					}

					moved.assign(cover.size(), 0.0f);
					for (int j = 0; j < N; j++)
					{
						const int sj = j + noy - oy;
						if (sj < 0 || sj >= N) {
							continue;
						}

						for (int i = 0; i < N; i++)
						{
							const int si = i + nox - ox;
							if (si >= 0 && si < N) {
								moved[static_cast<size_t>(j) * N + i] = cover[static_cast<size_t>(sj) * N + si];
							}
						}
					}

					cover.swap(moved);
					ox = nox;
					oy = noy;
				}

				/// The outwash of one helicopter raises ripples where its wind over the water is above 3 m/s.
				/// @param dt	s since the wind was last applied
				void blow(const sources::heli_s& heli, const float strength, const float dt)
				{
					// full strength below ~0.75 rotor diameters of height, gone by ~3
					const float diameter = 2.0f * heli.radius;
					const float fade = clamp01((heli.altitude - 0.75f * diameter) / (2.25f * diameter));
					const float low = 1.0f - fade * fade * (3.0f - 2.0f * fade);
					if (low <= 0.0f) {
						return;
					}

					const float vh = std::sqrt(heli.mass * water_physics::G / (2.0f * RHO_AIR * PI * heli.radius * heli.radius));
					const float reach = heli.radius * 7.0f;
					const int i0 = std::max(0, static_cast<int>(std::floor(heli.x - reach)) - ox);
					const int i1 = std::min(N - 1, static_cast<int>(std::floor(heli.x + reach)) - ox);
					const int j0 = std::max(0, static_cast<int>(std::floor(heli.y - reach)) - oy);
					const int j1 = std::min(N - 1, static_cast<int>(std::floor(heli.y + reach)) - oy);

					for (int j = j0; j <= j1; j++)
					{
						const float ry = static_cast<float>(oy + j) + 0.5f - heli.y;
						for (int i = i0; i <= i1; i++)
						{
							const float rx = static_cast<float>(ox + i) + 0.5f - heli.x;
							const float rr = std::sqrt(rx * rx + ry * ry) / heli.radius;

							// wind over the water there: the downwash under the disk, the wall jet beyond its edge
							const float jet = rr < 1.2f ? (0.7f + 0.3f * rr / 1.2f) : std::pow(1.2f / rr, 1.3f);
							const float wind = 2.0f * vh * jet * low;
							if (wind <= 3.0f) {
								continue;
							}

							// streaked outward: the jet is uneven around the disk and its streaks wander slowly
							const float th = std::atan2(ry, rx);
							const float streak = 0.62f + 0.38f * std::sin(13.0f * th + 2.1f * std::sin(4.0f * th + 0.5f * time) + 0.9f * time)
								* std::sin(5.0f * th - 0.6f * time + 1.0f);

							// the patch has a sharp edge: either the wind raises ripples or it does not
							const float target = std::min(clamp01((wind - 3.0f) / 0.7f) * streak * strength, 1.0f);

							// the outwash takes a few seconds to reach the edge of the patch
							float& c = cover[static_cast<size_t>(j) * N + i];
							if (target > c) {
								c += (target - c) * std::min(wind / (2.5f + 0.35f * rr * heli.radius) * dt, 1.0f);
							}

							peak = std::max(peak, c);
						}
					}
				}

				/// @param dt	simulated time of this update (s)
				void update(const float dt, const std::vector<sources::heli_s>& helis, const float strength)
				{
					const bool blowing = !helis.empty() && strength > 0.0f;
					if (peak <= 0.0f && !blowing) {
						return;
					}

					time += dt;

					// the ripples die down in ~7 s
					const float keep = std::exp(-dt / 7.0f);
					peak = 0.0f;
					for (auto& c : cover)
					{
						c *= keep;
						peak = std::max(peak, c);
					}

					if (peak < 0.01f && !blowing)
					{
						std::fill(cover.begin(), cover.end(), 0.0f);
						peak = 0.0f;
						time = 0.0f;
					}

					// the patch changes over seconds: the wind is applied every third update
					wind_time += dt;
					if (tick++ % 3u != 0u) {
						return;
					}

					const float wind_dt = wind_time;
					wind_time = 0.0f;

					if (blowing)
					{
						for (const auto& heli : helis) {
							blow(heli, strength, wind_dt);
						}
					}
				}

				/// Tables for a detail bake of n x n cells of size cell starting at world cell (bx, by).
				void prepare_bake(const int n, const int bx, const int by, const float cell)
				{
					const size_t count = static_cast<size_t>(WAVES) * n;
					cos_x.resize(count);
					sin_x.resize(count);
					cos_y.resize(count);
					sin_y.resize(count);

					for (int m = 0; m < WAVES; m++)
					{
						const auto& w = waves[m];
						const float phase = w.phase - w.omega * time;

						for (int q = 0; q < n; q++)
						{
							const float ax = w.kx * static_cast<float>(bx + q) * cell;
							const float ay = w.ky * static_cast<float>(by + q) * cell + phase;
							const size_t idx = static_cast<size_t>(m) * n + q;
							cos_x[idx] = std::cos(ax);
							sin_x[idx] = std::sin(ax);
							cos_y[idx] = std::cos(ay);
							sin_y[idx] = std::sin(ay);
						}
					}
				}

				/// Adds the ripples at bake cell (i, j), world position (x, y), to out: height, slope x, slope y.
				void add_to_bake(const int n, const int i, const int j, const float x, const float y, float* out) const
				{
					// bilinear from the 1 unit grid
					const float fx = x - static_cast<float>(ox) - 0.5f;
					const float fy = y - static_cast<float>(oy) - 0.5f;
					const int ri = static_cast<int>(std::floor(fx));
					const int rj = static_cast<int>(std::floor(fy));
					if (ri < 0 || rj < 0 || ri >= N - 1 || rj >= N - 1) {
						return;
					}

					const float tx = fx - static_cast<float>(ri);
					const float ty = fy - static_cast<float>(rj);
					const float* r0 = cover.data() + static_cast<size_t>(rj) * N + ri;
					const float covered = (r0[0] + (r0[1] - r0[0]) * tx) * (1.0f - ty) + (r0[N] + (r0[N + 1] - r0[N]) * tx) * ty;
					if (covered <= 0.02f) {
						return;
					}

					float h = 0.0f;
					float gx = 0.0f;
					float gy = 0.0f;

					for (int m = 0; m < WAVES; m++)
					{
						const auto& w = waves[m];
						const float cx = cos_x[static_cast<size_t>(m) * n + i];
						const float sx = sin_x[static_cast<size_t>(m) * n + i];
						const float cy = cos_y[static_cast<size_t>(m) * n + j];
						const float sy = sin_y[static_cast<size_t>(m) * n + j];

						// sin and cos of kx x + ky y + phase
						const float sine = sx * cy + cx * sy;
						const float cosine = cx * cy - sx * sy;
						h += w.amp * sine;
						gx += w.amp * w.kx * cosine;
						gy += w.amp * w.ky * cosine;
					}

					out[0] += covered * h;
					out[1] += covered * gx;
					out[2] += covered * gy;
				}
			};

			struct simulation_s
			{
				// wakes, blast waves, rotor wash: 256 m at 1 m cells, waves live ~30 s
				// ripples: 128 m at 0.5 m cells, effective viscosity of surface films, ~10 s
				water_physics::ewave_s wake;
				water_physics::ewave_s ripple;
				water_physics::ocean_s ocean;
				ruffle_s ruffle;
				shore_map_s shore_close;
				shore_map_s shore_wide;

				input_s in;
				float pending_time = 0.0f; // game time not yet stepped
				int steps = 0;             // steps of this update
				bool wake_active = false;
				bool ripple_active = false;
				bool sea_active = false;

				// scratch of an update
				std::vector<float> ripple_smooth;
				std::vector<float> wake_rows;
				std::vector<float> wake_fine;
				std::vector<float> hull_mean;
				coupling::grid_s buoyancy_grid;

				void init()
				{
					wake.init(256, 1.0f, STEP, 20, 2e-4f, 30.0f);
					ripple.init(256, 0.5f, STEP, 24, 1.5e-3f, 10.0f);
					ocean.init();
					ruffle.init();

					shore_close.cell = 1.0f;
					shore_close.with_beach = true;
					shore_wide.cell = 4.0f;
				}

				/// Waits for the next water render and takes over what the other threads handed in.
				void wait_for_input()
				{
					bool new_close_map = false;
					bool new_wide_map = false;
					{
						std::unique_lock lock(g_input_mutex);
						take_input(lock, new_close_map, new_wide_map);
					}

					if (new_close_map) {
						shore_close.despeckle();
					}

					if (new_wide_map) {
						shore_wide.despeckle();
					}
				}

				/// Called with g_input_mutex held, which is released while waiting.
				void take_input(std::unique_lock<std::mutex>& lock, bool& new_close_map, bool& new_wide_map)
				{
					g_input_cv.wait(lock, [] { return g_input_pending; });
					g_input_pending = false;

					in.hulls = g_input.hulls;
					in.bodies = g_input.bodies;
					in.helis = g_input.helis;
					in.splashes.clear();
					in.blasts.clear();
					in.splashes.swap(g_input.splashes);
					in.blasts.swap(g_input.blasts);
					in.game_dt = g_input.game_dt;
					in.cam_x = g_input.cam_x;
					in.cam_y = g_input.cam_y;
					in.have_cam = g_input.have_cam;
					in.reset = g_input.reset;
					in.player_x = g_input.player_x;
					in.player_y = g_input.player_y;
					in.water_level = g_input.water_level;
					in.settings = g_input.settings;
					g_input.game_dt = 0.0f;
					g_input.reset = false;

					new_close_map = shore_close.take(g_input.depth_close);
					new_wide_map = shore_wide.take(g_input.depth_wide);
				}

				/// Share of the wind sea left at a point near shores, see shore_wave_share.
				float shore_share(const float x, const float y) const {
					return shore_wave_share(shore_close.lost_view(), shore_wide.lost_view(), x, y);
				}

				/// Height multiplier of the wind waves for buoyancy at a point: that of the water the closest floating thing is on,
				/// or of the water around the player.
				float buoyancy_wind_gain(const float x, const float y) const
				{
					float closest = (in.player_x - x) * (in.player_x - x) + (in.player_y - y) * (in.player_y - y);
					float level = in.water_level;

					for (const auto& hull : in.hulls)
					{
						const float d = (hull.x - x) * (hull.x - x) + (hull.y - y) * (hull.y - y);
						if (d < closest)
						{
							closest = d;
							level = hull.water_level;
						}
					}

					return in.settings.wind_gain(level);
				}

				/// The wind sea is rebuilt when the wind or its settings changed noticeably, the phases stay.
				/// Wind 0.15 (the game's calm) is light air at ~2 m/s, 0.5 a fresh breeze at ~7 m/s, a storm 18 m/s and more.
				void update_ocean()
				{
					const auto& s = in.settings;
					if (!s.wind_waves) {
						return;
					}

					constexpr float FETCH = 4000.0f; // m of open water the wind blows over
					const float wind = std::clamp(g_wind.load(), 0.0f, 1.2f);
					const float u = 1.0f + 17.0f * wind * std::sqrt(wind);

					if (std::abs(u - ocean.wind_speed) > 0.25f || s.wind_direction != ocean.wind_dir || s.wind_scale != ocean.scale
						|| s.swell_base != ocean.swell_base || s.swell_scale != ocean.swell_scale)
					{
						ocean.build(u, FETCH, s.wind_direction, s.wind_scale, s.swell_base, s.swell_scale);
						ocean.evaluate(ocean.time);
					}
				}

				/// Splashes and blasts strike the surface once. Small ones make ripples, big ones waves.
				void apply_impulses()
				{
					for (const auto& splash : in.splashes)
					{
						if (splash.radius < 1.0f) {
							ripple.strike(splash.x, splash.y, splash.radius, splash.speed);
						} else {
							wake.strike(splash.x, splash.y, splash.radius, splash.speed);
						}
					}

					// a blast drives the surface down at a few m/s over a crater
					for (const auto& blast : in.blasts)
					{
						const float closeness = 1.0f - clamp01(blast.height / std::max(blast.radius * 1.2f, 2.0f));
						if (closeness > 0.0f)
						{
							const float r = std::clamp(blast.radius * 0.6f, 2.0f, 6.0f);
							wake.strike(blast.x, blast.y, r, 10.0f * closeness * std::sqrt(std::clamp(blast.radius / 6.0f, 0.3f, 2.0f)));
							ripple.strike(blast.x, blast.y, r * 0.6f, 3.5f * closeness);
						}
					}
				}

				/// Each wave field runs while something disturbs it or its waves are still out: a calm harbour costs nothing,
				/// a boat alone does not run the ripple field and a swimmer alone not the wake field.
				void pick_active_fields()
				{
					bool wake_fed = !in.blasts.empty();
					bool ripple_fed = wake_fed;

					for (const auto& body : in.bodies) {
						(body.wake ? wake_fed : ripple_fed) = true;
					}

					for (const auto& splash : in.splashes) {
						(splash.radius < 1.0f ? ripple_fed : wake_fed) = true;
					}

					// waves below half a millimetre rms are not seen any more
					constexpr float CALM_RMS = 5e-4f;
					wake_active = in.settings.interactive && (wake_fed || wake.rms() > CALM_RMS);
					ripple_active = in.settings.interactive && (ripple_fed || ripple.rms() > CALM_RMS);

					if (!wake_active && wake.rms() > 0.0f) {
						wake.clear();
					}

					if (!ripple_active && ripple.rms() > 0.0f) {
						ripple.clear();
					}
				}

				/// Breaking: crests steeper than the Stokes limit (slope ~0.5) lose height instead of piling up, and wake waves
				/// above 0.8 m are eased towards 1.2 m (soft knee, the potential is scaled alike).
				void break_wake()
				{
					constexpr float KNEE = 0.8f;
					constexpr float LIMIT = 1.2f;
					const int n = wake.n;
					const float inv2dx = 0.5f / wake.dx;

					for (int j = 1; j < n - 1; j++)
					{
						for (int i = 1; i < n - 1; i++)
						{
							const size_t c = static_cast<size_t>(j) * n + i;
							const float gx = (wake.h[c + 1] - wake.h[c - 1]) * inv2dx;
							const float gy = (wake.h[c + n] - wake.h[c - n]) * inv2dx;
							const float slope2 = gx * gx + gy * gy;

							if (slope2 > 0.25f)
							{
								const float k = std::max(0.97f, 1.0f - (std::sqrt(slope2) - 0.5f) * 0.2f);
								wake.h[c] *= k;
								wake.phi[c] *= k;
							}

							const float h = wake.h[c];
							if (std::abs(h) > KNEE)
							{
								const float eased = std::copysign(KNEE + (LIMIT - KNEE) * std::tanh((std::abs(h) - KNEE) / (LIMIT - KNEE)), h);
								wake.h[c] = eased;
								wake.phi[c] *= eased / h;
							}
						}
					}
				}

				/// Beaches absorb wakes and ripples.
				void absorb_at_beaches(water_physics::ewave_s& field) const
				{
					if (shore_close.depth.empty()) {
						return;
					}

					for (int j = 0; j < field.n; j++)
					{
						const float y = field.world_y0() + static_cast<float>(j) * field.dx;
						for (int i = 0; i < field.n; i++)
						{
							// deep, not scanned or outside of the map
							const float d = shore_close.depth_at(field.world_x0() + static_cast<float>(i) * field.dx, y);
							if (!(d < 0.4f)) {
								continue;
							}

							const float k = d <= 0.02f ? 0.6f : 0.9f + 0.1f * (d / 0.4f);
							const size_t idx = static_cast<size_t>(j) * field.n + i;
							field.h[idx] *= k;
							field.phi[idx] *= k;
						}
					}
				}

				void step_fields()
				{
					pending_time = std::min(pending_time + in.game_dt, STEP * 4.0f);
					steps = 0;

					while (pending_time >= STEP)
					{
						// everything in the water presses on it where it is right now
						if (wake_active || ripple_active)
						{
							for (const auto& body : in.bodies) {
								(body.wake ? wake : ripple).press(body.x, body.y, body.radius, body.depth);
							}
						}

						if (wake_active)
						{
							wake.step();
							break_wake();
							absorb_at_beaches(wake);
						}

						if (ripple_active)
						{
							ripple.step();
							absorb_at_beaches(ripple);
						}

						pending_time -= STEP;
						steps++;
					}
				}

				/// The wind sea bands at whole unit points: every other node of the 0.5 unit tile, slopes from the central
				/// differences (what a Catmull-Rom sample there gives).
				static void bake_ocean_nodes(const std::vector<float>& src, std::vector<float>& dst)
				{
					constexpr int SRC_N = water_physics::ocean_s::N;
					constexpr int N = ocean_nodes_s::N;
					constexpr int CHANNELS = ocean_nodes_s::CHANNELS;
					constexpr float INV_2_CELLS = 0.5f / (water_physics::ocean_s::L / SRC_N);

					dst.resize(static_cast<size_t>(N) * N * CHANNELS);
					for (int j = 0; j < N; j++)
					{
						const int sj = 2 * j;
						const int up = (sj + 1) & (SRC_N - 1);
						const int down = (sj - 1) & (SRC_N - 1);

						for (int i = 0; i < N; i++)
						{
							const int si = 2 * i;
							const int right = (si + 1) & (SRC_N - 1);
							const int left = (si - 1) & (SRC_N - 1);
							float* c = dst.data() + (static_cast<size_t>(j) * N + i) * CHANNELS;

							c[0] = src[static_cast<size_t>(sj) * SRC_N + si];
							c[1] = (src[static_cast<size_t>(sj) * SRC_N + right] - src[static_cast<size_t>(sj) * SRC_N + left]) * INV_2_CELLS;
							c[2] = (src[static_cast<size_t>(up) * SRC_N + si] - src[static_cast<size_t>(down) * SRC_N + si]) * INV_2_CELLS;
						}
					}
				}

				/// The 1 m wake field on the n x n cells (0.5 units) of a detail bake starting at world cell (bx, by), into
				/// wake_fine. Catmull-Rom along x, then along y: at a node the node itself, half way (-1 9 9 -1) / 16.
				void upsample_wake(const int n, const int bx, const int by)
				{
					const int wn = wake.n;
					const int rows = n / 2 + 4; // wake rows the bake touches, plus the reach of the filter
					const int wy0 = static_cast<int>(std::floor(static_cast<float>(by) * 0.5f)) - 1 - wake.oy;
					wake_rows.assign(static_cast<size_t>(rows) * n, 0.0f);
					wake_fine.assign(static_cast<size_t>(n) * n, 0.0f);

					for (int r = 0; r < rows; r++)
					{
						const int wj = wy0 + r;
						if (wj < 0 || wj >= wn) {
							continue;
						}

						const float* src = wake.h.data() + static_cast<size_t>(wj) * wn;
						float* dst = wake_rows.data() + static_cast<size_t>(r) * n;

						for (int i = 0; i < n; i++)
						{
							// world x = (bx + i) / 2: the wake cell, and whether the point is half way to the next one
							const int x2 = bx + i;
							const int wi = (x2 >> 1) - wake.ox;
							if (wi < 1 || wi >= wn - 2) {
								continue;
							}

							dst[i] = (x2 & 1) ? (9.0f * (src[wi] + src[wi + 1]) - src[wi - 1] - src[wi + 2]) * 0.0625f : src[wi];
						}
					}

					for (int j = 0; j < n; j++)
					{
						const int y2 = by + j;
						const int r = (y2 >> 1) - wake.oy - wy0;
						if (r < 1 || r >= rows - 2) {
							continue;
						}

						const float* r0 = wake_rows.data() + static_cast<size_t>(r - 1) * n;
						const float* r1 = r0 + n;
						const float* r2 = r1 + n;
						const float* r3 = r2 + n;
						float* dst = wake_fine.data() + static_cast<size_t>(j) * n;

						if (y2 & 1)
						{
							for (int i = 0; i < n; i++) {
								dst[i] = (9.0f * (r1[i] + r2[i]) - r0[i] - r3[i]) * 0.0625f;
							}
						}
						else {
							std::memcpy(dst, r1, sizeof(float) * static_cast<size_t>(n));
						}
					}
				}

				/// What the cells below 2 units show, baked once per update at 0.5 unit cells so a render only interpolates it.
				void bake_detail(output::buffer_s& out)
				{
					PROFILE_ZONE_N("WaterSim::bake_detail");
					constexpr float CELL = detail_s::CELL;
					constexpr int CHANNELS = detail_s::CHANNELS;
					constexpr float MARGIN = 24.0f; // the camera moves on between a bake and the renders that use it
					constexpr int OCEAN_N = water_physics::ocean_s::N;
					constexpr float INV_2_CELLS = 0.5f / CELL;

					auto& detail = out.frame.detail;
					detail = {};

					const float radius = in.settings.bake_radius;
					if (radius < 1.0f) {
						return;
					}

					const int n = static_cast<int>((radius + MARGIN) * 2.0f / CELL);
					const int bx = static_cast<int>(std::floor(in.cam_x / CELL)) - n / 2;
					const int by = static_cast<int>(std::floor(in.cam_y / CELL)) - n / 2;
					out.detail.assign(static_cast<size_t>(n) * n * CHANNELS, 0.0f);

					if (wake_active) {
						upsample_wake(n, bx, by);
					}

					const bool ruffled = ruffle.visible();
					if (ruffled) {
						ruffle.prepare_bake(n, bx, by, CELL);
					}

					// the ripple field has the cells of the bake, without the waves of two cells
					const bool ripples = ripple_active && ripple.placed && ripple.dx == CELL;
					if (ripples)
					{
						ripple_smooth = ripple.h;
						smooth_121(ripple_smooth, ripple.n, false);
					}

					const float short_scale = in.settings.short_scale;
					const auto ocean_node = [](const std::vector<float>& band, const int i, const int j)
						{
							return band[static_cast<size_t>(j) * OCEAN_N + i];
						};

					for (int j = 0; j < n; j++)
					{
						const int wy = by + j;
						const float y = static_cast<float>(wy) * CELL;

						for (int i = 0; i < n; i++)
						{
							const int wx = bx + i;
							const float x = static_cast<float>(wx) * CELL;
							float* c = out.detail.data() + (static_cast<size_t>(j) * n + i) * CHANNELS;

							if (wake_active && i > 0 && j > 0 && i < n - 1 && j < n - 1)
							{
								const float* w = wake_fine.data() + static_cast<size_t>(j) * n + i;
								c[0] += w[0];
								c[1] += (w[1] - w[-1]) * INV_2_CELLS;
								c[2] += (w[n] - w[-n]) * INV_2_CELLS;
							}

							// the ripple and ocean grids share the cells of the bake: nodes are read directly, slopes are central differences
							if (ripples)
							{
								const int ri = wx - ripple.ox;
								const int rj = wy - ripple.oy;
								const int rn = ripple.n;

								if (ri > 0 && rj > 0 && ri < rn - 1 && rj < rn - 1)
								{
									const float* r = ripple_smooth.data() + static_cast<size_t>(rj) * rn + ri;
									c[0] += r[0];
									c[1] += (r[1] - r[-1]) * INV_2_CELLS;
									c[2] += (r[rn] - r[-rn]) * INV_2_CELLS;
								}
							}

							if (ruffled) {
								ruffle.add_to_bake(n, i, j, x, y, c);
							}

							if (sea_active)
							{
								const float share = shore_share(x, y);
								const int oi = wx & (OCEAN_N - 1);
								const int oj = wy & (OCEAN_N - 1);
								const int left = (oi - 1) & (OCEAN_N - 1);
								const int right = (oi + 1) & (OCEAN_N - 1);
								const int down = (oj - 1) & (OCEAN_N - 1);
								const int up = (oj + 1) & (OCEAN_N - 1);
								const float k = share * short_scale;

								c[3] = k * (ocean_node(ocean.mid_h, oi, oj) + ocean_node(ocean.short_h, oi, oj));
								c[4] = k * (ocean_node(ocean.mid_h, right, oj) - ocean_node(ocean.mid_h, left, oj)
									+ ocean_node(ocean.short_h, right, oj) - ocean_node(ocean.short_h, left, oj)) * INV_2_CELLS;
								c[5] = k * (ocean_node(ocean.mid_h, oi, up) - ocean_node(ocean.mid_h, oi, down)
									+ ocean_node(ocean.short_h, oi, up) - ocean_node(ocean.short_h, oi, down)) * INV_2_CELLS;

								// steepened waves near a shore are not displaced any further
								const float kd = clamp01(share) * short_scale;
								c[6] = kd * ocean_node(ocean.disp_x, oi, oj);
								c[7] = kd * ocean_node(ocean.disp_y, oi, oj);
							}
						}
					}

					detail.n = n;
					detail.ox = bx;
					detail.oy = by;
					detail.d = out.detail.data();
				}

				void fill_output(output::buffer_s& out)
				{
					const auto& s = in.settings;
					auto& published = out.frame;

					published.wake_on = wake_active;
					published.ocean_on = sea_active;
					published.ocean_hs = s.wind_waves ? ocean.hs : 0.0f;
					published.long_scale = s.long_scale;
					published.short_scale = s.short_scale;
					published.wake_rms = wake.rms();
					published.ripple_rms = ripple.rms();

					// the wake as cells of 2, 4 and 8 units can show it: a 1-2-1 filter at half the cell size
					for (int l = 0; l < 3; l++)
					{
						published.wake_lattice[l] = {};
						if (wake_active)
						{
							out.wake_lattice[l] = wake.h;
							smooth_121(out.wake_lattice[l], wake.n, false, 1 << l);
							published.wake_lattice[l] = { wake.n, wake.dx, wake.ox, wake.oy, false, out.wake_lattice[l].data() };
						}
					}

					published.ocean_nodes = {};
					if (sea_active)
					{
						bake_ocean_nodes(ocean.long_h, out.nodes_long);
						bake_ocean_nodes(ocean.mid_h, out.nodes_mid);
						published.ocean_nodes = { out.nodes_long.data(), out.nodes_mid.data() };
					}

					out.shore = shore_close.lost;
					out.shore_far = shore_wide.lost;
					out.beach = shore_close.beach;

					published.shore = shore_close.lost_view();
					published.shore.h = out.shore.data();
					published.shore_far = shore_wide.lost_view();
					published.shore_far.h = out.shore_far.data();
					published.beach = published.shore;
					published.beach.h = out.beach.data();

					if (out.shore.empty())
					{
						published.shore = {};
						published.beach = {};
					}

					if (out.shore_far.empty()) {
						published.shore_far = {};
					}

					bake_detail(out);
				}

				/// Mean height of the wake and ripple fields along both sides of each hull.
				void measure_hull_means(const grid_view_s& wake_view, const grid_view_s& ripple_view)
				{
					constexpr int STATIONS = 5;
					hull_mean.assign(in.hulls.size(), 0.0f);

					for (size_t idx = 0; idx < in.hulls.size(); idx++)
					{
						const auto& hull = in.hulls[idx];
						const float reach = hull.beam * 0.5f + 1.5f;
						float sum = 0.0f;
						float h, unused_dx, unused_dy;

						for (int station = 0; station < STATIONS; station++)
						{
							const float along = hull.length * (static_cast<float>(station) / (STATIONS - 1) - 0.5f);
							const float cx = hull.x + hull.dirx * along;
							const float cy = hull.y + hull.diry * along;

							for (const float side : { -1.0f, 1.0f })
							{
								const float px = cx - hull.diry * reach * side;
								const float py = cy + hull.dirx * reach * side;
								wake_view.sample(px, py, h, unused_dx, unused_dy);
								sum += h;
								ripple_view.sample(px, py, h, unused_dx, unused_dy);
								sum += h;
							}
						}

						hull_mean[idx] = sum / (2.0f * STATIONS);
					}
				}

				/// The simulated heights on the game's wave grid, for buoyancy. Boats ride what the mesh shows around them:
				/// wakes, ripples, the long and the 4 - 16 unit wind waves (the grid's 2 unit cells show nothing shorter).
				void write_buoyancy_grid()
				{
					if (!coupling::hooked) {
						return;
					}

					const auto& s = in.settings;
					auto& grid = buoyancy_grid;
					grid.base_x = *game::pWaterDynamicBaseX;
					grid.base_y = *game::pWaterDynamicBaseY;

					const grid_view_s wake_view = { wake.n, wake.dx, wake.ox, wake.oy, false, wake.h.data() };
					const grid_view_s ripple_view = { ripple.n, ripple.dx, ripple.ox, ripple.oy, false, ripple.h.data() };
					const auto long_view = ocean_view(ocean.long_h);
					const auto mid_view = ocean_view(ocean.mid_h);
					measure_hull_means(wake_view, ripple_view);

					const auto fields_at = [&](const float x, const float y)
						{
							float wake_h, ripple_h, unused_dx, unused_dy;
							wake_view.sample(x, y, wake_h, unused_dx, unused_dy);
							ripple_view.sample(x, y, ripple_h, unused_dx, unused_dy);
							return wake_h + ripple_h;
						};

					for (int kx = 0; kx < coupling::CELLS; kx++)
					{
						const int cell_x = grid.base_x + coupling::CELL_SIZE * kx;
						const float x = static_cast<float>(cell_x);

						for (int ky = 0; ky < coupling::CELLS; ky++)
						{
							const int cell_y = grid.base_y + coupling::CELL_SIZE * ky;
							const float y = static_cast<float>(cell_y);
							float h = 0.0f;

							// Under something that floats the surface holds the dent it presses itself. That dent is its own
							// volume, it would sink into it. What tilts it are the waves coming in from around it: taken from both
							// sides just outside of it and carried across its beam, so a wake rolling in rolls and pitches it
							const sources::hull_s* under = nullptr;
							float along = 0.0f;
							float across = 0.0f;

							for (const auto& hull : in.hulls)
							{
								const float rx = x - hull.x;
								const float ry = y - hull.y;
								along = rx * hull.dirx + ry * hull.diry;
								across = -rx * hull.diry + ry * hull.dirx;

								if (std::abs(along) < hull.length * 0.5f + 1.0f && std::abs(across) < hull.beam * 0.5f + 1.0f)
								{
									under = &hull;
									break;
								}
							}

							if (under)
							{
								const float reach = under->beam * 0.5f + 1.5f;
								const float cx = under->x + under->dirx * along;
								const float cy = under->y + under->diry * along;
								const float left = fields_at(cx + under->diry * reach, cy - under->dirx * reach);
								const float right = fields_at(cx - under->diry * reach, cy + under->dirx * reach);
								const float side = clamp01((across + reach) / (2.0f * reach));

								// Without the mean along its length: the rings it sends out itself when it bobs would lift and drop
								// it again. And at 0.7: the grid runs a frame or two behind, what the hull sends out must not come
								// back whole
								h += 0.7f * (left + (right - left) * side - hull_mean[static_cast<size_t>(under - in.hulls.data())]);
							}
							else {
								h += fields_at(x, y);
							}

							if (sea_active)
							{
								const float k = shore_share(x, y) * buoyancy_wind_gain(x, y);
								float band, unused_dx, unused_dy;
								long_view.sample(x, y, band, unused_dx, unused_dy);
								h += band * k * s.long_scale;
								mid_view.sample(x, y, band, unused_dx, unused_dy);
								h += band * k * s.short_scale;
							}

							grid.heights[coupling::cell_index(cell_x, cell_y)] = std::isfinite(h) ? h : 0.0f;
						}
					}

					std::lock_guard lock(coupling::mutex);
					coupling::grid = grid;
					coupling::active = true;
				}

				/// Wind waves and interactive water are both off: the game's own simulation runs and is drawn untouched.
				void stand_down()
				{
					coupling::active = false;
					pending_time = 0.0f;
					wake.clear();
					ripple.clear();

					if (const int idx = output::pick_free(); idx >= 0)
					{
						output::buffers[idx].frame = {};
						output::publish(idx);
					}
				}

				void update()
				{
					PROFILE_ZONE_N("WaterSim::update");
					const auto start = std::chrono::steady_clock::now();
					const auto& s = in.settings;

					if (in.reset)
					{
						wake.clear();
						ripple.clear();
					}

					if (!in.have_cam) {
						return;
					}

					if (!s.wind_waves && !s.interactive)
					{
						stand_down();
						return;
					}

					wake.recenter(in.cam_x, in.cam_y);
					ripple.recenter(in.cam_x, in.cam_y);
					update_ocean();
					apply_impulses();
					pick_active_fields();
					step_fields();

					if (!steps) {
						return;
					}

					const float dt = STEP * static_cast<float>(steps);
					ruffle.recenter(in.cam_x, in.cam_y);
					ruffle.update(dt, in.helis, s.interactive ? s.heli_strength : 0.0f);

					// the wind sea is synthesised once per update
					sea_active = s.wind_waves && !ocean.calm;
					if (sea_active) {
						ocean.advance(STEP, steps);
					}

					const int idx = output::pick_free();
					if (idx < 0) {
						return;
					}

					// the waves break in water shallower than the tallest waves the settings ask for
					const float shore_hs = ocean.hs * std::max(s.long_scale, s.short_scale);
					shore_close.rebuild(shore_hs);
					shore_wide.rebuild(shore_hs);
					shore_close.ease(in.game_dt);
					shore_wide.ease(in.game_dt);

					fill_output(output::buffers[idx]);
					write_buoyancy_grid();
					output::publish(idx);

					// wall time of this update, smoothed
					const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
					g_sim_ms = g_sim_ms.load() * 0.9f + ms * 0.1f;
				}
			};

			void thread_main()
			{
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
				shared::common::profiler::name_current_thread("WaterSim");

				const auto simulation = std::make_unique<simulation_s>();
				simulation->init();

				while (true)
				{
					simulation->wait_for_input();
					simulation->update();
				}
			}
		}

		// explosions and bullets seen by the hooks, picked up by the game thread
		namespace events
		{
			enum explosion_tag_e : int
			{
				EXPLOSION_GRENADE = 0,
				EXPLOSION_MOLOTOV = 1,
				EXPLOSION_ROCKET = 2,
				EXPLOSION_HI_OCTANE = 3,
				EXPLOSION_CAR = 4,
				EXPLOSION_PLANE = 5,
				EXPLOSION_PETROL_PUMP = 6,
				EXPLOSION_BIKE = 7,
				EXPLOSION_DIR_STEAM = 8,
				EXPLOSION_DIR_FLAME = 9,
				EXPLOSION_DIR_WATER_HYDRANT = 10,
				EXPLOSION_DIR_GAS_CANISTER = 11,
				EXPLOSION_BOAT = 12,
				EXPLOSION_SHIP_DESTROY = 13,
				EXPLOSION_TRUCK = 14,
			};

			struct explosion_s
			{
				float x, y, z, radius;
			};

			struct shot_s
			{
				float x, y;
			};

			constexpr size_t MAX_EXPLOSIONS = 64u;
			constexpr size_t MAX_SHOTS = 32u;

			std::mutex mutex;
			std::vector<explosion_s> explosions;
			std::vector<shot_s> shots;

			/// Blast radius (m) of an explosion at full size.
			float blast_radius(const int tag)
			{
				switch (tag)
				{
				case EXPLOSION_GRENADE:
					return 5.0f;

				case EXPLOSION_MOLOTOV:
					return 2.0f;

				case EXPLOSION_ROCKET:
					return 6.0f;

				case EXPLOSION_CAR:
				case EXPLOSION_PLANE:
				case EXPLOSION_BOAT:
				case EXPLOSION_SHIP_DESTROY:
				case EXPLOSION_TRUCK:
					return 8.0f;

				default:
					return 4.0f;
				}
			}

			// the unnamed parameters are only passed on to the network event
			DETOUR_TYPEDEF(CExplosionManager_AddExplosion, bool, __cdecl, void* exploding_entity, void* owner, int tag, float size_scale, const Vector* pos,
				bool vfx_flag, std::uint32_t activation_delay, bool make_sound, float cam_shake, bool a10, bool no_fx, const Vector* direction,
				void* attach_entity, int attach_bone, bool a15, bool a16, void* dummy_object, bool from_network_event, int a19);

			/// @param size_scale	the game clamps it to 0.01 - 1
			/// @return false when the game did not add the explosion (one of the same size is already there, pool full, network request)
			bool __cdecl CExplosionManager_AddExplosion_hk(void* exploding_entity, void* owner, int tag, float size_scale, const Vector* pos,
				bool vfx_flag, std::uint32_t activation_delay, bool make_sound, float cam_shake, bool a10, bool no_fx, const Vector* direction,
				void* attach_entity, int attach_bone, bool a15, bool a16, void* dummy_object, bool from_network_event, int a19)
			{
				const bool added = CExplosionManager_AddExplosion_og(exploding_entity, owner, tag, size_scale, pos, vfx_flag, activation_delay, make_sound,
					cam_shake, a10, no_fx, direction, attach_entity, attach_bone, a15, a16, dummy_object, from_network_event, a19);

				// steam, flames and hydrants are jets, not blasts
				const bool jet = tag == EXPLOSION_DIR_STEAM || tag == EXPLOSION_DIR_FLAME || tag == EXPLOSION_DIR_WATER_HYDRANT;

				if (added && !jet && pos && std::isfinite(pos->x) && std::isfinite(pos->y) && std::isfinite(pos->z))
				{
					std::lock_guard lock(mutex);
					if (explosions.size() < MAX_EXPLOSIONS) {
						explosions.push_back({ pos->x, pos->y, pos->z, blast_radius(tag) * std::clamp(size_scale, 0.01f, 1.0f) });
					}
				}

				return added;
			}

			typedef bool(__cdecl* CWater_TestLineAgainstWater_t)(const Vector* start, const Vector* end, Vector* hit);

			/// Replaces the two calls of CWater::TestLineAgainstWater that place a bullet's splash: the path of the bullet
			/// (muzzle to what it hit) against the flat water. hit is only valid when the water was met.
			bool __cdecl bullet_water_test_hk(const Vector* start, const Vector* end, Vector* hit)
			{
				const auto test = reinterpret_cast<CWater_TestLineAgainstWater_t>(game::func_addr__CWater_TestLineAgainstWater);
				const bool met = test(start, end, hit);

				if (met && hit && std::isfinite(hit->x) && std::isfinite(hit->y))
				{
					std::lock_guard lock(mutex);
					if (shots.size() < MAX_SHOTS) {
						shots.push_back({ hit->x, hit->y });
					}
				}

				return met;
			}

			void init()
			{
				if (game::func_addr__CExplosion_AddExplosion)
				{
					shared::utils::hook::detour(game::func_addr__CExplosion_AddExplosion, &CExplosionManager_AddExplosion_hk,
						DETOUR_CAST(CExplosionManager_AddExplosion_og));
				}

				if (game::func_addr__CWater_TestLineAgainstWater && game::hk_addr__weapon_fire_fx_water_test && game::hk_addr__bullet_update_water_test)
				{
					shared::utils::hook(game::hk_addr__weapon_fire_fx_water_test, bullet_water_test_hk, HOOK_CALL).install()->quick(); // 0xB5D8FC
					shared::utils::hook(game::hk_addr__bullet_update_water_test, bullet_water_test_hk, HOOK_CALL).install()->quick(); // 0xD9C13A
				}
			}
		}

		// shore depth maps scanned on the game thread: the water level and the collision below it
		namespace depth
		{
			// 96 x 96 cells around the camera.
			// -1 = no water, 0 = dry (land or a deck at the water line), NaN = not scanned yet, else metres (30 = no bottom found)
			struct grid_s
			{
				static constexpr int N = 96;
				static constexpr float NO_WATER = -1.0f;
				static constexpr float NO_BOTTOM = 30.0f;

				float cell = 1.0f; // world units per cell
				int ox = 0;
				int oy = 0;
				bool placed = false;
				std::vector<float> d = std::vector<float>(static_cast<size_t>(N) * N, NAN);
				std::vector<std::uint8_t> recheck = std::vector<std::uint8_t>(static_cast<size_t>(N) * N, 0u); // "no water" cells that are asked again
				float level = 0.0f;       // calm water level last found in this map
				bool has_level = false;
				int cursor = 0;
				int settle = 0;           // frames without scanning after the camera jumped
				std::uint32_t tick = 0u;
				std::uint32_t version = 0u;
				bool changed = false;

				/// Keeps the map around the camera: moves by whole cells and keeps what it knows about the cells that stay.
				void recenter(const float cam_x, const float cam_y)
				{
					constexpr int MOVE = 8;  // cells the camera moves before the map follows
					constexpr int JUMP = 40; // cells that count as a teleport or a cut

					const int nox = static_cast<int>(std::floor(cam_x / cell)) - N / 2;
					const int noy = static_cast<int>(std::floor(cam_y / cell)) - N / 2;
					const bool jumped = placed && (std::abs(nox - ox) > JUMP || std::abs(noy - oy) > JUMP);

					// right after the camera jumps the game's water lookup still fails around the new place for a moment:
					// no scanning for a second, or whole bands of open water end up as "no water"
					if (jumped) {
						settle = 60;
					}

					if (placed && std::abs(nox - ox) <= MOVE && std::abs(noy - oy) <= MOVE) {
						return;
					}

					std::vector<float> moved(static_cast<size_t>(N) * N, NAN);
					if (placed)
					{
						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const int si = i + nox - ox;
								const int sj = j + noy - oy;
								if (si >= 0 && sj >= 0 && si < N && sj < N) {
									moved[static_cast<size_t>(j) * N + i] = d[static_cast<size_t>(sj) * N + si];
								}
							}
						}
					}

					d.swap(moved);
					std::fill(recheck.begin(), recheck.end(), std::uint8_t(0));

					if (!placed || jumped) {
						has_level = false;
					}

					ox = nox;
					oy = noy;
					placed = true;
					changed = true;
				}

				/// Depth of one cell from up to three natives, two of them ray casts.
				/// @param level_hint	water level last found under something
				float probe(natives* n, const float x, const float y, const float level_hint)
				{
					float water_level = 0.0f;
					float ground = -1000.0f;

					// under bridges and roofs the lookup from high above finds no water: asked again from just above the
					// level of the water around
					bool water = n->GetWaterHeightNoWaves(x, y, 100.0f, &water_level);
					if (!water) {
						water = n->GetWaterHeightNoWaves(x, y, level_hint + 1.0f, &water_level);
					}

					if (water)
					{
						level = water_level;
						has_level = true;
					}
					else
					{
						// the game's water polygons leave gaps and seams in open water: no water there, but the ground is below
						// the level of the water around. Counted as water, or the shallows around it sink into a pit
						if (has_level && n->GetGroundZFor3dCoord(x, y, level + 60.0f, &ground) && ground < level - 0.05f) {
							return std::min(level - ground, NO_BOTTOM);
						}

						return NO_WATER;
					}

					// land at the water line: beach, bank, low pier
					if (n->GetGroundZFor3dCoord(x, y, water_level + 60.0f, &ground) && ground > water_level - 0.05f && ground < water_level + 4.0f) {
						return 0.0f;
					}

					if (n->GetGroundZFor3dCoord(x, y, water_level - 0.05f, &ground) && ground < water_level) {
						return std::min(water_level - ground, NO_BOTTOM);
					}

					return NO_BOTTOM;
				}

				/// Scans up to budget cells that are not known yet or asked again.
				void scan(natives* n, const float cam_x, const float cam_y, const float level_hint, int budget)
				{
					recenter(cam_x, cam_y);

					if (settle > 0)
					{
						settle--;
						return;
					}

					// "no water" is asked again every ~10 s, a wrong answer does not stay. The cell keeps its answer until the
					// new one is in: wiping it would drop the waterline onto the beach until the map is scanned again
					if (++tick % 600u == 0u)
					{
						for (size_t c = 0; c < d.size(); c++) {
							recheck[c] = d[c] == NO_WATER ? 1u : 0u;
						}
					}

					for (int scanned = 0; scanned < N * N && budget > 0; scanned++)
					{
						const int c = cursor;
						cursor = (cursor + 1) % (N * N);
						if (!std::isnan(d[c]) && !recheck[c]) {
							continue;
						}

						budget--;
						recheck[c] = 0u;

						const float before = d[c];
						const float x = (static_cast<float>(ox + c % N) + 0.5f) * cell;
						const float y = (static_cast<float>(oy + c / N) + 0.5f) * cell;
						d[c] = probe(n, x, y, level_hint);

						if (!(d[c] == before)) {
							changed = true;
						}
					}
				}

				/// Hands a changed map to the simulation thread. Called with g_input_mutex held.
				void publish(depth_map_s& out)
				{
					if (!changed) {
						return;
					}

					changed = false;
					out.depth = d;
					out.ox = ox;
					out.oy = oy;
					out.version = ++version;
				}
			};
		}

		// game thread: vehicles, peds and objects near the player, each followed from frame to frame
		namespace entities
		{
			enum pool_e : int
			{
				POOL_VEHICLE = 0,
				POOL_PED = 1,
				POOL_OBJECT = 2,
				POOL_COUNT = 3,
			};

			// CPool
			struct pool_s
			{
				std::uint8_t* objects;
				std::int8_t* flags; // per slot: 0x80 = free, the low bits count how often the slot was reused
				int count;
				int item_size;
			};

			constexpr std::uint32_t ENTITY_MATRIX = 0x20; // pointer to right, forward, up and position rows of 16 bytes

			// a rigid entity followed from frame to frame
			struct track_s
			{
				float pose[16] = {};  // matrix of the last frame
				bool has_pose = false;
				bool model_known = false;
				bool boat = false;
				bool heli = false;
				float min[3] = { -1.0f, -2.25f, -0.6f }; // model box
				float max[3] = { 1.0f, 2.25f, 1.2f };
				float mass = 0.0f;    // kg, 0 = unknown
				float vz = 0.0f;      // vertical speed of the last frame
				float lift = 1.0f;    // helicopters: share of the weight the rotor carries (smoothed)
				float still = 0.0f;   // s without moving
				bool trim_valid = false; // floating hulls: draft and trim the dent follows (eased)
				float trim_z = 0.0f;
				float trim_pitch = 0.0f;
				float trim_roll = 0.0f;
				bool first_look = true;   // not looked at yet
				float settling = 0.0f;    // s left in which something that appeared in the water adds nothing
				bool float_valid = false; // everything else that floats: height above the surface it floats on
				float float_height = 0.0f;
				float float_eased = 0.0f;
				float time = 0.0f;    // module time of the last pose
				float water = 0.0f;   // level of the calm surface there, looked up every ~0.5 s
				bool has_water = false;
				std::uint32_t water_frame = 0u;
				std::uint32_t seen = 0u;
			};

			// what the entities of a pass add
			struct found_s
			{
				std::vector<sources::hull_s> hulls;
				std::vector<sources::body_s> bodies;
				std::vector<sources::heli_s> helis;
				std::vector<sources::splash_s> splashes;
				int vehicles_seen = 0;
			};

			struct state_s
			{
				std::unordered_map<std::uint32_t, track_s> tracks; // keyed by pool << 28 | script handle
				std::vector<std::uint8_t> hot[POOL_COUNT];        // per pool slot: in or near the water, looked at on every pass
				bool pool_checked[POOL_COUNT] = {};
				bool pool_ok[POOL_COUNT] = {};
				std::uint32_t frame = 0u;
				std::uint32_t pass = 0u;
				float time = 0.0f;             // game time of this module (s)
				float last_water_level = 0.0f; // level of the water last found under something
				float since_pass = 1.0f;       // s since the entities were last looked at
				float object_min_mass = 0.0f;  // water_interactive_object_min_mass
				found_s kept;                  // result of the last pass, used until the next one

				std::vector<water_physics::press_point_s> press;
				std::vector<water_physics::strike_point_s> strikes;
			};

			state_s g_state;

			const pool_s* get_pool(void** pool_ptr)
			{
				if (!pool_ptr || !*pool_ptr) {
					return nullptr;
				}

				const auto pool = static_cast<const pool_s*>(*pool_ptr);
				if (!pool->objects || !pool->flags || pool->count <= 0 || pool->count > 16384 || pool->item_size < 0x40 || pool->item_size > 0x10000) {
					return nullptr;
				}

				return pool;
			}

			/// Matrix of the entity at a pool slot, null for an entity without one.
			const float* entity_matrix(const pool_s* pool, const int slot)
			{
				const auto entity = pool->objects + static_cast<size_t>(slot) * pool->item_size;
				return *reinterpret_cast<float* const*>(entity + ENTITY_MATRIX);
			}

			/// Model box, mass and kind of an entity.
			void learn_model(natives* n, const int pool, const int handle, track_s& t)
			{
				t.model_known = true;

				if (pool == POOL_PED)
				{
					// a standing body, the origin of a ped is at its hips
					const float min[3] = { -0.25f, -0.18f, -1.0f };
					const float max[3] = { 0.25f, 0.18f, 0.8f };
					std::memcpy(t.min, min, sizeof(min));
					std::memcpy(t.max, max, sizeof(max));
					t.mass = 80.0f;
					return;
				}

				std::uint32_t model = 0u;
				if (pool == POOL_VEHICLE) {
					n->GetCarModel(handle, &model);
				} else {
					n->GetObjectModel(handle, &model);
				}

				if (!model) {
					return;
				}

				if (pool == POOL_VEHICLE)
				{
					t.boat = n->IsThisModelABoat(model);
					t.heli = n->IsThisModelAHeli(model);
				}

				Vector mn = {};
				Vector mx = {};
				n->GetModelDimensions(model, &mn, &mx);

				const float size[3] = { mx.x - mn.x, mx.y - mn.y, mx.z - mn.z };
				if (std::min({ size[0], size[1], size[2] }) > 0.05f && std::max({ size[0], size[1], size[2] }) < 80.0f)
				{
					const float min[3] = { mn.x, mn.y, mn.z };
					const float max[3] = { mx.x, mx.y, mx.z };
					std::memcpy(t.min, min, sizeof(min));
					std::memcpy(t.max, max, sizeof(max));
				}

				float mass = 0.0f;
				if (pool == POOL_VEHICLE) {
					n->GetCarMass(handle, &mass);
				}
				else {
					n->GetObjectMass(handle, &mass);
				}

				t.mass = std::isfinite(mass) && mass > 0.5f && mass < 500000.0f ? mass : 0.0f;
			}

			/// The box of an entity from its matrix and its model box.
			void pose_box(const float* m, const track_s& t, float axis[3][3], float centre[3], float half[3])
			{
				for (int a = 0; a < 3; a++)
				{
					const float* r = m + a * 4;
					const float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
					const float inv = len > 1e-4f ? 1.0f / len : 0.0f;
					axis[a][0] = r[0] * inv;
					axis[a][1] = r[1] * inv;
					axis[a][2] = r[2] * inv;
					half[a] = 0.5f * (t.max[a] - t.min[a]);
				}

				for (int k = 0; k < 3; k++)
				{
					centre[k] = m[12 + k];
					for (int a = 0; a < 3; a++) {
						centre[k] += axis[a][k] * 0.5f * (t.max[a] + t.min[a]);
					}
				}
			}

			/// Axes of a hull's dent on a heading: forward pitched up by pitch, right rolled by roll, made orthonormal again.
			void trim_axes(const float heading[3], const float pitch, const float roll, float axis[3][3])
			{
				const float len = std::sqrt(heading[0] * heading[0] + heading[1] * heading[1]);
				const float fx = len > 1e-4f ? heading[0] / len : 0.0f;
				const float fy = len > 1e-4f ? heading[1] / len : 1.0f;
				const float forward[3] = { fx * std::cos(pitch), fy * std::cos(pitch), std::sin(pitch) };
				float right[3] = { fy * std::cos(roll), -fx * std::cos(roll), std::sin(roll) };

				const float along = right[0] * forward[0] + right[1] * forward[1] + right[2] * forward[2];
				for (int k = 0; k < 3; k++) {
					right[k] -= along * forward[k];
				}

				const float right_len = std::max(std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]), 1e-4f);
				for (int k = 0; k < 3; k++) {
					right[k] /= right_len;
				}

				const float up[3] =
				{
					right[1] * forward[2] - right[2] * forward[1],
					right[2] * forward[0] - right[0] * forward[2],
					right[0] * forward[1] - right[1] * forward[0]
				};

				for (int k = 0; k < 3; k++)
				{
					axis[0][k] = right[k];
					axis[1][k] = forward[k];
					axis[2][k] = up[k];
				}
			}

			/// A floating hull presses the dent of its mean draft and trim (eased over ~1.5 s), not every bob and pitch: the rings
			/// it sends out pitching would lift and tip it again through the delay of the buoyancy grid, a loop that never runs
			/// down (a real hull loses that energy into the waves it sends away). Heading and position stay live, so wakes and
			/// turns are untouched. A fast drop or a capsize takes the real pose.
			void ease_hull_trim(track_s& t, water_physics::rigid_body_s& b)
			{
				if (!t.boat || b.axis[2][2] <= 0.5f)
				{
					t.trim_valid = false;
					return;
				}

				const float vz = b.has_prev ? (b.centre[2] - b.prev_centre[2]) / b.dt : 0.0f;
				const float pitch = std::asin(std::clamp(b.axis[1][2], -1.0f, 1.0f));
				const float roll = std::asin(std::clamp(b.axis[0][2], -1.0f, 1.0f));

				if (!t.trim_valid || !b.has_prev || std::abs(vz) > 1.5f)
				{
					t.trim_z = b.centre[2];
					t.trim_pitch = pitch;
					t.trim_roll = roll;
					t.trim_valid = true;
				}
				else
				{
					// the pose a frame ago on its own heading with the eased trim of then: the dent moves up and down as eased
					const float prev_heading[3] = { b.prev_axis[1][0], b.prev_axis[1][1], b.prev_axis[1][2] };
					trim_axes(prev_heading, t.trim_pitch, t.trim_roll, b.prev_axis);
					b.prev_centre[2] = t.trim_z;

					const float k = 1.0f - std::exp(-b.dt / 1.5f);
					t.trim_z += (b.centre[2] - t.trim_z) * k;
					t.trim_pitch += (pitch - t.trim_pitch) * k;
					t.trim_roll += (roll - t.trim_roll) * k;
				}

				const float heading[3] = { b.axis[1][0], b.axis[1][1], b.axis[1][2] };
				trim_axes(heading, t.trim_pitch, t.trim_roll, b.axis);
				b.centre[2] = t.trim_z;
			}

			/// Something that floats rides the waves, so its height is taken against the surface it floats on and eased (~1.5 s)
			/// like the trim of a hull. Taken against the calm level, every wave passing under it would look like the body
			/// sinking and rising, and it would send out rings of its own that move it again. Anything faster than 1.5 m/s
			/// against the surface (a drop, a jump) keeps its real motion.
			void ease_float_height(track_s& t, water_physics::rigid_body_s& b)
			{
				const float height = b.centre[2] - t.water - coupling::wave_height(b.centre[0], b.centre[1]);
				const bool fast = !t.float_valid || !b.has_prev || std::abs(height - t.float_height) > 1.5f * b.dt;
				float previous = height;

				if (fast)
				{
					if (t.float_valid) {
						previous = t.float_height;
					}

					t.float_eased = height;
				}
				else
				{
					previous = t.float_eased;
					t.float_eased += (height - t.float_eased) * (1.0f - std::exp(-b.dt / 1.5f));
				}

				t.float_height = height;
				t.float_valid = true;
				b.centre[2] = t.water + t.float_eased;
				b.prev_centre[2] = t.water + previous;
			}

			/// Rotor wash of a manned helicopter over water: the jet leaves along the rotor's axis and lands where that meets
			/// the water. The rotor carries the helicopter's weight plus what its climb or sink asks for.
			/// @param az		vertical acceleration (m/s2)
			/// @param scale	strength of what the rotor wash adds
			void rotor_wash(natives* n, const int handle, track_s& t, const water_physics::rigid_body_s& b, const float az, const float scale, found_s& found)
			{
				if (b.axis[2][2] <= 0.3f) {
					return;
				}

				const float hub[3] = { b.centre[0] + b.axis[2][0] * b.half[2], b.centre[1] + b.axis[2][1] * b.half[2], b.centre[2] + b.axis[2][2] * b.half[2] };
				const float reach = (hub[2] - t.water) / b.axis[2][2];
				const float altitude = reach - 2.0f * b.half[2];
				if (altitude >= 45.0f || altitude <= -1.0f) {
					return;
				}

				// only a manned helicopter has its rotor turning
				int pilot = 0;
				n->GetDriverOfCar(handle, &pilot);
				if (!pilot) {
					return;
				}

				const float lift = std::clamp((water_physics::G + az) / (water_physics::G * b.axis[2][2]), 0.6f, 1.6f);
				t.lift += (lift - t.lift) * std::min(b.dt * 2.0f, 1.0f);

				// the model box is longer than the rotor (tail boom)
				const float radius = std::clamp(0.8f * std::max(b.half[0], b.half[1]), 3.0f, 9.0f);
				const float jet_x = hub[0] - b.axis[2][0] * reach;
				const float jet_y = hub[1] - b.axis[2][1] * reach;
				const float carried = (t.mass > 0.0f ? t.mass : 2500.0f) * t.lift;
				found.helis.push_back({ jet_x, jet_y, altitude, radius, carried });

				// The rotor wash also dents the water. Induced velocity vh = sqrt(m g / (2 rho_air A)) (momentum theory), the jet
				// arrives with up to twice that (the far wake), so the surface holds p = rho_air (2 vh)^2 / 2 down by p / (rho g):
				// 2 - 4 cm under a light helicopter in ground effect, fading out above about two rotor diameters
				if (altitude < 4.0f * radius)
				{
					const float vh = std::sqrt(carried * water_physics::G / (2.0f * RHO_AIR * PI * radius * radius));
					const float pressure = 0.5f * RHO_AIR * 4.0f * vh * vh;
					const float height = std::max(altitude, 0.0f) / radius;
					const float fade = 1.0f / (1.0f + height * height);
					const float dent = std::min(pressure / (water_physics::RHO * water_physics::G), 0.08f) * fade * scale;
					found.bodies.push_back({ jet_x, jet_y, 0.7f * radius, dent, true });
				}
			}

			/// One rigid entity: what its box does to the water on this pass, from its pose now and at the last pass.
			/// @param m		entity matrix
			/// @param strength	water_interactive_strength, scales what the motion of the entity adds
			/// @param scale	scale of this kind of entity, applied to everything it adds
			/// @return true when it touches the water
			bool track_entity(natives* n, const int pool, const int handle, const float* m, const float strength, const float scale, found_s& found)
			{
				auto& state = g_state;
				auto& t = state.tracks[(static_cast<std::uint32_t>(pool) << 28) | static_cast<std::uint32_t>(handle)];
				if (!t.model_known) {
					learn_model(n, pool, handle, t);
				}

				t.seen = state.frame;
				const bool first_look = t.first_look;
				t.first_look = false;

				// Light objects (floating debris, litter) are carried by the water but do not disturb it. This also covers
				// objects without physics, which the game reports with a mass of 1, and those it reports no mass for
				if (pool == POOL_OBJECT && t.mass < state.object_min_mass) {
					return false;
				}

				// time since its last pose, dry entities are not looked at on every pass
				const float dt = state.time - t.time;
				t.time = state.time;

				water_physics::rigid_body_s b;
				pose_box(m, t, b.axis, b.centre, b.half);

				if (t.has_pose && dt > 0.0f && dt < 0.25f)
				{
					float unused_half[3];
					pose_box(t.pose, t, b.prev_axis, b.prev_centre, unused_half);
					b.has_prev = true;
					b.dt = dt;
				}

				ease_hull_trim(t, b);

				const float vz = b.has_prev ? (b.centre[2] - b.prev_centre[2]) / dt : 0.0f;
				const float az = b.has_prev && std::abs(vz) < 60.0f ? (vz - t.vz) / dt : 0.0f;
				t.vz = std::abs(vz) < 60.0f ? vz : 0.0f;
				std::memcpy(t.pose, m, sizeof(t.pose));
				t.has_pose = true;

				// the water level changes only from one body of water to the next
				if (!t.has_water || state.frame - t.water_frame > 30u)
				{
					t.water_frame = state.frame;
					t.has_water = n->GetWaterHeightNoWaves(m[12], m[13], m[14], &t.water);
				}

				if (!t.has_water) {
					return false;
				}

				state.last_water_level = t.water;

				if (t.heli) {
					rotor_wash(n, handle, t, b, az, strength * scale, found);
				}

				if (pool != POOL_PED && !t.boat) {
					ease_float_height(t, b);
				}

				// far above or below the surface
				const float reach_z = std::abs(b.axis[0][2]) * b.half[0] + std::abs(b.axis[1][2]) * b.half[1] + std::abs(b.axis[2][2]) * b.half[2];
				if (b.centre[2] - reach_z > t.water + 0.5f || b.centre[2] + reach_z < t.water - 3.0f) {
					return false;
				}

				// a ped in a vehicle is part of the vehicle
				if (pool == POOL_PED && n->IsCharInAnyCar(handle)) {
					return false;
				}

				b.water = t.water;
				b.mass = t.mass;
				b.hull = t.boat;
				b.stagnation = t.boat ? 0.4f : 0.5f;
				b.planing = 0.75f;

				// A helicopter's model box is its rotor disc and tail boom. What sits in the water is the cabin on its skids:
				// about a fifth of the length and a tenth of it wide, a rounded footprint a little ahead of the mast
				if (t.heli)
				{
					const float length = 2.0f * b.half[1];
					const float ahead = 0.05f * length;
					for (int a = 0; a < 3; a++)
					{
						b.centre[a] += b.axis[1][a] * ahead;
						b.prev_centre[a] += b.prev_axis[1][a] * ahead;
					}

					b.half[0] = std::clamp(0.1f * length, 0.8f, 1.4f);
					b.half[1] = std::clamp(0.2f * length, 1.5f, 3.2f);
					b.rounded = true;
				}

				// bodies narrower than 1.5 m go on the 0.5 m ripple field
				const float middle = b.half[0] + b.half[1] + b.half[2] - std::max({ b.half[0], b.half[1], b.half[2] }) - std::min({ b.half[0], b.half[1], b.half[2] });
				const bool on_wake = 2.0f * middle >= 1.5f;

				// 1.4 on what its motion adds: the wave fields do not resolve the sharp pressure edges of a hull, which make
				// most of its wake
				state.press.clear();
				state.strikes.clear();
				const float wetted = water_physics::rigid_body_sources(b, on_wake ? 1.0f : 0.5f, 1.4f * strength, state.press, state.strikes);

				if (b.has_prev)
				{
					const float mx = b.centre[0] - b.prev_centre[0];
					const float my = b.centre[1] - b.prev_centre[1];
					const float mz = b.centre[2] - b.prev_centre[2];
					const bool moved = mx * mx + my * my + mz * mz > (0.15f * b.dt) * (0.15f * b.dt);
					t.still = moved ? 0.0f : t.still + b.dt;
				}

				// The dent is what holds a body up, so it is only there where the water carries the body. Something fixed or
				// resting on the bottom (props, piles and rocks standing in the water, a helicopter or a car sitting on the
				// ground) has the flat surface around it once it has been still for 2 s. Boats always float. And no dent holds
				// more than the body weighs (1.5 x for what its motion adds)
				float carried = 1.0f;
				if (pool != POOL_PED && !t.boat && t.still > 2.0f) {
					carried = 0.0f;
				}
				else if (t.mass > 0.0f)
				{
					float volume = 0.0f;
					for (const auto& p : state.press) {
						volume += p.depth * PI * p.radius * p.radius;
					}

					const float limit = 1.5f * t.mass / water_physics::RHO;
					if (volume > limit) {
						carried = limit / volume;
					}
				}

				// Something that is already in the water when it is first seen (the game spawns debris and boats around the
				// camera) did not fall in: a dent appearing at once and the body finding its floating height would ring the
				// water. It adds nothing for a second, then its dent fades in over two more
				constexpr float SETTLE_TIME = 3.0f;
				constexpr float SETTLE_FADE = 2.0f;

				if (first_look && wetted > 0.0f) {
					t.settling = SETTLE_TIME;
				}

				const float settled = water_physics::clamp01((SETTLE_FADE - t.settling) / SETTLE_FADE);
				t.settling = std::max(t.settling - b.dt, 0.0f);
				const float gain = scale * settled;

				if (carried * gain > 0.0f)
				{
					for (const auto& p : state.press) {
						found.bodies.push_back({ p.x, p.y, p.radius, p.depth * carried * gain, on_wake });
					}
				}

				for (const auto& s : state.strikes)
				{
					if (gain <= 0.0f) {
						break;
					}

					found.splashes.push_back({ s.x, s.y, on_wake ? std::max(s.radius, 1.0f) : std::min(s.radius, 0.95f), s.speed * gain });

					// something big hitting the water also sends out the short rings the 1 m field cannot carry
					if (on_wake) {
						found.splashes.push_back({ s.x, s.y, 0.7f, s.speed * 0.6f * gain });
					}
				}

				// what floats: the game's buoyancy does not get the dent under it
				if (pool != POOL_PED && wetted > 0.0f)
				{
					const float len = std::sqrt(b.axis[1][0] * b.axis[1][0] + b.axis[1][1] * b.axis[1][1]);
					if (len > 0.1f) {
						found.hulls.push_back({ b.centre[0], b.centre[1], b.axis[1][0] / len, b.axis[1][1] / len, 2.0f * b.half[1], 2.0f * b.half[0], t.water });
					}
				}

				return wetted > 0.0f || !state.strikes.empty();
			}

			/// The slot and reuse counter of a pool slot have to be the script handle: checked once per pool against a native.
			/// @return false when the check has to wait for the next entity
			bool check_pool_handles(natives* n, const int pool, const int handle, const float x, const float y)
			{
				auto& state = g_state;
				float cx = 0.0f;
				float cy = 0.0f;
				float cz = 0.0f;

				if (pool == POOL_VEHICLE) {
					n->GetCarCoordinates(handle, &cx, &cy, &cz);
				} else if (pool == POOL_PED) {
					n->GetCharCoordinates(handle, &cx, &cy, &cz);
				} else {
					n->GetObjectCoordinates(handle, &cx, &cy, &cz);
				}

				const bool matches = std::abs(cx - x) < 0.5f && std::abs(cy - y) < 0.5f;

				// a ped in a vehicle reports the vehicle
				if (pool == POOL_PED && !matches && n->IsCharInAnyCar(handle)) {
					return false;
				}

				state.pool_ok[pool] = matches;
				state.pool_checked[pool] = true;

				if (!matches) {
					shared::common::log("WaterDynamics", "Entity pool handles do not match, these entities do not disturb the water.", shared::common::LOG_TYPE::LOG_TYPE_ERROR, false);
				}

				return true;
			}

			/// Looks at everything rigid near the player.
			/// @param player_ped	handle of the player, who has his own body model
			void run_pass(natives* n, const natives::Ped player_ped, const float px, const float py, const float strength, found_s& found)
			{
				static const auto cs = comp_settings::get();
				constexpr float RANGE[POOL_COUNT] = { 170.0f, 90.0f, 70.0f };
				constexpr int MAX_OBJECTS = 128;

				auto& state = g_state;
				void** const pools[POOL_COUNT] = { game::pVehiclePool, game::pPedPool, game::pObjectPool };
				const float boat_scale = std::max(cs->water_interactive_boat_scale._float(), 0.0f);
				const float heli_scale = std::max(cs->water_interactive_heli_scale._float(), 0.0f);
				const float pool_scale[POOL_COUNT] =
				{
					std::max(cs->water_interactive_vehicle_scale._float(), 0.0f),
					std::max(cs->water_interactive_ped_scale._float(), 0.0f),
					std::max(cs->water_interactive_object_scale._float(), 0.0f),
				};

				state.object_min_mass = std::max(cs->water_interactive_object_min_mass._float(), 0.0f);
				state.pass++;
				int objects_done = 0;

				for (int pi = 0; pi < POOL_COUNT; pi++)
				{
					// a kind that is scaled to nothing is not looked at
					const auto pool = get_pool(pools[pi]);
					if (!pool || (pi != POOL_VEHICLE && pool_scale[pi] <= 0.0f)) {
						continue;
					}

					auto& hot = state.hot[pi];
					if (hot.size() != static_cast<size_t>(pool->count)) {
						hot.assign(static_cast<size_t>(pool->count), 1u);
					}

					for (int i = 0; i < pool->count; i++)
					{
						// free slot
						const std::int8_t flag = pool->flags[i];
						if (flag < 0)
						{
							hot[i] = 1u;
							continue;
						}

						// what is in or near the water is followed on every pass, the rest on every 4th.
						// Vehicles always: wakes need their motion
						if (pi != POOL_VEHICLE && !hot[i] && ((static_cast<std::uint32_t>(i) + state.pass) & 3u) != 0u) {
							continue;
						}

						const float* m = entity_matrix(pool, i);
						if (!m) {
							continue;
						}

						if (pi == POOL_VEHICLE) {
							found.vehicles_seen++;
						}

						const float x = m[12];
						const float y = m[13];
						const float dx = x - px;
						const float dy = y - py;
						if (!std::isfinite(x) || !std::isfinite(y) || dx * dx + dy * dy > RANGE[pi] * RANGE[pi]) {
							continue;
						}

						const int handle = (i << 8) | static_cast<std::uint8_t>(flag);
						if (!state.pool_checked[pi] && !check_pool_handles(n, pi, handle, x, y)) {
							continue;
						}

						if (!state.pool_ok[pi]) {
							break;
						}

						if (pi == POOL_PED && handle == player_ped) {
							continue;
						}

						if (pi == POOL_OBJECT && ++objects_done > MAX_OBJECTS) {
							break;
						}

						float scale = pool_scale[pi];
						if (pi == POOL_VEHICLE)
						{
							// the kind of a vehicle is known after its first look
							const auto it = state.tracks.find(static_cast<std::uint32_t>(handle));
							if (it != state.tracks.end() && it->second.boat) {
								scale = boat_scale;
							}
							else if (it != state.tracks.end() && it->second.heli) {
								scale = heli_scale;
							}
						}

						if (scale <= 0.0f) {
							continue;
						}

						hot[i] = track_entity(n, pi, handle, m, strength, scale, found) ? 1u : 0u;
					}
				}

				// forget what is gone
				if (state.pass % 60u == 0u)
				{
					std::erase_if(state.tracks, [&](const auto& it)
						{
							return state.frame - it.second.seen > 240u;
						});
				}
			}
		}

		// The player's body in the water. The pose is not read from the game: capsules are posed from his position, heading,
		// pace and state (front crawl, treading water, wading) and sampled ~25 cm apart. Every point near the surface pushes on it with
		//  - the dynamic pressure of its motion: p / (rho g) = Cp v^2 / 2g, by its submerged share, fading with depth
		//  - the volume it carries through the surface: + plunging down, - lifting out
		namespace player
		{
			enum body_point_e
			{
				HEAD, NECK, PELVIS,
				L_THIGH, L_CALF, L_FOOT, L_TOE,
				R_THIGH, R_CALF, R_FOOT, R_TOE,
				L_UPPERARM, L_FOREARM, L_HAND,
				R_UPPERARM, R_FOREARM, R_HAND,
			};

			struct part_s
			{
				body_point_e a, b;
				float r;   // capsule radius (m)
				float cp;  // pressure coefficient
			};

			constexpr part_s PARTS[] =
			{
				{ HEAD, HEAD, 0.11f, 1.0f },
				{ PELVIS, NECK, 0.15f, 1.1f },      // trunk
				{ L_THIGH, L_CALF, 0.075f, 0.9f },
				{ R_THIGH, R_CALF, 0.075f, 0.9f },
				{ L_CALF, L_FOOT, 0.055f, 0.9f },
				{ R_CALF, R_FOOT, 0.055f, 0.9f },
				{ L_FOOT, L_TOE, 0.05f, 1.2f },     // flat paddle when kicking
				{ R_FOOT, R_TOE, 0.05f, 1.2f },
				{ L_UPPERARM, L_FOREARM, 0.05f, 0.9f },
				{ R_UPPERARM, R_FOREARM, 0.05f, 0.9f },
				{ L_FOREARM, L_HAND, 0.042f, 0.9f },
				{ R_FOREARM, R_HAND, 0.042f, 0.9f },
				{ L_HAND, L_HAND, 0.065f, 1.3f },   // cupped hand
				{ R_HAND, R_HAND, 0.065f, 1.3f },
			};

			constexpr int MAX_POINTS = 48;

			enum class mode_e
			{
				NONE,
				CRAWL,
				TREADING,
				WADING,
			};

			struct pose_s
			{
				float t = 0.0f;      // running time (s)
				float px = 0.0f;
				float py = 0.0f;
				float pz = 0.0f;
				float water = 0.0f;  // level of the calm surface
				float fx = 0.0f;     // heading
				float fy = 1.0f;
				float speed = 0.0f;
				mode_e mode = mode_e::NONE;
			};

			struct state_s
			{
				pose_s pose;

				// sampled points of the last frame and their smoothed velocities
				float px[MAX_POINTS] = {};
				float py[MAX_POINTS] = {};
				float pz[MAX_POINTS] = {};
				float vx[MAX_POINTS] = {};
				float vy[MAX_POINTS] = {};
				float vz[MAX_POINTS] = {};
				int points = 0;

				bool was_in_water = false;
				float fall_speed = 0.0f;   // vertical speed of the last frame
				bool has_position = false;
				float last_x = 0.0f;
				float last_y = 0.0f;
				float move_speed = 0.0f;   // horizontal speed from his movement (smoothed)
				float entry_time = -10.0f; // module time he last jumped or fell in
			};

			state_s g_state;

			bool is_left(const body_point_e point)
			{
				return point == L_THIGH || point == L_CALF || point == L_FOOT || point == L_TOE
					|| point == L_UPPERARM || point == L_FOREARM || point == L_HAND;
			}

			/// World position of a point of the posed body.
			Vector body_point(const body_point_e point)
			{
				constexpr float TWO_PI = water_physics::TWO_PI;
				const auto& k = g_state.pose;
				const float rx = k.fy;
				const float ry = -k.fx;

				// forward, right, height
				const auto at = [&](const float f, const float r, const float z)
					{
						return Vector(k.px + k.fx * f + rx * r, k.py + k.fy * f + ry * r, z);
					};

				// both sides half a cycle apart
				const float side = is_left(point) ? -1.0f : 1.0f;
				const float side_phase = is_left(point) ? 0.0f : 0.5f;

				// horizontal just under the surface, alternating arm strokes, flutter kick
				if (k.mode == mode_e::CRAWL)
				{
					const float period = std::clamp(1.6f - 0.3f * k.speed, 0.9f, 1.6f);
					const float stroke = TWO_PI * (k.t / period + side_phase);
					const float kick = 0.15f * std::sin(TWO_PI * (k.t * 2.4f + side_phase));

					switch (point)
					{
					case HEAD: return at(0.75f, 0.0f, k.water + 0.06f);
					case NECK: return at(0.55f, 0.0f, k.water + 0.0f);
					case PELVIS: return at(-0.15f, 0.0f, k.water - 0.08f);
					case L_THIGH: case R_THIGH: return at(-0.2f, side * 0.1f, k.water - 0.1f);
					case L_CALF: case R_CALF: return at(-0.6f, side * 0.1f, k.water - 0.18f + kick * 0.5f);
					case L_FOOT: case R_FOOT: return at(-1.0f, side * 0.1f, k.water - 0.15f + kick);
					case L_TOE: case R_TOE: return at(-1.15f, side * 0.1f, k.water - 0.12f + kick);
					case L_UPPERARM: case R_UPPERARM: return at(0.45f, side * 0.22f, k.water);
					case L_FOREARM: case R_FOREARM: return at(0.45f + 0.33f * std::cos(stroke), side * 0.26f, k.water + 0.28f * std::sin(stroke) + 0.1f);
					case L_HAND: case R_HAND: return at(0.45f + 0.65f * std::cos(stroke), side * 0.28f, k.water + 0.55f * std::sin(stroke));
					}
				}

				// upright, head out, sculling hands, eggbeater legs
				if (k.mode == mode_e::TREADING)
				{
					const float bob = 0.04f * std::sin(TWO_PI * 0.7f * k.t);
					const float scull = std::sin(TWO_PI * (0.8f * k.t + side_phase));
					const float egg = std::sin(TWO_PI * (1.1f * k.t + side_phase));

					switch (point)
					{
					case HEAD: return at(0.0f, 0.0f, k.water + 0.2f + bob);
					case NECK: return at(0.0f, 0.0f, k.water + 0.02f + bob);
					case PELVIS: return at(0.0f, 0.0f, k.water - 0.7f + bob);
					case L_THIGH: case R_THIGH: return at(0.0f, side * 0.1f, k.water - 0.75f + bob);
					case L_CALF: case R_CALF: return at(0.25f * egg, side * 0.3f, k.water - 1.0f + 0.1f * egg);
					case L_FOOT: case R_FOOT: return at(0.1f * egg, side * 0.25f, k.water - 1.35f);
					case L_TOE: case R_TOE: return at(0.25f + 0.1f * egg, side * 0.25f, k.water - 1.35f);
					case L_UPPERARM: case R_UPPERARM: return at(0.0f, side * 0.22f, k.water - 0.05f + bob);
					case L_FOREARM: case R_FOREARM: return at(0.1f + 0.1f * scull, side * 0.38f, k.water - 0.15f);
					case L_HAND: case R_HAND: return at(0.2f + 0.22f * scull, side * 0.5f, k.water - 0.14f + 0.04f * std::cos(TWO_PI * (0.8f * k.t + side_phase)));
					}
				}

				// standing, legs swing at the walking cadence
				if (k.mode == mode_e::WADING)
				{
					const float cadence = std::max(k.speed * 0.9f, 0.6f);
					const float swing = std::sin(TWO_PI * (cadence * 0.5f * k.t + side_phase));
					const float lift = 0.12f * std::max(0.0f, std::cos(TWO_PI * (cadence * 0.5f * k.t + side_phase)));
					const float feet = k.pz - 1.0f;

					switch (point)
					{
					case HEAD: return at(0.0f, 0.0f, k.pz + 0.75f);
					case NECK: return at(0.0f, 0.0f, k.pz + 0.55f);
					case PELVIS: return at(0.0f, 0.0f, k.pz);
					case L_THIGH: case R_THIGH: return at(0.0f, side * 0.1f, k.pz - 0.05f);
					case L_CALF: case R_CALF: return at(0.18f * swing, side * 0.11f, feet + 0.5f + lift);
					case L_FOOT: case R_FOOT: return at(0.35f * swing, side * 0.12f, feet + 0.08f + lift);
					case L_TOE: case R_TOE: return at(0.35f * swing + 0.15f, side * 0.12f, feet + 0.05f + lift);
					case L_UPPERARM: case R_UPPERARM: return at(0.0f, side * 0.22f, k.pz + 0.5f);
					case L_FOREARM: case R_FOREARM: return at(-0.1f * swing, side * 0.26f, k.pz + 0.2f);
					case L_HAND: case R_HAND: return at(-0.2f * swing, side * 0.28f, k.pz - 0.1f);
					}
				}

				return Vector(0.0f, 0.0f, 0.0f);
			}

			/// Pressure and impulse sources of the posed body.
			/// @param now		module time (s)
			/// @param scale	strength of what the body adds
			void body_sources(const float dt, const float now, const float scale, entities::found_s& found)
			{
				auto& s = g_state;
				if (dt <= 0.0f || s.pose.mode == mode_e::NONE)
				{
					s.points = 0;
					return;
				}

				// Mean motion of the body in the last frame. A whole body dropping in is handled once by the entry impulse,
				// here only the motion of the parts relative to it plunges into the water
				float body_vz = 0.0f;
				for (int q = 0; q < s.points; q++) {
					body_vz += s.vz[q] / static_cast<float>(s.points);
				}

				const bool falling_in = body_vz < -2.0f || now - s.entry_time < 0.4f;
				const float water = s.pose.water;
				int idx = 0;

				for (const auto& part : PARTS)
				{
					const Vector a = body_point(part.a);
					const Vector b = part.b != part.a ? body_point(part.b) : a;
					const float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
					const int samples = part.a == part.b ? 1 : std::clamp(static_cast<int>(len / 0.25f + 0.5f), 1, 3);

					for (int j = 0; j < samples && idx < MAX_POINTS; j++, idx++)
					{
						const float u = (static_cast<float>(j) + 0.5f) / static_cast<float>(samples);
						const float x = a.x + (b.x - a.x) * u;
						const float y = a.y + (b.y - a.y) * u;
						const float z = a.z + (b.z - a.z) * u;

						// smoothed velocity of this point, reset after a teleport
						if (idx < s.points)
						{
							const float nvx = (x - s.px[idx]) / dt;
							const float nvy = (y - s.py[idx]) / dt;
							const float nvz = (z - s.pz[idx]) / dt;

							if (nvx * nvx + nvy * nvy + nvz * nvz < 30.0f * 30.0f)
							{
								const float k = std::min(dt * 25.0f, 1.0f);
								s.vx[idx] += (nvx - s.vx[idx]) * k;
								s.vy[idx] += (nvy - s.vy[idx]) * k;
								s.vz[idx] += (nvz - s.vz[idx]) * k;
							}
							else
							{
								s.vx[idx] = 0.0f;
								s.vy[idx] = 0.0f;
								s.vz[idx] = 0.0f;
							}
						}
						else
						{
							s.vx[idx] = 0.0f;
							s.vy[idx] = 0.0f;
							s.vz[idx] = 0.0f;
						}

						s.px[idx] = x;
						s.py[idx] = y;
						s.pz[idx] = z;

						// submerged share of this slice, deeper parts barely move the surface
						const float r = part.r;
						const float d = water - z;
						const float submerged = water_physics::clamp01((d + r) / (2.0f * r));
						const float near_surface = std::exp(-std::max(d - r, 0.0f) / 0.35f);
						if (submerged <= 0.0f || near_surface < 0.02f) {
							continue;
						}

						const float vx = s.vx[idx];
						const float vy = s.vy[idx];
						const float vz = s.vz[idx];
						const float vh2 = vx * vx + vy * vy;
						const float vh = std::sqrt(vh2);

						// dynamic pressure at the leading side + the volume carried through the surface
						const float p_motion = part.cp * vh2 / (2.0f * water_physics::G) * submerged * near_surface;
						const float crossing = 4.0f * submerged * (1.0f - submerged);
						const float p_cross = part.cp * (-vz) * std::abs(vz) / (2.0f * water_physics::G) * std::max(crossing, 0.15f * submerged) * near_surface;

						// the footprints of neighbouring points overlap ~2 - 2.5x along a limb
						constexpr float OVERLAP = 0.45f;
						const float p = std::clamp((p_motion + p_cross) * OVERLAP, -0.1f, 0.15f);
						if (falling_in || std::abs(p) < 0.002f) {
							continue;
						}

						const float lead = vh > 0.05f ? r / vh : 0.0f;
						const float footprint = std::max(r * 2.5f, 0.3f);
						found.bodies.push_back({ x + vx * lead, y + vy * lead, footprint, p * scale });

						// a fast plunge relative to the body (a hand slapping in, a foot kicking down) also strikes the surface
						const float rel_vz = vz - body_vz;
						if (rel_vz < -1.2f && crossing > 0.3f) {
							found.splashes.push_back({ x, y, std::max(r * 3.0f, 0.4f), std::min(-rel_vz * 0.4f * (r / 0.1f), 2.0f) * crossing * scale });
						}
					}
				}

				s.points = idx;
			}

			/// The player on foot: poses his body from what the game tells about him and adds its sources.
			/// @param now		module time (s)
			/// @param scale	strength of what the body adds
			void update(natives* n, const natives::Ped ped, const float px, const float py, const float pz, const float dt, const float now, const float scale,
				entities::found_s& found)
			{
				auto& s = g_state;
				float vx = 0.0f;
				float vy = 0.0f;
				float vz = 0.0f;
				n->GetCharVelocity(ped, &vx, &vy, &vz);

				// the velocity native reads ~0 for a swimming ped: horizontal speed from his movement, reset after a teleport
				if (s.has_position && dt > 0.0f)
				{
					const float mx = (px - s.last_x) / dt;
					const float my = (py - s.last_y) / dt;
					const float moved = std::sqrt(mx * mx + my * my);
					s.move_speed = moved < 15.0f ? s.move_speed + (moved - s.move_speed) * std::min(dt * 6.0f, 1.0f) : 0.0f;
				}

				s.last_x = px;
				s.last_y = py;
				s.has_position = true;
				const float speed = std::max(std::sqrt(vx * vx + vy * vy), s.move_speed);

				float water = 0.0f;
				const bool water_here = n->GetWaterHeightNoWaves(px, py, pz, &water);
				const bool swimming = n->IsCharSwimming(ped);
				const float feet = pz - 1.0f;
				const float depth = water_here ? water - feet : -1.0f;
				const bool in_water = swimming || depth > 0.05f;

				// jumped or fell in: the body punches a cavity at most of its entry speed, it rings out in short waves
				if (in_water && !s.was_in_water && s.fall_speed < -1.5f)
				{
					const float v = std::min(-s.fall_speed, 12.0f);
					found.splashes.push_back({ px, py, 0.7f, v * 0.8f * scale });
					s.entry_time = now;
				}

				float heading = 0.0f;
				n->GetCharHeading(ped, &heading);
				heading *= DEG_TO_RAD;

				auto& pose = s.pose;
				pose.t += dt;
				pose.px = px;
				pose.py = py;
				pose.pz = pz;
				pose.water = water;
				pose.fx = -std::sin(heading);
				pose.fy = std::cos(heading);
				pose.speed = speed;

				if (!water_here) {
					pose.mode = mode_e::NONE;
				}
				else if (swimming) {
					pose.mode = speed > 0.4f ? mode_e::CRAWL : mode_e::TREADING;
				}
				else {
					pose.mode = in_water && depth < 1.4f ? mode_e::WADING : mode_e::NONE;
				}

				body_sources(dt, now, scale, found);

				s.was_in_water = in_water;
				s.fall_speed = vz;
			}
		}

		// game thread state of update()
		struct game_state_s
		{
			depth::grid_s depth_close;
			depth::grid_s depth_wide;
			std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();
			std::uint32_t frames_seen = 0u;
			std::uint32_t depth_publish_tick = 0u;
			int explosions_near_water = 0;
			int splashes = 0;
			float water_level = 0.0f; // level of the water around the player: under him, else the one last found near the camera
		};

		game_state_s g_game;

		/// Bullets and explosions near the water, from the hooks.
		void collect_impacts(natives* n, const bool interactive, const float scale, std::vector<sources::splash_s>& splashes, std::vector<sources::blast_s>& blasts)
		{
			std::vector<events::shot_s> shots;
			std::vector<events::explosion_s> explosions;
			{
				std::lock_guard lock(events::mutex);
				shots.swap(events::shots);
				explosions.swap(events::explosions);
			}

			if (!interactive || scale <= 0.0f) {
				return;
			}

			// each bullet strikes a small sharp ring where the game found it meeting the surface
			for (const auto& shot : shots) {
				splashes.push_back({ shot.x, shot.y, 0.3f, 2.5f * scale });
			}

			for (const auto& e : explosions)
			{
				float water = 0.0f;
				if (!n->GetWaterHeightNoWaves(e.x, e.y, e.z, &water)) {
					continue;
				}

				const float height = e.z - water;
				if (height < e.radius * 1.2f + 1.0f && height > -6.0f)
				{
					blasts.push_back({ e.x, e.y, e.radius * std::min(scale, 2.0f), std::max(height, 0.0f) });
					g_game.explosions_near_water++;
				}
			}
		}

		/// Scans the shore depth maps around the render camera and hands changed ones to the simulation thread.
		void update_depth_maps(natives* n)
		{
			constexpr int CLOSE_BUDGET = 16; // cells per frame: the close map fills in ~10 s
			constexpr int WIDE_BUDGET = 6;

			float cam_x = 0.0f;
			float cam_y = 0.0f;
			{
				std::lock_guard lock(g_input_mutex);
				if (!g_input.have_cam) {
					return;
				}

				cam_x = g_input.cam_x;
				cam_y = g_input.cam_y;
			}

			const float level_hint = entities::g_state.last_water_level;
			g_game.depth_close.scan(n, cam_x, cam_y, level_hint, CLOSE_BUDGET);
			g_game.depth_wide.scan(n, cam_x, cam_y, level_hint, WIDE_BUDGET);

			if (++g_game.depth_publish_tick % 15u == 0u)
			{
				std::lock_guard lock(g_input_mutex);
				g_game.depth_close.publish(g_input.depth_close);
				g_game.depth_wide.publish(g_input.depth_wide);
			}
		}
	}

	void init()
	{
		g_game.depth_close.cell = 1.0f;
		g_game.depth_wide.cell = 4.0f;

		coupling::init();
		events::init();
	}

	bool coupling_active() {
		return coupling::active.load();
	}

	stats_s get_stats()
	{
		std::lock_guard lock(g_stats_mutex);
		return g_stats;
	}

	const frame_s& frame() {
		return output::front_frame;
	}

	void update()
	{
		static const auto cs = comp_settings::get();
		constexpr std::uint32_t LOADING_FRAMES = 600u; // no natives while the game is still loading

		const auto now = std::chrono::steady_clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(now - g_game.last_update).count(), 0.0f, 0.1f);
		g_game.last_update = now;

		// the sea needs a while to build up and calm down
		if (game::pWeatherWind)
		{
			const float wind = std::clamp(*game::pWeatherWind, 0.0f, 1.5f);
			const float current = g_wind.load();
			g_wind.store(current + (wind - current) * std::min(dt * 0.5f, 1.0f));
		}

		if (g_game.frames_seen < LOADING_FRAMES)
		{
			g_game.frames_seen++;
			return;
		}

		const auto n = natives::get();
		if (!n || (game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive)) {
			return;
		}

		if (remix_vars::is_paused() || (game::m_CodePause && *game::m_CodePause)) {
			return;
		}

		auto& tracking = entities::g_state;
		tracking.frame++;
		tracking.time += dt;

		const bool interactive = cs->water_interactive._bool();
		if (interactive || cs->water_wind_waves._bool()) {
			update_depth_maps(n);
		}

		const float strength = std::clamp(cs->water_interactive_strength._float(), 0.0f, 4.0f);
		const float ped_scale = std::max(cs->water_interactive_ped_scale._float(), 0.0f);
		const float impact_scale = strength * std::max(cs->water_interactive_impact_scale._float(), 0.0f);

		const auto player_id = n->ConvertIntToPlayerindex(n->GetPlayerId());
		natives::Ped ped = 0;
		if (n->IsPlayerPlaying(player_id)) {
			n->GetPlayerChar(player_id, &ped);
		}

		float px = 0.0f;
		float py = 0.0f;
		float pz = 0.0f;
		const bool have_player = ped && !n->IsCharDead(ped);
		if (have_player) {
			n->GetCharCoordinates(ped, &px, &py, &pz);
		}

		// which water the player is at decides the wind waves things float on (inland water has its own scale)
		if (have_player && tracking.frame % 15u == 0u)
		{
			float level = 0.0f;
			if (n->GetWaterHeightNoWaves(px, py, pz, &level)) {
				g_game.water_level = level;
			}
			else if (g_game.depth_close.has_level) {
				g_game.water_level = g_game.depth_close.level;
			}
		}

		entities::found_s found;

		if (interactive && have_player)
		{
			// the simulation takes a new state 30 times a second: the entities are looked at no more often than that,
			// what they added is kept in between
			tracking.since_pass += dt;
			if (tracking.since_pass >= STEP * 0.9f)
			{
				tracking.since_pass = 0.0f;
				entities::run_pass(n, ped, px, py, strength, found);
				tracking.kept.hulls = found.hulls;
				tracking.kept.bodies = found.bodies;
				tracking.kept.helis = found.helis;
				tracking.kept.vehicles_seen = found.vehicles_seen;
			}
			else
			{
				found.hulls = tracking.kept.hulls;
				found.bodies = tracking.kept.bodies;
				found.helis = tracking.kept.helis;
				found.vehicles_seen = tracking.kept.vehicles_seen;
			}

			if (!n->IsCharInAnyCar(ped)) {
				player::update(n, ped, px, py, pz, dt, tracking.time, strength * ped_scale, found);
			}
		}

		std::vector<sources::blast_s> blasts;
		collect_impacts(n, interactive, impact_scale, found.splashes, blasts);
		g_game.splashes += static_cast<int>(found.splashes.size());

		{
			std::lock_guard lock(g_input_mutex);
			g_input.hulls = std::move(found.hulls);
			g_input.bodies = std::move(found.bodies);
			g_input.helis = std::move(found.helis);

			if (g_input.splashes.size() < 256u) {
				g_input.splashes.insert(g_input.splashes.end(), found.splashes.begin(), found.splashes.end());
			}

			g_input.blasts.insert(g_input.blasts.end(), blasts.begin(), blasts.end());
			g_input.game_dt = std::min(g_input.game_dt + dt, 0.25f);
			g_input.player_x = px;
			g_input.player_y = py;
			g_input.water_level = g_game.water_level;
		}

		{
			std::lock_guard lock(g_stats_mutex);
			g_stats.vehicles_seen = found.vehicles_seen;
			g_stats.pool_ok = tracking.pool_ok[entities::POOL_VEHICLE];
			g_stats.explosions = g_game.explosions_near_water;
			g_stats.splashes = g_game.splashes;
		}
	}

	void render_step(const float cam_x, const float cam_y, const float bake_radius)
	{
		// the simulation thread starts with the first water render
		static std::once_flag thread_started;
		std::call_once(thread_started, []
			{
				std::thread(sim::thread_main).detach();
			});

		const settings_s settings = read_settings(bake_radius);
		stats_s stats;
		{
			std::lock_guard lock(g_input_mutex);

			// switched off and on again: start calm
			if (settings.interactive != g_input.settings.interactive) {
				g_input.reset = true;
			}

			if (!settings.interactive)
			{
				g_input.hulls.clear();
				g_input.bodies.clear();
				g_input.helis.clear();
				g_input.splashes.clear();
				g_input.blasts.clear();
			}

			g_input.settings = settings;
			g_input.cam_x = cam_x;
			g_input.cam_y = cam_y;
			g_input.have_cam = true;
			g_input_pending = true;

			stats.hulls = static_cast<int>(g_input.hulls.size());
			stats.bodies = static_cast<int>(g_input.bodies.size());
			stats.helis = static_cast<int>(g_input.helis.size());
		}

		g_input_cv.notify_one();
		output::take_newest();

		const auto& front = output::front_frame;
		std::lock_guard lock(g_stats_mutex);
		g_stats.hulls = stats.hulls;
		g_stats.bodies = stats.bodies;
		g_stats.helis = stats.helis;
		g_stats.wind = g_wind.load();
		g_stats.wake_rms = front.wake_rms;
		g_stats.ripple_rms = front.ripple_rms;
		g_stats.ocean_hs = front.ocean_hs;
		g_stats.sim_ms = g_sim_ms.load();
	}
}
