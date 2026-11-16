#include "std_include.hpp"
#include "imgui_mesh_inspector.hpp"

#include <algorithm>

#include "free_camera.hpp"
#include "imgui.hpp"
#include "imgui_editor_common.hpp"
#include "map_settings.hpp"
#include "map_settings_toml.hpp"
#include "scene_view.hpp"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "shared/imgui/editor_host.hpp"
#include "shared/imgui/editor_widgets.hpp"
#include "shared/imgui/imgui_helper.hpp"
#include "shared/imgui/font_defines.hpp"
#include "shared/imgui/world_draw.hpp"

#define TT(TXT) ImGui::SetItemTooltipBlur((TXT));

namespace gta4::mesh_inspector
{
	namespace
	{
		using inspector_s = imgui::anticull_inspector_s;

		namespace world = shared::imgui::world;
		using shared::imgui::editor::tool_button;
		using shared::imgui::editor::tool_separator;
		using shared::imgui::editor::COL_BUTTON_UNSAVED;

		constexpr float SCREEN_PADDING = 16.0f; // default distance of the windows to the screen edges and to each other
		constexpr float RIGHT_PAD = 4.0f;
		constexpr ImVec2 WINDOW_PADDING = ImVec2(10.0f, 8.0f);
		constexpr ImVec4 COL_PANEL = ImVec4(0.0f, 0.0f, 0.0f, 0.22f);
		constexpr int INDEX_COLUMNS = 5;
		constexpr float PICK_SAME_SPOT_DIST_SQR = 36.0f;

		const ImVec4 COL_HIDE = ImVec4(0.16f, 0.40f, 0.55f, 1.0f);   // the selection box blue, darkened
		const ImVec4 COL_HIDDEN = ImVec4(0.45f, 0.20f, 0.55f, 1.0f); // the hidden state purple, darkened

		struct label_s
		{
			const inspector_s::instance_s* inst;
			ImVec2 pos;
			int priority;
		};

		struct rect_s
		{
			ImVec2 min;
			ImVec2 max;
		};

		struct state_s
		{
			bool open = false;
			bool show_meshes = true;
			bool show_categories = true;
			bool rmb_game_input = false;
			bool step_held = false;
			bool open_context_menu = false;

			bool anticull_modified = false;
			bool lod_fade_modified = false;

			world::view_s view;
			bool view_valid = false;

			ImGuiTextFilter filter;			// mesh table, overlay and picking
			ImGuiTextFilter index_filter;	// indices of the categories
			std::vector<int> rows;			// indices into the instance list that the table shows

			bool trace_open = false;
			bool tree_open = false;
			const void* tree_last_shown = nullptr;
			int add_category = 0;

			ImVec2 pick_last_click = ImVec2(-1000.0f, -1000.0f);
			std::vector<const void*> pick_last_hits;
			int pick_cycle = 0;

			std::vector<label_s> labels;
			std::vector<rect_s> placed_labels;

			std::string lod_fade_add_buffer;

			// imgui ids of the categories, parallel to the category list
			std::vector<std::uint32_t> category_ids;
			std::uint32_t next_category_id = 0u;

			shared::imgui::editor::status_toast_s status;
		};

		state_s g;

#pragma region MESHES

		inspector_s& inspector() {
			return imgui::get()->m_anticull_inspector;
		}

		auto& anticull_categories() {
			return map_settings::get_map_settings().anticull_meshes;
		}

		auto& lod_fade_excluded() {
			return map_settings::get_map_settings().lod_fade_excluded;
		}

		/// first category that forces the model index, -1 if none
		int find_category(const int model_index)
		{
			const auto& ac = anticull_categories();
			for (auto i = 0u; i < ac.size(); ++i)
			{
				if (ac[i].indices.contains(model_index)) {
					return static_cast<int>(i);
				}
			}

			return -1;
		}

		bool passes_filter(const inspector_s& ins, const inspector_s::instance_s& inst)
		{
			if (inst.is_lod ? !ins.show_lod : !ins.show_hd) {
				return false;
			}

			if (ins.only_forced && find_category(inst.model_index) < 0) {
				return false;
			}

			if (g.filter.IsActive())
			{
				const bool name_passes = inst.model_name && g.filter.PassFilter(inst.model_name);
				if (!name_passes && !g.filter.PassFilter(shared::utils::va("%d", inst.model_index))) {
					return false;
				}
			}

			return true;
		}

		const inspector_s::instance_s* find_instance(const inspector_s& ins, const void* entity)
		{
			if (entity)
			{
				for (const auto& inst : ins.instances)
				{
					if (inst.entity == entity) {
						return &inst;
					}
				}
			}

			return nullptr;
		}

		bool is_hidden(const inspector_s& ins, const void* entity) {
			return std::ranges::any_of(ins.hidden, [entity](const inspector_s::hidden_s& h) { return h.entity == entity; });
		}

		void set_hidden(inspector_s& ins, const void* entity, const std::int16_t model_index, const bool hide)
		{
			std::erase_if(ins.hidden, [entity](const inspector_s::hidden_s& h) { return h.entity == entity; });

			if (hide && ins.hidden.size() < inspector_s::MAX_HIDDEN) {
				ins.hidden.push_back({ entity, model_index });
			}
		}

		/// also hides the lod parents, so nothing takes the place of the mesh
		void hide_with_parents(inspector_s& ins, const inspector_s::instance_s& inst)
		{
			set_hidden(ins, inst.entity, inst.model_index, true);

			const void* parent = inst.lod_entity;
			std::int16_t parent_model_index = inst.lod_model_index;

			while (parent && !is_hidden(ins, parent))
			{
				set_hidden(ins, parent, parent_model_index, true);

				const auto p = find_instance(ins, parent);
				parent = p ? p->lod_entity : nullptr;
				parent_model_index = p ? p->lod_model_index : static_cast<std::int16_t>(-1);
			}
		}

		const char* frustum_reason_name(const inspector_s::frustum_reason reason)
		{
			using r = inspector_s::frustum_reason;
			switch (reason)
			{
				case r::stock:			return "stock frustum test";
				case r::never_cull:		return "debug: never cull statics";
				case r::near_dist:		return "near distance";
				case r::medium_radius:	return "medium cascade radius";
				case r::far_radius:		return "far cascade radius";
				case r::far_height:		return "far cascade height";
				case r::anticull_mesh:	return "anti cull mesh";
				case r::inspector:		return "hidden or lod view of the inspector";
				default:				return "-";
			}
		}

		std::pair<const char*, ImVec4> instance_state(const inspector_s& ins, const inspector_s::instance_s& inst)
		{
			if (is_hidden(ins, inst.entity)) {
				return { "hidden", ImVec4(0.85f, 0.4f, 0.95f, 1.0f) };
			}

			if (!inst.visited) {
				return { "not visited", ImVec4(0.55f, 0.55f, 0.55f, 1.0f) };
			}

			if (inst.past_lod_distance) {
				return { "past lod dist", ImVec4(0.9f, 0.6f, 0.2f, 1.0f) };
			}

			if (inst.visible) {
				return { "visible", ImVec4(0.4f, 0.85f, 0.3f, 1.0f) };
			}

			return { "culled", ImVec4(0.9f, 0.35f, 0.3f, 1.0f) };
		}

