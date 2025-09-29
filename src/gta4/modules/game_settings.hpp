#pragma once
namespace gta4
{
	class game_settings final : public shared::common::loader::component_module
	{
	public:
		game_settings();
		~game_settings() = default;
	
		static inline game_settings* p_this = nullptr;
		static auto get() { return &vars; }
	
		static void write_toml();
		static bool parse_toml();
	
	private:
		union var_value
		{
			bool boolean;
			int integer;
			float value[4] = {};
		};
	
		enum var_type : std::uint8_t
		{
			var_type_boolean = 0,
			var_type_integer = 1,
			var_type_value = 2,
			var_type_vec2 = 3,
			var_type_vec3 = 4,
			var_type_vec4 = 5,
		};
	
		class variable
		{
		public:
			// bool
			variable(const char* name, const char* desc, const bool boolean) :
				m_name(name), m_desc(desc), m_type(var_type_boolean)
			{
				m_var.boolean = boolean;
				m_var_default.boolean = boolean;
			}
	
			// int
			variable(const char* name, const char* desc, const int integer) :
				m_name(name), m_desc(desc), m_type(var_type_integer)
			{
				m_var.integer = integer;
				m_var_default.integer = integer;
			}
	
			// float
			variable(const char* name, const char* desc, const float value) :
				m_name(name), m_desc(desc), m_type(var_type_value)
			{
				m_var.value[0] = value;
				m_var_default.value[0] = value;
			}
	
			// vec2
			variable(const char* name, const char* desc, const float x, const float y) :
				m_name(name), m_desc(desc), m_type(var_type_vec2)
			{
				m_var.value[0] = x; m_var.value[1] = y;
				m_var_default.value[0] = x; m_var_default.value[1] = y;
			}
	
			// vec3
			variable(const char* name, const char* desc, const float x, const float y, const float z) :
				m_name(name), m_desc(desc), m_type(var_type_vec3)
			{
				m_var.value[0] = x; m_var.value[1] = y; m_var.value[2] = z;
				m_var_default.value[0] = x; m_var_default.value[1] = y; m_var_default.value[2] = z;
			}
	
			// vec4
			variable(const char* name, const char* desc, const float x, const float y, const float z, const float w) :
				m_name(name), m_desc(desc), m_type(var_type_vec4)
			{
				m_var.value[0] = x; m_var.value[1] = y; m_var.value[2] = z; m_var.value[3] = w;
				m_var_default.value[0] = x; m_var_default.value[1] = y; m_var_default.value[2] = z; m_var_default.value[3] = w;
			}
	
			const char* get_str_value(bool get_default = false) const
			{
				const auto pvec = !get_default ? &m_var.value[0] : &m_var_default.value[0];
	
				switch (m_type)
				{
				case var_type_boolean:
					return shared::utils::va("%s", (!get_default ? m_var.boolean : m_var_default.boolean) ? "true" : "false");
	
				case var_type_integer:
					return shared::utils::va("%d", !get_default ? m_var.integer : m_var_default.integer);
	
				case var_type_value:
					return shared::utils::va("%.2f", pvec[0]);
	
				case var_type_vec2:
					return shared::utils::va("[ %.2f, %.2f ]", pvec[0], pvec[1]);
	
				case var_type_vec3:
					return shared::utils::va("[ %.2f, %.2f, %.2f ]", pvec[0], pvec[1], pvec[2]);
	
				case var_type_vec4:
					return shared::utils::va("[ %.2f, %.2f, %.2f, %.2f ]", pvec[0], pvec[1], pvec[2], pvec[3]);
				}
	
				return nullptr;
			}
	
			const char* get_str_type() const
			{
				switch (m_type)
				{
				case var_type_boolean:
					return "BOOL";
	
				case var_type_integer:
					return "INT";
	
				case var_type_value:
					return "FLOAT";
	
				case var_type_vec2:
					return "VEC2";
	
				case var_type_vec3:
					return "VEC3";
	
				case var_type_vec4:
					return "VEC4";
				}
	
				return nullptr;
			}
	
