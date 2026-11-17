#include "std_include.hpp"
#include "scene_view.hpp"

namespace gta4::scene_view
{
	bool get(shared::imgui::world::view_s& out)
	{
		const auto viewports = game::pViewports;
		if (!viewports || !viewports->sceneviewport || !shared::globals::d3d_device) {
			return false;
		}

		const auto& vp = *viewports->sceneviewport;
		shared::globals::d3d_device->GetViewport(&out.viewport);
		shared::imgui::world::set_matrices(out, vp.view, vp.proj);

		out.cam_right = Vector(vp.cameraInv.m[0][0], vp.cameraInv.m[0][1], vp.cameraInv.m[0][2]);
		out.cam_up = Vector(vp.cameraInv.m[1][0], vp.cameraInv.m[1][1], vp.cameraInv.m[1][2]);
		out.cam_pos = Vector(vp.cameraInv.m[3][0], vp.cameraInv.m[3][1], vp.cameraInv.m[3][2]);
		return true;
	}
}
