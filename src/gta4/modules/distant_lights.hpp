#pragma once
#include "renderer_api.hpp"

namespace gta4
{
	/// The game's "vehicle imposters" (-disableimposters): distant street light / aircraft sprites and fake cars along the
	/// road light strings (headlight models at night, car models by day), optionally drawn through the Remix API:
	/// - sprites: every flush of the game's sprite buffer is collected into one api mesh per frame
	/// - cars: the instanced model becomes a cached api mesh, every car record one api instance (colour as texture factor)
	class distant_lights final : public shared::common::loader::component_module
	{
	public:
		distant_lights();

		static inline distant_lights* p_this = nullptr;
		static distant_lights* get() { return p_this; }

		struct stats_s
		{
			bool sprites_rendered = false;      // the game's sprite render ran (night, not disabled)
			bool sprites_api = false;
			bool sprites_warming_up = false;    // D3D9 render so the runtime uploads a new texture, api from the next render
			std::uint32_t sprites = 0u;
			std::uint32_t sprite_flushes = 0u;  // game flushes = D3D9 draws on the D3D9 path
			std::uint32_t sprite_vertices = 0u;
			std::uint32_t sprite_triangles = 0u;

			bool cars_rendered = false;
			bool cars_night = false;
			bool cars_api = false;
			std::uint32_t car_records = 0u;
			std::uint32_t car_flushes = 0u;     // game flushes = instanced D3D9 draws on the D3D9 path
			std::uint32_t car_warmups = 0u;     // flushes left on D3D9 so the runtime uploads a new texture
			std::uint32_t car_instances = 0u;   // api instances drawn
			std::uint32_t car_meshes = 0u;      // cached api meshes
			renderer_api::reject_reason car_reject = renderer_api::reject_reason::none;
		};

		/// render thread: the engine's instanced draw of a captured car flush, drawn as api instances instead
		/// @return true if the D3D9 draw must be skipped
		static bool try_draw_cars(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT base_vertex, UINT min_index, UINT num_vertices, UINT start_index, UINT prim_count);

		/// destroys all api meshes and materials of the module (rebuilt on the next render)
		static void destroy_all();

		static stats_s get_stats();
		static void devgui();
	};
}
