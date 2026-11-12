#include "std_include.hpp"
#include "imgui_light_editor.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>

#include "comp_settings.hpp"
#include "free_camera.hpp"
#include "game_lights.hpp"
#include "imgui.hpp"
#include "map_settings.hpp"
#include "remix_lights.hpp"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "ImGuizmo.h"
#include "shared/common/toml_ext.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/font_defines.hpp"

#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

namespace gta4::light_editor
{
	namespace
	{
		using override_s = map_settings::light_override_s;
		using overrides_file_s = map_settings::light_overrides_toml_info_s;
		using lights_file_s = map_settings::lights_toml_info_s;
		using category_s = map_settings::light_override_category_info_s;

		constexpr const char* LIGHT_TWEAKS_DIR = "rtx_comp\\light_tweaks";
		constexpr int MAIN_LIGHT = -1; // attached index of the light that owns the hash
		constexpr int NO_CATEGORY = -1;
		constexpr std::size_t UNDO_LIMIT = 64u;
		constexpr float SCREEN_PADDING = 16.0f; // default distance of the toolbar and the list to the screen edges and to each other

		// cone cosines of a light that becomes a spotlight by override, the game has no values for it
		const float DEFAULT_OUTER_CONE = cosf(static_cast<float>(DEG2RAD(45.0f)));
		const float DEFAULT_INNER_CONE = cosf(static_cast<float>(DEG2RAD(30.0f)));

		constexpr ImU32 COL_SELECTED = IM_COL32(60, 200, 255, 255);
		constexpr ImU32 COL_HOVERED = IM_COL32(255, 220, 60, 255);
		constexpr ImU32 COL_OVERRIDE = IM_COL32(90, 214, 106, 255);
		constexpr ImU32 COL_OVERRIDE_OTHER_FILE = IM_COL32(200, 140, 255, 255);
		constexpr ImU32 COL_IGNORED = IM_COL32(226, 75, 74, 255);
		constexpr ImU32 COL_PLAIN = IM_COL32(255, 255, 255, 150);
		const ImVec4 COL_BUTTON_ACTIVE = ImVec4(0.22f, 0.5f, 0.26f, 1.0f);
		const ImVec4 COL_BUTTON_UNSAVED = ImVec4(0.72f, 0.5f, 0.26f, 1.0f);
		const ImVec4 COL_TEXT_GAME_VALUE = ImVec4(0.56f, 0.56f, 0.56f, 1.0f); // #8F8F8F, a property without override
		const ImVec4 COL_TEXT_OVERRIDDEN = ImVec4(1.0f, 0.69f, 0.19f, 1.0f); // #FFB031

		// a translated light of this frame
		struct scene_light_s
		{
			std::uint64_t hash = 0u;	// hash of the light for map settings (the base light's hash for attached lights)
			int attached_index = MAIN_LIGHT;
			game::CLightSource def = {};
			Vector pos;
			ImVec2 screen = {};
			float distance = 0.0f;
			bool on_screen = false;
			bool in_front = false;
			bool ignored = false;
			bool filler = false;
			bool instanced = false;		// several lights share the hash (vehicle lights), a position override makes no sense
		};

		struct selection_s
		{
			std::uint64_t hash = 0u;
			int attached_index = MAIN_LIGHT;

			bool operator==(const selection_s& other) const {
				return hash == other.hash && attached_index == other.attached_index;
			}
		};

		// everything an edit can change, for undo / redo
		struct snapshot_s
		{
			std::unordered_map<std::string, overrides_file_s> overrides;
			std::unordered_map<std::string, lights_file_s> lights;
		};

		struct state_s
		{
			bool open = false;
			bool show_list = true;
			bool show_markers = true;
			bool show_labels = false;
			bool list_all_in_file = false;
			bool rmb_game_input = false;
			bool local_space = false;
			bool rotate_mode = false;
			bool pin_properties = false;
			float view_distance = 30.0f; // lights farther away get no marker and no list entry

			std::string active_file;	// edits go to this file
			std::set<std::string> dirty_files;

			selection_s selected;
			selection_s hovered;		// built during the frame
			selection_s hovered_drawn;	// the hovered light of the last frame, markers are drawn before the list sets 'hovered'
			selection_s context;
			bool place_properties = false;
			bool open_context_menu = false;
			bool open_new_file_popup = false;
			bool open_close_popup = false;
			bool open_category_popup = false;
			int category_popup_index = NO_CATEGORY;
			bool new_category_for_context = false;

			game::CLightSource selected_def = {};
			bool selected_def_valid = false;
			bool selected_instanced = false;

			float gizmo_matrix[16] = {};
			bool step_held = false;

			std::vector<scene_light_s> lights;
			ImGuiTextFilter filter;
			std::optional<override_s> clipboard;

			std::vector<snapshot_s> undo;
			std::vector<snapshot_s> redo;
			snapshot_s frame_snapshot;
			bool in_edit = false;

			snapshot_s saved; // the files as they are on disk, for discard
			bool saved_valid = false;

			std::string name_buffer;
			std::string comment_buffer;
			std::string status;
			double status_until = 0.0;
		};

		state_s g;

#pragma region FILES_AND_OVERRIDES

		auto& settings() {
			return map_settings::get_map_settings();
		}

		/// all light tweak files, highest priority first
		std::vector<std::string> sorted_files()
		{
			std::set<std::string> names;
			for (const auto& name : settings().light_overrides_toml_info | std::views::keys) {
				names.insert(name);
			}

			for (const auto& name : settings().lights_toml_info | std::views::keys) {
				names.insert(name);
			}

			return std::vector<std::string>(names.begin(), names.end());
		}

		bool file_exists(const std::string& file) {
			return settings().light_overrides_toml_info.contains(file) || settings().lights_toml_info.contains(file);
		}

		void ensure_active_file()
		{
			if (!g.active_file.empty() && file_exists(g.active_file)) {
				return;
			}

			const auto files = sorted_files();
			g.active_file = files.empty() ? std::string() : files.front();
		}

		/// override of a hash in one file, category_index receives its category (NO_CATEGORY = uncategorized)
		override_s* find_override(const std::string& file, const std::uint64_t hash, int* category_index = nullptr)
		{
			auto& files = settings().light_overrides_toml_info;
			const auto file_it = files.find(file);
			if (file_it == files.end()) {
				return nullptr;
			}

			auto& info = file_it->second;
			for (auto i = 0; i < static_cast<int>(info.categories.size()); i++)
			{
				if (const auto it = info.categories[i].overrides.find(hash); it != info.categories[i].overrides.end())
				{
					if (category_index) {
						*category_index = i;
					}
					return &it->second;
				}
			}

			if (const auto it = info.flat_overrides.find(hash); it != info.flat_overrides.end())
			{
				if (category_index) {
					*category_index = NO_CATEGORY;
				}
				return &it->second;
			}

			return nullptr;
		}

		/// the file whose override of the hash is in effect (highest priority), empty if there is none
		std::string owner_file(const std::uint64_t hash)
		{
			for (const auto& file : sorted_files())
			{
				if (find_override(file, hash)) {
					return file;
				}
			}

			return {};
		}

		/// the file that ignores the hash, empty if none does
		std::string ignoring_file(const std::uint64_t hash)
		{
			for (const auto& file : sorted_files())
			{
				if (const auto it = settings().lights_toml_info.find(file); it != settings().lights_toml_info.end() && it->second.ignored_lights.contains(hash)) {
					return file;
				}
			}

			return {};
		}

		void set_status(std::string text)
		{
			g.status = std::move(text);
			g.status_until = ImGui::GetTime() + 3.0;
		}

		snapshot_s take_snapshot() {
			return { settings().light_overrides_toml_info, settings().lights_toml_info };
		}

		/// call after every change: the first change of an interaction puts the state before it on the undo stack
		void commit(const std::string& file)
		{
			if (!g.in_edit)
			{
				g.undo.emplace_back(g.frame_snapshot);
				if (g.undo.size() > UNDO_LIMIT) {
					g.undo.erase(g.undo.begin());
				}

				g.redo.clear();
				g.in_edit = true;
			}

			g.dirty_files.insert(file);
			map_settings::rebuild_light_overrides_from_toml_info();
		}

		void restore_snapshot(std::vector<snapshot_s>& from, std::vector<snapshot_s>& to)
		{
			if (from.empty()) {
				return;
			}

			to.emplace_back(take_snapshot());
			settings().light_overrides_toml_info = std::move(from.back().overrides);
			settings().lights_toml_info = std::move(from.back().lights);
			from.pop_back();

			for (const auto& file : sorted_files()) {
				g.dirty_files.insert(file);
			}

			map_settings::rebuild_light_overrides_from_toml_info();
			g.frame_snapshot = take_snapshot();
			g.in_edit = false;
			ensure_active_file();
		}

		/// an override that holds the game's values, nothing is overridden until a value gets edited
		override_s override_from_def(const game::CLightSource& def)
		{
			override_s ov;
			ov.pos = def.mPosition;
			ov.dir = def.mDirection;
			ov.color = Vector(def.mColor);
			ov.radius = def.mRadius;
			ov.intensity = def.mIntensity;
			ov.volumetric_scale = def.mVolumeScale;

			if (def.mType == game::LT_SPOT)
			{
				ov.outer_cone_angle = def.mOuterConeAngle;
				ov.inner_cone_angle = def.mInnerConeAngle;
				ov.light_type = true;
			}
			else
			{
				ov.outer_cone_angle = DEFAULT_OUTER_CONE;
				ov.inner_cone_angle = DEFAULT_INNER_CONE;
			}

			return ov;
		}

		/// stands in for the game's light while it is not translated: an override is created from the game's values
		game::CLightSource def_from_override(const override_s& ov)
		{
			game::CLightSource def = {};
			def.mPosition = ov.pos;
			def.mDirection = ov.dir;
			def.mColor = Vector4D(ov.color.x, ov.color.y, ov.color.z, 1.0f);
			def.mRadius = ov.radius;
			def.mIntensity = ov.intensity;
			def.mVolumeScale = ov.volumetric_scale;
			def.mOuterConeAngle = ov.outer_cone_angle;
			def.mInnerConeAngle = ov.inner_cone_angle;
			def.mType = ov.light_type ? game::LT_SPOT : game::LT_POINT;
			return def;
		}

		const scene_light_s* find_scene_light(const selection_s& sel)
		{
			for (const auto& light : g.lights)
			{
				if (light.hash == sel.hash && light.attached_index == sel.attached_index) {
					return &light;
				}
			}

			return nullptr;
		}

		/// the game's values of a light, from the translated light or (if it is not around) from its override
		bool get_reference_def(const std::uint64_t hash, game::CLightSource& def)
		{
			if (const auto light = find_scene_light({ hash, MAIN_LIGHT }); light)
			{
				def = light->def;
				return true;
			}

			if (const auto owner = owner_file(hash); !owner.empty())
			{
				def = def_from_override(*find_override(owner, hash));
				return true;
			}

			return false;
		}

		void create_override(const std::uint64_t hash, const override_s* copy_from = nullptr)
		{
			if (g.active_file.empty() || find_override(g.active_file, hash)) {
				return;
			}

			override_s ov;
			if (copy_from) {
				ov = *copy_from;
			}
			else
			{
				game::CLightSource def = {};
				if (!get_reference_def(hash, def)) {
					return;
				}

				ov = override_from_def(def);
			}

			ov._order = map_settings::next_light_override_order();
			settings().light_overrides_toml_info[g.active_file].flat_overrides[hash] = std::move(ov);
			commit(g.active_file);
		}

