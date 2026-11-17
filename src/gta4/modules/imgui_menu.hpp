#pragma once
#include "comp_settings.hpp"
#include "shared/imgui/menu_widgets.hpp"

constexpr float TREENODE_SPACING = 6.0f;
constexpr float TREENODE_SPACING_INSIDE = 6.0f;
constexpr float SEPARATOR_SPACING = 12.0f;

#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

#define SET_CHILD_WIDGET_WIDTH_MAN(V)	ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild((V)));

#define CENTER_URL(text, link)					\
	ImGui::SetCursorForCenteredText((text));	\
	ImGui::TextURL((text), (link), true);

#define CLEAR_CACHE_CHECK(B, FN) \
	(B) = (FN) ? true : (B);

namespace gta4::menu
{
	namespace widgets = shared::imgui::menu;

	enum class page_id : std::uint8_t
	{
		home,
		water,
		weather,
		world,
		phone,
		other,
		tools,
		help,
		about,
		world_edits,
		lights,
		culling,
		debug,
		count,
	};

	/// Sidebar, search field and the selected page, drawn into the current window.
	/// @param footer_height	height that stays free below the menu
	void draw(float footer_height);

	/// Copies the colour settings of the menu into the colours the widgets draw with. Called every frame, so that edits,
	/// reloads and presets show at once.
	void apply_colors();

	/// True while the settings index is built. Pages run inside a window that skips its items,
	/// setting widgets only record themselves and the page scaffolding runs every body.
	bool is_indexing();

	/// Titled container for the part of a page that is always shown.
	void simple(const char* label, const char* icon, const std::function<void()>& body);

	/// Fold below the simple part of a page that holds its detailed settings, closed until the user opens it.
	void advanced(const std::function<void()>& body);

	/// Collapsing container with a title.
	void group(const char* label, const char* icon, const std::function<void()>& body, bool default_open = false);

	/// Block of widgets with a vertical label.
	void category(const char* label, const std::function<void()>& body);

	/// Called by a setting widget before it draws. Records the setting while the index is built and scrolls to it when it is the target of a search result.
	/// @return	false if the widget must not draw
	bool setting_begin(const char* label, comp_settings::variable& var);

	/// Called by a setting widget right after its item, highlights the target of a search result.
	void setting_end(const comp_settings::variable& var);

	/// Wiki links, shared by the help page and the help popup.
	void help_links();

#pragma region SETTING_WIDGETS

	void compsettings_var_reset_logic(comp_settings::variable& var);
	bool compsettings_bool_widget(const char* desc, comp_settings::variable& var);
	bool compsettings_int_widget(const char* desc, comp_settings::variable& var, const int& min = 0, const int& max = 0, const float& speed = 0.02f);

	/// @param items	zero separated list of the combo entries, the integer of the variable is the index of the selected entry
	bool compsettings_combo_widget(const char* desc, comp_settings::variable& var, const char* items);
	bool compsettings_float_widget(const char* desc, comp_settings::variable& var, const float& min = 0.0f, const float& max = 0.0f, const float& speed = 0.02f, const char* fmt = "%.2f");
	bool compsettings_vec_widget(const char* desc, comp_settings::variable& var, const int& size, const float& min = 0.0f, const float& max = 0.0f, const float& speed = 0.02f, const char* fmt = "%.2f");
	bool compsettings_color_widget(const char* desc, comp_settings::variable& var, const int& size, const ImGuiColorEditFlags_& flags);

	/// Pill switch for a bool setting, without a label.
	bool compsettings_switch_widget(const char* desc, comp_settings::variable& var);

#pragma endregion

#pragma region SETTING_ROWS

	/// Rows mark settings of the current minor version with a badge while this is on.
	void mark_new_settings(bool state);

	// One row per call: label on the left, the control of the setting in the control column on the right (widgets::row_begin).
	// Inside of widgets::grid_begin / grid_end the rows fill the cells of the grid. A change runs settings::on_changed.
	namespace rows
	{
		bool toggle(const char* label, comp_settings::variable& var);

		/// @param items	zero separated list of the combo entries
		bool combo(const char* label, comp_settings::variable& var, const char* items);
		bool drag_float(const char* label, comp_settings::variable& var, float min = 0.0f, float max = 0.0f, float speed = 0.02f, const char* fmt = "%.2f");

		/// switch and a value that is disabled while the switch is off, 'value_label' names the value in the search
		bool toggle_with_float(const char* label, comp_settings::variable& toggle_var, const char* value_label, comp_settings::variable& value_var,
			float min = 0.0f, float max = 0.0f, float speed = 0.02f, const char* fmt = "%.2f");
	}

#pragma endregion

	namespace settings
	{
		/// Side effects a setting needs when its value changes, wherever the change was made.
		void on_changed(comp_settings::variable& var);

		/// Rows shown on a topic page and on the home page.
		void wind_waves();
		void interactive_water();
		void phone_mode();
		void remix_atmosphere();
		void ped_wounds();
		void distant_light_sprites();
		void fake_distant_cars();
		void static_lod_fade();
	}

	namespace pages
	{
		void home();
		void water();
		void weather();
		void world();
		void phone();
		void other();
		void tools();
		void help();
		void about();
		void world_edits();
		void lights();
		void culling();
		void debug();
	}
}