		/// takes the newest snapshot of the game thread, names the entity the culling hooks trace and hands them the hidden meshes
		void sync(inspector_s& ins)
		{
			{
				std::lock_guard lock(ins.mutex);
				if (ins.published_generation != ins.instances_generation)
				{
					ins.instances.swap(ins.published);
					ins.instances_generation = ins.published_generation;
				}

				ins.trace_copy = ins.trace;
			}

			ins.traced_entity.store(ins.selected ? ins.selected : ins.hovered, std::memory_order_relaxed);

			const auto hidden_count = ins.enabled ? static_cast<std::uint32_t>(std::min<std::size_t>(ins.hidden.size(), inspector_s::MAX_HIDDEN)) : 0u;
			for (auto i = 0u; i < hidden_count; ++i)
			{
				ins.hidden_slots[i].entity.store(ins.hidden[i].entity, std::memory_order_relaxed);
				ins.hidden_slots[i].model_index.store(ins.hidden[i].model_index, std::memory_order_relaxed);
			}

			ins.hidden_count.store(hidden_count, std::memory_order_release);
			ins.active_lod_view.store(ins.enabled ? ins.lod_view : inspector_s::lod_view_mode::off, std::memory_order_relaxed);
		}

#pragma endregion

#pragma region EXPORT

		void set_status(std::string text) {
			g.status.set(std::move(text));
		}

		/// adds the index typed into 'buffer' to the set and clears the buffer, true if the set grew
		bool add_typed_index(std::unordered_set<int>& indices, std::string& buffer, const char* error_message)
		{
			bool added = false;

			try {
				added = indices.emplace(std::stoi(buffer)).second;
			}
			catch (const std::exception&) {
				shared::common::log("ImGui", error_message, shared::common::LOG_TYPE::LOG_TYPE_ERROR);
			}

			buffer.clear();
			return added;
		}

#pragma endregion

#pragma region SCENE

		void draw_box(const inspector_s::instance_s& inst, const ImU32 color, const float thickness) {
			world::draw_box(g.view, inst.world, inst.local_mins, inst.local_maxs, color, thickness);
		}

		/// ray vs the oriented model box, inside = ray starts within the box
		bool ray_hit(const inspector_s::instance_s& inst, const D3DXVECTOR3& origin, const D3DXVECTOR3& dir, float& out_t, bool& out_inside)
		{
			D3DXMATRIX inv;
			if (!D3DXMatrixInverse(&inv, nullptr, &inst.world)) {
				return false;
			}

			D3DXVECTOR3 o, d;
			D3DXVec3TransformCoord(&o, &origin, &inv);
			D3DXVec3TransformNormal(&d, &dir, &inv);

			const float ro[3] = { o.x, o.y, o.z };
			const float rd[3] = { d.x, d.y, d.z };
			const float mins[3] = { inst.local_mins.x, inst.local_mins.y, inst.local_mins.z };
			const float maxs[3] = { inst.local_maxs.x, inst.local_maxs.y, inst.local_maxs.z };

			float t_min = 0.0f;
			float t_max = FLT_MAX;
			out_inside = true;

			for (auto a = 0; a < 3; ++a)
			{
				if (ro[a] < mins[a] || ro[a] > maxs[a]) {
					out_inside = false;
				}

				if (fabsf(rd[a]) < 1e-6f)
				{
					if (ro[a] < mins[a] || ro[a] > maxs[a]) {
						return false;
					}
					continue;
				}

				float t1 = (mins[a] - ro[a]) / rd[a];
				float t2 = (maxs[a] - ro[a]) / rd[a];
				if (t1 > t2) {
					std::swap(t1, t2);
				}

				t_min = std::max(t_min, t1);
				t_max = std::min(t_max, t2);
				if (t_min > t_max) {
					return false;
				}
			}

			out_t = t_min;
			return true;
		}

		/// meshes under the mouse, nearest first. Boxes the camera is inside come last
		std::vector<const void*> hits_under_mouse(const inspector_s& ins)
		{
			const auto& v = g.view;
			const auto mouse = ImGui::GetIO().MousePos;
			const D3DXVECTOR3 screen_near(mouse.x, mouse.y, 0.0f);
			const D3DXVECTOR3 screen_mid(mouse.x, mouse.y, 0.5f);

			D3DXVECTOR3 origin, mid, dir;
			D3DXVec3Unproject(&origin, &screen_near, &v.viewport, &v.proj, &v.view, nullptr);
			D3DXVec3Unproject(&mid, &screen_mid, &v.viewport, &v.proj, &v.view, nullptr);
			dir = mid - origin;
			D3DXVec3Normalize(&dir, &dir);

			struct hit_s
			{
				const void* entity;
				bool inside;
				float t;
			};

			std::vector<hit_s> hits;
			for (const auto& inst : ins.instances)
			{
				if (!passes_filter(ins, inst) || (ins.pick_visible_only && (!inst.visible || inst.past_lod_distance))) {
					continue;
				}

				float t = 0.0f;
				bool inside = false;
				if (ray_hit(inst, origin, dir, t, inside)) {
					hits.push_back({ inst.entity, inside, t });
				}
			}

			std::ranges::sort(hits, [](const hit_s& a, const hit_s& b) {
				return a.inside != b.inside ? !a.inside : a.t < b.t;
			});

			std::vector<const void*> hit_entities;
			hit_entities.reserve(hits.size());
			for (const auto& h : hits) {
				hit_entities.emplace_back(h.entity);
			}

			return hit_entities;
		}

		/// selects the mesh under the mouse, clicking the same spot again cycles through all hits
		void pick(inspector_s& ins)
		{
			const auto mouse = ImGui::GetIO().MousePos;
			auto hit_entities = hits_under_mouse(ins);

			const float dx = mouse.x - g.pick_last_click.x;
			const float dy = mouse.y - g.pick_last_click.y;
			const bool same_spot = dx * dx + dy * dy < PICK_SAME_SPOT_DIST_SQR && hit_entities == g.pick_last_hits;

			g.pick_cycle = same_spot && !hit_entities.empty() ? (g.pick_cycle + 1) % static_cast<int>(hit_entities.size()) : 0;
			ins.selected = hit_entities.empty() ? nullptr : hit_entities[g.pick_cycle];
			ins.pick_hit = g.pick_cycle;
			ins.pick_hit_count = static_cast<int>(hit_entities.size());

			g.pick_last_click = mouse;
			g.pick_last_hits = std::move(hit_entities);
		}