		void remove_override(const std::string& file, const std::uint64_t hash)
		{
			const auto file_it = settings().light_overrides_toml_info.find(file);
			if (file_it == settings().light_overrides_toml_info.end()) {
				return;
			}

			for (auto& category : file_it->second.categories) {
				category.overrides.erase(hash);
			}

			file_it->second.flat_overrides.erase(hash);
			commit(file);
		}

		void move_to_category(const std::string& file, const std::uint64_t hash, const int category_index)
		{
			int current = NO_CATEGORY;
			const auto ov = find_override(file, hash, &current);
			if (!ov || current == category_index) {
				return;
			}

			auto& info = settings().light_overrides_toml_info[file];
			override_s copy = *ov;

			if (current == NO_CATEGORY) {
				info.flat_overrides.erase(hash);
			} else {
				info.categories[current].overrides.erase(hash);
			}

			if (category_index == NO_CATEGORY) {
				info.flat_overrides[hash] = std::move(copy);
			} else {
				info.categories[category_index].overrides[hash] = std::move(copy);
			}

			commit(file);
		}

		int add_category(const std::string& file, const std::string& name)
		{
			auto& info = settings().light_overrides_toml_info[file];
			for (auto i = 0; i < static_cast<int>(info.categories.size()); i++)
			{
				if (info.categories[i].category_name == name) {
					return i;
				}
			}

			category_s category;
			category.category_name = name;
			info.categories.emplace_back(std::move(category));
			return static_cast<int>(info.categories.size()) - 1;
		}

		void set_ignored(const std::uint64_t hash, const bool ignored)
		{
			auto& files = settings().lights_toml_info;
			if (ignored)
			{
				if (g.active_file.empty()) {
					return;
				}

				files[g.active_file].ignored_lights.insert(hash);
				commit(g.active_file);
				return;
			}

			for (auto& [file, info] : files)
			{
				if (info.ignored_lights.erase(hash)) {
					commit(file);
				}
			}
		}

		void set_allowed(const std::uint64_t hash, const bool allowed)
		{
			auto& files = settings().lights_toml_info;
			if (allowed)
			{
				if (!g.active_file.empty())
				{
					files[g.active_file].allow_lights.insert(hash);
					commit(g.active_file);
				}
				return;
			}

			for (auto& [file, info] : files)
			{
				if (info.allow_lights.erase(hash)) {
					commit(file);
				}
			}
		}

		void attach_light(const std::uint64_t hash)
		{
			const auto ov = find_override(g.active_file, hash);
			game::CLightSource def = {};
			
			if (!ov || !get_reference_def(hash, def)) {
				return;
			}

			override_s attached = *ov;
			attached.attached_lights.clear();

			if (!attached._use_pos) {
				attached.pos = def.mPosition;
			}

			attached.pos += Vector(0.1f, 0.1f, 0.1f);
			attached._use_pos = true;
			attached.comment = "Attached light #" + std::to_string(ov->attached_lights.size() + 1u);

			ov->attached_lights.emplace_back(std::move(attached));
			g.selected = { hash, static_cast<int>(ov->attached_lights.size()) - 1 };
			commit(g.active_file);
		}

		std::string light_name(const std::uint64_t hash)
		{
			if (const auto owner = owner_file(hash); !owner.empty())
			{
				if (const auto ov = find_override(owner, hash); !ov->comment.empty()) {
					return ov->comment;
				}
			}

			if (auto source = game_lights::get_light_source_name(hash); !source.empty()) {
				return source;
			}

			return std::format("{:X}", hash);
		}

#pragma endregion

#pragma region SAVE_AND_EXPORT

		std::string build_hash_array(const char* name, const std::unordered_set<std::uint64_t>& hashes)
		{
			std::vector<std::uint64_t> sorted(hashes.begin(), hashes.end());
			std::ranges::sort(sorted);

			std::string text = std::string(name) + " = [";
			for (auto i = 0u; i < sorted.size(); i++)
			{
				text += i % 9u ? ", " : (i ? ",\n    " : "\n    ");
				text += std::format("0x{:X}", sorted[i]);
			}

			text += "\n]";
			return text;
		}

		/// overrides of one container in the order they have in the file
		std::string build_override_entries(const std::unordered_map<std::uint64_t, override_s>& overrides, const std::string& indent)
		{
			std::vector<const std::pair<const std::uint64_t, override_s>*> sorted;
			sorted.reserve(overrides.size());

			for (const auto& entry : overrides) {
				sorted.emplace_back(&entry);
			}

			std::ranges::sort(sorted, [](const auto* a, const auto* b)
				{
					return a->second._order != b->second._order ? a->second._order < b->second._order : a->first < b->first;
				});

			std::string text;
			bool first = true;

			for (const auto* entry : sorted)
			{
				const auto& [hash, ov] = *entry;
				if (!ov.comment.empty())
				{
					if (!first) {
						text += "\n";
					}

					text += indent + "# " + ov.comment + "\n";
				}

				text += indent + "{ hash = " + std::format("0x{:X}", hash);
				if (const auto props = shared::common::toml_ext::build_override_properties(ov); !props.empty()) {
					text += ", " + props;
				}

				text += shared::common::toml_ext::build_attached_lights_array(ov.attached_lights, indent);
				text += " },\n";
				first = false;
			}

			return text;
		}

		std::string build_category(const category_s& category)
		{
			if (category.overrides.empty()) {
				return {};
			}

			std::string text;
			if (!category.category_comment.empty() && category.category_comment != category.category_name) {
				text += "    # " + category.category_comment + "\n";
			}

			text += "    { category = \"" + category.category_name + "\", overrides = [\n";
			text += build_override_entries(category.overrides, "        ");
			text += "    ]},\n";
			return text;
		}

		std::string build_overrides_array(const std::string& file)
		{
			std::string text = "LIGHT_OVERRIDES = [\n";

			if (const auto it = settings().light_overrides_toml_info.find(file); it != settings().light_overrides_toml_info.end())
			{
				for (const auto& category : it->second.categories)
				{
					if (const auto category_text = build_category(category); !category_text.empty()) {
						text += "\n" + category_text;
					}
				}

				if (!it->second.flat_overrides.empty()) {
					text += "\n" + build_override_entries(it->second.flat_overrides, "    ");
				}
			}

			text += "]";
			return text;
		}

		std::string build_file(const std::string& file)
		{
			static const std::string rule = "# " + std::string(138, '#') + "\n";

			lights_file_s lights;
			if (const auto it = settings().lights_toml_info.find(file); it != settings().lights_toml_info.end()) {
				lights = it->second;
			}

			std::string text = rule;
			text +=
				"# >> Ignore Game Lights\n"
				"#\n"
				"# :: Disables the translation of the listed game lights to remix lights\n"
				"# :: Lights are identified by a hash. World lights hash their position and need to stand still.\n"
				"#    Cutscene lights (cutscene + light name) and vehicle head / rear lights (model + light) keep their hash while moving.\n"
				"#\n"
				"# :: How to edit:\n"
				"#    > Light Editor (GUI -> Map Settings -> Light Tweaks -> Open Light Editor)\n"
				"#    > Right click a light -> Ignore light. Save in the editor's toolbar\n"
				"#\n"
				"# ~~ IGNORE_LIGHTS = [\n"
				"#     0x0, 0x1, 0x2, ...            [uint array - hex format]\n"
				"# ]\n\n";
			text += build_hash_array("IGNORE_LIGHTS", lights.ignored_lights) + "\n\n\n\n";

			text += rule;
			text +=
				"# >> Allow Game Lights\n"
				"#\n"
				"# :: Whitelists filler lights, only useful if 'ignore filler lights' is enabled\n"
				"#\n"
				"# :: How to edit:\n"
				"#    > Light Editor: right click a filler light -> Allow filler light\n"
				"#\n"
				"# ~~ ALLOW_LIGHTS = [\n"
				"#     0x0, 0x1, 0x2, ...            [uint array - hex format]\n"
				"# ]\n\n";
			text += build_hash_array("ALLOW_LIGHTS", lights.allow_lights) + "\n\n\n\n";

			text += rule;
			text +=
				"# >> Light Overrides\n"
				"#\n"
				"# :: Overrides settings of game lights and attaches additional lights to them\n"
				"#\n"
				"# :: How to edit:\n"
				"#    > Light Editor: select a light in the world or in the list, create an override and tweak it\n"
				"#\n"
				"# :: Notes:\n"
				"#    > Categories: group related overrides (optional)\n"
				"#    > Attached lights: additional lights that exist as long as their base light does\n"
				"#    > Priority: files are processed in alphabetical order (earlier files have higher priority)\n"
				"#    > Comments: the comment above an entry is kept and shown as the name of the light\n"
				"#\n"
				"# ~~ LIGHT_OVERRIDES = [\n"
				"#   {\n"
				"#       #                   comment\n"
				"#       hash =              hash of light                                   [uint - hex format]\n"
				"#       pos = []            position of light                        [opt.] [3D Vector]\n"
				"#       dir = []            direction of light                       [opt.] [3D Vector]\n"
				"#       color = []          color of light                           [opt.] [3D Vector]\n"
				"#       radius =            radius of light                          [opt.] [Float]\n"
				"#       intensity =         intensity of light                       [opt.] [Float]\n"
				"#       outer_cone_angle =  cosine of the outer cone angle           [opt.] [Float]\n"
				"#       inner_cone_angle =  cosine of the inner cone angle           [opt.] [Float]\n"
				"#       volumetric_scale =  volumetric scale of light                [opt.] [Float]\n"
				"#       light_type =        false: sphere -- true: spotlight         [opt.] [Bool]\n"
				"#       attached = [        attached lights for this hash            [opt.] [Array of light override structures]\n"
				"#       { ... light override }, { ... }]\n"
				"#   },\n"
				"#   {\n"
				"#       category =          category name                                   [String]\n"
				"#       overrides = [       light overrides                                 [Array of light override structures]\n"
				"#       { ... light override }, { ... }]\n"
				"#   },\n"
				"# ]\n\n";
			text += build_overrides_array(file) + "\n";
			return text;
		}

		bool save_file(const std::string& file)
		{
			namespace fs = std::filesystem;

			const fs::path dir = fs::path(shared::globals::root_path) / LIGHT_TWEAKS_DIR;
			const fs::path target = dir / file;
			fs::path temp = target;
			temp += ".tmp";

			std::error_code ec;
			fs::create_directories(dir, ec);

			{
				std::ofstream out(temp, std::ios::binary | std::ios::trunc);
				if (!out.is_open())
				{
					set_status("Could not write " + file);
					return false;
				}

				out << build_file(file);
			}

			fs::rename(temp, target, ec);
			if (ec)
			{
				fs::remove(temp, ec);
				set_status("Could not replace " + file);
				return false;
			}

			g.dirty_files.erase(file);
			g.saved.overrides[file] = settings().light_overrides_toml_info[file];
			g.saved.lights[file] = settings().lights_toml_info[file];
			return true;
		}

