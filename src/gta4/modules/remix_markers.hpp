#pragma once

namespace gta4
{
	class remix_markers final : public shared::common::loader::component_module
	{
	public:
		remix_markers();

		static inline remix_markers* p_this = nullptr;
		static remix_markers* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static void draw_nocull_markers();

		static constexpr uint32_t DISTANCE_CHECK_FRAME_INTERVAL = 120;

	private:
		bool m_initialized = false;
	};
}
