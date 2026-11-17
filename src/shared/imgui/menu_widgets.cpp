#include "std_include.hpp"

#include <algorithm>
#include <cmath>

#include "imgui_internal.h"
#include "imgui_helper.hpp"
#include "menu_widgets.hpp"

namespace shared::imgui::menu
{
	namespace
	{
		constexpr float SIDEBAR_ICON_SLOT_X = 10.0f;
		constexpr float SIDEBAR_ICON_SLOT_WIDTH = 24.0f;
		constexpr float SIDEBAR_LABEL_OFFSET = 46.0f;
		constexpr float SIDEBAR_EDGE_PADDING = 10.0f;
		constexpr float SIDEBAR_ACCENT_BAR_WIDTH = 3.0f;

		constexpr float ROW_CONTROL_WIDTH = 170.0f;
		constexpr float ROW_LABEL_GAP = 8.0f;
		constexpr float ROW_SPLIT_GAP = 6.0f;
		constexpr float GRID_COLUMN_GAP = 28.0f;

		constexpr float SECTION_CONTENT_INDENT = 4.0f;

		struct grid_s
		{
			bool table_open = false;
			bool rows_advance = false;
		};

		struct row_s
		{
			float control_x = 0.0f;
			float control_width = 0.0f;
		};

		struct section_s
		{
			ImVec4 background;
			ImVec4 border;
			float body_top = 0.0f;
		};

		std::vector<grid_s> g_grids;
		row_s g_row;
		section_s g_section;

		ImU32 accent_color(const float alpha_mul = 1.0f)
		{
			ImVec4 color = menu_style.accent;
			color.w *= ImGui::GetStyle().Alpha * alpha_mul;

			return ImGui::ColorConvertFloat4ToU32(color);
		}

		float switch_height() {
			return std::floor(ImGui::GetFrameHeight() * 0.72f);
		}

		void draw_switch(ImDrawList* draw_list, const ImVec2& min, const bool on, const bool hovered)
		{
			const float height = switch_height();
			const float width = switch_width();
			const float radius = height * 0.5f;
			const ImVec2 max(min.x + width, min.y + height);

			if (on) {
				draw_list->AddRectFilled(min, max, accent_color(hovered ? 1.0f : 0.88f), radius);
			}
			else
			{
				draw_list->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), radius);
				draw_list->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Separator, 0.8f), radius);
			}

			const float knob_x = on ? max.x - radius : min.x + radius;
			draw_list->AddCircleFilled(ImVec2(knob_x, min.y + radius), radius - 3.0f, ImGui::GetColorU32(ImGuiCol_Text, on ? 1.0f : 0.7f));
		}

		void draw_sidebar_row(const ImVec2& min, const ImVec2& max, const char* icon, const char* label, const bool selected, const bool hovered, const float flash)
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			const auto draw_list = ImGui::GetWindowDrawList();

			if (selected)
			{
				draw_list->AddRectFilled(min, max, accent_color(0.28f), style.FrameRounding);
				draw_list->AddRectFilled(min, ImVec2(min.x + SIDEBAR_ACCENT_BAR_WIDTH, max.y), accent_color(), style.FrameRounding);
			}
			else if (hovered) {
				draw_list->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBgHovered, 0.35f), style.FrameRounding);
			}

			if (flash > 0.0f) {
				draw_list->AddRectFilled(min, max, accent_color(0.6f * std::min(flash, 1.0f)), style.FrameRounding);
			}

			const float text_y = min.y + (max.y - min.y - ImGui::GetFontSize()) * 0.5f;
			const float icon_x = min.x + SIDEBAR_ICON_SLOT_X + (SIDEBAR_ICON_SLOT_WIDTH - ImGui::CalcTextSize(icon).x) * 0.5f;
			const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text, selected || hovered ? 1.0f : 0.75f);

			draw_list->AddText(ImVec2(icon_x, text_y), selected ? accent_color() : text_color, icon);
			draw_list->AddText(ImVec2(min.x + SIDEBAR_LABEL_OFFSET, text_y), text_color, label);
		}

		void grid_open(const char* id, const int columns, const float min_column_width, const bool rows_advance)
		{
			const bool fits = ImGui::GetContentRegionAvail().x >= min_column_width * static_cast<float>(columns);

			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(GRID_COLUMN_GAP * 0.5f, ImGui::GetStyle().CellPadding.y));
			const bool open = ImGui::BeginTable(id, fits ? columns : 1, ImGuiTableFlags_SizingStretchSame);
			ImGui::PopStyleVar();

			g_grids.push_back({ open, rows_advance });
		}

		void grid_close()
		{
			if (g_grids.empty()) {
				return;
			}

			if (g_grids.back().table_open) {
				ImGui::EndTable();
			}

			g_grids.pop_back();
		}
	}

