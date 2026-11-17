#pragma once
#include "imgui.h"
#include "imgui_helper.hpp"

namespace shared::imgui::editor
{
	inline const ImVec4 COL_BUTTON_ACTIVE = ImVec4(0.22f, 0.5f, 0.26f, 1.0f);
	inline const ImVec4 COL_BUTTON_UNSAVED = ImVec4(0.72f, 0.5f, 0.26f, 1.0f);
	inline const ImVec4 COL_TEXT_DEFAULT_VALUE = ImVec4(0.56f, 0.56f, 0.56f, 1.0f); // #8F8F8F, a property that is not set
	inline const ImVec4 COL_TEXT_OVERRIDDEN = ImVec4(1.0f, 0.69f, 0.19f, 1.0f); // #FFB031

	/// toolbar button, the next item goes on the same line
	bool tool_button(const char* label, const char* tooltip, bool active = false, const ImVec4& active_color = COL_BUTTON_ACTIVE);

	void tool_separator();

	/// square button with a small icon, as high as a frame
	bool icon_button(const char* label);

	/// starts a row of a three column properties table (label, value, reset) and leaves the value column current
	void property_row_begin(const char* label, bool used, bool disabled);

	/// reset column of the row, true if the reset button was pressed
	bool property_row_end(bool used, const char* reset_tooltip);

	/// one row of a properties table: label, widget, reset to the default value. 'use' tells if the property is set
	template <typename widget_fn>
	bool property_row(const char* label, bool& use, const bool disabled, const char* reset_tooltip, widget_fn&& widget)
	{
		bool changed = false;
		const bool was_used = use;

		property_row_begin(label, was_used, disabled);
		if (widget())
		{
			use = true;
			changed = true;
		}

		if (property_row_end(was_used, reset_tooltip))
		{
			use = false;
			changed = true;
		}

		return changed;
	}

	/// menu button that opens or closes an editor, disabled while another editor is open. True if it was pressed
	bool launcher_button(const char* label, bool is_open, const ImVec2& size, const char* tooltip);

	/// handle at the start of a toolbar, the next item goes on the same line
	void toolbar_grip();

	/// toolbar button of fixed width that shows a file name, a name that does not fit slides while the button is hovered
	bool file_button(const char* id, const std::string& label, const char* tooltip);

	/// keeps the current window on screen, for windows that follow their content
	void keep_window_on_screen();

	enum class file_menu_action : std::uint8_t
	{
		none,
		new_file,
		save_active,
		save_all,
		discard,
	};

	/// items of a file menu: choice of the active file, new file, save, save all and discard. 'discard_file' receives the file to discard
	file_menu_action file_menu_items(const std::vector<std::string>& files, std::string& active_file, const std::set<std::string>& dirty_files, std::string& discard_file);

	/// modal that asks for the name of a new toml file, opens when 'open' is set.
	/// True once a name that is valid and not in 'existing_files' got confirmed, 'file' receives it with its extension
	bool new_file_popup(const char* title, const char* note, bool& open, std::string& name_buffer, const std::vector<std::string>& existing_files, std::string& file);

	enum class close_action : std::uint8_t
	{
		none,
		save_all_and_close,
		close_without_saving,
	};

	/// modal that lists the files with unsaved changes before an editor closes, opens when 'open' is set
	close_action unsaved_changes_popup(const char* title, bool& open, const std::set<std::string>& dirty_files, const char* close_without_saving_tooltip);

	// a message that shows for a few seconds
	struct status_toast_s
	{
		std::string text;
		double until = 0.0;

		void set(std::string message, double seconds = 3.0);
		void draw(const ImVec2& pos, ImU32 color = IM_COL32(235, 235, 235, 255)) const;
	};
}
