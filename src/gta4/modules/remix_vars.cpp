#include "std_include.hpp"
#include "remix_vars.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "timecycle.hpp"
#include "shared/common/flags.hpp"
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

			shared::common::log("RemixVars", "Parsing 'rtx.conf' ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
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
		std::unique_lock lock(get()->mutex_);
		auto& custom_options = get()->custom_options;

		custom_options[name] = o;

		if (const auto it = custom_options.find(name); it != custom_options.end()) {
			return &*it;
		}

		return nullptr;
	}

	remix_vars::option_handle remix_vars::get_custom_option(const char* o)
	{
		std::shared_lock lock(get()->mutex_);
		auto& custom_options = get()->custom_options;

		if (const auto it = custom_options.find(o); it != custom_options.end()) {
			return &*it;
		}

		return nullptr;
	}

	remix_vars::option_handle remix_vars::get_custom_option(const std::string& o)
	{
		std::shared_lock lock(get()->mutex_);
		auto& custom_options = get()->custom_options;

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
		std::shared_lock lock(get()->mutex_);
		auto& options = get()->options;

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
		std::shared_lock lock(get()->mutex_);
		auto& options = get()->options;

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
	 * @param for_user_layer	options are usually set on the derived layer (below user) which might prevent some settings from being set -> set to true to set them on the user layer
	* @return					true if successfull
	 */
	bool remix_vars::set_option(option_handle o, const option_value& v, const bool is_level_setting, const bool always, bool for_user_layer)
	{
		if (o && shared::common::remix_api::is_initialized())
		{
			std::unique_lock lock(get()->mutex_);

			if (!always && o->second.current.compare(o->second.type, v, 0.01f))
			{
				if (imgui::get()->m_dbg_debug_single_frame_timecycle_remix_vars) {
					shared::common::log("RemixVars", std::format("Not setting {} because value did not change (- enough).", o->first));
				}

				return false;
			}

			o->second.current = v;

			if (is_level_setting) {
				o->second.reset_level = v;
			}

			std::string var_str = for_user_layer ? "user " : "";
			switch(o->second.type)
			{
			case OPTION_TYPE_BOOL:
				var_str += v.enabled ? "True" : "False";
				o->second.modified = o->second.current.enabled != o->second.reset.enabled;
				break;
			case OPTION_TYPE_INT:
				var_str += std::to_string(v.integer);
				o->second.modified = o->second.current.integer != o->second.reset.integer;
				break;
			case OPTION_TYPE_FLOAT:
				var_str += std::to_string(v.value);
				o->second.modified = o->second.current.value != o->second.reset.value;
				break;
			case OPTION_TYPE_VEC2:
				var_str += std::to_string(v.vector[0]) + ", " + std::to_string(v.vector[1]);
				o->second.modified = o->second.current.vector[0] != o->second.reset.vector[0] || o->second.current.vector[1] != o->second.reset.vector[1];
				break;
			case OPTION_TYPE_VEC3:
				var_str += std::to_string(v.vector[0]) + ", " + std::to_string(v.vector[1]) + ", " + std::to_string(v.vector[2]);
				o->second.modified = o->second.current.vector[0] != o->second.reset.vector[0] || o->second.current.vector[1] != o->second.reset.vector[1] || o->second.current.vector[2] != o->second.reset.vector[2];
				break;
			case OPTION_TYPE_NONE:
				return false;
			}

			if (!var_str.empty())
			{
				/*const auto result =*/ shared::common::remix_api::get().m_bridge.SetConfigVariable(o->first.c_str(), var_str.c_str());

				//shared::common::log("RemixVars", std::format("Set {} to {} - {}", o->first, var_str, result == REMIXAPI_ERROR_CODE_SUCCESS ? "SUCCESS" : "FAIL"));
				
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
			{
				std::unique_lock lock(get()->mutex_);
				o->second.current = reset_to_level_state ? o->second.reset_level : o->second.reset;
			}

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
			auto& options = get()->options;

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
			std::unique_lock lock(get()->mutex_);
			auto& options = get()->options;

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
	 * Parses a .conf within the specified directory and sets to contained values
	 * @param sub_dir				config name without extension
	 * @param file_name				name of config without extension
	 * @param delay					delay transition start (in seconds)
	 */
	void remix_vars::parse_and_apply_conf(const std::string& sub_dir, const std::string& file_name, const float delay)
	{
		std::ifstream file;
		if (shared::utils::open_file_homepath(sub_dir, file_name, file))
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
						remix_vars::get()->add_interpolate_entry(o, v, delay);
						//DEBUG_PRINT("[VAR-LERP] Start lerping var: %s to: %s\n", o->first.c_str(), pair[1].c_str());
					}
				}
			}

			file.close();
		} else {
			shared::common::log("RemixVars", std::format("Failed to find config: {} in {}", file_name, sub_dir), shared::common::LOG_TYPE::LOG_TYPE_WARN, true);
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
	bool remix_vars::add_interpolate_entry(option_handle handle, const option_value& goal, const float delay, const std::string& remix_var_name)
	{
		std::unique_lock lock(get()->mutex_);

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
			// directly apply when no delay
			if (delay == 0.0f) 
			{
				lock.unlock();
				set_option(handle, goal);
			}
			// interpolate over time or set after delay
			else
			{
				{
					interpolate_stack.emplace_back(interpolate_entry_s
						{ h, h->second.current, goal, h->second.type, -delay });
				}
			}

			return true;
		}

		return false;
	}

	void remix_vars::init_once_on_init()
	{
		if (const auto v = get();
					  !v->m_init_once_on_init)
		{
			v->m_init_once_on_init = true;

			if (static bool disable_addon_settings_flag = shared::common::flags::has_flag("disable_addon_settings"); !disable_addon_settings_flag)
			{
#if 1
				// Process all addon remix conf files in inverse alphabetical order (lower to higher priority)
				const auto addon_comp_settings_files = shared::utils::get_sorted_files("rtx_comp\\addon_settings", ".conf", true, false);
				for (const auto& file_name : addon_comp_settings_files)
				{
					shared::common::log("RemixVars", std::format("> Parsing Addon Config 'rtx_comp/addon_settings/{}' ...", file_name), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
					parse_and_apply_conf("rtx_comp\\addon_settings", file_name, 1);
				}
#endif
			}
		}
	}

	bool remix_vars::init_once_on_ingame_frame()
	{
		if (const auto v = get(); 
					  !v->m_init_once_on_ingame_frame)
		{
			v->m_init_once_on_ingame_frame = true;

			// so empty

			return true;
		}

		return false;
	}


	std::uint32_t framecounter = 0u;

	// Interpolates all variables on the 'interpolate_stack' and removes them once they reach their goal. \n
	// Called on d3d9ex::D3D9Device::EndScene
	void remix_vars::on_client_frame()
	{
		GTA4_PERF_SCOPE(performance_section::RemixVars);
		if (const auto v = get(); !v) {
			return;
		}

		const auto gs = comp_settings::get();
		if (shared::common::remix_api::is_initialized())
		{
			init_once_on_init();

			// called in gta4::on_begin_scene_cb() otherwise
			if (comp_settings::get()->timecycle_set_on_endscene.get_as<bool>()) {
				timecycle::translate_and_apply_timecycle_settings();
			}

			if (game::is_in_game)
			{
				if (framecounter++ > 60)
				{
					framecounter = 0u;

					init_once_on_ingame_frame();

					// Remix sets 'rtx.di.initialSampleCount' to hardcoded values on start
					// and we def. need more then 3 samples to get somewhat good looking vehicle lights
					const auto rtxdi_override_val = gs->remix_override_rtxdi_samplecount.get_as<int>();
					if (rtxdi_override_val) // override if > 0
					{
						static auto rtxdi_samplecount = get_option("rtx.di.initialSampleCount");
						option_value val { .value = (float)rtxdi_override_val };
						set_option(rtxdi_samplecount, val, false, true);
					}

					// particle mode is disabled by default (rtx.conf), enable if alpha emissive hack is on
					if (static auto rr_particle_mode = get_option("rtx.rayreconstruction.particleBufferMode"); rr_particle_mode)
					{
						rr_particle_mode->second.type = OPTION_TYPE::OPTION_TYPE_INT; // float by default
						option_value val{ .integer = (gs->emissive_alpha_blend_hack._bool() ? 1 : 0) };
						set_option(rr_particle_mode, val, false, gs->emissive_alpha_blend_hack._bool()); // only override constantly when hack is enabled
					}
				}

				if (!is_paused())
				{
					if (!interpolate_stack.empty())
					{
						for (auto& ip : interpolate_stack)
						{
							ip._time_elapsed += get()->get_frametime() * 0.001f; // ms to s

							// check if delayed
							if (ip._time_elapsed < 0.0f) {
								continue;
							}

							set_option(ip.option, ip.goal, false, true);
							ip._complete = true;

							auto completed_condition = [](const interpolate_entry_s& ip) {
								return ip._complete;
							};

							const auto it = std::remove_if(interpolate_stack.begin(), interpolate_stack.end(), completed_condition);
							interpolate_stack.erase(it, interpolate_stack.end());
						}
					}
				}
			}
		}
	}

	void remix_vars::xo_vars_parse_options_fn()
	{
		std::unique_lock lock(get()->mutex_);
		get()->options.clear();
		get()->custom_options.clear();
		parse_rtx_options();

		if (shared::common::remix_api::is_initialized())
		{
			auto& options = get()->options;
			for (auto& o : options) {
				remix_vars::set_option(&o, o.second.current, false, true);
			}
		}
	}

	void xo_vars_reset_all_options_fn()
	{
		std::unique_lock lock(remix_vars::get()->mutex_);
		remix_vars::reset_all_modified(false);
	}

	void xo_vars_clear_transitions_fn()
	{
		std::shared_lock lock(remix_vars::get()->mutex_);
		remix_vars::interpolate_stack.clear();
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
