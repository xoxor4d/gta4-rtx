#pragma once

namespace gta4::editor_common
{
	/// Toolbar parts that the free floating editors share.

	/// pause and single step buttons, 'step_held' is the editor's state of the step button. The next item goes on the same line
	void pause_buttons(bool& step_held, const char* pause_tooltip);

	/// free camera button and the button that hands the right mouse button to the game. The next item goes on the same line
	void camera_buttons(bool& rmb_game_input);

	void camera_speed_slider(float width);
}
