#include "std_include.hpp"
#include "remix_vars.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	// use a callback function for paused instead of a bool pointer
	void remix_vars::initialize(std::function<bool()> is_game_paused_callback, float* game_frametime)
	{
		if (!m_initialized)
		{
			if (is_game_paused_callback) {
				m_is_paused_callback = is_game_paused_callback;
			}

			if (game_frametime) {
				m_frametime_ptr = game_frametime;
			}

			parse_rtx_options();
			m_initialized = true;
		}
	}

	void remix_vars::initialize(bool* is_game_paused, float* game_frametime)
	{
		if (!m_initialized)
		{
			if (is_game_paused) {
				m_is_game_paused_ptr = is_game_paused;
			}

			if (game_frametime) {
				m_frametime_ptr = game_frametime;
			}

			parse_rtx_options();
			m_initialized = true;
		}
	}

	// checks if str is made up of numbers only
	// ignores dot, comma, minus and whitespaces
	bool is_single_num_or_vector(const std::string& str)
	{
		return std::ranges::all_of(str.begin(), str.end(), [](const char c) {
			return std::isdigit(c) || c == ',' || c == '.' || c == '-' || c == ' ';
		});
	}

	remix_vars::option_handle remix_vars::add_custom_option(const std::string& name, const option_s& o)
	{
		custom_options[name] = o;

		if (const auto it = custom_options.find(name); it != custom_options.end()) {
			return &*it;
		}

		return nullptr;
	}

	remix_vars::option_handle remix_vars::get_custom_option(const char* o)
	{
		if (const auto it = custom_options.find(o); it != custom_options.end()) {
			return &*it;
		}

		return nullptr;
	}

	remix_vars::option_handle remix_vars::get_custom_option(const std::string& o)
	{
		if (const auto it = custom_options.find(o); it != custom_options.end())
		{
			return &*it;
		}

		return nullptr;
	}

	/**
	 * Gets a handle of a variable from the options map
	 * @param o		remix variable name
	 * @return		handle (pointer to std::pair)
	 */
	remix_vars::option_handle remix_vars::get_option(const char* o)
	{
		if (const auto it = options.find(o); it != options.end()) {
			return &*it;
		}

		return nullptr;
	}

	/**
	 * Gets a handle of a variable from the options map
	 * @param o		remix variable name
	 * @return		handle (pointer to std::pair)
	 */
	remix_vars::option_handle remix_vars::get_option(const std::string& o)
	{
		if (const auto it = options.find(o); it != options.end()) {
			return &*it;
		}

		return nullptr;
	}

	/**
	 * Updates the given variable within the options map and sends it of to remix via the api
	 * @param o					handle into the options map
	 * @param v					variable will be set to this value 
	 * @param is_level_setting	update the reset_level value (used if reset_option() is called with reset_to_level_state)
	 * @param always			set this option even if the last state equals the new state (value might have changed on remix side - user or programmatically)
	 * @return					true if successfull
	 */
	bool remix_vars::set_option(option_handle o, const option_value& v, const bool is_level_setting, const bool always)
	{
		if (o && shared::common::remix_api::is_initialized())
		{
			if (!always && o->second.current.compare(o->second.type, v))
			{
				if (imgui::get()->m_dbg_debug_single_frame_timecycle_remix_vars) {
					shared::common::log("RemixVars", std::format("Not setting {} because value did not change.", o->first));
				}

				return false;
			}

			o->second.current = v;

			if (is_level_setting) {
				o->second.reset_level = v;
			}

			std::string var_str;
			switch(o->second.type)
			{
			case OPTION_TYPE_BOOL:
				var_str = v.enabled ? "True" : "False";
				o->second.modified = o->second.current.enabled != o->second.reset.enabled;
				break;
			case OPTION_TYPE_INT:
				var_str = std::to_string(v.integer);
				o->second.modified = o->second.current.integer != o->second.reset.integer;
				break;
			case OPTION_TYPE_FLOAT:
				var_str = std::to_string(v.value);
				o->second.modified = o->second.current.value != o->second.reset.value;
				break;
			case OPTION_TYPE_VEC2:
				var_str = std::to_string(v.vector[0]) + ", " + std::to_string(v.vector[1]);
				o->second.modified = o->second.current.vector[0] != o->second.reset.vector[0] || o->second.current.vector[1] != o->second.reset.vector[1];
				break;
			case OPTION_TYPE_VEC3:
				var_str = std::to_string(v.vector[0]) + ", " + std::to_string(v.vector[1]) + ", " + std::to_string(v.vector[2]);
				o->second.modified = o->second.current.vector[0] != o->second.reset.vector[0] || o->second.current.vector[1] != o->second.reset.vector[1] || o->second.current.vector[2] != o->second.reset.vector[2];
				break;
			case OPTION_TYPE_NONE:
				return false;
			}

			if (!var_str.empty())
			{
				shared::common::remix_api::get().m_bridge.SetConfigVariable(o->first.c_str(), var_str.c_str());

				if (imgui::get()->m_dbg_debug_single_frame_timecycle_remix_vars) {
					shared::common::log("RemixVars", std::format("Set {} to {}", o->first, var_str));
				}
				
				return true;
			}

			//DEBUG_PRINT("[RTX-SET-OPTION] Skipping unknown option type %d of option %s \n", (uint32_t) o->second.type, o->first.c_str());
		}

		return false;
	}

	/**
	 * Resets a specified remix variable
	 * @param o						handle into the options map
	 * @param reset_to_level_state	\n
	 *								false => reset options to values stored in rtx.conf\n
	 *								true  => reset options to per level conf
	 * @return						
	 */
	bool remix_vars::reset_option(option_handle o, const bool reset_to_level_state)
	{
		if (o && shared::common::remix_api::is_initialized())
		{
			o->second.current = reset_to_level_state ? o->second.reset_level : o->second.reset;

			// should reset modified
			set_option(o, o->second.current);

			if (!o->second.modified) {
				return true;
			}

			//DEBUG_PRINT("[RTX-RESET-OPTION] Failed to reset option %s \n", o->first.c_str());
		}

		return false;
	}

	/**
	 * Resets all modified remix variables
	 * @param reset_to_level_state \n
	 *		false => reset options to values stored in rtx.conf\n
	 *		true  => reset options to per level conf
	 */
	void remix_vars::reset_all_modified(const bool reset_to_level_state)
	{
		if (shared::common::remix_api::is_initialized())
		{
			auto count = 0u;
			for (auto& o : options)
			{
				if (o.second.modified)
				{
					if (reset_option(&o, reset_to_level_state)) {
						count++;
					}
				}
			}

			//DEBUG_PRINT("[RTX-RESET-ALL-OPTIONS] Reset %d options \n", count);
		}
	}

	/**
	 * Tries to convert a string to <option_value>
	 * @param type	variable type
	 * @param str	string containing the value/s
	 * @return		returns a valid <option_value> even if conversion failed 
	 */
	remix_vars::option_value remix_vars::string_to_option_value(OPTION_TYPE type, const std::string& str)
	{
		option_value out = {};

		switch (type)
		{
		case OPTION_TYPE_NONE:
		case OPTION_TYPE_BOOL:
			out.enabled = str == "True";
			break;
		case OPTION_TYPE_INT:
			out.integer = shared::utils::try_stoi(str);
			break;
		case OPTION_TYPE_FLOAT:
			out.value = shared::utils::try_stof(str);
			break;
		case OPTION_TYPE_VEC2:
			if (const auto v = shared::utils::split(str, ','); v.size() == 2)
			{
				out.vector[0] = shared::utils::try_stof(v[0]);
				out.vector[1] = shared::utils::try_stof(v[1]);
			}
			break;
		case OPTION_TYPE_VEC3:
			if (const auto v = shared::utils::split(str, ','); v.size() == 3)
			{
				out.vector[0] = shared::utils::try_stof(v[0]);
				out.vector[1] = shared::utils::try_stof(v[1]);
				out.vector[2] = shared::utils::try_stof(v[2]);
			}
			break;
		}

		return out;
	}

	/**
	 * Tries to convert a string to <option_s>
	 * @param str	string containing the value/s
	 * @return		option_s - type NONE if conversion failed
	 */
	remix_vars::option_s remix_vars::string_to_option(const std::string& str)
	{
		option_s out = {};

		if (str == "True" || str == "False")
		{
			// is bool
			out.type = OPTION_TYPE_BOOL;
			out.current.enabled = str == "True";
		}
		else if (is_single_num_or_vector(str))
		{
			if (const auto x = shared::utils::split(str, ','); x.size() > 1)
			{
				// is vector
				out.type = OPTION_TYPE_VEC2;
				out.current.vector[0] =  shared::utils::try_stof(x[0]);
				out.current.vector[1] =  shared::utils::try_stof(x[1]);

				if (x.size() > 2)
				{
					out.type = OPTION_TYPE_VEC3;
					out.current.vector[2] = shared::utils::try_stof(x[2]);
				}
			}
			else
			{
				// is single float
				out.type = OPTION_TYPE_FLOAT; // treat everything as float
				out.current.value = shared::utils::try_stof(str);
			}
		}

		out.reset = out.current;
		out.reset_level = out.current;

		return out;
	}

	/**
	 * Parses the rtx.conf in the root directory and builds an unordered map \n
	 * with pairs made of: <variable name> (std::string) and <variable value/type/...> (option_s) 
	 */
	void remix_vars::parse_rtx_options()
	{
		std::ifstream file;
		if (shared::utils::open_file_homepath("", "rtx.conf", file))
		{
			std::string input;
			while (std::getline(file, input))
			{
				if (auto pair = shared::utils::split(input, '='); pair.size() == 2u)
				{
					 shared::utils::trim(pair[0]);
					 shared::utils::trim(pair[1]);

					if (!pair[1].starts_with("0x") && !pair[1].empty())
					{
						if (const auto o = string_to_option(pair[1]); o.type != OPTION_TYPE_NONE) {
							options[pair[0]] = o;
						}
					}
				}
			}

			file.close();
		}
	}

	/**
	 * Parses a .conf within the map_configs folder lerps to contained values
	 * @param conf_name				config name without extension
	 * @param identifier			unique identifier so one can check if it exists within the interpolate_stack
	 * @param ease					[EASE_TYPE] ease mode
	 * @param duration				duration of the transition (in seconds)
	 * @param delay					delay transition start (in seconds)
	 * @param delay_transition_back	delay between end of transition and transition back to the initial starting value (in seconds) - only active if value > 0
	 */
	void remix_vars::parse_and_apply_conf_with_lerp(const std::string& conf_name, const std::uint64_t& identifier, const EASE_TYPE ease, const float duration, const float delay, const float delay_transition_back)
	{
		std::ifstream file;
		if (shared::utils::open_file_homepath("rtx_comp\\map_configs", conf_name, file))
		{
			std::string input;
			while (std::getline(file, input))
			{
				if (shared::utils::starts_with(input, "#") || input.empty()) {
					continue;
				}

				if (auto pair = shared::utils::split(input, '=');
					pair.size() == 2u)
				{
					 shared::utils::trim(pair[0]);
					 shared::utils::trim(pair[1]);

					if (pair[1].starts_with("0x") || pair[1].empty()) {
						continue;
					}

					if (const auto o = get_option(pair[0].c_str()); o)
					{
						const auto& v = string_to_option_value(o->second.type, pair[1]);

						remix_vars::get()->add_interpolate_entry(identifier, o, v, duration, delay, delay_transition_back, ease);
						//DEBUG_PRINT("[VAR-LERP] Start lerping var: %s to: %s\n", o->first.c_str(), pair[1].c_str());
					}
				}
			}

			file.close();
		}
		else {
			shared::common::log("RemixVars", std::format("Failed to find config: {} in \"rtx_comp\\map_configs\"", conf_name.c_str()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
		}
	}


	// #
	// Interpolation

	 /**
	  * Adds a remix var (option) to the interpolation stack and linearly interpolates it
	  *	@param identifier				unique identifier so one can check if it exists within the interpolate_stack
	  * @param handle					handle of remix var option in the options map (can be nullptr if 'remix_var_name' is used instead)
	  * @param goal						transition goal
	  * @param duration					duration of the transition (in seconds)
	  * @param delay					delay transition start (seconds)
	  *	@param delay_transition_back	delay between end of transition and transition back to the initial starting value (in seconds) - only active if value > 0
	  * @param ease						[EASE_TYPE] ease mode
	  * @param remix_var_name			can be used if handle = nullptr
	  * @return
	  */
	bool remix_vars::add_interpolate_entry(const std::uint64_t& identifier, option_handle handle, const option_value& goal, const float duration, const float delay, const float delay_transition_back, EASE_TYPE ease, const std::string& remix_var_name)
	{
		option_handle h = handle;
		if (!h)
		{
			if (remix_var_name.empty()) {
				return false;
			}

			h = remix_vars::get()->get_option(remix_var_name);
		}

		if (h)
		{
			// directly apply when no duration and no delay
			if (duration == 0.0f && delay == 0.0f) {
				set_option(handle, goal);
			}
			// interpolate over time or set after delay
			else
			{
				// check if we are already interpolating the value
				bool exists = false;

				for (auto& ip : interpolate_stack)
				{
					if (ip.option == h)
					{
						// update
						ip.identifier = identifier;
						ip.start = h->second.current;
						ip.goal = goal;
						ip.style = ease;
						ip.time_duration = duration;
						ip.time_delay_transition_back = delay_transition_back;
						ip._time_elapsed = -delay;

						exists = true;
						break;
					}
				}

				if (!exists)
				{
					interpolate_stack.emplace_back(interpolate_entry_s
						{ identifier, h, h->second.current, goal, h->second.type, ease, duration, delay_transition_back, -delay });
				}
			}

			return true;
		}

		return false;
	}


	void lerp_float(float* current, const float from, const float to, float fraction, remix_vars::EASE_TYPE style)
	{
		if (current)
		{
			const float distance = to - *current;
			if (std::fabs(distance) < 1e-8f)
			{
				*current = to;
				return;
			}

			float e = fraction;

			switch (style)
			{
			default:
			case remix_vars::EASE_TYPE_LINEAR:
				break;

			case remix_vars::EASE_TYPE_SIN_IN:
				e = 1.0f - cosf((fraction * M_PI) * 0.5f);
				break;

			case remix_vars::EASE_TYPE_SIN_OUT:
				e = sinf((fraction * M_PI) * 0.5f);
				break;

			case remix_vars::EASE_TYPE_SIN_INOUT:
				e = -(cosf(M_PI * fraction) - 1.0f) * 0.5f;
				break;

			case remix_vars::EASE_TYPE_CUBIC_IN:
				e = fraction * fraction * fraction;
				break;

			case remix_vars::EASE_TYPE_CUBIC_OUT:
				e = 1.0f - powf(1.0f - fraction, 3.0f);
				break;

			case remix_vars::EASE_TYPE_CUBIC_INOUT:
				e = fraction < 0.5
					? 4.0f * fraction * fraction * fraction
					: 1.0f - powf(-2.0f * fraction + 2.0f, 3.0f) * 0.5f;
				break;

			case remix_vars::EASE_TYPE_EXPO_IN:
				e = fraction == 0.0f
					? 0.0f
					: powf(2.0f, 10.0f * fraction - 10.0f);
				break;

			case remix_vars::EASE_TYPE_EXPO_OUT:
				e = fraction == 1.0f
					? 1.0f
					: 1.0f - powf(2.0f, -10.0f * fraction);
				break;

			case remix_vars::EASE_TYPE_EXPO_INOUT:
				e = fraction == 0.0f ? 0.0f : fraction == 1.0f ? 1.0f
						: fraction < 0.5f
							? powf(2.0f, 20.0f * fraction - 10.0f) * 0.5f
							: (2.0f - powf(2.0f, -20.0f * fraction + 10.0f)) * 0.5f;
				break;
			}

			*current = from + (to - from) * e;
		}
	}

	/*void remix_vars::on_map_load(std::string map_name)
	{
		remix_vars::custom_options.clear();
		remix_vars::interpolate_stack.clear();

		shared::utils::replace_all(map_name, ".bms", ".conf");

		if (!map_name.ends_with(".conf")) {
			map_name += ".conf";
		}

		parse_and_apply_conf_with_lerp(map_name, shared::utils::string_hash64(map_name), EASE_TYPE_SIN_IN, 0.0f, 0.0f);
	}*/

	std::uint32_t framecounter = 0u;

	// Interpolates all variables on the 'interpolate_stack' and removes them once they reach their goal. \n
	void remix_vars::on_client_frame()
	{
		const auto gs = game_settings::get();
		if (shared::common::remix_api::is_initialized())
		{
			if (game::is_in_game)
			{
				// called in gta4::on_begin_scene_cb() otherwise
				if (game_settings::get()->timecycle_set_on_endscene.get_as<bool>()) {
					translate_and_apply_timecycle_settings();
				}

				if (framecounter++ > 60)
				{
					framecounter = 0u;

					// Remix sets 'rtx.di.initialSampleCount' to hardcoded values on start
					// and we def. need more then 3 samples to get somewhat good looking vehicle lights
					const auto rtxdi_override_val = gs->remix_override_rtxdi_samplecount.get_as<int>();
					if (rtxdi_override_val) // override if > 0
					{
						static auto rtxdi_samplecount = get_option("rtx.di.initialSampleCount");
						remix_vars::option_value val{ .value = (float)rtxdi_override_val };
						set_option(rtxdi_samplecount, val, false, true);
					}
				}
			}

			// --- transitions

			if (!is_paused())
			{
				if (!interpolate_stack.empty())
				{
					// remove completed transitions - we do that in-front of the loop so that the final values (complete) can be used for the entire frame
					auto completed_condition = [](const interpolate_entry_s& ip)
						{
							//if (ip._complete)
							//{
								//int break_me = 1;
								//DEBUG_PRINT("[VAR-LERP] Complete: %s\n", ip.option->first.c_str());
							//}

							return ip._complete;
						};

					const auto it = std::remove_if(interpolate_stack.begin(), interpolate_stack.end(), completed_condition);
					interpolate_stack.erase(it, interpolate_stack.end());

					// #

					//const auto globalv = interfaces::get()->m_globals;
					//const auto delta_abs = globalv->absoluteframetime;

					for (auto& ip : interpolate_stack)
					{
						ip._time_elapsed += get()->get_frametime(); //globalv->frametime;

						// initial 'time_elapsed' value can be negative because of transition delay
						// or if transitioning backwards with delay 
						if (ip._time_elapsed < 0.0f) {
							continue;
						}

						const auto f = ip._time_elapsed / ip.time_duration;
						const bool transition_time_exceeded = ip._time_elapsed >= ip.time_duration;

						switch (ip.type)
						{
						case OPTION_TYPE_INT:
						{
							if (!transition_time_exceeded)
							{
								float temp = (float)ip.option->second.current.integer;
								lerp_float(&temp, (float)ip.start.integer, (float)ip.goal.integer, f, ip.style);
								ip.option->second.current.integer = (int)temp;

								ip._complete = ip.option->second.current.integer == ip.goal.integer;
							}
							else
							{
								ip.option->second.current.integer = ip.goal.integer;
								ip._complete = true;
							}
							break;
						}

						case OPTION_TYPE_FLOAT:
						{
							if (!transition_time_exceeded)
							{
								lerp_float(&ip.option->second.current.value, ip.start.value, ip.goal.value, f, ip.style);
								ip._complete = shared::utils::float_equal(ip.option->second.current.value, ip.goal.value);
							}
							else
							{
								ip.option->second.current.value = ip.goal.value;
								ip._complete = true;
							}
							break;
						}

						case OPTION_TYPE_VEC2:
						{
							if (!transition_time_exceeded)
							{
								lerp_float(&ip.option->second.current.vector[0], ip.start.vector[0], ip.goal.vector[0], f, ip.style);
								lerp_float(&ip.option->second.current.vector[1], ip.start.vector[1], ip.goal.vector[1], f, ip.style);
								ip._complete = shared::utils::float_equal(ip.option->second.current.vector[0], ip.goal.vector[0])
									&& shared::utils::float_equal(ip.option->second.current.vector[1], ip.goal.vector[1]);
							}
							else
							{
								ip.option->second.current.vector[0] = ip.goal.vector[0];
								ip.option->second.current.vector[1] = ip.goal.vector[1];
								ip._complete = true;
							}
							break;
						}

						case OPTION_TYPE_VEC3:
						{
							if (!transition_time_exceeded)
							{
								lerp_float(&ip.option->second.current.vector[0], ip.start.vector[0], ip.goal.vector[0], f, ip.style);
								lerp_float(&ip.option->second.current.vector[1], ip.start.vector[1], ip.goal.vector[1], f, ip.style);
								lerp_float(&ip.option->second.current.vector[2], ip.start.vector[2], ip.goal.vector[2], f, ip.style);
								ip._complete = shared::utils::float_equal(ip.option->second.current.vector[0], ip.goal.vector[0])
									&& shared::utils::float_equal(ip.option->second.current.vector[1], ip.goal.vector[1])
									&& shared::utils::float_equal(ip.option->second.current.vector[2], ip.goal.vector[2]);
							}
							else
							{
								ip.option->second.current.vector[0] = ip.goal.vector[0];
								ip.option->second.current.vector[1] = ip.goal.vector[1];
								ip.option->second.current.vector[2] = ip.goal.vector[2];
								ip._complete = true;
							}
							break;
						}

						case OPTION_TYPE_BOOL:
						{
							// "complete" the transition when the rest finishes
							if (transition_time_exceeded) {
								ip._complete = true;
							}

							// on forward transition: set goal on start of transition
							// on backward transition: set goal when transition is completed
							if (!transition_time_exceeded && ip._in_backwards_transition) {
								break;
							}

							ip.option->second.current.enabled = ip.goal.enabled;
							break;
						}

						case OPTION_TYPE_NONE:
							ip._complete = true; // remove none type
							continue;
						}

						if (!ip.option->second.not_a_remix_var) {
							remix_vars::get()->set_option(ip.option, ip.option->second.current, false);
						}

						// detect completion of first transition - check / setup backwards transition
						if (ip._complete && !ip._in_backwards_transition && ip.time_delay_transition_back > 0.0f)
						{
							// swap start/goal
							std::swap(ip.start, ip.goal);

							ip._time_elapsed = -ip.time_delay_transition_back;
							ip._in_backwards_transition = true;
							ip._complete = false;
						}
					}
				}
			}
		}
	}

	void remix_vars::xo_vars_parse_options_fn()
	{
		remix_vars::options.clear();
		remix_vars::custom_options.clear();
		remix_vars::parse_rtx_options();

		// reset all settings to rtx.conf level (incl. runtime settings)
		if (shared::common::remix_api::is_initialized())
		{
			for (auto& o : remix_vars::options)
			{
				o.second.current = o.second.reset_level;
				remix_vars::set_option(&o, o.second.current);
			}
		}
	}

	void xo_vars_reset_all_options_fn()
	{
		remix_vars::reset_all_modified(false);
	}

	void xo_vars_clear_transitions_fn()
	{
		remix_vars::interpolate_stack.clear();
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

	void remix_vars::translate_and_apply_timecycle_settings()
	{
		auto unpack_uint32 = [](const uint32_t& in, float* out)
			{
				out[0] = static_cast<float>(in >> 16 & 0xFF) / 255.0f;
				out[1] = static_cast<float>(in >> 8 & 0xFF) / 255.0f;
				out[2] = static_cast<float>(in >> 0 & 0xFF) / 255.0f;
				out[3] = static_cast<float>(in >> 24 & 0xFF) / 255.0f;
			};

		auto mapRange = [](float input, float in_min, float in_max, float out_min, float out_max)
			{
				return out_min + (out_max - out_min) * ((input - in_min) / (in_max - in_min));
			};

		{
			static auto im = imgui::get();
			static auto vars = remix_vars::get();
			static auto gs = game_settings::get();
			remix_vars::option_value val{};


			// TODO! Add defaults for used remix variables in case user disables the saving of ALL remix variables

			//auto first_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8910);
			//auto third_timecycle = reinterpret_cast<game::TimeCycleParams*>(0x15E8B20);

			game::TimeCycleParams* timecycle = game::m_pCurrentTimeCycleParams_01;

			if (game::m_dwCutsceneState && *game::m_dwCutsceneState > 0) {
				timecycle = game::m_pCurrentTimeCycleParams_Cutscene;
			}

			// manual override
			if (im->m_dbg_used_timecycle >= 0)
			{
				switch (im->m_dbg_used_timecycle)
				{
				default:
				case 0:
					timecycle = game::m_pCurrentTimeCycleParams_01;
					break;

				case 1:
					timecycle = game::m_pCurrentTimeCycleParams_02;
					break;

				case 2:
					timecycle = game::m_pCurrentTimeCycleParams_Cutscene;
					break;
				}
			}

			static auto rtxSkybrightness = vars->get_option("rtx.skyBrightness");
			if (gs->timecycle_skylight_enabled.get_as<bool>() && rtxSkybrightness)
			{
				val.value = timecycle->mSkyLightMultiplier * gs->timecycle_skylight_scalar.get_as<float>();
				vars->set_option(rtxSkybrightness, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyLightMultiplier);
			}


			static auto rtxBloomBurnIntensity = vars->get_option("rtx.bloom.burnIntensity");
			static auto rtxBloomLuminanceThreshold = vars->get_option("rtx.bloom.luminanceThreshold");
			if (gs->timecycle_bloom_enabled.get_as<bool>() && rtxBloomBurnIntensity && rtxBloomLuminanceThreshold)
			{
				val.value = timecycle->mBloomIntensity * gs->timecycle_bloomintensity_scalar.get_as<float>();
				vars->set_option(rtxBloomBurnIntensity, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomIntensity);

				val.value = timecycle->mBloomThreshold * gs->timecycle_bloomthreshold_scalar.get_as<float>();
				vars->set_option(rtxBloomLuminanceThreshold, val);
				ASSIGN_IMGUI_VIS_FLOAT(mBloomThreshold);
			}


			static auto rtxTonemapColorBalance = vars->get_option("rtx.tonemap.colorBalance");
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

				Vector4D color_correction;
				unpack_uint32(timecycle->mColorCorrection, &color_correction.x);

				const auto& cc_influence = gs->timecycle_colorcorrection_influence.get_as<float>();
				color_correction.x *= cc_influence;
				color_correction.y *= cc_influence;
				color_correction.z *= cc_influence;

				val.vector[0] = color_correction.x + temp_color_offset.x;
				val.vector[1] = color_correction.y + temp_color_offset.y;
				val.vector[2] = color_correction.z + temp_color_offset.z;
				vars->set_option(rtxTonemapColorBalance, val);
				ASSIGN_IMGUI_VIS_UNPACKED_COLOR(mColorCorrection, color_correction);
			}


			static auto rtxTonemapSaturation = vars->get_option("rtx.tonemap.saturation");
			if (gs->timecycle_desaturation_enabled.get_as<bool>() && rtxTonemapSaturation)
			{
				const float far_desaturation_influence = gs->timecycle_fardesaturation_influence.get_as<float>() * mapRange(timecycle->mDesaturationFar, 0.0f, 1.0f, 0.0f, 0.4f);
				val.value = 1.0f - ((1.0f - timecycle->mDesaturation) * gs->timecycle_desaturation_influence.get_as<float>());
				val.value -= far_desaturation_influence;
				vars->set_option(rtxTonemapSaturation, val);
				ASSIGN_IMGUI_VIS_FLOAT(mDesaturation);
				im->m_timecyc_curr_mDesaturationFar = timecycle->mDesaturationFar;
				im->m_timecyc_curr_mDesaturationFar_offset = far_desaturation_influence;
			}


			static auto rtxTonemapExposureBias = vars->get_option("rtx.tonemap.exposureBias");
			if (gs->timecycle_gamma_enabled.get_as<bool>() && rtxTonemapExposureBias)
			{
				val.value = -(1.0f - timecycle->mGamma) + gs->timecycle_gamma_offset.get_as<float>();
				vars->set_option(rtxTonemapExposureBias, val);
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

			static auto rtxVolumetricsSingleScatteringAlbedo = vars->get_option("rtx.volumetrics.singleScatteringAlbedo");
			if (gs->timecycle_fogcolor_enabled.get_as<bool>() && rtxVolumetricsSingleScatteringAlbedo)
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

				vars->set_option(rtxVolumetricsSingleScatteringAlbedo, val);
				ASSIGN_IMGUI_VIS_VEC3(singleScatteringAlbedo);
			}

			float atmos_height = 0.0f;
			static auto rtxVolumetricsAtmosphereHeightMeters = vars->get_option("rtx.volumetrics.atmosphereHeightMeters");
			if (gs->timecycle_skyhorizonheight_enabled.get_as<bool>() && rtxVolumetricsAtmosphereHeightMeters)
			{
				atmos_height = timecycle->mSkyHorizonHeight * 100.0f * gs->timecycle_skyhorizonheight_scalar.get_as<float>();
				val.value = atmos_height;
				vars->set_option(rtxVolumetricsAtmosphereHeightMeters, val);
				ASSIGN_IMGUI_VIS_FLOAT(mSkyHorizonHeight);
			}

			static auto rtxVolumetricsTransmittanceMeasurementDistanceMeters = vars->get_option("rtx.volumetrics.transmittanceMeasurementDistanceMeters");
			if (gs->timecycle_fogdensity_enabled.get_as<bool>() && rtxVolumetricsTransmittanceMeasurementDistanceMeters)
			{
				val.value = mapRange(fog_color_density.w, 0.0f, 0.9f, 200.0f, 0.6f)
					* gs->timecycle_fogdensity_influence_scalar.get_as<float>()
					+ mapRange(atmos_height, 0.0f, 1000.0f,
						gs->timecycle_skyhorizonheight_low_transmittance_offset.get_as<float>(),
						gs->timecycle_skyhorizonheight_high_transmittance_offset.get_as<float>());

				if (val.value < 0.6f) {
					val.value = 0.6f;
				}

				vars->set_option(rtxVolumetricsTransmittanceMeasurementDistanceMeters, val);
				im->m_timecyc_curr_volumetricsTransmittanceMeasurementDistanceMeters = val.value;
			}
		}
	}

	remix_vars::remix_vars()
	{
		p_this = this;

		initialize(game::CMenuManager__m_MenuActive, nullptr);

		// -----
		m_initialized = true;
		shared::common::log("RemixVars", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