#pragma region SIDEBAR

	bool sidebar_item(const char* icon, const char* label, const bool selected, const float height, const float flash, const char* trailing_icon)
	{
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 max(min.x + ImGui::GetContentRegionAvail().x, min.y + height);

		ImGui::PushID(label);
		const bool pressed = ImGui::InvisibleButton("##sidebar_item", ImVec2(max.x - min.x, height));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		draw_sidebar_row(min, max, icon, label, selected, hovered, flash);

		if (trailing_icon)
		{
			const ImVec2 icon_size = ImGui::CalcTextSize(trailing_icon);
			ImGui::GetWindowDrawList()->AddText(ImVec2(max.x - SIDEBAR_EDGE_PADDING - icon_size.x, min.y + (height - icon_size.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text, hovered ? 0.9f : 0.5f), trailing_icon);
		}

		return pressed;
	}

	bool sidebar_switch(const char* icon, const char* label, bool& value, const float height)
	{
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 max(min.x + ImGui::GetContentRegionAvail().x, min.y + height);

		ImGui::PushID(label);
		const bool pressed = ImGui::InvisibleButton("##sidebar_switch", ImVec2(max.x - min.x, height));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		if (pressed) {
			value = !value;
		}

		draw_sidebar_row(min, max, icon, label, false, hovered, 0.0f);
		draw_switch(ImGui::GetWindowDrawList(), ImVec2(max.x - SIDEBAR_EDGE_PADDING - switch_width(), min.y + (height - switch_height()) * 0.5f), value, hovered);

		return pressed;
	}

	void sidebar_group_label(const char* label)
	{
		ImGui::Spacing(0.0f, 10.0f);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + SIDEBAR_ICON_SLOT_X + 2.0f);
		ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
		ImGui::TextDisabled("%s", label);
		ImGui::PopFont();
		ImGui::Spacing(0.0f, 2.0f);
	}

#pragma endregion

