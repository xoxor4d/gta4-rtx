#include "std_include.hpp"
#include "remix_api.hpp"
#include "bridge_remix_api.h"

namespace shared::common
{
	// gets the singleton instance
	remix_api& remix_api::get()
	{
		static remix_api instance;
		return instance;
	}

	// called on device->BeginScene
	void remix_api::begin_scene_callback_internal()
	{
		auto& api = get();
		if (api.is_initialized())
		{
			if (api.m_debug_line_amount)
			{
				for (auto l = 1u; l < api.m_debug_line_amount + 1; l++)
				{
					if (api.m_debug_line_list[l])
					{
						remixapi_Transform t0 = {};
						t0.matrix[0][0] = 1.0f;
						t0.matrix[1][1] = 1.0f;
						t0.matrix[2][2] = 1.0f;

						const remixapi_InstanceInfo inst =
						{
							.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO,
							.pNext = nullptr,
							.categoryFlags = 0,
							.mesh = api.m_debug_line_list[l],
							.transform = t0,
							.doubleSided = true
						};

						api.m_bridge.DrawInstance(&inst);
					}
				}
			}

			// --

			if (!api.m_debug_circles.empty())
			{
				for (const auto circle : api.m_debug_circles)
				{
					if (circle.handle)
					{
						remixapi_Transform t0 = {};

						if (!circle.uses_custom_transform)
						{
							t0.matrix[0][0] = 1.0f;
							t0.matrix[1][1] = 1.0f;
							t0.matrix[2][2] = 1.0f;
						}

						const remixapi_InstanceInfo inst =
						{
							.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO,
							.pNext = nullptr,
							.categoryFlags = REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_LIGHTS,
							.mesh = circle.handle,
							.transform = circle.uses_custom_transform ? circle.transform : t0,
							.doubleSided = true,
						};

						api.m_bridge.DrawInstance(&inst);
					}
				}

				// remove all instances
				for (auto& circle : api.m_debug_circles)
				{
					if (circle.handle) {
						api.m_bridge.DestroyMesh(circle.handle);
					}
				}
				api.m_debug_circles.clear();
			}

			// --

			for (const auto& [n, fl] : api.m_flashlights)
			{
				if (fl.handle) {
					api.m_bridge.DrawLightInstance(fl.handle);
				}
			}


			// --
			// clear all after submitting them

			if (api.m_debug_line_amount)
			{
				for (auto& line : api.m_debug_line_list)
				{
					if (line)
					{
						api.m_bridge.DestroyMesh(line);
						line = nullptr;
					}
				}
				api.m_debug_line_amount = 0;
			}

			if (!api.m_debug_circle_materials.empty())
			{
				for (auto& m : api.m_debug_circle_materials)
				{
					if (m) {
						api.m_bridge.DestroyMaterial(m);
					}
				}
				api.m_debug_circle_materials.clear();
			}


			// --
			// external callback (if registered)

			if (api.begin_scene_callback_external) {
				api.begin_scene_callback_external();
			}
		}
	}

	void remix_api::end_scene_callback_internal()
	{
		// external callback (if registered)
		if (get().is_initialized() && get().end_scene_callback_external) {
			get().end_scene_callback_external();
		}
	}

	void remix_api::present_callback_internal()
	{
		// external callback (if registered)
		if (get().is_initialized() && get().present_callback_external) {
			get().present_callback_external();
		}
	}

	// ---

