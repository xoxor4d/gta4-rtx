#include "std_include.hpp"
#include "natives.hpp"

namespace gta4
{
	natives::natives()
	{
		p_this = this;

		// -----
		m_initialized = true;
		shared::common::log("Natives", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