		/// puts a file back to what is on disk, a file that was never saved disappears
		void discard_file(const std::string& file)
		{
			auto& overrides = settings().light_overrides_toml_info;
			auto& lights = settings().lights_toml_info;

			if (const auto it = g.saved.overrides.find(file); it != g.saved.overrides.end()) {
				overrides[file] = it->second;
			} else {
				overrides.erase(file);
			}

			if (const auto it = g.saved.lights.find(file); it != g.saved.lights.end()) {
				lights[file] = it->second;
			} else {
				lights.erase(file);
			}

			commit(file);
			g.dirty_files.erase(file);
			ensure_active_file();
			set_status("Discarded changes of " + file);
		}

		void save_active_file()
		{
			if (!g.active_file.empty() && save_file(g.active_file)) {
				set_status("Saved " + g.active_file);
			}
		}

		void save_all_files()
		{
			const auto dirty = g.dirty_files;
			auto saved = 0u;

			for (const auto& file : dirty)
			{
				if (file_exists(file) && save_file(file)) {
					saved++;
				} else {
					g.dirty_files.erase(file);
				}
			}

			set_status(std::format("Saved {} file(s)", saved));
		}

		void copy_to_clipboard(const std::string& text, const char* what)
		{
			ImGui::SetClipboardText(text.c_str());
			set_status(std::string("Copied ") + what + " to clipboard");
		}

#pragma endregion

#pragma region SCENE

		const float* view_matrix() {
			return &game::pViewports->sceneviewport->view.m[0][0];
		}

		const float* projection_matrix() {
			return &game::pViewports->sceneviewport->proj.m[0][0];
		}

		bool has_viewport() {
			return game::pViewports && game::pViewports->sceneviewport && shared::globals::d3d_device;
		}

		/// false if the point is behind the camera or outside of the screen, screen is valid if it is in front
		bool project(const Vector& world, ImVec2& screen, bool* in_front = nullptr)
		{
			D3DVIEWPORT9 vp;
			shared::globals::d3d_device->GetViewport(&vp);

			const auto wp = world.ToD3DXVector();
			D3DXVECTOR3 projected;
			D3DXVec3Project(&projected, &wp, &vp, &game::pViewports->sceneviewport->proj, &game::pViewports->sceneviewport->view, nullptr);

			const bool front = projected.z >= 0.0f && projected.z <= 1.0f;
			if (in_front) {
				*in_front = front;
			}

			screen = ImVec2(projected.x, projected.y);
			return front && screen.x >= 0.0f && screen.y >= 0.0f && screen.x <= static_cast<float>(vp.Width) && screen.y <= static_cast<float>(vp.Height);
		}

		void collect_scene_lights()
		{
			g.lights.clear();
			if (!has_viewport()) {
				return;
			}

			const auto& overrides = settings().light_overrides;
			const Vector cam_org = &game::pViewports->sceneviewport->cameraInv.m[3][0];
			std::unordered_set<std::uint64_t> seen;

			for (const auto& [key, light] : remix_lights::get()->get_active_lights())
			{
				scene_light_s sl;
				sl.def = light.m_def;
				sl.ignored = light.m_is_ignored;
				sl.filler = light.m_is_filler;

				if (light.m_is_attached_light)
				{
					sl.hash = light.m_hash & 0xFFFFFFFFull;
					sl.attached_index = static_cast<int>(light.m_hash >> 32) - 1;
					sl.pos = light.m_def.mPosition;
				}
				else
				{
					sl.hash = light.m_hash;
					sl.instanced = light.m_def.mInstanceId != 0u;

					// one entry per hash, the other instances can not be told apart in the map settings
					if (!seen.insert(sl.hash).second) {
						continue;
					}

					const auto it = overrides.find(sl.hash);
					sl.pos = remix_lights::get_light_position(light.m_def, it != overrides.end() ? &it->second : nullptr);
				}

				sl.distance = cam_org.DistTo(sl.pos);
				if (sl.distance > g.view_distance && sl.hash != g.selected.hash) {
					continue;
				}

				sl.on_screen = project(sl.pos, sl.screen, &sl.in_front);
				g.lights.emplace_back(sl);
			}

			if (const auto light = find_scene_light({ g.selected.hash, MAIN_LIGHT }); light)
			{
				g.selected_def = light->def;
				g.selected_def_valid = true;
				g.selected_instanced = light->instanced;
			}
		}

		void select(const selection_s& sel)
		{
			if (!(g.selected == sel))
			{
				g.selected = sel;
				g.place_properties = true;
				g.selected_def_valid = false;
				g.selected_instanced = false;
			}
		}

		ImU32 state_color(const scene_light_s& light)
		{
			if (light.ignored) {
				return COL_IGNORED;
			}

			if (const auto owner = owner_file(light.hash); !owner.empty()) {
				return owner == g.active_file ? COL_OVERRIDE : COL_OVERRIDE_OTHER_FILE;
			}

			return COL_PLAIN;
		}

		void build_light_matrix(const Vector& pos, Vector dir, float* m)
		{
			if (dir.LengthSqr() < 1.e-6f) {
				dir = Vector(0.0f, 0.0f, -1.0f);
			}

			dir.Normalize();
			const Vector up_ref = fabsf(dir.z) > 0.99f ? Vector(1.0f, 0.0f, 0.0f) : Vector(0.0f, 0.0f, 1.0f);
			Vector right = up_ref.Cross(dir);
			right.Normalize();
			const Vector up = dir.Cross(right);

			m[0] = right.x; m[1] = right.y; m[2]  = right.z; m[3]  = 0.0f;
			m[4] = up.x;    m[5] = up.y;    m[6]  = up.z;    m[7]  = 0.0f;
			m[8] = dir.x;   m[9] = dir.y;   m[10] = dir.z;   m[11] = 0.0f;
			m[12] = pos.x;  m[13] = pos.y;  m[14] = pos.z;   m[15] = 1.0f;
		}

		/// the override that the selection edits, nullptr if the active file does not own one
		override_s* editable_override(const selection_s& sel)
		{
			if (!sel.hash || g.active_file.empty() || owner_file(sel.hash) != g.active_file) {
				return nullptr;
			}

			const auto ov = find_override(g.active_file, sel.hash);
			if (sel.attached_index == MAIN_LIGHT) {
				return ov;
			}

			return sel.attached_index < static_cast<int>(ov->attached_lights.size()) ? &ov->attached_lights[sel.attached_index] : nullptr;
		}

		bool is_spot(const override_s& ov, const game::CLightSource& def) {
			return ov._use_light_type ? ov.light_type : def.mType == game::LT_SPOT;
		}

		void draw_light_shape(const Vector& pos, Vector dir, const bool spot, const float cos_outer, const float sphere_radius, const ImU32 color)
		{
			const auto draw = ImGui::GetBackgroundDrawList();
			constexpr int segments = 32;
			ImVec2 ring[segments];

			if (spot)
			{
				float m[16];
				build_light_matrix(pos, dir, m);
				const Vector right(m[0], m[1], m[2]), up(m[4], m[5], m[6]), fwd(m[8], m[9], m[10]);

				const float angle = acosf(std::clamp(cos_outer, -1.0f, 1.0f));
				constexpr float length = 2.5f;
				const float ring_radius = length * tanf(std::min(angle, 1.45f));

				bool all_in_front = true;
				for (auto i = 0; i < segments; i++)
				{
					const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
					bool front = false;
					project(pos + fwd * length + (right * cosf(a) + up * sinf(a)) * ring_radius, ring[i], &front);
					all_in_front &= front;
				}

				ImVec2 apex;
				bool apex_front = false;
				project(pos, apex, &apex_front);

				if (all_in_front && apex_front)
				{
					draw->AddPolyline(ring, segments, color, ImDrawFlags_Closed, 1.5f);
					for (auto i = 0; i < segments; i += segments / 4) {
						draw->AddLine(apex, ring[i], color, 1.5f);
					}
				}

				return;
			}

			// sphere light: the size of the remix light as a ring facing the camera
			const auto& cam = game::pViewports->sceneviewport->cameraInv;
			const Vector right(cam.m[0][0], cam.m[0][1], cam.m[0][2]), up(cam.m[1][0], cam.m[1][1], cam.m[1][2]);

			bool all_in_front = true;
			for (auto i = 0; i < segments; i++)
			{
				const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
				bool front = false;
				project(pos + (right * cosf(a) + up * sinf(a)) * sphere_radius, ring[i], &front);
				all_in_front &= front;
			}

			if (all_in_front) {
				draw->AddPolyline(ring, segments, color, ImDrawFlags_Closed, 1.5f);
			}
		}

		void setup_gizmo_style()
		{
			static bool done = false;
			if (done) {
				return;
			}

			done = true;
			auto& style = ImGuizmo::GetStyle();
			style.TranslationLineThickness = 5.0f;
			style.TranslationLineArrowSize = 8.0f;
			style.RotationLineThickness = 4.0f;
			style.RotationOuterLineThickness = 3.0f;
			style.HatchedAxisLineThickness = 0.0f;
			style.CenterCircleSize = 5.0f;

			style.Colors[ImGuizmo::DIRECTION_X] = ImVec4(1.00f, 0.47f, 0.47f, 1.0f);
			style.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.52f, 0.92f, 0.47f, 1.0f);
			style.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.47f, 0.67f, 1.00f, 1.0f);
			style.Colors[ImGuizmo::PLANE_X] = ImVec4(1.00f, 0.47f, 0.47f, 0.75f);
			style.Colors[ImGuizmo::PLANE_Y] = ImVec4(0.52f, 0.92f, 0.47f, 0.75f);
			style.Colors[ImGuizmo::PLANE_Z] = ImVec4(0.47f, 0.67f, 1.00f, 0.75f);
			style.Colors[ImGuizmo::SELECTION] = ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
			style.Colors[ImGuizmo::INACTIVE] = ImVec4(0.65f, 0.65f, 0.65f, 0.85f);
			style.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
			style.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(1.00f, 0.85f, 0.30f, 0.35f);

