#include "std_include.hpp"
#include "remix_markers.hpp"
#include "map_settings.hpp"
#include "renderer.hpp"

namespace gta4
{
	// draw map_setting marker meshes
	void remix_markers::draw_nocull_markers()
	{
		auto& msettings = map_settings::get_map_settings();
		const auto& renderer = renderer::get();
		const auto dev = game::get_d3d_device();

		struct vertex { D3DXVECTOR3 position; D3DCOLOR color; float tu, tv; };

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
			if (m.cull_distance > 0.0f)
			{
				if (m.internal__frames_until_next_vis_check > 0u) {
					m.internal__frames_until_next_vis_check--;
				}
				else
				{
					m.internal__is_hidden = fabs(m.origin.DistTo(player_pos)) > m.cull_distance;
					m.internal__frames_until_next_vis_check = DISTANCE_CHECK_FRAME_INTERVAL; // reset
				}
			}

			if (m.internal__is_hidden) {
				continue;
			}

			const float f_index = static_cast<float>(m.index);
			const vertex mesh_verts[4] =
			{
				D3DXVECTOR3(-0.1337f - (f_index * 0.001f), -0.1337f - (f_index * 0.001f), 0), D3DCOLOR_XRGB(m.index, 0, 0), 0.0f, f_index / 100.0f,
				D3DXVECTOR3( 0.1337f + (f_index * 0.001f), -0.1337f - (f_index * 0.001f), 0), D3DCOLOR_XRGB(0, m.index, 0), f_index / 100.0f, 0.0,
				D3DXVECTOR3( 0.1337f + (f_index * 0.001f),  0.1337f + (f_index * 0.001f), 0), D3DCOLOR_XRGB(0, 0, m.index), 0.0f, f_index / 100.0f,
				D3DXVECTOR3(-0.1337f - (f_index * 0.001f),  0.1337f + (f_index * 0.001f), 0), D3DCOLOR_XRGB(m.index, 0, m.index), 0.0f, f_index / 100.0f,
			};

			D3DXMATRIX scale_matrix, rotation_x, rotation_y, rotation_z, mat_rotation, mat_translation, world;

			D3DXMatrixScaling(&scale_matrix, m.scale.x, m.scale.y, m.scale.z);
			D3DXMatrixRotationX(&rotation_x, m.rotation.x); // pitch
			D3DXMatrixRotationY(&rotation_y, m.rotation.y); // yaw
			D3DXMatrixRotationZ(&rotation_z, m.rotation.z); // roll
			mat_rotation = rotation_z * rotation_y * rotation_x; // combine rotations (order: Z * Y * X)

			D3DXMatrixTranslation(&mat_translation, m.origin.x, m.origin.y, m.origin.z);
			world = scale_matrix * mat_rotation * mat_translation;

			// set remix texture hash ~req. dxvk-runtime changes - not really needed
			renderer->set_remix_texture_hash(dev, shared::utils::fnv::hash("marker", m.index));

			dev->SetTransform(D3DTS_WORLD, &world);
			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, mesh_verts, sizeof(vertex));
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
