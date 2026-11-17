#pragma once
#include "shared/imgui/world_draw.hpp"

namespace gta4::scene_view
{
	/// camera of the game's scene viewport for world space drawing, false if there is no viewport yet
	bool get(shared::imgui::world::view_s& out);
}