			std::string get_tooltip_string() const
			{
				std::string out;
				out += "# " + std::string(this->m_desc) + "\n";
				out += "# Type: " + std::string(this->get_str_type()) + " || Default: " + std::string(this->get_str_value(true));
				return out;
			}
	
			template <typename T>
			T get_as(bool default_val = false)
			{
				// if T is a pointer type, return a ptr
				if constexpr (std::is_pointer_v<T>)
				{
					// get the underlying type (e.g., int from int*)
					using base_type = std::remove_pointer_t<T>;
	
					if constexpr (std::is_same_v<base_type, bool>) {
						assert(m_type == var_type_boolean && "Type mismatch: expected boolean");
						return &(!default_val ? m_var.boolean : m_var_default.boolean);
					}
					else if constexpr (std::is_same_v<base_type, int>) {
						assert(m_type == var_type_integer && "Type mismatch: expected integer");
						return &(!default_val ? m_var.integer : m_var_default.integer);
					}
					else if constexpr (std::is_same_v<base_type, float>) {
						if (m_type == var_type_value) {
							return &(!default_val ? m_var.value[0] : m_var_default.value[0]);
						}
						if (m_type >= var_type_vec2 && m_type <= var_type_vec4) {
							return !default_val ? m_var.value : m_var_default.value;
						}
						assert(false && "Type mismatch: expected float or vector type");
						return nullptr;
					}
					else if constexpr (std::is_same_v<base_type, Vector2D>) {
						assert(m_type == var_type_vec2 && "Type mismatch: expected vec2 for Vector");
						return reinterpret_cast<Vector2D*>(!default_val ? m_var.value : m_var_default.value);
					}
					else if constexpr (std::is_same_v<base_type, Vector>) {
						assert(m_type == var_type_vec3 && "Type mismatch: expected vec3 for Vector");
						return reinterpret_cast<Vector*>(!default_val ? m_var.value : m_var_default.value);
					}
					else if constexpr (std::is_same_v<base_type, Vector4D>) {
						assert(m_type == var_type_vec4 && "Type mismatch: expected vec4 for Vector");
						return reinterpret_cast<Vector4D*>(!default_val ? m_var.value : m_var_default.value);
					}
					else {
						static_assert(std::is_same_v<T, void>, "Unsupported pointer type in get_as");
						return nullptr;
					}
				}
				// return by value for non-pointer types
				else
				{
					if constexpr (std::is_same_v<T, bool>) {
						assert(m_type == var_type_boolean && "Type mismatch: expected boolean");
						return static_cast<T>(!default_val ? m_var.boolean : m_var_default.boolean);
					}
					else if constexpr (std::is_same_v<T, int>) {
						assert(m_type == var_type_integer && "Type mismatch: expected integer");
						return static_cast<T>(!default_val ? m_var.integer : m_var_default.integer);
					}
					else if constexpr (std::is_same_v<T, float>) {
						assert(m_type == var_type_value && "Type mismatch: expected float");
						return static_cast<T>(!default_val ? m_var.value[0] : m_var_default.value[0]);
					}
					else if constexpr (std::is_same_v<T, Vector2D>) {
						assert(m_type == var_type_vec2 && "Type mismatch: expected vec2 for Vector");
						return Vector2D(!default_val ? m_var.value : m_var_default.value);
					}
					else if constexpr (std::is_same_v<T, Vector>) {
						assert(m_type == var_type_vec3 && "Type mismatch: expected vec3 for Vector");
						return Vector(!default_val ? m_var.value : m_var_default.value);
					}
					else if constexpr (std::is_same_v<T, Vector4D>) {
						assert(m_type == var_type_vec4 && "Type mismatch: expected vec4 for Vector");
						return Vector4D(!default_val ? m_var.value : m_var_default.value);
					}
					else {
						static_assert(std::is_same_v<T, void>, "Unsupported return type in get_as");
						return T{};
					}
				}
			}

