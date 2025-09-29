#pragma once

namespace shared::common
{
	class flags
	{
	public:
		// enforce singleton pattern
		flags(const flags&) = delete;
		flags& operator=(const flags&) = delete;
		static flags& get();

		static bool has_flag(const std::string& flag);

	private:
		flags() : m_initialized(false) {}
		bool	  m_initialized;

		static std::vector<std::string> m_enabled_flags;

		void parse_flags();
	};
}