		void draw_world_overlay(inspector_s& ins)
		{
			if (!g.view_valid) {
				return;
			}

			const auto& v = g.view;

			// boxes
			for (const auto& inst : ins.instances)
			{
				if (inst.entity == ins.selected) {
					draw_box(inst, IM_COL32(60, 200, 255, 255), 2.5f);
				}
				else if (inst.entity == ins.pinned) {
					draw_box(inst, IM_COL32(255, 170, 60, 255), 2.0f);
				}
				else if (inst.entity == ins.hovered) {
					draw_box(inst, IM_COL32(255, 220, 60, 255), 2.0f);
				}
				else if (!ins.hidden.empty() && is_hidden(ins, inst.entity)) {
					draw_box(inst, IM_COL32(215, 100, 240, 255), 1.5f);
				}
				else if (inst.model_index == ins.highlight_model_index) {
					draw_box(inst, IM_COL32(120, 255, 120, 255), 1.5f);
				}
				else if (ins.boxes_on_forced && passes_filter(ins, inst) && find_category(inst.model_index) >= 0) {
					draw_box(inst, IM_COL32(80, 200, 80, 140), 1.0f);
				}
			}

			// labels: selected and hovered first, then the largest meshes, skipped where they would overlap
			if (ins.show_labels)
			{
				auto& labels = g.labels;
				auto& placed = g.placed_labels;
				labels.clear();
				placed.clear();

				for (const auto& inst : ins.instances)
				{
					ImVec2 pos;
					if (passes_filter(ins, inst) && world::project(v, inst.center, pos)) {
						labels.push_back({ &inst, pos, inst.entity == ins.selected ? 2 : inst.entity == ins.hovered ? 1 : 0 });
					}
				}

				std::ranges::sort(labels, [](const label_s& a, const label_s& b) {
					return a.priority != b.priority ? a.priority > b.priority : a.inst->height > b.inst->height;
				});

				const auto dl = ImGui::GetBackgroundDrawList();
				int count = 0;

				for (const auto& l : labels)
				{
					if (!l.priority && count >= ins.max_labels) {
						break;
					}

					const char* text = l.inst->model_name
						? shared::utils::va("%d\n%s", l.inst->model_index, l.inst->model_name)
						: shared::utils::va("%d", l.inst->model_index);

					const ImVec2 half = ImGui::CalcTextSize(text) * 0.5f + ImVec2(4.0f, 2.0f);
					const rect_s rect = { l.pos - half, l.pos + half };

					const bool overlaps = std::ranges::any_of(placed, [&rect](const rect_s& p) {
						return rect.min.x < p.max.x && rect.max.x > p.min.x && rect.min.y < p.max.y && rect.max.y > p.min.y;
					});

					if (!l.priority && overlaps) {
						continue;
					}

					placed.push_back(rect);
					++count;

					ImU32 color = l.inst->visible ? IM_COL32(235, 235, 235, 255) : IM_COL32(150, 150, 150, 255);
					if (l.priority == 2) {
						color = IM_COL32(60, 200, 255, 255);
					}
					else if (l.priority == 1) {
						color = IM_COL32(255, 220, 60, 255);
					}
					else if (!ins.hidden.empty() && is_hidden(ins, l.inst->entity)) {
						color = IM_COL32(215, 100, 240, 255);
					}
					else if (find_category(l.inst->model_index) >= 0) {
						color = IM_COL32(110, 230, 90, 255);
					}

					dl->AddRectFilled(rect.min, rect.max, IM_COL32(0, 0, 0, 150), 3.0f);
					dl->AddText(rect.min + ImVec2(4.0f, 2.0f), color, text);
				}
			}

			const auto& io = ImGui::GetIO();
			if (io.WantCaptureMouse || io.MouseDown[ImGuiMouseButton_Right]) {
				return;
			}

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				pick(ins);
			}

			// a held right button can belong to the camera or the game, so a short click opens the menu on release
			if (world::is_short_right_click())
			{
				const auto hits = hits_under_mouse(ins);
				const auto selected = std::ranges::find(hits, ins.selected);
				const void* target = selected != hits.end() ? *selected : hits.empty() ? nullptr : hits.front();

				if (const auto inst = find_instance(ins, target))
				{
					ins.context = *inst;
					g.open_context_menu = true;
				}
			}
		}

#pragma endregion

#pragma region MESH_DETAILS

		/// right click menu of a table row or a mesh in the world
		void draw_context_menu(inspector_s& ins)
		{
			if (g.open_context_menu)
			{
				ImGui::OpenPopup("##mi_context");
				g.open_context_menu = false;
			}

			if (!ImGui::BeginPopup("##mi_context")) {
				return;
			}

			const auto& inst = ins.context;
			auto& ac = anticull_categories();
			ins.hovered = inst.entity;

			shared::imgui::draw_window_blur();

			ImGui::PushFont(shared::imgui::font::BOLD);
			ImGui::Text("%d  %s", inst.model_index, inst.model_name ? inst.model_name : "");
			ImGui::PopFont();
			ImGui::Separator();

			if (ImGui::BeginMenu("Anti Cull Categories"))
			{
				shared::imgui::draw_window_blur();
				if (ac.empty()) {
					ImGui::TextDisabled("No categories");
				}

				for (auto i = 0; i < static_cast<int>(ac.size()); ++i)
				{
					auto& indices = ac[i].indices;
					const bool contained = indices.contains(inst.model_index);

					ImGui::PushID(i);
					if (ImGui::MenuItem(ac[i].comment.c_str(), nullptr, contained))
					{
						if (contained) {
							indices.erase(inst.model_index);
						}
						else {
							indices.emplace(inst.model_index);
						}

						g.anticull_modified = true;
					}
					ImGui::PopID();
				}

				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Remove from all Categories", nullptr, false, find_category(inst.model_index) >= 0))
			{
				for (auto& cat : ac) {
					cat.indices.erase(inst.model_index);
				}

				g.anticull_modified = true;
			}

			auto& excluded = lod_fade_excluded();
			const bool fade_excluded = excluded.contains(inst.model_index);
			if (ImGui::MenuItem("Exclude from LOD Fade", nullptr, fade_excluded))
			{
				if (fade_excluded) {
					excluded.erase(inst.model_index);
				} else {
					excluded.emplace(inst.model_index);
				}

				g.lod_fade_modified = true;
			}

			ImGui::Separator();

			const bool selected = ins.selected == inst.entity;
			if (ImGui::MenuItem("Select", nullptr, selected)) {
				ins.selected = selected ? nullptr : inst.entity;
			}

			const bool highlighted = ins.highlight_model_index == inst.model_index;
			if (ImGui::MenuItem("Highlight all Instances", nullptr, highlighted)) {
				ins.highlight_model_index = highlighted ? -1 : inst.model_index;
			}

			const bool hidden = is_hidden(ins, inst.entity);
			const bool can_hide = hidden || ins.hidden.size() < inspector_s::MAX_HIDDEN;
			if (ImGui::MenuItem("Hide", nullptr, hidden, can_hide)) {
				set_hidden(ins, inst.entity, inst.model_index, !hidden);
			}

			if (ImGui::MenuItem("Hide with Lod Parents", nullptr, false, inst.lod_entity && can_hide)) {
				hide_with_parents(ins, inst);
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Copy Index")) {
				ImGui::SetClipboardText(shared::utils::va("%d", inst.model_index));
			}

			ImGui::EndPopup();
		}

		/// the info lines of a mesh have placeholders, so the panel keeps its layout when nothing is selected
		void name_line(const inspector_s& ins, const inspector_s::instance_s* inst, const bool show_hits)
		{
			ImGui::PushFont(shared::imgui::font::BOLD);
			if (inst) {
				ImGui::Text("%d  %s", inst->model_index, inst->model_name ? inst->model_name : "");
			}
			else {
				ImGui::TextUnformatted("<no mesh selected>");
			}
			ImGui::PopFont();

			if (inst && show_hits && ins.pick_hit_count > 1)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(hit %d / %d, click again to cycle)", ins.pick_hit + 1, ins.pick_hit_count);
			}
		}

		/// dimmed label and its value. Continues the current line unless 'spacing' is negative
		void labeled_value(const char* label, const char* value, const float spacing = 14.0f)
		{
			if (spacing >= 0.0f) {
				ImGui::SameLine(0.0f, spacing);
			}

			ImGui::TextDisabled("%s", label);
			ImGui::SameLine(0.0f, 5.0f);
			ImGui::TextUnformatted(value);
		}

		void stats_line(const inspector_s::instance_s* inst)
		{
			ImGui::TextUnformatted(inst ? (inst->is_lod ? "LOD" : "HD") : "-");
			labeled_value("Height", inst ? shared::utils::va("%.1f", inst->height) : "-");
			labeled_value("Radius", inst ? shared::utils::va("%.1f", inst->radius) : "-");
			labeled_value("Dist", inst ? shared::utils::va("%.1f", inst->distance) : "-");
			labeled_value("LodDist", inst ? shared::utils::va("%.1f", inst->lod_distance) : "-");
			labeled_value("Alpha", inst ? shared::utils::va("%d", inst->alpha) : "-");
		}