			var_type get_type() const {
				return m_type;
			}

			// sets var and writes toml (bool)
			void set_var(const bool boolean, bool no_toml_update = false)
			{
				m_var.boolean = boolean;
				if (!no_toml_update) {
					write_toml();
				}
			}
	
			// sets var and writes toml (integer)
			void set_var(const int integer, bool no_toml_update = false)
			{
				m_var.integer = integer;
				if (!no_toml_update) {
					write_toml();
				}
			}
	
			// sets var and writes toml (float)
			void set_var(const float value, bool no_toml_update = false)
			{
				m_var.value[0] = value;
				if (!no_toml_update) {
					write_toml();
				}
			}
	
			// sets var and writes toml (vec4)
			void set_vec(const float* v, bool no_toml_update = false)
			{
				switch (m_type)
				{
				default:
					break;
	
				case var_type_value:
					m_var.value[0] = v[0];
					break;
	
				case var_type_vec2:
					m_var.value[0] = v[0]; m_var.value[1] = v[1];
					break;
	
				case var_type_vec3:
					m_var.value[0] = v[0]; m_var.value[1] = v[1]; m_var.value[2] = v[2];
					break;
	
				case var_type_vec4:
					m_var.value[0] = v[0]; m_var.value[1] = v[1]; m_var.value[2] = v[2]; m_var.value[3] = v[3];
					break;
				}
	
				if (!no_toml_update) {
					write_toml();
				}
			}
	
			const char* m_name;
			const char* m_desc;
	
		private:
			var_value m_var;
			var_value m_var_default;
			var_type m_type;
		};
	
		struct var_definitions
		{
			variable fix_windowed_hud =
			{
				"fix_windowed_hud",
				("Fix in-game hud scaling when game window resolution does not match monitor resulition. Set desired windowed resolution via 'fix_windowed_hud_resolution'"),
				true
			};

			variable fix_windowed_hud_resolution =
			{
				"fix_windowed_hud_resolution",
				("Resolution used by the 'fix_windowed_hud' setting"),
				1920, 1080
			};

			// --

			variable fixed_function_trees =
			{
				"fixed_function_trees",
				("Render trees via fixed function. Improves performance but gets rid of wind sway"),
				true
			};

			variable tree_foliage_alpha_cutout_value =
			{
				"tree_foliage_alpha_cutout_value",
				("Value used for ALPHAREF. 0.625f for PC and 4.0 for Console"),
				0.5f
			};

			variable grass_foliage_alpha_cutout_value =
			{
				"grass_foliage_alpha_cutout_value",
				("Value used for ALPHAREF. 0.625f for PC and 4.0 for Console"),
				0.6f
			};

			// --

			variable nocull_dist_near_static =
			{
				"nocull_dist_near_static",
				("Distance (radius around player) where culling of static objects is disabled"),
				50.0f
			};

			variable nocull_dist_medium_static =
			{
				"nocull_dist_medium_static",
				("Distance (radius around player) were an object radius is checked against the 'nocull_radius_medium_static' setting. Objects with larger radii will NOT get culled."),
				80.0f
			};

			variable nocull_radius_medium_static =
			{
				"nocull_radius_medium_static",
				("The minimum radius an object has to have to not get culled within the distance set by 'nocull_dist_medium_static'"),
				40.0f
			};

			variable nocull_dist_far_static =
			{
				"nocull_dist_far_static",
				("Distance (radius around player) were an object radius is checked against the 'nocull_radius_far_static' setting. Objects with larger radii will NOT get culled."),
				300.0f
			};

			variable nocull_radius_far_static =
			{
				"nocull_radius_far_static",
				("The minimum radius an object has to have to not get culled within the distance set by 'nocull_dist_far_static'"),
				80.0f
			};

