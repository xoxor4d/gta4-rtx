#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <random>
#include <numbers>

// water surface physics, linear wave theory in metres and seconds
//
// ewave_s: interactive water, Tessendorf's eWave ("eWave: Using an Exponential Solver on the iWave Problem", 2014)
//   height h and velocity potential phi, propagated exactly in fourier space:
//     h'   = cos(w dt) h   + (K / w) sin(w dt) phi      K = k tanh(kd)
//     phi' = cos(w dt) phi - (w / K) sin(w dt) h        w^2 = (g k + sigma k^3) tanh(kd)
//   unconditionally stable and free of numerical dispersion: a moving pressure patch leaves a kelvin wake
//
// ocean_s: wind sea, Tessendorf FFT ocean with a fetch limited JONSWAP spectrum, cos^2s spreading and a narrow swell,
//   split into three wavelength bands (>= 16 m, 4 - 16 m, < 4 m) so coarse mesh cells never show waves they cannot resolve

namespace gta4::water_physics
{
	constexpr float G = 9.81f;
	constexpr float RHO = 1025.0f;     // sea water (kg/m3)
	constexpr float SIGMA = 7.28e-5f;  // surface tension / density (m^3/s^2)
	constexpr float DEPTH = 15.0f;     // harbour and rivers, roughly
	constexpr float TWO_PI = 6.28318531f;
	using cf = std::complex<float>;

	inline float omega_of(const float k, const float depth = DEPTH) {
		return std::sqrt((G * k + SIGMA * k * k * k) * std::tanh(k * depth));
	}

	inline float clamp01(const float v) {
		return std::clamp(v, 0.0f, 1.0f);
	}

	/// read only view of a height grid
	struct grid_view_s
	{
		int n = 0;
		float dx = 1.0f;
		int ox = 0, oy = 0;     // world cell index of cell [0][0] (non periodic grids)
		bool periodic = false;  // tiles over the world (ocean)
		const float* h = nullptr;

		bool valid() const { return h && n > 0; }

		/// catmull-rom sample with its gradient (bilinear on the coarse grids shows faceted crests)
		void sample_cubic(const float x, const float y, float& out_h, float& out_dx, float& out_dy) const
		{
			out_h = out_dx = out_dy = 0.0f;
			if (!valid()) {
				return;
			}

			const float fx = x / dx - static_cast<float>(ox);
			const float fy = y / dx - static_cast<float>(oy);
			const int i = static_cast<int>(std::floor(fx)), j = static_cast<int>(std::floor(fy));
			const float tx = fx - static_cast<float>(i), ty = fy - static_cast<float>(j);

			if (!periodic && (i < 2 || j < 2 || i >= n - 3 || j >= n - 3))
			{
				sample(x, y, out_h, out_dx, out_dy);
				return;
			}

			const auto weights = [](const float t, float w[4], float dw[4])
				{
					const float t2 = t * t, t3 = t2 * t;
					w[0] = 0.5f * (-t3 + 2.0f * t2 - t);           dw[0] = 0.5f * (-3.0f * t2 + 4.0f * t - 1.0f);
					w[1] = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);  dw[1] = 0.5f * (9.0f * t2 - 10.0f * t);
					w[2] = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);    dw[2] = 0.5f * (-9.0f * t2 + 8.0f * t + 1.0f);
					w[3] = 0.5f * (t3 - t2);                       dw[3] = 0.5f * (3.0f * t2 - 2.0f * t);
				};

			float wx[4], dwx[4], wy[4], dwy[4];
			weights(tx, wx, dwx);
			weights(ty, wy, dwy);

			float hsum = 0.0f, gx = 0.0f, gy = 0.0f;
			for (int b = 0; b < 4; b++)
			{
				const int jj = periodic ? ((j - 1 + b) & (n - 1)) : (j - 1 + b);
				const float* row = h + static_cast<size_t>(jj) * n;
				float r = 0.0f, rd = 0.0f;

				for (int a = 0; a < 4; a++)
				{
					const int ii = periodic ? ((i - 1 + a) & (n - 1)) : (i - 1 + a);
					r += row[ii] * wx[a];
					rd += row[ii] * dwx[a];
				}

				hsum += r * wy[b];
				gx += rd * wy[b];
				gy += r * dwy[b];
			}

