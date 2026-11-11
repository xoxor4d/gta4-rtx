#pragma once

namespace gta4::free_camera
{
	/// A camera that is detached from the game: it replaces the matrix the game hands to the scene viewport,
	/// so the player and the game's own cameras are not touched and it keeps working in cutscenes.

	void install();

	bool is_active();
	void set_active(bool active);

	/// units per second
	float get_speed();
	void set_speed(float speed);

	/// vertical field of view in degrees, the camera ignores the fov of the game's cameras (cutscenes zoom a lot)
	float get_fov();
	void set_fov(float fov);

	/// reads mouse and keyboard, once per imgui frame while the camera is active:
	/// hold the right mouse button to look around and move (WASD, Q/E down/up, Shift/Space fast/slow), mouse wheel changes the speed
	void update_input();

	/// the right mouse button is held and moves the camera
	bool is_looking();

	/// game thread, once per frame: applies the camera when the game did not update its own (pause)
	void on_game_frame();
}
