#pragma once
#include "renderer.hpp"

namespace gta4
{
	class renderer_ff final : public shared::common::loader::component_module
	{
	public:
		renderer_ff();

		static inline renderer_ff* p_this = nullptr;
		static renderer_ff* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static void on_ff_emissives(IDirect3DDevice9* dev, drawcall_mod_context& ctx);
		static void on_ff_emissives_alpha(IDirect3DDevice9* dev, drawcall_mod_context& ctx);

	private:
		bool m_initialized = false;
	};
}
