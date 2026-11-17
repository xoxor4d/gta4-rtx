#pragma once

namespace gta4::marker_editor
{
	/// Free floating editor for the map markers (rtx_comp/markers/*.toml).
	/// It stays open without the F4 menu and keeps mouse and keyboard on imgui while it is open.

	bool is_open();

	/// closing with unsaved changes asks first, it does not open while another editor is open
	void set_open(bool open);
	void toggle();

	bool has_unsaved_changes();

	/// holding the right mouse button hands the input to the game (toolbar toggle, not while the free camera is on)
	bool allows_game_input_on_rmb();

	/// drops the selection, the edit history and the unsaved state, the map settings were loaded from disk
	void on_map_settings_reloaded();

	/// draws all editor windows and the world overlay, once per frame while the editor is open
	void draw();

	/// adds the editor to the editor host, which draws it and asks it about the input
	void register_editor();
}
