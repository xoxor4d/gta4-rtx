#include "std_include.hpp"

namespace shared::common::toml
{
	// format 2 decimals
	inline std::string format_float(float value)
	{
		return std::format("{:.2f}", value);
	}
}