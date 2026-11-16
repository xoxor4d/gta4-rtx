#include "std_include.hpp"
#include "imgui_marker_editor.hpp"

#include <algorithm>
#include <format>

#include "free_camera.hpp"
#include "imgui.hpp"
#include "imgui_editor_common.hpp"
#include "map_settings.hpp"
#include "map_settings_toml.hpp"
#include "natives.hpp"
#include "scene_view.hpp"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "shared/common/editor_history.hpp"
#include "shared/imgui/editor_host.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/gizmo.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/world_draw.hpp"

#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

namespace gta4::marker_editor
{
	namespace
	{
		using marker_s = map_settings::marker_settings_s;

		namespace world = shared::imgui::world;
		namespace gizmo = shared::imgui::gizmo;
		using shared::imgui::editor::tool_button;
		using shared::imgui::editor::tool_separator;
		using shared::imgui::editor::property_row;
		using shared::imgui::editor::COL_BUTTON_UNSAVED;

		constexpr const char* MARKERS_DIR = "rtx_comp\\markers";
		constexpr int NO_MARKER = -1;
		constexpr float SCREEN_PADDING = 16.0f; // default distance of the windows to the screen edges and to each other
		constexpr float DEFAULT_WEATHER_TRANSITION = 0.2f;
		constexpr float MIN_GIZMO_SCALE = 0.001f;

		// numbers of the rain markers that the mod spawns itself
		constexpr std::uint32_t RESERVED_NUMBERS_BEGIN = 9000u;
		constexpr std::uint32_t RESERVED_NUMBERS_END = 9100u;
		constexpr std::uint32_t FIRST_NEW_NUMBER = 1u;
		constexpr float TP_CAMERA_DISTANCE = 6.0f; // the free camera looks at a marker from here after teleporting to it

		constexpr ImU32 COL_SELECTED = IM_COL32(60, 200, 255, 255);
		constexpr ImU32 COL_HOVERED = IM_COL32(255, 220, 60, 255);
		constexpr ImU32 COL_VISIBLE = IM_COL32(90, 214, 106, 255);
		constexpr ImU32 COL_HIDDEN = IM_COL32(140, 140, 140, 255);
		constexpr ImU32 COL_OVERRIDDEN = IM_COL32(226, 75, 74, 255);

		// everything an edit can change, for undo / redo
		struct snapshot_s
		{
			std::vector<marker_s> markers;
			std::set<std::string> files;
		};

		enum class action_e : std::uint8_t
		{
			none,
			select,
			duplicate,
			remove,
			tp_to_marker,
			tp_marker_to_player,
			move_to_file,
		};

		struct state_s
		{
			bool open = false;
			bool show_list = true;
			bool show_points = true;
			bool show_labels = true;
			bool rmb_game_input = false;
			bool local_space = false;
			gizmo::operation gizmo_op = gizmo::operation::translate;
			float view_distance = 500.0f; // markers farther away get no point in the world

			std::string active_file;	// new markers go to this file
			std::set<std::string> dirty_files;

			// indices into the marker list
			int selected = NO_MARKER;
			int hovered = NO_MARKER;		// built during the frame
			int hovered_drawn = NO_MARKER;	// the hovered marker of the last frame, points are drawn before the list sets 'hovered'
			int context = NO_MARKER;
			bool open_context_menu = false;
			bool open_new_file_popup = false;
			bool open_close_popup = false;
			bool scroll_to_selected = false;

			world::view_s view;
			bool view_valid = false;

			float gizmo_matrix[16] = {};
			bool step_held = false;

			ImGuiTextFilter filter;
			shared::common::editor_history<snapshot_s> history;

			snapshot_s saved; // the files as they are on disk, for discard
			bool saved_valid = false;

			std::string name_buffer;
			shared::imgui::editor::status_toast_s status;
		};

		state_s g;

#pragma region FILES_AND_MARKERS

		auto& settings() {
			return map_settings::get_map_settings();
		}

		auto& marker_list() {
			return settings().map_markers;
		}

		/// all marker files, highest priority first
		std::vector<std::string> sorted_files() {
			return std::vector<std::string>(settings().marker_files.begin(), settings().marker_files.end());
		}

		void ensure_active_file()
		{
			const auto& files = settings().marker_files;
			if (!g.active_file.empty() && files.contains(g.active_file)) {
				return;
			}

			g.active_file = files.empty() ? std::string() : *files.begin();
		}

		bool is_valid_index(const int index) {
			return index >= 0 && index < static_cast<int>(marker_list().size());
		}

		bool is_reserved_number(const std::uint32_t number) {
			return number >= RESERVED_NUMBERS_BEGIN && number < RESERVED_NUMBERS_END;
		}

		bool has_spawn_conditions(const marker_s& m) {
			return m.weather_type != game::WEATHER_NONE || (m.from_hour >= 0 && m.to_hour >= 0);
		}

		/// the file whose markers with the number spawn, empty if no marker uses the number
		std::string owner_file(const std::uint32_t number)
		{
			const std::string* owner = nullptr;
			for (const auto& m : marker_list())
			{
				if (m.index == number && (!owner || m.file < *owner)) {
					owner = &m.file;
				}
			}

			return owner ? *owner : std::string();
		}

		std::string marker_label(const marker_s& m) {
			return m.comment.empty() ? std::format("#{}", m.index) : std::format("#{}  {}", m.index, m.comment);
		}

		void set_status(std::string text) {
			g.status.set(std::move(text));
		}

		/// indices that are not the selection do not survive a change of the marker list
		void reset_hover()
		{
			g.hovered = NO_MARKER;
			g.hovered_drawn = NO_MARKER;
			g.context = NO_MARKER;
		}

