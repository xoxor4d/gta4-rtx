#include "std_include.hpp"
#include "game_lights.hpp"

#include "comp_settings.hpp"
#include "imgui.hpp"
#include "remix_lights.hpp"
#include "remix_vars.hpp"
#include "timecycle.hpp"
#include "shared/common/remix_api.hpp"

namespace gta4
{
	namespace
	{
		// where a channel of a sun key takes its value from
		enum class sun_key_source : int
		{
			follow = 0, // game's directional light live
			sample = 1, // game's directional light frozen when the clock passes the key (live until then)
			value = 2,  // number of the key
		};

		// one point of the atmos sun's path through the day
		struct sun_key_s
		{
			float time; // hours, 0..24
			sun_key_source elevation_source;
			float elevation;
			sun_key_source rotation_source;
			float rotation;
		};

		// The game's directional light is the sun by day and the moon by night and never reaches the horizon: 
		// around 18:30 it stops descending at ~30 deg, rotates and rises again as the moon. 
		// The atmos sun follows it by day and takes this path through the horizon in the evening and the morning (the atmos system needs it at -90 deg by night). 
		// Between two keys the elevation lerps, the rotation takes the shortest arc; two 'follow' keys in a row follow the game light.
		constexpr sun_key_s SUN_KEYS_DEFAULT[] =
		{
			{  8.00f, sun_key_source::follow,   0.0f, sun_key_source::follow,   0.0f },
			{ 18.50f, sun_key_source::follow,   0.0f, sun_key_source::follow,   0.0f },
			{ 20.00f, sun_key_source::value,    9.0f, sun_key_source::value,  270.0f },
			{ 21.00f, sun_key_source::value,    0.0f, sun_key_source::value,  270.0f },
			{ 21.50f, sun_key_source::value,   -4.0f, sun_key_source::value,  270.0f },
			{ 23.00f, sun_key_source::value,  -90.0f, sun_key_source::value,  270.0f },
			{  3.00f, sun_key_source::value,  -90.0f, sun_key_source::follow,   0.0f },
			{  5.00f, sun_key_source::value,   -2.0f, sun_key_source::follow,   0.0f },
			{  6.00f, sun_key_source::value,    8.0f, sun_key_source::follow,   0.0f },
		};

		struct sun_sample_s
		{
			bool taken = false;
			float elevation = 0.0f;
			float rotation = 0.0f;
		};

		struct sun_track_s
		{
			std::vector<sun_key_s> keys;       // sorted by time
			std::vector<sun_sample_s> samples; // one per key
			float last_time = -1.0f;           // clock of the last evaluation, keys passed since then take their sample
		};

		// the atmos sun of one evaluation
		struct sun_state_s
		{
			float elevation = 0.0f;
			float rotation = 0.0f;
			std::size_t key_a = 0u; // the segment the time falls in
			std::size_t key_b = 0u;
			float blend = 0.0f;
		};

		/// keys and their samples in time order
		void sort_sun_track(sun_track_s& track)
		{
			std::vector<std::size_t> order(track.keys.size());
			for (std::size_t i = 0u; i < order.size(); i++) {
				order[i] = i;
			}

			std::stable_sort(order.begin(), order.end(), [&track](const std::size_t a, const std::size_t b) {
				return track.keys[a].time < track.keys[b].time;
			});

			std::vector<sun_key_s> keys;
			std::vector<sun_sample_s> samples;

			for (const auto i : order)
			{
				keys.push_back(track.keys[i]);
				samples.push_back(track.samples[i]);
			}

			track.keys = std::move(keys);
			track.samples = std::move(samples);
		}

		sun_track_s default_sun_track()
		{
			sun_track_s track;
			track.keys.assign(std::begin(SUN_KEYS_DEFAULT), std::end(SUN_KEYS_DEFAULT));
			track.samples.assign(track.keys.size(), {});
			sort_sun_track(track);
			return track;
		}

		/// hours from one clock time forward to another (wraps at midnight)
		float hours_between(const float from, const float to) {
			return std::fmod(to - from + 48.0f, 24.0f);
		}

		/// shortest arc between two headings
		float lerp_rotation(const float a, const float b, const float t)
		{
			const float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
			return std::fmod(a + d * t + 720.0f, 360.0f);
		}

		float sun_channel(const sun_key_source source, const float value, const bool sampled, const float sampled_value, const float live)
		{
			switch (source)
			{
				case sun_key_source::value:		return value;
				case sun_key_source::sample:	return sampled ? sampled_value : live;
				default:						return live;
			}
		}

		/// the atmos sun at clock time t for the game light's elevation / rotation
		/// @param take_samples the clock really is t (a preview must not freeze samples or advance the track)
		sun_state_s evaluate_sun_track(sun_track_s& track, const float t, const float live_elevation, const float live_rotation, const bool take_samples)
		{
			sun_state_s s = {
				.elevation = live_elevation, 
				.rotation = live_rotation
			};

			const auto n = track.keys.size();

			if (!n) {
				return s;
			}

			if (take_samples)
			{
				if (track.last_time >= 0.0f)
				{
					const float since = hours_between(track.last_time, t);
					for (std::size_t i = 0u; i < n; i++)
					{
						const float until_key = hours_between(track.last_time, track.keys[i].time);

						if (until_key > 0.0f && until_key <= since) {
							track.samples[i] = { .taken = true, .elevation = live_elevation, .rotation = live_rotation };
						}
					}
				}

				track.last_time = t;
			}

			// the segment: the last key at or before t, before the first key of the day it is the last key of the day
			std::size_t a = n - 1u;
			for (std::size_t i = 0u; i < n; i++)
			{
				if (track.keys[i].time <= t) {
					a = i;
				}
			}

			const std::size_t b = (a + 1u) % n;
			const auto& ka = track.keys[a];
			const auto& kb = track.keys[b];
			const float length = hours_between(ka.time, kb.time);
			const float blend = (a == b || length <= 0.0f) ? 0.0f : std::clamp(hours_between(ka.time, t) / length, 0.0f, 1.0f);

			const auto& sa = track.samples[a];
			const auto& sb = track.samples[b];
			const float ea = sun_channel(ka.elevation_source, ka.elevation, sa.taken, sa.elevation, live_elevation);
			const float eb = sun_channel(kb.elevation_source, kb.elevation, sb.taken, sb.elevation, live_elevation);
			const float ra = sun_channel(ka.rotation_source, ka.rotation, sa.taken, sa.rotation, live_rotation);
			const float rb = sun_channel(kb.rotation_source, kb.rotation, sb.taken, sb.rotation, live_rotation);

			s.elevation = std::lerp(ea, eb, blend);
			s.rotation = lerp_rotation(ra, rb, blend);
			s.key_a = a;
			s.key_b = b;
			s.blend = blend;

			return s;
		}

