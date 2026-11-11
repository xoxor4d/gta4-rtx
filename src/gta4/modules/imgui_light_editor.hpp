#pragma once

namespace gta4::light_editor
{
	/// Free floating editor for light overrides and the ignore / allow light lists (rtx_comp/light_tweaks/*.toml).
	/// It stays open without the F4 menu and keeps mouse and keyboard on imgui while it is open.

	bool is_open();

	/// closing with unsaved changes asks first
	void set_open(bool open);
	void toggle();

	bool has_unsaved_changes();

	/// holding the right mouse button hands the input to the game (toolbar toggle, not while the free camera is on)
	bool allows_game_input_on_rmb();

	/// drops the edit history and the unsaved state, the map settings were loaded from disk
	void on_map_settings_reloaded();

	/// draws all editor windows and the world overlay, once per frame while the editor is open
	void draw();
}