			// an axis that points away from the camera stays where it is instead of flipping to the other side
			ImGuizmo::AllowAxisFlip(false);
		}

		void draw_gizmo()
		{
			const auto ov = editable_override(g.selected);
			if (!ov || !has_viewport()) {
				return;
			}

			game::CLightSource def = {};
			if (!get_reference_def(g.selected.hash, def)) {
				return;
			}

			const bool spot = is_spot(*ov, def);
			const Vector pos = ov->_use_pos ? ov->pos : def.mPosition;
			const Vector dir = ov->_use_dir ? ov->dir : def.mDirection;
			const float cos_outer = ov->_use_outer_cone_angle || def.mType != game::LT_SPOT ? ov->outer_cone_angle : def.mOuterConeAngle;

			const float sphere_radius = remix_lights::get_light_radius(def, ov) * comp_settings::get()->translate_game_light_radius_scalar.get_as<float>() * 0.01f;
			draw_light_shape(pos, dir, spot, cos_outer, sphere_radius, IM_COL32(255, 210, 60, 200));

			const bool can_translate = !(g.selected_instanced && g.selected.attached_index == MAIN_LIGHT);
			const bool rotate = g.rotate_mode && spot;
			if (!rotate && !can_translate) {
				return;
			}

			if (!ImGuizmo::IsUsing()) {
				build_light_matrix(pos, dir, g.gizmo_matrix);
			}

			const auto& io = ImGui::GetIO();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

			if (ImGuizmo::Manipulate(view_matrix(), projection_matrix(), rotate ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE,
				g.local_space ? ImGuizmo::LOCAL : ImGuizmo::WORLD, g.gizmo_matrix))
			{
				if (rotate)
				{
					Vector new_dir(g.gizmo_matrix[8], g.gizmo_matrix[9], g.gizmo_matrix[10]);
					new_dir.Normalize();
					ov->dir = new_dir;
					ov->_use_dir = true;
				}
				else
				{
					ov->pos = Vector(g.gizmo_matrix[12], g.gizmo_matrix[13], g.gizmo_matrix[14]);
					ov->_use_pos = true;
				}

				commit(g.active_file);
			}
		}

		/// category of the override that is in effect, empty if it has none
		std::string category_name(const std::uint64_t hash, const std::string& owner)
		{
			int category = NO_CATEGORY;
			if (owner.empty() || !find_override(owner, hash, &category) || category == NO_CATEGORY) {
				return {};
			}

			return settings().light_overrides_toml_info[owner].categories[category].category_name;
		}

		void draw_text_shadowed(ImDrawList* draw, const ImVec2& pos, const ImU32 color, const char* text, const float scale = 1.0f)
		{
			const float size = ImGui::GetFontSize() * scale;
			draw->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 230), text);
			draw->AddText(ImGui::GetFont(), size, pos, color, text);
		}

		void draw_world_overlay()
		{
			const auto& io = ImGui::GetIO();
			const auto draw = ImGui::GetBackgroundDrawList();
			const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
			constexpr float edge_inset = 34.0f;

			struct marker_s
			{
				const scene_light_s* light;
				ImVec2 pos;
				ImVec2 dir; // direction of the edge arrow, unused for lights on screen
				std::string owner;
			};

			static std::vector<marker_s> markers;
			markers.clear();

			// lights on screen get a circle, the selected light and the light hovered in the list an arrow at the screen edge when they are not
			for (const auto& light : g.lights)
			{
				marker_s m = { &light, light.screen, ImVec2(0.0f, 0.0f), owner_file(light.hash) };

				if (!light.on_screen)
				{
					const selection_s id = { light.hash, light.attached_index };
					if (!(id == g.selected) && !(id == g.hovered_drawn)) {
						continue;
					}

					ImVec2 d(light.screen.x - center.x, light.screen.y - center.y);
					if (!light.in_front) {
						d = ImVec2(-d.x, -d.y); // the projection mirrors points behind the camera
					}

					const float len = sqrtf(d.x * d.x + d.y * d.y);
					d = len > 0.001f ? ImVec2(d.x / len, d.y / len) : ImVec2(0.0f, 1.0f);

					const float tx = fabsf(d.x) > 0.0001f ? (center.x - edge_inset) / fabsf(d.x) : FLT_MAX;
					const float ty = fabsf(d.y) > 0.0001f ? (center.y - edge_inset) / fabsf(d.y) : FLT_MAX;
					const float t = std::min(tx, ty);

					m.pos = ImVec2(center.x + d.x * t, center.y + d.y * t);
					m.dir = d;
				}

				markers.emplace_back(std::move(m));
			}

			// picking
			const bool can_pick = !io.WantCaptureMouse && !io.MouseDown[ImGuiMouseButton_Right] && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing();
			const marker_s* nearest = nullptr;

			if (can_pick && g.show_markers)
			{
				float nearest_dist_sqr = 16.0f * 16.0f;
				for (const auto& m : markers)
				{
					const float dx = m.pos.x - io.MousePos.x, dy = m.pos.y - io.MousePos.y;
					if (const float d = dx * dx + dy * dy; d < nearest_dist_sqr)
					{
						nearest_dist_sqr = d;
						nearest = &m;
					}
				}
			}

			if (nearest)
			{
				g.hovered = { nearest->light->hash, nearest->light->attached_index };
				g.hovered_drawn = g.hovered;
			}

			if (g.show_markers)
			{
				for (const auto& m : markers)
				{
					const auto& light = *m.light;
					const selection_s id = { light.hash, light.attached_index };
					const bool selected = id == g.selected;
					const bool hovered = id == g.hovered_drawn;
					const bool attached = light.attached_index != MAIN_LIGHT;
					const ImU32 ring_color = selected ? COL_SELECTED : hovered ? COL_HOVERED : state_color(light);

					const ImU32 fill = light.ignored ? IM_COL32(60, 60, 60, 255)
						: ImGui::ColorConvertFloat4ToU32(ImVec4(std::min(light.def.mColor.x, 1.0f), std::min(light.def.mColor.y, 1.0f), std::min(light.def.mColor.z, 1.0f), 1.0f));

					float radius = (attached ? 5.0f : 7.0f) + (selected || hovered ? 2.0f : 0.0f);

					if (light.on_screen)
					{
						if (attached)
						{
							if (const auto base = find_scene_light({ light.hash, MAIN_LIGHT }); base && base->on_screen) {
								draw->AddLine(base->screen, light.screen, IM_COL32(255, 255, 255, 70), 1.0f);
							}
						}

						draw->AddCircleFilled(m.pos, radius + 1.5f, IM_COL32(0, 0, 0, 160));
						draw->AddCircleFilled(m.pos, radius, fill);
						draw->AddCircle(m.pos, radius, ring_color, 0, selected || hovered ? 2.5f : 1.5f);

						// what the map settings do with the light
						if (!attached)
						{
							const ImVec2 badge_pos(m.pos.x + radius + 3.0f, m.pos.y - radius - ImGui::GetFontSize() * 0.62f - 3.0f);
							if (light.ignored) {
								draw_text_shadowed(draw, badge_pos, COL_IGNORED, ICON_FA_EYE_SLASH, 0.62f);
							}
							else if (!m.owner.empty()) {
								draw_text_shadowed(draw, badge_pos, m.owner == g.active_file ? COL_OVERRIDE : COL_OVERRIDE_OTHER_FILE, ICON_FA_SLIDERS_H, 0.62f);
							}
						}
					}
					else
					{
						// arrow at the screen edge pointing towards the light
						radius = selected || hovered ? 12.0f : 9.0f;
						const ImVec2 side(-m.dir.y, m.dir.x);
						const ImVec2 tip(m.pos.x + m.dir.x * radius, m.pos.y + m.dir.y * radius);
						const ImVec2 left(m.pos.x - m.dir.x * radius * 0.6f + side.x * radius * 0.75f, m.pos.y - m.dir.y * radius * 0.6f + side.y * radius * 0.75f);
						const ImVec2 right(m.pos.x - m.dir.x * radius * 0.6f - side.x * radius * 0.75f, m.pos.y - m.dir.y * radius * 0.6f - side.y * radius * 0.75f);

						draw->AddTriangleFilled(tip, left, right, fill);
						draw->AddTriangle(tip, left, right, IM_COL32(0, 0, 0, 200), 3.5f);
						draw->AddTriangle(tip, left, right, ring_color, 1.5f);
					}

					if (g.show_labels || selected || hovered)
					{
						// name, then what else is known about a hovered or selected light
						std::string lines[4];
						auto count = 0;

						lines[count++] = attached ? std::format("{} / attached {}", light_name(light.hash), light.attached_index + 1) : light_name(light.hash);

						if (selected || hovered)
						{
							if (const auto category = category_name(light.hash, m.owner); !category.empty()) {
								lines[count++] = "category: " + category;
							}

							if (!m.owner.empty() && m.owner != g.active_file) {
								lines[count++] = "override in " + m.owner;
							}

							if (const auto source = game_lights::get_light_source_name(light.hash); !source.empty() && source != lines[0]) {
								lines[count++] = source;
							}
						}

						float width = 0.0f;
						for (auto i = 0; i < count; i++) {
							width = std::max(width, ImGui::CalcTextSize(lines[i].c_str()).x);
						}

						// lights at the right or bottom edge keep their label on screen
						ImVec2 text_pos(m.pos.x + radius + 8.0f, m.pos.y - ImGui::GetFontSize() * 0.5f);
						if (text_pos.x + width > io.DisplaySize.x - 4.0f) {
							text_pos.x = m.pos.x - radius - 8.0f - width;
						}

						text_pos.y = std::min(text_pos.y, io.DisplaySize.y - static_cast<float>(count) * ImGui::GetFontSize() - 4.0f);

						for (auto i = 0; i < count; i++)
						{
							draw_text_shadowed(draw, ImVec2(text_pos.x, text_pos.y + static_cast<float>(i) * ImGui::GetFontSize()),
								i ? IM_COL32(200, 200, 200, 255) : selected ? COL_SELECTED : hovered ? COL_HOVERED : IM_COL32(235, 235, 235, 255), lines[i].c_str());
						}
					}
				}
			}

			if (nearest)
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					select({ nearest->light->hash, nearest->light->attached_index });
				}

				// a held right button can hand the input to the game, so a short click opens the menu on release
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && io.MouseDownDurationPrev[ImGuiMouseButton_Right] < 0.3f
					&& io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 36.0f)
				{
					g.context = { nearest->light->hash, nearest->light->attached_index };
					g.open_context_menu = true;
				}
			}
		}

#pragma endregion

