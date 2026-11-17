#pragma once
#include "imgui.h"

namespace shared::imgui::menu
{
	struct style_s
	{
		ImVec4 accent = ImVec4(0.94f, 0.63f, 0.01f, 1.0f); // switches, selected sidebar rows, icon tiles
		ImVec4 badge_accent = ImVec4(0.94f, 0.63f, 0.01f, 1.0f); // badges of setting rows
	};

	// colours of the menu widgets, set by the game module
	inline style_s menu_style;

#pragma region SIDEBAR

	/// row of a sidebar: icon in a slot of fixed width, then the label, over the available width. True if it was pressed
	/// @param flash			0 to 1, tints the row in the accent colour
	/// @param trailing_icon	drawn dimmed at the right edge
	bool sidebar_item(const char* icon, const char* label, bool selected, float height = 32.0f, float flash = 0.0f, const char* trailing_icon = nullptr);

	/// sidebar row with a switch at its right edge, a click anywhere on the row toggles 'value'. True if it was toggled
	bool sidebar_switch(const char* icon, const char* label, bool& value, float height = 32.0f);

	/// dimmed label above a group of sidebar rows
	void sidebar_group_label(const char* label);

#pragma endregion

#pragma region WIDGETS

	/// small rounded tag, vertically centered on the current line
	void badge(const char* text, const ImVec4& color);

	/// pill in the middle of a divider line that shows or hides what follows it.
	/// @return	the state of 'open' after a click was handled
	bool fold_header(const char* label, bool& open);

	/// text input with a button that clears it. True if the text changed
	bool search_field(const char* id, char* buffer, std::size_t buffer_size, const char* hint, float width);

	float switch_width();

	/// pill switch as high as a frame, in the accent colour while on. True if it was toggled
	bool toggle_switch(const char* id, bool& value);

	/// button without a background
	/// @param text_color	nullptr for the text colour of the style
	bool quiet_button(const char* label, const ImVec4* text_color = nullptr);

	/// rounded square in the accent tint with an icon in its center
	void icon_tile(const char* icon, float size);

	/// Container with the geometry and colours of Widget_ContainerWithCollapsingTitle whose header takes items instead of collapsing:
	/// section_begin, the frame high items of the header, section_body, the content, section_end.
	void section_begin(const ImVec4& background, const ImVec4& border);

	/// ends the header of a section and starts its content, call section_end whatever it returns
	bool section_body(const char* id);

	void section_end();

#pragma endregion

#pragma region ROWS

	/// Grid of setting rows: every row_begin takes the next cell. The grid has one column while the
	/// available width is below 'min_column_width' per column.
	void grid_begin(const char* id, int columns = 2, float min_column_width = 300.0f);

	void grid_end();

	/// Grid of blocks, block_next starts the next cell. Rows inside of a block stack up.
	void blocks_begin(const char* id, int columns = 2, float min_column_width = 380.0f);

	void block_next();

	void blocks_end();

	/// Starts a setting row: label and optional badge on the left, the control column of fixed width on the right.
	/// The cursor is left at the control column and the width of the next item is set to fill it.
	/// @param used_width	what the controls of this row take at the right edge of the column, the label gets the rest. 0 = the whole column
	/// @return				width of the control column
	float row_begin(const char* label, const char* badge_text = nullptr, float used_width = 0.0f);

	/// width of a badge with this text
	float badge_width(const char* text);

	/// moves the cursor so that an item of this width ends at the right edge of the control column
	void row_align_right(float item_width);

	/// For two controls in one row: sets the width of the next item to what the second control leaves of the column.
	/// @return	that width
	float row_split(float second_width);

	/// places the second control of a row at the right edge of the control column
	void row_second(float second_width);

#pragma endregion

#pragma region POP_IN

	struct pop_in_s
	{
		float time = 0.0f;
	};

	/// starts the animation over, call it when the window opens
	void pop_in_restart(pop_in_s& anim);

	/// advances the animation by the frame time.
	/// @return	eased progress, 0 to 1
	float pop_in_advance(pop_in_s& anim, float duration = 0.15f);

	/// centers the next window on 'center', offset by what is left of the slide, and pushes its alpha.
	/// Call pop_in_pop after the window ended, whether it was visible or not
	void pop_in_push(float progress, const ImVec2& center, float slide = 10.0f);

	void pop_in_pop();

#pragma endregion
}
