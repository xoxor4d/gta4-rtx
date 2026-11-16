#include "std_include.hpp"

#include <algorithm>

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "editor_host.hpp"
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

	bool launcher_button(const char* label, const bool is_open, const ImVec2& size, const char* tooltip)
	{
		const char* other_editor = is_open ? nullptr : editor_host::open_editor_name();

		ImGui::BeginDisabled(other_editor != nullptr);
		ImGui::Style_ColorButtonPush(is_open ? COL_BUTTON_ACTIVE : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		const bool pressed = ImGui::Button(label, size);
		ImGui::Style_ColorButtonPop();
		ImGui::EndDisabled();

		if (!other_editor) {
			ImGui::SetItemTooltipBlur(tooltip);
		}
		else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip)) {
			ImGui::SetTooltip("Close the %s first, one editor is open at a time.", other_editor);
		}

		return pressed;
	}

	void toolbar_grip()
	{
		const ImVec2 grip_size = ImGui::CalcTextSize(ICON_FA_GRIP_VERTICAL);
		const float button_height = ImGui::GetFrameHeight() + 6.0f;
		const ImVec2 grip_pos = ImGui::GetCursorScreenPos();

		ImGui::Dummy(ImVec2(grip_size.x, button_height));
		ImGui::SetItemTooltipBlur("Drag to move the toolbar");
		ImGui::GetWindowDrawList()->AddText(ImVec2(grip_pos.x, grip_pos.y + (button_height - grip_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_GRIP_VERTICAL);
		ImGui::SameLine(0.0f, 8.0f);
	}

	bool file_button(const char* id, const std::string& label, const char* tooltip)
	{
		constexpr float file_button_width = 180.0f;
		const ImVec2 button_pos = ImGui::GetCursorScreenPos();
		const float button_height = ImGui::GetFrameHeight() + 6.0f;

		ImGui::Style_ColorButtonPush(ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		const bool pressed = ImGui::Button(id, ImVec2(file_button_width, button_height));
		ImGui::Style_ColorButtonPop();

		const bool hovered = ImGui::IsItemHovered();
		ImGui::SetItemTooltipBlur("%s\n%s", label.c_str(), tooltip);

		const float pad = ImGui::GetStyle().FramePadding.x + 2.0f;
		const ImVec2 caret_size = ImGui::CalcTextSize(ICON_FA_CARET_DOWN);
		const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
		const float caret_x = button_pos.x + file_button_width - pad - caret_size.x;
		const float text_min_x = button_pos.x + pad;
		const float text_max_x = caret_x - 6.0f;
		const float text_y = button_pos.y + (button_height - text_size.y) * 0.5f;

		float slide = 0.0f;
		if (const float overflow = text_size.x - (text_max_x - text_min_x); hovered && overflow > 0.0f)
		{
			// back and forth with a short rest at both ends
			const float rest = 20.0f;
			const float phase = fmodf(static_cast<float>(ImGui::GetTime()) * 45.0f, 2.0f * (overflow + rest));
			slide = std::clamp(phase < overflow + rest ? phase - rest * 0.5f : 2.0f * (overflow + rest) - phase - rest * 0.5f, 0.0f, overflow);
		}

		const auto draw = ImGui::GetWindowDrawList();
		draw->PushClipRect(ImVec2(text_min_x, button_pos.y), ImVec2(text_max_x, button_pos.y + button_height), true);
		draw->AddText(ImVec2(text_min_x - slide, text_y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
		draw->PopClipRect();
		draw->AddText(ImVec2(caret_x, button_pos.y + (button_height - caret_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), ICON_FA_CARET_DOWN);

		ImGui::SameLine(0.0f, 4.0f);
		return pressed;
	}

	void keep_window_on_screen()
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		const ImVec2 size = ImGui::GetWindowSize();
		const ImVec2 current = ImGui::GetWindowPos();
		const ImVec2 clamped(std::clamp(current.x, 0.0f, std::max(display.x - size.x, 0.0f)), std::clamp(current.y, 0.0f, std::max(display.y - size.y, 0.0f)));

		if (clamped.x != current.x || clamped.y != current.y) {
			ImGui::SetWindowPos(clamped);
		}
	}

	file_menu_action file_menu_items(const std::vector<std::string>& files, std::string& active_file, const std::set<std::string>& dirty_files, std::string& discard_file)
	{
		auto action = file_menu_action::none;
		ImGui::TextDisabled("Active file (highest priority first)");

		for (const auto& file : files)
		{
			const auto label = dirty_files.contains(file) ? file + "  *" : file;
			if (ImGui::MenuItem(label.c_str(), nullptr, file == active_file)) {
				active_file = file;
			}
		}

		if (ImGui::MenuItem(ICON_FA_FILE "  New file ...")) {
			action = file_menu_action::new_file;
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_SAVE "  Save active file", "Ctrl+S", false, !active_file.empty())) {
			action = file_menu_action::save_active;
		}

		if (ImGui::MenuItem(ICON_FA_SAVE "  Save all changed files", nullptr, false, !dirty_files.empty())) {
			action = file_menu_action::save_all;
		}

		if (ImGui::BeginMenu(ICON_FA_UNDO "  Discard changes", !dirty_files.empty()))
		{
			draw_window_blur();
			for (const auto& file : dirty_files)
			{
				if (ImGui::MenuItem(file.c_str()))
				{
					discard_file = file;
					action = file_menu_action::discard;
				}
			}

			ImGui::EndMenu();
		}

		return action;
	}

	bool new_file_popup(const char* title, const char* note, bool& open, std::string& name_buffer, const std::vector<std::string>& existing_files, std::string& file)
	{
		if (open)
		{
			ImGui::OpenPopup(title);
			open = false;
			name_buffer.clear();
		}

		if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
			return false;
		}

		draw_background_blur();
		ImGui::TextUnformatted(note);
		ImGui::Spacing(0, 4);

		ImGui::SetNextItemWidth(320.0f);
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		const bool enter = ImGui::InputTextWithHint("##file", "file name", &name_buffer, ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::TextDisabled(".toml");

		std::string name = name_buffer;
		if (name.ends_with(".toml")) {
			name.resize(name.size() - 5u);
		}

		const bool valid_chars = shared::utils::is_valid_file_stem(name);

		name += ".toml";
		const bool exists = std::ranges::find(existing_files, name) != existing_files.end();

		if (!name_buffer.empty() && !valid_chars) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Use letters, digits, '_', '-' and '.' only");
		}
		else if (exists) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "A file with that name exists");
		}

		bool created = false;
		ImGui::Spacing(0, 4);
		ImGui::BeginDisabled(!valid_chars || exists);
		if (ImGui::Button("Create", ImVec2(156.0f, 0.0f)) || (enter && valid_chars && !exists))
		{
			file = name;
			created = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Cancel", ImVec2(156.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return created;
	}

	close_action unsaved_changes_popup(const char* title, bool& open, const std::set<std::string>& dirty_files, const char* close_without_saving_tooltip)
	{
		if (open)
		{
			ImGui::OpenPopup(title);
			open = false;
		}

		if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
			return close_action::none;
		}

		auto action = close_action::none;
		draw_background_blur();
		ImGui::TextUnformatted("These files have changes that are not saved:");
		for (const auto& file : dirty_files) {
			ImGui::BulletText("%s", file.c_str());
		}

		ImGui::Spacing(0, 6);
		if (ImGui::Button("Save all and close", ImVec2(160.0f, 0.0f)))
		{
			action = close_action::save_all_and_close;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Close without saving", ImVec2(160.0f, 0.0f)))
		{
			action = close_action::close_without_saving;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemTooltipBlur(close_without_saving_tooltip);

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
		return action;
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
