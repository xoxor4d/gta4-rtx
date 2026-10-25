#include "std_include.hpp"

#if defined(GTA4_PROFILING)
namespace shared::common::profiler
{
	void name_current_thread(const char* name)
	{
		thread_local bool named = false;
		if (!named)
		{
			tracy::SetThreadName(name);
			named = true;
		}
	}

	void set_program_info(const char* name, const std::string_view info)
	{
		TracySetProgramName(name);
		TracyAppInfo(info.data(), info.size());
	}

	bool is_connected() {
		return TracyIsConnected;
	}

	void manual_zone::begin() {
		m_context = ___tracy_emit_zone_begin(&m_location, 1);
	}

	void manual_zone::end()
	{
		___tracy_emit_zone_end(m_context);
		m_context = {};
	}
}
#endif