			out_h = hsum;
			out_dx = gx / dx;
			out_dy = gy / dx;
		}

		/// bilinear sample with its gradient, 0 outside of non periodic grids
		void sample(const float x, const float y, float& out_h, float& out_dx, float& out_dy) const
		{
			out_h = out_dx = out_dy = 0.0f;
			if (!valid()) {
				return;
			}

			const float fx = x / dx - static_cast<float>(ox);
			const float fy = y / dx - static_cast<float>(oy);
			int i = static_cast<int>(std::floor(fx));
			int j = static_cast<int>(std::floor(fy));
			const float tx = fx - static_cast<float>(i);
			const float ty = fy - static_cast<float>(j);

			int i1, j1;
			if (periodic)
			{
				// n is a power of two
				i &= n - 1; j &= n - 1;
				i1 = (i + 1) & (n - 1); j1 = (j + 1) & (n - 1);
			}
			else
			{
				if (i < 1 || j < 1 || i >= n - 2 || j >= n - 2) {
					return;
				}

				i1 = i + 1; j1 = j + 1;
			}

			const float a = h[static_cast<size_t>(j) * n + i], b = h[static_cast<size_t>(j) * n + i1];
			const float c = h[static_cast<size_t>(j1) * n + i], d = h[static_cast<size_t>(j1) * n + i1];
			const float ab = a + (b - a) * tx, cd = c + (d - c) * tx;
			out_h = ab + (cd - ab) * ty;
			out_dx = ((b - a) + ((d - c) - (b - a)) * ty) / dx;
			out_dy = ((c - a) + ((d - b) - (c - a)) * tx) / dx;
		}
	};

	inline void fft2(std::vector<cf>& z, const int n, const bool forward, const float scale)
	{
		const pocketfft::shape_t shape = { static_cast<size_t>(n), static_cast<size_t>(n) };
		const pocketfft::stride_t stride = { static_cast<ptrdiff_t>(sizeof(cf) * n), static_cast<ptrdiff_t>(sizeof(cf)) };
		const pocketfft::shape_t axes = { 0, 1 };
		pocketfft::c2c(shape, stride, stride, axes, forward, z.data(), z.data(), scale, 1);
	}

	/// camera centred wave field (eWave)
	struct ewave_s
	{
		int n = 0;
		float dx = 1.0f;
		float dt = 1.0f / 60.0f;
		int sponge = 16;
		int ox = 0, oy = 0;
		bool placed = false;

		std::vector<float> h, phi, tmp, sponge_w;
		std::vector<cf> z, zs;
		std::vector<float> c_tab, a_tab, b_tab;
		std::vector<int> mirror;
		float energy = 0.0f;

		/// nu: effective kinematic viscosity (m^2/s), half_life: extra decay of every wave (s, 0 = none)
		void init(const int cells, const float cell_size, const float step, const int sponge_cells, const float nu, const float half_life)
		{
			n = cells; dx = cell_size; dt = step; sponge = sponge_cells;
			const size_t count = static_cast<size_t>(n) * n;
			h.assign(count, 0.0f); phi.assign(count, 0.0f); tmp.assign(count, 0.0f);
			z.assign(count, cf()); zs.assign(count, cf());
			c_tab.assign(count, 1.0f); a_tab.assign(count, 0.0f); b_tab.assign(count, 0.0f);
			mirror.assign(count, 0);

			const float dk = TWO_PI / (static_cast<float>(n) * dx);
			const float extra = half_life > 0.0f ? std::exp(-0.69314718f * dt / half_life) : 1.0f;

			for (int j = 0; j < n; j++)
			{
				for (int i = 0; i < n; i++)
				{
					const size_t idx = static_cast<size_t>(j) * n + i;
					mirror[idx] = ((n - j) % n) * n + (n - i) % n;

					const float kx = dk * static_cast<float>(i < n / 2 ? i : i - n);
					const float ky = dk * static_cast<float>(j < n / 2 ? j : j - n);
					const float k = std::sqrt(kx * kx + ky * ky);
					if (k <= 0.0f) {
						continue;
					}

					const float w = omega_of(k);
					const float kk = k * std::tanh(k * DEPTH);
					const float damp = std::exp(-2.0f * nu * k * k * dt) * extra;
					c_tab[idx] = std::cos(w * dt) * damp;
					a_tab[idx] = kk / w * std::sin(w * dt) * damp;
					b_tab[idx] = w / kk * std::sin(w * dt) * damp;
				}
			}

			// absorbing border, soft enough to not reflect
			sponge_w.assign(static_cast<size_t>(n), 1.0f);
			for (int i = 0; i < n; i++)
			{
				const int d = std::min(i, n - 1 - i);
				if (d < sponge)
				{
					const float t = 1.0f - static_cast<float>(d) / static_cast<float>(sponge);
					sponge_w[i] = 1.0f - 0.15f * t * t;
				}
			}

			placed = false;
		}

		void clear()
		{
			std::fill(h.begin(), h.end(), 0.0f);
			std::fill(phi.begin(), phi.end(), 0.0f);
			energy = 0.0f;
		}

		float world_x0() const { return static_cast<float>(ox) * dx; }
		float world_y0() const { return static_cast<float>(oy) * dx; }

		void shift(std::vector<float>& a, const int sx, const int sy)
		{
			std::fill(tmp.begin(), tmp.end(), 0.0f);
			const int i0 = std::max(0, -sx), i1 = std::min(n, n - sx);

			for (int j = 0; j < n; j++)
			{
				const int sj = j + sy;
				if (sj >= 0 && sj < n && i1 > i0) {
					std::memcpy(&tmp[static_cast<size_t>(j) * n + i0], &a[static_cast<size_t>(sj) * n + i0 + sx], sizeof(float) * (i1 - i0));
				}
			}

			a.swap(tmp);
		}

		/// keeps the grid centred on (cx, cy): moves by whole cells, new cells start calm
		void recenter(const float cx, const float cy)
		{
			const int nox = static_cast<int>(std::floor(cx / dx)) - n / 2;
			const int noy = static_cast<int>(std::floor(cy / dx)) - n / 2;

			if (!placed)
			{
				ox = nox; oy = noy; placed = true;
				clear();
				return;
			}

			// the absorbing border hides a few cells of lag
			const int sx = nox - ox, sy = noy - oy;
			if (std::abs(sx) < 4 && std::abs(sy) < 4) {
				return;
			}

			ox = nox; oy = noy;
			if (std::abs(sx) >= n || std::abs(sy) >= n)
			{
				clear();
				return;
			}

			shift(h, sx, sy);
			shift(phi, sx, sy);
		}

		/// visits the cells of a gaussian footprint (radius in m): fn(index, weight)
		template <typename Fn>
		void footprint(const float x, const float y, const float radius, Fn&& fn)
		{
			if (!placed || radius <= 0.0f) {
				return;
			}

			const float fx = (x - world_x0()) / dx, fy = (y - world_y0()) / dx;
			const float r = std::max(radius / dx, 0.5f);
			const int reach = static_cast<int>(std::ceil(r * 2.2f));
			const int ci = static_cast<int>(std::floor(fx)), cj = static_cast<int>(std::floor(fy));
			const float inv = 1.0f / (r * r);

			for (int j = std::max(1, cj - reach); j <= std::min(n - 2, cj + reach); j++)
			{
				for (int i = std::max(1, ci - reach); i <= std::min(n - 2, ci + reach); i++)
				{
					const float ddx = static_cast<float>(i) - fx, ddy = static_cast<float>(j) - fy;
					const float d2 = (ddx * ddx + ddy * ddy) * inv;
					if (d2 < 4.8f) {
						fn(static_cast<size_t>(j) * n + i, std::exp(-d2));
					}
				}
			}
		}

		/// surface pressure for one step, as the depth of water it holds down (p / rho = g * depth)
		void press(const float x, const float y, const float radius, const float depth)
		{
			const float k = G * depth * dt;
			footprint(x, y, radius, [&](const size_t idx, const float w) { phi[idx] -= k * w; });
		}

		/// impulse: the surface is struck down at speed (m/s) over a patch of this radius
		void strike(const float x, const float y, const float radius, const float speed)
		{
			// a patch of radius R moving at w has phi ~ -w R (dh/dt = |k| phi, |k| ~ 1 / R)
			const float k = -speed * std::max(radius, dx) * 0.8f;
			footprint(x, y, radius, [&](const size_t idx, const float w) { phi[idx] += k * w; });
		}

		void step()
		{
			if (!placed) {
				return;
			}

			const size_t count = static_cast<size_t>(n) * n;
			for (size_t i = 0; i < count; i++) {
				z[i] = cf(h[i], phi[i]);
			}

			fft2(z, n, true, 1.0f);

			// unpack the spectra of the two real fields, propagate, pack again
			for (size_t i = 0; i < count; i++)
			{
				const cf zk = z[i];
				const cf zm = std::conj(z[static_cast<size_t>(mirror[i])]);
				const cf hk = (zk + zm) * 0.5f;
				const cf pk = (zk - zm) * cf(0.0f, -0.5f);
				const cf hn = c_tab[i] * hk + a_tab[i] * pk;
				const cf pn = c_tab[i] * pk - b_tab[i] * hk;
				zs[i] = hn + cf(0.0f, 1.0f) * pn;
			}

			fft2(zs, n, false, 1.0f / static_cast<float>(count));

			double e = 0.0;
			for (int j = 0; j < n; j++)
			{
				const float swj = sponge_w[j];
				for (int i = 0; i < n; i++)
				{
					const size_t idx = static_cast<size_t>(j) * n + i;
					const float s = swj * sponge_w[i];
					const float hv = std::clamp(zs[idx].real() * s, -3.0f, 3.0f);
					h[idx] = hv;
					phi[idx] = zs[idx].imag() * s;
					e += static_cast<double>(hv) * hv;
				}
			}

			energy = static_cast<float>(e);
		}

		float rms() const { return std::sqrt(energy / static_cast<float>(std::max(n * n, 1))); }
	};

	/// periodic wind sea tile (FFT ocean)
	struct ocean_s
	{
		static constexpr int N = 256;
		static constexpr float L = 128.0f;         // tile size (m), 0.5 m cells
		static constexpr float BAND_LONG = 16.0f;  // >= : every mesh cell shows it
		static constexpr float BAND_SHORT = 4.0f;  // <  : only the 0.5 unit mesh

		std::vector<cf> xi, h0, h0m, z, zc;
		std::vector<cf> phase, rot;                // exp(i w t) per mode, advanced by multiplying with exp(i w dt)
		double time = 0.0;
		float rot_dt = 0.0f;
		int since_resync = 0;
		std::vector<float> omega;
		std::vector<std::uint8_t> band;
		std::vector<float> long_h, mid_h, short_h; // short_h and the displacement are band limited to the 0.5 unit mesh
		std::vector<cf> zd;
		std::vector<float> disp_x, disp_y;         // horizontal (choppy) displacement of the waves below 16 m
		std::vector<float> kxn, kyn;               // unit wave vector per mode
		std::vector<float> fine;                   // band limit of the 0.5 unit mesh per mode: the response of a 1-2-1 filter
		                                           // along both axes, cos^2(kx dx / 2) cos^2(ky dx / 2) (0 at two cells)
		float choppiness = 0.4f;                   // tessendorf lambda, 1 folds the surface over (high values pinch the crests)
		float wind_speed = -1.0f, wind_dir = 0.0f, scale = 1.0f;
		float hs = 0.0f;                           // significant wave height (m)
		bool calm = true;

		void init()
		{
			const size_t count = static_cast<size_t>(N) * N;
			xi.resize(count); h0.assign(count, cf()); h0m.assign(count, cf()); z.assign(count, cf()); zc.assign(count, cf());
			omega.assign(count, 0.0f); band.assign(count, 0);
			long_h.assign(count, 0.0f); mid_h.assign(count, 0.0f); short_h.assign(count, 0.0f);
			zd.assign(count, cf()); disp_x.assign(count, 0.0f); disp_y.assign(count, 0.0f);
			kxn.assign(count, 0.0f); kyn.assign(count, 0.0f); fine.assign(count, 1.0f);

			// fixed phases: a wind change only rescales the modes
			std::mt19937 rng(0x5EA5EAu);
			std::normal_distribution<float> nd(0.0f, 1.0f);
			const float dk = TWO_PI / L;

			for (int j = 0; j < N; j++)
			{
				for (int i = 0; i < N; i++)
				{
					const size_t idx = static_cast<size_t>(j) * N + i;
					xi[idx] = cf(nd(rng), nd(rng));

					const float kx = dk * static_cast<float>(i < N / 2 ? i : i - N);
					const float ky = dk * static_cast<float>(j < N / 2 ? j : j - N);
					const float k = std::sqrt(kx * kx + ky * ky);
					omega[idx] = k > 0.0f ? omega_of(k) : 0.0f;
					kxn[idx] = k > 0.0f ? kx / k : 0.0f;
					kyn[idx] = k > 0.0f ? ky / k : 0.0f;

					const float cx = std::cos(0.5f * kx * L / N), cy = std::cos(0.5f * ky * L / N);
					fine[idx] = cx * cx * cy * cy;

					const float lambda = k > 0.0f ? TWO_PI / k : 1e9f;
					band[idx] = lambda >= BAND_LONG ? 0 : lambda >= BAND_SHORT ? 1 : 2;
				}
			}
		}

		static int mirror_index(const size_t idx)
		{
			const int j = static_cast<int>(idx) / N, i = static_cast<int>(idx) % N;
			return ((N - j) % N) * N + (N - i) % N;
		}

		/// JONSWAP for wind speed u (m/s at 10 m) over a fetch (m), waves heading along dir (radians)
		void build(const float u, const float fetch, const float dir, const float height_scale)
		{
			wind_speed = u; wind_dir = dir; scale = height_scale;
			const float alpha = 0.076f * std::pow(u * u / (fetch * G), 0.22f);
			const float wp = 22.0f * std::cbrt(G * G / (u * fetch));
			constexpr float GAMMA = 3.3f;
			constexpr float SPREAD = 8.0f; // cos^2s(theta / 2), a young wind sea
			const float dk = TWO_PI / L;

			// directional normalisation (integral of cos^2s over the circle)
			float dnorm = 0.0f;
			for (int a = 0; a < 720; a++) {
				dnorm += std::pow(std::abs(std::cos((static_cast<float>(a) / 720.0f * TWO_PI - std::numbers::pi_v<float>) * 0.5f)), 2.0f * SPREAD);
			}
			dnorm *= TWO_PI / 720.0f;

			double variance = 0.0;
			for (int j = 0; j < N; j++)
			{
				for (int i = 0; i < N; i++)
				{
					const size_t idx = static_cast<size_t>(j) * N + i;
					const float kx = dk * static_cast<float>(i < N / 2 ? i : i - N);
					const float ky = dk * static_cast<float>(j < N / 2 ? j : j - N);
					const float k = std::sqrt(kx * kx + ky * ky);
					if (k <= 0.0f || u <= 0.0f)
					{
						h0[idx] = cf();
						continue;
					}

					const float w = omega[idx];
					const float sig = w <= wp ? 0.07f : 0.09f;
					const float r = std::exp(-(w - wp) * (w - wp) / (2.0f * sig * sig * wp * wp));
					const float s_w = alpha * G * G / std::pow(w, 5.0f) * std::exp(-1.25f * std::pow(wp / w, 4.0f)) * std::pow(GAMMA, r);

					const float th = std::remainder(std::atan2(ky, kx) - dir, TWO_PI);
					const float d_th = std::pow(std::abs(std::cos(th * 0.5f)), 2.0f * SPREAD) / dnorm;

					// S(k) dk^2 = S(w) D(theta) (dw/dk) / k dkx dky
					const float dwdk = (omega_of(k * 1.01f) - omega_of(k * 0.99f)) / (k * 0.02f);
					const float p = s_w * d_th * dwdk / k * dk * dk;
					h0[idx] = xi[idx] * (0.5f * std::sqrt(p) * height_scale);
					variance += p * height_scale * height_scale;
				}
			}

			// swell from the open sea: 40 - 130 m wavelength, narrow in direction, ~0.1 m on calm days up to ~0.8 m in storms
			{
				const float wind01 = std::clamp((u - 2.0f) / 16.0f, 0.0f, 1.2f);
				const float hs_swell = (0.08f + 0.7f * wind01 * wind01) * height_scale;
				const float k_peak = TWO_PI / 80.0f;
				const float sdir = dir + 0.35f;

				std::vector<std::pair<size_t, float>> modes;
				float wsum = 0.0f;

				for (int j = 0; j < N; j++)
				{
					for (int i = 0; i < N; i++)
					{
						const float kx = dk * static_cast<float>(i < N / 2 ? i : i - N);
						const float ky = dk * static_cast<float>(j < N / 2 ? j : j - N);
						const float k = std::sqrt(kx * kx + ky * ky);
						if (k <= 0.0f || k < TWO_PI / 130.0f || k > TWO_PI / 40.0f) {
							continue;
						}

						const float th = std::remainder(std::atan2(ky, kx) - sdir, TWO_PI);
						if (std::abs(th) > 0.6f) {
							continue;
						}

						const float wk = std::exp(-std::pow(std::log(k / k_peak) / 0.35f, 2.0f)) * std::exp(-th * th / (2.0f * 0.25f * 0.25f));
						if (wk > 1e-3f)
						{
							modes.push_back({ static_cast<size_t>(j) * N + i, wk });
							wsum += wk;
						}
					}
				}

				if (wsum > 0.0f && hs_swell > 0.0f)
				{
					const float var_target = (hs_swell / 4.0f) * (hs_swell / 4.0f);
					for (const auto& [idx, wk] : modes)
					{
						// unit variance amplitude like the wind sea, scaled to this mode's share of the swell energy
						const float p = var_target * wk / wsum;
						h0[idx] += xi[idx] * (0.5f * std::sqrt(p) * 1.41421356f);
						variance += p;
					}
				}
			}

			for (size_t idx = 0; idx < h0.size(); idx++) {
				h0m[idx] = std::conj(h0[static_cast<size_t>(mirror_index(idx))]);
			}

			hs = 4.0f * static_cast<float>(std::sqrt(variance));
			calm = hs < 0.003f;
		}

		/// the sea at time t (s): exact phases, then the inverse FFTs
		void evaluate(const double t)
		{
			const size_t count = static_cast<size_t>(N) * N;
			phase.resize(count);

			for (size_t idx = 0; idx < count; idx++)
			{
				const double ph = std::fmod(static_cast<double>(omega[idx]) * t, 6.283185307179586);
				phase[idx] = cf(static_cast<float>(std::cos(ph)), static_cast<float>(std::sin(ph)));
			}

			time = t;
			since_resync = 0;
			synthesize();
		}

		/// advances by dt with one complex multiply per mode, exact phases every 600 steps against float drift
		void advance(const float dt, const int steps = 1)
		{
			if (phase.empty() || (since_resync += steps) > 600)
			{
				evaluate(time + static_cast<double>(dt) * steps);
				return;
			}

			const size_t count = static_cast<size_t>(N) * N;
			if (rot.size() != count || rot_dt != dt)
			{
				rot.resize(count);
				for (size_t idx = 0; idx < count; idx++) {
					rot[idx] = cf(std::cos(omega[idx] * dt), std::sin(omega[idx] * dt));
				}

				rot_dt = dt;
			}

			for (size_t idx = 0; idx < count; idx++)
			{
				cf p = phase[idx];
				for (int i = 0; i < steps; i++) {
					p *= rot[idx];
				}

				phase[idx] = p;
			}

			time += static_cast<double>(dt) * steps;
			synthesize();
		}

		/// three inverse FFTs: long + i mid, short, choppy displacement
		void synthesize()
		{
			if (calm)
			{
				std::fill(long_h.begin(), long_h.end(), 0.0f);
				std::fill(mid_h.begin(), mid_h.end(), 0.0f);
				std::fill(short_h.begin(), short_h.end(), 0.0f);
				std::fill(disp_x.begin(), disp_x.end(), 0.0f);
				std::fill(disp_y.begin(), disp_y.end(), 0.0f);
				return;
			}

			const size_t count = static_cast<size_t>(N) * N;
			for (size_t idx = 0; idx < count; idx++)
			{
				const cf e = phase[idx];
				const cf hk = h0[idx] * e + h0m[idx] * std::conj(e);
				zc[idx] = cf();

				switch (band[idx])
				{
				case 0: z[idx] = hk; break;
				case 1: z[idx] = cf(0.0f, 1.0f) * hk; break;
				default: z[idx] = cf(); zc[idx] = hk * fine[idx]; break;
				}

				// D(k) = -i k / |k| h(k), the long band is too gentle to matter: dx + i dy in one transform
				if (band[idx] != 0)
				{
					const cf dxk = cf(0.0f, -kxn[idx]) * hk;
					const cf dyk = cf(0.0f, -kyn[idx]) * hk;
					zd[idx] = (dxk + cf(0.0f, 1.0f) * dyk) * fine[idx];
				}
				else {
					zd[idx] = cf();
				}
			}

			fft2(z, N, false, 1.0f);
			fft2(zc, N, false, 1.0f);
			fft2(zd, N, false, 1.0f);

			for (size_t idx = 0; idx < count; idx++)
			{
				long_h[idx] = z[idx].real();
				mid_h[idx] = z[idx].imag();
				short_h[idx] = zc[idx].real();
				disp_x[idx] = zd[idx].real() * choppiness;
				disp_y[idx] = zd[idx].imag() * choppiness;
			}
		}
	};

	// ---- rigid bodies ------------------------------------------------------------------------------------------------

	/// a rigid body touching the water: its oriented bounding box now and one game frame ago
	struct rigid_body_s
	{
		float axis[3][3] = {};       // unit axes of the box (world): right, forward, up
		float half[3] = {};          // half extents along them (m)
		float centre[3] = {};        // box centre (world)
		float prev_axis[3][3] = {};
		float prev_centre[3] = {};
		bool has_prev = false;
		float dt = 0.0f;             // s between the two poses
		float water = 0.0f;          // level of the calm surface there
		float mass = 0.0f;           // kg, 0 = unknown
		float stagnation = 0.5f;     // share of the stagnation pressure the leading edge holds (sharp hull ~0.15, bluff ~0.5)
		float planing = 0.5f;        // dynamic lift of the wetted bottom (see rigid_body_sources)
		bool hull = false;           // boat hull form: v bottom, bow rising and narrowing along the forward axis
		bool rounded = false;        // round footprint (the ellipse in the box): fuselages
	};

	struct press_point_s
	{
		float x, y, radius, depth;   // surface pressure as the depth of water it holds down (negative: suction)
	};

	struct strike_point_s
	{
		float x, y, radius, speed;   // surface struck down at speed (m/s) over a patch of radius
	};

	/// What a rigid body does to the surface, from nothing but where it is and how it moves:
	/// - every part of it below the surface presses the water down by its own draft there (the hydrostatic footprint; moved
	///   along, it is what makes a wake, and a body dropping or bobbing changes it and sends out rings)
	/// - parts ploughing into the water hold their stagnation pressure v^2 / 2g at the leading edge, the trailing edge sucks
	/// - parts crossing the surface downwards strike it at their speed
	/// The box is sampled along vertical lines on a lattice of cell_size (the wave field's cells), so pitch, roll, heel in a
	/// turn or a capsized hull need no special case. Returns the wetted share of the lattice (0 - 1).
	inline float rigid_body_sources(const rigid_body_s& b, const float cell_size, const float gain, std::vector<press_point_s>& press, std::vector<strike_point_s>& strikes)
	{
		// the lattice lies in the two most horizontal axes
		int w = 2;
		if (std::abs(b.axis[0][2]) > std::abs(b.axis[w][2])) { w = 0; }
		if (std::abs(b.axis[1][2]) > std::abs(b.axis[w][2])) { w = 1; }
		const int i = (w + 1) % 3, j = (w + 2) % 3;

		const int ni = std::clamp(static_cast<int>(std::lround(2.0f * b.half[i] / cell_size)), 1, 16);
		const int nj = std::clamp(static_cast<int>(std::lround(2.0f * b.half[j] / cell_size)), 1, 16);
		const float si = 2.0f * b.half[i] / static_cast<float>(ni), sj = 2.0f * b.half[j] / static_cast<float>(nj);
		const float radius = std::max(0.9f * std::sqrt(si * sj), 0.75f * cell_size);
		const float share = si * sj / (3.14159265f * radius * radius); // gaussians of this radius add up to a uniform pressure
		const bool upright = w == 2 && b.axis[2][2] > 0.7f;

		// bottom and top of the box on the vertical line through c + axis[i] * s + axis[j] * t
		const auto column = [&](const float axis[3][3], const float centre[3], const float s, const float t, float& low, float& high)
			{
				float z0 = -1e9f, z1 = 1e9f;
				const float base[3] = { w == 0 ? 0.0f : (i == 0 ? s : t), w == 1 ? 0.0f : (i == 1 ? s : t), w == 2 ? 0.0f : (i == 2 ? s : t) };
				for (int k = 0; k < 3; k++)
				{
					const float az = axis[k][2];
					if (std::abs(az) < 1e-4f) {
						continue;
					}

					const float a = (-b.half[k] - base[k]) / az, c = (b.half[k] - base[k]) / az;
					z0 = std::max(z0, std::min(a, c));
					z1 = std::min(z1, std::max(a, c));
				}

				const float qz = centre[2] + axis[i][2] * s + axis[j][2] * t;
				low = qz + z0;
				high = qz + z1;
				return z1 > z0;
			};

		struct wet_s { float x, y, draft, along, vh; };
		wet_s wet[256];
		int count = 0, total = 0;
		float vx_sum = 0.0f, vy_sum = 0.0f;
		const float impact = b.mass > 0.0f
			? std::clamp(std::sqrt(b.mass / (RHO * std::max(4.0f * b.half[i] * b.half[j], 0.1f) * 0.5f)), 0.25f, 1.2f)
			: 0.6f;

		for (int a = 0; a < ni; a++)
		{
			for (int c = 0; c < nj; c++)
			{
				const float s = -b.half[i] + (static_cast<float>(a) + 0.5f) * si;
				const float t = -b.half[j] + (static_cast<float>(c) + 0.5f) * sj;
				float form = 0.0f;

				if (b.hull && upright)
				{
					// axis 0 across, axis 1 forward: the bow narrows and rises, the bottom is a shallow v
					const float u = (i == 1 ? s : t) / b.half[1], v = (i == 0 ? s : t) / b.half[0];
					const float bow = clamp01((u - 0.2f) / 0.8f);
					if (std::abs(v) > 1.0f - 0.85f * bow * bow) {
						continue;
					}

					form = 2.0f * b.half[2] * (0.12f * v * v + 0.4f * bow * bow);
				}
				else if (b.rounded)
				{
					const float u = s / b.half[i], v = t / b.half[j];
					if (u * u + v * v > 1.0f) {
						continue;
					}
				}

				total++;
				float low, high;
				if (!column(b.axis, b.centre, s, t, low, high)) {
					continue;
				}

				low = std::min(low + form, high);
				const float x = b.centre[0] + b.axis[i][0] * s + b.axis[j][0] * t;
				const float y = b.centre[1] + b.axis[i][1] * s + b.axis[j][1] * t;
				const float draft = b.water - low;

				float vx = 0.0f, vy = 0.0f, vz = 0.0f, prev_draft = draft;
				if (b.has_prev && b.dt > 0.0f)
				{
					float plow, phigh;
					if (column(b.prev_axis, b.prev_centre, s, t, plow, phigh))
					{
						plow = std::min(plow + form, phigh);
						const float px = b.prev_centre[0] + b.prev_axis[i][0] * s + b.prev_axis[j][0] * t;
						const float py = b.prev_centre[1] + b.prev_axis[i][1] * s + b.prev_axis[j][1] * t;
						vx = (x - px) / b.dt; vy = (y - py) / b.dt; vz = (low - plow) / b.dt;
						prev_draft = b.water - plow;

						// teleported
						if (std::abs(vx) > 90.0f || std::abs(vy) > 90.0f || std::abs(vz) > 90.0f) {
							vx = vy = vz = 0.0f; prev_draft = draft;
						}
					}
				}

				if (draft <= 0.0f) {
					continue;
				}

				// crossed the surface on its way down: the water under it is driven down at nearly its speed
				if (prev_draft <= 0.0f && vz < -1.0f) {
					strikes.push_back({ x, y, std::max(radius, 0.5f), std::min(-vz * impact * 0.9f, 9.0f) * share * gain });
				}

				// the surface closes over what is fully under water, and flows around what reaches deeper than ~1 m
				const float over = b.water - high;
				const float held = std::min({ draft, high - low, 1.0f }) * (over > 0.0f ? std::exp(-over / 0.35f) : 1.0f);

				if (count < 256)
				{
					wet[count++] = { x, y, held, 0.0f, std::sqrt(vx * vx + vy * vy) };
					vx_sum += vx; vy_sum += vy;
				}
			}
		}

		if (!count) {
			return 0.0f;
		}

		// where each wetted column lies along the body's way through the water: 1 = leading edge, 0 = trailing edge
		const float speed = std::sqrt(vx_sum * vx_sum + vy_sum * vy_sum) / static_cast<float>(count);
		const float dirx = speed > 0.3f ? vx_sum / (speed * static_cast<float>(count)) : 0.0f;
		const float diry = speed > 0.3f ? vy_sum / (speed * static_cast<float>(count)) : 0.0f;
		float lo = 1e9f, hi = -1e9f;
		for (int p = 0; p < count; p++)
		{
			wet[p].along = wet[p].x * dirx + wet[p].y * diry;
			lo = std::min(lo, wet[p].along); hi = std::max(hi, wet[p].along);
		}

		const float wetted_length = std::max(hi - lo, cell_size);
		for (int p = 0; p < count; p++)
		{
			// the footprint is the body's real draft: a floating hull lies in the dent of its own volume, no deeper.
			// what its motion adds is scaled by the gain
			const float depth = wet[p].draft;
			float moving = 0.0f;
			if (speed > 0.3f)
			{
				// dynamic lift of the wetted bottom: a plate at incidence a holds ~ rho v^2 a, the incidence taken as the
				// column's draft over the wetted length (a planing hull rides on this, not on its displacement)
				const float v2g = wet[p].vh * wet[p].vh / G;
				moving += b.planing * v2g * wet[p].draft / wetted_length;

				// stagnation at the leading edge, suction at the trailing edge
				const float q = hi - lo > 0.1f ? (wet[p].along - lo) / (hi - lo) : 1.0f;
				const float lead = q > 0.7f ? (q - 0.7f) / 0.3f : q < 0.3f ? -0.35f * (0.3f - q) / 0.3f : 0.0f;
				moving += b.stagnation * 0.5f * v2g * std::min(wet[p].draft / cell_size, 1.0f) * lead * std::abs(lead);
			}

			press.push_back({ wet[p].x, wet[p].y, radius, (depth + moving * gain) * share });
		}

		return static_cast<float>(count) / static_cast<float>(std::max(total, 1));
	}
}
