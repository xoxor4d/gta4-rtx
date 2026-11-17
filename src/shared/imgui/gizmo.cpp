#include "std_include.hpp"
#include "gizmo.hpp"

#include "ImGuizmo.h"

namespace shared::imgui::gizmo
{
	void setup_style()
	{
		static bool done = false;
		if (done) {
			return;
		}

		done = true;
		auto& gizmo_style = ImGuizmo::GetStyle();
		gizmo_style.TranslationLineThickness = 5.0f;
		gizmo_style.TranslationLineArrowSize = 8.0f;
		gizmo_style.RotationLineThickness = 4.0f;
		gizmo_style.RotationOuterLineThickness = 3.0f;
		gizmo_style.HatchedAxisLineThickness = 0.0f;
		gizmo_style.CenterCircleSize = 5.0f;

		gizmo_style.Colors[ImGuizmo::DIRECTION_X] = ImVec4(1.00f, 0.47f, 0.47f, 1.0f);
		gizmo_style.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.52f, 0.92f, 0.47f, 1.0f);
		gizmo_style.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.47f, 0.67f, 1.00f, 1.0f);
		gizmo_style.Colors[ImGuizmo::PLANE_X] = ImVec4(1.00f, 0.47f, 0.47f, 0.75f);
		gizmo_style.Colors[ImGuizmo::PLANE_Y] = ImVec4(0.52f, 0.92f, 0.47f, 0.75f);
		gizmo_style.Colors[ImGuizmo::PLANE_Z] = ImVec4(0.47f, 0.67f, 1.00f, 0.75f);
		gizmo_style.Colors[ImGuizmo::SELECTION] = ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
		gizmo_style.Colors[ImGuizmo::INACTIVE] = ImVec4(0.65f, 0.65f, 0.65f, 0.85f);
		gizmo_style.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
		gizmo_style.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(1.00f, 0.85f, 0.30f, 0.35f);

		// an axis that points away from the camera stays where it is instead of flipping to the other side
		ImGuizmo::AllowAxisFlip(false);
	}

	void begin_frame() {
		ImGuizmo::BeginFrame();
	}

	bool is_using() {
		return ImGuizmo::IsUsing();
	}

	bool is_over() {
		return ImGuizmo::IsOver();
	}

	void matrix_from_axes(const Vector& x_axis, const Vector& y_axis, const Vector& z_axis, const Vector& pos, float matrix[16])
	{
		float* m = matrix;
		m[0] = x_axis.x;	m[1] = x_axis.y;	m[2] = x_axis.z;	m[3] = 0.0f;
		m[4] = y_axis.x;	m[5] = y_axis.y;	m[6] = y_axis.z;	m[7] = 0.0f;
		m[8] = z_axis.x;	m[9] = z_axis.y;	m[10] = z_axis.z;	m[11] = 0.0f;
		m[12] = pos.x;		m[13] = pos.y;		m[14] = pos.z;		m[15] = 1.0f;
	}

	void axes_from_matrix(const float matrix[16], Vector& x_axis, Vector& y_axis, Vector& z_axis, Vector& pos)
	{
		x_axis = Vector(matrix[0], matrix[1], matrix[2]);
		y_axis = Vector(matrix[4], matrix[5], matrix[6]);
		z_axis = Vector(matrix[8], matrix[9], matrix[10]);
		pos = Vector(matrix[12], matrix[13], matrix[14]);
	}

	bool manipulate(const world::view_s& view, const operation op, const bool local, float matrix[16], const float* snap)
	{
		auto gizmo_operation = ImGuizmo::TRANSLATE;
		if (op == operation::rotate) {
			gizmo_operation = ImGuizmo::ROTATE;
		}
		else if (op == operation::scale) {
			gizmo_operation = ImGuizmo::SCALE;
		}

		const auto& io = ImGui::GetIO();
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

		return ImGuizmo::Manipulate(&view.view.m[0][0], &view.proj.m[0][0], gizmo_operation, local ? ImGuizmo::LOCAL : ImGuizmo::WORLD, matrix, nullptr, snap);
	}
}
