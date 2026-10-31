#pragma once
#include <source_location>

// Tracy instrumentation: GTA4_PROFILING is only defined for the ReleaseTracy configuration (deps/premake/tracy.lua).
// In every other configuration the macros expand to nothing and the functions are empty inlines.

#if defined(GTA4_PROFILING)
#pragma warning(push, 0)
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#pragma warning(pop)

/// end of a rendered frame, the main frame set of the trace
#define PROFILE_FRAME_MARK()					FrameMark

/// end of a frame of a secondary frame set (e.g. the game thread tick), NAME has to be a string literal
#define PROFILE_FRAME_MARK_NAMED(NAME)			FrameMarkNamed(NAME)

/// zone until the end of the enclosing scope, named after the function
#define PROFILE_ZONE()							ZoneScoped

/// zone until the end of the enclosing scope, NAME has to be a constant expression
#define PROFILE_ZONE_N(NAME)					ZoneScopedN(NAME)

/// PROFILE_ZONE_N with a color (0xRRGGBB)
#define PROFILE_ZONE_NC(NAME, COLOR)			ZoneScopedNC(NAME, COLOR)

/// printf formatted text attached to the innermost zone of the current scope
#define PROFILE_ZONE_TEXT(FMT, ...)				ZoneTextF(FMT, ##__VA_ARGS__)

/// number attached to the innermost zone of the current scope
#define PROFILE_ZONE_VALUE(VALUE)				ZoneValue(static_cast<std::uint64_t>(VALUE))

/// one sample of a named plot, usually once per frame
#define PROFILE_PLOT(NAME, VALUE)				TracyPlot(NAME, static_cast<double>(VALUE))

/// message on the timeline, TEXT has to be a string literal
#define PROFILE_MESSAGE(TEXT)					TracyMessageL(TEXT)

/// declares mutex VAR of TYPE whose locks and contention show up in the trace
#define PROFILE_LOCKABLE(TYPE, VAR)				TracyLockable(TYPE, VAR)

/// PROFILE_LOCKABLE for shared mutex types (std::shared_mutex)
#define PROFILE_SHARED_LOCKABLE(TYPE, VAR)		TracySharedLockable(TYPE, VAR)

#else
#define PROFILE_FRAME_MARK()					((void)0)
#define PROFILE_FRAME_MARK_NAMED(NAME)			((void)0)
#define PROFILE_ZONE()							((void)0)
#define PROFILE_ZONE_N(NAME)					((void)0)
#define PROFILE_ZONE_NC(NAME, COLOR)			((void)0)
#define PROFILE_ZONE_TEXT(FMT, ...)				((void)0)
#define PROFILE_ZONE_VALUE(VALUE)				((void)0)
#define PROFILE_PLOT(NAME, VALUE)				((void)0)
#define PROFILE_MESSAGE(TEXT)					((void)0)
#define PROFILE_LOCKABLE(TYPE, VAR)				TYPE VAR
#define PROFILE_SHARED_LOCKABLE(TYPE, VAR)		TYPE VAR
#endif

namespace shared::common::profiler
{
#if defined(GTA4_PROFILING)
	inline constexpr bool enabled = true;

	/// names the calling thread in the trace and in debuggers, later calls from the same thread are ignored
	void name_current_thread(const char* name);

	/// program name and info line shown in the trace information, name has to be a string literal
	void set_program_info(const char* name, std::string_view info);

	/// zones are only recorded while a profiler is connected (TRACY_ON_DEMAND)
	bool is_connected();

	/// Zone whose begin and end are not in one scope, e.g. the pre and post hook of one game function.
	/// - the instance needs static storage duration, it holds the source location of the zone
	/// - begin() and end() have to be called on the same thread and the zone is not reentrant
	class manual_zone
	{
	public:
		explicit constexpr manual_zone(const char* name, const std::uint32_t color = 0u, const std::source_location loc = std::source_location::current()) :
			m_location { name, loc.function_name(), loc.file_name(), loc.line(), color } {}

		void begin();
		void end();

	private:
		___tracy_source_location_data m_location;
		TracyCZoneCtx m_context = {};
	};
#else
	inline constexpr bool enabled = false;

	inline void name_current_thread(const char*) {}
	inline void set_program_info(const char*, std::string_view) {}
	inline bool is_connected() { return false; }

	class manual_zone
	{
	public:
		explicit constexpr manual_zone(const char*, std::uint32_t = 0u) {}

		void begin() {}
		void end() {}
	};
#endif
}