		void forced_line(const inspector_s& ins, const inspector_s::instance_s* inst)
		{
			const auto& ac = anticull_categories();
			const int category = inst ? find_category(inst->model_index) : -1;

			labeled_value("Forced by", category >= 0 ? ac[category].comment.c_str() : "-", -1.0f);
			ImGui::SameLine(0, 14);
			ImGui::TextDisabled("Last Traversal");
			ImGui::SameLine(0, 5);

			if (inst)
			{
				const auto [state, state_color] = instance_state(ins, *inst);
				ImGui::TextColored(state_color, "%s", state);

				if (!inst->model_loaded)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(model not loaded)");
				}
			} else {
				ImGui::TextUnformatted("-");
			}
		}

		// lod tree of the shown mesh: its lod parent on top, the siblings below it and its own children one level deeper
		namespace lod_tree
		{
			constexpr float INDENT = 18.0f;
			constexpr float ROWS = 8.0f;

			struct ctx_s
			{
				inspector_s& ins;
				const inspector_s::instance_s* shown;
				float x0;    // screen x of depth 0
				float right; // local x where the state text ends
			};

			/// returns the vertical center of the row in screen space
			float row(const ctx_s& c, const inspector_s::instance_s* inst, const int model_index, const int depth)
			{
				const bool shown = inst && inst == c.shown;
				const bool is_selected = inst && inst->entity == c.ins.selected;

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + static_cast<float>(depth) * INDENT);
				ImGui::PushID(inst ? inst->entity : nullptr);
				if (shown) {
					ImGui::PushFont(shared::imgui::font::BOLD);
				}
				ImGui::BeginDisabled(!inst);
				if (ImGui::Selectable(inst ? shared::utils::va("%d  %s", model_index, inst->model_name ? inst->model_name : "") : shared::utils::va("%d  <not listed>", model_index), is_selected)) {
					c.ins.selected = is_selected ? nullptr : inst->entity;
				}
				ImGui::EndDisabled();
				if (shown) {
					ImGui::PopFont();
				}
				ImGui::PopID();

				const auto rect_min = ImGui::GetItemRectMin();
				const auto rect_max = ImGui::GetItemRectMax();

				if (inst)
				{
					if (ImGui::IsItemHovered()) {
						c.ins.hovered = inst->entity;
					}

					const auto [state, state_color] = instance_state(c.ins, *inst);
					const auto state_str = shared::utils::va("%s  %s", inst->is_lod ? "LOD" : "HD", state);
					ImGui::SameLine();
					ImGui::SetCursorPosX(c.right - ImGui::CalcTextSize(state_str).x);
					ImGui::TextColored(state_color, "%s", state_str);
				}

				// keep the shown mesh in view when it changes
				if (shown && g.tree_last_shown != inst->entity)
				{
					ImGui::SetScrollHereY(0.5f);
					g.tree_last_shown = inst->entity;
				}

				return (rect_min.y + rect_max.y) * 0.5f;
			}

			/// the listed children of an entity under its row, connected with tree lines. The shown mesh unfolds its own children
			void children(const ctx_s& c, const void* parent, const int child_count, const int depth)
			{
				const auto dl = ImGui::GetWindowDrawList();
				const auto line_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
				const float x_line = c.x0 + static_cast<float>(depth - 1) * INDENT + 6.0f;
				const float y_top = ImGui::GetItemRectMax().y;
				float y_last = 0.0f;
				int listed = 0;

				for (const auto& inst : c.ins.instances)
				{
					if (inst.lod_entity != parent) {
						continue;
					}

					listed++;
					y_last = row(c, &inst, inst.model_index, depth);
					dl->AddLine(ImVec2(x_line, y_last), ImVec2(c.x0 + static_cast<float>(depth) * INDENT - 3.0f, y_last), line_color);

					if (&inst == c.shown && inst.lod_children) {
						children(c, inst.entity, inst.lod_children, depth + 1);
					}
				}

				if (listed) {
					dl->AddLine(ImVec2(x_line, y_top), ImVec2(x_line, y_last), line_color);
				}

				if (child_count > listed)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + static_cast<float>(depth) * INDENT);
					ImGui::TextDisabled("%d more outside the inspector radius or below Min Radius / Min Height", child_count - listed);
				}
			}

			/// the selected mesh gets its own info lines below the tree while another one is pinned
			void draw(inspector_s& ins, const inspector_s::instance_s& shown, const inspector_s::instance_s* other, const float height)
			{
				ImGui::Indent();
				ImGui::BeginChild("##mi_lod_tree", ImVec2(-RIGHT_PAD, height), ImGuiChildFlags_FrameStyle);

				const ctx_s c = { ins, &shown, ImGui::GetCursorScreenPos().x, ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x };
				const auto parent = find_instance(ins, shown.lod_entity);

				if (shown.lod_entity)
				{
					row(c, parent, shown.lod_model_index, 0);
					children(c, shown.lod_entity, parent ? parent->lod_children : 0, 1);
				}
				else
				{
					row(c, &shown, shown.model_index, 0);
					children(c, shown.entity, shown.lod_children, 1);
				}

				ImGui::EndChild();

				if (other)
				{
					name_line(ins, other, true);
					stats_line(other);
					forced_line(ins, other);
				}
				ImGui::Unindent();
			}
		}

		void draw_details(inspector_s& ins)
		{
			const auto& style = ImGui::GetStyle();
			auto& ac = anticull_categories();

			// the pinned mesh stays in the panel while others of its lod tree get selected, the buttons and the trace follow the selection
			const auto sel = find_instance(ins, ins.selected);
			const auto pinned = find_instance(ins, ins.pinned);
			const auto inst = pinned ? pinned : sel;
			const auto other = pinned && sel && sel != pinned ? sel : nullptr;
			const int sel_category = sel ? find_category(sel->model_index) : -1;

			// fixed height and placeholders, so that everything below keeps its position when the selection changes
			const float line = ImGui::GetTextLineHeightWithSpacing();
			const float frame = ImGui::GetFrameHeightWithSpacing();
			const float tree_height = line * lod_tree::ROWS + style.FramePadding.y * 2.0f;

			float height = line * 5.0f + frame * 2.0f + 16.0f;

			if (g.trace_open) {
				height += line * 4.0f;
			}

			if (g.tree_open) {
				height += tree_height + style.ItemSpacing.y + (other ? line * 3.0f : 0.0f);
			}

			ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_PANEL);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
			ImGui::BeginChild("##mi_details", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
			const float grid_x = ImGui::GetCursorPosX();
			const float avail = ImGui::GetContentRegionAvail().x - RIGHT_PAD;
			const float button_width = (avail - style.ItemSpacing.x * 4.0f) / 5.0f;
			const float column = button_width + style.ItemSpacing.x;
			ImGui::BeginDisabled(!inst);

			name_line(ins, inst, !other);

			ImGui::SameLine();
			ImGui::SetCursorPosX(grid_x + avail - ImGui::CalcTextSize(ICON_FA_THUMBTACK).x - style.FramePadding.x * 2.0f);

			if (pinned) {
				ImGui::Style_ColorButtonPush(COL_HIDE, true);
			}

			if (ImGui::SmallButton(ICON_FA_THUMBTACK "##pin")) {
				ins.pinned = pinned ? nullptr : inst->entity;
			}

			if (pinned) {
				ImGui::Style_ColorButtonPop();
			} TT("Pins the mesh to this panel. Selecting another mesh of its lod tree then shows that one below the tree while the pinned mesh stays up.");

			stats_line(inst);
			const auto parent = inst ? find_instance(ins, inst->lod_entity) : nullptr;

			if (inst && inst->lod_model_index >= 0) {
				labeled_value("Lod Parent", shared::utils::va("%d %s", inst->lod_model_index, parent && parent->model_name ? parent->model_name : ""), -1.0f);
			} else {
				labeled_value("Lod Parent", "-", -1.0f);
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!parent);
			if (ImGui::SmallButton("Select##lod_parent")) {
				ins.selected = parent->entity;
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(!inst || (!inst->lod_children && !inst->lod_entity));

			if (ImGui::SmallButton(g.tree_open ? ICON_FA_CHEVRON_UP "##tree_toggle" : ICON_FA_CHEVRON_DOWN "##tree_toggle")) {
				g.tree_open = !g.tree_open;
			} TT("Shows the lod tree of this mesh: its lod parent, the siblings and its own children. Click a row to select it.");

			ImGui::EndDisabled();

			labeled_value("Lod Children", inst ? shared::utils::va("%d", inst->lod_children) : "-");

			if (g.tree_open)
			{
				if (inst) {
					lod_tree::draw(ins, *inst, other, tree_height);
				} else {
					ImGui::Dummy(ImVec2(0.0f, tree_height));
				}
			}

			forced_line(ins, inst);

			g.trace_open = ImGui::TreeNodeEx(other ? "Culling Trace: selection###trace" : "Culling Trace###trace", ImGuiTreeNodeFlags_SpanAvailWidth);
			if (g.trace_open)
			{
				const auto& t = ins.trace_copy;
				const auto primary = ins.primary_phase_bit.load(std::memory_order_relaxed);
				const bool has_trace = sel && sel->visited && t.entity == sel->entity && t.stamp == ins.traversal_stamp.load(std::memory_order_relaxed);

				if (!has_trace)
				{
					const bool not_visited = sel && !sel->visited;
					ImGui::TextUnformatted(not_visited ? "Frustum: not reached" : "Frustum: -");
					ImGui::TextUnformatted(not_visited ? "Occlusion: not reached" : "Occlusion: -");

					if (not_visited) {
						ImGui::TextWrapped("Its map cells were not scanned. They are outside of every render phase or beyond the ~300m HD range in front of the camera.");
					} else if (sel) {
						ImGui::TextDisabled("Stage trace not available yet.");
					}
				}
				else
				{
					if (!(t.frustum_tested & primary)) {
						ImGui::TextUnformatted("Frustum: not reached (past lod distance or no phase bit)");
					} else if (t.frustum_passed & primary) {
						ImGui::Text("Frustum: passed (%s)", frustum_reason_name(t.primary_reason));
					} else {
						ImGui::Text("Frustum: culled (%s)", frustum_reason_name(t.primary_reason));
					}

					if (!t.occlusion_tested) {
						ImGui::TextUnformatted("Occlusion: not reached");
					} else {
						ImGui::Text("Occlusion: %s", t.occlusion_forced ? "skipped (extended anti culling)" : "tested");
					}

					if ((t.frustum_passed & primary) && !sel->visible && !sel->past_lod_distance) {
						ImGui::TextWrapped("Culled after the frustum test: occluded, or its lod parent swap hides it.");
					}
				}

				ImGui::TreePop();
			}

			ImGui::EndDisabled();
			ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), height - frame * 2.0f));

			// hidden count and unhide on the button grid below
			ImGui::SetCursorPosX(grid_x + column * 2.0f);
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Hidden: %d / %d", static_cast<int>(ins.hidden.size()), static_cast<int>(inspector_s::MAX_HIDDEN));
			TT("Hidden meshes stay culled until you unhide them or close the inspector. Their boxes are drawn in purple.\n"
			   "Hiding an HD mesh shows its LOD, use 'Hide with Lod Parents' to see what is behind it.");

			ImGui::SameLine();
			ImGui::SetCursorPosX(grid_x + column * 3.0f);
			ImGui::BeginDisabled(ins.hidden.empty());
			if (ImGui::Button("Unhide All", ImVec2(button_width * 2.0f + style.ItemSpacing.x, 0.0f))) {
				ins.hidden.clear();
			}
			ImGui::EndDisabled();

			g.add_category = ac.empty() ? 0 : std::clamp(g.add_category, 0, static_cast<int>(ac.size()) - 1);

			const ImVec2 button_size = ImVec2(button_width, 0.0f);
			ImGui::BeginDisabled(!sel);

			ImGui::SetNextItemWidth(button_width);
			ImGui::BeginDisabled(ac.empty());
			if (ImGui::BeginCombo("##category", ac.empty() ? "No categories" : ac[g.add_category].comment.c_str()))
			{
				for (auto i = 0; i < static_cast<int>(ac.size()); ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(ac[i].comment.c_str(), i == g.add_category)) {
						g.add_category = i;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(ac.empty() || sel_category >= 0);
			ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonGreen, true);
			if (ImGui::Button("Add Index", button_size))
			{
				ac[g.add_category].indices.emplace(sel->model_index);
				g.anticull_modified = true;
			}
			ImGui::Style_ColorButtonPop();
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(sel_category < 0);
			ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonRed, true);
			if (ImGui::Button("Remove Index", button_size))
			{
				for (auto& cat : ac) {
					cat.indices.erase(sel->model_index);
				}

				g.anticull_modified = true;
			}
			ImGui::Style_ColorButtonPop();
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Deselect", button_size)) {
				ins.selected = nullptr;
			}

			ImGui::SameLine();
			const bool hidden = sel && is_hidden(ins, sel->entity);
			ImGui::BeginDisabled(!hidden && ins.hidden.size() >= inspector_s::MAX_HIDDEN);
			ImGui::Style_ColorButtonPush(hidden ? COL_HIDDEN : COL_HIDE, true);
			if (ImGui::Button(hidden ? "Unhide###hide" : "Hide###hide", button_size)) {
				set_hidden(ins, sel->entity, sel->model_index, !hidden);
			}
			ImGui::Style_ColorButtonPop();
			ImGui::EndDisabled();

			ImGui::EndDisabled();
			ImGui::EndChild();
		}

