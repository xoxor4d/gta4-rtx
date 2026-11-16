#include "std_include.hpp"
#include "imgui_menu.hpp"

#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/menu_widgets.hpp"

namespace gta4::menu
{
	void compsettings_var_reset_logic(comp_settings::variable& var)
	{
		std::string popup_id = "Reset "s + var.m_name + " ?";

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			if (!ImGui::IsPopupOpen(popup_id.c_str())) {
				ImGui::OpenPopup(popup_id.c_str());
			}
		}

		ImGui::SetNextWindowSize(ImVec2(400.0f, 160.0f));
		if (ImGui::BeginPopupModal(popup_id.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::Spacing(0.0f, 0.0f);

			ImGui::Spacing();
			ImGui::CenterText("This will reset the current variable");

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::CenterText("Are you sure?");
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0); ImGui::SameLine();

			const auto xpos = ImGui::GetCursorPosX();
			ImGui::BeginGroup();

			const auto half_width = ImGui::GetContentRegionAvail().x * 0.5f;
			ImVec2 button_size(half_width - (ImGui::GetStyle().WindowPadding.x * 2.0f) - ImGui::GetStyle().ItemSpacing.x, 0.0f);
			if (ImGui::Button("Back To Saved", button_size))
			{
				var.reset_base();
				ImGui::CloseCurrentPopup();
			} TT("Restores setting to value stored in your comp_settings.toml file.");

			ImGui::SameLine();
			if (ImGui::Button("Back To Default", button_size)) 
			{
				var.reset_default();
				ImGui::CloseCurrentPopup();
			} TT("Restores setting to the default value defined by the compatibility mod.");
			ImGui::EndGroup();
			const auto group_width = ImGui::GetItemRectSize().x;

			ImGui::SetCursorPosX(xpos);
			if (ImGui::Button("Cancel", ImVec2(group_width, 0))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	bool compsettings_bool_widget(const char* desc, comp_settings::variable& var)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto gs_var_ptr = var.get_as<bool*>();
		const bool result = ImGui::Checkbox(desc, gs_var_ptr);
		
		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();
		return result;
	}

	bool compsettings_int_widget(const char* desc, comp_settings::variable& var, const int& min, const int& max, const float& speed)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto gs_var_ptr = var.get_as<int*>();
		const bool result = ImGui::DragInt(desc, gs_var_ptr, speed, min, max, "%d", (min != 0 || max != 0) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_combo_widget(const char* desc, comp_settings::variable& var, const char* items)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto gs_var_ptr = var.get_as<int*>();
		const bool result = ImGui::Combo(desc, gs_var_ptr, items);

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_float_widget(const char* desc, comp_settings::variable& var, const float& min, const float& max, const float& speed, const char* fmt)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());
		
		const auto gs_var_ptr = var.get_as<float*>();
		const bool result = ImGui::DragFloat(desc, gs_var_ptr, speed, min, max, fmt, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str()); 
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_vec_widget(const char* desc, comp_settings::variable& var, const int& size, const float& min, const float& max, const float& speed, const char* fmt)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto cs_var_ptr = var.get_as<float*>();
		bool result = false;
		switch (size)
		{
		case 2:
			assert(var.get_type() == comp_settings::var_type_vec2 && "Type mismatch: expected vec2");
			result = ImGui::DragFloat2(desc, cs_var_ptr, speed, min, max, fmt, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;

		case 3:
			assert(var.get_type() == comp_settings::var_type_vec3 && "Type mismatch: expected vec3");
			result = ImGui::DragFloat3(desc, cs_var_ptr, speed, min, max, fmt, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;

		default:
		case 4:
			assert(var.get_type() == comp_settings::var_type_vec4 && "Type mismatch: expected vec4");
			result = ImGui::DragFloat4(desc, cs_var_ptr, speed, min, max, fmt, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
			break;
		}

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();

		return result;
	}

	bool compsettings_color_widget(const char* desc, comp_settings::variable& var, const int& size, const ImGuiColorEditFlags_& flags)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
		if (colorize) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const auto cs_var_ptr = var.get_as<float*>();
		bool result = false;

		switch (size)
		{
		case 3:
			assert(var.get_type() == comp_settings::var_type_vec3 && "Type mismatch: expected vec3");
			result = ImGui::ColorEdit3(desc, cs_var_ptr, flags);
			break;

		default:
		case 4:
			assert(var.get_type() == comp_settings::var_type_vec4 && "Type mismatch: expected vec4");
			result = ImGui::ColorEdit4(desc, cs_var_ptr, flags);
			break;
		}

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		if (colorize) {
			ImGui::PopStyleColor();
		}

		ImGui::EndDisabled();
		return result;
	}

	bool compsettings_switch_widget(const char* desc, comp_settings::variable& var)
	{
		if (!setting_begin(desc, var)) {
			return false;
		}

		ImGui::BeginDisabled(var.get_temp_override_state());

		const bool result = widgets::toggle_switch(desc, *var.get_as<bool*>());

		setting_end(var);

		if (result) {
			var.set_dirty(false);
		}

		TT(var.get_tooltip_string().c_str());
		compsettings_var_reset_logic(var);

		ImGui::EndDisabled();
		return result;
	}

#pragma region ROWS

	namespace
	{
		bool g_mark_new_settings = false;

		/// @return	true if the setting was added or changed in the current minor version of the mod
		bool is_new_setting(const comp_settings::variable& var)
		{
			shared::utils::version_t new_since;
			new_since.major = COMP_MOD_VERSION_MAJOR;
			new_since.minor = COMP_MOD_VERSION_MINOR;

			return var.m_version >= new_since;
		}

		/// Starts the row of a setting: the label takes the colour of a dirty or overridden setting, the id of the setting is pushed.
		void setting_row_begin(const char* label, const comp_settings::variable& var, const float used_width = 0.0f)
		{
			const bool colorize = var.get_temp_override_state() || var.get_dirty_state();
			if (colorize) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.15f, 1.0f));
			}

			widgets::row_begin(label, g_mark_new_settings && is_new_setting(var) ? "NEW" : nullptr, used_width);

			if (colorize) {
				ImGui::PopStyleColor();
			}

			ImGui::PushID(var.m_name);
		}

