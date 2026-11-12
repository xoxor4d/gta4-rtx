#pragma once
#include "map_settings.hpp"

namespace gta4
{
	class remix_lights final : public shared::common::loader::component_module
	{
	public:
		remix_lights();

		static constexpr float CYLINDER_DEFAULT_RADIUS = 0.05f; // tube radius of a cylinder light whose override sets none

		static inline remix_lights* p_this = nullptr;
		static remix_lights* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static void on_client_frame();
		static void reset();

	public:
		// -----

		struct remix_light_def
		{
			remixapi_LightHandle m_handle = nullptr;
			remixapi_LightInfoSphereEXT m_ext = {};
			remixapi_LightInfoRectEXT m_ext_rect = {}; // m_info points at the struct of the light's shape
			remixapi_LightInfoDiskEXT m_ext_disk = {};
			remixapi_LightInfoCylinderEXT m_ext_cylinder = {};
			remixapi_LightInfo m_info = {};
			game::CLightSource m_def = {};
			uint64_t m_hash;
			std::uint32_t m_light_num = 0u;
			std::uint32_t m_updateframe = 0u;
			bool m_is_ignored = false;
			bool m_is_allowed_filler = false;
			bool m_is_filler = false;
			bool m_is_attached_light = false;
		};

		struct remix_distant_light_def
		{
			remixapi_LightHandle m_handle = nullptr;
			remixapi_LightInfoDistantEXT m_ext = {};
			remixapi_LightInfo m_info = {};
			//game::CDirectionalLight m_def = {};
			uint64_t m_hash;
			std::uint32_t m_updateframe = 0u;
		};

		static const Vector&	get_light_position(const game::CLightSource& def, const map_settings::light_override_s* lov = nullptr);
		static const Vector&	get_light_dir(const game::CLightSource& def, const map_settings::light_override_s* lov = nullptr);
		static Vector			get_light_color(const game::CLightSource & def, const map_settings::light_override_s * lov = nullptr);
		static float			get_light_radius(const game::CLightSource& def, const map_settings::light_override_s* lov = nullptr);
		static float			get_light_intensity(const game::CLightSource& def, const map_settings::light_override_s* lov = nullptr);
		static float			get_light_outer_cone_angle(const game::CLightSource& def, const map_settings::light_override_s* lov= nullptr );
		static float			get_light_inner_cone_angle(const game::CLightSource& def, const map_settings::light_override_s* lov= nullptr );
		static float			get_light_volumetric_scale(const game::CLightSource& def, const map_settings::light_override_s* lov= nullptr );
		static int				get_light_type(const game::CLightSource& def, const map_settings::light_override_s* lov = nullptr);

		/// orthonormal axes of a light that points along 'dir', x and y rotated around it by 'roll_degrees'
		static void				get_light_axes(Vector dir, float roll_degrees, Vector& x_axis, Vector& y_axis, Vector& z_axis);

		bool spawn_or_update_remix_sphere_light(remix_light_def& light, bool update = false, bool custom = false, const map_settings::light_override_s* custom_lov = nullptr);

		void add_light(const game::CLightSource& def, const uint64_t& key, const uint64_t& hash, bool is_filler = false, bool add_but_do_not_draw = false);
		void destroy_light(remix_light_def& light);

		void destroy_all_lights();
		void destroy_and_clear_all_active_lights();
		void draw_all_active_lights();

		size_t get_active_light_count() const {
			return m_active_lights.size();
		}

		std::unordered_map<std::uint64_t, remix_light_def>& get_active_lights() {
			return m_active_lights;
		}

		static void clear_light_cache() {
			m_active_lights.clear();
		}

		remix_distant_light_def& get_distant_light() {
			return m_distant_light;
		}

		std::uint32_t get_updateframe() const {
			return m_updateframe;
		}

	private:
		static inline std::uint32_t m_active_light_spawn_tracker = 0u;
		static inline std::unordered_map<std::uint64_t, remix_light_def> m_active_lights = {};
		static inline remix_distant_light_def m_distant_light = {};
		std::uint32_t m_updateframe = 0u;
		bool m_is_paused = false;
		bool m_initialized = false;
	};
}