#pragma endregion

#pragma region WINDOWS

		/// the filter box with its hint and the count of listed meshes inside, a clear button next to it
		void draw_filter(const inspector_s& ins)
		{
			int count = 0;
			for (const auto& inst : ins.instances)
			{
				if (passes_filter(ins, inst)) {
					count++;
				}
			}

			const auto& style = ImGui::GetStyle();
			const float clear_width = ImGui::GetFrameHeight();

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - RIGHT_PAD - clear_width - style.ItemSpacing.x);
			if (ImGui::InputTextWithHint("##filter", "Filter (index or name)", g.filter.InputBuf, IM_ARRAYSIZE(g.filter.InputBuf))) {
				g.filter.Build();
			}

			const auto count_str = shared::utils::va("%d meshes", count);
			const auto count_size = ImGui::CalcTextSize(count_str);
			const auto box_min = ImGui::GetItemRectMin();
			const auto box_max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddText(ImVec2(box_max.x - count_size.x - style.FramePadding.x, box_min.y + (box_max.y - box_min.y - count_size.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled), count_str);

			ImGui::SameLine();
			if (ImGui::Button("X##filter_clear", ImVec2(clear_width, clear_width))) {
				g.filter.Clear();
			}
		}

		void draw_table(inspector_s& ins)
		{
			auto& rows = g.rows;
			rows.clear();

			for (auto i = 0; i < static_cast<int>(ins.instances.size()); ++i)
			{
				if (passes_filter(ins, ins.instances[i])) {
					rows.emplace_back(i);
				}
			}

			enum column_id : int { COL_INDEX, COL_NAME, COL_HEIGHT, COL_RADIUS, COL_DIST, COL_TYPE, COL_STATE, COL_FORCED };
			constexpr auto flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
			constexpr float min_table_height = 150.0f;

			if (!ImGui::BeginTable("##mi_table", 8, flags, ImVec2(0.0f, std::max(ImGui::GetContentRegionAvail().y, min_table_height)))) {
				return;
			}

			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_None, 0.0f, COL_INDEX);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, COL_NAME);
			ImGui::TableSetupColumn("Height", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, COL_HEIGHT);
			ImGui::TableSetupColumn("Radius", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, COL_RADIUS);
			ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_None, 0.0f, COL_DIST);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 0.0f, COL_TYPE);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_None, 0.0f, COL_STATE);
			ImGui::TableSetupColumn("Forced", ImGuiTableColumnFlags_None, 0.0f, COL_FORCED);
			ImGui::TableHeadersRow();

			if (const auto specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0)
			{
				const auto column = specs->Specs[0].ColumnUserID;
				const bool ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;

				std::ranges::sort(rows, [&](const int a, const int b)
					{
						const auto& ia = ins.instances[a];
						const auto& ib = ins.instances[b];

						float ka = 0.0f;
						float kb = 0.0f;
						switch (column)
						{
							case COL_NAME:
							{
								const int cmp = std::strcmp(ia.model_name ? ia.model_name : "", ib.model_name ? ib.model_name : "");
								return ascending ? cmp < 0 : cmp > 0;
							}

							case COL_HEIGHT:
								ka = ia.height;
								kb = ib.height;
								break;

							case COL_RADIUS:
								ka = ia.radius;
								kb = ib.radius;
								break;

							case COL_DIST:
								ka = ia.distance;
								kb = ib.distance;
								break;

							case COL_TYPE:
								ka = ia.is_lod ? 1.0f : 0.0f;
								kb = ib.is_lod ? 1.0f : 0.0f;
								break;

							case COL_STATE:
								ka = ia.visible ? 1.0f : 0.0f;
								kb = ib.visible ? 1.0f : 0.0f;
								break;

							case COL_FORCED:
								ka = static_cast<float>(find_category(ia.model_index));
								kb = static_cast<float>(find_category(ib.model_index));
								break;

							default:
								ka = static_cast<float>(ia.model_index);
								kb = static_cast<float>(ib.model_index);
								break;
						}

						return ascending ? ka < kb : ka > kb;
					});
			}

			const auto& ac = anticull_categories();

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(rows.size()));
			while (clipper.Step())
			{
				for (auto r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r)
				{
					const auto& inst = ins.instances[rows[r]];
					const bool is_selected = inst.entity == ins.selected;

					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					ImGui::PushID(rows[r]);
					if (ImGui::Selectable(shared::utils::va("%d", inst.model_index), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
						ins.selected = is_selected ? nullptr : inst.entity;
					}

					if (ImGui::IsItemHovered())
					{
						ins.hovered = inst.entity;

						if (world::is_short_right_click())
						{
							ins.context = inst;
							g.open_context_menu = true;
						}
					}
					ImGui::PopID();

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(inst.model_name ? inst.model_name : "-");

					ImGui::TableNextColumn();
					ImGui::Text("%.1f", inst.height);

					ImGui::TableNextColumn();
					ImGui::Text("%.1f", inst.radius);

					ImGui::TableNextColumn();
					ImGui::Text("%.0f", inst.distance);

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(inst.is_lod ? "LOD" : "HD");

					ImGui::TableNextColumn();
					const auto [state, state_color] = instance_state(ins, inst);
					ImGui::TextColored(state_color, "%s", state);

					ImGui::TableNextColumn();
					const int category = find_category(inst.model_index);
					ImGui::TextUnformatted(category >= 0 ? ac[category].comment.c_str() : "");
				}
			}

			ImGui::EndTable();
		}

		/// height of the toolbar: two rows of widgets plus spacing
		float toolbar_height()
		{
			const auto& style = ImGui::GetStyle();
			return 12.0f + ImGui::GetFrameHeight() + 6.0f + style.ItemSpacing.y * 3.0f + 3.0f + ImGui::GetFontSize() * 0.82f;
		}

		void draw_meshes_window(inspector_s& ins)
		{
			if (!g.show_meshes) {
				return;
			}

			// below the toolbar
			{
				const auto& io = ImGui::GetIO();
				const float top = SCREEN_PADDING + toolbar_height() + SCREEN_PADDING;

				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - SCREEN_PADDING, top), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
				ImGui::SetNextWindowSize(ImVec2(580.0f, std::max(io.DisplaySize.y - top - SCREEN_PADDING, 300.0f)), ImGuiCond_FirstUseEver);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, WINDOW_PADDING);
			const bool visible = ImGui::Begin("Meshes###mesh_inspector_meshes", &g.show_meshes, ImGuiWindowFlags_NoCollapse);
			ImGui::PopStyleVar();

			if (!visible)
			{
				ImGui::End();
				return;
			}

			shared::imgui::draw_window_blur();

			ImGui::Spacing(0, 2);
			draw_details(ins);
			ImGui::Spacing(0, 8);
			draw_filter(ins);
			draw_table(ins);

			ImGui::End();
		}

		/// model indices of a set in columns: a click highlights the instances of an index, a double click removes it. True if one got removed
		bool draw_index_table(const char* id, std::unordered_set<int>& indices, const ImGuiTextFilter* filter, inspector_s& ins)
		{
			bool removed = false;

			ImGui::Widget_ContainerWithDropdownShadowSquare(120, [&]()
				{
					if (!ImGui::BeginTable(id, INDEX_COLUMNS, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(ImGui::GetContentRegionAvail().x, 100))) {
						return;
					}

					const float column_width = ImGui::GetContentRegionAvail().x / static_cast<float>(INDEX_COLUMNS);
					for (auto c = 0; c < INDEX_COLUMNS; c++) {
						ImGui::TableSetupColumn(shared::utils::va("##col%d", c), ImGuiTableColumnFlags_WidthFixed, column_width);
					}

					for (auto it = indices.begin(); it != indices.end(); )
					{
						const auto index_str = shared::utils::va("%d", *it);
						if (filter && !filter->PassFilter(index_str))
						{
							++it;
							continue;
						}

						// starts a new row after the last column
						ImGui::TableNextColumn();
						bool erase_this = false;

						if (ImGui::Selectable(index_str, false, ImGuiSelectableFlags_AllowDoubleClick))
						{
							if (ImGui::IsMouseDoubleClicked(0)) {
								erase_this = true;
							} else {
								ins.highlight_model_index = *it;
							}
						}

						if (erase_this)
						{
							it = indices.erase(it);
							removed = true;
						}
						else {
							++it;
						}
					}

					ImGui::EndTable();
				});

			return removed;
		}

		/// keeps one imgui id per category, so the state of a category does not move to its neighbour when one is removed
		void sync_category_ids(const std::size_t count)
		{
			while (g.category_ids.size() < count) {
				g.category_ids.emplace_back(g.next_category_id++);
			}

			g.category_ids.resize(count);
		}

		void draw_category(map_settings::anti_cull_meshes_s& cat, inspector_s& ins, bool& remove_category)
		{
			if (!ImGui::CollapsingHeader(shared::utils::va("Section: %s  (%d)###category", cat.comment.c_str(), static_cast<int>(cat.indices.size())))) {
				return;
			}

			ImGui::Spacing(0, 4);

			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
			if (ImGui::DragInt("Distance", &cat.distance)) {
				g.anticull_modified = true;
			}

			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
			if (ImGui::InputText("Comment", &cat._internal_comment_buffer, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				cat.comment = cat._internal_comment_buffer;
				cat._internal_comment_buffer.clear();
				g.anticull_modified = true;
			} TT("Press ENTER to rename the category.");

			ImGui::Spacing(0, 4);

			ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
			ImGui::TextUnformatted("  Click to highlight, double click to remove .. ");
			ImGui::PopFont();

			if (draw_index_table("##ac_table", cat.indices, &g.index_filter, ins)) {
				g.anticull_modified = true;
			}

			ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonRed, true);
			if (ImGui::Button("Remove Category")) {
				remove_category = true;
			}
			ImGui::Style_ColorButtonPop();

			ImGui::SameLine(0, 24);

			bool add_index = false;
			ImGui::BeginDisabled(cat._internal_buffer.empty());
			if (ImGui::Button(" + ")) {
				add_index = true;
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
			if (ImGui::InputText("Add Index", &cat._internal_buffer, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
				add_index = true;
			} TT("Use the + Button on the left or press ENTER to add the index.");

			if (add_index && add_typed_index(cat.indices, cat._internal_buffer, "AntiCull - Add Index - Invalid Argument")) {
				g.anticull_modified = true;
			}
		}

		void draw_categories_tab(inspector_s& ins)
		{
			auto& ac = anticull_categories();

			ImGui::Spacing(0, 4);
			ImGui::Style_ColorButtonPush(imgui::get()->ImGuiCol_ButtonGreen, true);
			if (ImGui::Button(ICON_FA_PLUS "  Category", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				ac.emplace_back(map_settings::anti_cull_meshes_s { .distance = 0, .comment = "new" + std::to_string(ac.size()) });
				g.anticull_modified = true;
			}
			ImGui::Style_ColorButtonPop();

			ImGui::Spacing(0, 6);

			ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
			ImGui::TextUnformatted("  Filter Indices ... ");
			ImGui::PopFont();

			g.index_filter.Draw("##index_filter", ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);

			ImGui::SameLine();
			if (ImGui::Button("X##index_filter_clear", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
				g.index_filter.Clear();
			}

			ImGui::Spacing(0, 8);
			sync_category_ids(ac.size());

			for (auto i = 0; i < static_cast<int>(ac.size()); )
			{
				bool remove_category = false;

				ImGui::PushID(static_cast<int>(g.category_ids[i]));
				draw_category(ac[i], ins, remove_category);
				ImGui::PopID();

				if (remove_category)
				{
					ac.erase(ac.begin() + i);
					g.category_ids.erase(g.category_ids.begin() + i);
					g.anticull_modified = true;
				}
				else {
					++i;
				}
			}
		}

		// map_settings LOD_FADE_EXCLUDE: meshes the lod cross fade never dithers
		void draw_lod_fade_tab(inspector_s& ins)
		{
			auto& excluded = lod_fade_excluded();

			ImGui::Spacing(0, 4);
			ImGui::TextWrapped("Excluded meshes are not drawn while the game cross fades them with their lod: they pop instead of dithering. "
							   "Right click a mesh in the world or in the mesh list to exclude it.");
			ImGui::Spacing(0, 4);

			ImGui::PushFont(shared::imgui::font::REGULAR_SMALL);
			ImGui::TextUnformatted("  Click to highlight, double click to remove .. ");
			ImGui::PopFont();

			if (draw_index_table("##fade_exclude_table", excluded, nullptr, ins)) {
				g.lod_fade_modified = true;
			}

			bool add_index = false;
			ImGui::BeginDisabled(g.lod_fade_add_buffer.empty());
			if (ImGui::Button(" + ##fade_exclude")) {
				add_index = true;
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::CalcWidgetWidthForChild(80.0f));
			if (ImGui::InputText("Add Index##fade_exclude", &g.lod_fade_add_buffer, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
				add_index = true;
			} TT("Use the + Button on the left or press ENTER to add the index.");

			if (add_index && add_typed_index(excluded, g.lod_fade_add_buffer, "LOD Fade Exclude - Add Index - Invalid Argument")) {
				g.lod_fade_modified = true;
			}
		}

		/// what gets listed, picked and drawn in the world
		void draw_options(inspector_s& ins)
		{
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float label_width = ImGui::CalcTextSize("Min Radius").x + ImGui::GetStyle().ItemInnerSpacing.x;
			const float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
			const float second_column = ImGui::GetCursorPosX() + half + spacing;

			ImGui::SeparatorText("Inspector Options");

			ImGui::Checkbox("HD", &ins.show_hd);
			TT("Lists HD meshes.");

			ImGui::SameLine(0, 14);
			ImGui::Checkbox("LOD", &ins.show_lod);
			TT("Lists LOD meshes. Their boxes span several buildings and get in the way when picking.");

			ImGui::SameLine(0, 14);
			ImGui::Checkbox("Only Forced", &ins.only_forced);
			TT("Only lists meshes that an anti cull category forces.");

			ImGui::SameLine(0, 14);
			ImGui::Checkbox("Pick Visible Only", &ins.pick_visible_only);
			TT("Clicks in the world only pick meshes that were drawn last frame. The table still lists every mesh.");

			ImGui::Checkbox("Boxes on Forced", &ins.boxes_on_forced);
			TT("Draws a box around every listed mesh that an anti cull category forces.");

			ImGui::SameLine(0, 14);
			ImGui::Checkbox("Labels", &ins.show_labels);
			TT("Model index and name on the listed meshes in the world.");

			ImGui::SameLine(second_column);
			ImGui::SetNextItemWidth(half - label_width);
			ImGui::DragInt("Max Labels", &ins.max_labels, 0.2f, 1, 300);
			TT("Labels go to the largest meshes first and are skipped where they would overlap.");

			ImGui::SetNextItemWidth(half - label_width);
			ImGui::DragFloat("Min Radius", &ins.min_radius, 0.5f, 0.0f, 1000.0f, "%.1f");
			TT("Meshes with a smaller bounding radius are not listed.");

			ImGui::SameLine(second_column);
			ImGui::SetNextItemWidth(half - label_width);
			ImGui::DragFloat("Min Height", &ins.min_height, 0.5f, 0.0f, 1000.0f, "%.1f");
			TT("Meshes lower than this are not listed.");

			ImGui::SetNextItemWidth(half - label_width);
			auto lod_view = static_cast<int>(ins.lod_view);
			if (ImGui::Combo("LOD View", &lod_view, "Off\0LOD (hide HD)\0Lowest LOD\0"))
			{
				ins.lod_view = static_cast<inspector_s::lod_view_mode>(lod_view);
				if (ins.lod_view != inspector_s::lod_view_mode::off) {
					ins.show_lod = true;
				}
			}
			TT("Culls detail levels map wide to look at the lod meshes.\n"
			   "LOD: hides every HD mesh that has a lod parent, so its LOD is drawn instead.\n"
			   "Lowest LOD: only draws meshes without a lod parent.\n"
			   "Picking a mode enables LOD entries in the list.");

			ImGui::SameLine(second_column);
			ImGui::SetNextItemWidth(half - label_width);
			ImGui::DragInt("Highlight", &ins.highlight_model_index, 1.0f, -1, 0xFFFF);
			TT("Draws a box around every listed instance of this model index. -1 = off.\nClicking an index of a category or of the lod fade exclusions sets it.");
		}

		void draw_categories_window(inspector_s& ins)
		{
			if (!g.show_categories) {
				return;
			}

			const auto& io = ImGui::GetIO();
			ImGui::SetNextWindowPos(ImVec2(SCREEN_PADDING, SCREEN_PADDING), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(420.0f, std::max(io.DisplaySize.y - SCREEN_PADDING * 2.0f, 300.0f)), ImGuiCond_FirstUseEver);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, WINDOW_PADDING);
			const bool visible = ImGui::Begin("Anti Cull / LOD Fade###mesh_inspector_categories", &g.show_categories, ImGuiWindowFlags_NoCollapse);
			ImGui::PopStyleVar();

			if (!visible)
			{
				ImGui::End();
				return;
			}

			shared::imgui::draw_window_blur();

			ImGui::PushStyleColor(ImGuiCol_Text, has_uncopied_changes() ? COL_BUTTON_UNSAVED : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped("Changes apply at once but are not saved. Copy them to the clipboard (toolbar) and paste them into map_settings.toml, a reload or restart drops them.");
			ImGui::PopStyleColor();

			// the options keep their place at the bottom, the tabs scroll above them
			const float options_height = ImGui::GetFrameHeightWithSpacing() * 4.0f + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f;

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::BeginChild("##mi_tabs_area", ImVec2(0.0f, -options_height));
			ImGui::PopStyleColor();

			if (ImGui::BeginTabBar("##mi_tabs"))
			{
				if (ImGui::BeginTabItem(g.anticull_modified ? "Anti Cull Categories *###anticull" : "Anti Cull Categories###anticull"))
				{
					draw_categories_tab(ins);
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem(g.lod_fade_modified ? "LOD Fade Exclusions *###lod_fade" : "LOD Fade Exclusions###lod_fade"))
				{
					draw_lod_fade_tab(ins);
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::EndChild();
			draw_options(ins);

			ImGui::End();
		}

		void close_editor()
		{
			auto& ins = inspector();

			g.open = false;
			g.open_context_menu = false;

			// the hooks get the cleared state right away, nothing syncs it while the editor is closed
			ins.enabled = false;
			ins.hidden.clear();
			ins.pinned = nullptr;
			ins.hovered = nullptr;
			ins.lod_view = inspector_s::lod_view_mode::off;
			sync(ins);

			free_camera::set_active(false);
			shared::imgui::editor_host::sync_open_state();
		}

		void draw_toolbar(inspector_s& ins)
		{
			const auto& io = ImGui::GetIO();

			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - SCREEN_PADDING, SCREEN_PADDING), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

			if (ImGui::Begin("##mesh_inspector_toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				shared::imgui::draw_window_blur();
				shared::imgui::editor::toolbar_grip();

				editor_common::pause_buttons(g.step_held, "Pause the game like the pause menu does, cutscenes included. The scene keeps rendering.");
				tool_separator();

				editor_common::camera_buttons(g.rmb_game_input);
				tool_separator();

				// view
				if (tool_button(ICON_FA_LIST "##meshes", "Mesh list and details of the selected mesh", g.show_meshes)) {
					g.show_meshes = !g.show_meshes;
				}

				if (tool_button(ICON_FA_LAYER_GROUP "##categories", "Anti cull categories and LOD fade exclusions", g.show_categories)) {
					g.show_categories = !g.show_categories;
				}

				if (tool_button(ICON_FA_TAG "##labels", "Model index and name on the listed meshes in the world", ins.show_labels)) {
					ins.show_labels = !ins.show_labels;
				}

				tool_separator();

				// export
				if (tool_button(ICON_FA_CLIPBOARD "  ANTICULL##copy_anticull",
					g.anticull_modified ? "Copy the ANTICULL array to the clipboard, it changed since the last copy" : "Copy the ANTICULL array to the clipboard",
					g.anticull_modified, COL_BUTTON_UNSAVED))
				{
					copy_anticull_to_clipboard();
				}

				if (tool_button(ICON_FA_CLIPBOARD "  LOD_FADE_EXCLUDE##copy_lod_fade",
					g.lod_fade_modified ? "Copy the LOD_FADE_EXCLUDE array to the clipboard, it changed since the last copy" : "Copy the LOD_FADE_EXCLUDE array to the clipboard",
					g.lod_fade_modified, COL_BUTTON_UNSAVED))
				{
					copy_lod_fade_exclusions_to_clipboard();
				}

				if (tool_button(ICON_FA_REDO "##reload", "Reload the map settings from disk. Changes that were not pasted into map_settings.toml are lost")) {
					editor_common::open_reload_map_settings_popup();
				}

				tool_separator();

				if (tool_button(ICON_FA_TIMES "##close", "Close the mesh inspector: unhides all meshes and turns the LOD view off")) {
					set_open(false);
				}

				// the second row is as wide as the buttons
				const float row_width = ImGui::GetItemRectMax().x - (ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x);
				ImGui::NewLine();
				ImGui::Spacing(0, 3);

				// camera speed and inspector radius share the width of the button row
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
					ImGui::SliderFloat("##radius", &ins.radius, 10.0f, 3000.0f, "Mesh radius  %.0f m", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

					ImGui::SetWindowFontScale(1.0f);
					ImGui::PopStyleVar(2);
					TT("Only meshes within this distance around the camera are listed");
				}

				// messages go below the toolbar, so it keeps its size
				g.status.draw(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowPos().y + ImGui::GetWindowHeight() + 4.0f));

				draw_context_menu(ins);
				editor_common::reload_map_settings_popup();
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		void handle_shortcuts(inspector_s& ins)
		{
			const auto& io = ImGui::GetIO();
			if (io.WantTextInput || io.MouseDown[ImGuiMouseButton_Right]) {
				return;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
				ins.selected = nullptr;
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
			close_editor();
			return;
		}

		if (shared::imgui::editor_host::any_open()) {
			return;
		}

		g.open = true;
		inspector().enabled = true;
		shared::imgui::editor_host::sync_open_state();
		shared::globals::imgui_menu_open = false;
		free_camera::set_active(true);
	}

	void toggle() {
		set_open(!g.open);
	}

	bool has_uncopied_changes() {
		return g.anticull_modified || g.lod_fade_modified;
	}

	void copy_anticull_to_clipboard()
	{
		ImGui::SetClipboardText(map_settings_toml::build_anticull_array(anticull_categories()).c_str());
		g.anticull_modified = false;
		set_status("Copied ANTICULL to clipboard");
	}

	void copy_lod_fade_exclusions_to_clipboard()
	{
		ImGui::SetClipboardText(map_settings_toml::build_lod_fade_exclude_array(lod_fade_excluded()).c_str());
		g.lod_fade_modified = false;
		set_status("Copied LOD_FADE_EXCLUDE to clipboard");
	}

	bool allows_game_input_on_rmb() {
		return g.open && g.rmb_game_input && !free_camera::is_active();
	}

	void on_map_settings_reloaded()
	{
		auto& ins = inspector();
		ins.selected = nullptr;
		ins.highlight_model_index = -1;

		g.open_context_menu = false;
		g.anticull_modified = false;
		g.lod_fade_modified = false;
		g.add_category = 0;
		g.lod_fade_add_buffer.clear();
		g.category_ids.clear();
	}

	void draw()
	{
		if (!g.open) {
			return;
		}

		auto& ins = inspector();
		g.view_valid = scene_view::get(g.view);

		handle_shortcuts(ins);
		draw_toolbar(ins);

		if (!g.open) {
			return;
		}

		draw_meshes_window(ins);
		draw_categories_window(ins);

		// after the windows: they name the hovered mesh and still point into the instance list that the sync swaps
		sync(ins);
		draw_world_overlay(ins);
		ins.hovered = nullptr;
	}

	void register_editor() {
		shared::imgui::editor_host::register_editor({ "Mesh Inspector", is_open, draw, allows_game_input_on_rmb });
	}
}