		sun_track_s g_sun_track = default_sun_track(); // the keys in use, edited by the dev tab

		// the last evaluation, for the dev tab
		struct sun_debug_s
		{
			bool valid = false;
			float t = 0.0f;
			float game_elevation = 0.0f;
			float game_rotation = 0.0f;
			sun_state_s state;
		};
		sun_debug_s g_sun_debug;
	}

#pragma region LIGHT_IDS

	/// Stable identities for game lights that move.
	/// The scene light list holds no owner info and the hash of a light includes its position, so a moving light gets a new
	/// hash every frame. Hooks around known light sources set the identity of the light that is being added and the copy
	/// into the scene light list writes it into the unused tail of the entry (CLightSource::mStableId / mInstanceId).
	namespace light_ids
	{
		struct scope_s
		{
			std::uint32_t stable_id = 0u;
			std::uint32_t instance_id = 0u;
		};

		scope_s g_scope; // identity of the light that is being added, game update thread only

		std::mutex g_names_mtx;
		std::unordered_map<std::uint32_t, std::string> g_names; // stable id -> readable source, for the gui

		std::uint32_t hash_lowercase(std::uint32_t hash, const char* str)
		{
			for (; *str; ++str) {
				hash = (hash ^ static_cast<std::uint8_t>(std::tolower(static_cast<std::uint8_t>(*str)))) * 16777619u;
			}
			return hash;
		}

		template <typename make_name_fn>
		void register_name(const std::uint32_t id, make_name_fn&& make_name)
		{
			std::lock_guard lock(g_names_mtx);
			if (!g_names.contains(id)) {
				g_names.emplace(id, make_name());
			}
		}

		/// replaces CLightSource::operator= of the two copies into the scene light list
		game::CLightSource* __fastcall scene_light_list_assign_hk(game::CLightSource* dst, [[maybe_unused]] void* edx, const game::CLightSource* src)
		{
			std::memcpy(dst, src, offsetof(game::CLightSource, mStableId));
			dst->mStableId = g_scope.stable_id;
			dst->mInstanceId = g_scope.instance_id;
			return dst;
		}

		// ----- cutscene lights: cutscene name + name of the light in the [LIGHTS] block of the .cut file

		constexpr std::uint32_t CUTSCENE_LIGHT_NAME_OFFSET = 4u; // section light entry: { CCutsceneAnimation* anim; char name[32]; }
		std::uint32_t g_cutscene_light_entry_offset = 0u; // offset of the light's entry within its cutscene section

		void cutscene_add_light_hk(int unk_flag, game::eLightType type, int flag, float* dir, float* otherdir, float* pos, float* color, float intensity, int tex_hash, int txd_hash, float radius, float inner_cone, float outer_cone, int inter_index, int room_index, int shadow_rel_index)
		{
			if (game::ms_cCutsceneName && game::ms_CutsceneSections && game::ms_iCutsceneSectionPlaying)
			{
				if (const auto section = game::ms_CutsceneSections[*game::ms_iCutsceneSectionPlaying]; section)
				{
					const char* cutscene_name = game::ms_cCutsceneName;
					const char* light_name = reinterpret_cast<const char*>(section + g_cutscene_light_entry_offset + CUTSCENE_LIGHT_NAME_OFFSET);

					auto id = hash_lowercase(2166136261u, cutscene_name);
					id = hash_lowercase(id, "/");
					id = hash_lowercase(id, light_name);
					id = id ? id : 1u;

					register_name(id, [&] { return std::format("cutscene {} / {}", cutscene_name, light_name); });
					g_scope = { .stable_id = id, .instance_id = 0u };
				}
			}

			game::AddSceneLight(unk_flag, type, flag, dir, otherdir, pos, color, intensity, tex_hash, txd_hash, radius, inner_cone, outer_cone, inter_index, room_index, shadow_rel_index);
			g_scope = {};
		}

		__declspec (naked) void cutscene_add_light_stub()
		{
			__asm
			{
				mov		g_cutscene_light_entry_offset, esi;
				jmp		cutscene_add_light_hk;
			}
		}

		// ----- vehicle lights: vehicle model + light slot, one instance per vehicle

		enum class vehicle_light : int
		{
			headlight_left = 1,
			headlight_right,
			headlight_single,
			rearlight_left,
			rearlight_right,
			rearlight_single,
		};

		constexpr const char* VEHICLE_LIGHT_NAMES[] = { "", "headlight left", "headlight right", "headlight single", "rearlight left", "rearlight right", "rearlight single" };

		game::CEntity* g_vehicle = nullptr; // vehicle whose lights are being updated

		/// gives the lights added within its lifetime the identity of a light slot of 'g_vehicle'
		struct vehicle_light_scope
		{
			explicit vehicle_light_scope(const vehicle_light slot) {
				set(slot);
			}

			void set(const vehicle_light slot) const
			{
				g_scope = {};

				if (!g_vehicle || g_vehicle->m_wModelIndex < 0 || !game::g_modelPointers) {
					return;
				}

				const auto model = game::g_modelPointers[g_vehicle->m_wModelIndex];
				if (!model) {
					return;
				}

				auto id = shared::utils::hash32_combine(static_cast<std::uint32_t>(model->m_dwModel), static_cast<int>(slot));
				id = id ? id : 1u;

				register_name(id, [&] { return std::format("vehicle model {:#x} / {}", static_cast<std::uint32_t>(model->m_dwModel), VEHICLE_LIGHT_NAMES[static_cast<int>(slot)]); });
				g_scope = { .stable_id = id, .instance_id = reinterpret_cast<std::uint32_t>(g_vehicle) };
			}

			~vehicle_light_scope() {
				g_scope = {};
			}

			vehicle_light_scope(const vehicle_light_scope&) = delete;
			vehicle_light_scope& operator=(const vehicle_light_scope&) = delete;
		};

		DETOUR_TYPEDEF(vehicle_do_lights, char, __fastcall, game::CEntity* vehicle, void* edx, float* a2, char a3);
		char __fastcall vehicle_do_lights_hk(game::CEntity* vehicle, void* edx, float* a2, char a3)
		{
			g_vehicle = vehicle;
			const auto result = vehicle_do_lights_og(vehicle, edx, a2, a3);
			g_vehicle = nullptr;
			return result;
		}
	}

#pragma endregion

#pragma region LIGHT_CULLING

	/// Skips lights that are too far away and too dim to matter.
	/// The score of a light is the irradiance it delivers at the camera (game units). Lights the game added because they touch
	/// the view frustum are held to a lower threshold than lights that are only there because of 'nocull_dist_lights'.
	namespace light_culling
	{
		constexpr float ACTIVE_LIGHT_THRESHOLD_SCALE = 0.8f; // translated lights stay until they drop a bit further, so nothing flickers

