#include "std_include.hpp"
#include "world_draw.hpp"

#include <algorithm>
#include <cmath>

namespace shared::imgui::world
{
	void set_matrices(view_s& v, const D3DXMATRIX& view, const D3DXMATRIX& proj)
	{
		v.view = view;
		v.proj = proj;
		D3DXMatrixMultiply(&v.view_proj, &v.view, &v.proj);
	}

	bool project(const view_s& v, const Vector& point, ImVec2& screen, bool* in_front)
	{
		const auto wp = point.ToD3DXVector();
		D3DXVECTOR3 projected;
		D3DXVec3Project(&projected, &wp, &v.viewport, &v.proj, &v.view, nullptr);

		const bool front = projected.z >= 0.0f && projected.z <= 1.0f;
		if (in_front) {
			*in_front = front;
		}

		screen = ImVec2(projected.x, projected.y);
		return front && screen.x >= 0.0f && screen.y >= 0.0f && screen.x <= static_cast<float>(v.viewport.Width) && screen.y <= static_cast<float>(v.viewport.Height);
	}

	bool project_segment(const view_s& v, const D3DXVECTOR3& a, const D3DXVECTOR3& b, ImVec2& out_a, ImVec2& out_b)
	{
		constexpr float MIN_W = 0.05f;

		D3DXVECTOR4 ca, cb;
		D3DXVec3Transform(&ca, &a, &v.view_proj);
		D3DXVec3Transform(&cb, &b, &v.view_proj);

		if (ca.w < MIN_W && cb.w < MIN_W) {
			return false;
		}

		if (ca.w < MIN_W) {
			ca += (cb - ca) * ((MIN_W - ca.w) / (cb.w - ca.w));
		}
		else if (cb.w < MIN_W) {
			cb += (ca - cb) * ((MIN_W - cb.w) / (ca.w - cb.w));
		}

		const auto to_screen = [&v](const D3DXVECTOR4& c)
			{
				return ImVec2(static_cast<float>(v.viewport.X) + (c.x / c.w * 0.5f + 0.5f) * static_cast<float>(v.viewport.Width),
							  static_cast<float>(v.viewport.Y) + (0.5f - c.y / c.w * 0.5f) * static_cast<float>(v.viewport.Height));
			};

		out_a = to_screen(ca);
		out_b = to_screen(cb);
		return true;
	}

	void draw_line(const view_s& v, const Vector& from, const Vector& to, const ImU32 color, const float thickness)
	{
		ImVec2 a, b;
		bool front_a = false;
		bool front_b = false;
		project(v, from, a, &front_a);
		project(v, to, b, &front_b);

		if (front_a && front_b) {
			ImGui::GetBackgroundDrawList()->AddLine(a, b, color, thickness);
		}
	}

	bool draw_loop(const view_s& v, const Vector* points, int count, const ImU32 color, const float thickness)
	{
		constexpr int max_points = 32;
		ImVec2 screen[max_points];
		count = std::min(count, max_points);

		for (auto i = 0; i < count; i++)
		{
			bool front = false;
			project(v, points[i], screen[i], &front);
			if (!front) {
				return false;
			}
		}

		ImGui::GetBackgroundDrawList()->AddPolyline(screen, count, color, ImDrawFlags_Closed, thickness);
		return true;
	}

	void make_ring(const Vector& center, const Vector& axis_a, const Vector& axis_b, const float radius_a, const float radius_b, Vector* points, const int count)
	{
		for (auto i = 0; i < count; i++)
		{
			const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
			points[i] = center + axis_a * (cosf(a) * radius_a) + axis_b * (sinf(a) * radius_b);
		}
	}

