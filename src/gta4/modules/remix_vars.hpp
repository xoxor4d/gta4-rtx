#pragma once

namespace gta4
{
	class remix_vars final : public shared::common::loader::component_module
	{
	public:
		remix_vars();

		static inline remix_vars* p_this = nullptr;
		static remix_vars* get() { return p_this; }

		void initialize(std::function<bool()> is_game_paused_callback = nullptr, float* game_frametime = nullptr);
		void initialize(bool* is_game_paused, float* game_frametime);

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static void xo_vars_parse_options_fn();

		static constexpr const char* EASE_TYPE_STR[] =
		{
			"Linear",
			"SinIn",
			"SinOut",
			"SinInOut",
			"CubicIn",
			"CubicOut",
			"CubicInOut",
			"ExpoIn",
			"ExpoOut",
			"ExpoInOut",
		};

		enum EASE_TYPE : std::uint8_t
		{
			EASE_TYPE_LINEAR,
			EASE_TYPE_SIN_IN,
			EASE_TYPE_SIN_OUT,
			EASE_TYPE_SIN_INOUT,
			EASE_TYPE_CUBIC_IN,
			EASE_TYPE_CUBIC_OUT,
			EASE_TYPE_CUBIC_INOUT,
			EASE_TYPE_EXPO_IN,
			EASE_TYPE_EXPO_OUT,
			EASE_TYPE_EXPO_INOUT,
		};

		enum OPTION_TYPE : uint8_t
		{
			OPTION_TYPE_BOOL,
			OPTION_TYPE_INT,
			OPTION_TYPE_FLOAT,
			OPTION_TYPE_VEC2,
			OPTION_TYPE_VEC3,
			OPTION_TYPE_NONE,
		};

		union option_value
		{
			bool enabled;
			int integer;
			float value;
			float vector[4];

			// return true if option_values match
			bool compare(const OPTION_TYPE& type, const option_value& other, float eps = 1e-6f) const
			{
				switch (type)
				{
				case OPTION_TYPE_BOOL:    return enabled == other.enabled;
				case OPTION_TYPE_INT:     return integer == other.integer;
				case OPTION_TYPE_FLOAT:   return std::abs(value - other.value) <= eps;
				case OPTION_TYPE_VEC2:
				case OPTION_TYPE_VEC3:
					for (int i = 0; i < 3; ++i) 
					{
						if (std::abs(vector[i] - other.vector[i]) > eps) {
							return false;
						}
					}
					return true;
				default:
					return false;
				}
			}
		};

		struct option_s
		{
			
			option_s(const OPTION_TYPE& _type, const option_value& _current)
			{
				current = _current;
				reset = current;
				reset_level = current;
				type = _type;
				not_a_remix_var = false;
				modified = false;
			}

			option_s()
			{
				current = { false };
				reset = current;
				reset_level = current;
				type = OPTION_TYPE_NONE;
				not_a_remix_var = false;
				modified = false;
			}

			option_value current;
			option_value reset;
			option_value reset_level;
			OPTION_TYPE type;
			bool not_a_remix_var;
			bool modified;
		};

		typedef std::pair<const std::string, option_s>* option_handle;
		static inline std::unordered_map<std::string, option_s> options;
		static inline std::unordered_map<std::string, option_s> custom_options;

		static option_handle	add_custom_option(const std::string& name, const option_s& o);
		static option_handle	get_custom_option(const char* o);
		static option_handle	get_custom_option(const std::string& o);

		static option_handle	get_option(const char*);
		static option_handle	get_option(const std::string& o);
		static bool				set_option(option_handle o, const option_value& v, bool is_level_setting = false);
		static bool				reset_option(option_handle o, bool reset_to_level_state = false);
		static void				reset_all_modified(bool reset_to_level_state = false);
		static option_value		string_to_option_value(OPTION_TYPE type, const std::string& str);
		static option_s			string_to_option(const std::string& str);
		static void				parse_rtx_options();
		static void				parse_and_apply_conf_with_lerp(const std::string& conf_name, const std::uint64_t& identifier, const EASE_TYPE ease, float duration, float delay = 0.0f, float delay_transition_back = 0.0f);

		//static void				on_map_load(std::string map_name);
		static void				on_client_frame();
		static void				translate_and_apply_timecycle_settings();

		struct interpolate_entry_s
		{
			std::uint64_t identifier;
			option_handle option;
			option_value start;
			option_value goal;
			OPTION_TYPE type;
			EASE_TYPE style;
			float time_duration;
			float time_delay_transition_back;
			float _time_elapsed;
			bool _in_backwards_transition;
			bool _complete;
		};

		static inline std::vector<interpolate_entry_s> interpolate_stack;
		bool add_interpolate_entry(const std::uint64_t& identifier, option_handle handle, const option_value& goal, float duration, float delay, float delay_transition_back, EASE_TYPE ease, const std::string& remix_var_name = "");

		static bool is_paused()
		{
			if (get()->is_initialized()) 
			{
				if (get()->m_is_paused_callback) {
					return get()->m_is_paused_callback();
				}

				return *get()->m_is_game_paused_ptr;
			}

			return false;
		}

		static float get_frametime()
		{
			if (get()->is_initialized() && get()->m_frametime_ptr) {
				return *get()->m_frametime_ptr;
			}

			return shared::globals::frame_time_ms;
		}

	private:
		bool m_initialized = false;

		std::function<bool()> m_is_paused_callback;
		bool* m_is_game_paused_ptr = nullptr;
		float* m_frametime_ptr = nullptr;
	};
}