		struct context_s
		{
			bool enabled = false;
			Vector cam_org;
			const float* frustum_planes = nullptr; // 6 planes, 4 floats each
			float min_dist_sqr = 0.0f;
			float threshold_in_view = 0.0f;
			float threshold_out_of_view = 0.0f;
		};

		context_s setup()
		{
			context_s ctx;
			const auto gs = comp_settings::get();
			const auto vp = game::pViewports;

			ctx.threshold_in_view = gs->translate_game_lights_cull_threshold_in_view._float();
			ctx.threshold_out_of_view = gs->translate_game_lights_cull_threshold_out_of_view._float();

			if (gs->translate_game_lights_cull._bool() && (ctx.threshold_in_view > 0.0f || ctx.threshold_out_of_view > 0.0f)
				&& vp && vp->sceneviewport && !imgui::get()->m_dbg_visualize_api_light_hashes)
			{
				// the scene viewport is a 0x10 byte header followed by the grcViewport
				const auto grc_viewport = reinterpret_cast<const game::grcViewport*>(reinterpret_cast<const std::uint8_t*>(vp->sceneviewport) + 0x10u);
				const float min_dist = gs->translate_game_lights_cull_min_distance._float();

				ctx.enabled = true;
				ctx.cam_org = Vector(&vp->sceneviewport->cameraInv.m[3][0]);
				ctx.frustum_planes = grc_viewport->frustumClipPlane0;
				ctx.min_dist_sqr = min_dist * min_dist;
			}

			return ctx;
		}

		bool touches_frustum(const float* planes, const Vector& pos, const float radius)
		{
			for (auto i = 0u; i < 6u; i++, planes += 4)
			{
				if (planes[0] * pos.x + planes[1] * pos.y + planes[2] * pos.z + planes[3] + radius < 0.0f) {
					return false;
				}
			}

			return true;
		}

		bool should_cull(const context_s& ctx, const game::CLightSource& def, const bool is_active)
		{
			const float dist_sqr = ctx.cam_org.DistToSqr(def.mPosition);
			if (dist_sqr < ctx.min_dist_sqr) {
				return false;
			}

			float threshold = touches_frustum(ctx.frustum_planes, def.mPosition, def.mRadius) ? ctx.threshold_in_view : ctx.threshold_out_of_view;
			if (threshold <= 0.0f) {
				return false;
			}

			if (is_active) {
				threshold *= ACTIVE_LIGHT_THRESHOLD_SCALE;
			}

			const float score = def.mIntensity * std::max({ def.mColor.x, def.mColor.y, def.mColor.z }) * def.mRadius * def.mRadius / dist_sqr;
			return score < threshold;
		}
	}

#pragma endregion

	std::string game_lights::get_light_source_name(const std::uint64_t hash)
	{
		std::lock_guard lock(light_ids::g_names_mtx);
		const auto it = light_ids::g_names.find(static_cast<std::uint32_t>(hash));
		return it != light_ids::g_names.end() ? it->second : std::string();
	}

	uint64_t calculate_light_hash(const game::CLightSource& def)
	{
		std::uint32_t hash = 0u;

		hash = shared::utils::hash32_combine(hash, def.mPosition.x);
		hash = shared::utils::hash32_combine(hash, def.mPosition.y);
		hash = shared::utils::hash32_combine(hash, def.mPosition.z);
		hash = shared::utils::hash32_combine(hash, def.mFlags);
		hash = shared::utils::hash32_combine(hash, def.mRoomIndex);
		//hash = shared::utils::hash32_combine(hash, def.mInteriorIndex); // this changes when starting a new game from an old save
		hash = shared::utils::hash32_combine(hash, def.mTxdHash);

		//hash = shared::utils::hash32_combine(hash, def.mVolumeSize);
		//hash = shared::utils::hash32_combine(hash, def.mVolumeScale);
		//hash = shared::utils::hash32_combine(hash, def.mIntensity);

		return hash;
	}

	/// exact comparison of everything that ends up in the translated light, for lights that are matched by a stable id
	bool compare_dynamic_light(const game::CLightSource& l1, const game::CLightSource& l2)
	{
		return l1.mPosition.x == l2.mPosition.x && l1.mPosition.y == l2.mPosition.y && l1.mPosition.z == l2.mPosition.z
			&& l1.mDirection.x == l2.mDirection.x && l1.mDirection.y == l2.mDirection.y && l1.mDirection.z == l2.mDirection.z
			&& l1.mColor.x == l2.mColor.x && l1.mColor.y == l2.mColor.y && l1.mColor.z == l2.mColor.z
			&& l1.mIntensity == l2.mIntensity && l1.mRadius == l2.mRadius
			&& l1.mInnerConeAngle == l2.mInnerConeAngle && l1.mOuterConeAngle == l2.mOuterConeAngle
			&& l1.mVolumeScale == l2.mVolumeScale && l1.mType == l2.mType;
	}

	bool compare_dynamic_light_without_position(const game::CLightSource& l1, const game::CLightSource& l2, float eps = 1.e-6f)
	{
		if (shared::utils::float_equal(l1.mIntensity, l2.mIntensity, eps)) 
		{
			if (shared::utils::float_equal(l1.mInnerConeAngle, l2.mInnerConeAngle, eps))
			{
				if (shared::utils::float_equal(l1.mRadius, l2.mRadius, eps)) {
					return true;
				}
			}
		}

		return false;
	}

