#pragma once

namespace shared::imgui::editor_host
{
	// a free floating editor that stays open without the menu
	struct editor_s
	{
		const char* name = nullptr;
		bool (*is_open)() = nullptr;
		void (*draw)() = nullptr;					// all windows and the world overlay, once per frame while the editor is open
		bool (*allows_game_input_on_rmb)() = nullptr;	// optional: holding the right mouse button hands the input to the game
	};

	void register_editor(const editor_s& editor);

	bool any_open();

	/// name of the open editor, nullptr if none is open. An editor does not open while another one is
	const char* open_editor_name();

	/// updates globals::imgui_editor_open, an editor calls this whenever it opens or closes
	void sync_open_state();

	/// draws every open editor
	void draw();

	/// every open editor has to agree, false if none is open
	bool allows_game_input_on_rmb();
}
