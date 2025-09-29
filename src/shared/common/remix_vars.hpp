#pragma once

namespace shared::common
{
	class remix_vars
	{
	public:
		// enforce singleton pattern
		remix_vars(const remix_vars&) = delete;
		remix_vars& operator=(const remix_vars&) = delete;

		static remix_vars& get();

		static void initialize(std::function<bool()> is_game_paused_callback = nullptr, float* game_frametime = nullptr);
		static void initialize(bool* is_game_paused, float* game_frametime);
		static bool is_initialized() { return get().m_initialized; }

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

		static void				on_map_load(std::string map_name);
		//static void				on_sound_start(std::uint32_t hash, const std::string_view& sound_name);
		static void				on_client_frame();

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

		//static bool add_linear_interpolate_entry(option_handle handle, const option_value& goal, float duration, const std::string& remix_var_name = "");
		//static bool add_smooth_interpolate_entry(option_handle handle, const option_value& goal, float duration, const std::string& remix_var_name = "");
		//static bool add_progressive_interpolate_entry(option_handle handle, const option_value& goal, float speed, const std::string& remix_var_name = "");

		bool add_interpolate_entry(const std::uint64_t& identifier, option_handle handle, const option_value& goal, float duration, float delay, float delay_transition_back, EASE_TYPE ease, const std::string& remix_var_name = "");

		static bool is_paused()
		{
			if (get().is_initialized()) 
			{
				if (get().m_is_paused_callback) {
					return get().m_is_paused_callback();
				}

				return *get().m_is_game_paused_ptr;
			}

			return get().m_is_game_paused_internal;
		}

		static float get_frametime()
		{
			if (get().is_initialized()) {
				return *get().m_frametime_ptr;
			}

			return get().m_frametime_internal;
		}

	private:
		remix_vars() : m_initialized(false) {}
		bool m_initialized;

		std::function<bool()> m_is_paused_callback;
		bool* m_is_game_paused_ptr = nullptr;
		bool m_is_game_paused_internal = false;

		float* m_frametime_ptr = nullptr;
		float m_frametime_internal = 16.0f;
	};
}
