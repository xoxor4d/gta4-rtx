#include "std_include.hpp"
#include "timecycle.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "natives.hpp"
#include "remix_vars.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	// -----
	// Game values for RemixFX effects, written with SetGameValue (an effect binds a parameter with 'param.<id>.gamevalue').
	// What the game hands Fusion Rain Droplets, for remix-shaders/lens_drops.remixfx.slang, which is a port of it:
	//   gta4.lens.rain:     the weather's rain on the frames the game asks for raindrops on the camera, 0 on the others
	//   gta4.lens.blood:    blood on the camera, event counter (+1 per splatter; the effect turns one into its burst)
	//   gta4.lens.splash:   water on the camera, a running count of drops: 20 per splash of the camera, and the splashes
	//                       of the water within 6 m of the camera, fewer the further off (Fusion Rain Droplets' StartEffect)
	//   gta4.lens.spray:    a spray the camera stands in (the Middle Park fountain), the amount a frame, 0 outside of it
	//   gta4.lens.timestep: the game's own frame time in seconds, 0 while it is paused
	// Counters, not pulses, so an effect never misses an event that happened between two of its frames.

	namespace lens_fx
	{
		float* weather_rain = nullptr;
		float* timestep = nullptr;
		std::uintptr_t camera_matrix = 0u; // right, up, at, pos rows of 16 bytes
		std::atomic<float> rain_request = 0.0f;
		std::atomic<bool> rain_requested = false;
		std::atomic<std::uint32_t> blood_events = 0u;
		std::atomic<std::uint32_t> splash_drops = 0u;

		// the splashes of the water the game starts, landed on the lens with the next frame
		constexpr float SPLASH_RADIUS = 6.0f;
		std::mutex splash_mutex;
		std::vector<std::pair<Vector, float>> splashes;

		// Fusion Rain Droplets' DropsOfAmount of a splash: an amount of 1 is 20 drops
		void add_splash_amount(const float amount)
		{
			if (amount > 0.0f) {
				splash_drops.fetch_add(static_cast<std::uint32_t>(amount * 20.0f));
			}
		}

		// the game's camera effect requests (raindrops / blood / water on the lens), flags it sets during the frame;
		// replaces the function that consumes them (the game's own lens effects are not drawn under Remix anyway)
		void __fastcall on_camera_fx_requests(std::uint8_t* self, [[maybe_unused]] void* edx)
		{
			if (self[0]) // blood_cam
			{
				blood_events.fetch_add(1u);
				self[0] = 0u;
			}

			if (self[12]) // rain_drops
			{
				rain_request.store(weather_rain ? *weather_rain : 1.0f);
				rain_requested.store(true);
				self[12] = 0u;
			}

			if (self[28]) // water_splash_cam
			{
				add_splash_amount(1.0f);
				self[28] = 0u;
			}
		}

		// the effects the game starts: the splashes of everything that goes through the water near the camera
		typedef void(__fastcall* start_effect_t)(std::uint8_t* effect, void* edx);
		start_effect_t start_effect_og = nullptr;

		void __fastcall start_effect_hk(std::uint8_t* effect, void* edx)
		{
			// the game moves an effect that is attached to something to its place as it starts it
			start_effect_og(effect, edx);

			if (const auto rule = *reinterpret_cast<std::uint8_t**>(effect + 0x78); rule)
			{
				if (const auto name = *reinterpret_cast<const char**>(rule + 0xF8); name)
				{
					const std::string_view effect_name(name);
					if (effect_name.starts_with("water_splash_") && effect_name != "water_splash_cam")
					{
						// a limb that goes through the surface splashes less than a body or a car that falls in
						const bool limb = effect_name == "water_splash_ped_in" || effect_name == "water_splash_ped_out" || effect_name == "water_splash_ped_wade";
						const auto p = reinterpret_cast<float*>(effect + 0x110);
						std::lock_guard lock(splash_mutex);
						splashes.emplace_back(Vector(p[0], p[1], p[2]), limb ? 0.5f : 2.0f);
					}
				}
			}
		}

		bool camera_position(Vector& out)
		{
			if (!camera_matrix) {
				return false;
			}
			const auto p = reinterpret_cast<const float*>(camera_matrix + 0x30);
			out = Vector(p[0], p[1], p[2]);
			return true;
		}

		/// once a frame: the splashes of the frame before, by how far from the camera they are
		void process_splashes()
		{
			static std::vector<std::pair<Vector, float>> pending;
			{
				std::lock_guard lock(splash_mutex);
				pending.swap(splashes);
			}

			Vector cam;
			if (camera_position(cam))
			{
				for (const auto& [point, amount] : pending)
				{
					const float len = sqrtf(cam.DistToSqr(point));
					if (len < SPLASH_RADIUS) {
						add_splash_amount(amount * (1.0f - len / SPLASH_RADIUS));
					}
				}
			}
			pending.clear();
		}

		/// the spray the camera stands in, the amount a frame (Fusion Rain Droplets' global emitters)
		float spray_now()
		{
			Vector cam;
			if (!camera_position(cam)) {
				return 0.0f;
			}

			const Vector fountain(-234.365f, 769.001f, 6.83f); // middle park fountain
			return cam.DistToSqr(fountain) <= 50.0f ? 10.0f : 0.0f;
		}

		/// the rain of this frame: the weather's rain if the game asked for drops on the camera since the last frame
		float rain_now()
		{
			const bool requested = rain_requested.exchange(false);
			return requested ? rain_request.load() : 0.0f;
		}

		float timestep_now()
		{
			return timestep ? *timestep : -1.0f;
		}

		void install()
		{
			const auto fx_requests = shared::utils::mem::find_pattern("56 8B F1 80 3E 00 74 08", 0, "lens_fx: camera fx requests", true, 0u);
			if (!fx_requests)
			{
				shared::common::log("Timecycle", "lens fx: camera fx requests not found, RemixFX lens game values stay 0", shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
				return;
			}

			// another plugin (Fusion Rain Droplets) already took the function over: leave it to that one
			if (*reinterpret_cast<std::uint8_t*>(fx_requests) == 0xE9)
			{
				shared::common::log("Timecycle", "lens fx: camera fx requests already hooked by another plugin, RemixFX lens game values stay 0", shared::common::LOG_TYPE::LOG_TYPE_WARN, false);
				return;
			}

			if (const auto rain = shared::utils::mem::find_pattern("F3 0F 11 05 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? F3 0F 11 0D", 4, "lens_fx: weather rain", true, 0u); rain) {
				weather_rain = *reinterpret_cast<float**>(rain);
			}
			else if (const auto rain2 = shared::utils::mem::find_pattern("F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 84 C0 74 15 E8 ? ? ? ? 84 C0", 4, "lens_fx: weather rain (alt)", true, 0u); rain2) {
				weather_rain = *reinterpret_cast<float**>(rain2);
			}

			// the game only asks for raindrops on the camera while it looks up: ask regardless
			if (const auto look_up = shared::utils::mem::find_pattern("76 15 B3 01", 0, "lens_fx: rain only looking up", true, 0u); look_up) {
				shared::utils::hook::nop(look_up, 2);
			}
			else if (const auto look_up2 = shared::utils::mem::find_pattern("74 16 33 C0 80 7C 06", 0, "lens_fx: rain only looking up (alt)", true, 0u); look_up2)
			{
				shared::utils::hook::nop(look_up2 - 9u, 3);
				shared::utils::hook::set(look_up2 - 9u, 0xB0, 0x01); // mov al, 1
			}

			// the camera, the way Fusion Rain Droplets reads it
			if (const auto cam = shared::utils::mem::find_pattern("B9 ? ? ? ? E8 ? ? ? ? 84 C0 75 13 6A 01 6A 01 68 F4 01 00 00 B9 ? ? ? ? E8 ? ? ? ? 57", 1, "lens_fx: camera matrix", true, 0u); cam) {
				camera_matrix = *reinterpret_cast<std::uint32_t*>(cam);
			}
			else if (const auto cam2 = shared::utils::mem::find_pattern("7A 21 B9", 3, "lens_fx: camera matrix (alt)", true, 0u); cam2) {
				camera_matrix = *reinterpret_cast<std::uint32_t*>(cam2);
			}

			// the game's frame time
			if (const auto ts = shared::utils::mem::find_pattern("D8 0D ? ? ? ? 83 C0 30", 0, "lens_fx: timestep", true, 0u); ts) {
				timestep = *reinterpret_cast<float**>(ts - 9u);
			}
			else if (const auto ts2 = shared::utils::mem::find_pattern("F3 0F 10 05 ? ? ? ? 8B 02 51", 4, "lens_fx: timestep (alt)", true, 0u); ts2) {
				timestep = *reinterpret_cast<float**>(ts2);
			}

			if (const auto start_effect = shared::utils::mem::find_pattern("56 8B F1 83 7E 10 00 8D 46 10 75 06 83 78 08 00 74 0A 56 50 8B 40 0C FF D0 83 C4 08", 0, "lens_fx: start effect", true, 0u); start_effect) {
				shared::utils::hook::detour(start_effect, &start_effect_hk, reinterpret_cast<void**>(&start_effect_og));
			}

			shared::utils::hook(fx_requests, on_camera_fx_requests, HOOK_JUMP).install()->quick();
			shared::common::log("Timecycle", "lens fx: RemixFX lens game values active", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}
	}

	// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_FLOAT(name) \
		im->m_timecyc_curr_##name = timecycle->##name; \
		im->m_timecyc_curr_##name##_final = val.value;

			// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_VEC3(tc_name) \
		im->m_timecyc_curr_##tc_name##.x = val.vector[0]; \
		im->m_timecyc_curr_##tc_name##.y = val.vector[1]; \
		im->m_timecyc_curr_##tc_name##.z = val.vector[2];

		// for visualization of values in gui
#define ASSIGN_IMGUI_VIS_UNPACKED_COLOR(tc_name, temp_vec) \
		im->m_timecyc_curr_##tc_name = ##temp_vec; \
		im->m_timecyc_curr_##tc_name##_final.x = val.vector[0]; \
		im->m_timecyc_curr_##tc_name##_final.y = val.vector[1]; \
		im->m_timecyc_curr_##tc_name##_final.z = val.vector[2]; \
		im->m_timecyc_curr_##tc_name##_final.w = val.vector[3];

	// returns a factor between 0 and 1 based on prev and next weather
	// 0 = good weather, 1 = bad weather
	// can be used like: val.value += (max_offset * weather_factor);
	float timecycle::get_bad_weather_factor()
	{
		auto is_bad_weather = [](game::eWeatherType wt) -> bool
			{
				return	wt == game::eWeatherType::WEATHER_RAIN ||
						wt == game::eWeatherType::WEATHER_LIGHTNING ||
						wt == game::eWeatherType::WEATHER_CLOUDY;
			};

		const float transition = *game::weather_change_value; // 0.0f = fully previous, 1.0f = fully new
		float weather_factor = 0.0f;

		if (is_bad_weather(*game::weather_type_prev)) {
			weather_factor += (1.0f - transition);
		}

		if (is_bad_weather(*game::weather_type_new)) {
			weather_factor += transition;
		}

		return std::clamp(weather_factor, 0.0f, 1.0f);
	}

	timecycle::wetness_state_s timecycle::m_wetness = {};

	// wetness of the game's weather blend or the debug override
	float timecycle::get_game_wetness()
	{
		if (const auto im = imgui::get(); im && im->m_dbg_global_wetness_override) {
			return im->m_dbg_global_wetness;
		}

		if (!game::pTimeCycleWetnessChange || !game::pTimeCycleSpecularOffset) {
			return 0.0f;
		}

		// normally pTimeCycleWetness but we use wetness change
		return std::clamp(*game::pTimeCycleSpecularOffset * std::clamp(*game::pTimeCycleWetnessChange, 0.0f, 1.0f), 0.0f, 1.0f);
	}

	// follows the game wetness while it rains and holds + fades out a level derived from the rain peak once the weather clears.
	// the result never exceeds the rain peak and the game takes over again as soon as it rains or is wetter than the held level.
	// vehicles get wet within a few seconds of rain and dry up over half the fade time, starting with the hold
	void timecycle::update_wetness()
	{
		const auto gs = comp_settings::get();
		auto& w = m_wetness;

		// clamped so a single hitch does not eat the hold time
		const float dt = remix_vars::is_paused() ? 0.0f : std::min(remix_vars::get_frametime() * 0.001f, 0.25f);

		const auto is_rain_weather = [](const game::eWeatherType wt)
			{
				return wt == game::WEATHER_RAIN || wt == game::WEATHER_DRIZZLE || wt == game::WEATHER_LIGHTNING;
			};

		// the game wetness dips while it rains, the hold only starts once the weather moves away from rain
		const bool rain_ahead = game::weather_type_new && is_rain_weather(*game::weather_type_new);

		w.game = get_game_wetness();

		if (w.phase == wetness_phase::follow_game) {
			w.peak = std::max(w.peak, w.game);
		} else {
			w.timer += dt;
		}

		const float level_scalar = std::clamp(gs->timecycle_wetness_afterrain_scalar._float(), 0.0f, 1.0f);
		w.level = w.peak * level_scalar;

		if (w.phase == wetness_phase::follow_game && !rain_ahead && (w.game < w.level || w.game <= 0.0f))
		{
			w.phase = wetness_phase::after_rain;
			w.timer = 0.0f;
			w.vehicle_drying_from = w.vehicle_progress;
		}

		const float fade_duration = std::max(gs->timecycle_wetness_afterrain_fade_seconds._float(), 0.0f);

		float level_left = 1.0f;
		if (w.phase == wetness_phase::after_rain)
		{
			const float fade_time = w.timer - std::max(gs->timecycle_wetness_afterrain_hold_seconds._float(), 0.0f);

			if (fade_time > 0.0f)
			{
				const float x = fade_duration > 0.0f ? std::min(fade_time / fade_duration, 1.0f) : 1.0f;
				level_left = 1.0f - x * x * (3.0f - 2.0f * x);
			}

			// raining again: the faded peak keeps the world where it is
			if (w.game > w.level * level_left || (rain_ahead && w.game > 0.0f))
			{
				w.phase = wetness_phase::follow_game;
				w.peak = std::max(w.game, w.peak * level_left);
				w.level = w.peak * level_scalar;
				w.timer = 0.0f;
				level_left = 1.0f;
			}
		}

		w.effective = std::max(w.game, w.level * level_left);

		if (w.phase == wetness_phase::follow_game)
		{
			if (w.game > 0.0f)
			{
				const float wet_duration = std::max(gs->timecycle_wetness_vehicle_wet_seconds._float(), 0.0f);
				w.vehicle_progress = wet_duration > 0.0f ? std::min(w.vehicle_progress + dt / wet_duration, 1.0f) : 1.0f;
			}
		}
		else
		{
			const float dry_duration = fade_duration * 0.5f;
			w.vehicle_progress = dry_duration > 0.0f ? w.vehicle_drying_from * std::max(1.0f - w.timer / dry_duration, 0.0f) : 0.0f;
		}

		w.vehicle = w.vehicle_progress * w.vehicle_progress * (3.0f - 2.0f * w.vehicle_progress);

		// ripples follow the rain itself so they settle at the min value while the after-rain wetness is held
		const float ripple_min = gs->timecycle_wetness_ripple_min._float();
		const float ripple_max = gs->timecycle_wetness_ripple_max._float();
		w.ripple_strength = ripple_min + (ripple_max - ripple_min) * std::pow(w.game, std::max(gs->timecycle_wetness_ripple_curve._float(), 0.0f));
	}

	namespace
	{
		// copies of the game's timecycle params, taken on the game thread and read on the render thread
		struct timecycle_snapshot_s
		{
			game::TimeCycleParams params_01 = {};
			game::TimeCycleParams params_02 = {};
			game::TimeCycleParams params_cutscene = {};
			bool is_playing_cutscene = false;
			bool valid = false;
		};

		std::mutex g_snapshot_mtx;
		timecycle_snapshot_s g_snapshot;
	}

	void timecycle::capture_on_game_frame()
	{
		if (!game::m_pCurrentTimeCycleParams_01 || !game::m_pCurrentTimeCycleParams_02 || !game::m_pCurrentTimeCycleParams_Cutscene) {
			return;
		}

		std::lock_guard lock(g_snapshot_mtx);
		g_snapshot.params_01 = *game::m_pCurrentTimeCycleParams_01;
		g_snapshot.params_02 = *game::m_pCurrentTimeCycleParams_02;
		g_snapshot.params_cutscene = *game::m_pCurrentTimeCycleParams_Cutscene;
		g_snapshot.is_playing_cutscene = game::m_dwCutsceneState && *game::m_dwCutsceneState > 0;
		g_snapshot.valid = true;
	}

	void timecycle::translate_and_apply_timecycle_settings()
	{
		PROFILE_ZONE();
		auto unpack_uint32 = [](const uint32_t& in, float* out)
			{
				out[0] = static_cast<float>(in >> 16 & 0xFF) / 255.0f;
				out[1] = static_cast<float>(in >>  8 & 0xFF) / 255.0f;
				out[2] = static_cast<float>(in >>  0 & 0xFF) / 255.0f;
				out[3] = static_cast<float>(in >> 24 & 0xFF) / 255.0f;
			};

		auto map_range = [](const float& input, const float& in_min, const float& in_max, const float& out_min, const float& out_max)
			{
				return out_min + (out_max - out_min) * ((input - in_min) / (in_max - in_min));
			};

		{
			const auto& im = imgui::get();
			const auto& vars = remix_vars::get();
			const auto& gs = comp_settings::get();
			remix_vars::option_value val{};

			// RemixFX game values (see lens_fx): effects bind a parameter to these keys with 'param.<id>.gamevalue'.
			// Written only when they change; a runtime without RemixFX game value bindings just stores them.
			if (shared::common::remix_api::is_initialized())
			{
				const auto& bridge = shared::common::remix_api::get().m_bridge;

				static float s_sent_rain = -1.0f, s_sent_spray = -1.0f, s_sent_timestep = -2.0f;
				static std::uint32_t s_sent_blood = ~0u, s_sent_splash = ~0u;

				lens_fx::process_splashes();

				if (const float rain = lens_fx::rain_now(); std::fabs(rain - s_sent_rain) > 0.0001f)
				{
					bridge.SetGameValue("gta4.lens.rain", std::format("{:.4f}", rain).c_str());
					s_sent_rain = rain;
				}
				if (const auto blood = lens_fx::blood_events.load(); blood != s_sent_blood)
				{
					bridge.SetGameValue("gta4.lens.blood", std::to_string(blood).c_str());
					s_sent_blood = blood;
				}
				if (const auto splash = lens_fx::splash_drops.load(); splash != s_sent_splash)
				{
					bridge.SetGameValue("gta4.lens.splash", std::to_string(splash).c_str());
					s_sent_splash = splash;
				}
				if (const float spray = lens_fx::spray_now(); spray != s_sent_spray)
				{
					bridge.SetGameValue("gta4.lens.spray", std::format("{:.1f}", spray).c_str());
					s_sent_spray = spray;
				}
				if (const float ts = lens_fx::timestep_now(); std::fabs(ts - s_sent_timestep) > 0.00001f)
				{
					bridge.SetGameValue("gta4.lens.timestep", std::format("{:.6f}", ts).c_str());
					s_sent_timestep = ts;
				}
			}

			//auto first_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8910);
			//auto third_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8B20);

			// the params of the last game frame, nothing below reads the game's own structs
			timecycle_snapshot_s snapshot;
			{
				std::lock_guard lock(g_snapshot_mtx);
				snapshot = g_snapshot;
			}

			if (!snapshot.valid) {
				return;
			}

			const bool is_playing_cutscene = snapshot.is_playing_cutscene;
			const game::TimeCycleParams* timecycle = is_playing_cutscene ? &snapshot.params_cutscene : &snapshot.params_01;

			// manual override
			if (im->m_dbg_used_timecycle >= 0)
			{
				switch (im->m_dbg_used_timecycle)
				{
				default:
				case 0:
					timecycle = &snapshot.params_01;
					break;

				case 1:
					timecycle = &snapshot.params_02;
					break;

				case 2:
					timecycle = &snapshot.params_cutscene;
					break;
				}
			}

			const auto using_atmos = gs->timecycle_use_remix_atmos_system.get_as<bool>() || im->m_dbg_manual_atmos_system;

			// temp. disable certain settings when atmos is on
			gs->timecycle_fogcolor_enabled.set_temp_override_state(using_atmos);
			gs->timecycle_fogdensity_enabled.set_temp_override_state(using_atmos);
			gs->timecycle_skylight_enabled.set_temp_override_state(using_atmos);
			gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.set_temp_override_state(using_atmos);

			gs->translate_sunlight_intensity_scalar.set_temp_override_state(using_atmos);
			gs->translate_sunlight_intensity_bad_weather_influence.set_temp_override_state(using_atmos);
			gs->translate_sunlight_angular_diameter_degrees.set_temp_override_state(using_atmos);
			gs->translate_sunlight_volumetric_radiance_base.set_temp_override_state(using_atmos);
			gs->translate_moonlight_intensity_scalar.set_temp_override_state(using_atmos);

			// no need to process these when we are not in the game yet
			if (!game::is_in_game) {
				return;
			}

			// ----------------

			update_wetness();

			static auto rtxWetRippleStrength = vars->get_option("rtx.wet.rippleStrength", "0.08");
			if (gs->timecycle_wetness_enabled._bool() && gs->timecycle_wetness_ripple_enabled._bool() && rtxWetRippleStrength)
			{
				val.value = m_wetness.ripple_strength;
				vars->add_queue_entry(rtxWetRippleStrength, val, 0.0f, false, 0.0005f); // the ripple range is smaller than the default epsilon
			}


			static auto rtxSkybrightness = vars->get_option("rtx.skyBrightness", "0.1");
			if (!using_atmos && gs->timecycle_skylight_enabled.get_as<bool>() && rtxSkybrightness)
			{
				val.value = timecycle->mSkyLightMultiplier * gs->timecycle_skylight_scalar.get_as<float>();

				// bad weather
				auto weather_factor = get_bad_weather_factor();
				if (game::m_game_clock_hours && game::m_game_clock_minutes)
				{
					const int& hour = *game::m_game_clock_hours;
					const int& minute = *game::m_game_clock_minutes;
					
					if (hour >= 21 || hour < 6) {
						weather_factor = 0.0f; // no skylight increase at night
					}
					else if (hour >= 20)
					{
						// fade from: 20:00-21:00
						const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 20:00, 1.0 at 21:00
						weather_factor = 1.0f - fade_progress;
					}
					else if (hour == 6)
					{
						// fade from: 06:00-07:00
						const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 06:00, 1.0 at 07:00
						weather_factor = fade_progress * weather_factor;
					}
				}

				val.value += (gs->timecycle_skylight_max_offset_bad_weather._float() * weather_factor);
				vars->add_queue_entry(rtxSkybrightness, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyLightMultiplier);
			}


			static auto rtxBloomBurnIntensity = vars->get_option("rtx.bloom.burnIntensity", "0");
			static auto rtxBloomLuminanceThreshold = vars->get_option("rtx.bloom.luminanceThreshold", "0");
			if (gs->timecycle_bloom_enabled.get_as<bool>() && rtxBloomBurnIntensity && rtxBloomLuminanceThreshold)
			{
				float night_factor = 0.0f;
				const bool can_clamp = !is_playing_cutscene && game::m_game_clock_hours && game::m_game_clock_minutes;
				if (can_clamp)
				{
					const int& hour = *game::m_game_clock_hours;
					const int& minute = *game::m_game_clock_minutes;

					if (hour >= 20 || hour < 5) {
						night_factor = 1.0f;
					} else if (hour == 19) {
						night_factor = static_cast<float>(minute) / 60.0f;
					} else if (hour == 5) {
						night_factor = 1.0f - static_cast<float>(minute) / 60.0f;
					}
				}

				const bool clamp_night = can_clamp && gs->timecycle_bloom_night_min_clamp_enabled._bool();
				const bool clamp_day = can_clamp && gs->timecycle_bloom_day_min_clamp_enabled._bool();
				const float day_factor = 1.0f - night_factor;

				val.value = timecycle->mBloomIntensity * gs->timecycle_bloomintensity_scalar.get_as<float>();
				
				if (clamp_night) {
					val.value = std::max(gs->timecycle_bloom_night_min_clamp_value._float() * night_factor, val.value);
				}

				if (clamp_day) {
					val.value = std::max(gs->timecycle_bloom_day_min_clamp_value._float() * day_factor, val.value);
				}

				vars->add_queue_entry(rtxBloomBurnIntensity, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomIntensity);

				// -----

				val.value = timecycle->mBloomThreshold * gs->timecycle_bloomthreshold_scalar.get_as<float>();
				
				if (clamp_night) {
					val.value = std::max(gs->timecycle_bloomthreshold_night_min_clamp_value._float() * night_factor, val.value);
				}

				if (clamp_day) {
					val.value = std::max(gs->timecycle_bloomthreshold_day_min_clamp_value._float() * day_factor, val.value);
				}

				vars->add_queue_entry(rtxBloomLuminanceThreshold, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomThreshold);
			}


			static auto rtxTonemapColorBalance = vars->get_option("rtx.tonemap.colorBalance", "1 1 1");
			if (gs->timecycle_colorcorrection_enabled.get_as<bool>() && rtxTonemapColorBalance)
			{
				Vector temp_color_offset;
				if (gs->timecycle_colortemp_enabled.get_as<bool>())
				{
					const float nrml_temp = std::clamp(gs->timecycle_colortemp_value.get_as<float>() /*timecycle->mTemperature*/ / 15.0f, -1.0f, 1.0f);
					temp_color_offset.x = nrml_temp * 0.3f;
					temp_color_offset.y = 0.0f;
					temp_color_offset.z = -nrml_temp * 0.3f;
					temp_color_offset *= gs->timecycle_colortemp_influence.get_as<float>();
					//im->m_timecyc_curr_mTemperature = timecycle->mTemperature;
					im->m_timecyc_curr_mTemperature_offset = temp_color_offset;
				}

				Vector4D color_correction_og;
				unpack_uint32(timecycle->mColorCorrection, &color_correction_og.x);

				Vector4D color_correction(color_correction_og);

				const auto& cc_influence = gs->timecycle_colorcorrection_influence.get_as<float>();
				color_correction.x *= cc_influence;
				color_correction.y *= cc_influence;
				color_correction.z *= cc_influence;

				val.vector[0] = color_correction.x + temp_color_offset.x;
				val.vector[1] = color_correction.y + temp_color_offset.y;
				val.vector[2] = color_correction.z + temp_color_offset.z;
				vars->add_queue_entry(rtxTonemapColorBalance, val);
				ASSIGN_IMGUI_VIS_UNPACKED_COLOR(mColorCorrection, color_correction_og);
			}


			static auto rtxTonemapSaturation = vars->get_option("rtx.tonemap.saturation", "1");
			if (gs->timecycle_desaturation_enabled.get_as<bool>() && rtxTonemapSaturation)
			{
				const float far_desaturation_influence = gs->timecycle_fardesaturation_influence.get_as<float>() * map_range(timecycle->mDesaturationFar, 0.0f, 1.0f, 0.0f, 0.4f);
				val.value = 1.0f - ((1.0f - timecycle->mDesaturation) * gs->timecycle_desaturation_influence.get_as<float>());
				val.value -= far_desaturation_influence;
				vars->add_queue_entry(rtxTonemapSaturation, val);
				ASSIGN_IMGUI_VIS_FLOAT(mDesaturation);
				im->m_timecyc_curr_mDesaturationFar = timecycle->mDesaturationFar;
				im->m_timecyc_curr_mDesaturationFar_offset = far_desaturation_influence;
			}


			static auto rtxTonemapExposureBias = vars->get_option("rtx.tonemap.exposureBias", "0");
			if (gs->timecycle_gamma_enabled.get_as<bool>() && rtxTonemapExposureBias)
			{
				val.value = -(1.0f - timecycle->mGamma) + gs->timecycle_gamma_offset.get_as<float>();
				vars->add_queue_entry(rtxTonemapExposureBias, val);
				ASSIGN_IMGUI_VIS_FLOAT(mGamma);
			}


			{
				//val.value = log2(game::m_pCurrentTimeCycleParams->mLumMin * 0.01f) + 4.0f; // +6?m_dbg_timecyc_skylight_scalar
				//vars->set_option(vars->get_option("rtx.autoExposure.evMinValue"), val);

				//val.value = log2(game::m_pCurrentTimeCycleParams->mLumMax * 0.01f) + 4.0f; // +4?
				//vars->set_option(vars->get_option("rtx.autoExposure.evMaxValue"), val);
			}


			Vector4D fog_color_density;
			unpack_uint32(timecycle->mSkyBottomColorFogDensity, &fog_color_density.x);
			game::helper_timecycle_current_fog_density = fog_color_density.w; // global

			// vis.
			im->m_timecyc_curr_mSkyBottomColorFogDensity.x = val.vector[0];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.y = val.vector[1];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.z = val.vector[2];
			im->m_timecyc_curr_mSkyBottomColorFogDensity.w = val.vector[3];

			static auto rtxVolumetricsSingleScatteringAlbedo = vars->get_option("rtx.volumetrics.singleScatteringAlbedo", "1 1 1");
			if (!using_atmos && gs->timecycle_fogcolor_enabled.get_as<bool>() && rtxVolumetricsSingleScatteringAlbedo)
			{
				const auto& base_strength = gs->timecycle_fogcolor_base_strength.get_as<float>();
				const auto& influence = gs->timecycle_fogcolor_influence_scalar.get_as<float>();

				Vector t = fog_color_density * influence;
				t += base_strength;

				t.Normalize();

				val.vector[0] = t.x;
				val.vector[1] = t.y;
				val.vector[2] = t.z;

				//val.vector[0] = base_strength + fog_color_density.x * influence;
				//val.vector[1] = base_strength + fog_color_density.y * influence;
				//val.vector[2] = base_strength + fog_color_density.z * influence;

				vars->add_queue_entry(rtxVolumetricsSingleScatteringAlbedo, val);
				ASSIGN_IMGUI_VIS_VEC3(singleScatteringAlbedo);
			}

			float atmos_height = 0.0f;
			static auto rtxVolumetricsAtmosphereHeightMeters = vars->get_option("rtx.volumetrics.atmosphereHeightMeters", "1000");
			if (gs->timecycle_skyhorizonheight_enabled.get_as<bool>() && rtxVolumetricsAtmosphereHeightMeters)
			{
				// Base atmosphere height from timecycle
				atmos_height = timecycle->mSkyHorizonHeight * 100.0f * gs->timecycle_skyhorizonheight_scalar.get_as<float>();

				// Add camera height influence with smooth non-linear curve
				const auto vp = game::pViewports;
				if (vp && vp->sceneviewport)
				{
					const float cam_height = vp->sceneviewport->cameraInv.m[3][2];
					const float threshold = gs->timecycle_skyhorizonheight_cam_height_threshold.get_as<float>();
					const float influence_low = gs->timecycle_skyhorizonheight_cam_height_influence_low.get_as<float>();
					const float influence_high = gs->timecycle_skyhorizonheight_cam_height_influence_high.get_as<float>();

					float cam_height_contribution = 0.0f;

					if (cam_height > 0.0f && threshold > 0.0f)
					{
						if (cam_height <= threshold)
						{
							// Below/at threshold: linear scaling (minimal impact per meter)
							cam_height_contribution = influence_low * cam_height;
						}
						else
						{
							// Above threshold: contribution from below threshold + linear scaled excess height
							// The curve is smooth at threshold: both formulas give the same value at threshold
							const float excess_height = cam_height - threshold;
							
							// Contribution up to threshold + linear scaling of excess height
							const float base_contribution = influence_low * threshold;
							const float excess_contribution = influence_high * excess_height;
							
							cam_height_contribution = base_contribution + excess_contribution;
						}

						atmos_height += cam_height_contribution;
					}
				}

				val.value = atmos_height;
				vars->add_queue_entry(rtxVolumetricsAtmosphereHeightMeters, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyHorizonHeight);
			}

			static auto rtxVolumetricsTransmittanceMeasurementDistanceMeters = vars->get_option("rtx.volumetrics.transmittanceMeasurementDistanceMeters", "200");
			if (!using_atmos && gs->timecycle_fogdensity_enabled.get_as<bool>() && rtxVolumetricsTransmittanceMeasurementDistanceMeters)
			{
				val.value = map_range(fog_color_density.w, 0.0f, 0.9f, 200.0f, 0.6f)
					* gs->timecycle_fogdensity_influence_scalar.get_as<float>()
					+ map_range(atmos_height, 0.0f, 1000.0f,
						gs->timecycle_skyhorizonheight_low_transmittance_offset.get_as<float>(),
						gs->timecycle_skyhorizonheight_high_transmittance_offset.get_as<float>());

				if (val.value < 0.6f) {
					val.value = 0.6f;
				}

				vars->add_queue_entry(rtxVolumetricsTransmittanceMeasurementDistanceMeters, val);
				im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters = val.value;
			}

			if (using_atmos && !im->m_dbg_manual_atmos_system)
			{
				const auto weather_type_to_preset = [](game::eWeatherType wt) -> const char*
				{
					switch (wt)
					{
					default:
					case game::WEATHER_EXTRASUNNY:  return "clear";
					case game::WEATHER_SUNNY:       return "smoggy";
					case game::WEATHER_SUNNY_WINDY: return "partlyCloudy";
					case game::WEATHER_CLOUDY:      return "overcast";
					case game::WEATHER_RAIN:        return "rainstorm";
					case game::WEATHER_DRIZZLE:     return "drizzle";
					case game::WEATHER_FOGGY:       return "foggy";
					case game::WEATHER_LIGHTNING:   return "thunderstorm";
					}
				};

				const char* const game_prev = weather_type_to_preset(*game::weather_type_prev);
				const char* const game_new  = weather_type_to_preset(*game::weather_type_new);

				// reading and writing the runtime's weather blocks until the runtime has processed everything queued before
				// the call, so the weather is only compared when the game's changes and every few hundred frames, and
				// values are only written when they differ from the last write
				constexpr std::uint32_t WEATHER_CHECK_INTERVAL_FRAMES = 300u;
				constexpr std::uint32_t WEATHER_MISMATCH_CHECK_INTERVAL_FRAMES = 30u;

				static game::eWeatherType s_checked_prev = static_cast<game::eWeatherType>(~0u);
				static game::eWeatherType s_checked_new = static_cast<game::eWeatherType>(~0u);
				static std::uint32_t s_frames_until_check = 0u;
				static bool s_mismatch = true;
				static std::string s_sent_target, s_sent_previous_target;
				static float s_sent_blend = -1.0f;
				constexpr float BLEND_WRITE_EPSILON = 0.005f; // the blend runs 0..1 over a weather transition

				const auto& bridge = shared::common::remix_api::get().m_bridge;

				if (!s_frames_until_check || s_checked_prev != *game::weather_type_prev || s_checked_new != *game::weather_type_new)
				{
					char remix_prev[256] = {};
					char remix_target[256] = {};
					uint32_t str_size = 0u;

					bridge.GetGameValue("__weather.previous", remix_prev, sizeof(remix_prev), &str_size);
					bridge.GetGameValue("__weather.target", remix_target, sizeof(remix_target), &str_size);

					const std::string_view prev = remix_prev;
					s_mismatch = prev.empty() || prev == "(initial)" || prev != game_prev || std::string_view(remix_target) != game_new;

					s_checked_prev = *game::weather_type_prev;
					s_checked_new = *game::weather_type_new;
					s_frames_until_check = s_mismatch ? WEATHER_MISMATCH_CHECK_INTERVAL_FRAMES : WEATHER_CHECK_INTERVAL_FRAMES;

					// written again below, in case something else (dev menu) changed them in the runtime
					s_sent_target.clear();
					s_sent_blend = -1.0f;
					s_sent_previous_target.clear();
				}

				s_frames_until_check--;

				const auto set_if_changed = [&bridge](const char* key, std::string& last_sent, std::string value)
				{
					if (value != last_sent)
					{
						bridge.SetGameValue(key, value.c_str());
						last_sent = std::move(value);
					}
				};

				set_if_changed("__weather.target", s_sent_target, game_new);
				if (const float blend = *game::weather_change_value; std::fabs(blend - s_sent_blend) > BLEND_WRITE_EPSILON)
				{
					bridge.SetGameValue("__weather.blend_absolute", std::format("{:.4f}", blend).c_str());
					s_sent_blend = blend;
				}
				set_if_changed("__weather.previous_target", s_sent_previous_target, s_mismatch ? game_prev : "");
			}
		}
	}

	timecycle::timecycle()
	{
		p_this = this;
		lens_fx::install();

		// -----
		m_initialized = true;
		shared::common::log("Timecycle", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