			// --

			variable translate_game_lights =
			{
				"translate_game_lights",
				("This recreates game-lights as remixApi lights"),
				true
			};

			variable translate_game_light_radius_scalar =
			{
				"translate_game_light_radius_scalar",
				("Scale radius of translated game lights"),
				0.2f
			};

			variable translate_game_light_intensity_scalar =
			{
				"translate_game_light_intensity_scalar",
				("Scale intensity of translated game lights"),
				7000.0f
			};

			variable translate_game_light_softness_offset =
			{
				"translate_game_light_softness_offset",
				("Offset softness of translated game lights"),
				0.20f
			};

			variable translate_game_light_angle_offset =
			{
				"translate_game_light_angle_offset",
				("Offset spotlight angles of translated game lights"),
				0.0f
			};

			variable translate_sunlight_intensity_scalar =
			{
				"translate_sunlight_intensity_scalar",
				("Scale intensity of translated game sunlight"),
				1.0f
			};

			variable vehicle_lights_emissive_scalar =
			{
				"vehicle_lights_emissive_scalar",
				("Scale emissive strength of vehicle lights (< gta4 emissiveMultiplier shader constant >  *  < this scalar >  *  < remix material setting >  *  < global remix emissive setting >)"),
				13.0f
			};

			variable vehicle_lights_dual_render_proxy_texture =
			{
				"vehicle_lights_dual_render_proxy_texture",
				("This renders surfaces using the 'gta_vehicle_lightsemissive' shader a second time using a proxy texture (veh_light_ems_glass.png)."
				"# The remix-mod of the compatibility mod makes that surface translucent."),
				true
			};

			// --

			variable game_wetness_scalar =
			{
				"game_wetness_scalar",
				("Scales the weather wetness value (ranges from 0-1). Final value is clamped to 0-1. Increasing this will mostly affect damper weather states"),
				1.8f
			};

			// --

			variable render_emissive_surfaces_using_shaders =
			{
				"render_emissive_surfaces_using_shaders",
				("Enabling this will render all surfaces using a shader ending on 'emissivenight/emissive/strong' via shaders instead of fixed function."),
				true
			};

			variable assign_decal_category_to_emissive_surfaces =
			{
				"assign_decal_category_to_emissive_surfaces",
				("This automatically assigns the 'Decal' texture category (remix) to every surface using a shader ending on 'emissivenight/emissive/strong'\n"
				 "# This reduces flickering and z-fighting."),
				true
			};

			variable emissive_night_surfaces_emissive_scalar =
			{
				"emissive_night_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'emissivenight.fxc'"),
				0.2f
			};

			variable emissive_surfaces_emissive_scalar =
			{
				"emissive_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'emissive.fxc'"),
				0.8f
			};

			variable emissive_strong_surfaces_emissive_scalar =
			{
				"emissive_strong_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'strong.fxc'"),
				0.4f
			};

			// --

			variable decal_dirt_shader_usage =
			{
				"handle_decal_dirt_shader",
				("Enable decal_dirt shader logic. Runtime will use 'rtx_comp/textures/decal_dirt.png' in texture slot 0 and the games intensity/alpha mask in slot 1."
				"# This allows remixing the dirt texture while the game is handling the blending."),
				true
			};

			variable decal_dirt_shader_scalar =
			{
				"decal_dirt_shader_scalar",
				("Scale decal_dirt shader strength"),
				0.5f
			};

			variable decal_dirt_shader_contrast =
			{
				"decal_dirt_shader_contrast",
				("Mask contrast of decal_dirt shader"),
				1.0f
			};

			// --

			variable gta_rmptfx_litsprite_alpha_scalar =
			{
				"gta_rmptfx_litsprite_alpha_scalar",
				("Scale alpha of gta_rmptfx_litsprite"),
				40.0f
			};
		};
	
		static inline var_definitions vars = {};
	};
}