#include "std_include.hpp"
#include "imgui_menu.hpp"

#include <algorithm>
#include <sstream>

#include "imgui_internal.h"
#include "comp_settings.hpp"
#include "imgui.hpp"
#include "remix_vars.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/menu_widgets.hpp"

namespace gta4::menu
{
	namespace
	{
		constexpr float SIDEBAR_WIDTH = 170.0f;
		constexpr float SIDEBAR_GAP = 8.0f;
		constexpr float SIDEBAR_ROW_HEIGHT = 32.0f;
		constexpr float SAVE_FLASH_SECONDS = 0.6f;
		constexpr float RELOAD_PANEL_WIDTH = 280.0f;
		constexpr float RELOAD_PANEL_GAP = 14.0f;
		constexpr float PANEL_ROW_HEIGHT = 28.0f;
		constexpr float PANEL_HEADING_INDENT = 10.0f;
		constexpr float PRESET_ROWS_VISIBLE = 7.0f;
		constexpr std::size_t PRESET_FILTER_MIN_COUNT = 8u;
		constexpr float HIGHLIGHT_SECONDS = 2.0f;
		constexpr float HIGHLIGHT_FADE_SECONDS = 0.5f;
		constexpr std::size_t PAGE_COUNT = static_cast<std::size_t>(page_id::count);
		constexpr const char* RELOAD_POPUP_NAME = "Reload CompSettings?";
		constexpr const char* RESET_POPUP_NAME = "Reset Settings?";
		constexpr const char* RELOAD_MENU_NAME = "##menu_reload";
		constexpr const char* HELP_POPUP_NAME = "Having trouble?";
		constexpr const char* PRESET_DIR = "rtx_comp\\addon_settings\\presets";

		// containers that open for a search result need a few frames until their layout is known
		constexpr int GO_TO_FRAMES = 4;

		struct page_s
		{
			page_id id;
			const char* label;
			const char* icon;
			void (*draw)();
			bool creator;
			bool has_settings;
		};

		// in the order of page_id
		constexpr page_s PAGES[] =
		{
			{ page_id::home, "Home", ICON_FA_HOME, pages::home, false, false },
			{ page_id::water, "Water", ICON_FA_WATER, pages::water, false, true },
			{ page_id::weather, "Weather", ICON_FA_CLOUD_SUN_RAIN, pages::weather, false, true },
			{ page_id::world, "World", ICON_FA_CITY, pages::world, false, true },
			{ page_id::phone, "Phone", ICON_FA_MOBILE_ALT, pages::phone, false, true },
			{ page_id::other, "Other", ICON_FA_COG, pages::other, false, true },
			{ page_id::tools, "Tools", ICON_FA_TOOLS, pages::tools, false, true },
			{ page_id::help, "Help", ICON_FA_QUESTION_CIRCLE, pages::help, false, false },
			{ page_id::about, "About", ICON_FA_INFO_CIRCLE, pages::about, false, false },
			{ page_id::world_edits, "World Edits", ICON_FA_MAP_MARKED_ALT, pages::world_edits, true, false },
			{ page_id::lights, "Lights", ICON_FA_LIGHTBULB, pages::lights, true, true },
			{ page_id::culling, "Culling", ICON_FA_EYE_SLASH, pages::culling, true, true },
			{ page_id::debug, "Debug", ICON_FA_BUG, pages::debug, true, false },
		};

		static_assert(std::size(PAGES) == PAGE_COUNT);

		struct index_entry_s
		{
			comp_settings::variable* var = nullptr;
			std::string label;
			std::string path;
			std::string haystack;
			const char* group = nullptr;
			page_id page = page_id::count; // count: the setting has no widget
			bool in_advanced = false;
		};

		struct presets_s
		{
			std::vector<std::string> names;
			std::string active;
			bool loaded = false;
		};

		struct state_s
		{
			bool initialized = false;
			page_id selected = page_id::home;
			page_id drawing = page_id::count; // page whose body runs, count outside of a page
			bool advanced_open[PAGE_COUNT] = {};
			std::unordered_map<ImGuiID, float> container_heights;
			ImGuiWindow* page_window = nullptr;
			char search[64] = {};

