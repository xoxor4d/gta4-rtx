#include "std_include.hpp"

#include "imgui_internal.h"
#include "editor_widgets.hpp"
#include "world_draw.hpp"

namespace shared::imgui::editor
{
	bool tool_button(const char* label, const char* tooltip, const bool active, const ImVec4& active_color)
	{
		ImGui::Style_ColorButtonPush(active ? active_color : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		const bool pressed = ImGui::Button(label, ImVec2(0.0f, ImGui::GetFrameHeight() + 6.0f));
		ImGui::Style_ColorButtonPop();
		ImGui::SetItemTooltipBlur(tooltip);
		ImGui::SameLine(0.0f, 4.0f);
		return pressed;
	}

	void tool_separator()
	{
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine(0.0f, 6.0f);
	}

	bool icon_button(const char* label)
	{
		const float size = ImGui::GetFrameHeight();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::SetWindowFontScale(0.72f);
		const bool pressed = ImGui::Button(label, ImVec2(size, size));
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleVar();
		return pressed;
	}

	void property_row_begin(const char* label, const bool used, const bool disabled)
	{
		// popped in property_row_end, so the widget shares the color of the label
		ImGui::PushStyleColor(ImGuiCol_Text, used ? COL_TEXT_OVERRIDDEN : COL_TEXT_DEFAULT_VALUE);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);

		ImGui::TableNextColumn();
		ImGui::PushID(label);
		ImGui::BeginDisabled(disabled);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	bool property_row_end(const bool used, const char* reset_tooltip)
	{
		bool reset = false;
		ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		if (used)
		{
			reset = icon_button(ICON_FA_UNDO);
			ImGui::SetItemTooltipBlur(reset_tooltip);
		}

		ImGui::EndDisabled();
		ImGui::PopID();
		return reset;
	}

	void status_toast_s::set(std::string message, const double seconds)
	{
		text = std::move(message);
		until = ImGui::GetTime() + seconds;
	}

	void status_toast_s::draw(const ImVec2& pos, const ImU32 color) const
	{
		if (ImGui::GetTime() < until) {
			world::draw_text_shadowed(ImGui::GetForegroundDrawList(), pos, color, text.c_str());
		}
	}
}