		/// the renderer rechecks the spawn conditions and the cull distance of every marker on its next frame
		void recheck_visibility()
		{
			for (auto& m : marker_list()) {
				m.internal__frames_until_next_vis_check = 0u;
			}
		}

		snapshot_s take_snapshot() {
			return { marker_list(), settings().marker_files };
		}

		/// call after every change: the first change of an interaction puts the state before it on the undo stack
		void commit(const std::string& file)
		{
			g.history.commit();
			g.dirty_files.insert(file);
			map_settings::resolve_marker_priority();
		}

		void commit(marker_s& m)
		{
			m.internal__frames_until_next_vis_check = 0u;
			commit(m.file);
		}

		void apply_snapshot(snapshot_s&& snapshot)
		{
			marker_list() = std::move(snapshot.markers);
			settings().marker_files = std::move(snapshot.files);

			// a file that the editor created can be gone again
			std::erase_if(g.dirty_files, [](const std::string& file) { return !settings().marker_files.contains(file); });

			for (const auto& file : settings().marker_files) {
				g.dirty_files.insert(file);
			}

			if (!is_valid_index(g.selected)) {
				g.selected = NO_MARKER;
			}

			reset_hover();
			recheck_visibility();
			map_settings::resolve_marker_priority();
		}

		void undo()
		{
			if (g.history.undo(take_snapshot, apply_snapshot)) {
				ensure_active_file();
			}
		}

		void redo()
		{
			if (g.history.redo(take_snapshot, apply_snapshot)) {
				ensure_active_file();
			}
		}

		/// lowest number above 0 that no marker of any file uses
		std::uint32_t free_marker_number()
		{
			std::unordered_set<std::uint32_t> used;
			for (const auto& m : marker_list()) {
				used.insert(m.index);
			}

			std::uint32_t number = FIRST_NEW_NUMBER;
			while (used.contains(number) || is_reserved_number(number)) {
				number++;
			}

			return number;
		}

		Vector marker_position_at_player()
		{
			Vector player_pos;
			player_pos = game::FindPlayerCentreOfWorld(&player_pos);
			player_pos.z -= 0.5f;
			return player_pos;
		}

		void add_marker()
		{
			if (g.active_file.empty()) {
				return;
			}

			marker_s m;
			m.index = free_marker_number();
			m.origin = marker_position_at_player();
			m.file = g.active_file;

			marker_list().emplace_back(std::move(m));
			g.selected = static_cast<int>(marker_list().size()) - 1;
			g.scroll_to_selected = true;
			reset_hover();
			commit(g.active_file);
		}

		/// the copy keeps the number and follows its source in the same file
		void duplicate_marker(const int index)
		{
			marker_s copy = marker_list()[index];
			const std::string file = copy.file;

			marker_list().insert(marker_list().begin() + index + 1, std::move(copy));
			g.selected = index + 1;
			g.scroll_to_selected = true;
			reset_hover();
			commit(file);
		}

		void remove_marker(const int index)
		{
			const std::string file = marker_list()[index].file;
			marker_list().erase(marker_list().begin() + index);

			if (g.selected == index) {
				g.selected = NO_MARKER;
			}
			else if (g.selected > index) {
				g.selected--;
			}

			reset_hover();
			commit(file);
		}

		/// the marker becomes the last one of the other file
		void move_marker_to_file(const int index, const std::string& file)
		{
			marker_s moved = marker_list()[index];
			if (moved.file == file) {
				return;
			}

			const bool was_selected = g.selected == index;
			const std::string old_file = moved.file;
			moved.file = file;
			moved.internal__frames_until_next_vis_check = 0u;

			remove_marker(index);
			marker_list().emplace_back(std::move(moved));

			if (was_selected) {
				g.selected = static_cast<int>(marker_list().size()) - 1;
			}

			commit(file);
			set_status("Moved the marker from " + old_file + " to " + file);
		}

		void teleport_player_to(const Vector& pos)
		{
			const auto n = natives::get();
			natives::Ped ped;

			n->GetPlayerChar(n->ConvertIntToPlayerindex(n->GetPlayerId()), &ped);
			n->SetCharCoordinatesNoOffset(ped, pos.x, pos.y, pos.z);
		}

		void run_action(const action_e action, const int index, const std::string& file = {})
		{
			if (action == action_e::none || !is_valid_index(index)) {
				return;
			}

			switch (action)
			{
				case action_e::select:
					g.selected = index;
					break;

				case action_e::duplicate:
					duplicate_marker(index);
					break;

				case action_e::remove:
					remove_marker(index);
					break;

				case action_e::tp_to_marker:
					teleport_player_to(marker_list()[index].origin);
					free_camera::focus(marker_list()[index].origin, TP_CAMERA_DISTANCE);
					break;

				case action_e::tp_marker_to_player:
					marker_list()[index].origin = marker_position_at_player();
					commit(marker_list()[index]);
					break;

				case action_e::move_to_file:
					move_marker_to_file(index, file);
					break;

				default:
					break;
			}
		}

#pragma endregion

#pragma region SAVE_AND_EXPORT

		std::vector<marker_s> markers_of_file(const std::string& file)
		{
			std::vector<marker_s> list;
			for (const auto& m : marker_list())
			{
				if (m.file == file) {
					list.emplace_back(m);
				}
			}

			return list;
		}

		/// the markers that spawn
		std::vector<marker_s> effective_markers()
		{
			std::vector<marker_s> list;
			for (const auto& m : marker_list())
			{
				if (!m.internal__is_overridden) {
					list.emplace_back(m);
				}
			}

			return list;
		}