			std::vector<index_entry_s> index;
			bool indexing = false;
			bool index_in_advanced = false;
			const char* index_group = nullptr;
			const char* index_category = nullptr;

			std::size_t go_to_entry = 0u;
			int go_to_frames = 0;
			float highlight_time = 0.0f;

			bool has_addon_files = false;
			bool reload_with_addons = false;
			float save_flash = 0.0f;
			char preset_filter[48] = {};
			presets_s presets;
			widgets::pop_in_s help_pop_in;
		};

		state_s g_state;

		const page_s& get_page(const page_id id) {
			return PAGES[static_cast<std::size_t>(id)];
		}

		/// @return	the search result the menu is moving to, nullptr if there is none
		const index_entry_s* go_to_target()
		{
			if (g_state.go_to_frames <= 0) {
				return nullptr;
			}

			return &g_state.index[g_state.go_to_entry];
		}

		void go_to(const std::size_t entry_index)
		{
			g_state.go_to_entry = entry_index;
			g_state.go_to_frames = GO_TO_FRAMES;
			g_state.highlight_time = HIGHLIGHT_SECONDS;
			g_state.selected = g_state.index[entry_index].page;
			g_state.search[0] = '\0';
		}

		bool search_active()
		{
			for (const char* c = g_state.search; *c; c++)
			{
				if (*c != ' ') {
					return true;
				}
			}

			return false;
		}

		void container(const char* label, const char* icon, const std::function<void()>& body, const bool default_open, const bool force_open)
		{
			const auto im = imgui::get();

			if (force_open) {
				ImGui::SetNextItemOpen(true);
			}

			float& height = g_state.container_heights[ImGui::GetID(label)];
			height = ImGui::Widget_ContainerWithCollapsingTitle(label, height, body, default_open, icon, &im->ImGuiCol_ContainerBackground, &im->ImGuiCol_ContainerBorder);
		}

#pragma region INDEX

		void record_setting(const char* label, comp_settings::variable& var)
		{
			for (const auto& entry : g_state.index)
			{
				if (entry.var == &var) {
					return;
				}
			}

			index_entry_s entry;
			entry.var = &var;
			entry.page = g_state.drawing;
			entry.in_advanced = g_state.index_in_advanced;
			entry.group = g_state.index_group;

			entry.label = label;
			if (const auto id_pos = entry.label.find("##"); id_pos != std::string::npos) {
				entry.label.erase(id_pos);
			}

			entry.path = get_page(g_state.drawing).label;
			if (g_state.index_in_advanced) {
				entry.path += " > Advanced";
			}

			if (g_state.index_group) {
				entry.path += " > "s + g_state.index_group;
			}

			if (g_state.index_category) {
				entry.path += " > "s + g_state.index_category;
			}

			entry.haystack = shared::utils::str_to_lower(entry.label + "\n" + var.m_name + "\n" + var.m_desc);
			g_state.index.push_back(std::move(entry));
		}

		/// Runs every page with settings once to collect where each setting is drawn, then adds the settings that have no widget.
		void build_index()
		{
			ImGui::SetNextWindowPos(ImVec2(-10000.0f, -10000.0f));
			ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f));
			ImGui::Begin("##menu_index_pass", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

			// items of a window that skips them return before they draw or react to input
			ImGui::GetCurrentWindow()->SkipItems = true;
			ImGui::BeginDisabled();

			g_state.indexing = true;
			for (const auto& page : PAGES)
			{
				if (page.has_settings)
				{
					g_state.drawing = page.id;
					page.draw();
				}
			}

			g_state.indexing = false;
			g_state.drawing = page_id::count;

			ImGui::EndDisabled();
			ImGui::End();

			const auto vars = comp_settings::get();
			const std::size_t var_count = sizeof(*vars) / sizeof(comp_settings::variable);

			for (std::size_t i = 0u; i < var_count; i++)
			{
				const auto var = reinterpret_cast<comp_settings::variable*>(reinterpret_cast<char*>(vars) + i * sizeof(comp_settings::variable));
				const bool has_widget = std::ranges::any_of(g_state.index, [var](const index_entry_s& entry) { return entry.var == var; });

				// the creator sections toggle is part of the sidebar
				if (has_widget || var == &vars->menu_creator_sections) {
					continue;
				}

				index_entry_s entry;
				entry.var = var;
				entry.label = var->m_name;
				entry.haystack = shared::utils::str_to_lower(std::string(var->m_name) + "\n" + var->m_desc);
				g_state.index.push_back(std::move(entry));
			}
		}

#pragma endregion

#pragma region SEARCH

