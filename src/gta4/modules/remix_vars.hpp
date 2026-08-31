#pragma once
#include <shared_mutex>

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
				last_queue_add_frame = 0;
			}

			option_s()
			{
				current = { false };
				reset = current;
				reset_level = current;
				type = OPTION_TYPE_NONE;
				not_a_remix_var = false;
				modified = false;
				last_queue_add_frame = 0;
			}

			option_value current;
			option_value reset;
			option_value reset_level;
			OPTION_TYPE type;
			bool not_a_remix_var;
			bool modified;
			std::uint32_t last_queue_add_frame = 0;
		};

		typedef std::pair<const std::string, option_s>* option_handle;
		std::unordered_map<std::string, option_s> options;
		std::unordered_map<std::string, option_s> custom_options;
		mutable std::shared_mutex mutex_;

		static option_handle	add_custom_option(const std::string& name, const option_s& o);
		static option_handle	get_custom_option(const char* o);
		static option_handle	get_custom_option(const std::string& o);

		static option_handle	get_option(const char*);
		static option_handle	get_option(const std::string& o);
		static bool				set_option(option_handle o, const option_value& v, bool is_level_setting = false, bool for_user_layer = false);
		static bool				reset_option(option_handle o, bool reset_to_level_state = false);
		static void				reset_all_modified(bool reset_to_level_state = false);
		static option_value		string_to_option_value(OPTION_TYPE type, const std::string& str);
		static option_s			string_to_option(const std::string& str);
		static void				parse_rtx_options();
		static void				parse_and_apply_conf(const std::string& sub_dir, const std::string& file_name,float delay = 0.0f);

		//static void			reset(std::string map_name);
		static void				init_once_on_init();
		static bool				init_once_on_ingame_frame();
		static void				on_client_frame();

		// delayed option set - multiple entries for the same option are allowed
		struct queue_entry_s
		{
			option_handle option = nullptr;
			option_value goal = {};
			OPTION_TYPE type = OPTION_TYPE_NONE;
			float _time_elapsed = 0.0f;
			bool always = false;
			float epsilon = 0.01f;
			bool _complete = false;
		};

		static inline std::vector<queue_entry_s> var_queue;
		bool add_queue_entry(option_handle handle, const option_value& goal, float delay = 0.0f, bool always = false, float epsilon = 0.01f, const std::string& remix_var_name = "");

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
		bool m_init_once_on_ingame_frame = false;
		bool m_init_once_on_init = false;

		std::function<bool()> m_is_paused_callback;
		bool* m_is_game_paused_ptr = nullptr;
		float* m_frametime_ptr = nullptr;
		std::uint32_t m_option_set_frame = 1;
	};
}