		std::string build_file(const std::string& file)
		{
			static const std::string rule = "# " + std::string(138, '#') + "\n";

			std::string text = rule;
			text +=
				"# >> Spawn unique marker meshes\n"
				"#\n"
				"# :: Can be used as anchor mesh (same one can be spawned multiple times)\n"
				"#\n"
				"# :: How to edit:\n"
				"#    > Marker Editor (GUI -> Creator Sections -> World Edits -> Map Markers -> Open Marker Editor)\n"
				"#    > Add, move, rotate and scale markers in the world. Save in the editor's toolbar\n"
				"#\n"
				"# :: Notes:\n"
				"#    > A marker number can be used several times within a file, every entry spawns\n"
				"#    > Priority: files are processed in alphabetical order (earlier files have higher priority).\n"
				"#      A number that a file uses is dropped from every file with a lower priority\n"
				"#    > Numbers 9000-9099 are reserved\n"
				"#    > Comments: the comment above an entry is kept and shown as the name of the marker\n"
				"#\n"
				"# ~~ Parameters\n"
				"#   {\n"
				"#       #                                   comment\n"
				"#       marker_num =                        num of marker                                                           [Int 0-Inf]\n"
				"#       position = []                       position of the marker mesh                                             [3D Vector]\n"
				"#       rotation = []                       rotation of the marker mesh in degrees                                  [3D Vector]\n"
				"#       scale = []                          scale of the marker mesh                                                [3D Vector]\n"
				"#       cull_distance =                     disable drawing of marker mesh after this distance from the player      [Float]\n"
				"#       spawn_on = {\n"
				"#           weather = \"TYPE\"                only spawn marker when THIS weather is active                           [STRING] - [EXTRASUNNY, SUNNY, SUNNY_WINDY, CLOUDY, RAIN, DRIZZLE, FOGGY, LIGHTNING]\n"
				"#           weather_transition_value =      transition value the weather needs to be at for the marker to show      [Float 0-1]\n"
				"#           between_hours = [from, to]      only spawn marker between certain hours (from-to)                       [2D Int]\n"
				"#       }\n"
				"#   },\n"
				"#   { ... },\n\n";

			text += map_settings_toml::build_map_marker_array(markers_of_file(file)) + "\n";
			return text;
		}

		/// replaces the markers of a file in the state that is on disk with the current ones
		void remember_saved_file(const std::string& file)
		{
			std::erase_if(g.saved.markers, [&file](const marker_s& m) { return m.file == file; });
			for (const auto& m : marker_list())
			{
				if (m.file == file) {
					g.saved.markers.emplace_back(m);
				}
			}

			g.saved.files.insert(file);
		}

		bool save_file(const std::string& file)
		{
			const auto target = std::filesystem::path(shared::globals::root_path) / MARKERS_DIR / file;

			switch (shared::utils::write_file_atomic(target, build_file(file)))
			{
				case shared::utils::write_file_result::write_failed:
					set_status("Could not write " + file);
					return false;

				case shared::utils::write_file_result::replace_failed:
					set_status("Could not replace " + file);
					return false;

				default:
					break;
			}

			g.dirty_files.erase(file);
			remember_saved_file(file);
			return true;
		}

