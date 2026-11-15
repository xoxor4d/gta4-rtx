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
		// sources handed from the game thread to the simulation thread (metres, m/s)
		/// footprint of a floating vehicle (the game's buoyancy must not see the dent the hull itself presses)
		struct hull_s
		{
			float x, y, dirx, diry, length, beam;
		};

		/// surface pressure of something in the water, as the depth of water it holds down
		struct body_s
		{
			float x, y, radius, depth;
			bool wake = false; // on the 1 m wake field (else the 0.5 m ripple field)
		};

		struct heli_s
		{
			float x, y;     // where the rotor's axis meets the water
			float altitude; // of the skids above the water, along the axis
			float radius;   // rotor
			float mass;     // kg, times the share of its weight the rotor carries right now
		};

		struct splash_s
		{
			float x, y, radius, speed; // surface struck down at speed over a patch of radius
		};

		struct blast_s
		{
			float x, y, radius, height;
		};

		struct shared_s
		{
			std::vector<hull_s> hulls;       // latest game frame
			std::vector<body_s> bodies;      // latest game frame
			std::vector<heli_s> helis;       // latest game frame
			std::vector<splash_s> splashes;  // queued until the simulation thread takes them
			std::vector<blast_s> blasts;
			float game_dt = 0.0f;            // game time not yet simulated (0 while paused)
			float cam_x = 0.0f, cam_y = 0.0f;
			bool have_cam = false;
			std::vector<float> depth;        // shore depth map, see depth_grid_s
			int depth_ox = 0, depth_oy = 0;
			std::uint32_t depth_version = 0u;
		};

		std::mutex g_mutex;
		shared_s g_shared;

		// explosions seen by the hook, resolved against the water on the game thread
		struct explosion_s
		{
			float x, y, z, radius;
			int type;
		};

		std::mutex g_explosion_mutex;
		std::vector<explosion_s> g_explosions;

		// bullets that met the water, seen by the hook
		struct shot_s
		{
			float x, y;
		};

		std::mutex g_shot_mutex;
		std::vector<shot_s> g_shots;

		std::atomic<float> g_wind = 0.0f;
		stats_s g_stats;
		std::mutex g_stats_mutex;

		// the game's 100 x 100 wave grid (2 unit cells around the camera) is what buoyancy reads:
		// our heights are written into it after each of its steps (double buffered, filled by the simulation thread)
		constexpr int GAME_GRID_CELLS = 100;
		std::array<float, GAME_GRID_CELLS * GAME_GRID_CELLS> g_cpl[2];
		std::atomic<int> g_cpl_ready = -1;
		std::atomic<bool> g_cpl_hooked = false;

		DETOUR_TYPEDEF(CWater_UpdateDynamicWater, void, __cdecl, void* params);

		void __cdecl CWater_UpdateDynamicWater_hk(void* params)
		{
			CWater_UpdateDynamicWater_og(params);

			if (const int r = g_cpl_ready.load(); r >= 0)
			{
				// our fields own the motion: no velocities left for the game's step
				std::memcpy(game::pWaterDynamicHeights, g_cpl[r].data(), sizeof(float) * g_cpl[r].size());
				std::memset(game::pWaterDynamicVelocities, 0, sizeof(float) * g_cpl[r].size());
			}
		}

		// simulation thread: owns the fields, publishes copies of the heights (triple buffered).
		// the propagator is exact, so 30 Hz shows the same waves as 60 Hz
		constexpr float STEP = 1.0f / 30.0f;

		struct output_s
		{
			std::vector<float> wake, ripple, o_long, o_mid, o_short, o_dx, o_dy, shore, beach, detail;
			std::vector<float> wake_lattice[3];
			std::vector<float> nodes_long, nodes_mid;
			frame_s frame;
		};

		output_s g_out[3];
		std::mutex g_out_mutex;
		int g_out_latest = -1;  // newest complete buffer
		int g_out_front = -1;   // read by the render thread this frame
		frame_s g_front_frame;

		std::condition_variable g_cv;
		std::atomic<bool> g_work = false;
		std::thread g_thread;
		std::atomic<bool> g_thread_started = false;
		std::atomic<float> g_sim_ms = 0.0f;
		std::atomic<bool> g_reset = false;

		int pick_free_buffer()
		{
			std::lock_guard lock(g_out_mutex);
			for (int i = 0; i < 3; i++)
			{
				if (i != g_out_latest && i != g_out_front) {
					return i;
				}
			}

			return -1;
		}

		void publish(const int idx)
		{
			std::lock_guard lock(g_out_mutex);
			g_out_latest = idx;
		}

		void copy_grid(std::vector<float>& dst, const water_physics::ewave_s& f, water_physics::grid_view_s& view)
		{
			dst = f.h;
			view = { f.n, f.dx, f.ox, f.oy, false, f.placed ? dst.data() : nullptr };
		}

		/// 1-2-1 filter along both axes of an n x n grid: removes the waves of two cells that a mesh of the same cell size
		/// shows as spikes (response cos^2(k dx / 2): 0 at 2 cells, 0.5 at 4, 0.85 at 8)
		void smooth_121(std::vector<float>& v, const int n, const bool periodic, const int s = 1)
		{
			if (v.size() != static_cast<size_t>(n) * n) {
				return;
			}

			static thread_local std::vector<float> tmp;
			tmp.resize(v.size());
			const auto at = [&](const int i) { return periodic ? (i + n) % n : std::clamp(i, 0, n - 1); };

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

			// along the columns back into v, row by row
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

		water_physics::grid_view_s ocean_view(const std::vector<float>& h)
		{
			constexpr float CELL = water_physics::ocean_s::L / water_physics::ocean_s::N;
			return { water_physics::ocean_s::N, CELL, 0, 0, true, h.data() };
		}

		void sim_thread()
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			shared::common::profiler::name_current_thread("WaterSim");

			// wakes, blast waves, rotor wash: 256 m at 1 m, waves live ~30 s
			// ripples: 128 m at 0.5 m, effective viscosity of surface films, ~10 s
			water_physics::ewave_s wake, ripple;
			wake.init(256, 1.0f, STEP, 20, 2e-4f, 30.0f);
			ripple.init(256, 0.5f, STEP, 24, 1.5e-3f, 10.0f);

			water_physics::ocean_s ocean;
			ocean.init();

			// water ruffled by rotor outwash: how much of the surface is covered with wind ripples there (0 - 1), 1 m cells
			// around the camera. see "rotor downwash" below
			constexpr int RUFFLE_N = 128;
			std::vector<float> ruffle(static_cast<size_t>(RUFFLE_N) * RUFFLE_N, 0.0f), ruffle_tmp;
			int ruffle_ox = 0, ruffle_oy = 0;
			bool ruffle_placed = false;
			float ruffle_peak = 0.0f; // largest value on the grid
			float ruffle_time = 0.0f; // s the ripples have been running
			float ruffle_wind = 0.0f; // s since the wind was last applied (every third update)
			std::uint32_t ruffle_tick = 0u;

			// the wind ripples themselves: 12 short waves (1 - 2.2 m, from the shortest the 0.5 unit mesh shows) in all
			// directions, each running at its own deep water speed. together a slope of ~0.22 rms where the water is fully ruffled
			constexpr int RUFFLE_WAVES = 12;
			struct ruffle_wave_s { float kx, ky, omega, amp, phase; };
			ruffle_wave_s ruffle_waves[RUFFLE_WAVES];
			{
				std::uint32_t seed = 0x9E3779B9u;
				const auto rnd = [&]
					{
						seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
						return static_cast<float>(seed >> 8) * (1.0f / 16777216.0f);
					};

				for (int m = 0; m < RUFFLE_WAVES; m++)
				{
					const float lambda = 1.0f + 1.2f * rnd() * rnd();
					const float k = water_physics::TWO_PI / lambda, dir = water_physics::TWO_PI * rnd();
					const float slope = 0.22f * std::sqrt(2.0f / RUFFLE_WAVES) * (0.6f + 0.8f * rnd());
					ruffle_waves[m] = { k * std::cos(dir), k * std::sin(dir), std::sqrt(water_physics::G * k), slope / k, water_physics::TWO_PI * rnd() };
				}
			}

			float acc = 0.0f;

			// shore depth map (1 m cells) and what it does to the wind sea
			constexpr int DEPTH_N = 96;
			std::vector<float> depth, shore, beach;
			std::vector<float> shore_t, beach_t, shore_s, beach_s; // what the shore asks for, and what is shown (eased towards it)
			int shown_ox = 0, shown_oy = 0;
			int depth_ox = 0, depth_oy = 0;
			std::uint32_t depth_version_seen = 0u;
			bool shore_dirty = false;
			float shore_hs = -1.0f;

			// land (dry or no water) within 6 m of cell i, j of the depth map
			const auto near_land = [&](const int i, const int j) -> bool
				{
					constexpr int N = DEPTH_N, R = 6;
					for (int dj = -R; dj <= R; dj++)
					{
						for (int di = -R; di <= R; di++)
						{
							const int ii = i + di, jj = j + dj;
							if (ii < 0 || jj < 0 || ii >= N || jj >= N || di * di + dj * dj > R * R) {
								continue;
							}

							const float e = depth[static_cast<size_t>(jj) * N + ii];
							if (e == -1.0f || e == 0.0f) {
								return true;
							}
						}
					}

					return false;
				};

			const auto depth_at = [&](const float x, const float y) -> float
				{
					if (depth.empty()) {
						return NAN;
					}

					const int i = static_cast<int>(std::floor(x)) - depth_ox, j = static_cast<int>(std::floor(y)) - depth_oy;
					if (i < 0 || j < 0 || i >= DEPTH_N || j >= DEPTH_N) {
						return NAN;
					}

					return depth[static_cast<size_t>(j) * DEPTH_N + i];
				};

			while (true)
			{
				{
					std::unique_lock lock(g_mutex);
					g_cv.wait(lock, [] { return g_work.load(); });
					g_work = false;
				}

				PROFILE_ZONE_N("WaterSim::update");
				const auto t0 = std::chrono::steady_clock::now();

				shared_s in;
				{
					std::lock_guard lock(g_mutex);
					in.hulls = g_shared.hulls;
					in.bodies = g_shared.bodies;
					in.helis = g_shared.helis;
					in.splashes.swap(g_shared.splashes);
					in.blasts.swap(g_shared.blasts);
					in.game_dt = g_shared.game_dt;
					g_shared.game_dt = 0.0f;
					in.cam_x = g_shared.cam_x;
					in.cam_y = g_shared.cam_y;
					in.have_cam = g_shared.have_cam;

					if (g_shared.depth_version != depth_version_seen)
					{
						depth = g_shared.depth;
						depth_ox = g_shared.depth_ox;
						depth_oy = g_shared.depth_oy;
						depth_version_seen = g_shared.depth_version;
						shore_dirty = true;
					}
				}

				// the game's water lookup fails now and then in open water, and finds piles, buoys or rocks as dry land: a cell most of
				// its neighbours contradict takes their median (two passes, for pairs). left alone it sinks a hole into the surface
				// around it, or flattens the waves on a patch of open water
				if (shore_dirty && !depth.empty())
				{
					constexpr int N = DEPTH_N;
					for (int pass = 0; pass < 2; pass++)
					{
						std::vector<float> out = depth;
						for (int j = 1; j < N - 1; j++)
						{
							for (int i = 1; i < N - 1; i++)
							{
								const size_t c = static_cast<size_t>(j) * N + i;
								const float d = depth[c];
								if (!(d == -1.0f || (d >= 0.0f && d < 0.3f))) {
									continue;
								}

								float wet[8];
								int n_wet = 0, n_deep = 0;
								for (int dj = -1; dj <= 1; dj++)
								{
									for (int di = -1; di <= 1; di++)
									{
										const float e = depth[c + static_cast<size_t>(dj * N + di)];
										if ((di || dj) && e >= 0.0f)
										{
											wet[n_wet++] = e;
											n_deep += e > 1.0f ? 1 : 0;
										}
									}
								}

								// no water amid water (6 of 8: a lookup that failed), dry or shallow amid deep water (7 of 8: a pile or a
								// buoy, a one cell wide pier keeps its two neighbours on the line)
								if ((d == -1.0f && n_wet >= 6) || (d >= 0.0f && n_deep >= 7))
								{
									std::nth_element(wet, wet + n_wet / 2, wet + n_wet);
									out[c] = wet[n_wet / 2];
								}
							}
						}

						depth.swap(out);
					}
				}

				if (g_reset.exchange(false))
				{
					wake.clear();
					ripple.clear();
				}

				if (!in.have_cam) {
					continue;
				}

				wake.recenter(in.cam_x, in.cam_y);
				ripple.recenter(in.cam_x, in.cam_y);

				// wind sea, rebuilt when the wind changed noticeably (the phases stay)
				// wind 0.15 (the game's calm) is light air at ~2 m/s, 0.5 a fresh breeze at ~7 m/s, a storm 18 m/s and more
				static const auto gs = comp_settings::get();
				const bool wind_on = gs->water_wind_waves.get_as<bool>();
				const float wind_c = std::clamp(g_wind.load(), 0.0f, 1.2f);
				const float u = 1.0f + 17.0f * wind_c * std::sqrt(wind_c);
				const float dir = gs->water_far_wave_direction.get_as<float>() * 0.0174532925f;
				const float scale = std::max(gs->water_wind_wave_scale.get_as<float>(), 0.0f);

				if (wind_on && (std::abs(u - ocean.wind_speed) > 0.25f || dir != ocean.wind_dir || scale != ocean.scale))
				{
					ocean.build(u, 4000.0f, dir, scale);
					ocean.evaluate(ocean.time);
				}

				// one-off sources: small ones make ripples, big ones waves
				for (const auto& sp : in.splashes)
				{
					if (sp.radius < 1.0f) {
						ripple.strike(sp.x, sp.y, sp.radius, sp.speed);
					} else {
						wake.strike(sp.x, sp.y, sp.radius, sp.speed);
					}
				}

				// a blast drives the surface down at a few m/s over a crater
				for (const auto& b : in.blasts)
				{
					const float closeness = 1.0f - water_physics::clamp01(b.height / std::max(b.radius * 1.2f, 2.0f));
					if (closeness > 0.0f)
					{
						const float r = std::clamp(b.radius * 0.6f, 2.0f, 6.0f);
						wake.strike(b.x, b.y, r, 10.0f * closeness * std::sqrt(std::clamp(b.radius / 6.0f, 0.3f, 2.0f)));
						ripple.strike(b.x, b.y, r * 0.6f, 3.5f * closeness);
					}
				}

				acc = std::min(acc + in.game_dt, STEP * 4.0f);
				int steps = 0;

				// each wave field runs while something disturbs it or its waves are still out: a calm harbour costs nothing,
				// a boat alone does not run the ripple field, a swimmer alone not the wake field
				bool wake_fed = !in.blasts.empty(), ripple_fed = wake_fed;
				for (const auto& b : in.bodies) {
					(b.wake ? wake_fed : ripple_fed) = true;
				}

				for (const auto& sp : in.splashes) {
					(sp.radius < 1.0f ? ripple_fed : wake_fed) = true;
				}

				// waves below half a millimetre rms are not seen any more
				const bool interactive = gs->water_interactive.get_as<bool>();
				const bool wake_active = interactive && (wake_fed || wake.rms() > 5e-4f);
				const bool ripple_active = interactive && (ripple_fed || ripple.rms() > 5e-4f);
				const bool fields_active = wake_active || ripple_active;

				if (!wake_active && wake.rms() > 0.0f) {
					wake.clear();
				}

				if (!ripple_active && ripple.rms() > 0.0f) {
					ripple.clear();
				}

				while (acc >= STEP)
				{
					if (fields_active)
					{
						// everything in the water presses on it where it is right now (see rigid_body_sources and the player's body)
						for (const auto& b : in.bodies) {
							(b.wake ? wake : ripple).press(b.x, b.y, b.radius, b.depth);
						}

						if (wake_active) {
							wake.step();
						}

						if (ripple_active) {
							ripple.step();
						}

						// breaking: crests steeper than the stokes limit (slope ~0.5) lose height instead of piling up,
						// wake waves above 0.8 m are eased towards 1.2 m (soft knee, phi scaled alike)
						if (wake_active)
						{
							const int nn = wake.n;
							const float inv2dx = 0.5f / wake.dx;

							for (int j = 1; j < nn - 1; j++)
							{
								for (int i = 1; i < nn - 1; i++)
								{
									const size_t c = static_cast<size_t>(j) * nn + i;
									const float gx = (wake.h[c + 1] - wake.h[c - 1]) * inv2dx;
									const float gy = (wake.h[c + nn] - wake.h[c - nn]) * inv2dx;
									const float s2 = gx * gx + gy * gy;

									if (s2 > 0.25f)
									{
										const float k = std::max(0.97f, 1.0f - (std::sqrt(s2) - 0.5f) * 0.2f);
										wake.h[c] *= k;
										wake.phi[c] *= k;
									}

									constexpr float T = 0.8f, A = 1.2f;
									const float hv = wake.h[c];
									if (std::abs(hv) > T)
									{
										const float hs = std::copysign(T + (A - T) * std::tanh((std::abs(hv) - T) / (A - T)), hv);
										const float r = hs / hv;
										wake.h[c] = hs;
										wake.phi[c] *= r;
									}
								}
							}
						}

						// beaches absorb wakes and ripples
						if (!depth.empty())
						{
							for (auto* f : { &wake, &ripple })
							{
								if (!(f == &wake ? wake_active : ripple_active)) {
									continue;
								}

								for (int j = 0; j < f->n; j++)
								{
									const float y = f->world_y0() + static_cast<float>(j) * f->dx;
									for (int i = 0; i < f->n; i++)
									{
										// deep, unknown or no data
										const float d = depth_at(f->world_x0() + static_cast<float>(i) * f->dx, y);
										if (!(d < 0.4f)) {
											continue;
										}

										const float k = d <= 0.02f ? 0.6f : 0.9f + 0.1f * (d / 0.4f);
										const size_t idx = static_cast<size_t>(j) * f->n + i;
										f->h[idx] *= k;
										f->phi[idx] *= k;
									}
								}
							}
						}
					}

					acc -= STEP;
					steps++;
				}

				if (!steps) {
					continue;
				}

				// rotor downwash over water. what a helicopter low over water does (footage of hovering and slow passes): no
				// waves to speak of, but its outwash ruffles the surface into a sharp edged patch of fine wind ripples, streaked
				// outward, several rotor diameters wide; it spreads within a few seconds, follows the helicopter and stays for
				// ~10 s after it has left.
				// - momentum theory: hover induced velocity vh = sqrt(m g / (2 rho_air A)), ~9 m/s for a light helicopter
				// - at the surface the downwash turns into a radial wall jet, ~2 vh just outside the disk, falling off ~1/r
				// - wind ripples form where that wind is above ~3 m/s, the faster the wind the sooner
				{
					const int nox = static_cast<int>(std::floor(in.cam_x)) - RUFFLE_N / 2, noy = static_cast<int>(std::floor(in.cam_y)) - RUFFLE_N / 2;
					if (!ruffle_placed || ruffle_peak <= 0.0f)
					{
						ruffle_ox = nox; ruffle_oy = noy; ruffle_placed = true;
					}
					else if (std::abs(nox - ruffle_ox) > 8 || std::abs(noy - ruffle_oy) > 8)
					{
						ruffle_tmp.assign(ruffle.size(), 0.0f);
						for (int j = 0; j < RUFFLE_N; j++)
						{
							const int sj = j + noy - ruffle_oy;
							if (sj < 0 || sj >= RUFFLE_N) {
								continue;
							}

							for (int i = 0; i < RUFFLE_N; i++)
							{
								const int si = i + nox - ruffle_ox;
								if (si >= 0 && si < RUFFLE_N) {
									ruffle_tmp[static_cast<size_t>(j) * RUFFLE_N + i] = ruffle[static_cast<size_t>(sj) * RUFFLE_N + si];
								}
							}
						}

						ruffle.swap(ruffle_tmp);
						ruffle_ox = nox; ruffle_oy = noy;
					}

					const float dt_u = STEP * static_cast<float>(steps);
					const bool blowing = interactive && !in.helis.empty();

					if (ruffle_peak > 0.0f || blowing)
					{
						ruffle_time += dt_u;

						// the ripples die down in ~7 s
						const float keep = std::exp(-dt_u / 7.0f);
						ruffle_peak = 0.0f;
						for (auto& r : ruffle)
						{
							r *= keep;
							ruffle_peak = std::max(ruffle_peak, r);
						}

						if (ruffle_peak < 0.01f && !blowing)
						{
							std::fill(ruffle.begin(), ruffle.end(), 0.0f);
							ruffle_peak = 0.0f;
							ruffle_time = 0.0f;
						}

						constexpr float RHO_AIR = 1.225f;
						const float str = std::clamp(gs->water_interactive_strength.get_as<float>(), 0.0f, 4.0f);

						// the wind is applied every third update: the patch changes over seconds
						ruffle_wind += dt_u;
						const bool apply = ruffle_tick++ % 3u == 0u;
						const float dt_w = ruffle_wind;
						if (apply) {
							ruffle_wind = 0.0f;
						}

						for (const auto& hl : in.helis)
						{
							if (!interactive || !apply) {
								break;
							}

							// full strength below ~0.75 rotor diameters of height, gone by ~3
							const float d = 2.0f * hl.radius;
							const float fade = water_physics::clamp01((hl.altitude - 0.75f * d) / (2.25f * d));
							const float low = 1.0f - fade * fade * (3.0f - 2.0f * fade);
							if (low <= 0.0f) {
								continue;
							}

							const float vh = std::sqrt(hl.mass * water_physics::G / (2.0f * RHO_AIR * 3.14159265f * hl.radius * hl.radius));
							const float reach = hl.radius * 7.0f;
							const int i0 = std::max(0, static_cast<int>(std::floor(hl.x - reach)) - ruffle_ox), i1 = std::min(RUFFLE_N - 1, static_cast<int>(std::floor(hl.x + reach)) - ruffle_ox);
							const int j0 = std::max(0, static_cast<int>(std::floor(hl.y - reach)) - ruffle_oy), j1 = std::min(RUFFLE_N - 1, static_cast<int>(std::floor(hl.y + reach)) - ruffle_oy);

							for (int j = j0; j <= j1; j++)
							{
								const float ry = static_cast<float>(ruffle_oy + j) + 0.5f - hl.y;
								for (int i = i0; i <= i1; i++)
								{
									const float rx = static_cast<float>(ruffle_ox + i) + 0.5f - hl.x;
									const float rr = std::sqrt(rx * rx + ry * ry) / hl.radius;

									// wind over the water there: the downwash under the disk, the wall jet beyond its edge
									const float jet = rr < 1.2f ? (0.7f + 0.3f * rr / 1.2f) : std::pow(1.2f / rr, 1.3f);
									const float wind = 2.0f * vh * jet * low;
									if (wind <= 3.0f) {
										continue;
									}

									// streaked outward: the jet is uneven around the disk and its streaks wander slowly
									const float th = std::atan2(ry, rx);
									const float streak = 0.62f + 0.38f * std::sin(13.0f * th + 2.1f * std::sin(4.0f * th + 0.5f * ruffle_time) + 0.9f * ruffle_time)
										* std::sin(5.0f * th - 0.6f * ruffle_time + 1.0f);
									// the patch has a sharp edge: either the wind raises ripples or it does not
									const float target = std::min(water_physics::clamp01((wind - 3.0f) / 0.7f) * streak * str, 1.0f);

									float& r = ruffle[static_cast<size_t>(j) * RUFFLE_N + i];
									// the outwash takes a few seconds to reach the edge of the patch: it spreads
									if (target > r) {
										r += (target - r) * std::min(wind / (2.5f + 0.35f * rr * hl.radius) * dt_w, 1.0f);
									}

									ruffle_peak = std::max(ruffle_peak, r);
								}
							}
						}
					}
				}

				// the wind sea is synthesised once per update
				if (wind_on && !ocean.calm) {
					ocean.advance(STEP, steps);
				}

				const int idx = pick_free_buffer();
				if (idx < 0) {
					continue;
				}

				auto& o = g_out[idx];
				o.frame.wake = o.frame.ripple = {};
				if (wake_active) {
					copy_grid(o.wake, wake, o.frame.wake);
				}

				if (ripple_active)
				{
					copy_grid(o.ripple, ripple, o.frame.ripple);
					smooth_121(o.ripple, ripple.n, false); // 0.5 m field on the 0.5 unit fine mesh
				}
				o.frame.ocean_on = wind_on && !ocean.calm;
				o.frame.ocean_hs = wind_on ? ocean.hs : 0.0f;

				if (o.frame.ocean_on)
				{
					o.o_long = ocean.long_h;
					o.o_mid = ocean.mid_h;
					o.o_short = ocean.short_h;
					o.o_dx = ocean.disp_x;
					o.o_dy = ocean.disp_y;

					// the long and mid band at the whole unit lattice points: every other node of the 0.5 unit tile,
					// slopes from the central differences (what a catmull-rom sample there gives)
					{
						constexpr int ON = water_physics::ocean_s::N, NN = water_dynamics::ocean_nodes_s::N, CH = water_dynamics::ocean_nodes_s::CHANNELS;
						constexpr float INV2C = 0.5f / (water_physics::ocean_s::L / ON);
						const auto bake = [&](const std::vector<float>& src, std::vector<float>& dst)
							{
								dst.resize(static_cast<size_t>(NN) * NN * CH);
								for (int j = 0; j < NN; j++)
								{
									const int sj = 2 * j, ju = (sj + 1) & (ON - 1), jd = (sj - 1) & (ON - 1);
									for (int i = 0; i < NN; i++)
									{
										const int si = 2 * i, ir = (si + 1) & (ON - 1), il = (si - 1) & (ON - 1);
										float* c = dst.data() + (static_cast<size_t>(j) * NN + i) * CH;
										c[0] = src[static_cast<size_t>(sj) * ON + si];
										c[1] = (src[static_cast<size_t>(sj) * ON + ir] - src[static_cast<size_t>(sj) * ON + il]) * INV2C;
										c[2] = (src[static_cast<size_t>(ju) * ON + si] - src[static_cast<size_t>(jd) * ON + si]) * INV2C;
									}
								}
							};

						bake(o.o_long, o.nodes_long);
						bake(o.o_mid, o.nodes_mid);
						o.frame.ocean_nodes = { o.nodes_long.data(), o.nodes_mid.data() };
					}

					o.frame.ocean_long = ocean_view(o.o_long);
					o.frame.ocean_mid = ocean_view(o.o_mid);
					o.frame.ocean_short = ocean_view(o.o_short);
					o.frame.ocean_disp_x = ocean_view(o.o_dx);
					o.frame.ocean_disp_y = ocean_view(o.o_dy);
				}
				else {
					o.frame.ocean_long = o.frame.ocean_mid = o.frame.ocean_short = o.frame.ocean_disp_x = o.frame.ocean_disp_y = {};
					o.frame.ocean_nodes = {};
				}

				// the wind sea near shores: shoaling (green's law, ~depth^-1/4), broken down where the water is shallower than
				// ~1.3 x the wave height. stored as 1 - factor (0 = untouched)
				if (!depth.empty() && (shore_dirty || std::abs(ocean.hs - shore_hs) > 0.03f))
				{
					shore_dirty = false;
					shore_hs = ocean.hs;
					shore.assign(depth.size(), 0.0f);
					const float db = std::max(1.3f * std::max(ocean.hs, 0.12f), 0.15f);

					for (size_t c = 0; c < depth.size(); c++)
					{
						// unknown or no water
						const float d = depth[c];
						if (!(d >= 0.0f)) {
							continue;
						}

						const float shoal = d < 8.0f ? std::min(std::pow(8.0f / std::max(d, 0.5f), 0.25f), 1.5f) : 1.0f;
						shore[c] = 1.0f - shoal * water_physics::clamp01(d / db);
					}

					// the surface sinks into the sand where the bottom is < 0.2 m down next to land (within 6 m): the waterline follows
					// the beach. a sandbar out in the water keeps the flat surface over it
					beach.assign(depth.size(), 0.0f);
					for (size_t c = 0; c < depth.size(); c++)
					{
						const float d = depth[c];
						if (d >= 0.0f && d < 0.2f && near_land(static_cast<int>(c % DEPTH_N), static_cast<int>(c / DEPTH_N)))
						{
							const float t = 1.0f - d / 0.2f;
							beach[c] = 0.17f * t * t * (3.0f - 2.0f * t);
						}
					}

					// water polygons end in straight lines across beaches: near such an edge (no water within 4 m)
					// shallow water (< 0.6 m) is lowered under the sand so the waterline follows the sand instead
					{
						constexpr int N = DEPTH_N, R = 4;
						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const size_t c = static_cast<size_t>(j) * N + i;
								const float d = depth[c];
								if (!(d >= 0.0f && d < 0.6f)) {
									continue;
								}

								float nearest = 99.0f;
								for (int dj = -R; dj <= R; dj++)
								{
									for (int di = -R; di <= R; di++)
									{
										const int ii = i + di, jj = j + dj;
										if (ii < 0 || jj < 0 || ii >= N || jj >= N) {
											continue;
										}

										if (depth[static_cast<size_t>(jj) * N + ii] == -1.0f) {
											nearest = std::min(nearest, std::sqrt(static_cast<float>(di * di + dj * dj)));
										}
									}
								}

								if (nearest <= static_cast<float>(R))
								{
									const float k = water_physics::clamp01(1.0f - (nearest - 1.0f) / static_cast<float>(R));
									beach[c] = std::max(beach[c], (d + 0.08f) * k * k * (3.0f - 2.0f * k));
								}
							}
						}

						// cells without water take their neighbours' sink: the mesh edge vertices sample them
						std::vector<float> edge = beach;
						for (int j = 1; j < N - 1; j++)
						{
							for (int i = 1; i < N - 1; i++)
							{
								const size_t c = static_cast<size_t>(j) * N + i;
								if (depth[c] != -1.0f) {
									continue;
								}

								float m = 0.0f;
								for (int dj = -1; dj <= 1; dj++)
								{
									for (int di = -1; di <= 1; di++) {
										m = std::max(m, beach[c + static_cast<size_t>(dj * N + di)]);
									}
								}

								edge[c] = m;
							}
						}

						beach.swap(edge);
					}

					// 1 m cells with hard steps between them read as bumps and pits on the 0.5 unit mesh: one 1-2-1 pass each way
					for (auto* f : { &shore, &beach })
					{
						constexpr int N = DEPTH_N;
						std::vector<float> tmp(f->size());
						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const size_t c = static_cast<size_t>(j) * N + i;
								const float l = (*f)[i > 0 ? c - 1 : c], r = (*f)[i < N - 1 ? c + 1 : c];
								tmp[c] = 0.25f * l + 0.5f * (*f)[c] + 0.25f * r;
							}
						}

						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const size_t c = static_cast<size_t>(j) * N + i;
								const float lo = tmp[j > 0 ? c - N : c], hi = tmp[j < N - 1 ? c + N : c];
								(*f)[c] = 0.25f * lo + 0.5f * tmp[c] + 0.25f * hi;
							}
						}
					}

					shore_t = shore;
					beach_t = beach;
				}

				// what is shown eases towards what the shore asks for (~0.6 s): new map data never makes the waterline jump
				if (!shore_t.empty())
				{
					constexpr int N = DEPTH_N;
					if (shore_s.size() != shore_t.size())
					{
						shore_s = shore_t;
						beach_s = beach_t;
						shown_ox = depth_ox;
						shown_oy = depth_oy;
					}
					else if (shown_ox != depth_ox || shown_oy != depth_oy)
					{
						// the map moved with the camera: what is shown moves along, new cells start where the shore wants them
						std::vector<float> ns = shore_t, nb = beach_t;
						for (int j = 0; j < N; j++)
						{
							for (int i = 0; i < N; i++)
							{
								const int si = i + depth_ox - shown_ox, sj = j + depth_oy - shown_oy;
								if (si >= 0 && sj >= 0 && si < N && sj < N)
								{
									ns[static_cast<size_t>(j) * N + i] = shore_s[static_cast<size_t>(sj) * N + si];
									nb[static_cast<size_t>(j) * N + i] = beach_s[static_cast<size_t>(sj) * N + si];
								}
							}
						}

						shore_s.swap(ns);
						beach_s.swap(nb);
						shown_ox = depth_ox;
						shown_oy = depth_oy;
					}

					const float ease = 1.0f - std::exp(-std::clamp(in.game_dt, 0.0f, 0.25f) / 0.6f);
					for (size_t c = 0; c < shore_s.size(); c++)
					{
						shore_s[c] += (shore_t[c] - shore_s[c]) * ease;
						beach_s[c] += (beach_t[c] - beach_s[c]) * ease;
					}

					shore = shore_s;
					beach = beach_s;
				}

				o.shore = shore;
				o.beach = beach;
				o.frame.shore = shore.empty() ? water_physics::grid_view_s {} : water_physics::grid_view_s { DEPTH_N, 1.0f, depth_ox, depth_oy, false, o.shore.data() };
				o.frame.beach = beach.empty() ? water_physics::grid_view_s {} : water_physics::grid_view_s { DEPTH_N, 1.0f, depth_ox, depth_oy, false, o.beach.data() };
				o.frame.wake_rms = wake.rms();
				o.frame.ripple_rms = ripple.rms();

				// the wake for the lattice: a 1-2-1 filter at half the lattice step (1, 2 and 4 cells)
				for (int l = 0; l < 3; l++)
				{
					if (!wake_active)
					{
						o.frame.wake_lattice[l] = {};
						continue;
					}

					o.wake_lattice[l] = o.wake;
					smooth_121(o.wake_lattice[l], wake.n, false, 1 << l);
					o.frame.wake_lattice[l] = { wake.n, wake.dx, wake.ox, wake.oy, false, wake.placed ? o.wake_lattice[l].data() : nullptr };
				}

				// the fine mesh's detail baked once per update at its own 0.5 unit cells, so a render only interpolates it:
				// wake + ripples + short wind waves (height, slope) and the choppy displacement
				{
					PROFILE_ZONE_N("WaterSim::bake_detail");
					constexpr float C = water_dynamics::detail_s::CELL;
					constexpr int CH = water_dynamics::detail_s::CHANNELS;
					const bool near_on = gs->water_near_tessellation_scale.get_as<float>() >= 0.375f && (gs->water_wind_waves.get_as<bool>() || gs->water_interactive.get_as<bool>());
					const float radius = near_on ? std::max(gs->water_detail_mesh_radius.get_as<float>(), 0.0f) : 0.0f;
					const float near_scale = std::max(gs->water_wind_wave_near_scale.get_as<float>(), 0.0f);
					const int n = radius >= 1.0f ? static_cast<int>((radius + 24.0f) * 2.0f / C) : 0;
					auto& det = o.frame.detail;
					det = {};

					if (n > 0)
					{
						const int bx = static_cast<int>(std::floor(in.cam_x / C)) - n / 2;
						const int by = static_cast<int>(std::floor(in.cam_y / C)) - n / 2;
						o.detail.assign(static_cast<size_t>(n) * n * CH, 0.0f);

						const auto& fr = o.frame;

						// the 1 m wake field on the bake's 0.5 unit cells: catmull-rom along x then y (at a node the node itself,
						// half way (-1 9 9 -1) / 16), its slopes are central differences like those of the other grids
						static thread_local std::vector<float> wake_rows, wake_fine;
						if (wake_active)
						{
							const int wn = wake.n;
							const int rows = n / 2 + 4;                      // wake rows the bake touches (+ the filter's reach)
							const int wy0 = static_cast<int>(std::floor(static_cast<float>(by) * 0.5f)) - 1 - wake.oy;
							wake_rows.assign(static_cast<size_t>(rows) * n, 0.0f);
							wake_fine.assign(static_cast<size_t>(n) * n, 0.0f);

							for (int r = 0; r < rows; r++)
							{
								const int wj = wy0 + r;
								if (wj < 0 || wj >= wn) {
									continue;
								}

								const float* src = o.wake.data() + static_cast<size_t>(wj) * wn;
								float* dst = wake_rows.data() + static_cast<size_t>(r) * n;
								for (int i = 0; i < n; i++)
								{
									// world x = (bx + i) / 2: wake cell and whether it is half way to the next
									const int fx2 = bx + i;
									const int wi = (fx2 >> 1) - wake.ox;
									if (wi < 1 || wi >= wn - 2) {
										continue;
									}

									dst[i] = (fx2 & 1) ? (9.0f * (src[wi] + src[wi + 1]) - src[wi - 1] - src[wi + 2]) * 0.0625f : src[wi];
								}
							}

							for (int j = 0; j < n; j++)
							{
								const int fy2 = by + j;
								const int r = (fy2 >> 1) - wake.oy - wy0;
								if (r < 1 || r >= rows - 2) {
									continue;
								}

								const float* r0 = wake_rows.data() + static_cast<size_t>(r - 1) * n;
								const float* r1 = r0 + n, * r2 = r1 + n, * r3 = r2 + n;
								float* dst = wake_fine.data() + static_cast<size_t>(j) * n;

								if (fy2 & 1)
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

						// tables of the wind ripples for this bake: cos / sin of kx x per column, of ky y + phase per row
						const bool ruffled = ruffle_peak > 0.01f;
						static thread_local std::vector<float> rcx, rsx, rcy, rsy;
						if (ruffled)
						{
							rcx.resize(static_cast<size_t>(RUFFLE_WAVES) * n); rsx.resize(rcx.size()); rcy.resize(rcx.size()); rsy.resize(rcx.size());
							for (int m = 0; m < RUFFLE_WAVES; m++)
							{
								const auto& rw = ruffle_waves[m];
								const float ph = rw.phase - rw.omega * ruffle_time;
								for (int q = 0; q < n; q++)
								{
									const float ax = rw.kx * static_cast<float>(bx + q) * C, ay = rw.ky * static_cast<float>(by + q) * C + ph;
									rcx[static_cast<size_t>(m) * n + q] = std::cos(ax); rsx[static_cast<size_t>(m) * n + q] = std::sin(ax);
									rcy[static_cast<size_t>(m) * n + q] = std::cos(ay); rsy[static_cast<size_t>(m) * n + q] = std::sin(ay);
								}
							}
						}

						const bool sea = fr.ocean_on;
						const bool rip = fr.ripple.valid() && ripple.dx == C;
						constexpr int ON = water_physics::ocean_s::N;
						const float inv2c = 0.5f / C;
						const auto node = [](const std::vector<float>& f, const int ii, const int jj) { return f[static_cast<size_t>(jj) * ON + ii]; };

						for (int j = 0; j < n; j++)
						{
							const int wy = by + j;
							const float y = static_cast<float>(wy) * C;

							for (int i = 0; i < n; i++)
							{
								const int wx = bx + i;
								const float x = static_cast<float>(wx) * C;
								float h = 0.0f, gx = 0.0f, gy = 0.0f, px = 0.0f, py = 0.0f, t, a, b;

								if (wake_active && i > 0 && j > 0 && i < n - 1 && j < n - 1)
								{
									const float* wf = wake_fine.data() + static_cast<size_t>(j) * n + i;
									h += wf[0];
									gx += (wf[1] - wf[-1]) * inv2c;
									gy += (wf[n] - wf[-n]) * inv2c;
								}

								// grid aligned: ripple and ocean nodes are read directly (a catmull-rom sample at a node is the node
								// value, its gradient the central difference)
								if (rip)
								{
									const int ri = wx - fr.ripple.ox, rj = wy - fr.ripple.oy, rn = fr.ripple.n;
									if (ri > 0 && rj > 0 && ri < rn - 1 && rj < rn - 1)
									{
										const float* r = fr.ripple.h + static_cast<size_t>(rj) * rn + ri;
										h += r[0];
										gx += (r[1] - r[-1]) * inv2c;
										gy += (r[rn] - r[-rn]) * inv2c;
									}
								}

								if (sea)
								{
									float sf = 1.0f;
									if (fr.shore.valid())
									{
										fr.shore.sample(x, y, t, a, b);
										sf = 1.0f - t;
									}

									const int oi = wx & (ON - 1), oj = wy & (ON - 1);
									const int il = (oi - 1) & (ON - 1), ir = (oi + 1) & (ON - 1);
									const int jd = (oj - 1) & (ON - 1), ju = (oj + 1) & (ON - 1);
									const float k = sf * near_scale;

									h += k * (node(o.o_mid, oi, oj) + node(o.o_short, oi, oj));
									gx += k * (node(o.o_mid, ir, oj) - node(o.o_mid, il, oj) + node(o.o_short, ir, oj) - node(o.o_short, il, oj)) * inv2c;
									gy += k * (node(o.o_mid, oi, ju) - node(o.o_mid, oi, jd) + node(o.o_short, oi, ju) - node(o.o_short, oi, jd)) * inv2c;

									const float kd = std::clamp(sf, 0.0f, 1.0f) * near_scale;
									px = kd * node(o.o_dx, oi, oj);
									py = kd * node(o.o_dy, oi, oj);
								}

								float* c = o.detail.data() + (static_cast<size_t>(j) * n + i) * CH;
								// rotor outwash: wind ripples where the water is ruffled (bilinear from the 1 m grid)
								if (ruffled)
								{
									const float fx = x - static_cast<float>(ruffle_ox) - 0.5f, fy = y - static_cast<float>(ruffle_oy) - 0.5f;
									const int ri = static_cast<int>(std::floor(fx)), rj = static_cast<int>(std::floor(fy));
									if (ri >= 0 && rj >= 0 && ri < RUFFLE_N - 1 && rj < RUFFLE_N - 1)
									{
										const float tx = fx - static_cast<float>(ri), ty = fy - static_cast<float>(rj);
										const float* r0 = ruffle.data() + static_cast<size_t>(rj) * RUFFLE_N + ri;
										const float rf = (r0[0] + (r0[1] - r0[0]) * tx) * (1.0f - ty) + (r0[RUFFLE_N] + (r0[RUFFLE_N + 1] - r0[RUFFLE_N]) * tx) * ty;

										if (rf > 0.02f)
										{
											float rh = 0.0f, rgx = 0.0f, rgy = 0.0f;
											for (int m = 0; m < RUFFLE_WAVES; m++)
											{
												const auto& rw = ruffle_waves[m];
												const float cx = rcx[static_cast<size_t>(m) * n + i], sx = rsx[static_cast<size_t>(m) * n + i];
												const float cy = rcy[static_cast<size_t>(m) * n + j], sy = rsy[static_cast<size_t>(m) * n + j];
												const float sn = sx * cy + cx * sy, cs = cx * cy - sx * sy;
												rh += rw.amp * sn;
												rgx += rw.amp * rw.kx * cs;
												rgy += rw.amp * rw.ky * cs;
											}

											h += rf * rh; gx += rf * rgx; gy += rf * rgy;
										}
									}
								}

								c[0] = h; c[1] = gx; c[2] = gy; c[3] = px; c[4] = py;
							}
						}

						det.n = n;
						det.ox = bx;
						det.oy = by;
						det.d = o.detail.data();
					}
				}

				// our heights on the game's 2 unit grid around its base, for buoyancy
				if (g_cpl_hooked.load())
				{
					const int bx = *game::pWaterDynamicBaseX, by = *game::pWaterDynamicBaseY;
					const int w = g_cpl_ready.load() == 0 ? 1 : 0;
					auto& grid = g_cpl[w];

					const water_physics::grid_view_s wv = { wake.n, wake.dx, wake.ox, wake.oy, false, wake.h.data() };
					const water_physics::grid_view_s rv = { ripple.n, ripple.dx, ripple.ox, ripple.oy, false, ripple.h.data() };
					const water_physics::grid_view_s sv = { DEPTH_N, 1.0f, depth_ox, depth_oy, false, shore.data() };
					const auto lv = ocean_view(ocean.long_h);
					const auto mv = ocean_view(ocean.mid_h);
					const bool sea = wind_on && !ocean.calm;

					// mean height of the wake and ripple fields along both sides of each hull
					static thread_local std::vector<float> hull_mean;
					hull_mean.assign(in.hulls.size(), 0.0f);
					for (size_t hi = 0; hi < in.hulls.size(); hi++)
					{
						const auto& hl = in.hulls[hi];
						const float reach = hl.beam * 0.5f + 1.5f;
						float sum = 0.0f, t, a, b;
						constexpr int STATIONS = 5;

						for (int st = 0; st < STATIONS; st++)
						{
							const float along = hl.length * (static_cast<float>(st) / (STATIONS - 1) - 0.5f);
							const float cx = hl.x + hl.dirx * along, cy = hl.y + hl.diry * along;
							for (const float sgn : { -1.0f, 1.0f })
							{
								const float px = cx - hl.diry * reach * sgn, py = cy + hl.dirx * reach * sgn;
								wv.sample(px, py, t, a, b); sum += t;
								rv.sample(px, py, t, a, b); sum += t;
							}
						}

						hull_mean[hi] = sum / (2.0f * STATIONS);
					}

					for (int kx = 0; kx < GAME_GRID_CELLS; kx++)
					{
						const int X = bx + 2 * kx;
						const int ix = ((X / 2) + 100000) % GAME_GRID_CELLS;

						for (int ky = 0; ky < GAME_GRID_CELLS; ky++)
						{
							const int Y = by + 2 * ky;
							const int iy = ((Y / 2) + 100000) % GAME_GRID_CELLS;
							const float fx = static_cast<float>(X), fy = static_cast<float>(Y);
							float h = 0.0f, t, a, b;

							// under something that floats the surface holds the dent it presses itself: that is its own volume, it
							// would sink into it. what tilts it are the waves coming in from around it: taken from both sides
							// just outside of it and carried across its beam, so a wake rolling in rolls and pitches it
							const hull_s* under = nullptr;
							float along = 0.0f, across = 0.0f;
							for (const auto& hl : in.hulls)
							{
								const float rx = fx - hl.x, ry = fy - hl.y;
								along = rx * hl.dirx + ry * hl.diry;
								across = -rx * hl.diry + ry * hl.dirx;
								if (std::abs(along) < hl.length * 0.5f + 1.0f && std::abs(across) < hl.beam * 0.5f + 1.0f)
								{
									under = &hl;
									break;
								}
							}

							if (under)
							{
								const float reach = under->beam * 0.5f + 1.5f;
								const float cx = under->x + under->dirx * along, cy = under->y + under->diry * along;
								const float lx = cx + under->diry * reach, ly = cy - under->dirx * reach; // across = -reach
								const float rx = cx - under->diry * reach, ry = cy + under->dirx * reach; // across = +reach
								const float side = water_physics::clamp01((across + reach) / (2.0f * reach));

								float l = 0.0f, r = 0.0f;
								wv.sample(lx, ly, t, a, b); l += t;
								rv.sample(lx, ly, t, a, b); l += t;
								wv.sample(rx, ry, t, a, b); r += t;
								rv.sample(rx, ry, t, a, b); r += t;

								// without the mean along its length: the rings it sends out itself when it bobs would lift and
								// drop it again (a loop that feeds itself through the delay of the coupling)
								// and at 0.7: the coupling runs a frame or two behind, what the hull sends out must not come back whole
								h += 0.7f * (l + (r - l) * side - hull_mean[static_cast<size_t>(under - in.hulls.data())]);
							}
							else
							{
								wv.sample(fx, fy, t, a, b);
								h += t;
								rv.sample(fx, fy, t, a, b);
								h += t;
							}

							if (sea)
							{
								float sf = 1.0f;
								if (!shore.empty())
								{
									sv.sample(fx, fy, t, a, b);
									sf = 1.0f - t;
								}

								lv.sample(fx, fy, t, a, b);
								h += t * sf;
								mv.sample(fx, fy, t, a, b);
								h += t * sf;
							}

							grid[static_cast<size_t>(ix) * GAME_GRID_CELLS + iy] = std::isfinite(h) ? h : 0.0f;
						}
					}

					g_cpl_ready = w;
				}

				publish(idx);

				// wall time of this update, smoothed
				const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
				g_sim_ms = g_sim_ms.load() * 0.9f + ms * 0.1f;
			}
		}

		bool enabled()
		{
			static const auto gs = comp_settings::get();
			return gs->water_interactive.get_as<bool>();
		}

		float strength()
		{
			static const auto gs = comp_settings::get();
			return std::clamp(gs->water_interactive_strength.get_as<float>(), 0.0f, 4.0f);
		}

		DETOUR_TYPEDEF(CExplosion_AddExplosion, int, __cdecl, int a1, int a2, int type, float radius, float* pos, int a6, int a7, int a8, int a9,
			int a10, int a11, int a12, int a13, int a14, int a15, int a16, int a17, int a18, int a19);

		int __cdecl CExplosion_AddExplosion_hk(int a1, int a2, int type, float radius, float* pos, int a6, int a7, int a8, int a9,
			int a10, int a11, int a12, int a13, int a14, int a15, int a16, int a17, int a18, int a19)
		{
			if (pos && std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]))
			{
				std::lock_guard lock(g_explosion_mutex);
				if (g_explosions.size() < 64u) {
					g_explosions.push_back({ pos[0], pos[1], pos[2], radius, type });
				}
			}

			return CExplosion_AddExplosion_og(a1, a2, type, radius, pos, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19);
		}

		// the game asks where a bullet's path (muzzle to what it hit) meets the water, for its splash effect and sound. the
		// same test places the heli downwash effect, which is no bullet
		DETOUR_TYPEDEF(CWater_TestLineAgainstWater, bool, __cdecl, const float* start, const float* end, float* hit);

		bool __cdecl CWater_TestLineAgainstWater_hk(const float* start, const float* end, float* hit)
		{
			const bool met = CWater_TestLineAgainstWater_og(start, end, hit);

			if (met && hit && reinterpret_cast<std::uint32_t>(_ReturnAddress()) != game::retn_addr__heli_downwash_water_test
				&& std::isfinite(hit[0]) && std::isfinite(hit[1]))
			{
				std::lock_guard lock(g_shot_mutex);
				if (g_shots.size() < 32u) {
					g_shots.push_back({ hit[0], hit[1] });
				}
			}

			return met;
		}

		/// blast radius by explosion type when the caller passes none
		float default_radius(const int type)
		{
			switch (type)
			{
			case 0: return 5.0f;  // grenade
			case 1: return 2.0f;  // molotov
			case 2: return 6.0f;  // rocket
			case 4: case 5: case 12: return 8.0f; // car, plane, boat
			default: return 4.0f;
			}
		}

		struct pool_s
		{
			std::uint8_t* objects;
			std::int8_t* flags;
			int count;
			int item_size;
		};

		/// a rigid entity followed from frame to frame
		struct track_s
		{
			float pose[16] = {};  // matrix of the last frame (right, forward, up, position rows)
			bool has_pose = false;
			bool model_known = false;
			bool boat = false, heli = false, plane = false;
			float min[3] = { -1.0f, -2.25f, -0.6f }, max[3] = { 1.0f, 2.25f, 1.2f }; // model box
			float mass = 0.0f;    // kg, 0 = unknown
			float vz = 0.0f;      // vertical speed of the last frame
			float lift = 1.0f;    // helicopters: share of the weight the rotor carries (smoothed)
			bool trim_valid = false; // floating hulls: draft and trim the dent follows (eased)
			float still = 0.0f;      // s without moving
			float trim_z = 0.0f, trim_pitch = 0.0f, trim_roll = 0.0f;
			bool wet = false;     // touched the water last frame
			float time = 0.0f;    // g_time of the last pose
			float water = 0.0f;   // level of the calm surface there, looked up every ~0.5 s
			bool has_water = false;
			std::uint32_t water_frame = 0u;
			std::uint32_t seen = 0u;
		};

		// keyed by pool (vehicle 0, ped 1, object 2) << 28 | script handle
		std::unordered_map<std::uint32_t, track_s> g_tracks;
		std::uint32_t g_frame = 0u;
		float g_time = 0.0f;                  // game time of this module (s)
		float g_water_level = 0.0f;           // level of the water last found under something
		std::vector<std::uint8_t> g_hot[3];   // per pool slot: in or near the water, looked at every frame
		bool g_pool_checked[3] = {};
		bool g_pool_ok[3] = {};

		const pool_s* entity_pool(void** pool_ptr)
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

		/// matrix of the entity at slot i (right, forward, up, position rows of 16 bytes), null for one without
		const float* entity_matrix(const pool_s* pool, const int i)
		{
			const auto ent = pool->objects + static_cast<size_t>(i) * pool->item_size;
			return *reinterpret_cast<float* const*>(ent + 0x20);
		}

		/// model box and mass of a vehicle (pool 0), ped (1) or object (2)
		void learn_model(natives* n, const int pool, const int handle, track_s& t)
		{
			t.model_known = true;

			if (pool == 1)
			{
				// a standing body: the ped's origin is at the hips
				t.min[0] = -0.25f; t.min[1] = -0.18f; t.min[2] = -1.0f;
				t.max[0] = 0.25f; t.max[1] = 0.18f; t.max[2] = 0.8f;
				t.mass = 80.0f;
				return;
			}

			std::uint32_t model = 0u;
			if (pool == 0) {
				n->GetCarModel(handle, &model);
			} else {
				n->GetObjectModel(handle, &model);
			}

			if (!model) {
				return;
			}

			if (pool == 0)
			{
				t.boat = n->IsThisModelABoat(model);
				t.heli = n->IsThisModelAHeli(model);
				t.plane = n->IsThisModelAPlane(model);
			}

			Vector mn = {}, mx = {};
			n->GetModelDimensions(model, &mn, &mx);
			if (mx.x - mn.x > 0.05f && mx.y - mn.y > 0.05f && mx.z - mn.z > 0.05f && mx.x - mn.x < 80.0f && mx.y - mn.y < 80.0f && mx.z - mn.z < 80.0f)
			{
				t.min[0] = mn.x; t.min[1] = mn.y; t.min[2] = mn.z;
				t.max[0] = mx.x; t.max[1] = mx.y; t.max[2] = mx.z;
			}

			float mass = 0.0f;
			if (pool == 0) {
				n->GetCarMass(handle, &mass);
			} else {
				n->GetObjectMass(handle, &mass);
			}

			t.mass = std::isfinite(mass) && mass > 0.5f && mass < 500000.0f ? mass : 0.0f;
		}

		/// the box of an entity from its matrix (right, forward, up, position rows of 16 bytes) and its model box
		void pose_box(const float* m, const track_s& t, float axis[3][3], float centre[3], float half[3])
		{
			for (int a = 0; a < 3; a++)
			{
				const float* r = m + a * 4;
				const float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
				const float inv = len > 1e-4f ? 1.0f / len : 0.0f;
				axis[a][0] = r[0] * inv; axis[a][1] = r[1] * inv; axis[a][2] = r[2] * inv;
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

		/// One rigid entity: what its box does to the water this frame, from its pose now and a frame ago.
		/// Returns true when it touches the water.
		bool rigid_entity(natives* n, const int pool, const int handle, const float* m, const float k,
			std::vector<hull_s>& hulls, std::vector<body_s>& bodies, std::vector<heli_s>& helis, std::vector<splash_s>& splashes)
		{
			auto& t = g_tracks[(static_cast<std::uint32_t>(pool) << 28) | static_cast<std::uint32_t>(handle)];
			if (!t.model_known) {
				learn_model(n, pool, handle, t);
			}

			t.seen = g_frame;

			// time since its last pose (dry entities are not looked at every frame)
			const float dt = g_time - t.time;
			t.time = g_time;

			water_physics::rigid_body_s b;
			pose_box(m, t, b.axis, b.centre, b.half);
			if (t.has_pose && dt > 0.0f && dt < 0.25f)
			{
				float unused[3];
				pose_box(t.pose, t, b.prev_axis, b.prev_centre, unused);
				b.has_prev = true;
				b.dt = dt;
			}

			// a floating hull presses the dent of its mean draft and trim (eased over ~1.5 s), not every bob and pitch: the rings
			// it sends out pitching would lift and tip it again through the delay of the coupling, a loop that never runs down
			// (a real hull loses that energy into the waves it sends away). heading and position stay live, so wakes and turns
			// are untouched; a fast drop or a capsize takes the real pose
			if (t.boat && b.axis[2][2] > 0.5f)
			{
				const float rawvz = b.has_prev ? (b.centre[2] - b.prev_centre[2]) / dt : 0.0f;
				const float pitch = std::asin(std::clamp(b.axis[1][2], -1.0f, 1.0f));
				const float roll = std::asin(std::clamp(b.axis[0][2], -1.0f, 1.0f));

				// the dent's trim along a heading: forward pitched up by p, right rolled by r, orthonormal again
				const auto trim_axes = [](const float fwd_raw[3], const float p, const float r, float ax[3][3])
					{
						const float hl = std::sqrt(fwd_raw[0] * fwd_raw[0] + fwd_raw[1] * fwd_raw[1]);
						const float fx = hl > 1e-4f ? fwd_raw[0] / hl : 0.0f, fy = hl > 1e-4f ? fwd_raw[1] / hl : 1.0f;
						float f[3] = { fx * std::cos(p), fy * std::cos(p), std::sin(p) };
						float rt[3] = { fy * std::cos(r), -fx * std::cos(r), std::sin(r) };
						const float d = rt[0] * f[0] + rt[1] * f[1] + rt[2] * f[2];
						for (int k = 0; k < 3; k++) { rt[k] -= d * f[k]; }
						const float rl = std::sqrt(rt[0] * rt[0] + rt[1] * rt[1] + rt[2] * rt[2]);
						for (int k = 0; k < 3; k++) { rt[k] /= std::max(rl, 1e-4f); }
						const float u[3] = { rt[1] * f[2] - rt[2] * f[1], rt[2] * f[0] - rt[0] * f[2], rt[0] * f[1] - rt[1] * f[0] };
						for (int k = 0; k < 3; k++) { ax[0][k] = rt[k]; ax[1][k] = f[k]; ax[2][k] = u[k]; }
					};

				if (!t.trim_valid || !b.has_prev || std::abs(rawvz) > 1.5f)
				{
					t.trim_z = b.centre[2];
					t.trim_pitch = pitch;
					t.trim_roll = roll;
					t.trim_valid = true;
				}
				else
				{
					const float prev_z = t.trim_z, prev_pitch = t.trim_pitch, prev_roll = t.trim_roll;
					const float e = 1.0f - std::exp(-dt / 1.5f);
					t.trim_z += (b.centre[2] - t.trim_z) * e;
					t.trim_pitch += (pitch - t.trim_pitch) * e;
					t.trim_roll += (roll - t.trim_roll) * e;

					// the pose a frame ago on its own heading with the eased trim of then: the dent's vertical motion is the eased one
					const float pf[3] = { b.prev_axis[1][0], b.prev_axis[1][1], b.prev_axis[1][2] };
					trim_axes(pf, prev_pitch, prev_roll, b.prev_axis);
					b.prev_centre[2] = prev_z;
				}

				const float cf[3] = { b.axis[1][0], b.axis[1][1], b.axis[1][2] };
				trim_axes(cf, t.trim_pitch, t.trim_roll, b.axis);
				b.centre[2] = t.trim_z;
			}
			else {
				t.trim_valid = false;
			}

			const float vz = b.has_prev ? (b.centre[2] - b.prev_centre[2]) / dt : 0.0f;
			const float az = b.has_prev && std::abs(vz) < 60.0f ? (vz - t.vz) / dt : 0.0f;
			t.vz = std::abs(vz) < 60.0f ? vz : 0.0f;
			std::memcpy(t.pose, m, sizeof(t.pose));
			t.has_pose = true;
			t.wet = false;

			// the water level changes only from one body of water to the next
			if (!t.has_water || g_frame - t.water_frame > 30u)
			{
				t.water_frame = g_frame;
				t.has_water = n->GetWaterHeightNoWaves(m[12], m[13], m[14], &t.water);
			}

			if (!t.has_water)
			{
				return false;
			}

			const float wh = t.water;
			g_water_level = wh;

			// rotor downwash: the jet leaves along the rotor's axis and lands where that meets the water. the rotor carries
			// the helicopter's weight plus what its climb or sink asks for
			if (t.heli && b.axis[2][2] > 0.3f)
			{
				const float hub[3] = { b.centre[0] + b.axis[2][0] * b.half[2], b.centre[1] + b.axis[2][1] * b.half[2], b.centre[2] + b.axis[2][2] * b.half[2] };
				const float reach = (hub[2] - wh) / b.axis[2][2];
				const float altitude = reach - 2.0f * b.half[2];

				int pilot = 0;
				if (altitude < 45.0f && altitude > -1.0f) {
					n->GetDriverOfCar(handle, &pilot);
				}

				// only a manned helicopter has its rotor turning
				if (pilot)
				{
					const float lift = std::clamp((water_physics::G + az) / (water_physics::G * b.axis[2][2]), 0.6f, 1.6f);
					t.lift += (lift - t.lift) * std::min(dt * 2.0f, 1.0f);

					const float radius = std::clamp(0.8f * std::max(b.half[0], b.half[1]), 3.0f, 9.0f); // the box is longer than the rotor (tail boom)
					const float jet_x = hub[0] - b.axis[2][0] * reach, jet_y = hub[1] - b.axis[2][1] * reach;
					const float carried = (t.mass > 0.0f ? t.mass : 2500.0f) * t.lift;
					helis.push_back({ jet_x, jet_y, altitude, radius, carried });

					// the rotor wash also dents the water: induced velocity vh = sqrt(m g / (2 rho_air A)) (momentum theory), the jet
					// arrives with up to twice that (the far wake), so the surface holds p = rho_air (2 vh)^2 / 2 down by p / (rho g):
					// 2 - 4 cm under a light helicopter in ground effect, fading out above about two rotor diameters
					if (altitude < 4.0f * radius)
					{
						constexpr float RHO_AIR = 1.225f;
						const float vh = std::sqrt(carried * water_physics::G / (2.0f * RHO_AIR * 3.14159265f * radius * radius));
						const float p = 0.5f * RHO_AIR * 4.0f * vh * vh;
						const float fade = 1.0f / (1.0f + (std::max(altitude, 0.0f) / radius) * (std::max(altitude, 0.0f) / radius));
						bodies.push_back({ jet_x, jet_y, 0.7f * radius, std::min(p / (water_physics::RHO * water_physics::G), 0.08f) * fade * k, true });
					}
				}
			}

			// far above or below the surface
			const float reach_z = std::abs(b.axis[0][2]) * b.half[0] + std::abs(b.axis[1][2]) * b.half[1] + std::abs(b.axis[2][2]) * b.half[2];
			if (b.centre[2] - reach_z > wh + 0.5f || b.centre[2] + reach_z < wh - 3.0f) {
				return false;
			}

			// a ped in a vehicle is part of the vehicle
			if (pool == 1 && n->IsCharInAnyCar(handle)) {
				return false;
			}

			b.water = wh;
			b.mass = t.mass;
			b.hull = t.boat;
			b.stagnation = t.boat ? 0.4f : 0.5f;
			b.planing = 0.75f;

			// a helicopter's model box is its rotor disc and tail boom: what sits in the water is the cabin on its skids, about a
			// fifth of the length and a tenth of it wide, a rounded footprint a little ahead of the mast
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
			float mid = b.half[0] + b.half[1] + b.half[2] - std::max({ b.half[0], b.half[1], b.half[2] }) - std::min({ b.half[0], b.half[1], b.half[2] });
			const bool on_wake = 2.0f * mid >= 1.5f;

			static std::vector<water_physics::press_point_s> press;
			static std::vector<water_physics::strike_point_s> strikes;
			press.clear();
			strikes.clear();

			// 1.4 on what its motion adds: the wave fields do not resolve the sharp pressure edges of a hull, which make
			// most of its wake
			const float wetted = water_physics::rigid_body_sources(b, on_wake ? 1.0f : 0.5f, 1.4f * k, press, strikes);

			// the dent is what holds a body up: only where the water carries it. something fixed or resting on the bottom
			// (props, piles and rocks standing in the water, a helicopter or a car sitting on the ground) has the flat
			// surface around it once it has been still for 2 s, boats always float. and no dent holds more than the body
			// weighs (1.5 x for what its motion adds)
			if (b.has_prev)
			{
				const float mx = b.centre[0] - b.prev_centre[0], my = b.centre[1] - b.prev_centre[1], mz = b.centre[2] - b.prev_centre[2];
				t.still = mx * mx + my * my + mz * mz > (0.15f * b.dt) * (0.15f * b.dt) ? 0.0f : t.still + b.dt;
			}

			float carried = 1.0f;
			if (pool != 1 && !t.boat && t.still > 2.0f) {
				carried = 0.0f;
			}
			else if (t.mass > 0.0f)
			{
				float volume = 0.0f;
				for (const auto& p : press) {
					volume += p.depth * 3.14159265f * p.radius * p.radius;
				}

				const float limit = 1.5f * t.mass / water_physics::RHO;
				if (volume > limit) {
					carried = limit / volume;
				}
			}

			if (carried > 0.0f)
			{
				for (const auto& p : press) {
					bodies.push_back({ p.x, p.y, p.radius, p.depth * carried, on_wake });
				}
			}

			for (const auto& s : strikes)
			{
				splashes.push_back({ s.x, s.y, on_wake ? std::max(s.radius, 1.0f) : std::min(s.radius, 0.95f), s.speed });

				// something big hitting the water also sends out the short rings the 1 m field cannot carry
				if (on_wake) {
					splashes.push_back({ s.x, s.y, 0.7f, s.speed * 0.6f });
				}
			}

			t.wet = wetted > 0.0f || !strikes.empty();

			// what floats: the game's buoyancy does not get the dent under it
			if (pool != 1 && wetted > 0.0f)
			{
				const float fl = std::sqrt(b.axis[1][0] * b.axis[1][0] + b.axis[1][1] * b.axis[1][1]);
				if (fl > 0.1f) {
					hulls.push_back({ b.centre[0], b.centre[1], b.axis[1][0] / fl, b.axis[1][1] / fl, 2.0f * b.half[1], 2.0f * b.half[0] });
				}
			}

			return t.wet;
		}

		/// shore depth map, 96 x 96 one metre cells around the camera from the water level and the collision below it.
		/// -1 = no water, 0 = dry (land or a deck at the water line), else metres (30 = no bottom found)
		struct depth_grid_s
		{
			static constexpr int N = 96;
			int ox = 0, oy = 0;
			bool placed = false;
			std::vector<float> d = std::vector<float>(static_cast<size_t>(N) * N, NAN);
			std::vector<std::uint8_t> recheck = std::vector<std::uint8_t>(static_cast<size_t>(N) * N, 0u); // "no water" asked again
			float level = 0.0f;   // calm water level last found in this map
			bool has_level = false;
			int cursor = 0;
			int changed = 0;
		};

		depth_grid_s g_depth;

		void update_depth(natives* n, const float cx, const float cy)
		{
			auto& g = g_depth;
			constexpr int N = depth_grid_s::N;
			const int nox = static_cast<int>(std::floor(cx)) - N / 2, noy = static_cast<int>(std::floor(cy)) - N / 2;

			// right after the camera jumps (teleport, cut scene) the game's water lookup still fails around the new place for
			// a moment: no scanning for a second, or whole bands of open water end up as "no water"
			static int settle = 0;
			if (g.placed && (std::abs(nox - g.ox) > 40 || std::abs(noy - g.oy) > 40)) {
				settle = 60;
			}

			if (!g.placed || std::abs(nox - g.ox) > 8 || std::abs(noy - g.oy) > 8)
			{
				std::vector<float> nd(static_cast<size_t>(N) * N, NAN);
				if (g.placed)
				{
					for (int j = 0; j < N; j++)
					{
						for (int i = 0; i < N; i++)
						{
							const int si = i + nox - g.ox, sj = j + noy - g.oy;
							if (si >= 0 && sj >= 0 && si < N && sj < N) {
								nd[static_cast<size_t>(j) * N + i] = g.d[static_cast<size_t>(sj) * N + si];
							}
						}
					}
				}

				g.d.swap(nd);
				std::fill(g.recheck.begin(), g.recheck.end(), std::uint8_t(0));
				if (!g.placed || std::abs(nox - g.ox) > 40 || std::abs(noy - g.oy) > 40) {
					g.has_level = false;
				}
				g.ox = nox;
				g.oy = noy;
				g.placed = true;
				g.changed++;
			}

			if (settle > 0)
			{
				settle--;
				return;
			}

			// "no water" is asked again every ~10 s (cheap: one lookup that fails), a wrong answer does not stay. the cell keeps its
			// answer until the new one is in: wiping it would drop the waterline onto the beach until the map is scanned again
			static std::uint32_t tick = 0u;
			if (++tick % 600u == 0u)
			{
				for (size_t c = 0; c < g.d.size(); c++) {
					g.recheck[c] = g.d[c] == -1.0f ? 1u : 0u;
				}
			}

			// cells per game frame (up to 3 natives each, two of them ray casts): the map fills in ~10 s, a moving camera adds a strip of ~800 cells every 8 m
			int budget = 16;
			for (int scanned = 0; scanned < N * N && budget > 0; scanned++)
			{
				const int c = g.cursor;
				g.cursor = (g.cursor + 1) % (N * N);
				if (!std::isnan(g.d[c]) && !g.recheck[c]) {
					continue;
				}

				budget--;
				const float before = g.d[c];
				g.recheck[c] = 0u;

				const float x = static_cast<float>(g.ox + c % N) + 0.5f, y = static_cast<float>(g.oy + c / N) + 0.5f;
				float wh = 0.0f, top = -1000.0f, gz = -1000.0f;

				// under bridges and roofs the lookup from high above finds no water: asked again from just above the
				// level of the water around
				bool water = n->GetWaterHeightNoWaves(x, y, 100.0f, &wh);
				if (!water) {
					water = n->GetWaterHeightNoWaves(x, y, g_water_level + 1.0f, &wh);
				}

				if (water)
				{
					g.level = wh;
					g.has_level = true;
				}

				// the game's water polygons leave gaps and seams in open water: no water there, but the ground is below the level
				// of the water around. counted as water (else the shallows around it sink under the "sand" into a pit)
				if (!water && g.has_level && n->GetGroundZFor3dCoord(x, y, g.level + 60.0f, &top) && top < g.level - 0.05f)
				{
					g.d[c] = std::min(g.level - top, 30.0f);
				}
				else if (!water) {
					g.d[c] = -1.0f;
				}
				else if (n->GetGroundZFor3dCoord(x, y, wh + 60.0f, &top) && top > wh - 0.05f && top < wh + 4.0f) {
					g.d[c] = 0.0f; // land at the water line: beach, bank, low pier
				}
				else if (n->GetGroundZFor3dCoord(x, y, wh - 0.05f, &gz) && gz < wh) {
					g.d[c] = std::min(wh - gz, 30.0f);
				}
				else {
					g.d[c] = 30.0f;
				}

				if (!(g.d[c] == before)) {
					g.changed++;
				}
			}
		}

		struct player_s
		{
			bool was_in_water = false;
			float vz = 0.0f;
			bool has_pos = false;
			float last_x = 0.0f, last_y = 0.0f, move_speed = 0.0f;
			std::int64_t entry_ms = 0;
		};

		player_s g_player;

		std::int64_t now_ms() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		// the player's body in the water: capsules sampled ~25 cm apart, posed from his position, heading, pace and state
		// (front crawl, treading water, wading). every point near the surface pushes on it with
		//  - the dynamic pressure of its motion: p / (rho g) = Cp v^2 / 2g, by its submerged share, fading with depth
		//  - the volume it carries through the surface: + plunging down, - lifting out
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

		struct body_track_s
		{
			float px[MAX_POINTS] = {}, py[MAX_POINTS] = {}, pz[MAX_POINTS] = {};
			float vx[MAX_POINTS] = {}, vy[MAX_POINTS] = {}, vz[MAX_POINTS] = {};
			int count = 0;
			bool valid = false;
		};

		body_track_s g_body_track;

		enum class body_mode_e
		{
			NONE,
			CRAWL,
			TREADING,
			WADING,
		};

		struct body_pose_s
		{
			float t = 0.0f;                  // running time (s)
			float px = 0.0f, py = 0.0f, pz = 0.0f, wh = 0.0f;
			float fx = 0.0f, fy = 1.0f;      // heading
			float speed = 0.0f;
			body_mode_e mode = body_mode_e::NONE;
		};

		body_pose_s g_pose;

		Vector body_point(const body_point_e point)
		{
			const auto& k = g_pose;
			const float rx = k.fy, ry = -k.fx; // right
			const float two_pi = 6.2831853f;
			const auto at = [&](const float f, const float r, const float z) { return Vector(k.px + k.fx * f + rx * r, k.py + k.fy * f + ry * r, z); };

			// both sides half a cycle apart
			const bool left = point == L_THIGH || point == L_CALF || point == L_FOOT || point == L_TOE || point == L_UPPERARM || point == L_FOREARM || point == L_HAND;
			const float side = left ? -1.0f : 1.0f;
			const float sphase = left ? 0.0f : 0.5f;

			// horizontal just under the surface, alternating arm strokes, flutter kick
			if (k.mode == body_mode_e::CRAWL)
			{
				const float T = std::clamp(1.6f - 0.3f * k.speed, 0.9f, 1.6f);
				const float th = two_pi * (k.t / T + sphase);
				const float kick = 0.15f * std::sin(two_pi * (k.t * 2.4f + sphase));

				switch (point)
				{
				case HEAD: return at(0.75f, 0.0f, k.wh + 0.06f);
				case NECK: return at(0.55f, 0.0f, k.wh + 0.0f);
				case PELVIS: return at(-0.15f, 0.0f, k.wh - 0.08f);
				case L_THIGH: case R_THIGH: return at(-0.2f, side * 0.1f, k.wh - 0.1f);
				case L_CALF: case R_CALF: return at(-0.6f, side * 0.1f, k.wh - 0.18f + kick * 0.5f);
				case L_FOOT: case R_FOOT: return at(-1.0f, side * 0.1f, k.wh - 0.15f + kick);
				case L_TOE: case R_TOE: return at(-1.15f, side * 0.1f, k.wh - 0.12f + kick);
				case L_UPPERARM: case R_UPPERARM: return at(0.45f, side * 0.22f, k.wh);
				case L_FOREARM: case R_FOREARM: return at(0.45f + 0.33f * std::cos(th), side * 0.26f, k.wh + 0.28f * std::sin(th) + 0.1f);
				case L_HAND: case R_HAND: return at(0.45f + 0.65f * std::cos(th), side * 0.28f, k.wh + 0.55f * std::sin(th));
				}
			}

			// upright, head out, sculling hands, eggbeater legs
			if (k.mode == body_mode_e::TREADING)
			{
				const float bob = 0.04f * std::sin(two_pi * 0.7f * k.t);
				const float scull = std::sin(two_pi * (0.8f * k.t + sphase));
				const float egg = std::sin(two_pi * (1.1f * k.t + sphase));

				switch (point)
				{
				case HEAD: return at(0.0f, 0.0f, k.wh + 0.2f + bob);
				case NECK: return at(0.0f, 0.0f, k.wh + 0.02f + bob);
				case PELVIS: return at(0.0f, 0.0f, k.wh - 0.7f + bob);
				case L_THIGH: case R_THIGH: return at(0.0f, side * 0.1f, k.wh - 0.75f + bob);
				case L_CALF: case R_CALF: return at(0.25f * egg, side * 0.3f, k.wh - 1.0f + 0.1f * egg);
				case L_FOOT: case R_FOOT: return at(0.1f * egg, side * 0.25f, k.wh - 1.35f);
				case L_TOE: case R_TOE: return at(0.25f + 0.1f * egg, side * 0.25f, k.wh - 1.35f);
				case L_UPPERARM: case R_UPPERARM: return at(0.0f, side * 0.22f, k.wh - 0.05f + bob);
				case L_FOREARM: case R_FOREARM: return at(0.1f + 0.1f * scull, side * 0.38f, k.wh - 0.15f);
				case L_HAND: case R_HAND: return at(0.2f + 0.22f * scull, side * 0.5f, k.wh - 0.14f + 0.04f * std::cos(two_pi * (0.8f * k.t + sphase)));
				}
			}

			// standing, legs swing at the walking cadence
			if (k.mode == body_mode_e::WADING)
			{
				const float cad = std::max(k.speed * 0.9f, 0.6f);
				const float sw = std::sin(two_pi * (cad * 0.5f * k.t + sphase));
				const float lift = 0.12f * std::max(0.0f, std::cos(two_pi * (cad * 0.5f * k.t + sphase)));
				const float feet = k.pz - 1.0f;

				switch (point)
				{
				case HEAD: return at(0.0f, 0.0f, k.pz + 0.75f);
				case NECK: return at(0.0f, 0.0f, k.pz + 0.55f);
				case PELVIS: return at(0.0f, 0.0f, k.pz);
				case L_THIGH: case R_THIGH: return at(0.0f, side * 0.1f, k.pz - 0.05f);
				case L_CALF: case R_CALF: return at(0.18f * sw, side * 0.11f, feet + 0.5f + lift);
				case L_FOOT: case R_FOOT: return at(0.35f * sw, side * 0.12f, feet + 0.08f + lift);
				case L_TOE: case R_TOE: return at(0.35f * sw + 0.15f, side * 0.12f, feet + 0.05f + lift);
				case L_UPPERARM: case R_UPPERARM: return at(0.0f, side * 0.22f, k.pz + 0.5f);
				case L_FOREARM: case R_FOREARM: return at(-0.1f * sw, side * 0.26f, k.pz + 0.2f);
				case L_HAND: case R_HAND: return at(-0.2f * sw, side * 0.28f, k.pz - 0.1f);
				}
			}

			return Vector(0.0f, 0.0f, 0.0f);
		}

		/// pressure and impulse sources of the posed body
		void body_sources(const float wh, const float dt, const float k, std::vector<body_s>& bodies, std::vector<splash_s>& splashes)
		{
			if (dt <= 0.0f || g_pose.mode == body_mode_e::NONE) {
				return;
			}

			auto& t = g_body_track;
			int idx = 0;

			// mean motion of the body (last frame): a whole body dropping in is handled once by the entry impulse,
			// here only the motion of the parts relative to it plunges into the water
			float body_vz = 0.0f;
			if (t.valid && t.count > 0)
			{
				for (int q = 0; q < t.count; q++) {
					body_vz += t.vz[q];
				}

				body_vz /= static_cast<float>(t.count);
			}

			const bool falling_in = body_vz < -2.0f || now_ms() - g_player.entry_ms < 400;

			for (const auto& part : PARTS)
			{
				const Vector a = body_point(part.a);
				const Vector b = part.b != part.a ? body_point(part.b) : a;
				const float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
				const int pts = part.a == part.b ? 1 : std::clamp(static_cast<int>(len / 0.25f + 0.5f), 1, 3);

				for (int j = 0; j < pts && idx < MAX_POINTS; j++, idx++)
				{
					const float u = (static_cast<float>(j) + 0.5f) / static_cast<float>(pts);
					const float x = a.x + (b.x - a.x) * u, y = a.y + (b.y - a.y) * u, z = a.z + (b.z - a.z) * u;

					// smoothed velocity of this point, reset after a teleport
					if (t.valid && idx < t.count)
					{
						const float nvx = (x - t.px[idx]) / dt, nvy = (y - t.py[idx]) / dt, nvz = (z - t.pz[idx]) / dt;
						if (nvx * nvx + nvy * nvy + nvz * nvz < 30.0f * 30.0f)
						{
							const float s = std::min(dt * 25.0f, 1.0f);
							t.vx[idx] += (nvx - t.vx[idx]) * s;
							t.vy[idx] += (nvy - t.vy[idx]) * s;
							t.vz[idx] += (nvz - t.vz[idx]) * s;
						}
						else {
							t.vx[idx] = t.vy[idx] = t.vz[idx] = 0.0f;
						}
					}

					t.px[idx] = x;
					t.py[idx] = y;
					t.pz[idx] = z;

					// submerged share of this slice, deeper parts barely move the surface
					const float r = part.r;
					const float d = wh - z;
					const float sub = water_physics::clamp01((d + r) / (2.0f * r));
					const float surf_w = std::exp(-std::max(d - r, 0.0f) / 0.35f);
					if (sub <= 0.0f || surf_w < 0.02f) {
						continue;
					}

					const float vx = t.vx[idx], vy = t.vy[idx], vz = t.vz[idx];
					const float vh2 = vx * vx + vy * vy;
					const float vh = std::sqrt(vh2);

					// dynamic pressure at the leading side + the volume carried through the surface
					const float p_motion = part.cp * vh2 / (2.0f * water_physics::G) * sub * surf_w;
					const float crossing = 4.0f * sub * (1.0f - sub);
					const float p_cross = part.cp * (-vz) * std::abs(vz) / (2.0f * water_physics::G) * std::max(crossing, 0.15f * sub) * surf_w;

					// the footprints of neighbouring points overlap ~2 - 2.5x along a limb
					constexpr float OVERLAP = 0.45f;
					const float p = std::clamp((p_motion + p_cross) * OVERLAP, -0.1f, 0.15f);
					if (falling_in || std::abs(p) < 0.002f) {
						continue;
					}

					const float lead = vh > 0.05f ? r / vh : 0.0f;
					const float footprint = std::max(r * 2.5f, 0.3f);
					bodies.push_back({ x + vx * lead, y + vy * lead, footprint, p * k });

					// a fast plunge relative to the body (a hand slapping in, a foot kicking down) also strikes the surface
					const float rel_vz = vz - body_vz;
					if (rel_vz < -1.2f && crossing > 0.3f) {
						splashes.push_back({ x, y, std::max(r * 3.0f, 0.4f), std::min(-rel_vz * 0.4f * (r / 0.1f), 2.0f) * crossing * k });
					}
				}
			}

			t.count = idx;
			t.valid = idx > 0;
		}
	}

	void init()
	{
		if (game::func_addr__CWater_TestLineAgainstWater && game::retn_addr__heli_downwash_water_test) {
			shared::utils::hook::detour(game::func_addr__CWater_TestLineAgainstWater, &CWater_TestLineAgainstWater_hk, DETOUR_CAST(CWater_TestLineAgainstWater_og));
		}

		if (game::func_addr__CWater_UpdateDynamicWater && game::pWaterDynamicHeights && game::pWaterDynamicVelocities
			&& game::pWaterDynamicBaseX && game::pWaterDynamicBaseY)
		{
			shared::utils::hook::detour(game::func_addr__CWater_UpdateDynamicWater, &CWater_UpdateDynamicWater_hk, DETOUR_CAST(CWater_UpdateDynamicWater_og));
			g_cpl_hooked = CWater_UpdateDynamicWater_og != nullptr;
		}

		if (game::func_addr__CExplosion_AddExplosion) {
			shared::utils::hook::detour(game::func_addr__CExplosion_AddExplosion, &CExplosion_AddExplosion_hk, DETOUR_CAST(CExplosion_AddExplosion_og));
		}
	}

	bool coupling_active() {
		return g_cpl_hooked.load() && g_cpl_ready.load() >= 0;
	}

	stats_s get_stats()
	{
		std::lock_guard lock(g_stats_mutex);
		return g_stats;
	}

	void update()
	{
		static auto last = std::chrono::steady_clock::now();
		const auto now = std::chrono::steady_clock::now();
		const float dt = std::clamp(std::chrono::duration<float>(now - last).count(), 0.0f, 0.1f);
		last = now;

		// the sea needs a while to build up and calm down
		if (game::pWeatherWind)
		{
			const float w = std::clamp(*game::pWeatherWind, 0.0f, 1.5f);
			const float cur = g_wind.load();
			g_wind.store(cur + (w - cur) * std::min(dt * 0.5f, 1.0f));
		}

		// no natives while the game is still loading
		static std::uint32_t frames_seen = 0u;
		if (frames_seen < 600u)
		{
			frames_seen++;
			return;
		}

		const auto n = natives::get();
		if (!n || (game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive)) {
			return;
		}

		if (remix_vars::is_paused() || (game::m_CodePause && *game::m_CodePause)) {
			return;
		}

		g_frame++;
		g_time += dt;

		const auto player = n->ConvertIntToPlayerindex(n->GetPlayerId());
		natives::Ped ped = 0;
		if (n->IsPlayerPlaying(player)) {
			n->GetPlayerChar(player, &ped);
		}

		// shore depth map around the render camera
		{
			float cx = 0.0f, cy = 0.0f;
			bool have = false;
			{
				std::lock_guard lock(g_mutex);
				cx = g_shared.cam_x;
				cy = g_shared.cam_y;
				have = g_shared.have_cam;
			}

			if (have)
			{
				update_depth(n, cx, cy);

				static std::uint32_t publish_tick = 0u;
				if (g_depth.changed && ++publish_tick % 15u == 0u)
				{
					g_depth.changed = 0;
					std::lock_guard lock(g_mutex);
					g_shared.depth = g_depth.d;
					g_shared.depth_ox = g_depth.ox;
					g_shared.depth_oy = g_depth.oy;
					g_shared.depth_version++;
				}
			}
		}

		std::vector<hull_s> hulls;
		std::vector<body_s> bodies;
		std::vector<heli_s> helis;
		std::vector<splash_s> splashes;
		std::vector<blast_s> blasts;

		float px = 0.0f, py = 0.0f, pz = 0.0f;
		const bool have_player = ped && !n->IsCharDead(ped);
		if (have_player) {
			n->GetCharCoordinates(ped, &px, &py, &pz);
		}

		const bool on = enabled();
		const float k = strength();

		// everything rigid near the player: vehicles, peds and objects, each from its own pose and motion.
		// the simulation takes a new state 30 times a second: looked at no more often than that, kept in between
		static std::vector<hull_s> kept_hulls;
		static std::vector<body_s> kept_bodies;
		static std::vector<heli_s> kept_helis;
		static int kept_vehicles = 0;
		static float since_entities = 1.0f;
		since_entities += dt;

		int vehicles_seen = 0;
		if (on && have_player && since_entities < STEP * 0.9f)
		{
			hulls = kept_hulls;
			bodies = kept_bodies;
			helis = kept_helis;
			vehicles_seen = kept_vehicles;
		}
		else if (on && have_player)
		{
			since_entities = 0.0f;
			static std::uint32_t entity_pass = 0u;
			entity_pass++;
			void** const pools[3] = { game::pVehiclePool, game::pPedPool, game::pObjectPool };
			constexpr float RANGE[3] = { 170.0f, 90.0f, 70.0f };
			int objects_done = 0;

			for (int pi = 0; pi < 3; pi++)
			{
				const auto pool = entity_pool(pools[pi]);
				if (!pool) {
					continue;
				}

				auto& hot = g_hot[pi];
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

					// what is in or near the water is followed every frame, the rest every 4th (vehicles always: wakes
					// need their motion)
					if (pi != 0 && !hot[i] && ((static_cast<std::uint32_t>(i) + entity_pass) & 3u) != 0u) {
						continue;
					}

					const float* m = entity_matrix(pool, i);
					if (!m) {
						continue;
					}

					if (pi == 0) {
						vehicles_seen++;
					}

					const float x = m[12], y = m[13];
					const float ddx = x - px, ddy = y - py;
					if (!std::isfinite(x) || !std::isfinite(y) || ddx * ddx + ddy * ddy > RANGE[pi] * RANGE[pi]) {
						continue;
					}

					const int handle = (i << 8) | static_cast<std::uint8_t>(flag);

					// once per pool: slot + flag has to be the script handle
					if (!g_pool_checked[pi])
					{
						float cx = 0.0f, cy = 0.0f, cz = 0.0f;
						if (pi == 0) {
							n->GetCarCoordinates(handle, &cx, &cy, &cz);
						} else if (pi == 1) {
							n->GetCharCoordinates(handle, &cx, &cy, &cz);
						} else {
							n->GetObjectCoordinates(handle, &cx, &cy, &cz);
						}

						// a ped in a vehicle reports the vehicle: checked with the next one
						if (pi == 1 && (std::abs(cx - x) > 0.5f || std::abs(cy - y) > 0.5f) && n->IsCharInAnyCar(handle)) {
							continue;
						}

						g_pool_ok[pi] = std::abs(cx - x) < 0.5f && std::abs(cy - y) < 0.5f;
						g_pool_checked[pi] = true;

						if (!g_pool_ok[pi]) {
							shared::common::log("WaterDynamics", "Entity pool handles do not match, these entities do not disturb the water.", shared::common::LOG_TYPE::LOG_TYPE_ERROR, false);
						}
					}

					if (!g_pool_ok[pi]) {
						break;
					}

					// the player has his own body model
					if (pi == 1 && handle == ped) {
						continue;
					}

					// at most 128 objects a frame
					if (pi == 2 && ++objects_done > 128) {
						break;
					}

					hot[i] = rigid_entity(n, pi, handle, m, k, hulls, bodies, helis, splashes) ? 1u : 0u;
				}
			}

			// forget what is gone
			if (g_frame % 120u == 0u) {
				std::erase_if(g_tracks, [](const auto& it) { return g_frame - it.second.seen > 240u; });
			}

			kept_hulls = hulls;
			kept_bodies = bodies;
			kept_helis = helis;
			kept_vehicles = vehicles_seen;
		}

		// the player in the water
		if (on && have_player && !n->IsCharInAnyCar(ped))
		{
			float vx = 0.0f, vy = 0.0f, vz = 0.0f;
			n->GetCharVelocity(ped, &vx, &vy, &vz);

			// the velocity native reads ~0 for a swimming ped: horizontal speed from his movement, reset after a teleport
			if (g_player.has_pos && dt > 0.0f)
			{
				const float mx = (px - g_player.last_x) / dt, my = (py - g_player.last_y) / dt;
				if (mx * mx + my * my < 15.0f * 15.0f) {
					g_player.move_speed += (std::sqrt(mx * mx + my * my) - g_player.move_speed) * std::min(dt * 6.0f, 1.0f);
				} else {
					g_player.move_speed = 0.0f;
				}
			}

			g_player.last_x = px;
			g_player.last_y = py;
			g_player.has_pos = true;
			const float speed = std::max(std::sqrt(vx * vx + vy * vy), g_player.move_speed);

			float wh = 0.0f;
			const bool water_here = n->GetWaterHeightNoWaves(px, py, pz, &wh);
			const bool swimming = n->IsCharSwimming(ped);
			const float feet = pz - 1.0f;
			const float depth = water_here ? wh - feet : -1.0f;
			const bool in_water = swimming || depth > 0.05f;

			// jumped or fell in: the body punches a cavity at most of its entry speed, it rings out in short waves
			if (in_water && !g_player.was_in_water && g_player.vz < -1.5f)
			{
				const float v = std::min(-g_player.vz, 12.0f);
				splashes.push_back({ px, py, 0.7f, v * 0.8f * k });
				g_player.entry_ms = now_ms();
			}

			float heading = 0.0f;
			n->GetCharHeading(ped, &heading);
			const float h = heading * 0.0174532925f;

			g_pose.t += dt;
			g_pose.px = px;
			g_pose.py = py;
			g_pose.pz = pz;
			g_pose.wh = wh;
			g_pose.fx = -std::sin(h);
			g_pose.fy = std::cos(h);
			g_pose.speed = speed;
			g_pose.mode = !water_here ? body_mode_e::NONE
				: swimming ? (speed > 0.4f ? body_mode_e::CRAWL : body_mode_e::TREADING)
				: (in_water && depth < 1.4f) ? body_mode_e::WADING : body_mode_e::NONE;

			body_sources(wh, dt, k, bodies, splashes);

			g_player.was_in_water = in_water;
			g_player.vz = vz;
		}

		// bullets into the water (anyone's): each strikes a small sharp ring where the game found it meeting the surface
		{
			std::vector<shot_s> shots;
			{
				std::lock_guard lock(g_shot_mutex);
				shots.swap(g_shots);
			}

			if (on)
			{
				for (const auto& s : shots) {
					splashes.push_back({ s.x, s.y, 0.3f, 2.5f * k });
				}
			}
		}

		// explosions from the hook
		std::vector<explosion_s> explosions;
		{
			std::lock_guard lock(g_explosion_mutex);
			explosions.swap(g_explosions);
		}

		int near_water = 0;
		for (const auto& e : explosions)
		{
			float wh = 0.0f;
			if (!on || !n->GetWaterHeightNoWaves(e.x, e.y, e.z, &wh)) {
				continue;
			}

			const float radius = e.radius > 0.1f ? e.radius : default_radius(e.type);
			const float height = e.z - wh;
			if (height < radius * 1.2f + 1.0f && height > -6.0f)
			{
				blasts.push_back({ e.x, e.y, radius * std::min(k, 2.0f), std::max(height, 0.0f) });
				near_water++;
			}
		}

		{
			std::lock_guard lock(g_mutex);
			g_shared.hulls = std::move(hulls);
			g_shared.bodies = std::move(bodies);
			g_shared.helis = std::move(helis);

			if (g_shared.splashes.size() < 256u) {
				g_shared.splashes.insert(g_shared.splashes.end(), splashes.begin(), splashes.end());
			}

			g_shared.blasts.insert(g_shared.blasts.end(), blasts.begin(), blasts.end());
			g_shared.game_dt = std::min(g_shared.game_dt + dt, 0.25f);
		}

		{
			std::lock_guard lock(g_stats_mutex);
			g_stats.vehicles_seen = vehicles_seen;
			g_stats.pool_ok = g_pool_ok;
			g_stats.explosions += near_water;
			g_stats.splashes += static_cast<int>(splashes.size());
		}
	}

	void render_step(const float cam_x, const float cam_y)
	{
		// the simulation thread starts with the first water render
		if (!g_thread_started.exchange(true))
		{
			g_thread = std::thread(sim_thread);
			g_thread.detach();
		}

		// switched off and on again: start calm
		const bool on = enabled();
		static bool was_on = true;
		if (on != was_on)
		{
			g_reset = true;
			was_on = on;
		}

		{
			std::lock_guard lock(g_mutex);
			g_shared.cam_x = cam_x;
			g_shared.cam_y = cam_y;
			g_shared.have_cam = true;

			if (!on)
			{
				g_shared.hulls.clear();
				g_shared.bodies.clear();
				g_shared.helis.clear();
				g_shared.splashes.clear();
				g_shared.blasts.clear();
			}

			g_work = true;
		}

		g_cv.notify_one();

		// newest published heights, held until the next render
		{
			std::lock_guard lock(g_out_mutex);
			g_out_front = g_out_latest;
			g_front_frame = g_out_front >= 0 ? g_out[g_out_front].frame : frame_s {};
		}

		// only the wind sea while the interactive fields are off
		if (!on)
		{
			g_front_frame.wake = {};
			g_front_frame.ripple = {};
		}

		std::lock_guard lock(g_stats_mutex);
		{
			std::lock_guard lock2(g_mutex);
			g_stats.hulls = static_cast<int>(g_shared.hulls.size());
			g_stats.bodies = static_cast<int>(g_shared.bodies.size());
			g_stats.helis = static_cast<int>(g_shared.helis.size());
		}

		g_stats.wind = g_wind.load();
		g_stats.wake_rms = g_front_frame.wake_rms;
		g_stats.ripple_rms = g_front_frame.ripple_rms;
		g_stats.ocean_hs = g_front_frame.ocean_hs;
		g_stats.sim_ms = g_sim_ms.load();
	}

	const frame_s& frame() {
		return g_front_frame;
	}
}