	void game_lights::draw_debug()
	{
		const auto im = imgui::get();
		if (im->m_dbg_visualize_api_lights)
		{
			Vector player_pos;
			player_pos = game::FindPlayerCentreOfWorld(&player_pos);

			game::CLightSource* list = game::get_renderLights();
			const auto count = game::get_renderLightsCount();

			for (auto i = 0u; i < count; i++)
			{
				auto& def = list[i];

				const Vector circle_pos = def.mPosition;
				if (fabs(circle_pos.DistToSqr(player_pos)) > im->m_dbg_visualize_api_lights_3d_distance * im->m_dbg_visualize_api_lights_3d_distance) {
					continue;
				}

				const float radius = def.mRadius * (comp_settings::get()->translate_game_light_radius_scalar.get_as<float>() * 0.01f);
				auto& remixapi = shared::common::remix_api::get();

				remixapi.add_debug_circle(circle_pos, Vector(0.0f, 0.0f, 1.0f), radius, radius * 0.5f, def.mColor, false);
				remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 0, 90), Vector(1.0f, 1.0f, 1.0f));
				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(0, 90, 0), Vector(1.0f, 1.0f, 1.0f));
				//remixapi.add_debug_circle_based_on_previous(circle_pos, Vector(90, 0, 90), Vector(1.0f, 1.0f, 1.0f));
			}
		}
	}


	
	/// Adds an actual game light via 'game::AddSceneLight' which is later translated to a remixApi light
	/// @param type				sphere/spot
	/// @param pos				position of light	
	/// @param color 			color of light (game scale)
	/// @param intensity 		intensity of light (game scale)
	/// @param radius 			radius of light (game scale)
	/// @param inner_cone 		inner_cone of light (game scale)
	/// @param outer_cone 		outer_cone of light (game scale)
	/// @param dir 				direction of light - spot light only (game scale)
	/// @param always_update 	update translated remix light every frame - code tries to re-use lights of previous frames when false
	void add_game_light(const game::eLightType& type, Vector pos, Vector color, const float& intensity, const float& radius, const float& inner_cone = 0.0f, const float& outer_cone = 0.0f, Vector dir = { 0, 1, 0 }, bool always_update = true)
	{
		game::AddSceneLight(0, type,
			always_update ? 0x400 : 0x0, // constantly update lights with 0x400 flag (remix_lights.cpp)
			&dir.x, &dir.x, &pos.x, &color.x,
			intensity, 0, 0, radius, inner_cone, outer_cone, 0, 0, 0);
	}


	/// Iterate all game lights this frame 
	void game_lights::iterate_all_game_lights()
	{
		GTA4_PERF_SCOPE(performance_section::GameLights);
		const auto im = imgui::get();
		const auto gs = comp_settings::get();
		const auto rml = remix_lights::get();
		const auto& api = shared::common::remix_api::get();

		auto& active_lights = rml->get_active_lights();
		const auto& updateframe = rml->get_updateframe();
		g_sun_debug.valid = false;

		// sun without remix atmosphere system
		if ((!gs->timecycle_use_remix_atmos_system._bool() || im->m_dbg_force_distant_light_translation) && game::g_directionalLights)
		{
			auto& def = game::g_directionalLights[0];
			auto& l = rml->get_distant_light();

			if (l.m_handle)
			{
				api.m_bridge.DestroyLight(l.m_handle);
				l.m_handle = nullptr;
			}

			l.m_updateframe = updateframe;

			l.m_ext.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_DISTANT_EXT;
			l.m_ext.pNext = nullptr;

			auto dir = def.mDirection; dir.Normalize();
			l.m_ext.direction = dir.ToRemixFloat3D();

			l.m_ext.angularDiameterDegrees = gs->translate_sunlight_angular_diameter_degrees.get_as<float>();
			l.m_ext.volumetricRadianceScale = gs->translate_sunlight_volumetric_radiance_base.get_as<float>();

			if (gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_enabled.get_as<bool>())
			{
				l.m_ext.volumetricRadianceScale +=
					game::helper_timecycle_current_fog_density * gs->translate_sunlight_timecycle_fogdensity_volumetric_influence_scalar.get_as<float>();
			}

			// calculate time-based intensity multiplier for moonlight
			float intensity_multiplier = 1.0f;
			if (game::m_game_clock_hours && game::m_game_clock_minutes)
			{
				const int& hour = *game::m_game_clock_hours;
				const int& minute = *game::m_game_clock_minutes;
				const float moon_scalar = std::clamp(gs->translate_moonlight_intensity_scalar._float(), 0.0f, 1.0f);
				
				if (hour >= 22 || hour < 5) 
				{
					// full moon: 22:00-05:00
					intensity_multiplier = moon_scalar; 
				}
				else if (hour == 21)
				{
					// fade from sun to moon: 21:00-22:00
					const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 21:00, 1.0 at 22:00
					intensity_multiplier = 1.0f + fade_progress * (moon_scalar - 1.0f); // lerp from 1.0 to moon_scalar
				}
				else if (hour == 5)
				{
					// fade from moon to sun: 05:00-06:00
					const float fade_progress = static_cast<float>(minute) / 60.0f; // 0.0 at 05:00, 1.0 at 06:00
					intensity_multiplier = moon_scalar + fade_progress * (1.0f - moon_scalar); // lerp from moon_scalar to 1.0
				}
			}

			l.m_info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
			l.m_info.pNext = &l.m_ext;
			l.m_info.hash = shared::utils::string_hash64("apilight_distant");

			Vector rad = (def.mIntensity * Vector(def.mColor) * gs->translate_sunlight_intensity_scalar._float() * intensity_multiplier);

			const auto weather_factor = 1.0f - timecycle::get_bad_weather_factor() * std::clamp(gs->translate_sunlight_intensity_bad_weather_influence._float(), 0.0f, 1.0f);
			rad *= weather_factor;

			l.m_info.radiance = rad.ToRemixFloat3D();

			if (api.m_bridge.CreateLight(&l.m_info, &l.m_handle) == REMIXAPI_ERROR_CODE_SUCCESS && l.m_handle) {
				api.m_bridge.DrawLightInstance(l.m_handle);
			}
		}
		else if (game::g_directionalLights)
		{
			auto& def = game::g_directionalLights[0];
			auto dir = def.mDirection; dir.Normalize();
				 dir = -dir;

			const static auto rtx_atmosphere_sunElevation = remix_vars::get_option("rtx.atmosphere.sunElevation", "45");
			const static auto rtx_atmosphere_sunRotation = remix_vars::get_option("rtx.atmosphere.sunRotation", "0");
			const static auto rtx_atmosphere_moon0_elevation = remix_vars::get_option("rtx.atmosphere.moon0.elevation", "45");
			const static auto rtx_atmosphere_moon0_rotation = remix_vars::get_option("rtx.atmosphere.moon0.rotation", "0");

			if (rtx_atmosphere_sunElevation && rtx_atmosphere_sunRotation && rtx_atmosphere_moon0_elevation && rtx_atmosphere_moon0_rotation)
			{
				float game_sun_elevation = RAD2DEG(std::asin(dir.z));
				float game_sun_rotation = RAD2DEG(std::atan2(dir.x, dir.y));
				if (game_sun_rotation < 0.0f) {
					game_sun_rotation += 360.0f;
				}

				const int hour = *game::m_game_clock_hours;
				const int minute = *game::m_game_clock_minutes;
				const int seconds = *game::m_game_clock_seconds;
				const float t = static_cast<float>(hour) + static_cast<float>(minute) / 60.0f + static_cast<float>(seconds) / 3600.0f;

				// the game light turns into the moon in the evening, the sun track takes the atmos sun through the horizon instead
				const auto sun = evaluate_sun_track(g_sun_track, t, game_sun_elevation, game_sun_rotation, true);
				g_sun_debug = { .valid = true, .t = t, .game_elevation = game_sun_elevation, .game_rotation = game_sun_rotation, .state = sun };

				const auto& v1 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(sun.elevation));
				remix_vars::get()->add_queue_entry(rtx_atmosphere_sunElevation, v1);

				const auto& v2 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(sun.rotation));
				remix_vars::get()->add_queue_entry(rtx_atmosphere_sunRotation, v2);

				// moon: the game light itself, pushed below the horizon from its lowest point at 05:15 until 07:00 while the game
				// light rises again as the sun; its rotation is the atmos sun's
				static float moon_sunrise_5h15m_altitude = 0.0f;
				if (hour == 5 && minute == 15 && seconds < 2) {
					moon_sunrise_5h15m_altitude = game_sun_elevation;
				}

				if (hour >= 20 || hour <= 6)
				{
					constexpr auto t_moonset_start = 5.25f;
					constexpr auto t_moonset_end = 7.0f;

					float moon_elevation = game_sun_elevation;
					if (t >= t_moonset_start && t < t_moonset_end) {
						moon_elevation = std::lerp(moon_sunrise_5h15m_altitude, -15.0f, (t - t_moonset_start) / (t_moonset_end - t_moonset_start));
					}

					const auto& v3 = remix_vars::string_to_option_value(remix_vars::OPTION_TYPE_FLOAT, std::to_string(moon_elevation));
					remix_vars::get()->add_queue_entry(rtx_atmosphere_moon0_elevation, v3);
					remix_vars::get()->add_queue_entry(rtx_atmosphere_moon0_rotation, v2);
				}
			}
		}

		const auto light_list = game::get_renderLights();
		const auto light_count = game::get_renderLightsCount();

		const auto& ignored_lights = map_settings::get_map_settings().ignored_lights;
		const auto& allowed_lights = map_settings::get_map_settings().allow_lights;
		const auto& light_overrides = map_settings::get_map_settings().light_overrides;

		const auto cull = light_culling::setup();
		m_culled_light_count = 0u;

		if (gs->translate_game_lights.get_as<bool>() && light_count && light_list)
		{
			for (auto i = 0u; i < light_count; i++)
			{
				auto& def = light_list[i];

				// 'hash' identifies the light for map settings, 'key' the translated light (several lights can share a stable id)
				const bool has_stable_id = def.mStableId != 0u;
				const std::uint64_t hash = has_stable_id ? def.mStableId : calculate_light_hash(def);
				const std::uint64_t key = hash | (static_cast<std::uint64_t>(def.mInstanceId) << 32);

				bool is_filler_light = def.mFlags & 0x10;

				bool add_zero_intensity_light = false;
				bool is_allowed_filler_light = false;

				// debug setting to disable ignore logic (performance impact test)
				if (!im->m_dbg_disable_ignore_light_hash_logic)
				{
					if (ignored_lights.contains(hash))
					{
						// we need this light in the active list to visualize it
						if (im->m_dbg_visualize_api_light_hashes) {
							add_zero_intensity_light = true;
						} else {
							continue;
						}
					}
				}

				// dev setting to test light flags
				if (im->m_dbg_ignore_lights_with_flag_logic)
				{
					if (im->m_dbg_ignore_lights_with_flag_add_second_flag)
					{
						if (def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_01)
							&& def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_02)) {
							continue;
						}
					}
					else
					{
						if (def.mFlags & (1u << im->m_dbg_ignore_lights_with_flag_01)) {
							continue;
						}
					}
				}

				// ignore filler light game setting
				else if (gs->translate_game_lights_ignore_filler_lights.get_as<bool>())
				{
					if (is_filler_light)
					{
						// check if this filler light is whitelisted
						if (allowed_lights.contains(hash)) {
							is_allowed_filler_light = true;
						}

						// keep vis. working because the user needs to see the hashes to be able to whitelist them
						else if (im->m_dbg_visualize_api_light_hashes) {
							add_zero_intensity_light = true;
						}

						if (!(is_allowed_filler_light || add_zero_intensity_light)) {
							continue;
						}
					}
				}

				const auto it = active_lights.find(key);
				const bool is_active = it != active_lights.end();

				// an untouched light gets destroyed below
				if (cull.enabled && !add_zero_intensity_light && light_culling::should_cull(cull, def, is_active) && !light_overrides.contains(hash))
				{
					m_culled_light_count++;
					continue;
				}

				bool touched_light = false;
				if (is_active)
				{
					const auto always_update = it->second.m_def.mFlags & 0x400; 
					const auto custom_game_light = it->second.m_def.mFlags & 0x40000;

					// always update if in vis mode -- or light requests it (a light with a stable id is compared in full instead)
					bool should_update = im->m_dbg_visualize_api_light_hashes || (always_update && !has_stable_id);
					it->second.m_is_filler = is_filler_light;

					// check if most important properties changed - position unchanged as matched a hash
					if (!should_update && (has_stable_id ? compare_dynamic_light(def, it->second.m_def) : compare_dynamic_light_without_position(def, it->second.m_def))) {
						it->second.m_updateframe = updateframe; // light is up to date
					}
					else
					{
						it->second.m_def = def;
						should_update = true; // radius/intensity or something else has changed, update
					}

					if (should_update || add_zero_intensity_light)
					{
						if (add_zero_intensity_light)
						{
							it->second.m_def.mIntensity = 0.0f;
							it->second.m_is_ignored = true;
						} else {
							it->second.m_is_ignored = false;
						}

						it->second.m_is_allowed_filler = is_allowed_filler_light;
						rml->spawn_or_update_remix_sphere_light(it->second, true, custom_game_light);
					}

					touched_light = true;
				}
				else if (!has_stable_id && !(def.mFlags & 0x400))
				{
					// search for light with very very similar settings, then check if within a certain distance comp. to last state
					for (auto& l : active_lights)
					{
						// do not try to match an existing light if flag 0x400 (always update) or if it has its own identity
						if (l.second.m_def.mFlags & 0x400 || l.second.m_def.mStableId) {
							continue;
						}

						if (l.second.m_updateframe != updateframe && // do not recheck already updated lights
							compare_dynamic_light_without_position(def, l.second.m_def, 0.05f))
						{
							if (def.mPosition.DistToSqr(l.second.m_def.mPosition) < 1.0f) // expose this?
							{
								l.second.m_def = def;
								l.second.m_hash = hash; // can be used to check if a light changed its position
								rml->spawn_or_update_remix_sphere_light(l.second, true);
								touched_light = true;
								break;
							}
						}
					}
				}

				// this is a new light
				if (!touched_light) {
					rml->add_light(def, key, hash, is_filler_light, add_zero_intensity_light);
				}
			}
		}
		else {
			rml->destroy_and_clear_all_active_lights();
		}

		// delete all untouched lights
		for (auto it = active_lights.begin(); it != active_lights.end(); )
		{
			if (it->second.m_updateframe != updateframe)
			{
				rml->destroy_light(it->second);
				it = active_lights.erase(it);
			}
			else { ++it; }
		}
	}

	// --------------

	/// hooked function call that would normally render a singular headlight in the center until one of the two is defect
	/// we do not call the original center headlight function and instead call the singular one two times here. Once for with the left light pos and once with the right light pos.
	void veh_center_headlight(D3DXMATRIX* some_mtx, D3DXMATRIX* left_light_pos, D3DXMATRIX* right_light_pos, [[maybe_unused]] float* pos, float* light_dir, float* color, float intensity, float radius, [[maybe_unused]] std::int64_t ee, int inter_index, int room_index, int shadow_rel_index, char some_flag)
	{
		const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = 0.8f * (1.0f * 20.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC54) * *reinterpret_cast<float*>(0x12E23C8))
		const float outer_cone = 0.8f * (1.0f * 50.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC58) * *reinterpret_cast<float*>(0x12E23CC))

		light_ids::vehicle_light_scope light_scope(light_ids::vehicle_light::headlight_left);
		game::AddSingleVehicleLight(some_mtx, &left_light_pos->m[3][0],
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, 1, some_flag);

		light_scope.set(light_ids::vehicle_light::headlight_right);
		game::AddSingleVehicleLight(some_mtx, &right_light_pos->m[3][0],
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, 1, some_flag);

		/*shared::utils::hook::call<void(__cdecl)(float* _some_mtx, float* _headlight_pos, float* some_vec3, float* _a4, float _cc, float _dd, float _h1, float _h2, int _this_p0x48, int _this_p0x40, int _shadowRelIndex, char _one, char _some_flag)>
			(0xA3DE90)(some_mtx, &left_light_pos->m[3][0], light_dir, color, intensity, radius, inner_cone, outer_cone, inter_index, room_index, 0, 1, some_flag);*/
	}

	__declspec (naked) void veh_center_headlight_stub()
	{
		__asm
		{
			// we are at +100 here
			mov     ecx, [esp + 0x50]; // right_light_pos
			mov     eax, [esp + 0x54]; // left_light_pos
			//add     eax, 0x30; // 12th float (base matrix4x4) .. we push the ptr to the matrix

			push	ecx; // r
			push	eax; // l

			push    dword ptr[ebp + 0x24]; // some_mtx (overwritten with hk) .. normally at stack offs + 0x30

			call	veh_center_headlight;
			add     esp, 0x38; // normally + 0x30 but we added two additional pushes

			mov		eax, game::hk_addr__vehicle_center_headlight;
			add		eax, 11; // skip og add esp op 
			jmp		eax; //  0xA3FE19
		}
	}

	/// detoured single headlight func to apply same light settings as in dual mode
	void veh_single_headlight_hk(D3DXMATRIX* some_mtx, float* light_pos, float* light_dir, float* color, float intensity, float radius, [[maybe_unused]] float inner_cone_angle, [[maybe_unused]] float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = 0.8f * (1.0f * 20.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC54) * *reinterpret_cast<float*>(0x12E23C8))
		const float outer_cone = 0.8f * (1.0f * 50.0f); // *reinterpret_cast<float*>(0x103CC5C) * (*reinterpret_cast<float*>(0x103CC58) * *reinterpret_cast<float*>(0x12E23CC))

		const light_ids::vehicle_light_scope light_scope(light_ids::vehicle_light::headlight_single);
		game::AddSingleVehicleLight(some_mtx, light_pos,
			light_dir,
			im->m_dbg_custom_veh_headlight_enabled ? &im->m_dbg_custom_veh_headlight_color.x : color,
			intensity * gs->translate_vehicle_headlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_headlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	/// single center headlight to dual headlight
	void veh_center_rearlight(D3DXMATRIX* some_mtx, D3DXMATRIX* left_light_pos, D3DXMATRIX* right_light_pos, [[maybe_unused]] float* some_vec3, float* color, float intensity, float radius, float inner_cone_angle, float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = inner_cone_angle * 0.6f + gs->translate_vehicle_rearlight_inner_cone_angle_offset.get_as<float>();
		const float outer_cone = outer_cone_angle * 0.6f + gs->translate_vehicle_rearlight_outer_cone_angle_offset.get_as<float>();

		Vector light_dir = { 0.0f, -1.0f, 0.0f };
		light_dir += *gs->translate_vehicle_rearlight_direction_offset.get_as<Vector*>();

		light_ids::vehicle_light_scope light_scope(light_ids::vehicle_light::rearlight_left);
		game::AddSingleVehicleLight(some_mtx, &left_light_pos->m[3][0],
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);

		light_scope.set(light_ids::vehicle_light::rearlight_right);
		game::AddSingleVehicleLight(some_mtx, &right_light_pos->m[3][0],
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	__declspec (naked) void veh_center_rearlight_stub()
	{
		__asm
		{
			// we are at +0xA0 here
			mov     ecx, [esp + 0x58]; // right_light_pos
			mov     eax, [esp + 0x50]; // left_light_pos
			//add     eax, 0x30; // 12th float (base matrix4x4) .. we push the ptr to the matrix

			push	ecx; // r
			push	eax; // l

			push    dword ptr[ebp + 0x2C]; // some_mtx

			call	veh_center_rearlight;
			add     esp, 0x38; // normally + 0x34 but we omit one push (arg set to 0.0 when single light rendering) with the hook and add two additional pushes

			mov		eax, game::hk_addr__vehicle_center_rearlight;
			add		eax, 16; // skip og add esp op 
			jmp		eax; //  0xA4337E
		}
	}

	/// detoured single rearlight func to apply same light settings as in dual mode
	void veh_single_rearlight_hk(D3DXMATRIX* some_mtx, float* light_pos, [[maybe_unused]] float* og_light_dir, float* color, float intensity, float radius, float inner_cone_angle, float outer_cone_angle, int inter_index, int room_index, int shadow_rel_index, char flag1, char flag2)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		const float inner_cone = inner_cone_angle * 0.6f + gs->translate_vehicle_rearlight_inner_cone_angle_offset.get_as<float>();
		const float outer_cone = outer_cone_angle * 0.6f + gs->translate_vehicle_rearlight_outer_cone_angle_offset.get_as<float>();

		Vector light_dir = { 0.0f, -1.0f, 0.0f };
		light_dir += *gs->translate_vehicle_rearlight_direction_offset.get_as<Vector*>();

		const light_ids::vehicle_light_scope light_scope(light_ids::vehicle_light::rearlight_single);
		game::AddSingleVehicleLight(some_mtx, light_pos,
			&light_dir.x,
			color,
			intensity * gs->translate_vehicle_rearlight_intensity_scalar.get_as<float>(),
			radius * gs->translate_vehicle_rearlight_radius_scalar.get_as<float>(),
			inner_cone,
			outer_cone,
			inter_index, room_index, shadow_rel_index, flag1, flag2);
	}

	// -----

	/// handles "fake" sirenlight waay above the vehicle
	void veh_siren_fake_light_hk(int unk_flag, game::eLightType type, int flag, float* dir, float* otherdir, float* pos, float* color, float intensity, int tex_hash, int txd_hash, float radius, float inner_cone, float outer_cone, int inter_index, int room_index, int shadow_rel_index)
	{
		//const auto im = imgui::get();
		const auto gs = comp_settings::get();

		pos[2] += gs->translate_vehicle_fake_siren_z_offset._float();
		intensity += gs->translate_vehicle_fake_siren_intensity_offset._float();
		radius += gs->translate_vehicle_fake_siren_radius_offset._float();

		game::AddSceneLight(unk_flag, type, flag, dir, otherdir, pos, color, intensity, tex_hash, txd_hash, radius, inner_cone, outer_cone, inter_index, room_index, shadow_rel_index);
	}


	/// normally adds deferred/2d lights into the actual sirens - we create actual lights instead (AddProjectedLight)
	void veh_siren_vlights_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		float* light_direction)
	{
		const auto gs = comp_settings::get();

		Vector color =
		{
			(float)r / 255.0f,
			(float)g / 255.0f,
			(float)b / 255.0f
		};

		add_game_light(	gs->translate_vehicle_vsirens_make_spotlight._bool() ? game::eLightType::LT_SPOT : game::eLightType::LT_POINT, 
						pos, color,
						ems_scale + gs->translate_vehicle_vsirens_intensity_offset._float(),
						gs->translate_vehicle_vsirens_radius_offset._float(),
						0.0f, 45.0f, light_direction);

		if (gs->translate_vehicle_vsirens_secondary_spherelight_enabled._bool())
		{
			Vector new_pos = pos; // always add a slight offset so code recognizes it as a new light in case z offset gets set to 0
				   new_pos.z += (gs->translate_vehicle_vsirens_secondary_spherelight_z_offset._float() + 0.01f); 

			add_game_light(	game::eLightType::LT_POINT, new_pos, color,
							ems_scale + gs->translate_vehicle_vsirens_secondary_spherelight_intensity_offset._float(), 
							gs->translate_vehicle_vsirens_secondary_spherelight_radius_offset._float());
		}
	}


	/// normally adds deferred/2d lights into the actual sirens - we create actual lights instead
	void veh_bar_siren_vlights_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		[[maybe_unused]] float* light_direction /*nullptr*/)
	{
		const auto gs = comp_settings::get();

		const Vector color = { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f };
		add_game_light(game::eLightType::LT_POINT, pos, color, ems_scale * gs->translate_vehicle_barsirens_intensity_scalar._float(), maybe_radius * gs->translate_vehicle_barsirens_radius_scalar._float());
	}


	/// normally adds deferred/2d lights at rear window sirens - we create actual lights instead
	void veh_fbi_buffalo_siren_hk([[maybe_unused]] int unk2, byte r, byte g, byte b, float ems_scale, float* pos,
		[[maybe_unused]] float maybe_radius, // some gamesetting - does not seem to be dynamic
		[[maybe_unused]] float a9_160_0, [[maybe_unused]] float a10_0_2, [[maybe_unused]] float a11_0_0, [[maybe_unused]] float a12_2_0, // all hardcoded
		[[maybe_unused]] char a13, [[maybe_unused]] char a14,
		[[maybe_unused]] float* light_direction /*nullptr*/)
	{
		// note: can not create remixApi lights directly because this runs in the main thread and seems to be framerate dependent
		// drawing the light instances later in the same thread does not help either
		// so we create proper game lights which are translated to remixApi lights later

		const Vector color = { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f };
		add_game_light(	game::eLightType::LT_POINT, pos, color, 
						ems_scale * (0.6f /*+ im->m_debug_vector2.x*/), 
						maybe_radius * (0.05f /*+ im->m_debug_vector2.y*/));
	}

	void game_lights::devgui_atmos_sun()
	{
		auto& track = g_sun_track;
		const auto& d = g_sun_debug;
		static imgui::debug_plot plot_elevation;
		static imgui::debug_plot plot_rotation;
		static float preview_t = 12.0f;

		if (d.valid)
		{
			plot_elevation.add(d.state.elevation);
			plot_rotation.add(d.state.rotation);
			ImGui::Text("Clock %.2f h, game light: elevation %.2f, rotation %.2f", d.t, d.game_elevation, d.game_rotation);
			ImGui::Text("Atmos sun: elevation %.2f, rotation %.2f (keys %zu -> %zu, blend %.2f)", d.state.elevation, d.state.rotation, d.state.key_a, d.state.key_b, d.state.blend);
		} else {
			ImGui::TextDisabled("No atmos sun this frame (atmos system off, distant light forced or no directional light)");
		}

		constexpr float end_padding = 12.0f; // the dev tab's widgets keep off the right edge
		const float width = ImGui::GetContentRegionAvail().x - end_padding;
		const float plot_width = width * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;
		ImGui::PlotLines("##sun_elevation", plot_elevation.values, imgui::debug_plot::history_size, plot_elevation.offset, "elevation -90..90", -90.0f, 90.0f, ImVec2(plot_width, 60.0f));
		ImGui::SameLine();
		ImGui::PlotLines("##sun_rotation", plot_rotation.values, imgui::debug_plot::history_size, plot_rotation.offset, "rotation 0..360", 0.0f, 360.0f, ImVec2(plot_width, 60.0f));

		ImGui::SetNextItemWidth(plot_width);
		ImGui::SliderFloat("##sun_preview", &preview_t, 0.0f, 24.0f, "preview at %.2f h", ImGuiSliderFlags_AlwaysClamp);
		
		ImGui::SameLine();
		const auto preview = evaluate_sun_track(track, preview_t, d.game_elevation, d.game_rotation, false);
		ImGui::Text("elevation %.2f, rotation %.2f", preview.elevation, preview.rotation);
		ImGui::SetItemTooltipBlur("Evaluates the keys at that time with the game light of this frame for the follow / sample channels.");

		// the key editor
		constexpr const char* SOURCE_LABELS = "follow game\0sample game\0value\0";
		bool changed = false;
		std::size_t remove = SIZE_MAX;

		if (ImGui::BeginTable("##sun_keys", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX, ImVec2(width, 0.0f)))
		{
			ImGui::TableSetupColumn("Time (h)");
			ImGui::TableSetupColumn("Elevation from");
			ImGui::TableSetupColumn("Elevation (deg)");
			ImGui::TableSetupColumn("Rotation from");
			ImGui::TableSetupColumn("Rotation (deg)");
			ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			for (std::size_t i = 0u; i < track.keys.size(); i++)
			{
				auto& k = track.keys[i];
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::DragFloat("##time", &k.time, 1.0f / 60.0f, 0.0f, 24.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

				const auto source_combo = [](const char* id, sun_key_source& source)
				{
					int value = static_cast<int>(source);
					ImGui::SetNextItemWidth(-FLT_MIN);

					if (ImGui::Combo(id, &value, SOURCE_LABELS)) {
						source = static_cast<sun_key_source>(value);
					}
				};

				ImGui::TableNextColumn();
				source_combo("##elevation_source", k.elevation_source);

				ImGui::TableNextColumn();
				ImGui::BeginDisabled(k.elevation_source != sun_key_source::value);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##elevation", &k.elevation, 0.1f, -90.0f, 90.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndDisabled();

				ImGui::TableNextColumn();
				source_combo("##rotation_source", k.rotation_source);

				ImGui::TableNextColumn();
				ImGui::BeginDisabled(k.rotation_source != sun_key_source::value);
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::DragFloat("##rotation", &k.rotation, 0.5f, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::EndDisabled();

				ImGui::TableNextColumn();
				if (ImGui::SmallButton("x")) {
					remove = i;
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (remove != SIZE_MAX)
		{
			track.keys.erase(track.keys.begin() + static_cast<std::ptrdiff_t>(remove));
			track.samples.erase(track.samples.begin() + static_cast<std::ptrdiff_t>(remove));
		}

		if (changed) {
			sort_sun_track(track);
		}

		if (ImGui::Button("Add key at preview time"))
		{
			track.keys.push_back({ preview_t, sun_key_source::value, preview.elevation, sun_key_source::value, preview.rotation });
			track.samples.push_back({});
			sort_sun_track(track);
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset to defaults")) {
			track = default_sun_track();
		}

		ImGui::SameLine();
		if (ImGui::Button("Copy as code"))
		{
			constexpr const char* SOURCE_NAMES[] = { "sun_key_source::follow", "sun_key_source::sample", "sun_key_source::value" };
			std::string code;

			for (const auto& k : track.keys)
			{
				code += std::format("\t\t\t{{ {:5.2f}f, {}, {:6.1f}f, {}, {:6.1f}f }},\n", k.time,
					SOURCE_NAMES[static_cast<int>(k.elevation_source)], k.elevation, SOURCE_NAMES[static_cast<int>(k.rotation_source)], k.rotation);
			}

			ImGui::SetClipboardText(code.c_str());
		}
		ImGui::SetItemTooltipBlur("The key list as the SUN_KEYS_DEFAULT initializer (game_lights.cpp), to keep tuned values.");
	}

	game_lights::game_lights()
	{
		p_this = this;

		// draw two headlights instead of a singular center one
		shared::utils::hook::nop(game::hk_addr__vehicle_center_headlight, 8); // nop push so we start at stack+0x100 - 0xA3FE0E
		shared::utils::hook(game::hk_addr__vehicle_center_headlight, veh_center_headlight_stub, HOOK_JUMP).install()->quick();
		shared::utils::hook::nop(game::nop_addr__vehicle_headlight_prevent_override, 6); // overrides right light pos - 0xA3FCBB
		shared::utils::hook::nop(game::nop_addr__vehicle_headlight_prevent_read, 6); // reads overridden right light pos (prob. not needed) - 0xA3FCF2

		// also hook single light drawing because we adjust light parameters and we want single lights to have the same settings as the two above
		shared::utils::hook(game::hk_addr__vehicle_single_headlight, veh_single_headlight_hk, HOOK_CALL).install()->quick(); // single light mode if one is defect (both use the same func) - 0xA3FF0F

		// rear lights
		shared::utils::hook(game::hk_addr__vehicle_center_rearlight, veh_center_rearlight_stub, HOOK_JUMP).install()->quick(); // center light to two lights - 0xA4336E
		shared::utils::hook(game::hk_addr__vehicle_single_rearlight, veh_single_rearlight_hk, HOOK_CALL).install()->quick(); // single light mode if one is defect (both use the same func) - 0xA4342D

		// "fake" light ontop of v-shaped sirens
		shared::utils::hook(game::hk_addr__vehicle_vshaped_sirens_fake_light, veh_siren_fake_light_hk, HOOK_CALL).install()->quick(); // 0xA40D0A - police / ambulance
		shared::utils::hook(game::hk_addr__vehicle_firetruck_sirens_fake_light, veh_siren_fake_light_hk, HOOK_CALL).install()->quick(); // 0xA4126E - firetruck


		// replace defered/2d light inside sirens with actual lights
		shared::utils::hook(game::hk_addr__vehicle_vshaped_sirens_vlight, veh_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA40AAA - police / ambulance
		shared::utils::hook(game::hk_addr__vehicle_firetruck_sirens, veh_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA4100E - firetruck

		// replace defered/2d light of siren bars
		shared::utils::hook(game::hk_addr__vehicle_barshaped_sirens, veh_bar_siren_vlights_hk, HOOK_CALL).install()->quick(); // 0xA4146F
		shared::utils::hook(game::hk_addr__vehicle_fbibuffalo_sirens, veh_fbi_buffalo_siren_hk, HOOK_CALL).install()->quick(); // 0xA417DF - fbi rear projected lights



		// stable ids for moving lights
		if (game::hk_addr__scene_light_list_assign01 && game::hk_addr__scene_light_list_assign02)
		{
			shared::utils::hook(game::hk_addr__scene_light_list_assign01, light_ids::scene_light_list_assign_hk, HOOK_CALL).install()->quick(); // 0xABD2E0
			shared::utils::hook(game::hk_addr__scene_light_list_assign02, light_ids::scene_light_list_assign_hk, HOOK_CALL).install()->quick(); // 0xABD32C

			if (game::hk_addr__cutscene_add_light) {
				shared::utils::hook(game::hk_addr__cutscene_add_light, light_ids::cutscene_add_light_stub, HOOK_CALL).install()->quick(); // 0x9CAAB3
			}

			if (game::hk_addr__vehicle_do_lights) {
				shared::utils::hook::detour(game::hk_addr__vehicle_do_lights, &light_ids::vehicle_do_lights_hk, DETOUR_CAST(light_ids::vehicle_do_lights_og)); // 0xA435E0
			}
		}

		// -----
		m_initialized = true;
		shared::common::log("GameLights", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