#pragma region WINDOWS

		bool tool_button(const char* label, const char* tooltip, const bool active = false, const ImVec4& active_color = COL_BUTTON_ACTIVE)
		{
			ImGui::Style_ColorButtonPush(active ? active_color : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
			const bool pressed = ImGui::Button(label, ImVec2(0.0f, ImGui::GetFrameHeight() + 6.0f));
			ImGui::Style_ColorButtonPop();
			TT(tooltip);
			ImGui::SameLine(0.0f, 4.0f);
			return pressed;
		}

		void tool_separator()
		{
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine(0.0f, 6.0f);
		}

		void category_menu_items(const std::string& file, const std::uint64_t hash)
		{
			int current = NO_CATEGORY;
			find_override(file, hash, &current);
			auto& categories = settings().light_overrides_toml_info[file].categories;

			if (ImGui::MenuItem("Uncategorized", nullptr, current == NO_CATEGORY)) {
				move_to_category(file, hash, NO_CATEGORY);
			}

			for (auto i = 0; i < static_cast<int>(categories.size()); i++)
			{
				ImGui::PushID(i);
				if (ImGui::MenuItem(categories[i].category_name.c_str(), nullptr, current == i)) {
					move_to_category(file, hash, i);
				}
				ImGui::PopID();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS "  New category ..."))
			{
				g.context = { hash, MAIN_LIGHT };
				g.category_popup_index = NO_CATEGORY;
				g.new_category_for_context = true;
				g.open_category_popup = true;
				g.name_buffer.clear();
				g.comment_buffer.clear();
			}
		}

		void draw_context_menu()
		{
			if (g.open_context_menu)
			{
				ImGui::OpenPopup("##le_context");
				g.open_context_menu = false;
			}

			if (!ImGui::BeginPopup("##le_context")) {
				return;
			}

			shared::imgui::draw_window_blur();

			const auto hash = g.context.hash;
			const auto owner = owner_file(hash);
			const bool in_active = !g.active_file.empty() && find_override(g.active_file, hash);
			const bool editable = in_active && owner == g.active_file;
			const auto ignored_in = ignoring_file(hash);

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::TextUnformatted(light_name(hash).c_str());
			ImGui::PopFont();
			ImGui::TextDisabled("0x%llX", static_cast<unsigned long long>(hash));
			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER "  Select")) {
				select(g.context);
			}

			ImGui::BeginDisabled(g.active_file.empty());
			if (!in_active)
			{
				if (ImGui::MenuItem(ICON_FA_PLUS "  Create override"))
				{
					create_override(hash);
					select({ hash, MAIN_LIGHT });
				} TT("Starts from the game's values in the active file");

				if (!owner.empty() && ImGui::MenuItem(std::format(ICON_FA_COPY "  Create override from {}", owner).c_str()))
				{
					const override_s copy = *find_override(owner, hash);
					create_override(hash, &copy);
					select({ hash, MAIN_LIGHT });
				}
			}
			else
			{
				if (ImGui::MenuItem(ICON_FA_TRASH "  Remove override"))
				{
					remove_override(g.active_file, hash);
					if (g.selected.hash == hash) {
						g.selected.attached_index = MAIN_LIGHT;
					}
				}
			}
			ImGui::EndDisabled();

			if (editable)
			{
				if (ImGui::BeginMenu(ICON_FA_FOLDER "  Move to category"))
				{
					shared::imgui::draw_window_blur();
					category_menu_items(g.active_file, hash);
					ImGui::EndMenu();
				}

				if (ImGui::MenuItem(ICON_FA_PLUS "  Attach new light")) {
					attach_light(hash);
				} TT("Adds another light that exists as long as this light does");
			}

			ImGui::Separator();

			if (ignored_in.empty())
			{
				ImGui::BeginDisabled(g.active_file.empty() || in_active);
				if (ImGui::MenuItem(ICON_FA_EYE_SLASH "  Ignore light")) {
					set_ignored(hash, true);
				}
				ImGui::EndDisabled();

				if (in_active) {
					ImGui::SetItemTooltip("%s", "An override in the same file takes precedence. Remove the override first.");
				}
			}
			else if (ImGui::MenuItem(std::format(ICON_FA_EYE "  Stop ignoring ({})", ignored_in).c_str())) {
				set_ignored(hash, false);
			}

			if (const auto light = find_scene_light({ hash, MAIN_LIGHT });
				light && light->filler && comp_settings::get()->translate_game_lights_ignore_filler_lights._bool())
			{
				const bool allowed = settings().allow_lights.contains(hash);
				if (ImGui::MenuItem(ICON_FA_CHECK "  Allow filler light", nullptr, allowed)) {
					set_allowed(hash, !allowed);
				}
			}

			ImGui::Separator();

			if (const auto source = !owner.empty() ? find_override(owner, hash) : nullptr; source)
			{
				if (ImGui::MenuItem(ICON_FA_COPY "  Copy settings"))
				{
					g.clipboard = *source;
					g.clipboard->attached_lights.clear();
				}
			}

			if (editable && g.clipboard.has_value())
			{
				if (ImGui::MenuItem(ICON_FA_PASTE "  Paste settings"))
				{
					const auto target = find_override(g.active_file, hash);
					const auto keep_attached = target->attached_lights;
					const auto keep_comment = target->comment;
					const auto keep_order = target->_order;
					const auto keep_pos = target->pos;
					const auto keep_use_pos = target->_use_pos;

					*target = g.clipboard.value();
					target->attached_lights = keep_attached;
					target->comment = keep_comment;
					target->_order = keep_order;
					target->pos = keep_pos;
					target->_use_pos = keep_use_pos;
					commit(g.active_file);
				} TT("Everything but the position, the comment and attached lights");
			}

			if (ImGui::MenuItem(ICON_FA_HASHTAG "  Copy hash")) {
				ImGui::SetClipboardText(std::format("0x{:X}", hash).c_str());
			}

			ImGui::EndPopup();
		}

		void draw_category_popup()
		{
			if (g.open_category_popup)
			{
				ImGui::OpenPopup("Light category");
				g.open_category_popup = false;
			}

			if (!ImGui::BeginPopupModal("Light category", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}

			shared::imgui::draw_background_blur();
			auto& info = settings().light_overrides_toml_info[g.active_file];
			const bool rename = g.category_popup_index != NO_CATEGORY && g.category_popup_index < static_cast<int>(info.categories.size());

			ImGui::SetNextItemWidth(320.0f);
			if (ImGui::IsWindowAppearing()) {
				ImGui::SetKeyboardFocusHere();
			}

			const bool enter = ImGui::InputTextWithHint("##name", "Category name", &g.name_buffer, ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SetNextItemWidth(320.0f);
			ImGui::InputTextWithHint("##comment", "Comment above the category (optional)", &g.comment_buffer);
			ImGui::Spacing(0, 4);

			ImGui::BeginDisabled(g.name_buffer.empty());
			if (ImGui::Button(rename ? "Apply" : "Create", ImVec2(156.0f, 0.0f)) || (enter && !g.name_buffer.empty()))
			{
				if (rename)
				{
					info.categories[g.category_popup_index].category_name = g.name_buffer;
					info.categories[g.category_popup_index].category_comment = g.comment_buffer;
					commit(g.active_file);
				}
				else
				{
					const int index = add_category(g.active_file, g.name_buffer);
					info.categories[index].category_comment = g.comment_buffer;

					if (g.new_category_for_context) {
						move_to_category(g.active_file, g.context.hash, index);
					}
				}

				g.new_category_for_context = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, 8.0f);
			if (ImGui::Button("Cancel", ImVec2(156.0f, 0.0f)))
			{
				g.new_category_for_context = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		void draw_new_file_popup()
		{
			if (g.open_new_file_popup)
			{
				ImGui::OpenPopup("New light tweak file");
				g.open_new_file_popup = false;
				g.name_buffer.clear();
			}

			if (!ImGui::BeginPopupModal("New light tweak file", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}

			shared::imgui::draw_background_blur();
			ImGui::TextUnformatted("Files are applied in alphabetical order, earlier names have higher priority.");
			ImGui::Spacing(0, 4);

			ImGui::SetNextItemWidth(320.0f);
			if (ImGui::IsWindowAppearing()) {
				ImGui::SetKeyboardFocusHere();
			}

			const bool enter = ImGui::InputTextWithHint("##file", "file name", &g.name_buffer, ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SameLine();
			ImGui::TextDisabled(".toml");

			std::string file = g.name_buffer;
			if (file.ends_with(".toml")) {
				file.resize(file.size() - 5u);
			}

			const bool valid_chars = !file.empty() && std::ranges::all_of(file, [](const char c)
				{
					return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
				});

			file += ".toml";
			const bool exists = file_exists(file);

			if (!g.name_buffer.empty() && !valid_chars) {
				ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Use letters, digits, '_', '-' and '.' only");
			}
			else if (exists) {
				ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "A file with that name exists");
			}

			ImGui::Spacing(0, 4);
			ImGui::BeginDisabled(!valid_chars || exists);
			if (ImGui::Button("Create", ImVec2(156.0f, 0.0f)) || (enter && valid_chars && !exists))
			{
				settings().light_overrides_toml_info[file];
				settings().lights_toml_info[file];
				g.active_file = file;
				g.dirty_files.insert(file);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, 8.0f);
			if (ImGui::Button("Cancel", ImVec2(156.0f, 0.0f))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		void close_editor()
		{
			g.open = false;
			free_camera::set_active(false);
			shared::globals::imgui_editor_open = false;
			imgui::get()->m_dbg_visualize_api_light_hashes = false;
		}

		void draw_close_popup()
		{
			if (g.open_close_popup)
			{
				ImGui::OpenPopup("Unsaved light tweaks");
				g.open_close_popup = false;
			}

			if (!ImGui::BeginPopupModal("Unsaved light tweaks", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}

			shared::imgui::draw_background_blur();
			ImGui::TextUnformatted("These files have changes that are not saved:");
			for (const auto& file : g.dirty_files) {
				ImGui::BulletText("%s", file.c_str());
			}

			ImGui::Spacing(0, 6);
			if (ImGui::Button("Save all and close", ImVec2(160.0f, 0.0f)))
			{
				save_all_files();
				close_editor();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine(0.0f, 8.0f);
			if (ImGui::Button("Close without saving", ImVec2(160.0f, 0.0f)))
			{
				close_editor();
				ImGui::CloseCurrentPopup();
			} TT("The changes stay active until the map settings are reloaded and can still be saved when the editor is opened again.");

			ImGui::SameLine(0.0f, 8.0f);
			if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		void draw_file_menu()
		{
			if (!ImGui::BeginPopup("##le_file_menu")) {
				return;
			}

			shared::imgui::draw_window_blur();
			ImGui::TextDisabled("Active file (highest priority first)");

			for (const auto& file : sorted_files())
			{
				const auto label = g.dirty_files.contains(file) ? file + "  *" : file;
				if (ImGui::MenuItem(label.c_str(), nullptr, file == g.active_file)) {
					g.active_file = file;
				}
			}

			if (ImGui::MenuItem(ICON_FA_FILE "  New file ...")) {
				g.open_new_file_popup = true;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_SAVE "  Save active file", "Ctrl+S", false, !g.active_file.empty())) {
				save_active_file();
			}

			if (ImGui::MenuItem(ICON_FA_SAVE "  Save all changed files", nullptr, false, !g.dirty_files.empty())) {
				save_all_files();
			}

			if (ImGui::BeginMenu(ICON_FA_UNDO "  Discard changes", !g.dirty_files.empty()))
			{
				shared::imgui::draw_window_blur();
				const auto dirty = g.dirty_files;

				for (const auto& file : dirty)
				{
					if (ImGui::MenuItem(file.c_str())) {
						discard_file(file);
					}
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy active file", nullptr, false, !g.active_file.empty())) {
				copy_to_clipboard(build_file(g.active_file), g.active_file.c_str());
			}

			if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy overrides of all files")) {
				copy_to_clipboard(shared::common::toml_ext::build_lightweak_mixed_array(settings().light_overrides_toml_info), "all overrides");
			}

			if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy ignore / allow lists of all files"))
			{
				copy_to_clipboard(shared::common::toml_ext::build_ignore_lights_array_from_toml_info(settings().lights_toml_info) + "\n\n"
					+ shared::common::toml_ext::build_allow_lights_array_from_toml_info(settings().lights_toml_info), "ignore / allow lists");
			}

			if (!g.active_file.empty())
			{
				if (const auto it = settings().light_overrides_toml_info.find(g.active_file);
					it != settings().light_overrides_toml_info.end() && !it->second.categories.empty() && ImGui::BeginMenu(ICON_FA_CLIPBOARD "  Copy category"))
				{
					shared::imgui::draw_window_blur();
					for (auto i = 0; i < static_cast<int>(it->second.categories.size()); i++)
					{
						ImGui::PushID(i);
						if (ImGui::MenuItem(it->second.categories[i].category_name.c_str())) {
							copy_to_clipboard(build_category(it->second.categories[i]), "category");
						}
						ImGui::PopID();
					}
					ImGui::EndMenu();
				}
			}

			ImGui::EndPopup();
		}

		void draw_toolbar()
		{
			const auto im = imgui::get();
			const auto& io = ImGui::GetIO();

			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - SCREEN_PADDING, SCREEN_PADDING), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

			if (ImGui::Begin("##light_editor_toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				shared::imgui::draw_window_blur();

				{
					const ImVec2 grip_size = ImGui::CalcTextSize(ICON_FA_GRIP_VERTICAL);
					const float button_height = ImGui::GetFrameHeight() + 6.0f;
					const ImVec2 grip_pos = ImGui::GetCursorScreenPos();

					ImGui::Dummy(ImVec2(grip_size.x, button_height));
					TT("Drag to move the toolbar");
					ImGui::GetWindowDrawList()->AddText(ImVec2(grip_pos.x, grip_pos.y + (button_height - grip_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_GRIP_VERTICAL);
					ImGui::SameLine(0.0f, 8.0f);
				}

				// game
				ImGui::BeginDisabled(!game::is_in_game || !game::ms_bUserPause);
				if (tool_button(im->m_pause_game ? ICON_FA_PLAY "##pause" : ICON_FA_PAUSE "##pause", "Pause the game like the pause menu does, cutscenes included. The scene and its lights keep rendering.", im->m_pause_game))
				{
					im->m_pause_game = !im->m_pause_game;
					if (!im->m_pause_game && !(game::CMenuManager__m_MenuActive && *game::CMenuManager__m_MenuActive)) {
						*game::ms_bUserPause = 0u;
					}
				}

				ImGui::BeginDisabled(!im->m_pause_game);
				tool_button(ICON_FA_STEP_FORWARD "##step", "Click to advance a single frame, hold to let the game run until released");
				ImGui::SameLine(0.0f, 0.0f);

				// the button above is the last item (tool_button ends with SameLine)
				const bool step_held = ImGui::IsItemActive();
				if (step_held || g.step_held) {
					im->m_pause_step_held = step_held;
				}
				g.step_held = step_held;

				ImGui::EndDisabled();
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 4.0f);
				tool_separator();

				// camera / input
				const bool freecam = free_camera::is_active();
				if (tool_button(ICON_FA_VIDEO "##freecam",
					"Free camera, the player stays put\n"
					"Hold the right mouse button to look around and move:\n"
					"WASD: Forward & Strafing\n"
					"Q/E:  Down & Up\n"
					"Shift/Space: Speedup & Slowdown\n"
					"Mouse wheel: Speed", freecam))
				{
					free_camera::set_active(!freecam);
				}

				ImGui::BeginDisabled(freecam);
				if (tool_button(ICON_FA_GAMEPAD "##rmb_input", "Hold the right mouse button over the world to hand mouse and keyboard to the game", g.rmb_game_input && !freecam)) {
					g.rmb_game_input = !g.rmb_game_input;
				}
				ImGui::EndDisabled();

				if (freecam && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
				{
					ImGui::SetTooltip("%s",
						"Hold the right mouse button over the world to hand mouse and keyboard to the game\n"
						"Not available while the free camera is on: it uses the right mouse button itself. Turn the free camera off first.");
				}

				ImGui::SameLine(0.0f, 4.0f);

				tool_separator();

				// gizmo
				if (tool_button(ICON_FA_ARROWS_ALT "##translate", "Move the selected light (T)", !g.rotate_mode)) {
					g.rotate_mode = false;
				}

				if (tool_button(ICON_FA_SYNC_ALT "##rotate", "Rotate the selected spotlight (R)", g.rotate_mode)) {
					g.rotate_mode = true;
				}

				if (tool_button(g.local_space ? ICON_FA_CUBE "##space" : ICON_FA_GLOBE "##space", g.local_space ? "Gizmo axes: light. Click for world axes" : "Gizmo axes: world. Click for the light's own axes")) {
					g.local_space = !g.local_space;
				}

				tool_separator();

				// view
				if (tool_button(ICON_FA_LIST "##list", "Light list", g.show_list)) {
					g.show_list = !g.show_list;
				}

				if (tool_button(ICON_FA_LIGHTBULB "##markers", "Light markers in the world", g.show_markers)) {
					g.show_markers = !g.show_markers;
				}

				if (tool_button(ICON_FA_TAG "##labels", "Names on all markers (selected and hovered lights always show theirs)", g.show_labels)) {
					g.show_labels = !g.show_labels;
				}

				tool_separator();

				// history
				ImGui::BeginDisabled(g.undo.empty());
				if (tool_button(ICON_FA_UNDO "##undo", "Undo (Ctrl+Z)")) {
					restore_snapshot(g.undo, g.redo);
				}
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 4.0f);

				ImGui::BeginDisabled(g.redo.empty());
				if (tool_button(ICON_FA_REDO "##redo", "Redo (Ctrl+Y)")) {
					restore_snapshot(g.redo, g.undo);
				}
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 4.0f);

				tool_separator();

				// files
				const bool active_dirty = g.dirty_files.contains(g.active_file);
				ImGui::BeginDisabled(g.active_file.empty());
				if (tool_button(ICON_FA_SAVE "##save", active_dirty ? "Save the active file (Ctrl+S)" : "The active file has no unsaved changes", active_dirty, COL_BUTTON_UNSAVED)) {
					save_active_file();
				}
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 4.0f);

				// fixed width, a name that does not fit slides while the button is hovered
				{
					constexpr float file_button_width = 180.0f;
					const auto file_label = std::format("{}{}", g.active_file.empty() ? "No file" : g.active_file, active_dirty ? "  *" : "");
					const ImVec2 button_pos = ImGui::GetCursorScreenPos();
					const float button_height = ImGui::GetFrameHeight() + 6.0f;

					ImGui::Style_ColorButtonPush(ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
					if (ImGui::Button("##file", ImVec2(file_button_width, button_height))) {
						ImGui::OpenPopup("##le_file_menu");
					}
					ImGui::Style_ColorButtonPop();

					const bool hovered = ImGui::IsItemHovered();
					ImGui::SetItemTooltipBlur("%s\nActive file, new file, save all, discard and clipboard export. '*' marks unsaved changes.", file_label.c_str());

					const float pad = ImGui::GetStyle().FramePadding.x + 2.0f;
					const ImVec2 caret_size = ImGui::CalcTextSize(ICON_FA_CARET_DOWN);
					const ImVec2 text_size = ImGui::CalcTextSize(file_label.c_str());
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
					draw->AddText(ImVec2(text_min_x - slide, text_y), ImGui::GetColorU32(ImGuiCol_Text), file_label.c_str());
					draw->PopClipRect();
					draw->AddText(ImVec2(caret_x, button_pos.y + (button_height - caret_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), ICON_FA_CARET_DOWN);

					ImGui::SameLine(0.0f, 4.0f);
				}

				tool_separator();

				if (tool_button(ICON_FA_TIMES "##close", "Close the light editor")) {
					set_open(false);
				}

				// the second row is as wide as the buttons
				const float row_width = ImGui::GetItemRectMax().x - (ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x);
				ImGui::NewLine();
				ImGui::Spacing(0, 3);

				// camera speed and view distance share the width of the button row
				{
					const float spacing = 8.0f;
					const float width = std::max((row_width - spacing) * 0.5f, 120.0f);

					// half as high as a regular slider
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
					ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 8.0f);
					ImGui::SetWindowFontScale(0.82f);

					float speed = free_camera::get_speed();
					ImGui::SetNextItemWidth(width);
					if (ImGui::SliderFloat("##camera_speed", &speed, 0.25f, 400.0f, "Camera speed  %.1f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
						free_camera::set_speed(speed);
					} TT("Speed of the free camera, also changed with the mouse wheel while the right mouse button is held");

					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(width);
					ImGui::SliderFloat("##view_distance", &g.view_distance, 5.0f, 1000.0f, "Light distance  %.0f m", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

					ImGui::SetWindowFontScale(1.0f);
					ImGui::PopStyleVar(2);
					TT("Lights farther away from the camera get no marker and no entry in the list");
				}

				// messages go below the toolbar, so it keeps its size
				if (ImGui::GetTime() < g.status_until)
				{
					const ImVec2 pos(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowPos().y + ImGui::GetWindowHeight() + 4.0f);
					draw_text_shadowed(ImGui::GetForegroundDrawList(), pos, IM_COL32(235, 235, 235, 255), g.status.c_str());
				}

				draw_file_menu();
				draw_context_menu();
				draw_category_popup();
				draw_new_file_popup();
				draw_close_popup();
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		struct list_row_s
		{
			selection_s id;
			const scene_light_s* light = nullptr;
			int group = 0;
			int category = NO_CATEGORY;
			float distance = 0.0f;
			std::string name;
			std::string owner;
		};

		enum list_group : int
		{
			GROUP_CATEGORY = 0,
			GROUP_UNCATEGORIZED,
			GROUP_OTHER_FILE,
			GROUP_NO_OVERRIDE,
			GROUP_IGNORED,
		};

		void draw_list_row(const list_row_s& row)
		{
			const bool selected = row.id == g.selected;
			const bool present = row.light != nullptr;

			ImGui::PushID(static_cast<int>(row.id.hash));
			ImGui::PushID(row.id.attached_index);

			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const float height = ImGui::GetTextLineHeight();
			ImGui::Dummy(ImVec2(height, height));
			ImGui::SameLine(0.0f, 6.0f);

			const ImU32 fill = !present ? IM_COL32(90, 90, 90, 255) : row.light->ignored ? IM_COL32(60, 60, 60, 255)
				: ImGui::ColorConvertFloat4ToU32(ImVec4(std::min(row.light->def.mColor.x, 1.0f), std::min(row.light->def.mColor.y, 1.0f), std::min(row.light->def.mColor.z, 1.0f), 1.0f));
			ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + height * 0.5f, cursor.y + height * 0.5f), height * 0.32f, fill);
			if (present) {
				ImGui::GetWindowDrawList()->AddCircle(ImVec2(cursor.x + height * 0.5f, cursor.y + height * 0.5f), height * 0.32f, state_color(*row.light), 0, 1.5f);
			}

			if (!present) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			}

			if (ImGui::Selectable(row.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
				select(row.id);
			}

			if (!present) {
				ImGui::PopStyleColor();
			}

			if (ImGui::IsItemHovered())
			{
				g.hovered = row.id;

				const auto& io = ImGui::GetIO();
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && io.MouseDownDurationPrev[ImGuiMouseButton_Right] < 0.3f
					&& io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 36.0f)
				{
					g.context = { row.id.hash, MAIN_LIGHT };
					g.open_context_menu = true;
				}
			}

			std::string info;
			if (!row.owner.empty() && row.owner != g.active_file) {
				info = row.owner + "  ";
			}

			if (!present) {
				info += "not around";
			}
			else
			{
				info += std::format("{:.0f}m", row.distance);
				if (!row.light->on_screen) {
					info = "out of view  " + info;
				}
			}

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(info.c_str()).x);
			ImGui::TextDisabled("%s", info.c_str());

			ImGui::PopID();
			ImGui::PopID();
		}

		void draw_list_window()
		{
			if (!g.show_list) {
				return;
			}

			// below the toolbar: its height is two rows of widgets plus spacing
			{
				const auto& io = ImGui::GetIO();
				const auto& style = ImGui::GetStyle();
				const float toolbar_height = 12.0f + ImGui::GetFrameHeight() + 6.0f + style.ItemSpacing.y * 3.0f + 3.0f + ImGui::GetFontSize() * 0.82f;
				const float top = SCREEN_PADDING + toolbar_height + SCREEN_PADDING;

				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - SCREEN_PADDING, top), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
				ImGui::SetNextWindowSize(ImVec2(340.0f, std::max(io.DisplaySize.y - top - SCREEN_PADDING, 200.0f)), ImGuiCond_FirstUseEver);
			}

			if (!ImGui::Begin("Lights###light_editor_list", &g.show_list, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			shared::imgui::draw_window_blur();

			const float clear_width = ImGui::GetFrameHeight();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clear_width - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::InputTextWithHint("##filter", "Filter (name, hash, category, file)", g.filter.InputBuf, IM_ARRAYSIZE(g.filter.InputBuf))) {
				g.filter.Build();
			}

			ImGui::SameLine();
			if (ImGui::Button("X##filter_clear", ImVec2(clear_width, clear_width))) {
				g.filter.Clear();
			}

			if (ImGui::RadioButton("Nearby", !g.list_all_in_file)) {
				g.list_all_in_file = false;
			}

			ImGui::SameLine();
			if (ImGui::RadioButton("All in active file", g.list_all_in_file)) {
				g.list_all_in_file = true;
			} TT("Every override and ignored light of the active file, around or not");

			ImGui::Separator();

			// rows
			static std::vector<list_row_s> rows;
			rows.clear();

			auto* active_info = g.active_file.empty() ? nullptr : &settings().light_overrides_toml_info[g.active_file];

			const auto add_row = [&](const std::uint64_t hash, const scene_light_s* light)
				{
					list_row_s row;
					row.id = { hash, MAIN_LIGHT };
					row.light = light;
					row.distance = light ? light->distance : FLT_MAX;
					row.name = light_name(hash);
					row.owner = owner_file(hash);

					int category = NO_CATEGORY;
					const bool in_active = !g.active_file.empty() && find_override(g.active_file, hash, &category);

					if ((light && light->ignored) || (!light && !in_active)) {
						row.group = GROUP_IGNORED;
					}
					else if (in_active && row.owner == g.active_file)
					{
						row.group = category == NO_CATEGORY ? GROUP_UNCATEGORIZED : GROUP_CATEGORY;
						row.category = category;
					}
					else {
						row.group = row.owner.empty() ? GROUP_NO_OVERRIDE : GROUP_OTHER_FILE;
					}

					const auto hash_str = std::format("{:X}", hash);
					const char* category_name = row.category != NO_CATEGORY ? active_info->categories[row.category].category_name.c_str() : "";

					if (g.filter.IsActive() && !g.filter.PassFilter(row.name.c_str()) && !g.filter.PassFilter(hash_str.c_str())
						&& !g.filter.PassFilter(category_name) && !g.filter.PassFilter(row.owner.c_str())) {
						return;
					}

					rows.emplace_back(std::move(row));
				};

			if (g.list_all_in_file)
			{
				if (active_info)
				{
					std::unordered_set<std::uint64_t> hashes;
					for (const auto& category : active_info->categories)
					{
						for (const auto& hash : category.overrides | std::views::keys) {
							hashes.insert(hash);
						}
					}

					for (const auto& hash : active_info->flat_overrides | std::views::keys) {
						hashes.insert(hash);
					}

					if (const auto it = settings().lights_toml_info.find(g.active_file); it != settings().lights_toml_info.end()) {
						hashes.insert(it->second.ignored_lights.begin(), it->second.ignored_lights.end());
					}

					for (const auto& hash : hashes) {
						add_row(hash, find_scene_light({ hash, MAIN_LIGHT }));
					}
				}
			}
			else
			{
				for (const auto& light : g.lights)
				{
					if (light.attached_index == MAIN_LIGHT) {
						add_row(light.hash, &light);
					}
				}
			}

			std::ranges::sort(rows, [](const list_row_s& a, const list_row_s& b)
				{
					if (a.group != b.group) {
						return a.group < b.group;
					}

					if (a.category != b.category) {
						return a.category < b.category;
					}

					return a.distance != b.distance ? a.distance < b.distance : a.id.hash < b.id.hash;
				});

			if (ImGui::BeginChild("##rows", ImVec2(0.0f, 0.0f)))
			{
				int last_group = -1;
				int last_category = NO_CATEGORY - 1;
				bool group_open = true;

				for (const auto& row : rows)
				{
					if (row.group != last_group || row.category != last_category)
					{
						last_group = row.group;
						last_category = row.category;

						const char* title = "";
						switch (row.group)
						{
							case GROUP_CATEGORY:		title = active_info->categories[row.category].category_name.c_str(); break;
							case GROUP_UNCATEGORIZED:	title = "Uncategorized overrides"; break;
							case GROUP_OTHER_FILE:		title = "Overrides in other files"; break;
							case GROUP_NO_OVERRIDE:		title = "No override"; break;
							default:					title = "Ignored"; break;
						}

						ImGui::PushID(row.group * 1000 + row.category);
						group_open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);

						if (row.group == GROUP_CATEGORY && ImGui::BeginPopupContextItem("##category_context"))
						{
							shared::imgui::draw_window_blur();
							auto& category = active_info->categories[row.category];

							if (ImGui::MenuItem(ICON_FA_PEN "  Rename ..."))
							{
								g.category_popup_index = row.category;
								g.new_category_for_context = false;
								g.name_buffer = category.category_name;
								g.comment_buffer = category.category_comment;
								g.open_category_popup = true;
							}

							if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy to clipboard")) {
								copy_to_clipboard(build_category(category), "category");
							}

							if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Dissolve category"))
							{
								for (auto& [hash, ov] : category.overrides) {
									active_info->flat_overrides[hash] = ov;
								}

								active_info->categories.erase(active_info->categories.begin() + row.category);
								commit(g.active_file);

								ImGui::EndPopup();
								ImGui::PopID();
								break; // rows refer to the old category indices
							} TT("Moves its overrides to the uncategorized ones");

							ImGui::EndPopup();
						}

						ImGui::PopID();
					}

					if (group_open) {
						draw_list_row(row);
					}
				}

				if (rows.empty()) {
					ImGui::TextDisabled(g.list_all_in_file ? "The active file has no entries" : "No translated lights around");
				}
			}

			ImGui::EndChild();
			ImGui::End();
		}

		/// square button with a small icon, as high as a frame
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

		/// one row of the properties table: label, widget, reset to the game's value
		template <typename widget_fn>
		bool property_row(const char* label, bool& use, const bool disabled, widget_fn&& widget)
		{
			bool changed = false;

			const bool was_used = use;
			ImGui::PushStyleColor(ImGuiCol_Text, was_used ? COL_TEXT_OVERRIDDEN : COL_TEXT_GAME_VALUE);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);

			ImGui::TableNextColumn();
			ImGui::PushID(label);
			ImGui::BeginDisabled(disabled);
			ImGui::SetNextItemWidth(-FLT_MIN);

			if (widget())
			{
				use = true;
				changed = true;
			}

			ImGui::PopStyleColor();

			ImGui::TableNextColumn();
			if (was_used)
			{
				if (icon_button(ICON_FA_UNDO))
				{
					use = false;
					changed = true;
				} TT("Back to the game's value");
			}

			ImGui::EndDisabled();
			ImGui::PopID();
			return changed;
		}

		float cone_degrees(const float cosine) {
			return static_cast<float>(RAD2DEG(acosf(std::clamp(cosine, -1.0f, 1.0f))));
		}

		void draw_properties_window()
		{
			if (!g.selected.hash) {
				return;
			}

			const auto hash = g.selected.hash;
			const auto owner = owner_file(hash);
			const bool in_active = !g.active_file.empty() && find_override(g.active_file, hash);
			auto* ov = editable_override(g.selected);

			// the attached light of the selection is gone
			if (!ov && g.selected.attached_index != MAIN_LIGHT)
			{
				g.selected.attached_index = MAIN_LIGHT;
				ov = editable_override(g.selected);
			}

			game::CLightSource def = {};
			const bool has_def = get_reference_def(hash, def);

			if (g.place_properties && !g.pin_properties)
			{
				ImVec2 pos(ImGui::GetIO().DisplaySize.x * 0.6f, ImGui::GetIO().DisplaySize.y * 0.3f);
				if (const auto light = find_scene_light(g.selected); light && light->on_screen) {
					pos = ImVec2(light->screen.x + 70.0f, light->screen.y - 60.0f);
				}

				ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
			}
			g.place_properties = false;

			bool keep_open = true;
			ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.9f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

			const auto title = std::format("{}###le_properties", light_name(hash));
			if (ImGui::Begin(title.c_str(), &keep_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				// the window follows its content, so it is pushed back whenever a part of it would leave the screen
				{
					const ImVec2 display = ImGui::GetIO().DisplaySize;
					const ImVec2 size = ImGui::GetWindowSize();
					const ImVec2 current = ImGui::GetWindowPos();
					const ImVec2 clamped(std::clamp(current.x, 0.0f, std::max(display.x - size.x, 0.0f)), std::clamp(current.y, 0.0f, std::max(display.y - size.y, 0.0f)));

					if (clamped.x != current.x || clamped.y != current.y) {
						ImGui::SetWindowPos(clamped);
					}
				}

				shared::imgui::draw_window_blur();

				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("0x%llX", static_cast<unsigned long long>(hash));
				if (const auto source = game_lights::get_light_source_name(hash); !source.empty())
				{
					ImGui::SameLine();
					ImGui::TextDisabled("  %s", source.c_str());
				}

				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
				ImGui::Style_ColorButtonPush(g.pin_properties ? COL_BUTTON_ACTIVE : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
				if (icon_button(ICON_FA_THUMBTACK)) {
					g.pin_properties = !g.pin_properties;
				}
				ImGui::Style_ColorButtonPop();
				TT("Keep the window where it is when another light gets selected");

				if (const auto ignored_in = ignoring_file(hash); !ignored_in.empty() && settings().ignored_lights.contains(hash)) {
					ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Ignored in %s", ignored_in.c_str());
				}

				ImGui::Separator();

				if (!ov)
				{
					ImGui::Spacing(0, 4);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.0f));

					if (g.active_file.empty())
					{
						ImGui::TextWrapped("There is no light tweak file yet.");
						if (ImGui::Button(ICON_FA_FILE "  New file ...", ImVec2(-FLT_MIN, 0.0f))) {
							g.open_new_file_popup = true;
						}
					}
					else if (in_active || (!owner.empty() && owner < g.active_file))
					{
						// a higher priority file owns the light, an edit in the active file would have no effect
						ImGui::TextWrapped("Overridden by %s, which has a higher priority than the active file %s.", owner.c_str(), g.active_file.c_str());
						if (ImGui::Button(std::format("Edit in {}", owner).c_str(), ImVec2(-FLT_MIN, 0.0f))) {
							g.active_file = owner;
						}
					}
					else
					{
						if (!owner.empty()) {
							ImGui::TextWrapped("Overridden by %s. An override in the active file %s replaces it.", owner.c_str(), g.active_file.c_str());
						} else {
							ImGui::TextWrapped("No override. Edits go to the active file %s.", g.active_file.c_str());
						}

						ImGui::BeginDisabled(!has_def);
						if (ImGui::Button(ICON_FA_PLUS "  Create override", ImVec2(owner.empty() ? -FLT_MIN : 170.0f, 0.0f))) {
							create_override(hash);
						} TT("Starts from the game's values");

						if (!owner.empty())
						{
							ImGui::SameLine();
							if (ImGui::Button(ICON_FA_COPY "  Copy existing", ImVec2(-FLT_MIN, 0.0f)))
							{
								const override_s copy = *find_override(owner, hash);
								create_override(hash, &copy);
							} TT("Starts from the override of the other file");
						}
						ImGui::EndDisabled();
					}

					ImGui::PopStyleVar();
					ImGui::Spacing(0, 2);
				}
				else if (has_def)
				{
					bool changed = false;
					auto* main_ov = find_override(g.active_file, hash);
					const bool attached = g.selected.attached_index != MAIN_LIGHT;

					// main light / attached lights
					{
						ImGui::Style_ColorButtonPush(!attached ? COL_BUTTON_ACTIVE : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
						if (ImGui::Button("Main")) {
							g.selected.attached_index = MAIN_LIGHT;
						}
						ImGui::Style_ColorButtonPop();

						for (auto i = 0; i < static_cast<int>(main_ov->attached_lights.size()); i++)
						{
							ImGui::SameLine(0.0f, 4.0f);
							ImGui::PushID(i);
							ImGui::Style_ColorButtonPush(g.selected.attached_index == i ? COL_BUTTON_ACTIVE : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
							if (ImGui::Button(std::to_string(i + 1).c_str(), ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
								g.selected.attached_index = i;
							}
							ImGui::Style_ColorButtonPop();
							TT(main_ov->attached_lights[i].comment.empty() ? "Attached light" : main_ov->attached_lights[i].comment.c_str());
							ImGui::PopID();
						}

						ImGui::SameLine(0.0f, 4.0f);
						if (icon_button(ICON_FA_PLUS "##attach")) {
							attach_light(hash);
						} TT("Attach a new light: another light that exists as long as this light does");

						if (attached)
						{
							ImGui::SameLine(0.0f, 4.0f);
							ImGui::Style_DeleteButtonPush();
							if (icon_button(ICON_FA_TRASH "##detach"))
							{
								main_ov->attached_lights.erase(main_ov->attached_lights.begin() + g.selected.attached_index);
								g.selected.attached_index = MAIN_LIGHT;
								commit(g.active_file);
								ImGui::Style_DeleteButtonPop();
								ImGui::End();
								ImGui::PopStyleVar();
								ImGui::PopStyleColor();
								return;
							}
							ImGui::Style_DeleteButtonPop();
							TT("Remove this attached light");
						}

						// the selection can have changed above
						ov = editable_override(g.selected);
					}

					ImGui::Spacing(0, 2);

					if (ov && ImGui::BeginTable("##properties", 3, ImGuiTableFlags_SizingFixedFit))
					{
						ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
						ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("reset", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());

						// name
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted("Name");
						ImGui::TableNextColumn();
						ImGui::SetNextItemWidth(-FLT_MIN);
						changed |= ImGui::InputTextWithHint("##comment", "comment in the file", &ov->comment);

						// category
						if (g.selected.attached_index == MAIN_LIGHT)
						{
							int category = NO_CATEGORY;
							find_override(g.active_file, hash, &category);
							const auto& categories = settings().light_overrides_toml_info[g.active_file].categories;

							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::AlignTextToFramePadding();
							ImGui::TextUnformatted("Category");
							ImGui::TableNextColumn();
							ImGui::SetNextItemWidth(-FLT_MIN);

							if (ImGui::BeginCombo("##category", category == NO_CATEGORY ? "Uncategorized" : categories[category].category_name.c_str()))
							{
								category_menu_items(g.active_file, hash);
								ImGui::EndCombo();

								// the override can have moved to another container
								ov = editable_override(g.selected);
							}
						}

						if (ov)
						{
							const bool spot = is_spot(*ov, def);

							// type
							changed |= property_row("Type", ov->_use_light_type, false, [&]
								{
									bool edited = false;
									if (ImGui::RadioButton("Sphere", !spot))
									{
										ov->light_type = false;
										edited = true;
									}

									ImGui::SameLine();
									if (ImGui::RadioButton("Spot", spot))
									{
										ov->light_type = true;
										edited = true;
									}

									return edited;
								});

							changed |= property_row("Color", ov->_use_color, false, [&]
								{
									Vector value = ov->_use_color ? ov->color : Vector(def.mColor);
									if (ImGui::ColorEdit3("##v", &value.x, ImGuiColorEditFlags_Float))
									{
										ov->color = value;
										return true;
									}
									return false;
								});

							const auto float_row = [&](const char* label, bool& use, float& value, const float game_value, const float speed, const float min, const float max)
								{
									changed |= property_row(label, use, false, [&]
										{
											float v = use ? value : game_value;
											if (ImGui::DragFloat("##v", &v, speed, min, max, "%.2f", ImGuiSliderFlags_AlwaysClamp))
											{
												value = v;
												return true;
											}
											return false;
										});
								};

							float_row("Intensity", ov->_use_intensity, ov->intensity, def.mIntensity, 0.025f, 0.0f, FLT_MAX);
							float_row("Radius", ov->_use_radius, ov->radius, def.mRadius, 0.025f, 0.0f, FLT_MAX);
							float_row("Volumetric", ov->_use_volumetric_scale, ov->volumetric_scale, def.mVolumeScale, 0.01f, 0.0f, 10.0f);

							if (spot)
							{
								// cone angles are stored as cosines
								const bool game_spot = def.mType == game::LT_SPOT;
								const auto effective_degrees = [&](const bool use, const float value, const float game_value) {
									return cone_degrees(use || !game_spot ? value : game_value);
								};

								const auto cone_row = [&](const char* label, bool& use, float& value, const float game_value, const float default_value, const float max_degrees)
									{
										const bool was_used = use;
										const bool row_changed = property_row(label, use, false, [&]
											{
												float degrees = effective_degrees(use, value, game_value);
												if (ImGui::DragFloat("##v", &degrees, 0.1f, 0.0f, std::max(max_degrees, 0.01f), "%.1f deg", ImGuiSliderFlags_AlwaysClamp))
												{
													value = cosf(static_cast<float>(DEG2RAD(degrees)));
													return true;
												}
												return false;
											});

										// the stored value stays in use for a light that is no spotlight in the game, so a reset has to change it
										if (row_changed && was_used && !use && !game_spot) {
											value = default_value;
										}

										changed |= row_changed;
										return row_changed;
									};

								// the inner cone can not be wider than the outer one, a smaller outer cone takes the inner one with it
								const float outer_degrees = effective_degrees(ov->_use_outer_cone_angle, ov->outer_cone_angle, def.mOuterConeAngle);
								cone_row("Inner cone", ov->_use_inner_cone_angle, ov->inner_cone_angle, def.mInnerConeAngle, DEFAULT_INNER_CONE, outer_degrees);

								if (cone_row("Outer cone", ov->_use_outer_cone_angle, ov->outer_cone_angle, def.mOuterConeAngle, DEFAULT_OUTER_CONE, 180.0f))
								{
									const float new_outer_degrees = effective_degrees(ov->_use_outer_cone_angle, ov->outer_cone_angle, def.mOuterConeAngle);
									if (effective_degrees(ov->_use_inner_cone_angle, ov->inner_cone_angle, def.mInnerConeAngle) > new_outer_degrees)
									{
										ov->inner_cone_angle = cosf(static_cast<float>(DEG2RAD(new_outer_degrees)));
										ov->_use_inner_cone_angle = true;
									}
								}

								changed |= property_row("Direction", ov->_use_dir, false, [&]
									{
										Vector value = ov->_use_dir ? ov->dir : def.mDirection;
										if (ImGui::DragFloat3("##v", &value.x, 0.01f, -1.0f, 1.0f, "%.2f"))
										{
											if (value.LengthSqr() > 1.e-6f) {
												value.Normalize();
											}

											ov->dir = value;
											return true;
										}
										return false;
									});
							}

							const bool position_locked = g.selected_instanced && g.selected.attached_index == MAIN_LIGHT;
							changed |= property_row("Position", ov->_use_pos, position_locked, [&]
								{
									Vector value = ov->_use_pos ? ov->pos : def.mPosition;
									if (ImGui::DragFloat3("##v", &value.x, 0.025f, 0.0f, 0.0f, "%.2f"))
									{
										ov->pos = value;
										return true;
									}
									return false;
								});

							if (position_locked) {
								ImGui::SetItemTooltip("%s", "Several lights share this hash (the same light on every vehicle of the model), a position would pin all of them to one place.");
							}
						}

						ImGui::EndTable();
					}

					if (changed) {
						commit(g.active_file);
					}

					ImGui::Spacing(0, 4);
					const float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

					const float button_height = ImGui::GetFrameHeight() + 4.0f;
					if (ov && ImGui::Button(ICON_FA_UNDO "  Game values", ImVec2(button_width, button_height)))
					{
						ov->_use_pos = ov->_use_dir = ov->_use_color = ov->_use_radius = ov->_use_intensity = false;
						ov->_use_outer_cone_angle = ov->_use_inner_cone_angle = ov->_use_volumetric_scale = ov->_use_light_type = false;
						commit(g.active_file);
					} TT("Drops every overridden value of this light, the entry stays");

					ImGui::SameLine();
					ImGui::Style_DeleteButtonPush();
					if (ImGui::Button(ICON_FA_TRASH "  Remove override", ImVec2(button_width, button_height)))
					{
						remove_override(g.active_file, hash);
						g.selected.attached_index = MAIN_LIGHT;
					}
					ImGui::Style_DeleteButtonPop();
				}
				else {
					ImGui::TextDisabled("The light is not around");
				}
			}

			ImGui::End();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			if (!keep_open) {
				g.selected = {};
			}
		}

		void handle_shortcuts()
		{
			const auto& io = ImGui::GetIO();
			if (io.WantTextInput || io.MouseDown[ImGuiMouseButton_Right]) {
				return;
			}

			if (io.KeyCtrl)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
					restore_snapshot(g.undo, g.redo);
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
					restore_snapshot(g.redo, g.undo);
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
					save_active_file();
				}

				return;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
				g.rotate_mode = false;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
				g.rotate_mode = true;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
				g.selected = {};
			}
		}

#pragma endregion
	}

	bool is_open() {
		return g.open;
	}

	void set_open(const bool open)
	{
		if (open == g.open) {
			return;
		}

		if (!open)
		{
			if (!g.dirty_files.empty()) {
				g.open_close_popup = true;
			} else {
				close_editor();
			}

			return;
		}

		g.open = true;
		shared::globals::imgui_editor_open = true;
		shared::globals::imgui_menu_open = false;
		ensure_active_file();

		if (static bool opened_before = false; !opened_before)
		{
			opened_before = true;
			free_camera::set_active(true);
		}
	}

	void toggle() {
		set_open(!g.open);
	}

	bool has_unsaved_changes() {
		return !g.dirty_files.empty();
	}

	bool allows_game_input_on_rmb() {
		return g.open && g.rmb_game_input && !free_camera::is_active();
	}

	void on_map_settings_reloaded()
	{
		g.dirty_files.clear();
		g.undo.clear();
		g.redo.clear();
		g.in_edit = false;
		g.saved_valid = false;
		g.active_file.clear();
	}

	void draw()
	{
		if (!g.open) {
			return;
		}

		// ignored lights stay translated (without intensity), every light updates each frame and no light is culled
		imgui::get()->m_dbg_visualize_api_light_hashes = true;

		ensure_active_file();
		setup_gizmo_style();
		ImGuizmo::BeginFrame();

		if (!g.saved_valid && g.dirty_files.empty())
		{
			g.saved = take_snapshot();
			g.saved_valid = true;
		}

		if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing()) {
			g.in_edit = false;
		}

		if (!g.in_edit) {
			g.frame_snapshot = take_snapshot();
		}

		g.hovered_drawn = g.hovered;
		g.hovered = {};

		collect_scene_lights();
		handle_shortcuts();
		draw_gizmo();
		draw_world_overlay();
		draw_toolbar();
		draw_list_window();
		draw_properties_window();
	}
}
