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

		static bool is_initialized()
		{
			if (p_this && p_this->m_initialized) {
				return true;
			}
			return false;
		}

		static void write_toml();
		static bool parse_toml();

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

				const auto desc_lines = shared::utils::split(std::string(this->m_desc), '\n');
				for (const auto& line : desc_lines) {
					out += "# " + line + "\n";
				}

				out += "# Type: " + std::string(this->get_str_type()) + " || Default: " + std::string(this->get_str_value(true));
				return out;
			}

			const bool& _bool(const bool default_value = false) const
			{
				assert(m_type == var_type_boolean && "Type mismatch: expected boolean");
				return !default_value ? m_var.boolean : m_var_default.boolean;
			}

			const bool* _bool_ptr(const bool default_value = false)
			{
				assert(m_type == var_type_boolean && "Type mismatch: expected boolean");
				return &(!default_value ? m_var.boolean : m_var_default.boolean);
			}

			const float& _float(const bool default_value = false) const
			{
				assert(m_type == var_type_value && "Type mismatch: expected float");
				return !default_value ? m_var.value[0] : m_var_default.value[0];
			}

			const float* _float_ptr(const bool default_value = false)
			{
				assert(m_type == var_type_value && "Type mismatch: expected float");
				return !default_value ? m_var.value : m_var_default.value;
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

		private:
			bool m_initialized = false;
	
		struct var_definitions
		{
			// ----------------------------------
			// remix related settings

			variable manual_game_resolution_enabled = {
				"manual_game_resolution_enabled",
				("Enabling this will override saved resolution settings\n"
				 "and use settings defined in 'manual_game_resolution'."),
				false
			};

			variable manual_game_resolution =
			{
				"manual_game_resolution",
				("Resolution override when 'manual_game_resolution_enabled' is enabled\n"
				 "Not required normally"),
				1920, 1080
			};

			variable load_colormaps_only = {
				"load_colormaps_only",
				("This setting will prevent loading and usage of all non-colormap textures.\n"
				 "Useful to declutter the remix UI and reducing used VRAM."),
				true
			};

			variable remix_override_rtxdi_samplecount = {
				"remix_override_rtxdi_samplecount",
				("Remix sets 'rtx.di.initialSampleCount' to hardcoded values on start.\n"
				 "Setting this value to anything greater 0 constantly sets the remix variable with this value."),
				30
			};


			// ----------------------------------
			// culling related settings

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
				500.0f
			};

			variable nocull_radius_far_static =
			{
				"nocull_radius_far_static",
				("The minimum radius an object has to have to not get culled within the distance set by 'nocull_dist_far_static'"),
				50.0f
			};

			variable nocull_height_far_static =
			{
				"nocull_height_far_static",
				("The minimum height an object has to have to not get culled within the distance set by 'nocull_dist_far_static'\n"
				"Setting this to 0 disables the condition."),
				13.0f
			};

			variable nocull_dist_lights =
			{
				"nocull_dist_lights",
				("Distance (radius around player) where culling of game lights is disabled."),
				20.0f
			};

			variable nocull_extended = {
				"nocull_extended",
				("Extended Anti Culling logic. Rechecks for manually added anti culling meshes added via mapsettings.\n"
				 "Does not prevent culling if part of the map containing the mesh 'unloads'"),
				true
			};


			// ----------------------------------
			// light translation related settings

			variable translate_game_lights =
			{
				"translate_game_lights",
				("This recreates game-lights as remixApi lights"),
				true
			};

			variable translate_game_lights_ignore_filler_lights =
			{
				"translate_game_lights_ignore_filler_lights",
				("This prevents translation of game-lights when they have a certain FLAG set.\n"
				 "Ignores a lot of filler lights but also disables emergency lights. It's recommended to mod the actual game files to remove unwanted lights."),
				false
			};

			variable translate_game_light_radius_scalar =
			{
				"translate_game_light_radius_scalar",
				("Scale radius of translated game lights"),
				0.5f
			};

			variable translate_game_light_intensity_scalar =
			{
				"translate_game_light_intensity_scalar",
				("Scale intensity of translated game lights"),
				200.0f
			};

			variable translate_game_light_softness_offset =
			{
				"translate_game_light_softness_offset",
				("Offset softness of translated game lights"),
				0.20f
			};

			variable translate_game_light_spotlight_volumetric_radiance_scale =
			{
				"translate_game_light_spotlight_volumetric_radiance_scale",
				("Volumetric scale of translated game spotlights"),
				1.0f
			};

			variable translate_game_light_spherelight_volumetric_radiance_scale =
			{
				"translate_game_light_spherelight_volumetric_radiance_scale",
				("Volumetric scale of translated game sphere lights"),
				1.0f
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

			variable translate_sunlight_angular_diameter_degrees =
			{
				"translate_sunlight_angular_diameter_degrees",
				("Angular Diameter of sunlight (Static value, not influenced by the game)"),
				0.45f
			};

			variable translate_sunlight_volumetric_radiance_base =
			{
				"translate_sunlight_volumetric_radiance_base",
				("Base volumetric scale of sunlight (Static value, not influenced by the game)\n"
				"The timecycle fogdensity setting can also influence the volumetric scale when enabled."),
				1.0f
			};

			variable translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled =
			{
				"translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled",
				("Enables influence of timecycle fogdensity setting on sunlight volumetric scale"),
				true
			};

			variable translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar =
			{
				"translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar",
				("Scale influence of fogdensity timecycle setting on volumetric scale of sunlight.\n"
				"( < translate_sunlight_volumetric_radiance_base >  +  < timecycle fogdensity (0-1) >  *  < this scalar >"),
				4.0f
			};

			// --

			variable translate_vehicle_headlight_intensity_scalar =
			{
				"translate_vehicle_headlight_intensity_scalar",
				("Scale intensity of vehicle headlights."),
				1.2f
			};

			variable translate_vehicle_headlight_radius_scalar =
			{
				"translate_vehicle_headlight_radius_scalar",
				("Scale radius of vehicle headlights."),
				1.0f
			};

			// --

			variable translate_vehicle_rearlight_intensity_scalar =
			{
				"translate_vehicle_rearlight_intensity_scalar",
				("Scale intensity of vehicle rearlights."),
				0.2f
			};

			variable translate_vehicle_rearlight_radius_scalar =
			{
				"translate_vehicle_rearlight_radius_scalar",
				("Scale radius of vehicle headlights."),
				4.0f
			};

			variable translate_vehicle_rearlight_inner_cone_angle_offset =
			{
				"translate_vehicle_rearlight_inner_cone_angle_offset",
				("Additional offset applied to the inner cone of the spotlight (can be negative)"),
				0.0f
			};

			variable translate_vehicle_rearlight_outer_cone_angle_offset =
			{
				"translate_vehicle_rearlight_outer_cone_angle_offset",
				("Additional offset applied to the outer cone of the spotlight (can be negative)"),
				-45.0f
			};

			variable translate_vehicle_rearlight_direction_offset =
			{
				"translate_vehicle_rearlight_direction_offset",
				("Additional offset applied to direction vector of the light"),
				0.0f, 0.0f, -0.2f
			};

			// ---

			variable translate_vehicle_fake_siren_z_offset =
			{
				"translate_vehicle_fake_siren_z_offset",
				("Z Offset applied to the fake siren light that is way above the vehicle"),
				-1.3f
			};

			variable translate_vehicle_fake_siren_intensity_offset =
			{
				"translate_vehicle_fake_siren_intensity_offset",
				("Intensity offset applied to the fake siren light that is way above the vehicle"),
				0.0f
			};

			variable translate_vehicle_fake_siren_radius_offset =
			{
				"translate_vehicle_fake_siren_radius_offset",
				("Radius offset applied to the fake siren light that is way above the vehicle"),
				0.0f
			};

			variable translate_vehicle_vsirens_make_spotlight =
			{
				"translate_vehicle_vsirens_make_spotlight",
				("Use spotlights on v-siren lights"),
				true
			};
		
			variable translate_vehicle_vsirens_intensity_offset =
			{
				"translate_vehicle_vsirens_intensity_offset",
				("Intensity offset (in game units) applied to the v-siren lights inside the actual sirens"),
				20.0f
			};
			variable translate_vehicle_vsirens_radius_offset =
			{
				"translate_vehicle_vsirens_radius_offset",
				("Radius offset (in game units) applied to the v-siren lights inside the actual sirens"),
				25.0f
			};

			// ----------------------------------
			// emissive related settings

			variable vehicle_lights_emissive_scalar =
			{
				"vehicle_lights_emissive_scalar",
				("Scale emissive strength of vehicle lights\n"
				"(< gta4 emissiveMultiplier shader constant >  *  < this scalar >  *  < remix material setting >  *  < global remix emissive setting >)"),
				13.0f
			};

			variable vehicle_lights_dual_render_proxy_texture =
			{
				"vehicle_lights_dual_render_proxy_texture",
				("This renders surfaces using the 'gta_vehicle_lightsemissive' shader a second time using a proxy texture (veh_light_ems_glass.png).\n"
				"The remix-mod of the compatibility mod makes that surface translucent."),
				false
			};

			variable emissive_night_surfaces_emissive_scalar =
			{
				"emissive_night_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'emissivenight.fxc'"),
				0.25f
			};

			variable emissive_surfaces_emissive_scalar =
			{
				"emissive_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'emissive.fxc'"),
				0.15f
			};

			variable emissive_strong_surfaces_emissive_scalar =
			{
				"emissive_strong_surfaces_emissive_scalar",
				("Scale emissive strength of every surface using a shader ending on 'strong.fxc'"),
				0.4f
			};

			variable emissive_generic_scale =
			{
				"emissive_generic_scale",
				("Some emissive surfaces do not use the emissiveMultiplier shader constant. These will use this constant."),
				1.6f
			};

			variable emissive_alpha_blend_hack = 
			{
				"emissive_alpha_blend_hack",
				("Assign WorldUI and DecalStatic to AlphaBlended Emissives.\n"
				 "Fixes aliasing induced by RR Particle Mode. Enables RR Particle Mode when on.\n"
				 "Disables RR Particle Mode when off (not recommended)"),
				true
			};

			variable emissive_alpha_blend_hack_scale =
			{
				"emissive_alpha_blend_hack_scale",
				("AlphaBlended Emissive Surface that are tagged as 'Decal' are less emissive\n"
				 "so we have to increase their emissive intensity to compensate for that."),
				4.0f
			};

			variable phone_emissive_override = {
				"phone_emissive_override",
				("Automatically tags phone related meshes as world-ui and adjusts the emissive scale.\n"
				 "Emissive intensity can be tweaked via 'phone_emissive_scalar'"),
				true
			};

			variable phone_emissive_scalar = {
				"phone_emissive_scalar",
				("Scales the emissive intensity of phone meshes. Needs 'phone_emissive_override'"),
				2.5f
			};

			// ----------------------------------
			// general rendering related settings

			variable vehicle_dirt_enabled =
			{
				"vehicle_dirt_enabled",
				("Enable dirt on vehicles. This renders the vehicle surface a second time\n"
				"using the dirt texture and applies it as a decal."),
				true
			};

			variable vehicle_dirt_custom_color_enabled =
			{
				"vehicle_dirt_custom_color_enabled",
				("Enable dirt color override on vehicles. The color constant of the game seems to be static.\n"
				"This option can be used to use a custom dirt color."),
				false
			};

			variable vehicle_dirt_custom_color =
			{
				"vehicle_dirt_custom_color",
				("Color used for vehicle dirt when 'vehicle_dirt_custom_color_enabled' is enabled."),
				0.22f, 0.21f, 0.20f
			};


			variable vehicle_dirt_expo =
			{
				"vehicle_dirt_expo",
				("Exponent applied to the dirtiness value so that smaller values do not make the vehicle dirty and rough as quickly."),
				3.0f
			};

			variable vehicle_dirt_roughness_z_normal =
			{
				"vehicle_dirt_roughness_z_normal",
				("Surfaces with a Z-Normal value above this will be influenced by adjusted roughness."),
				1.0f
			};

			variable vehicle_dirt_roughness_blending =
			{
				"vehicle_dirt_roughness_blending",
				("Defines the blending strength used to go from original roughness to adjusted roughness."),
				0.02f
			};

			// -----
			variable vehicle_livery_enabled =
			{
				"vehicle_livery_enabled",
				("Enable livery on vehicles. This renders the vehicle surface a second time\n"
				"using the livery texture and applies it as a decal."),
				true
			};

			// -----
			variable decal_dirt_shader_usage =
			{
				"handle_decal_dirt_shader",
				("Enable decal_dirt shader logic. Runtime will use 'rtx_comp/textures/decal_dirt.png' in texture slot 0 and the games intensity/alpha mask in slot 1.\n"
				"This allows remixing the dirt texture while the game is handling the blending."),
				false
			};

			variable decal_dirt_shader_scalar =
			{
				"decal_dirt_shader_scalar",
				("Scale decal_dirt shader strength"),
				0.25f
			};

			variable decal_dirt_shader_contrast =
			{
				"decal_dirt_shader_contrast",
				("Mask contrast of decal_dirt shader"),
				1.0f
			};

			// -----
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

			// -----
			variable npc_expensive_hair_alpha_testing =
			{
				"npc_expensive_hair_alpha_testing",
				("Alpha-test hair rendered with 'gta_hair_sorted_alpha_expensive' shader to make it look a little more like hair.\n"
				 "Not perfect and still WIP."),
				true
			};

			variable npc_expensive_hair_alpha_cutout_value =
			{
				"npc_expensive_hair_alpha_cutout_value",
				("Value used for ALPHAREF. Lower values might cause transparency issues while higher values will reduce hair visibility."),
				0.35f
			};

			// -----
			variable override_water_texture_hash =
			{
				"override_water_texture_hash",
				("This assigns the same texture hash to all water surfaces. Aids with water replacements."),
				true
			};

			// -----
			variable gta_rmptfx_litsprite_alpha_scalar =
			{
				"gta_rmptfx_litsprite_alpha_scalar",
				("Scale alpha of gta_rmptfx_litsprite"),
				1.0f
			};

			variable rain_particle_system_enabled =
			{
				"rain_particle_system_enabled",
				("Enable rain handled by a remix particle system. Has limited collision detection - can still rain 'inside' - WIP"),
				true
			};


			// ----------------------------------
			// timecycle related settings

			variable timecycle_set_on_endscene = {
				"timecycle_set_on_endscene",
				("Set timecycle related remix variables on EndScene. Set on BeginScene when false.\n"
				 "Using BeginScene might get values stuck while EndScene might cause some very minor flickering."),
				true
			};

			variable timecycle_wetness_enabled = {
				"timecycle_wetness_enabled",
				("Enables material roughness tweaks based on timecycle wetness settings."),
				true
			};

			variable timecycle_wetness_world_scalar =
			{
				"timecycle_wetness_world_scalar",
				("Scales the weather wetness value (ranges from 0-1). Final value is clamped to 0-1. Increasing this will mostly affect damper weather states"),
				2.4f
			};

			variable timecycle_wetness_world_offset =
			{
				"timecycle_wetness_world_offset",
				("Additional offset applied onto the final wetness value."),
				0.0f
			};

			variable timecycle_wetness_world_z_normal =
			{
				"timecycle_wetness_world_z_normal",
				("Surfaces with a Z-Normal value above this can get wet."),
				0.30f
			};

			variable timecycle_wetness_world_blending =
			{
				"timecycle_wetness_world_blending",
				("Defines the blending strength used to go from original roughness to adjusted roughness."),
				0.65f
			};

			variable timecycle_wetness_world_puddles_enable = {
				"timecycle_wetness_world_puddles_enable",
				("Enables Puddle logic on World Surfaces."),
				true
			};

			variable timecycle_wetness_world_raindrop_enable = {
				"timecycle_wetness_world_raindrop_enable",
				("Enables Raindrop logic on World Surfaces."),
				true
			};

			variable timecycle_wetness_world_raindrop_scalar =
			{
				"timecycle_wetness_world_raindrop_scalar",
				"Scale of raindrops on World Surfaces",
				0.23f
			};

			// -----

			variable timecycle_wetness_ped_raindrop_enable = {
				"timecycle_wetness_ped_raindrop_enable",
				("Enables Raindrop logic on Supported Ped Surfaces."),
				true
			};

			variable timecycle_wetness_ped_raindrop_scalar =
			{
				"timecycle_wetness_ped_raindrop_scalar",
				"Scale of raindrops on Supported Ped Surfaces",
				3.0f
			};

			// -----

			variable timecycle_wetness_vehicle_scalar =
			{
				"timecycle_wetness_vehicle_scalar",
				("Vehicle Roughness Scalar when it's wet.\n"
				 "0 = No Roughness, 1 = Original Roughness, > 1 increases the original roughness"),
				0.0f
			};

			variable timecycle_wetness_vehicle_z_normal =
			{
				"timecycle_wetness_vehicle_z_normal",
				("Surfaces with a Z-Normal value above this can get wet."),
				0.15f
			};

			variable timecycle_wetness_vehicle_blending =
			{
				"timecycle_wetness_vehicle_blending",
				("Defines the blending strength used to go from original roughness to adjusted roughness."),
				1.0f
			};


			variable timecycle_wetness_vehicle_raindrop_enable = {
				"timecycle_wetness_vehicle_raindrop_enable",
				("Enables Raindrop logic on Vehicles."),
				true
			};

			variable timecycle_wetness_vehicle_raindrop_scalar =
			{
				"timecycle_wetness_vehicle_raindrop_scalar",
				"Scale of raindrops on vehicles",
				1.6f
			};


			variable timecycle_wetness_vehicle_dirt_intensity_scalar =
			{
				"timecycle_wetness_vehicle_dirt_intensity_scalar",
				("Vehicle Dirt Intensity Scalar when it's wet.\n"
				 "0 = No Dirt to 1 = Original Dirt Amount"),
				0.8f
			};

			variable timecycle_wetness_vehicle_dirt_roughness_scalar =
			{
				"timecycle_wetness_vehicle_dirt_roughness_scalar",
				("Vehicle Dirt Roughness Scalar when it's wet.\n"
				 "0 = No Roughness, 1 = Original Roughness, > 1 increases the original roughness"),
				0.1f
			};

			variable timecycle_wetness_vehicle_dirt_z_normal =
			{
				"timecycle_wetness_vehicle_dirt_z_normal",
				("Surfaces with a Z-Normal value above this can get wet."),
				0.2f
			};

			variable timecycle_wetness_vehicle_dirt_blending =
			{
				"timecycle_wetness_vehicle_dirt_blending",
				("Defines the blending strength used to go from original roughness to adjusted roughness."),
				1.0f
			};

			// -----

			variable timecycle_fogcolor_enabled = {
				"timecycle_fogcolor_enabled",
				("Enables automatic adjustment of 'rtx.volumetrics.singleScatteringAlbedo' based on timecycle settings."),
				true
			};

			variable timecycle_fogcolor_base_strength = {
				"timecycle_fogcolor_base_strength",
				("Sets the base vector (rgb=val) of 'rtx.volumetrics.singleScatteringAlbedo'. Static offset not bound to any timecycle variable."),
				0.3f
			};

			variable timecycle_fogcolor_influence_scalar = {
				"timecycle_fogcolor_influence_scalar",
				("Controls how much the fogcolor timecycle variable influences 'rtx.volumetrics.singleScatteringAlbedo'"),
				1.0f
			};


			variable timecycle_fogdensity_enabled = {
				"timecycle_fogdensity_enabled",
				("Enables automatic adjustment of 'rtx.volumetrics.transmittanceMeasurementDistanceMeters' based on timecycle settings."),
				true
			};

			variable timecycle_fogdensity_influence_scalar = {
				"timecycle_fogdensity_influence_scalar",
				("Controls how much the fogdensity timecycle variable influences 'rtx.volumetrics.transmittanceMeasurementDistanceMeters'"),
				1.0f
			};


			variable timecycle_skyhorizonheight_enabled = {
				"timecycle_skyhorizonheight_enabled",
				("Enables automatic adjustment of 'rtx.volumetrics.atmosphereHeightMeters' based on timecycle settings.\n"
				"Also influences 'rtx.volumetrics.transmittanceMeasurementDistanceMeters' based on low/high transmittance offsets."),
				true
			};

			variable timecycle_skyhorizonheight_scalar = {
				"timecycle_skyhorizonheight_scalar",
				("Controls how much the horizon height timecycle variable influences 'rtx.volumetrics.atmosphereHeightMeters'.\n"
				"Final value is also used to offset 'transmittanceMeasurementDistanceMeters' based on atmospheric height."),
				1.2f
			};

			variable timecycle_skyhorizonheight_low_transmittance_offset = {
				"timecycle_skyhorizonheight_low_transmittance_offset",
				("Increase fog transmittance with higher skyhorizonheight values. Lowest offset that can be applied to 'transmittanceMeasurementDistanceMeters' based on atmospheric height.\n"
				"Higher values result in less fog."),
				0.0f
			};

			variable timecycle_skyhorizonheight_high_transmittance_offset = {
				"timecycle_skyhorizonheight_high_transmittance_offset",
				("Increase fog transmittance with higher skyhorizonheight values. Highest offset that can be applied to 'transmittanceMeasurementDistanceMeters' based on atmospheric height.\n"
				"Higher values result in less fog."),
				100.0f
			};

			// -----
			variable timecycle_skylight_enabled = {
				"timecycle_skylight_enabled",
				("Enables automatic adjustment of 'rtx.skyBrightness' based on timecycle settings."),
				true
			};

			variable timecycle_skylight_scalar = {
				"timecycle_skylight_scalar",
				("Controls how much the skylight timecycle variable influences 'rtx.skyBrightness'"),
				0.03f
			};

			// -----
			variable timecycle_colorcorrection_enabled = {
				"timecycle_colorcorrection_enabled",
				("Enables influence of color correction on 'rtx.tonemap.colorBalance' based on timecycle settings.\n"
				"Disabling this also disables color temperature influence."),
				true
			};

			variable timecycle_colorcorrection_influence = {
				"timecycle_colorcorrection_influence",
				("Controls how much the timecycle color correction variable influences 'rtx.tonemap.colorBalance'"),
				1.0f
			};

			variable timecycle_colortemp_enabled = {
				"timecycle_colortemp_enabled",
				("Enables influence of color temperature on 'rtx.tonemap.colorBalance'. NOT based on timecycle setting."),
				true
			};

			variable timecycle_colortemp_value = {
				"timecycle_colortemp_value",
				("Base colortemp value used for calculations."),
				11.2f
			};

			variable timecycle_colortemp_influence = {
				"timecycle_colortemp_influence",
				("Controls how much the timecycle_colortemp_value variable influences 'rtx.tonemap.colorBalance'"),
				0.15f
			};

			// -----
			variable timecycle_desaturation_enabled = {
				"timecycle_desaturation_enabled",
				("Enables automatic adjustment of 'rtx.tonemap.saturation' based on timecycle settings."),
				true
			};

			variable timecycle_desaturation_influence = {
				"timecycle_desaturation_influence",
				("Controls how much the desaturation timecycle variable influences 'rtx.tonemap.saturation'"),
				0.55f
			};

			variable timecycle_fardesaturation_influence = {
				"timecycle_fardesaturation_influence",
				("Controls how much the fardesaturation timecycle variable influences 'rtx.tonemap.saturation'."),
				0.06f
			};

			// -----
			variable timecycle_gamma_enabled = {
				"timecycle_gamma_enabled",
				("Enables automatic adjustment of 'rtx.tonemap.exposureBias' based on timecycle settings."),
				true
			};

			variable timecycle_gamma_offset = {
				"timecycle_gamma_offset",
				("Controls the offset that's added to the gamma timecycle variable which influences 'rtx.tonemap.exposureBias'."),
				0.3f
			};

			// -----
			variable timecycle_bloom_enabled = {
				"timecycle_bloomintensity_enabled",
				("Enables automatic adjustment of 'rtx.bloom.burnIntensity' and 'rtx.bloom.luminanceThreshold' based on timecycle settings."),
				true
			};

			variable timecycle_bloomintensity_scalar = {
				"timecycle_bloomintensity_scalar",
				("Scales the bloom intensity timecycle variable which influences 'rtx.tonemap.saturation'."),
				1.0f
			};

			variable timecycle_bloomthreshold_scalar = {
				"timecycle_bloomthreshold_scalar",
				("Scales the bloom threshold timecycle variable which influences 'rtx.tonemap.luminanceThreshold'."),
				1.0f
			};

		};
	
		static inline var_definitions vars = {};
	};
}