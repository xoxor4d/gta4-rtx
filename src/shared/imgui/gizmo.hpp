#pragma once
#include "world_draw.hpp"

namespace shared::imgui::gizmo
{
	enum class operation : std::uint8_t
	{
		translate,
		rotate,
		scale,
	};

	/// applies the look of the gizmo, once
	void setup_style();

	/// once per frame before any other gizmo call
	void begin_frame();

	/// the gizmo is being dragged
	bool is_using();

	/// the mouse is over the gizmo
	bool is_over();

	/// gizmo matrix (rows: x axis, y axis, z axis, position)
	void matrix_from_axes(const Vector& x_axis, const Vector& y_axis, const Vector& z_axis, const Vector& pos, float matrix[16]);

	/// the axes keep their length, which is the scale of the matrix
	void axes_from_matrix(const float matrix[16], Vector& x_axis, Vector& y_axis, Vector& z_axis, Vector& pos);

	/// draws the gizmo over the whole screen and applies the drag to 'matrix', true while it changes
	/// 'snap' is one step for all axes of a rotation (degrees) or one step per axis otherwise
	bool manipulate(const world::view_s& view, operation op, bool local, float matrix[16], const float* snap = nullptr);
}
