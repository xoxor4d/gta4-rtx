#pragma once

namespace gta4::mesh_inspector
{
	/// Free floating editor for the anti cull categories and the lod fade exclusions of the map settings.
	/// It lists the building meshes around the camera, traces their culling and hides meshes while it is open.
	/// It does not write files: changes apply to the loaded map settings and are exported through the clipboard.

	bool is_open();

	/// closing unhides all meshes and turns the lod view off, it does not open while another editor is open
	void set_open(bool open);
	void toggle();

	/// categories or exclusions changed since they were last copied to the clipboard or loaded from disk
	bool has_uncopied_changes();

	void copy_anticull_to_clipboard();
	void copy_lod_fade_exclusions_to_clipboard();

	/// holding the right mouse button hands the input to the game (toolbar toggle, not while the free camera is on)
	bool allows_game_input_on_rmb();

	/// drops the selection, the highlight and the input buffers, the map settings were loaded from disk
	void on_map_settings_reloaded();

	/// draws all editor windows and the world overlay, once per frame while the editor is open
	void draw();

	/// adds the editor to the editor host, which draws it and asks it about the input
	void register_editor();
}
