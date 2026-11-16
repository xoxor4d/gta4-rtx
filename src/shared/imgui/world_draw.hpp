#pragma once
#include "imgui.h"

namespace shared::imgui::world
{
	// camera of the frame, filled by the game module
	struct view_s
	{
		D3DXMATRIX view;
		D3DXMATRIX proj;
		D3DXMATRIX view_proj;
		D3DVIEWPORT9 viewport;
		Vector cam_pos;
		Vector cam_right;
		Vector cam_up;
	};

	/// sets view, proj and their product
	void set_matrices(view_s& v, const D3DXMATRIX& view, const D3DXMATRIX& proj);

	/// false if the point is behind the camera or outside of the screen, screen is valid if it is in front
	bool project(const view_s& v, const Vector& point, ImVec2& screen, bool* in_front = nullptr);

	/// clips the segment against the camera plane, false if both ends are behind it. Imgui clips against the screen
	bool project_segment(const view_s& v, const D3DXVECTOR3& a, const D3DXVECTOR3& b, ImVec2& out_a, ImVec2& out_b);

	/// skipped if one of the ends is behind the camera
	void draw_line(const view_s& v, const Vector& from, const Vector& to, ImU32 color, float thickness = 1.5f);

	/// closed outline through up to 32 world space points, skipped if a point is behind the camera
	bool draw_loop(const view_s& v, const Vector* points, int count, ImU32 color, float thickness = 1.5f);

	/// ring around 'center' in the plane of two axes
	void make_ring(const Vector& center, const Vector& axis_a, const Vector& axis_b, float radius_a, float radius_b, Vector* points, int count);

	/// edges of an oriented box, 'transform' places the local bounds in the world
	void draw_box(const view_s& v, const D3DXMATRIX& transform, const Vector& local_mins, const Vector& local_maxs, ImU32 color, float thickness);

	/// circle with the on screen size of a sphere, skipped if its center is not on screen
	void draw_sphere_outline(const view_s& v, const Vector& center, float radius, ImU32 color);

	void draw_text_shadowed(ImDrawList* draw, const ImVec2& pos, ImU32 color, const char* text, float scale = 1.0f);

	/// where an arrow towards a projected point outside of the screen sits at the screen edge, 'dir' points to the point
	void edge_arrow(const ImVec2& screen, bool in_front, float edge_inset, ImVec2& pos, ImVec2& dir);

	void draw_edge_arrow(ImDrawList* draw, const ImVec2& pos, const ImVec2& dir, float size, ImU32 fill, ImU32 outline);

	/// the right mouse button was released after a short click without movement (holding it can belong to the camera or the game)
	bool is_short_right_click();

	/// index of the item closest to 'target' within 'max_distance' pixels, -1 if there is none. 'pos_of' returns the screen position of an item
	template <typename range_t, typename pos_fn>
	int nearest_point(const range_t& items, const ImVec2& target, const float max_distance, pos_fn&& pos_of)
	{
		float nearest_dist_sqr = max_distance * max_distance;
		int nearest = -1;
		int index = 0;

		for (const auto& item : items)
		{
			const ImVec2 pos = pos_of(item);
			const float dx = pos.x - target.x;
			const float dy = pos.y - target.y;

			if (const float d = dx * dx + dy * dy; d < nearest_dist_sqr)
			{
				nearest_dist_sqr = d;
				nearest = index;
			}

			index++;
		}

		return nearest;
	}
}