#pragma region WIDGETS

	namespace
	{
		constexpr float BADGE_FONT_SCALE = 0.8f;
		constexpr float BADGE_PADDING_X = 6.0f;
		constexpr float BADGE_PADDING_Y = 1.0f;
		constexpr float BADGE_LABEL_GAP = 8.0f;
	}

	float badge_width(const char* text) {
		return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * BADGE_FONT_SCALE, FLT_MAX, 0.0f, text).x + BADGE_PADDING_X * 2.0f;
	}

	void badge(const char* text, const ImVec4& color)
	{
		constexpr float FONT_SCALE = BADGE_FONT_SCALE;
		constexpr float PADDING_X = BADGE_PADDING_X;
		constexpr float PADDING_Y = BADGE_PADDING_Y;

		const ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImFont* font = ImGui::GetFont();
		const float font_size = ImGui::GetFontSize() * FONT_SCALE;
		const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
		const ImVec2 size(text_size.x + PADDING_X * 2.0f, text_size.y + PADDING_Y * 2.0f);
		const float line_height = std::max(window->DC.CurrLineSize.y, size.y);

		ImVec2 min = ImGui::GetCursorScreenPos();
		min.y += (line_height - size.y) * 0.5f;
		ImGui::Dummy(ImVec2(size.x, line_height));

		const auto draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), ImGui::ColorConvertFloat4ToU32(color), size.y * 0.5f);
		draw_list->AddText(font, font_size, ImVec2(min.x + PADDING_X, min.y + PADDING_Y), IM_COL32(0, 0, 0, 255), text);
	}

	bool fold_header(const char* label, bool& open)
	{
		constexpr float PADDING_X = 14.0f;
		constexpr float ICON_GAP = 8.0f;
		constexpr float LINE_GAP = 10.0f;

		const char* icon = open ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
		const float label_width = ImGui::CalcTextSize(label).x;
		const float icon_width = ImGui::CalcTextSize(icon).x;
		const float height = ImGui::GetFrameHeight();
		const float pill_width = PADDING_X * 2.0f + label_width + ICON_GAP + icon_width;

		const ImVec2 row_min = ImGui::GetCursorScreenPos();
		const float row_width = ImGui::GetContentRegionAvail().x;
		const ImVec2 pill_min(std::floor(row_min.x + (row_width - pill_width) * 0.5f), row_min.y);
		const ImVec2 pill_max(pill_min.x + pill_width, pill_min.y + height);

		ImGui::PushID(label);
		ImGui::SetCursorScreenPos(pill_min);
		if (ImGui::InvisibleButton("##fold_header", ImVec2(pill_width, height))) {
			open = !open;
		}
		const bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();

		// the row keeps the full width, whatever follows starts below it at the left edge
		ImGui::SetCursorScreenPos(row_min);
		ImGui::Dummy(ImVec2(row_width, height));

		const auto draw_list = ImGui::GetWindowDrawList();
		const ImU32 line_color = ImGui::GetColorU32(ImGuiCol_Separator, 0.6f);
		const ImU32 text_color = hovered || open ? ImGui::ColorConvertFloat4ToU32(menu_style.accent) : ImGui::GetColorU32(ImGuiCol_Text);
		const float line_y = std::floor(row_min.y + height * 0.5f);

		draw_list->AddLine(ImVec2(row_min.x, line_y), ImVec2(pill_min.x - LINE_GAP, line_y), line_color);
		draw_list->AddLine(ImVec2(pill_max.x + LINE_GAP, line_y), ImVec2(row_min.x + row_width, line_y), line_color);
		draw_list->AddRect(pill_min, pill_max, hovered ? text_color : line_color, height * 0.5f);

		const float text_y = pill_min.y + (height - ImGui::GetFontSize()) * 0.5f;
		draw_list->AddText(ImVec2(pill_min.x + PADDING_X, text_y), text_color, label);
		draw_list->AddText(ImVec2(pill_min.x + PADDING_X + label_width + ICON_GAP, text_y), text_color, icon);

		return open;
	}

	bool search_field(const char* id, char* buffer, const std::size_t buffer_size, const char* hint, const float width)
	{
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float clear_width = ImGui::GetFrameHeight();

		ImGui::PushID(id);
		ImGui::SetNextItemWidth(width - clear_width - spacing);
		bool changed = ImGui::InputTextWithHint("##text", hint, buffer, buffer_size);

		ImGui::SameLine(0.0f, spacing);
		ImGui::BeginDisabled(buffer[0] == '\0');
		if (ImGui::Button(ICON_FA_TIMES, ImVec2(clear_width, 0.0f)))
		{
			buffer[0] = '\0';
			changed = true;
		}
		ImGui::EndDisabled();
		ImGui::PopID();

		return changed;
	}

	float switch_width() {
		return std::floor(switch_height() * 1.8f);
	}

	bool toggle_switch(const char* id, bool& value)
	{
		const float frame_height = ImGui::GetFrameHeight();
		const ImVec2 pos = ImGui::GetCursorScreenPos();

		const bool pressed = ImGui::InvisibleButton(id, ImVec2(switch_width(), frame_height));
		if (pressed) {
			value = !value;
		}

		draw_switch(ImGui::GetWindowDrawList(), ImVec2(pos.x, pos.y + (frame_height - switch_height()) * 0.5f), value, ImGui::IsItemHovered());
		return pressed;
	}

	bool quiet_button(const char* label, const ImVec4* text_color)
	{
		ImVec4 hovered = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
		hovered.w *= 0.5f;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
		ImGui::PushStyleColor(ImGuiCol_Text, text_color ? *text_color : ImGui::GetStyleColorVec4(ImGuiCol_Text));
		const bool pressed = ImGui::Button(label);
		ImGui::PopStyleColor(5);

		return pressed;
	}

	void icon_tile(const char* icon, const float size)
	{
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 icon_size = ImGui::CalcTextSize(icon);
		ImGui::Dummy(ImVec2(size, size));

		const auto draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRectFilled(min, ImVec2(min.x + size, min.y + size), accent_color(0.2f), size * 0.2f);
		draw_list->AddText(ImVec2(min.x + (size - icon_size.x) * 0.5f, min.y + (size - icon_size.y) * 0.5f), accent_color(), icon);
	}

	void section_begin(const ImVec4& background, const ImVec4& border)
	{
		g_section.background = background;
		g_section.border = border;
		g_section.body_top = ImGui::Widget_ContainerStaticHeader(border);
	}

	bool section_body(const char* id)
	{
		float min_x = 0.0f;
		float max_x = 0.0f;
		ImGui::Widget_ContainerEdges(min_x, max_x);

		ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCurrentWindow()->WorkRect.Min.x, g_section.body_top + ImGui::GetStyle().ItemSpacing.y));
		const bool visible = ImGui::Widget_ContainerChildBegin(id, min_x, max_x);

		ImGui::Indent(SECTION_CONTENT_INDENT);
		return visible;
	}

	void section_end()
	{
		float min_x = 0.0f;
		float max_x = 0.0f;

		ImGui::Unindent(SECTION_CONTENT_INDENT);
		const float body_height = ImGui::Widget_ContainerChildEnd();
		ImGui::Widget_ContainerEdges(min_x, max_x);

		// drawn after the child, the window draws it below the content of the child all the same
		ImGui::Widget_ContainerBody(ImVec2(min_x, g_section.body_top), ImVec2(max_x, g_section.body_top + body_height), g_section.background, g_section.border);
	}