		/// puts a file back to what is on disk, a file that was never saved disappears
		void discard_file(const std::string file)
		{
			auto& list = marker_list();
			std::erase_if(list, [&file](const marker_s& m) { return m.file == file; });

			for (const auto& m : g.saved.markers)
			{
				if (m.file == file) {
					list.emplace_back(m);
				}
			}

			if (!g.saved.files.contains(file)) {
				settings().marker_files.erase(file);
			}

			g.selected = NO_MARKER;
			reset_hover();
			recheck_visibility();
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
				if (settings().marker_files.contains(file) && save_file(file)) {
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

		float axis_sign(const float value) {
			return value < 0.0f ? -1.0f : 1.0f;
		}

		/// an axis without length would have no direction in the gizmo matrix
		float gizmo_axis_scale(const float value) {
			return fabsf(value) < MIN_GIZMO_SCALE ? MIN_GIZMO_SCALE * axis_sign(value) : value;
		}

		/// the transform the renderer draws a marker with: scale, rotation around z, y and x, translation
		void marker_matrix(const marker_s& m, float matrix[16])
		{
			D3DXMATRIX scale, rot_x, rot_y, rot_z, translation;
			D3DXMatrixScaling(&scale, gizmo_axis_scale(m.scale.x), gizmo_axis_scale(m.scale.y), gizmo_axis_scale(m.scale.z));
			D3DXMatrixRotationX(&rot_x, m.rotation.x);
			D3DXMatrixRotationY(&rot_y, m.rotation.y);
			D3DXMatrixRotationZ(&rot_z, m.rotation.z);
			D3DXMatrixTranslation(&translation, m.origin.x, m.origin.y, m.origin.z);

			const D3DXMATRIX transform = scale * rot_z * rot_y * rot_x * translation;
			memcpy(matrix, &transform.m[0][0], sizeof(float) * 16u);
		}

		/// marker rotation (radians) from the normalized axes of its transform
		Vector rotation_from_axes(const Vector& x_axis, const Vector& y_axis, const Vector& z_axis)
		{
			Vector rotation(0.0f, 0.0f, 0.0f);
			rotation.y = asinf(std::clamp(z_axis.x, -1.0f, 1.0f));

			if (fabsf(z_axis.x) < 0.9999f)
			{
				rotation.x = atan2f(-z_axis.y, z_axis.z);
				rotation.z = atan2f(-y_axis.x, x_axis.x);
			}
			else
			{
				// x and z turn around the same axis, all of it goes to z
				rotation.z = atan2f(x_axis.y, y_axis.y);
			}

			return rotation;
		}

		void draw_gizmo()
		{
			if (!is_valid_index(g.selected) || !g.view_valid) {
				return;
			}

			auto& m = marker_list()[g.selected];
			if (!gizmo::is_using()) {
				marker_matrix(m, g.gizmo_matrix);
			}

			if (!gizmo::manipulate(g.view, g.gizmo_op, g.local_space, g.gizmo_matrix)) {
				return;
			}

			Vector x_axis, y_axis, z_axis, pos;
			gizmo::axes_from_matrix(g.gizmo_matrix, x_axis, y_axis, z_axis, pos);

			// the axes carry the scale, a negative scale mirrors its axis
			const Vector lengths(x_axis.Length(), y_axis.Length(), z_axis.Length());
			const Vector signs(axis_sign(m.scale.x), axis_sign(m.scale.y), axis_sign(m.scale.z));

			switch (g.gizmo_op)
			{
				case gizmo::operation::rotate:
					if (lengths.x > 0.0f && lengths.y > 0.0f && lengths.z > 0.0f) {
						m.rotation = rotation_from_axes(x_axis * (signs.x / lengths.x), y_axis * (signs.y / lengths.y), z_axis * (signs.z / lengths.z));
					}
					break;

				case gizmo::operation::scale:
					m.scale = lengths * signs;
					break;

				default:
					m.origin = pos;
					break;
			}

			commit(m);
		}

		ImU32 state_color(const marker_s& m)
		{
			if (m.internal__is_overridden) {
				return COL_OVERRIDDEN;
			}

			return m.internal__is_hidden ? COL_HIDDEN : COL_VISIBLE;
		}

		void draw_world_overlay()
		{
			if (!g.view_valid) {
				return;
			}

			const auto& io = ImGui::GetIO();
			const auto draw_list = ImGui::GetBackgroundDrawList();
			const auto& list = marker_list();
			constexpr float edge_inset = 34.0f;

			struct point_s
			{
				int index;
				ImVec2 pos;
				ImVec2 dir; // direction of the edge arrow, unused for markers on screen
				bool on_screen;
			};

			static std::vector<point_s> points;
			points.clear();

			// markers on screen get a circle, the selected marker and the marker hovered in the list an arrow at the screen edge when they are not
			for (auto i = 0; i < static_cast<int>(list.size()); i++)
			{
				const bool highlighted = i == g.selected || i == g.hovered_drawn;
				if (!highlighted && g.view.cam_pos.DistTo(list[i].origin) > g.view_distance) {
					continue;
				}

				point_s point = { i, ImVec2(0.0f, 0.0f), ImVec2(0.0f, 0.0f), false };
				bool in_front = false;
				point.on_screen = world::project(g.view, list[i].origin, point.pos, &in_front);

				if (!point.on_screen)
				{
					if (!highlighted) {
						continue;
					}

					const ImVec2 screen = point.pos;
					world::edge_arrow(screen, in_front, edge_inset, point.pos, point.dir);
				}

				points.emplace_back(point);
			}

			// picking
			const bool can_pick = g.show_points && !io.WantCaptureMouse && !io.MouseDown[ImGuiMouseButton_Right] && !gizmo::is_over() && !gizmo::is_using();
			int nearest = NO_MARKER;

			if (can_pick)
			{
				if (const int point_index = world::nearest_point(points, io.MousePos, 16.0f, [](const point_s& point) { return point.pos; }); point_index >= 0) {
					nearest = points[point_index].index;
				}
			}

			if (nearest != NO_MARKER)
			{
				g.hovered = nearest;
				g.hovered_drawn = nearest;
			}

			if (g.show_points)
			{
				for (const auto& point : points)
				{
					const auto& m = list[point.index];
					const bool selected = point.index == g.selected;
					const bool hovered = point.index == g.hovered_drawn;
					const ImU32 fill = state_color(m);
					const ImU32 ring_color = selected ? COL_SELECTED : hovered ? COL_HOVERED : IM_COL32(255, 255, 255, 200);
					float radius = selected || hovered ? 8.0f : 6.0f;

					if (point.on_screen)
					{
						draw_list->AddCircleFilled(point.pos, radius + 1.5f, IM_COL32(0, 0, 0, 160));
						draw_list->AddCircleFilled(point.pos, radius, fill);
						draw_list->AddCircle(point.pos, radius, ring_color, 0, selected || hovered ? 2.5f : 1.5f);
					}
					else
					{
						// arrow at the screen edge pointing towards the marker
						radius = 12.0f;
						world::draw_edge_arrow(draw_list, point.pos, point.dir, radius, fill, ring_color);
					}

					if (!g.show_labels && !selected && !hovered) {
						continue;
					}

					// number and comment, then what else is known about a hovered or selected marker
					std::string lines[3];
					auto count = 0;

					lines[count++] = marker_label(m);

					if (selected || hovered)
					{
						lines[count++] = m.file;

						if (m.internal__is_overridden) {
							lines[count++] = "overridden by " + owner_file(m.index);
						}
						else if (m.internal__is_hidden) {
							lines[count++] = "hidden by its spawn conditions or cull distance";
						}
					}

					float width = 0.0f;
					for (auto i = 0; i < count; i++) {
						width = std::max(width, ImGui::CalcTextSize(lines[i].c_str()).x);
					}

					// markers at the right or bottom edge keep their label on screen
					ImVec2 text_pos(point.pos.x + radius + 8.0f, point.pos.y - ImGui::GetFontSize() * 0.5f);
					if (text_pos.x + width > io.DisplaySize.x - 4.0f) {
						text_pos.x = point.pos.x - radius - 8.0f - width;
					}

					text_pos.y = std::min(text_pos.y, io.DisplaySize.y - static_cast<float>(count) * ImGui::GetFontSize() - 4.0f);

					for (auto i = 0; i < count; i++)
					{
						world::draw_text_shadowed(draw_list, ImVec2(text_pos.x, text_pos.y + static_cast<float>(i) * ImGui::GetFontSize()),
							i ? IM_COL32(200, 200, 200, 255) : selected ? COL_SELECTED : hovered ? COL_HOVERED : IM_COL32(235, 235, 235, 255), lines[i].c_str());
					}
				}
			}

			if (nearest != NO_MARKER)
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					g.selected = nearest;
					g.scroll_to_selected = true;
				}

				// a held right button can hand the input to the game, so a short click opens the menu on release
				if (world::is_short_right_click())
				{
					g.context = nearest;
					g.open_context_menu = true;
				}
			}
		}

#pragma endregion

#pragma region WINDOWS

