#pragma once

namespace shared::common::free_camera
{
	constexpr float SPEED_MIN = 0.25f;
	constexpr float SPEED_MAX = 400.0f;

	// a z-up fly camera, the game module decides where its matrix goes
	struct state_s
	{
		bool active = false;
		bool initialized = false;		// position and angles were taken from the game's camera
		Vector pos;
		float yaw = 0.0f;				// 0 looks along +y, positive turns towards +x
		float pitch = 0.0f;
		float speed = 8.0f;				// units per second
		float fov = 50.0f;
		float look_sensitivity = 1.0f;
	};

	// mouse look of the thread that runs imgui
	struct look_s
	{
		bool looking = false;
		POINT anchor = {};
	};

	Vector forward_of(const state_s& s);
	Vector right_of(const state_s& s);

	/// camera matrix (rows: right, up, back, position), 'm' is the matrix of the game's camera
	void init_from_matrix(state_s& s, const float* m);
	void build_matrix(const state_s& s, float* m);

	/// holds the cursor in place while the right mouse button is down. The look starts over the world ('allowed') and lasts until the button is released
	/// @return true while looking, dx / dy then hold the mouse movement of the frame
	bool update_look(look_s& look, bool allowed, float& dx, float& dy);

	/// turns and moves the camera: WASD, Q/E down/up, Shift/Space fast/slow, mouse wheel changes the speed
	void apply_input(state_s& s, float dx, float dy);
}
