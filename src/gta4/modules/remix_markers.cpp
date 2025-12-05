#include "std_include.hpp"
#include "remix_markers.hpp"

#include "game_settings.hpp"
#include "imgui.hpp"
#include "map_settings.hpp"
#include "renderer.hpp"

namespace gta4
{
	struct vertex
	{
		D3DXVECTOR3 position; D3DCOLOR color; float tu, tv;
	};


	void draw_single_marker(IDirect3DDevice9* dev, uint32_t index, const Vector& pos, const Vector& rot, const Vector& scale)
	{
		const auto& renderer = renderer::get();

		const float f_index = static_cast<float>(index);
		const vertex mesh_verts[4] =
		{
			D3DXVECTOR3(-0.1337f - (f_index * 0.001f), -0.1337f - (f_index * 0.001f), 0), D3DCOLOR_XRGB(index, 0, 0), 0.0f, f_index / 100.0f,
			D3DXVECTOR3( 0.1337f + (f_index * 0.001f), -0.1337f - (f_index * 0.001f), 0), D3DCOLOR_XRGB(0, index, 0), f_index / 100.0f, 0.0,
			D3DXVECTOR3( 0.1337f + (f_index * 0.001f),  0.1337f + (f_index * 0.001f), 0), D3DCOLOR_XRGB(0, 0, index), 0.0f, f_index / 100.0f,
			D3DXVECTOR3(-0.1337f - (f_index * 0.001f),  0.1337f + (f_index * 0.001f), 0), D3DCOLOR_XRGB(index, 0, index), 0.0f, f_index / 100.0f,
		};

		D3DXMATRIX scale_matrix, rotation_x, rotation_y, rotation_z, mat_rotation, mat_translation, world;

		D3DXMatrixScaling(&scale_matrix, scale.x, scale.y, scale.z);
		D3DXMatrixRotationX(&rotation_x, rot.x); // pitch
		D3DXMatrixRotationY(&rotation_y, rot.y); // yaw
		D3DXMatrixRotationZ(&rotation_z, rot.z); // roll
		mat_rotation = rotation_z * rotation_y * rotation_x; // combine rotations (order: Z * Y * X)

		D3DXMatrixTranslation(&mat_translation, pos.x, pos.y, pos.z);
		world = scale_matrix * mat_rotation * mat_translation;

		// set remix texture hash ~req. dxvk-runtime changes - not really needed
		renderer->set_remix_texture_hash(dev, shared::utils::fnv::hash("marker", index));

		dev->SetTransform(D3DTS_WORLD, &world);
		dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, mesh_verts, sizeof(vertex));
	}

	void handle_rain_marker(IDirect3DDevice9* dev)
	{
		const auto gs = game_settings::get();

		if (gs->rain_particle_system_enabled._bool())
		{
			// save & restore after drawing
			IDirect3DVertexShader9* og_vs = nullptr;
			dev->GetVertexShader(&og_vs);
			dev->SetVertexShader(nullptr);

			IDirect3DBaseTexture9* og_tex = nullptr;
			dev->GetTexture(0, &og_tex);
			dev->SetTexture(0, tex_addons::white01);

			DWORD og_rs;
			dev->GetRenderState((D3DRENDERSTATETYPE)150, &og_rs);
			dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

			// ----

			if ((*game::weather_type_prev == game::eWeatherType::WEATHER_RAIN && *game::weather_change_value < 0.6f) || *game::weather_type_new == game::eWeatherType::WEATHER_RAIN
				|| (*game::weather_type_prev == game::eWeatherType::WEATHER_LIGHTNING && *game::weather_change_value < 0.6f) || *game::weather_type_new == game::eWeatherType::WEATHER_LIGHTNING)
			{
				const auto vp = game::pViewports;
				if (vp->sceneviewport)
				{
					Vector cam_org = &vp->sceneviewport->cameraInv.m[3][0];
					cam_org.z += (20.0f + (float)imgui::get()->m_dbg_int_01);
					draw_single_marker(dev, 9001, cam_org, Vector(0.0f, 0.0f, 0.0f), Vector(1.0f, 1.0f, 1.0f));
				}
			}

			// TODO: handle light-rain
			// if ((*game::weather_type_prev == game::eWeatherType::WEATHER_DRIZZLE && *game::weather_change_value < 0.4f) || (*game::weather_type_new == game::eWeatherType::WEATHER_DRIZZLE && *game::weather_change_value > 0.4f))

			// ----

			// restore
			dev->SetVertexShader(og_vs);
			dev->SetTexture(0, og_tex);
			renderer::dc_ctx.restore_render_state(dev, (D3DRENDERSTATETYPE)150);
			dev->SetFVF(NULL);
			dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);
		}
	}

	// draw map_setting marker meshes
	void remix_markers::draw_nocull_markers()
	{
		auto& msettings = map_settings::get_map_settings();
		const auto dev = game::get_d3d_device();

		// check for rain
		handle_rain_marker(dev);

		if (msettings.map_markers.empty()) {
			return;
		}

		Vector player_pos;
		player_pos = game::FindPlayerCentreOfWorld(&player_pos);

		// save & restore after drawing
		IDirect3DVertexShader9* og_vs = nullptr;
		dev->GetVertexShader(&og_vs);
		dev->SetVertexShader(nullptr);

		IDirect3DBaseTexture9* og_tex = nullptr;
		dev->GetTexture(0, &og_tex);
		dev->SetTexture(0, tex_addons::white01);

		DWORD og_rs;
		dev->GetRenderState((D3DRENDERSTATETYPE)150, &og_rs);
		dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);

		for (auto& m : msettings.map_markers)
		{
			const bool needs_vis_check =
				   m.weather_type != game::WEATHER_NONE
				|| m.from_hour >= 0 && m.to_hour >= 0
				|| m.cull_distance > 0.0f;

			if (needs_vis_check)
			{
				if (m.internal__frames_until_next_vis_check > 0u) {
					m.internal__frames_until_next_vis_check--;
				}
				else
				{
					m.internal__frames_until_next_vis_check = DISTANCE_CHECK_FRAME_INTERVAL; // reset

					bool spawned_on_weather = false;
					if (m.weather_type != game::WEATHER_NONE)
					{
						// not the target weather
						if (m.weather_type != *game::weather_type_new) {
							m.internal__is_hidden = true;
						}

						// target weather, check if transition value is larger than target value
						else if (*game::weather_type_prev == *game::weather_type_new
							|| *game::weather_change_value > m.weather_transition_value)
						{
							m.internal__is_hidden = false;
							spawned_on_weather = true;
						}
					}

					// only check time if weather has not triggered spawning
					if (!spawned_on_weather && (m.from_hour >= 0 && m.to_hour >= 0))
					{
						const auto& hour = *game::m_game_clock_hours;
						const bool wraps_midnight = m.from_hour > m.to_hour;

						if (!wraps_midnight) {
							m.internal__is_hidden = !(hour >= m.from_hour && hour < m.to_hour);
						}
						else {
							m.internal__is_hidden = !((hour >= m.from_hour) || (hour < m.to_hour));
						}
					}

					// cull dist last check
					if (m.cull_distance > 0.0f) {
						m.internal__is_hidden = fabs(m.origin.DistTo(player_pos)) > m.cull_distance;
					}
				}
			}

			if (m.internal__is_hidden) {
				continue;
			}

			draw_single_marker(dev, m.index, m.origin, m.rotation, m.scale);
		}

		// restore
		dev->SetVertexShader(og_vs);
		dev->SetTexture(0, og_tex);
		renderer::dc_ctx.restore_render_state(dev, (D3DRENDERSTATETYPE)150);
		dev->SetFVF(NULL);
		dev->SetTransform(D3DTS_WORLD, &shared::globals::IDENTITY);
	}

	remix_markers::remix_markers()
	{
		p_this = this;

		// -----
		m_initialized = true;
		shared::common::log("RemixMarkers", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
