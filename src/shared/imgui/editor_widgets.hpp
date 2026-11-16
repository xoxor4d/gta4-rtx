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

	// a message that shows for a few seconds
	struct status_toast_s
	{
		std::string text;
		double until = 0.0;

		void set(std::string message, double seconds = 3.0);
		void draw(const ImVec2& pos, ImU32 color = IM_COL32(235, 235, 235, 255)) const;
	};
}
