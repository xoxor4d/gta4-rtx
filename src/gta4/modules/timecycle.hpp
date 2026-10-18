#pragma once

namespace gta4
{
	class timecycle final : public shared::common::loader::component_module
	{
	public:
		timecycle();

		static inline timecycle* p_this = nullptr;
		static timecycle* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		enum class wetness_phase : std::uint8_t
		{
			follow_game,
			after_rain,
		};

		struct wetness_state_s
		{
			wetness_phase phase = wetness_phase::follow_game;
			float game = 0.0f;
			float peak = 0.0f; // highest game wetness since it last started to rain
			float level = 0.0f; // after-rain level derived from peak
			float timer = 0.0f; // seconds spent in after_rain
			float effective = 0.0f;
			float ripple_strength = 0.0f;
		};

		static float	get_bad_weather_factor();
		static float	get_game_wetness();
		static float	get_wetness() { return m_wetness.effective; }
		static const wetness_state_s& get_wetness_state() { return m_wetness; }
		static void		reset_wetness() { m_wetness = {}; }
		static void		translate_and_apply_timecycle_settings();

	private:
		static void		update_wetness();
		static wetness_state_s m_wetness;

		bool m_initialized = false;
	};
}