		void setting_row_end(const bool changed, comp_settings::variable& var)
		{
			ImGui::PopID();

			if (changed) {
				settings::on_changed(var);
			}
		}
	}

	void mark_new_settings(const bool state) {
		g_mark_new_settings = state;
	}

	bool rows::toggle(const char* label, comp_settings::variable& var)
	{
		if (is_indexing())
		{
			setting_begin(label, var);
			return false;
		}

		setting_row_begin(label, var, widgets::switch_width());
		widgets::row_align_right(widgets::switch_width());
		const bool changed = compsettings_switch_widget("##value", var);
		setting_row_end(changed, var);

		return changed;
	}

	bool rows::combo(const char* label, comp_settings::variable& var, const char* items)
	{
		if (is_indexing())
		{
			setting_begin(label, var);
			return false;
		}

		setting_row_begin(label, var);
		const bool changed = compsettings_combo_widget("##value", var, items);
		setting_row_end(changed, var);

		return changed;
	}

	bool rows::drag_float(const char* label, comp_settings::variable& var, const float min, const float max, const float speed, const char* fmt)
	{
		if (is_indexing())
		{
			setting_begin(label, var);
			return false;
		}

		setting_row_begin(label, var);
		const bool changed = compsettings_float_widget("##value", var, min, max, speed, fmt);
		setting_row_end(changed, var);

		return changed;
	}

	bool rows::toggle_with_float(const char* label, comp_settings::variable& toggle_var, const char* value_label, comp_settings::variable& value_var,
		const float min, const float max, const float speed, const char* fmt)
	{
		if (is_indexing())
		{
			setting_begin(label, toggle_var);
			setting_begin(value_label, value_var);
			return false;
		}

		const float toggle_width = widgets::switch_width();
		setting_row_begin(label, toggle_var);

		ImGui::BeginDisabled(!toggle_var.get_as<bool>());
		widgets::row_split(toggle_width);
		if (compsettings_float_widget("##amount", value_var, min, max, speed, fmt)) {
			settings::on_changed(value_var);
		}
		ImGui::EndDisabled();

		widgets::row_second(toggle_width);
		const bool changed = compsettings_switch_widget("##value", toggle_var);
		setting_row_end(changed, toggle_var);

		return changed;
	}

#pragma endregion
}