		std::string value_text(comp_settings::variable& var)
		{
			const auto type = var.get_type();

			if (type == comp_settings::var_type_boolean) {
				return var.get_as<bool>() ? "true" : "false";
			}

			if (type == comp_settings::var_type_integer) {
				return std::to_string(var.get_as<int>());
			}

			const float* values = var.get_as<float*>();
			const int count = type == comp_settings::var_type_value ? 1 : static_cast<int>(type) - static_cast<int>(comp_settings::var_type_vec2) + 2;

			std::string out;
			for (int i = 0; i < count; i++)
			{
				if (i) {
					out += ", ";
				}

				out += comp_settings::variable::float_to_str(values[i]);
			}

			return out;
		}

		void draw_search_row(const std::size_t entry_index)
		{
			const index_entry_s& entry = g_state.index[entry_index];
			const bool on_page = entry.page != page_id::count;

			ImGui::TableNextRow();
			ImGui::PushID(entry.var);

			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(entry.label.c_str());
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
				ImGui::SetItemTooltipBlur("%s", entry.var->get_tooltip_string().c_str());
			}

			if (on_page)
			{
				ImGui::SameLine(0.0f, 12.0f);
				ImGui::TextDisabled("%s", entry.path.c_str());
			}

			ImGui::TableNextColumn();
			if (on_page && entry.var->get_type() == comp_settings::var_type_boolean)
			{
				if (compsettings_switch_widget("##value", *entry.var)) {
					settings::on_changed(*entry.var);
				}
			}
			else
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(value_text(*entry.var).c_str());
			}

			ImGui::TableNextColumn();
			if (on_page && ImGui::Button("Go to")) {
				go_to(entry_index);
			}

			ImGui::PopID();
		}

		void draw_search_results(const float height, const bool creator_sections)
		{
			std::vector<std::string> words;
			{
				std::istringstream stream(shared::utils::str_to_lower(g_state.search));
				for (std::string word; stream >> word;) {
					words.push_back(word);
				}
			}

			const auto matches = [&words](const index_entry_s& entry)
				{
					return std::ranges::all_of(words, [&entry](const std::string& word) { return entry.haystack.find(word) != std::string::npos; });
				};

			std::uint32_t shown = 0u;
			std::uint32_t hidden_creator = 0u;

			const auto draw_bucket = [&](const page_id id, const char* icon, const char* label, const bool listed)
				{
					bool table_open = false;

					for (std::size_t i = 0u; i < g_state.index.size(); i++)
					{
						if (g_state.index[i].page != id || !matches(g_state.index[i])) {
							continue;
						}

						if (!listed)
						{
							hidden_creator++;
							continue;
						}

						if (!table_open)
						{
							ImGui::Spacing(0, 8);
							ImGui::TextUnformatted(icon);
							ImGui::SameLine(0.0f, 8.0f);
							ImGui::PushFont(shared::imgui::font::BOLD);
							ImGui::TextUnformatted(label);
							ImGui::PopFont();
							ImGui::Spacing(0, 2);

							if (!ImGui::BeginTable(label, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX)) {
								return;
							}

							table_open = true;
							ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch);
							ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthFixed, 170.0f);
							ImGui::TableSetupColumn("##go_to", ImGuiTableColumnFlags_WidthFixed, 60.0f);
						}

						draw_search_row(i);
						shown++;
					}

					if (table_open) {
						ImGui::EndTable();
					}
				};

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (ImGui::BeginChild("##menu_search_results", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				for (const auto& page : PAGES) {
					draw_bucket(page.id, page.icon, page.label, !page.creator || creator_sections);
				}

				draw_bucket(page_id::count, ICON_FA_FILE_ALT, "Only in comp_settings.toml", true);

				if (!shown)
				{
					ImGui::Spacing(0, 8);
					ImGui::TextDisabled("No setting matches the search.");
				}

				if (hidden_creator)
				{
					ImGui::Spacing(0, 8);
					ImGui::TextDisabled("%u more in the creator sections, enable 'Creator Sections' in the sidebar to list them.", hidden_creator);
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

#pragma endregion

#pragma region SIDEBAR

		void reload_settings_popup()
		{
			if (!ImGui::BeginPopupModal(RELOAD_POPUP_NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}

			// addon files can show up while the game runs
			g_state.has_addon_files = !shared::utils::get_sorted_files("rtx_comp\\addon_settings", ".toml").empty();

			shared::imgui::draw_background_blur();
			ImGui::Spacing(0.0f, 0.0f);

			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			const auto line1_str = "You'll loose all unsaved changes if you continue!   ";
			const auto line2_str = "To save your changes, use:";
			const auto line3_str = "Save Settings";

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
			ImGui::TextUnformatted(line1_str);

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line2_str).x * 0.5f));
			ImGui::TextUnformatted(line2_str);

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
			ImGui::TextUnformatted(line3_str);
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0);
			ImGui::SameLine();

			const ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				if (g_state.reload_with_addons) {
					comp_settings::load_all_settings();
				} else {
					comp_settings::load_comp_settings_only();
				}

				g_state.reload_with_addons = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size))
			{
				g_state.reload_with_addons = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		std::vector<std::string> get_presets(const std::string& dir_path)
		{
			std::vector<std::string> presets;
			std::unordered_set<std::string> unique_presets;

			std::filesystem::path dir(shared::globals::root_path + "\\" + dir_path);

			if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
				return presets;
			}

			for (const auto& entry : std::filesystem::directory_iterator(dir))
			{
				if (!entry.is_regular_file()) {
					continue;
				}

				const auto ext = entry.path().extension().string();

				if (ext == ".toml" || ext == ".conf") {
					unique_presets.insert(entry.path().stem().string());
				}
			}

			presets.assign(unique_presets.begin(), unique_presets.end());
			std::sort(presets.begin(), presets.end());

			return presets;
		}

		void apply_preset(const std::string& name)
		{
			// comp settings (TOML)
			shared::common::log("ImGui", std::format("Preset: Parsing Addon CompSetting 'rtx_comp/addon_settings/presets/{}' ...", name), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			comp_settings::parse_toml("presets\\" + name + ".toml");

			// remix config (CONF)
			shared::common::log("ImGui", std::format("Preset: Parsing Addon Config 'rtx_comp/addon_settings/presets/{}' ...", name), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
			remix_vars::parse_and_apply_conf("rtx_comp\\addon_settings\\presets\\", name + ".conf", 0.1f, true);
		}

		void reset_settings_popup()
		{
			if (!ImGui::BeginPopupModal(RESET_POPUP_NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}

			shared::imgui::draw_background_blur();
			ImGui::Spacing(0.0f, 0.0f);

			const auto half_width = ImGui::GetContentRegionMax().x * 0.5f;
			const auto line1_str = "You'll loose all unsaved changes if you continue!   ";
			const auto line2_str = "To save your changes, use:";
			const auto line3_str = "Save Settings";

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line1_str).x * 0.5f));
			ImGui::TextUnformatted(line1_str);

			ImGui::Spacing();
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line2_str).x * 0.5f));
			ImGui::TextUnformatted(line2_str);

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::SetCursorPosX(5.0f + half_width - (ImGui::CalcTextSize(line3_str).x * 0.5f));
			ImGui::TextUnformatted(line3_str);
			ImGui::PopFont();

			ImGui::Spacing(0, 8);
			ImGui::Spacing(0, 0);
			ImGui::SameLine();

			const ImVec2 button_size(half_width - 6.0f - ImGui::GetStyle().WindowPadding.x, 0.0f);
			if (ImGui::Button("Reload", button_size))
			{
				comp_settings::load_all_settings();
				g_state.presets.active.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0, 6.0f);
			if (ImGui::Button("Cancel", button_size)) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		void refresh_presets()
		{
			auto& presets = g_state.presets;

			presets.names = get_presets(PRESET_DIR);
			presets.loaded = true;

			if (std::ranges::find(presets.names, presets.active) == presets.names.end()) {
				presets.active.clear();
			}
		}

		/// Panel of the reload row: reload actions and the list of presets. The modals it leads to are opened by the sidebar,
		/// 'wants_reload' and 'wants_reset' are set for them.
		/// @param anchor	lower left corner of the panel
		void reload_panel(const ImVec2& anchor, bool& wants_reload, bool& wants_reset)
		{
			const auto im = imgui::get();
			const ImGuiStyle& style = ImGui::GetStyle();

			ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
			ImGui::SetNextWindowSize(ImVec2(RELOAD_PANEL_WIDTH, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_PopupBg, im->ImGuiCol_ContainerBackground);
			ImGui::PushStyleColor(ImGuiCol_Border, im->ImGuiCol_ContainerBorder);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			const bool open = ImGui::BeginPopup(RELOAD_MENU_NAME, ImGuiWindowFlags_NoMove);
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);

			if (!open) {
				return;
			}

			// the blurred scene replaces the popup background, the container colour goes back on top of it
			shared::imgui::draw_window_blur();
			ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(),
				ImGui::ColorConvertFloat4ToU32(im->ImGuiCol_ContainerBackground), style.PopupRounding);

			auto& presets = g_state.presets;
			bool close = false;

			if (widgets::sidebar_item(ICON_FA_REDO, "Reload settings", false, PANEL_ROW_HEIGHT))
			{
				g_state.reload_with_addons = false;
				wants_reload = true;
				close = true;
			} TT("Reload the comp_settings file, unsaved changes are lost.");

			if (g_state.has_addon_files)
			{
				if (widgets::sidebar_item(ICON_FA_LAYER_GROUP, "Reload settings + addons", false, PANEL_ROW_HEIGHT))
				{
					g_state.reload_with_addons = true;
					wants_reload = true;
					close = true;
				} TT("Reload the comp_settings file and all addon files stored in 'rtx_comp/addon_settings/");
			}

			ImGui::Spacing(0, 2);
			ImGui::Separator();
			ImGui::Spacing(0, 2);

			const float refresh_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + PANEL_HEADING_INDENT);
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("PRESETS");
			TT("Setting presets to quickly change the look of the game.\n"
				"Files can be found in 'rtx_comp/addon_settings/presets'");

			ImGui::SameLine(0.0f, 0.0f);
			ImGui::SetCursorPosX(refresh_x);
			if (shared::imgui::editor::icon_button(ICON_FA_REDO "##refresh_presets")) {
				refresh_presets();
			} TT("Refresh presets found in 'rtx_comp/addon_settings/presets'");

			std::string filter;
			if (presets.names.size() > PRESET_FILTER_MIN_COUNT)
			{
				widgets::search_field("##preset_filter", g_state.preset_filter, sizeof(g_state.preset_filter), ICON_FA_SEARCH "  Filter presets", ImGui::GetContentRegionAvail().x);
				filter = shared::utils::str_to_lower(g_state.preset_filter);
			}

			const std::string* picked = nullptr;
			const float list_height = (PANEL_ROW_HEIGHT + style.ItemSpacing.y) * PRESET_ROWS_VISIBLE - style.ItemSpacing.y;

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (ImGui::BeginChild("##preset_list", ImVec2(0.0f, list_height)))
			{
				if (widgets::sidebar_item(ICON_FA_BAN, "None", presets.active.empty(), PANEL_ROW_HEIGHT))
				{
					wants_reset = true;
					close = true;
				} TT("Resets everything back to how the game initially set things up\n"
					 "by reloading the comp_settings file and all addon files stored in 'rtx_comp/addon_settings/");

				ImGui::PushID("##preset_files");
				for (const auto& name : presets.names)
				{
					if (!filter.empty() && shared::utils::str_to_lower(name).find(filter) == std::string::npos) {
						continue;
					}

					if (widgets::sidebar_item(ICON_FA_PALETTE, name.c_str(), name == presets.active, PANEL_ROW_HEIGHT)) {
						picked = &name;
					}
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();

			if (picked)
			{
				presets.active = *picked;
				apply_preset(presets.active);
				close = true;
			}

			if (close) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		/// @return	height of what draw_sidebar_actions draws
		float sidebar_actions_height()
		{
			const float spacing = ImGui::GetStyle().ItemSpacing.y;
			return (SIDEBAR_ROW_HEIGHT + spacing) * 3.0f + spacing * 2.0f + 5.0f;
		}

		void draw_sidebar_actions()
		{
			const auto& cs = comp_settings::get();
			bool wants_reload = false;
			bool wants_reset = false;

			ImGui::Separator();
			ImGui::Spacing(0, 4);

			if (widgets::sidebar_item(ICON_FA_SAVE, "Save Settings", false, SIDEBAR_ROW_HEIGHT, g_state.save_flash / SAVE_FLASH_SECONDS))
			{
				comp_settings::write_comp_settings_toml();
				g_state.save_flash = SAVE_FLASH_SECONDS;
			} TT("Saves current settings to 'comp_settings.toml'. Ignores settings that were modified by addon setting files.");

			g_state.save_flash = std::max(g_state.save_flash - ImGui::GetIO().DeltaTime, 0.0f);

			if (widgets::sidebar_item(ICON_FA_REDO, "Reload", false, SIDEBAR_ROW_HEIGHT, 0.0f, ICON_FA_CHEVRON_RIGHT))
			{
				if (!g_state.presets.loaded) {
					refresh_presets();
				}

				g_state.preset_filter[0] = '\0';
				ImGui::OpenPopup(RELOAD_MENU_NAME);
			} TT("Reload the settings or apply a preset.");

			reload_panel(ImVec2(ImGui::GetItemRectMax().x + RELOAD_PANEL_GAP, ImGui::GetItemRectMax().y), wants_reload, wants_reset);

			if (wants_reload && !ImGui::IsPopupOpen(RELOAD_POPUP_NAME)) {
				ImGui::OpenPopup(RELOAD_POPUP_NAME);
			}

			if (wants_reset && !ImGui::IsPopupOpen(RESET_POPUP_NAME)) {
				ImGui::OpenPopup(RESET_POPUP_NAME);
			}

			reload_settings_popup();
			reset_settings_popup();

			bool creator_sections = cs->menu_creator_sections._bool();
			if (widgets::sidebar_switch(ICON_FA_WRENCH, "Creator", creator_sections)) {
				cs->menu_creator_sections.set_var(creator_sections);
			} TT("Lists the sections for creators: world edits, lights, culling and debug.");
		}

		void draw_sidebar(const float height, const bool creator_sections)
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
			if (ImGui::BeginChild("##menu_sidebar", ImVec2(SIDEBAR_WIDTH, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				if (ImGui::BeginChild("##menu_sidebar_pages", ImVec2(0.0f, ImGui::GetContentRegionAvail().y - sidebar_actions_height())))
				{
					for (const auto& page : PAGES)
					{
						if (page.creator && !creator_sections) {
							continue;
						}

						if (page.id == page_id::world_edits) {
							widgets::sidebar_group_label("CREATOR");
						}

						if (widgets::sidebar_item(page.icon, page.label, page.id == g_state.selected))
						{
							g_state.selected = page.id;
							g_state.search[0] = '\0';
						}
					}
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();

				draw_sidebar_actions();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

#pragma endregion

		void draw_page(const float height)
		{
			const page_s& page = get_page(g_state.selected);

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			if (ImGui::BeginChild(page.label, ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				g_state.page_window = ImGui::GetCurrentWindow();
				g_state.drawing = page.id;
				page.draw();
				g_state.drawing = page_id::count;
				g_state.page_window = nullptr;

				if (g_state.go_to_frames > 0) {
					g_state.go_to_frames--;
				}

				g_state.highlight_time = std::max(g_state.highlight_time - ImGui::GetIO().DeltaTime, 0.0f);
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}

#pragma region HOME

		void home_new_settings()
		{
			ImGui::Spacing(0, 4);
			mark_new_settings(true);
			widgets::grid_begin("##home_rows");
			{
				settings::wind_waves();
				settings::interactive_water();
				settings::ped_wounds();
				settings::fake_distant_cars();
				settings::distant_light_sprites();
				settings::static_lod_fade();
				settings::phone_mode();
				settings::remix_atmosphere();
			}
			widgets::grid_end();
			mark_new_settings(false);
			ImGui::Spacing(0, 4);
		}

		void help_popup()
		{
			const float progress = widgets::pop_in_advance(g_state.help_pop_in);
			widgets::pop_in_push(progress, ImGui::GetMainViewport()->GetCenter());

			if (ImGui::BeginPopupModal(HELP_POPUP_NAME, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove))
			{
				shared::imgui::draw_background_blur();
				ImGui::Spacing(0, 4);
				help_links();
				ImGui::Spacing(0, 12);

				if (ImGui::Button("Close", ImVec2(360.0f, 0.0f))) {
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			widgets::pop_in_pop();
		}

#pragma endregion
	}

	bool is_indexing() {
		return g_state.indexing;
	}

	void apply_colors()
	{
		const auto& cs = comp_settings::get();
		const auto im = imgui::get();

		const auto color = [](comp_settings::variable& var)
			{
				const float* v = var.get_as<float*>();
				return ImVec4(v[0], v[1], v[2], v[3]);
			};

		im->ImGuiCol_ContainerBackground = color(cs->menu_color_container);
		im->ImGuiCol_ContainerBorder = color(cs->menu_color_container_border);
		shared::imgui::widget_style.vertical_fade_container_bg_start = color(cs->menu_color_category_fade_start);
		shared::imgui::widget_style.vertical_fade_container_bg_end = color(cs->menu_color_category_fade_end);
		widgets::menu_style.accent = color(cs->menu_color_accent);
		widgets::menu_style.badge_accent = color(cs->menu_color_badge);
	}

	void simple(const char* label, const char* icon, const std::function<void()>& body)
	{
		if (g_state.indexing)
		{
			body();
			return;
		}

		const auto target = go_to_target();
		container(label, icon, body, true, target && target->page == g_state.drawing && !target->in_advanced && !target->group);
	}

	void advanced(const std::function<void()>& body)
	{
		if (g_state.indexing)
		{
			g_state.index_in_advanced = true;
			body();
			g_state.index_in_advanced = false;
			return;
		}

		bool& open = g_state.advanced_open[static_cast<std::size_t>(g_state.drawing)];
		if (const auto target = go_to_target(); target && target->page == g_state.drawing && target->in_advanced) {
			open = true;
		}

		ImGui::Spacing(0, 6);
		const bool show = widgets::fold_header("Advanced", open);
		ImGui::Spacing(0, 8);

		if (show) {
			body();
		}
	}

	void group(const char* label, const char* icon, const std::function<void()>& body, const bool default_open)
	{
		if (g_state.indexing)
		{
			g_state.index_group = label;
			body();
			g_state.index_group = nullptr;
			return;
		}

		const auto target = go_to_target();
		container(label, icon, body, default_open, target && target->page == g_state.drawing && target->group && std::strcmp(target->group, label) == 0);
	}

	void category(const char* label, const std::function<void()>& body)
	{
		if (g_state.indexing)
		{
			g_state.index_category = label;
			body();
			g_state.index_category = nullptr;
			return;
		}

		ImGui::Widget_CategoryWithVerticalLabel(label, body);
	}

	bool setting_begin(const char* label, comp_settings::variable& var)
	{
		if (g_state.indexing)
		{
			record_setting(label, var);
			return false;
		}

		if (const auto target = go_to_target(); target && target->var == &var && target->page == g_state.drawing && g_state.page_window) {
			ImGui::SetScrollFromPosY(g_state.page_window, ImGui::GetCursorScreenPos().y - g_state.page_window->Pos.y, 0.35f);
		}

		return true;
	}

	void setting_end(const comp_settings::variable& var)
	{
		if (g_state.highlight_time <= 0.0f) {
			return;
		}

		const index_entry_s& target = g_state.index[g_state.go_to_entry];
		if (target.var != &var || target.page != g_state.drawing) {
			return;
		}

		ImVec4 outline_color = widgets::menu_style.accent;
		outline_color.w *= std::min(g_state.highlight_time / HIGHLIGHT_FADE_SECONDS, 1.0f);

		ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() - ImVec2(4.0f, 2.0f), ImGui::GetItemRectMax() + ImVec2(4.0f, 2.0f),
			ImGui::ColorConvertFloat4ToU32(outline_color), ImGui::GetStyle().FrameRounding, ImDrawFlags_None, 2.0f);
	}

	void draw(const float footer_height)
	{
		if (!g_state.initialized)
		{
			build_index();
			g_state.has_addon_files = !shared::utils::get_sorted_files("rtx_comp\\addon_settings", ".toml").empty();
			g_state.initialized = true;
		}

		const bool creator_sections = comp_settings::get()->menu_creator_sections._bool();
		if (get_page(g_state.selected).creator && !creator_sections) {
			g_state.selected = page_id::home;
		}

		const float height = ImGui::GetContentRegionAvail().y - footer_height;
		draw_sidebar(height, creator_sections);

		ImGui::SameLine(0.0f, SIDEBAR_GAP);
		ImGui::BeginGroup();
		{
			const float top = ImGui::GetCursorPosY();
			widgets::search_field("##menu_search", g_state.search, sizeof(g_state.search), ICON_FA_SEARCH "  Search settings", ImGui::GetContentRegionAvail().x);
			ImGui::Spacing(0, 4);

			const float content_height = height - (ImGui::GetCursorPosY() - top);
			if (search_active()) {
				draw_search_results(content_height, creator_sections);
			} else {
				draw_page(content_height);
			}
		}
		ImGui::EndGroup();
	}

	void help_links()
	{
		ImGui::PushFont(shared::imgui::font::FONTS::BOLD_LARGE);
		ImGui::SeparatorText("Graphic Settings");
		ImGui::PopFont();
		ImGui::Spacing(0, 2);

		ImGui::Indent(4);
		ImGui::TextURL("Game Graphic Settings", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#game-graphic-settings", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("60 FPS Cap Suggestion", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#suggestion-60-fps-target", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("Performance / Quality Tipps", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#performance--quality-tipps", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("Weather / Sky Tweaking", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#weather--sky-tweaking", true);
		ImGui::Unindent(4);

		ImGui::Spacing(0, 6);
		ImGui::PushFont(shared::imgui::font::FONTS::BOLD_LARGE);
		ImGui::SeparatorText("Post Processing");
		ImGui::PopFont();
		ImGui::Spacing(0, 2);

		ImGui::Indent(4);
		ImGui::TextURL("Timecyle Post Processing", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#timecyle-post-processing", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("Remix Post Processing and HDR", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#remix-post-processing-and-hdr", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("DLSS5 / Neural Rendering", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#dlss5--neural-rendering", true);
		ImGui::Unindent(4);

		ImGui::Spacing(0, 6);
		ImGui::PushFont(shared::imgui::font::FONTS::BOLD_LARGE);
		ImGui::SeparatorText("Troubleshooting");
		ImGui::PopFont();
		ImGui::Spacing(0, 2);

		ImGui::Indent(4);
		ImGui::TextURL("Stuttering", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#stuttering", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("Input Lag", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#input-lag", true);

		ImGui::Spacing(0, 2);
		ImGui::TextURL("Instability", "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides#instability", true);
		ImGui::Unindent(4);
	}

	void pages::home()
	{
		ImGui::Spacing(0, 8);
		ImGui::Indent(4);
		ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
		ImGui::TextUnformatted("GTA IV - RTX Remix Compatibility Mod");
		ImGui::PopFont();
		ImGui::TextWrapped("Settings are sorted by topic on the left. Every page starts with the options most players change, the detailed ones are under Advanced.");
		ImGui::Unindent(4);
		ImGui::Spacing(0, 12);

		group("New", ICON_FA_STAR, home_new_settings, true);

		ImGui::Spacing(0, 8);
		if (ImGui::Button(ICON_FA_LIFE_RING "  Having trouble?", ImVec2(220.0f, 0.0f)))
		{
			ImGui::OpenPopup(HELP_POPUP_NAME);
			widgets::pop_in_restart(g_state.help_pop_in);
		} TT("Links to the troubleshooting guides");

		help_popup();
	}

	void pages::help()
	{
		ImGui::Spacing(0, 8);
		help_links();
	}
}