#pragma endregion

#pragma region ROWS

	void grid_begin(const char* id, const int columns, const float min_column_width) {
		grid_open(id, columns, min_column_width, true);
	}

	void grid_end() {
		grid_close();
	}

	void blocks_begin(const char* id, const int columns, const float min_column_width) {
		grid_open(id, columns, min_column_width, false);
	}

	void block_next()
	{
		if (!g_grids.empty() && g_grids.back().table_open) {
			ImGui::TableNextColumn();
		}
	}

	void blocks_end() {
		grid_close();
	}

	float row_begin(const char* label, const char* badge_text, const float used_width)
	{
		if (!g_grids.empty() && g_grids.back().rows_advance && g_grids.back().table_open) {
			ImGui::TableNextColumn();
		}

		const float avail = ImGui::GetContentRegionAvail().x;
		g_row.control_width = std::min(ROW_CONTROL_WIDTH, avail * 0.5f);
		g_row.control_x = ImGui::GetCursorPosX() + avail - g_row.control_width;

		// a long label is cut off, its badge stays whole
		const float taken = used_width > 0.0f ? std::min(used_width, g_row.control_width) : g_row.control_width;
		const float badge_space = badge_text ? badge_width(badge_text) + BADGE_LABEL_GAP : 0.0f;
		const float label_start_x = ImGui::GetCursorPosX();

		const ImVec2 label_min = ImGui::GetCursorScreenPos();
		const ImVec2 label_max(label_min.x + avail - taken - ROW_LABEL_GAP - badge_space, label_min.y + ImGui::GetFrameHeight());
		const char* label_end = ImGui::FindRenderedTextEnd(label);
		const float label_width = std::min(ImGui::CalcTextSize(label, label_end).x, std::max(label_max.x - label_min.x, 0.0f));

		ImGui::PushClipRect(label_min, label_max, true);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label, label_end);
		ImGui::PopClipRect();

		if (badge_text)
		{
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::SetCursorPosX(label_start_x + label_width + BADGE_LABEL_GAP);
			badge(badge_text, menu_style.badge_accent);
		}

		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetCursorPosX(g_row.control_x);
		ImGui::SetNextItemWidth(g_row.control_width);

		return g_row.control_width;
	}

	void row_align_right(const float item_width) {
		ImGui::SetCursorPosX(g_row.control_x + g_row.control_width - item_width);
	}

	float row_split(const float second_width)
	{
		const float first_width = g_row.control_width - second_width - ROW_SPLIT_GAP;
		ImGui::SetNextItemWidth(first_width);
		return first_width;
	}

	void row_second(const float second_width)
	{
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::SetCursorPosX(g_row.control_x + g_row.control_width - second_width);
	}

#pragma endregion

#pragma region POP_IN

	void pop_in_restart(pop_in_s& anim) {
		anim.time = 0.0f;
	}

	float pop_in_advance(pop_in_s& anim, const float duration)
	{
		anim.time = std::min(anim.time + ImGui::GetIO().DeltaTime, duration);

		// ease out cubic
		const float inv = 1.0f - (duration > 0.0f ? anim.time / duration : 1.0f);
		return 1.0f - inv * inv * inv;
	}

	void pop_in_push(const float progress, const ImVec2& center, const float slide)
	{
		// a window that begins with an alpha of zero is skipped for the frame
		constexpr float MIN_ALPHA = 0.02f;

		ImGui::SetNextWindowPos(ImVec2(center.x, center.y + (1.0f - progress) * slide), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * std::max(progress, MIN_ALPHA));
	}

	void pop_in_pop() {
		ImGui::PopStyleVar();
	}

#pragma endregion
}