		void draw_context_menu()
		{
			if (g.open_context_menu)
			{
				ImGui::OpenPopup("##me_context");
				g.open_context_menu = false;
			}

			if (!ImGui::BeginPopup("##me_context")) {
				return;
			}

			if (!is_valid_index(g.context))
			{
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return;
			}

			shared::imgui::draw_window_blur();

			const int index = g.context;
			const auto& m = marker_list()[index];
			auto action = action_e::none;
			std::string target_file;

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::TextUnformatted(marker_label(m).c_str());
			ImGui::PopFont();
			ImGui::TextDisabled("%s", m.file.c_str());
			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_MOUSE_POINTER "  Select")) {
				action = action_e::select;
			}

			if (ImGui::MenuItem(ICON_FA_COPY "  Duplicate")) {
				action = action_e::duplicate;
			}

			if (ImGui::MenuItem(ICON_FA_TRASH "  Delete")) {
				action = action_e::remove;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_LOCATION_ARROW "  TP to marker", nullptr, false, game::is_in_game)) {
				action = action_e::tp_to_marker;
			}

			if (ImGui::MenuItem(ICON_FA_STREET_VIEW "  TP marker to player", nullptr, false, game::is_in_game)) {
				action = action_e::tp_marker_to_player;
			}

			ImGui::Separator();

			if (ImGui::BeginMenu(ICON_FA_FOLDER "  Move to file"))
			{
				shared::imgui::draw_window_blur();
				for (const auto& file : settings().marker_files)
				{
					if (ImGui::MenuItem(file.c_str(), nullptr, file == m.file) && file != m.file)
					{
						target_file = file;
						action = action_e::move_to_file;
					}
				}

				ImGui::EndMenu();
			}

			ImGui::EndPopup();
			run_action(action, index, target_file);
		}

		void draw_new_file_popup()
		{
			std::string file;
			if (shared::imgui::editor::new_file_popup("New marker file", "Files are applied in alphabetical order, earlier names have higher priority.",
				g.open_new_file_popup, g.name_buffer, sorted_files(), file))
			{
				settings().marker_files.insert(file);
				g.active_file = file;
				commit(file);
			}
		}

		void close_editor()
		{
			g.open = false;
			free_camera::set_active(false);
			shared::imgui::editor_host::sync_open_state();
		}

		void draw_close_popup()
		{
			switch (shared::imgui::editor::unsaved_changes_popup("Unsaved markers", g.open_close_popup, g.dirty_files,
				"The changes stay active until the map settings are reloaded and can still be saved when the editor is opened again."))
			{
				case shared::imgui::editor::close_action::save_all_and_close:
					save_all_files();
					close_editor();
					break;

				case shared::imgui::editor::close_action::close_without_saving:
					close_editor();
					break;

				default:
					break;
			}
		}

		void draw_file_menu()
		{
			if (!ImGui::BeginPopup("##me_file_menu")) {
				return;
			}

			shared::imgui::draw_window_blur();

			std::string file_to_discard;
			switch (shared::imgui::editor::file_menu_items(sorted_files(), g.active_file, g.dirty_files, file_to_discard))
			{
				case shared::imgui::editor::file_menu_action::new_file:
					g.open_new_file_popup = true;
					break;

				case shared::imgui::editor::file_menu_action::save_active:
					save_active_file();
					break;

				case shared::imgui::editor::file_menu_action::save_all:
					save_all_files();
					break;

				case shared::imgui::editor::file_menu_action::discard:
					discard_file(file_to_discard);
					break;

				default:
					break;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy active file", nullptr, false, !g.active_file.empty())) {
				copy_to_clipboard(build_file(g.active_file), g.active_file.c_str());
			}

			if (ImGui::MenuItem(ICON_FA_CLIPBOARD "  Copy the spawning markers of all files")) {
				copy_to_clipboard(map_settings_toml::build_map_marker_array(effective_markers()), "all spawning markers");
			}

			ImGui::EndPopup();
		}

		void draw_toolbar()
		{
			const auto& io = ImGui::GetIO();

			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - SCREEN_PADDING, SCREEN_PADDING), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

			if (ImGui::Begin("##marker_editor_toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				shared::imgui::draw_window_blur();
				shared::imgui::editor::toolbar_grip();

				editor_common::pause_buttons(g.step_held, "Pause the game like the pause menu does, cutscenes included. The scene and its markers keep rendering.");
				tool_separator();

				editor_common::camera_buttons(g.rmb_game_input);
				tool_separator();

				// gizmo
				if (tool_button(ICON_FA_ARROWS_ALT "##translate", "Move the selected marker (W)", g.gizmo_op == gizmo::operation::translate)) {
					g.gizmo_op = gizmo::operation::translate;
				}

				if (tool_button(ICON_FA_SYNC_ALT "##rotate", "Rotate the selected marker (R)", g.gizmo_op == gizmo::operation::rotate)) {
					g.gizmo_op = gizmo::operation::rotate;
				}

				if (tool_button(ICON_FA_EXPAND_ARROWS_ALT "##scale", "Scale the selected marker (S)", g.gizmo_op == gizmo::operation::scale)) {
					g.gizmo_op = gizmo::operation::scale;
				}

				if (tool_button(g.local_space ? ICON_FA_CUBE "##space" : ICON_FA_GLOBE "##space",
					g.local_space ? "Gizmo axes: marker. Click for world axes" : "Gizmo axes: world. Click for the marker's own axes (scaling always uses them)"))
				{
					g.local_space = !g.local_space;
				}

				tool_separator();

				// view
				if (tool_button(ICON_FA_LIST "##list", "Marker list", g.show_list)) {
					g.show_list = !g.show_list;
				}

				if (tool_button(ICON_FA_MAP_MARKER_ALT "##points", "Marker points in the world", g.show_points)) {
					g.show_points = !g.show_points;
				}

				if (tool_button(ICON_FA_TAG "##labels", "Number and comment on all points (selected and hovered markers always show theirs)", g.show_labels)) {
					g.show_labels = !g.show_labels;
				}

				tool_separator();

				// history
				ImGui::BeginDisabled(!g.history.can_undo());
				if (tool_button(ICON_FA_UNDO "##undo", "Undo (Ctrl+Z)")) {
					undo();
				}
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 4.0f);

				ImGui::BeginDisabled(!g.history.can_redo());
				if (tool_button(ICON_FA_REDO "##redo", "Redo (Ctrl+Y)")) {
					redo();
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

				if (shared::imgui::editor::file_button("##file", std::format("{}{}", g.active_file.empty() ? "No file" : g.active_file, active_dirty ? "  *" : ""),
					"Active file (new markers go to it), new file, save all, discard and clipboard export. '*' marks unsaved changes."))
				{
					ImGui::OpenPopup("##me_file_menu");
				}

				tool_separator();

				if (tool_button(ICON_FA_TIMES "##close", "Close the marker editor")) {
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

					editor_common::camera_speed_slider(width);

					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(width);
					ImGui::SliderFloat("##view_distance", &g.view_distance, 10.0f, 10000.0f, "Marker distance  %.0f m", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

					ImGui::SetWindowFontScale(1.0f);
					ImGui::PopStyleVar(2);
					TT("Markers farther away from the camera get no point in the world, the list shows all of them");
				}

				// messages go below the toolbar, so it keeps its size
				g.status.draw(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowPos().y + ImGui::GetWindowHeight() + 4.0f));

				draw_file_menu();
				draw_context_menu();
				draw_new_file_popup();
				draw_close_popup();
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		void draw_list_row(const int index)
		{
			const auto& m = marker_list()[index];
			const bool selected = index == g.selected;

			ImGui::PushID(index);

			const ImVec2 cursor = ImGui::GetCursorScreenPos();
			const float height = ImGui::GetTextLineHeight();
			ImGui::Dummy(ImVec2(height, height));
			ImGui::SameLine(0.0f, 6.0f);
			ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cursor.x + height * 0.5f, cursor.y + height * 0.5f), height * 0.32f, state_color(m));

			if (ImGui::Selectable(marker_label(m).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
				g.selected = index;
			}

			if (selected && g.scroll_to_selected)
			{
				ImGui::SetScrollHereY();
				g.scroll_to_selected = false;
			}

			if (ImGui::IsItemHovered())
			{
				g.hovered = index;

				if (world::is_short_right_click())
				{
					g.context = index;
					g.open_context_menu = true;
				}
			}

			std::string info;
			if (m.internal__is_overridden) {
				info = "overridden  ";
			}
			else if (m.internal__is_hidden) {
				info = "hidden  ";
			}
			else if (has_spawn_conditions(m)) {
				info = "conditional  ";
			}

			if (g.view_valid) {
				info += std::format("{:.0f}m", g.view.cam_pos.DistTo(m.origin));
			}

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(info.c_str()).x);
			ImGui::TextDisabled("%s", info.c_str());

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

			if (!ImGui::Begin("Markers###marker_editor_list", &g.show_list, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			shared::imgui::draw_window_blur();

			auto action = action_e::none;
			const bool has_selection = is_valid_index(g.selected);

			// actions
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
				const float spacing = ImGui::GetStyle().ItemSpacing.x;
				const float third = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
				const float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

				ImGui::BeginDisabled(g.active_file.empty());
				if (ImGui::Button(ICON_FA_PLUS "  Add", ImVec2(third, 0.0f))) {
					add_marker();
				}
				ImGui::EndDisabled();
				TT("New marker with the first free number below the player, it goes to the active file");

				ImGui::BeginDisabled(!has_selection);
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_COPY "  Duplicate", ImVec2(third, 0.0f))) {
					action = action_e::duplicate;
				} TT("Copy of the selected marker with the same number in the same file (Ctrl+D)");

				ImGui::SameLine();
				ImGui::Style_ColorButtonPush(ImVec4(0.55f, 0.05f, 0.05f, 1.0f), true);
				if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(third, 0.0f))) {
					action = action_e::remove;
				}
				ImGui::Style_ColorButtonPop();
				TT("Delete the selected marker (Del)");
				ImGui::EndDisabled();

				ImGui::BeginDisabled(!has_selection || !game::is_in_game);
				if (ImGui::Button(ICON_FA_LOCATION_ARROW "  TP to marker", ImVec2(half, 0.0f))) {
					action = action_e::tp_to_marker;
				} TT("Teleports the player to the selected marker");

				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_STREET_VIEW "  TP marker to player", ImVec2(half, 0.0f))) {
					action = action_e::tp_marker_to_player;
				} TT("Moves the selected marker below the player");
				ImGui::EndDisabled();
				ImGui::PopStyleVar();
			}

			if (g.active_file.empty())
			{
				ImGui::TextWrapped("There is no marker file yet.");
				if (ImGui::Button(ICON_FA_FILE "  New file ...", ImVec2(-FLT_MIN, 0.0f))) {
					g.open_new_file_popup = true;
				}
			}

			const float clear_width = ImGui::GetFrameHeight();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clear_width - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::InputTextWithHint("##filter", "Filter (number, comment, file)", g.filter.InputBuf, IM_ARRAYSIZE(g.filter.InputBuf))) {
				g.filter.Build();
			}

			ImGui::SameLine();
			if (ImGui::Button("X##filter_clear", ImVec2(clear_width, clear_width))) {
				g.filter.Clear();
			}

			ImGui::Separator();

			if (ImGui::BeginChild("##rows", ImVec2(0.0f, 0.0f)))
			{
				const auto& list = marker_list();
				static std::vector<int> rows;

				// one group per file, a file's markers in the order they have in the file
				for (const auto& file : settings().marker_files)
				{
					rows.clear();
					for (auto i = 0; i < static_cast<int>(list.size()); i++)
					{
						const auto& m = list[i];
						if (m.file != file) {
							continue;
						}

						if (g.filter.IsActive() && !g.filter.PassFilter(marker_label(m).c_str()) && !g.filter.PassFilter(file.c_str())) {
							continue;
						}

						rows.emplace_back(i);
					}

					if (rows.empty() && g.filter.IsActive()) {
						continue;
					}

					const auto title = std::format("{}{}  ({})###{}", file, g.dirty_files.contains(file) ? "  *" : "", rows.size(), file);
					if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
						continue;
					}

					for (const int index : rows) {
						draw_list_row(index);
					}
				}

				if (list.empty()) {
					ImGui::TextDisabled("No markers");
				}
			}

			ImGui::EndChild();
			ImGui::End();

			run_action(action, g.selected);
		}

		bool begin_properties_table(const char* id)
		{
			if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingFixedFit)) {
				return false;
			}

			ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("reset", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
			return true;
		}

		/// row of a properties table for a value without a default, leaves the value column current
		void plain_row(const char* label)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
		}

		void draw_properties_window()
		{
			if (!is_valid_index(g.selected)) {
				return;
			}

			auto& m = marker_list()[g.selected];
			auto action = action_e::none;
			std::string target_file;
			bool changed = false;

			bool keep_open = true;
			ImGui::SetNextWindowPos(ImVec2(SCREEN_PADDING, SCREEN_PADDING), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Always);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.9f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

			const auto title = std::format("Marker #{}###me_properties", m.index);
			if (ImGui::Begin(title.c_str(), &keep_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				// the window follows its content, so it is pushed back whenever a part of it would leave the screen
				shared::imgui::editor::keep_window_on_screen();

				shared::imgui::draw_window_blur();

				ImGui::TextDisabled("%s", m.file.c_str());

				if (m.internal__is_overridden)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
					ImGui::TextWrapped("Does not spawn: %s has a higher priority and uses the number.", owner_file(m.index).c_str());
					ImGui::PopStyleColor();
				}
				else if (m.internal__is_hidden) {
					ImGui::TextDisabled("Hidden right now by its spawn conditions or cull distance");
				}

				ImGui::Separator();

				if (begin_properties_table("##properties"))
				{
					plain_row("Comment");
					changed |= ImGui::InputTextWithHint("##comment", "comment above the entry", &m.comment);

					plain_row("Number");
					int number = static_cast<int>(m.index);
					if (ImGui::DragInt("##number", &number, 0.1f, 0, 50000, "%d", ImGuiSliderFlags_AlwaysClamp))
					{
						auto value = static_cast<std::uint32_t>(std::max(number, 0));
						if (is_reserved_number(value)) {
							value = value > m.index ? RESERVED_NUMBERS_END : RESERVED_NUMBERS_BEGIN - 1u;
						}

						if (value != m.index)
						{
							m.index = value;
							changed = true;
						}
					}
					TT("Several markers of a file can share a number. 9000-9099 are reserved");

					plain_row("File");
					if (ImGui::BeginCombo("##file", m.file.c_str()))
					{
						for (const auto& file : settings().marker_files)
						{
							if (ImGui::Selectable(file.c_str(), file == m.file) && file != m.file)
							{
								target_file = file;
								action = action_e::move_to_file;
							}
						}

						ImGui::EndCombo();
					}

					plain_row("Origin");
					changed |= ImGui::DragFloat3("##origin", &m.origin.x, 0.01f, 0.0f, 0.0f, "%.2f");

					bool use_rotation = m.rotation.x != 0.0f || m.rotation.y != 0.0f || m.rotation.z != 0.0f;
					if (property_row("Rotation", use_rotation, false, "Back to no rotation", [&m]
						{
							float degrees[3] = { RAD2DEG(m.rotation.x), RAD2DEG(m.rotation.y), RAD2DEG(m.rotation.z) };
							if (ImGui::DragFloat3("##v", degrees, 0.1f, -360.0f, 360.0f, "%.2f deg", ImGuiSliderFlags_AlwaysClamp))
							{
								m.rotation = Vector(DEG2RAD(degrees[0]), DEG2RAD(degrees[1]), DEG2RAD(degrees[2]));
								return true;
							}
							return false;
						}))
					{
						if (!use_rotation) {
							m.rotation = Vector(0.0f, 0.0f, 0.0f);
						}

						changed = true;
					}

					bool use_scale = m.scale.x != 1.0f || m.scale.y != 1.0f || m.scale.z != 1.0f;
					if (property_row("Scale", use_scale, false, "Back to a scale of 1", [&m]
						{
							return ImGui::DragFloat3("##v", &m.scale.x, 0.01f, 0.0f, 0.0f, "%.2f");
						}))
					{
						if (!use_scale) {
							m.scale = Vector(1.0f, 1.0f, 1.0f);
						}

						changed = true;
					}

					bool use_cull_distance = m.cull_distance > 0.0f;
					if (property_row("Cull distance", use_cull_distance, false, "Back to no culling", [&m]
						{
							return ImGui::DragFloat("##v", &m.cull_distance, 0.05f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
						}))
					{
						if (!use_cull_distance) {
							m.cull_distance = 0.0f;
						}

						changed = true;
					}

					ImGui::EndTable();
				}

				ImGui::SeparatorText("Spawn conditions");

				if (begin_properties_table("##conditions"))
				{
					const bool on_weather = m.weather_type != game::WEATHER_NONE;

					bool use_weather = on_weather;
					if (property_row("Weather", use_weather, false, "Back to any weather", [&m]
						{
							int weather = static_cast<int>(m.weather_type);
							if (ImGui::Combo("##v", &weather, game::eWeatherTypeStr, IM_ARRAYSIZE(game::eWeatherTypeStr)))
							{
								m.weather_type = static_cast<game::eWeatherType>(weather);
								return true;
							}
							return false;
						}))
					{
						if (!use_weather) {
							m.weather_type = game::WEATHER_NONE;
						}

						changed = true;
					}

					bool use_transition = on_weather && m.weather_transition_value != DEFAULT_WEATHER_TRANSITION;
					if (property_row("Weather transition", use_transition, !on_weather, "Back to the default transition value", [&m]
						{
							return ImGui::SliderFloat("##v", &m.weather_transition_value, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
						}))
					{
						if (!use_transition) {
							m.weather_transition_value = DEFAULT_WEATHER_TRANSITION;
						}

						changed = true;
					}

					const bool between_hours = m.from_hour >= 0 && m.to_hour >= 0;

					bool use_hours = between_hours;
					if (property_row("Between hours", use_hours, false, "Back to any hour", [&m, between_hours]
						{
							int hours[2] = { between_hours ? m.from_hour : 0, between_hours ? m.to_hour : 24 };
							if (ImGui::SliderInt2("##v", hours, 0, 24, "%d h", ImGuiSliderFlags_AlwaysClamp))
							{
								m.from_hour = hours[0];
								m.to_hour = hours[1];
								return true;
							}
							return false;
						}))
					{
						if (!use_hours)
						{
							m.from_hour = -1;
							m.to_hour = -1;
						}

						changed = true;
					}

					ImGui::EndTable();
				}

				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				ImGui::TextWrapped("Weather: the marker shows once the transition to the weather passed the value. Hours: from - to, a range like 20 - 6 runs over midnight.");
				ImGui::PopStyleColor();
			}

			ImGui::End();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			if (changed) {
				commit(m);
			}

			if (!keep_open) {
				g.selected = NO_MARKER;
			}

			run_action(action, g.selected, target_file);
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
					undo();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
					redo();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
					save_active_file();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_D, false)) {
					run_action(action_e::duplicate, g.selected);
				}

				return;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
				g.gizmo_op = gizmo::operation::translate;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
				g.gizmo_op = gizmo::operation::rotate;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
				g.gizmo_op = gizmo::operation::scale;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
				run_action(action_e::remove, g.selected);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
				g.selected = NO_MARKER;
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

		if (shared::imgui::editor_host::any_open()) {
			return;
		}

		g.open = true;
		shared::imgui::editor_host::sync_open_state();
		shared::globals::imgui_menu_open = false;
		ensure_active_file();
		free_camera::set_active(true);
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
		g.history.clear();
		g.saved_valid = false;
		g.active_file.clear();
		g.selected = NO_MARKER;
		reset_hover();
	}

	void draw()
	{
		if (!g.open) {
			return;
		}

		g.view_valid = scene_view::get(g.view);
		ensure_active_file();
		gizmo::setup_style();
		gizmo::begin_frame();

		if (!is_valid_index(g.selected)) {
			g.selected = NO_MARKER;
		}

		if (!g.saved_valid && g.dirty_files.empty())
		{
			g.saved = take_snapshot();
			g.saved_valid = true;
		}

		g.history.begin_frame(ImGui::IsAnyItemActive() || gizmo::is_using(), take_snapshot);

		g.hovered_drawn = is_valid_index(g.hovered) ? g.hovered : NO_MARKER;
		g.hovered = NO_MARKER;

		handle_shortcuts();
		draw_gizmo();
		draw_world_overlay();
		draw_toolbar();
		draw_list_window();
		draw_properties_window();
	}

	void register_editor() {
		shared::imgui::editor_host::register_editor({ "Marker Editor", is_open, draw, allows_game_input_on_rmb });
	}
}