	void remix_api::init_debug_lines()
	{
		if (!m_debug_lines_initialized)
		{
			remixapi_MaterialInfoOpaqueEXT ext = {};
			{
				ext.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
				ext.useDrawCallAlphaState = 1;
				ext.opacityConstant = 1.0f;
				ext.roughnessConstant = 1.0f;
				ext.metallicConstant = 1.0f;
				ext.roughnessTexture = L"";
				ext.metallicTexture = L"";
				ext.heightTexture = L"";
			}

			remixapi_MaterialInfo info
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,
				.pNext = &ext,
				.hash = utils::string_hash64("linemat0"),
				.albedoTexture = L"",
				.normalTexture = L"",
				.tangentTexture = L"",
				.emissiveTexture = L"",
				.emissiveIntensity = 5.0f,
				.emissiveColorConstant = { 1.0f, 0.0f, 0.0f },
			};
			m_bridge.CreateMaterial(&info, &m_debug_line_materials[0]);

			info.hash = utils::string_hash64("linemat1");
			info.emissiveColorConstant = { 0.0f, 1.0f, 0.0f };
			m_bridge.CreateMaterial(&info, &m_debug_line_materials[1]);

			info.hash = utils::string_hash64("linemat3");
			info.emissiveColorConstant = { 0.0f, 1.0f, 1.0f };
			m_bridge.CreateMaterial(&info, &m_debug_line_materials[2]);

			info.hash = utils::string_hash64("linemat4");
			info.emissiveColorConstant = { 1.0f, 1.0f, 1.0f };
			m_bridge.CreateMaterial(&info, &m_debug_line_materials[3]);

			m_debug_lines_initialized = true;
		}
	}

	void remix_api::create_quad(remixapi_HardcodedVertex* v_out, uint32_t* i_out, const float scale)
	{
		if (!v_out || !i_out) {
			return;
		}

		auto make_vertex = [&](const float x, const float y, const float z, const float u, const float v)
			{
				const remixapi_HardcodedVertex vert =
				{
				  .position = { x, y, z },
				  .normal = { 0.0f, 0.0f, -1.0f },
				  .texcoord = { u, v },
				  .color = 0xFFFFFFFF,
				};
				return vert;
			};

		v_out[0] = make_vertex(-1.0f * scale, 1, -1.0f * scale, 0.0f, 0.0f); // bottom left
		v_out[1] = make_vertex(-1.0f * scale, 1, 1.0f * scale, 0.0f, 1.0f); // top left
		v_out[2] = make_vertex(1.0f * scale, 1, -1.0f * scale, 1.0f, 0.0f); // bottom right
		v_out[3] = make_vertex(1.0f * scale, 1, 1.0f * scale, 1.0f, 1.0f); // top right

		i_out[0] = 0; i_out[1] = 1; i_out[2] = 2;
		i_out[3] = 3; i_out[4] = 2; i_out[5] = 1;
	}

	void remix_api::create_line_quad(remixapi_HardcodedVertex* v_out, uint32_t* i_out, const Vector& p1, const Vector& p2, const float width)
	{
		if (!v_out || !i_out) {
			return;
		}

		auto make_vertex = [&](const Vector& pos, const float u, const float v)
			{
				const remixapi_HardcodedVertex vert =
				{
				  .position = { pos.x, pos.y, pos.z },
				  .normal = { 0.0f, 0.0f, -1.0f },
				  .texcoord = { u, v },
				  .color = 0xFFFFFFFF,
				};
				return vert;
			};

		Vector up = { 0.0f, 0.0f, 1.0f };
		Vector dir = p2 - p1;
		dir.Normalize();

		// check if dir is parallel or very close to the up vector
		if (fabs(DotProduct(dir, up)) > 0.999f) {
			up = { 1.0f, 0.0f, 0.0f }; // if parallel, choose a different up vector
		}

		// perpendicular vector to line
		Vector perp = dir.Cross(up);
		perp.Normalize();

		// scale by half width to offset vertices
		const Vector offset = perp * (width * 0.5f);

		v_out[0] = make_vertex(p1 - offset, 0.0f, 0.0f); // bottom left
		v_out[1] = make_vertex(p1 + offset, 0.0f, 1.0f); // top left
		v_out[2] = make_vertex(p2 - offset, 1.0f, 0.0f); // bottom right
		v_out[3] = make_vertex(p2 + offset, 1.0f, 1.0f); // top right

		i_out[0] = 0; i_out[1] = 1; i_out[2] = 2;
		i_out[3] = 3; i_out[4] = 2; i_out[5] = 1;
	}

	void remix_api::add_debug_line(const Vector& p1, const Vector& p2, const float width, DEBUG_REMIX_LINE_COLOR color)
	{
		if (m_debug_line_materials[color])
		{
			if (can_add_debug_lines())
			{
				m_debug_line_amount++;
				remixapi_HardcodedVertex verts[4] = {};
				uint32_t indices[6] = {};
				create_line_quad(verts, indices, p1, p2, width);

				remixapi_MeshInfoSurfaceTriangles triangles =
				{
				  .vertices_values = verts,
				  .vertices_count = ARRAYSIZE(verts),
				  .indices_values = indices,
				  .indices_count = 6,
				  .skinning_hasvalue = FALSE,
				  .skinning_value = {},
				  .material = m_debug_line_materials[color],
				};

				remixapi_MeshInfo info
				{
					.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
					.hash = utils::string_hash64(utils::va("line%d", m_debug_last_line_hash ? m_debug_last_line_hash : 1)),
					.surfaces_values = &triangles,
					.surfaces_count = 1,
				};

				m_bridge.CreateMesh(&info, &m_debug_line_list[m_debug_line_amount]);
				m_debug_last_line_hash = reinterpret_cast<std::uint64_t>(m_debug_line_list[m_debug_line_amount]);
			}
		}
	}

	/// This re-uses the previously added circle and re-draws it using a different transform (instanced drawing)
	/// @param center	center point of circle
	/// @param rot		rotation in degrees
	/// @param scale	scale of circle
	void remix_api::add_debug_circle_based_on_previous(const Vector& center, const Vector& rot, const Vector& scale)
	{
		if (m_debug_circles.empty()) {
			return;
		}

		m_debug_circles.push_back({});
		auto& circle = m_debug_circles.back();
		circle.handle = m_debug_circles[m_debug_circles.size() - 2u].handle; // previous handle

		D3DXMATRIX scale_matrix, rotation_x, rotation_y, rotation_z, mat_rotation, mat_translation, world, transposed;

		D3DXMatrixScaling(&scale_matrix, scale.x, scale.y, scale.z);
		D3DXMatrixRotationX(&rotation_x, DEG2RAD(rot.x)); // pitch
		D3DXMatrixRotationY(&rotation_y, DEG2RAD(rot.y)); // yaw
		D3DXMatrixRotationZ(&rotation_z, DEG2RAD(rot.z)); // roll
		mat_rotation = rotation_z * rotation_y * rotation_x; // combine rotations (order: Z * Y * X)

		D3DXMatrixTranslation(&mat_translation, center.x, center.y, center.z);
		world = scale_matrix * mat_rotation * mat_translation;

		D3DXMatrixTranspose(&transposed, &world);

		circle.transform.matrix[0][0] = transposed.m[0][0];
		circle.transform.matrix[0][1] = transposed.m[0][1];
		circle.transform.matrix[0][2] = transposed.m[0][2];
		circle.transform.matrix[0][3] = transposed.m[0][3];

		circle.transform.matrix[1][0] = transposed.m[1][0];
		circle.transform.matrix[1][1] = transposed.m[1][1];
		circle.transform.matrix[1][2] = transposed.m[1][2];
		circle.transform.matrix[1][3] = transposed.m[1][3];

		circle.transform.matrix[2][0] = transposed.m[2][0];
		circle.transform.matrix[2][1] = transposed.m[2][1];
		circle.transform.matrix[2][2] = transposed.m[2][2];
		circle.transform.matrix[2][3] = transposed.m[2][3];

		circle.uses_custom_transform = true;
	}

	void remix_api::add_debug_circle(const Vector& center, const Vector& normal, const float radius, const float thickness, const Vector& color, bool drawcall_alpha)
	{
		// material
		{
			/*	// BlendType
				kAlpha = 0,
				kAlphaEmissive = 1,
				kReverseAlphaEmissive = 2,
				kColor = 3,
				kColorEmissive = 4,
				kReverseColorEmissive = 5,
				kEmissive = 6,
				kMultiplicative = 7,
				kDoubleMultiplicative = 8,
				kReverseAlpha = 9,
				kReverseColor = 10,

				kMinValue = 0, // kAlpha
				kMaxValue = 10, // kReverseColor */

			remixapi_MaterialInfoOpaqueEXT ext = {};
			{
				//ext.pNext = &blend;
				ext.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
				ext.useDrawCallAlphaState = drawcall_alpha;
				ext.blendType_hasvalue = true;
				ext.blendType_value = 4;
				ext.opacityConstant = 1.0f;
				ext.albedoConstant = color.ToRemixFloat3D();
				ext.roughnessConstant = 1.0f;
				ext.metallicConstant = 1.0f;
				ext.roughnessTexture = L"";
				ext.metallicTexture = L"";
				ext.heightTexture = L"";
			}

			remixapi_MaterialInfo mat_info
			{
				.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO,
				.pNext = &ext,
				.hash = utils::string_hash64("circlemat" + std::to_string(m_debug_circle_materials.size())),
				.albedoTexture = L"",
				.normalTexture = L"",
				.tangentTexture = L"",
				.emissiveTexture = L"",
				.emissiveIntensity = 1.0f,
				.emissiveColorConstant = color.ToRemixFloat3D(),
			};

			m_debug_circle_materials.push_back(nullptr);
			m_bridge.CreateMaterial(&mat_info, &m_debug_circle_materials.back());
		}

		// mesh
		const uint32_t segments = 48u;

		std::vector<remixapi_HardcodedVertex> vertices;
		std::vector<uint32_t> indices;

		Vector up = fabs(normal.y) < 0.99f ? Vector(0, 1, 0) : Vector(1, 0, 0);
		Vector tangent = up.Cross(normal); tangent.Normalize();
		Vector bitangent = normal.Cross(tangent); bitangent.Normalize();

		// flat circle
		//for (auto i = 0u; i < segments; ++i)
		//{
		//	float angle = (2.0f * M_PI * (float)i) / segments;
		//	float next_angle = (2.0f * M_PI * (float)(i + 1)) / segments;

		//	Vector offset_outer = tangent * cosf(angle) * radius + bitangent * sinf(angle) * radius;
		//	Vector offset_inner = tangent * cosf(angle) * (radius - thickness) + bitangent * sinf(angle) * (radius - thickness);

		//	Vector next_offset_outer = tangent * cosf(next_angle) * radius + bitangent * sinf(next_angle) * radius;
		//	Vector next_offset_inner = tangent * cosf(next_angle) * (radius - thickness) + bitangent * sinf(next_angle) * (radius - thickness);

		//	Vector outer1 = /*center +*/ offset_outer;
		//	Vector inner1 = /*center +*/ offset_inner;
		//	Vector outer2 = /*center +*/ next_offset_outer;
		//	Vector inner2 = /*center +*/ next_offset_inner;

		//	uint32_t idx = vertices.size();
		//	vertices.push_back({ { outer1.x, outer1.y, outer1.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF });
		//	vertices.push_back({ { inner1.x, inner1.y, inner1.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF });
		//	vertices.push_back({ { outer2.x, outer2.y, outer2.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF });
		//	vertices.push_back({ { inner2.x, inner2.y, inner2.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF });
		//	indices.insert(indices.end(), { idx, idx + 1, idx + 2, idx + 1, idx + 3, idx + 2 });
		//}

		// torus
		Vector normal_offset = normal; normal_offset.Normalize();
		normal_offset *= 0.05f;  // torus thickness

		for (size_t i = 0u; i < segments; ++i)
		{
			float angle = (2.0f * M_PI * i) / segments;
			float next_angle = (2.0f * M_PI * (i + 1)) / segments;

			// uuter and inner ring calculations
			Vector offset_outer = tangent * cosf(angle) * radius + bitangent * sinf(angle) * radius;
			Vector offset_inner = tangent * cosf(angle) * (radius - thickness) + bitangent * sinf(angle) * (radius - thickness);
			Vector next_offset_outer = tangent * cosf(next_angle) * radius + bitangent * sinf(next_angle) * radius;
			Vector next_offset_inner = tangent * cosf(next_angle) * (radius - thickness) + bitangent * sinf(next_angle) * (radius - thickness);

			// top and bottom vertices for outer and inner ring
			Vector outer1_top = offset_outer + normal_offset;
			Vector outer1_bottom = offset_outer - normal_offset;
			Vector inner1_top = offset_inner + normal_offset;
			Vector inner1_bottom = offset_inner - normal_offset;

			Vector outer2_top = next_offset_outer + normal_offset;
			Vector outer2_bottom = next_offset_outer - normal_offset;
			Vector inner2_top = next_offset_inner + normal_offset;
			Vector inner2_bottom = next_offset_inner - normal_offset;

			uint32_t idx = vertices.size();
			vertices.push_back({ { outer1_top.x, outer1_top.y, outer1_top.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF });  // 0
			vertices.push_back({ { outer1_bottom.x, outer1_bottom.y, outer1_bottom.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 1
			vertices.push_back({ { inner1_top.x, inner1_top.y, inner1_top.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 2
			vertices.push_back({ { inner1_bottom.x, inner1_bottom.y, inner1_bottom.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 3

			vertices.push_back({ { outer2_top.x, outer2_top.y, outer2_top.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 4
			vertices.push_back({ { outer2_bottom.x, outer2_bottom.y, outer2_bottom.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 5
			vertices.push_back({ { inner2_top.x, inner2_top.y, inner2_top.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 6
			vertices.push_back({ { inner2_bottom.x, inner2_bottom.y, inner2_bottom.z }, { normal.x, normal.y, normal.z }, {}, 0xFFFFFFFF }); // 7

			// side walls (connect top/bottom)
			indices.insert(indices.end(), { idx, idx + 1, idx + 4, idx + 1, idx + 5, idx + 4 }); // Outer wall
			indices.insert(indices.end(), { idx + 2, idx + 6, idx + 3, idx + 3, idx + 6, idx + 7 }); // Inner wall

			// top and bottom caps
			indices.insert(indices.end(), { idx, idx + 2, idx + 4, idx + 2, idx + 6, idx + 4 }); // Top cap
			indices.insert(indices.end(), { idx + 1, idx + 5, idx + 3, idx + 3, idx + 5, idx + 7 }); // Bottom cap
		}

		remixapi_MeshInfoSurfaceTriangles triangles =
		{
		  .vertices_values = vertices.data(),
		  .vertices_count = vertices.size(),
		  .indices_values = indices.data(),
		  .indices_count = indices.size(),
		  .skinning_hasvalue = FALSE,
		  .skinning_value = {},
		  .material = m_debug_circle_materials.back(),
		};

		remixapi_MeshInfo info
		{
			.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO,
			.hash = utils::string_hash64(utils::va("circle%d", m_debug_circles_last_hash ? m_debug_circles_last_hash : 1)),
			.surfaces_values = &triangles,
			.surfaces_count = 1,
		};

		m_debug_circles.push_back({});

		auto& circle = m_debug_circles.back();

		circle.transform.matrix[0][0] = 1.0f;
		circle.transform.matrix[1][1] = 1.0f;
		circle.transform.matrix[2][2] = 1.0f;

		circle.transform.matrix[0][3] = center.x;
		circle.transform.matrix[1][3] = center.y;
		circle.transform.matrix[2][3] = center.z;
		circle.uses_custom_transform = true;

		m_bridge.CreateMesh(&info, &m_debug_circles.back().handle);
		m_debug_circles_last_hash = reinterpret_cast<std::uint64_t>(m_debug_circles.back().handle);
	}

	/**
	 * Draw a wireframe box using the remix api
	 * @param mins			Bound mins
	 * @param maxs			Bound maxs
	 * @param line_width	Line width
	 * @param color			Line color
	 */
	void remix_api::debug_draw_box(const Vector& mins, const Vector& maxs, const float line_width, const DEBUG_REMIX_LINE_COLOR& color)
	{
		Vector corners[8];

		// get the corners of the cube
		corners[0] = Vector(mins.x, mins.y, mins.z);
		corners[1] = Vector(mins.x, mins.y, maxs.z);
		corners[2] = Vector(mins.x, maxs.y, mins.z);
		corners[3] = Vector(mins.x, maxs.y, maxs.z);
		corners[4] = Vector(maxs.x, mins.y, mins.z);
		corners[5] = Vector(maxs.x, mins.y, maxs.z);
		corners[6] = Vector(maxs.x, maxs.y, mins.z);
		corners[7] = Vector(maxs.x, maxs.y, maxs.z);

		// define the edges
		Vector lines[12][2];
		lines[0][0] = corners[0];	lines[0][1] = corners[1]; // Edge 1
		lines[1][0] = corners[0];	lines[1][1] = corners[2]; // Edge 2
		lines[2][0] = corners[0];	lines[2][1] = corners[4]; // Edge 3
		lines[3][0] = corners[1];	lines[3][1] = corners[3]; // Edge 4
		lines[4][0] = corners[1];	lines[4][1] = corners[5]; // Edge 5
		lines[5][0] = corners[2];	lines[5][1] = corners[3]; // Edge 6
		lines[6][0] = corners[2];	lines[6][1] = corners[6]; // Edge 7
		lines[7][0] = corners[3];	lines[7][1] = corners[7]; // Edge 8
		lines[8][0] = corners[4];	lines[8][1] = corners[5]; // Edge 9
		lines[9][0] = corners[4];	lines[9][1] = corners[6]; // Edge 10
		lines[10][0] = corners[5];	lines[10][1] = corners[7]; // Edge 11
		lines[11][0] = corners[6];	lines[11][1] = corners[7]; // Edge 12

		for (auto e = 0u; e < 12; e++) {
			add_debug_line(lines[e][0], lines[e][1], line_width, color);
		}
	}

	/**
	 * Draw a wireframe box using the remix api
	 * @param center		Center of the cube
	 * @param half_diagonal Half diagonal distance of the box
	 * @param line_width	Line width
	 * @param color			Line color
	 */
	void remix_api::debug_draw_box(const VectorAligned& center, const VectorAligned& half_diagonal, const float line_width, const DEBUG_REMIX_LINE_COLOR& color)
	{
		debug_draw_box(center - half_diagonal, center + half_diagonal, line_width, color);
	}

	void remix_api::flashlight_create_or_update(const char* player_name, flashlight_def_s& def, bool is_enabled, bool is_player)
	{
		if (const auto it = m_flashlights.find(player_name);
			it == m_flashlights.end())
		{
			// insert new flashlight data
			m_flashlights[player_name] =
			{
				.def = std::move(def),
				.is_player = is_player,
				.is_enabled = is_enabled
			};
		}
		else
		{
			// update existing flashlight data
			it->second.def = std::move(def);
			/*it->second.def.pos = def.pos;
			it->second.def.fwd = def.fwd;
			it->second.def.rt = def.rt;
			it->second.def.up = def.up;*/
			it->second.is_player = is_player;
			it->second.is_enabled = is_enabled;
		}
	}

	void remix_api::flashlight_frame()
	{
		if (auto& api = remix_api::get();
			api.is_initialized())
		{
			for (auto& [name, fl] : api.m_flashlights)
			{
				if (fl.handle)
				{
					api.m_bridge.DestroyLight(fl.handle);
					fl.handle = nullptr;
				}

				if (fl.is_enabled)
				{
					//const auto gs = game_settings::get();

					auto& info = fl.info;
					auto& ext = fl.ext;

					ext.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO_SPHERE_EXT;
					ext.pNext = nullptr;

					/*const Vector light_org = fl.def.pos +
						(fl.is_player ? gs->flashlight_offset_player.get_as<float*>() :
										gs->flashlight_offset_bot.get_as<float*>());*/

					Vector lpos = fl.def.pos;
					const Vector offs = fl.def.offset; //fl.is_player ? gs->flashlight_offset_player.get_as<float*>() : gs->flashlight_offset_bot.get_as<float*>();
					lpos += (fl.def.fwd * offs.x) + (fl.def.rt * offs.z) + (fl.def.up * offs.y);

					ext.position = lpos.ToRemixFloat3D();

					ext.radius = fl.def.radius; //gs->flashlight_radius.get_as<float>();
					ext.shaping_hasvalue = TRUE;
					ext.shaping_value = {};

					ext.shaping_value.direction = fl.def.fwd.ToRemixFloat3D();

					ext.shaping_value.coneAngleDegrees = fl.def.angle; //gs->flashlight_angle.get_as<float>();
					ext.shaping_value.coneSoftness = fl.def.softness; //gs->flashlight_softness.get_as<float>();
					ext.shaping_value.focusExponent = fl.def.expo; //gs->flashlight_expo.get_as<float>();

					info.sType = REMIXAPI_STRUCT_TYPE_LIGHT_INFO;
					info.pNext = &fl.ext;
					info.hash = utils::string_hash64(utils::va("fl%s", name.c_str()));

					const float intensity = fl.def.intensity; //gs->flashlight_intensity.get_as<float>();
					info.radiance = remixapi_Float3D{ 20.0f * intensity, 20.0f * intensity, 20.0f * intensity };

					api.m_bridge.CreateLight(&fl.info, &fl.handle);
				}
			}
		}
	}

	// ---

	void remix_api::initialize(
		PFN_remixapi_BridgeCallback begin_scene_callback,
		PFN_remixapi_BridgeCallback end_scene_callback,
		PFN_remixapi_BridgeCallback present_callback, 
		bool is_asi)
	{
		auto& instance = get();
		if (!instance.m_initialized)
		{
			if (const auto status = remixapi::bridge_initRemixApi(&instance.m_bridge, is_asi);
				status == REMIXAPI_ERROR_CODE_SUCCESS)
			{
				instance.begin_scene_callback_external = begin_scene_callback;
				instance.end_scene_callback_external = end_scene_callback;
				instance.present_callback_external = present_callback;

				remixapi::bridge_setRemixApiCallbacks(begin_scene_callback_internal, end_scene_callback_internal, present_callback_internal);

				instance.init_debug_lines();

				instance.m_debug_circles.reserve(512);
				instance.m_debug_circle_materials.reserve(512);

				instance.m_initialized = true;
				shared::common::log("RemixApi", "Initialized RemixApi", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
			}
			else {
				shared::common::log("RemixApi", std::format("Failed to initialize the remixApi - Code: {:d}", static_cast<int>(status)), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}
		}
	}
}