	void draw_box(const view_s& v, const D3DXMATRIX& transform, const Vector& local_mins, const Vector& local_maxs, const ImU32 color, const float thickness)
	{
		D3DXVECTOR3 corners[8];
		for (auto i = 0; i < 8; ++i)
		{
			const D3DXVECTOR3 local(
				(i & 1) ? local_maxs.x : local_mins.x,
				(i & 2) ? local_maxs.y : local_mins.y,
				(i & 4) ? local_maxs.z : local_mins.z);

			D3DXVec3TransformCoord(&corners[i], &local, &transform);
		}

		const auto dl = ImGui::GetBackgroundDrawList();
		for (auto i = 0; i < 8; ++i)
		{
			for (const auto axis : { 1, 2, 4 })
			{
				ImVec2 a, b;
				if (!(i & axis) && project_segment(v, corners[i], corners[i | axis], a, b)) {
					dl->AddLine(a, b, color, thickness);
				}
			}
		}
	}

	void draw_sphere_outline(const view_s& v, const Vector& center, const float radius, const ImU32 color)
	{
		ImVec2 screen_pos;
		if (!project(v, center, screen_pos)) {
			return;
		}

		// a point on the sphere next to its center gives the radius on screen
		ImVec2 screen_edge;
		bool edge_in_front = false;
		project(v, center + v.cam_right * radius, screen_edge, &edge_in_front);

		if (!edge_in_front) {
			return;
		}

		const float dx = screen_edge.x - screen_pos.x;
		const float dy = screen_edge.y - screen_pos.y;
		const float screen_radius = std::sqrt(dx * dx + dy * dy);

		ImGui::GetBackgroundDrawList()->AddCircle(screen_pos, screen_radius, color, 32, 1.0f);
	}

	void draw_text_shadowed(ImDrawList* draw, const ImVec2& pos, const ImU32 color, const char* text, const float scale)
	{
		const float size = ImGui::GetFontSize() * scale;
		draw->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 230), text);
		draw->AddText(ImGui::GetFont(), size, pos, color, text);
	}

	void edge_arrow(const ImVec2& screen, const bool in_front, const float edge_inset, ImVec2& pos, ImVec2& dir)
	{
		const auto& io = ImGui::GetIO();
		const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

		ImVec2 d(screen.x - center.x, screen.y - center.y);
		if (!in_front) {
			d = ImVec2(-d.x, -d.y); // the projection mirrors points behind the camera
		}

		const float len = sqrtf(d.x * d.x + d.y * d.y);
		d = len > 0.001f ? ImVec2(d.x / len, d.y / len) : ImVec2(0.0f, 1.0f);

		const float tx = fabsf(d.x) > 0.0001f ? (center.x - edge_inset) / fabsf(d.x) : FLT_MAX;
		const float ty = fabsf(d.y) > 0.0001f ? (center.y - edge_inset) / fabsf(d.y) : FLT_MAX;
		const float t = std::min(tx, ty);

		pos = ImVec2(center.x + d.x * t, center.y + d.y * t);
		dir = d;
	}

	void draw_edge_arrow(ImDrawList* draw, const ImVec2& pos, const ImVec2& dir, const float size, const ImU32 fill, const ImU32 outline)
	{
		const ImVec2 side(-dir.y, dir.x);
		const ImVec2 tip(pos.x + dir.x * size, pos.y + dir.y * size);
		const ImVec2 left(pos.x - dir.x * size * 0.6f + side.x * size * 0.75f, pos.y - dir.y * size * 0.6f + side.y * size * 0.75f);
		const ImVec2 right(pos.x - dir.x * size * 0.6f - side.x * size * 0.75f, pos.y - dir.y * size * 0.6f - side.y * size * 0.75f);

		draw->AddTriangleFilled(tip, left, right, fill);
		draw->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 200), 3.5f);
		draw->AddTriangle(tip, left, right, outline, 1.5f);
	}

	bool is_short_right_click()
	{
		const auto& io = ImGui::GetIO();
		return ImGui::IsMouseReleased(ImGuiMouseButton_Right)
			&& io.MouseDownDurationPrev[ImGuiMouseButton_Right] < 0.3f
			&& io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 36.0f;
	}
}
