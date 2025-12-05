#include "std_include.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_helper.hpp"

#include "gta4/modules/natives.hpp"
#include "shared/globals.hpp"

namespace shared::imgui
{
	bool world_to_screen(const Vector& world_position, ImVec2& screen_position_out)
	{
		int32_t vp_id = 0u;
		gta4::natives::GetGameViewportId(&vp_id);

		return gta4::natives::GetViewportPositionOfCoord(world_position.x, world_position.y, world_position.z, vp_id, &screen_position_out.x, &screen_position_out.y);
	}

	void get_and_add_integers_to_set(char* str, std::unordered_set<std::uint32_t>& set, const std::uint32_t& buf_len, const bool clear_buf)
	{
		std::vector<int> temp_vec;
		utils::extract_integer_words(str, temp_vec, true);
		set.insert(temp_vec.begin(), temp_vec.end());

		if (clear_buf) {
			memset(str, 0, buf_len);
		}
	}

	void get_and_remove_integers_from_set(char* str, std::unordered_set<std::uint32_t>& set, const std::uint32_t& buf_len, const bool clear_buf)
	{
		std::vector<int> temp_vec;
		utils::extract_integer_words(str, temp_vec, true);

		for (const auto& v : temp_vec) {
			set.erase(v);
		}

		if (clear_buf) {
			memset(str, 0, buf_len);
		}
	}

	namespace blur
	{
		namespace
		{
			IDirect3DSurface9* rt_backup = nullptr;
			IDirect3DPixelShader9* shader_x = nullptr;
			IDirect3DPixelShader9* shader_y = nullptr;
			IDirect3DTexture9* texture = nullptr;
			std::uint32_t backbuffer_width = 0u;
			std::uint32_t backbuffer_height = 0u;

			void begin_blur([[maybe_unused]] const ImDrawList* parent_list, const ImDrawCmd* cmd)
			{
				if (cmd->UserCallbackData)
				{
					const auto device = static_cast<IDirect3DDevice9*>(cmd->UserCallbackData);

					if (!shader_x) {
						device->CreatePixelShader(reinterpret_cast<const DWORD*>(blur_x.data()), &shader_x);
					}

					if (!shader_y) {
						device->CreatePixelShader(reinterpret_cast<const DWORD*>(blur_y.data()), &shader_y);
					}

					IDirect3DSurface9* backBuffer;
					device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);

					D3DSURFACE_DESC desc;
					backBuffer->GetDesc(&desc);

					if (backbuffer_width != desc.Width || backbuffer_height != desc.Height)
					{
						if (texture) {
							texture->Release();
						}

						backbuffer_width = desc.Width;
						backbuffer_height = desc.Height;
						device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr);
					}

					device->GetRenderTarget(0, &rt_backup);

					{
						IDirect3DSurface9* surface;
						texture->GetSurfaceLevel(0, &surface);
						device->StretchRect(backBuffer, nullptr, surface, nullptr, D3DTEXF_NONE);
						device->SetRenderTarget(0, surface);
						surface->Release();
					}

					backBuffer->Release();

					device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
					device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
				}
			}

			void first_blur_pass([[maybe_unused]] const ImDrawList* parent_list, const ImDrawCmd* cmd)
			{
				if (cmd->UserCallbackData)
				{
					const auto device = static_cast<IDirect3DDevice9*>(cmd->UserCallbackData);

					device->SetPixelShader(shader_x);
					const float params[4] = { 1.0f / (float)backbuffer_width };
					device->SetPixelShaderConstantF(0, params, 1);
				}
			}

			void second_blur_pass([[maybe_unused]] const ImDrawList* parent_list, const ImDrawCmd* cmd)
			{
				if (cmd->UserCallbackData)
				{
					const auto device = static_cast<IDirect3DDevice9*>(cmd->UserCallbackData);

					device->SetPixelShader(shader_y);
					const float params[4] = { 1.0f / (float)backbuffer_height };
					device->SetPixelShaderConstantF(0, params, 1);
				}
			}

			void end_blur([[maybe_unused]] const ImDrawList* parent_list, const ImDrawCmd* cmd)
			{
				if (cmd->UserCallbackData)
				{
					const auto device = static_cast<IDirect3DDevice9*>(cmd->UserCallbackData);

					device->SetRenderTarget(0, rt_backup);
					rt_backup->Release();

					device->SetPixelShader(nullptr);
					device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
					device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
				}
			}
		}
	}

	void draw_blur(ImDrawList* draw_list)
	{
		assert(globals::d3d_device != nullptr && "Trying to draw ImGui background blur when the d3d device was null!");
		IDirect3DDevice9* dev = globals::d3d_device;

		if (dev)
		{
			const ImVec2 img_size = { (float)blur::backbuffer_width, (float)blur::backbuffer_height };

			draw_list->AddCallback(blur::begin_blur, dev);
			for (int i = 0; i < 8; ++i)
			{
				draw_list->AddCallback(blur::first_blur_pass, dev);
				draw_list->AddImage((ImTextureID)blur::texture, { 0.0f, 0.0f }, img_size);
				draw_list->AddCallback(blur::second_blur_pass, dev);
				draw_list->AddImage((ImTextureID)blur::texture, { 0.0f, 0.0f }, img_size);
			}

			draw_list->AddCallback(blur::end_blur, dev);
			draw_list->AddImageRounded((ImTextureID)blur::texture,
				{ 0.0f, 0.0f },
				img_size,
				{ 0.00f, 0.00f },
				{ 1.00f, 1.00f },
				IM_COL32(255, 255, 255, 255),
				0.f);
		}
	}

	// Blur window background
	void draw_window_blur()
	{
		// only blur the window, clip everything else
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImGui::PushClipRect(window->InnerClipRect.Min, window->InnerClipRect.Max, true);

		draw_blur(ImGui::GetWindowDrawList());
		ImGui::PopClipRect();
	}

	// Blur entire background
	void draw_background_blur()
	{
		draw_blur(ImGui::GetBackgroundDrawList());
	}
}

namespace ImGui
{
	void Spacing(const float& x, const float& y) {
		Dummy(ImVec2(x, y));
	}

	void PushFont(shared::imgui::font::FONTS font)
	{
		ImGuiIO& io = GetIO();

		if (io.Fonts->Fonts[font]) {
			PushFont(io.Fonts->Fonts[font]);
		}
		else {
			PushFont(GetDefaultFont());
		}
	}

	void SeparatorTextLarge(const char* text, bool pre_spacing)
	{
		if (pre_spacing) {
			Spacing(0, 12);
		}

		PushFont(shared::imgui::font::BOLD_LARGE);
		SeparatorText(text);
		PopFont();
		Spacing(0, 4);
	}

	// Calculates the width for each buttons to fit in a single row, accounting for ImGui's content region and inter-button spacing
	float CalcButtonWidthSameRow(std::uint8_t btn_count)
	{
		const auto b = static_cast<float>(btn_count);
		return (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * std::max(b, 0.0f)) / std::max(b, 1.0f);
	}

	// Draw wrapped text containing all unsigned integers from the provided unordered_set
	void TextWrapped_IntegersFromUnorderedSet(const std::unordered_set<std::uint32_t>& set)
	{
		std::string arr_str;
		for (auto it = set.begin(); it != set.end(); ++it)
		{
			if (it != set.begin()) {
				arr_str += ", ";
			} arr_str += std::to_string(*it);
		}
		if (arr_str.empty()) {
			arr_str = "// empty";
		}
		TextWrapped("%s", arr_str.c_str());
	}

	//void Widget_UnorderedSetModifier(const char* id, Widget_UnorderedSetModifierFlags flag, std::unordered_set<std::uint32_t>& set, char* buffer, std::uint32_t buffer_len)
	//{
	//	const auto txt_input_full = "Add/Remove..";
	//	const auto txt_input_full_width = CalcTextSize(txt_input_full).x;
	//	const auto txt_input_min = "...";
	//	const auto txt_input_min_width = CalcTextSize(txt_input_min).x;

	//	const bool narrow = GetContentRegionAvail().x < 100.0f;

	//	PushID(id);

	//	if (!narrow) 
	//	{
	//		if (Button("-##Remove"))
	//		{
	//			common::imgui::get_and_remove_integers_from_set(buffer, set, buffer_len, true);
	//			main_module::trigger_vis_logic();
	//		}
	//		SetCursorScreenPos(ImVec2(GetItemRectMax().x, GetItemRectMin().y));
	//	}

	//	const auto spos = GetCursorScreenPos();
	//	
	//	SetNextItemWidth(GetContentRegionAvail().x - (narrow ? 0.0f : 40.0f));
	//	if (InputText("##Input", buffer, buffer_len, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll)) {
	//		common::imgui::get_and_add_integers_to_set(buffer, set, buffer_len, true);
	//	}

	//	SetCursorScreenPos(spos);
	//	if (!buffer[0])
	//	{
	//		const auto min_content_area_width = GetContentRegionAvail().x - 40.0f;
	//		ImVec2 pos = GetCursorScreenPos() + ImVec2(8.0f, CalcTextSize("A").y * 0.45f);
	//		if (min_content_area_width > txt_input_full_width) {
	//			GetWindowDrawList()->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), txt_input_full);
	//		}
	//		else if (min_content_area_width > txt_input_min_width) {
	//			GetWindowDrawList()->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), txt_input_min);
	//		}
	//	}

	//	if (narrow) 
	//	{
	//		// next line :>
	//		Dummy(ImVec2(0, GetFrameHeight()));
	//		if (Button("-##Remove"))
	//		{
	//			common::imgui::get_and_remove_integers_from_set(buffer, set, buffer_len, true);
	//			main_module::trigger_vis_logic();
	//		}
	//	}

	//	SetCursorScreenPos(ImVec2(GetItemRectMax().x, GetItemRectMin().y));
	//	if (Button("+##Add")) {
	//		common::imgui::get_and_add_integers_to_set(buffer, set, buffer_len, true);
	//	}
	//	SetCursorScreenPos(ImVec2(GetItemRectMax().x + 1.0f, GetItemRectMin().y));
	//	if (Button("P##Picker"))
	//	{
	//		const auto c_str = utils::va("%d", flag == Widget_UnorderedSetModifierFlags_Leaf ? g_current_leaf : g_current_area);
	//		common::imgui::get_and_add_integers_to_set((char*)c_str, set);
	//		main_module::trigger_vis_logic();
	//	}
	//	SetItemTooltipBlur(flag == Widget_UnorderedSetModifierFlags_Leaf ? "Pick Current Leaf" : "Pick Current Area");
	//	PopID();
	//}

	// #

	void Style_DeleteButtonPush()
	{
		PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.05f, 0.05f, 1.0f));
		PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.05f, 0.05f, 1.0f));
		PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 1.0f));

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		PushFont(shared::imgui::font::BOLD);
	}

	void Style_DeleteButtonPop()
	{
		PopStyleVar(2);
		PopStyleColor(4);
		PopFont();
	}

	// #

	void Style_ColorButtonPush(const ImVec4& base_color, bool black_border)
	{
		PushStyleColor(ImGuiCol_Button, base_color);
		PushStyleColor(ImGuiCol_ButtonHovered, base_color * ImVec4(1.4f, 1.4f, 1.4f, 1.0f));
		PushStyleColor(ImGuiCol_ButtonActive, base_color * ImVec4(1.8f, 1.8f, 1.8f, 1.0f));

		PushStyleColor(ImGuiCol_Border, black_border 
			? ImVec4(0, 0, 0, 1.0f) 
			: GetStyleColorVec4(ImGuiCol_Border));
	}

	void Style_ColorButtonPop() {
		PopStyleColor(4);
	}

	// #

	void Style_InvisibleSelectorPush() {
		PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
	}

	void Style_InvisibleSelectorPop() {
		PopStyleColor(2);
	}

	// #

	bool BeginTooltipBlurEx(ImGuiTooltipFlags tooltip_flags, ImGuiWindowFlags extra_window_flags)
	{
		ImGuiContext& g = *GImGui;

		const bool is_dragdrop_tooltip = g.DragDropWithinSource || g.DragDropWithinTarget;
		if (is_dragdrop_tooltip)
		{
			if ((g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasPos) == 0)
			{
				ImVec2 tooltip_pos = (g.IO.MousePos * g.Style.MouseCursorScale);
				ImVec2 tooltip_pivot = ImVec2(0.0f, 0.0f);
				SetNextWindowPos(tooltip_pos, ImGuiCond_None, tooltip_pivot);
			}

			SetNextWindowBgAlpha(g.Style.Colors[ImGuiCol_PopupBg].w * 0.60f);
			tooltip_flags |= ImGuiTooltipFlags_OverridePrevious;
		}

		const char* window_name_template = is_dragdrop_tooltip ? "##Tooltip_DragDrop_%02d" : "##Tooltip_%02d";
		char window_name[32];
		ImFormatString(window_name, IM_ARRAYSIZE(window_name), window_name_template, g.TooltipOverrideCount);

		if ((tooltip_flags & ImGuiTooltipFlags_OverridePrevious) && g.TooltipPreviousWindow != nullptr && g.TooltipPreviousWindow->Active)
		{
			SetWindowHiddenAndSkipItemsForCurrentFrame(g.TooltipPreviousWindow);
			ImFormatString(window_name, IM_ARRAYSIZE(window_name), window_name_template, ++g.TooltipOverrideCount);
		}

		ImGuiWindowFlags flags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
		Begin(window_name, nullptr, flags | extra_window_flags, &shared::imgui::draw_window_blur_callback);
		return true;
	}

	void SetItemTooltipBlur(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		if (IsItemHovered(ImGuiHoveredFlags_ForTooltip))
		{
			// (0.124f, 0.124f, 0.124f, 0.776f)
			PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.124f, 0.124f, 0.124f, 0.776f));

			if (!BeginTooltipBlurEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None)) {
				return;
			}
			PopStyleColor();

			const auto padding = 4.0f;

			Spacing(0, padding);			 // top padding
			Spacing(padding, 0); SameLine(); // left padding

			TextV(fmt, args);

			SameLine(); Spacing(padding, 0); // right padding
			Spacing(0, padding);			 // bottom padding

			EndTooltip();
		}

		va_end(args);
	}

	void TableHeadersRowWithTooltip(const char** tooltip_strings)
	{
		const float row_height = TableGetHeaderRowHeight();
		TableNextRow(ImGuiTableRowFlags_Headers, row_height);
		const float row_y1 = GetCursorScreenPos().y;

		const int columns_count = TableGetColumnCount();
		for (int column_n = 0; column_n < columns_count; column_n++)
		{
			if (!TableSetColumnIndex(column_n)) {
				continue;
			}

			TableHeader(TableGetColumnName(column_n));

			if (!std::string_view(tooltip_strings[column_n]).empty()) {
				SetItemTooltipBlur("%s", tooltip_strings[column_n]);
			}

			// Allow opening popup from the right-most section after the last column.
			ImVec2 mouse_pos = GetMousePos();

			if (IsMouseReleased(1) && TableGetHoveredColumn() == columns_count)
			{
				if (mouse_pos.y >= row_y1 && mouse_pos.y < row_y1 + row_height) {
					TableOpenContextMenu(columns_count); // Will open a non-column-specific popup.
				}
			}
		}
	}

	// #

	float CalcWidgetWidthForChild(const float label_width)
	{
		return GetContentRegionAvail().x - 4.0f - (label_width + GetStyle().ItemInnerSpacing.x + GetStyle().FramePadding.y);
	}

	void CenterText(const char* text, bool disabled)
	{
		const auto text_width = CalcTextSize(text).x;
		SetCursorPosX(GetContentRegionAvail().x * 0.5f - text_width * 0.5f);
		if (!disabled) {
			TextUnformatted(text);
		}
		else {
			TextDisabled("%s", text);
		}
	}

	void AddUnterline(ImColor col)
	{
		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		min.y = max.y;
		GetWindowDrawList()->AddLine(min, max, col, 1.0f);
	}

	void TextURL(const char* name, const char* url, bool use_are_you_sure_popup)
	{
		TextUnformatted(name);
		if (IsItemHovered())
		{
			if (IsMouseClicked(0))
			{
				if (use_are_you_sure_popup)
				{
					if (!IsPopupOpen("Are You Sure?")) 
					{
						PushID(name);
						OpenPopup("Are You Sure?");
						PopID();
					}
				}
				else
				{
					ImGuiIO& io = GetIO();
					io.AddMouseButtonEvent(0, false);
					io.AddMousePosEvent(0, 0);
					ShellExecuteA(nullptr, nullptr, url, nullptr, nullptr, SW_SHOW);
				}
			}

			AddUnterline(GetStyle().Colors[ImGuiCol_TabHovered]);
			SetTooltip("Clicking this will open the following link:\n[%s]", url);
		}
		else {
			AddUnterline(GetStyle().Colors[ImGuiCol_Button]);
		}

		PushID(name);
		if (BeginPopupModal("Are You Sure?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			shared::imgui::draw_background_blur();
			Spacing(0.0f, 0.0f);

			const auto half_width = GetContentRegionMax().x * 0.5f;
			auto line1_str = "This will open the following link:";

			Spacing();
			SetCursorPosX(5.0f + half_width - (CalcTextSize(line1_str).x * 0.5f));
			TextUnformatted(line1_str);

			PushFont(shared::imgui::font::BOLD);
			SetCursorPosX(5.0f + half_width - (CalcTextSize(url).x * 0.5f));
			TextUnformatted(url);

			InvisibleButton("##spacer", ImVec2(CalcTextSize(url).x, 1));
			PopFont();

			Spacing(0, 8);
			Spacing(0, 0); SameLine();

			ImVec2 button_size(half_width - 6.0f - GetStyle().WindowPadding.x, 0.0f);
			if (Button("Open", button_size))
			{
				ImGuiIO& io = GetIO();
				io.AddMouseButtonEvent(0, false);
				io.AddMousePosEvent(0, 0);
				CloseCurrentPopup();
				ShellExecuteA(nullptr, nullptr, url, nullptr, nullptr, SW_SHOW);
			}

			SameLine(0, 6.0f);
			if (Button("Cancel", button_size)) {
				CloseCurrentPopup();
			}

			EndPopup();
		}
		PopID();
	}

	void SetCursorForCenteredText(const char* text)
	{
		//SetCursorPosX((GetWindowSize().x - CalcTextSize(text).x) * 0.5f);
		const auto text_width = CalcTextSize(text).x;
		SetCursorPosX(GetContentRegionAvail().x * 0.5f - text_width * 0.5f);
	}

	bool TextUnformatted_ClippedByColumnTooltip(const char* str)
	{
		TextUnformatted(str);
		if (CalcTextSize(str).x > GetColumnWidth()) 
		{
			SetItemTooltipBlur(str);
			return true;
		}

		return false;
	}

	void Draw3DCircle(ImDrawList* draw_list, const Vector& world_pos, const Vector& normal, const float radius, const bool filled, const ImColor& color, const float& thickness, int num_points)
	{
		num_points = std::clamp(num_points, 7, 200);
		static ImVec2 points[200];

		float step = 6.2831f / (float)num_points;
		float theta = 0.f;

		// Create a vector that's not parallel to the normal
		Vector temp = (std::abs(normal.x) > 0.9f) ? Vector(0, 1, 0) : Vector(1, 0, 0);

		// Right vector is perpendicular to the normal and temp vector
		Vector right = normal.Cross(temp);
		right.Normalize();

		// Up vector is perpendicular to both normal and right vectors
		Vector up = right.Cross(normal);
		up.Normalize();

		for (auto i = 0u; i < (std::uint32_t)num_points; i++, theta += step) 
		{
			//Vector world_space = { world_pos.x + radius * cos(theta), world_pos.y, world_pos.z - radius * sin(theta) };
			Vector world_space = world_pos + (right * cos(theta) + up * sin(theta)) * radius;
			Vector screen_space;
			//common::imgui::world2screen(world_space, screen_space); // TODO

			points[i].x = screen_space.x;
			points[i].y = screen_space.y;
		}

		if (filled) {
			draw_list->AddConvexPolyFilled(points, num_points, color);
		} else {
			draw_list->AddPolyline(points, num_points, color, true, thickness);
		}
	}

	void TableHeaderDropshadow(const float height, const float max_alpha, const float neg_y_offset, const float custom_width)
	{
		const float dshadow_height = height;
		const auto dshadow_pmin = GetCursorScreenPos() - ImVec2(0, neg_y_offset);
		const auto dshadow_pmax = dshadow_pmin + ImVec2((custom_width > 0.0f ? custom_width : GetContentRegionAvail().x), dshadow_height);
		const auto col_top = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.0f));
		const auto col_bottom = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, max_alpha));
		GetWindowDrawList()->AddRectFilledMultiColor(dshadow_pmin, dshadow_pmax, col_top, col_top, col_bottom, col_bottom);
		Spacing(0, dshadow_height - 3.0f - neg_y_offset);
	}

	// Labelwidth = 80
	bool Widget_PrettyDragVec3(const char* ID, float* vec_in, bool show_label, const float label_size, const float speed, const float min, const float max,
		const char* x_str, const char* y_str, const char* z_str)
	{
		auto left_label_button = [](const char* label, const ImVec2& button_size, const ImVec4& text_color, const ImVec4& bg_color)
			{
				bool clicked = false;

				//PushFont(common::imgui::font::REGULAR);
				PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 5.0f));
				PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

				PushStyleColor(ImGuiCol_Text, text_color);
				PushStyleColor(ImGuiCol_Border, GetColorU32(ImGuiCol_Border));
				PushStyleColor(ImGuiCol_Button, bg_color); // GetColorU32(ImGuiCol_FrameBg)
				PushStyleColor(ImGuiCol_ButtonHovered, bg_color);

				if (ButtonEx(label, button_size, ImGuiButtonFlags_MouseButtonMiddle)) {
					clicked = true;
				}

				PopStyleColor(4);
				PopStyleVar(2);
				//PopFont();

				SameLine();
				SetCursorPosX(GetCursorPosX() - 1.0f);

				return clicked;
			};

		// ---------------
		bool dirty = false;

		PushID(ID);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));

		const float line_height = GetFrameHeight();
		const auto  button_size = ImVec2(line_height - 2.0f, line_height);
		const float widget_spacing = 4.0f;

		//ImVec2 label_size = CalcTextSize(ID, nullptr, true);
		//label_size.x = ImMax(label_size.x, 80.0f);

		const float widget_width_horz = (GetContentRegionAvail().x - 3.0f * button_size.x - 2.0f * widget_spacing -
			(show_label ? label_size + GetStyle().ItemInnerSpacing.x + GetStyle().FramePadding.y : 0.0f)) * 0.333333f;

		/*const float widget_width_vert = (GetContentRegionAvail().x - 3.0f * button_size.x - 2.0f * widget_spacing -
			(show_label ? label_size.x + GetStyle().ItemInnerSpacing.x + GetStyle().FramePadding.y : 0.0f));*/

		const bool  narrow_window = GetWindowWidth() < 440.0f;

		// label if window width < min
		if (narrow_window) {
			SeparatorText(ID);
		}

		// -------
		// -- X --

		if (left_label_button(x_str, button_size, ImVec4(0.84f, 0.55f, 0.53f, 1.0f), ImVec4(0.21f, 0.16f, 0.16f, 1.0f))) {
			vec_in[0] = 0.0f; dirty = true;
		}

		SetNextItemWidth(!narrow_window ? widget_width_horz : -1);
		if (DragFloat("##X", &vec_in[0], speed, min, max, "%.2f")) {
			dirty = true;
		}


		// -------
		// -- Y --

		if (!narrow_window) {
			SameLine(0, widget_spacing);
		}

		if (left_label_button(y_str, button_size, ImVec4(0.73f, 0.78f, 0.5f, 1.0f), ImVec4(0.17f, 0.18f, 0.15f, 1.0f))) {
			vec_in[1] = 0.0f; dirty = true;
		}

		SetNextItemWidth(!narrow_window ? widget_width_horz : -1);
		if (DragFloat("##Y", &vec_in[1], speed, min, max, "%.2f")) {
			dirty = true;
		}

		// -------
		// -- Z --

		if (!narrow_window) {
			SameLine(0, widget_spacing);
		}

		if (left_label_button(z_str, button_size, ImVec4(0.67f, 0.71f, 0.79f, 1.0f), ImVec4(0.18f, 0.21f, 0.23f, 1.0f))) {
			vec_in[2] = 0.0f; dirty = true;
		}

		SetNextItemWidth(!narrow_window ? widget_width_horz : -1);
		if (DragFloat("##Z", &vec_in[2], speed, min, max, "%.2f")) {
			dirty = true;
		}

		PopStyleVar();
		PopID();

		// right label if window width > min 
		if (!narrow_window)
		{
			SameLine(0, GetStyle().ItemInnerSpacing.x);
			TextUnformatted(ID);
		}

		return dirty;
	}

	bool Widget_PrettyStepVec3(const char* ID, float* vec_in, bool show_labels, const float label_size, const float step_amount,
		const char* x_str, const char* y_str, const char* z_str)
	{
		auto left_label_button = [](const char* label, const ImVec2& button_size, const ImVec4& text_color, const ImVec4& bg_color)
			{
				bool clicked = false;

				//PushFont(common::imgui::font::REGULAR);
				PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 5.0f));
				PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

				PushStyleColor(ImGuiCol_Text, text_color);
				PushStyleColor(ImGuiCol_Border, GetColorU32(ImGuiCol_Border));
				PushStyleColor(ImGuiCol_Button, bg_color); // GetColorU32(ImGuiCol_FrameBg)
				PushStyleColor(ImGuiCol_ButtonHovered, bg_color);

				if (ButtonEx(label, button_size, ImGuiButtonFlags_MouseButtonMiddle)) {
					clicked = true;
				}

				PopStyleColor(4);
				PopStyleVar(2);
				//PopFont();

				SameLine();
				SetCursorPosX(GetCursorPosX() - 1.0f);

				return clicked;
			};

		auto& style = GetStyle();

		// ---------------
		bool dirty = false;

		PushID(ID);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));

		const float line_height = GetFrameHeight();
		const auto  button_size = ImVec2(line_height - 2.0f, line_height);
		const float widget_spacing = 4.0f;

		//ImVec2 label_size = CalcTextSize(ID, nullptr, true);
		//label_size.x = ImMax(label_size.x, 80.0f);

		const float widget_width_horz = (GetContentRegionAvail().x - 3.0f * button_size.x - 2.0f * widget_spacing -
			(show_labels ? label_size + /*style.ItemInnerSpacing.x +*/ style.FramePadding.y : style.ItemInnerSpacing.x + style.FramePadding.y)) * 0.333333f;

		const ImVec2 step_button_size = ImVec2(widget_width_horz * 0.5f - widget_spacing - 3.0f, line_height);

		// -------
		// -- X --

		if (left_label_button(x_str, button_size, ImVec4(0.84f, 0.55f, 0.53f, 1.0f), ImVec4(0.21f, 0.16f, 0.16f, 1.0f))) {
			vec_in[0] = 0.0f; dirty = true;
		}

		PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		if (ButtonEx("-##X", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '-', &vec_in[0], &vec_in[0], &step_amount);
			dirty = true;
		}
		SameLine(); SetCursorPosX(GetCursorPosX() - 1.0f);
		if (ButtonEx("+##X", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '+', &vec_in[0], &vec_in[0], &step_amount);
			dirty = true;
		}
		PopItemFlag();

		// -------
		// -- Y --

		SameLine(0, widget_spacing - 1);
		if (left_label_button(y_str, button_size, ImVec4(0.73f, 0.78f, 0.5f, 1.0f), ImVec4(0.17f, 0.18f, 0.15f, 1.0f))) {
			vec_in[1] = 0.0f; dirty = true;
		}

		PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		if (ButtonEx("-##Y", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '-', &vec_in[1], &vec_in[1], &step_amount);
			dirty = true;
		}
		SameLine(); SetCursorPosX(GetCursorPosX() - 1.0f);
		if (ButtonEx("+##Y", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '+', &vec_in[1], &vec_in[1], &step_amount);
			dirty = true;
		}
		PopItemFlag();

		// -------
		// -- Z --

		
		SameLine(0, widget_spacing - 1);
		if (left_label_button(z_str, button_size, ImVec4(0.67f, 0.71f, 0.79f, 1.0f), ImVec4(0.18f, 0.21f, 0.23f, 1.0f))) {
			vec_in[2] = 0.0f; dirty = true;
		}

		PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
		if (ButtonEx("-##Z", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '-', &vec_in[2], &vec_in[2], &step_amount);
			dirty = true;
		}
		SameLine(); SetCursorPosX(GetCursorPosX() - 1.0f);
		if (ButtonEx("+##Z", step_button_size))
		{
			DataTypeApplyOp(ImGuiDataType_Float, '+', &vec_in[2], &vec_in[2], &step_amount);
			dirty = true;
		}
		PopItemFlag();

		PopStyleVar();
		PopID();

		if (show_labels)
		{
			SameLine(0, GetStyle().ItemInnerSpacing.x);
			TextUnformatted(ID);
		}

		return dirty;
	}

	/// Custom Collapsing Header with changeable height - Background color: ImGuiCol_HeaderActive 
	/// @param title_text	Label
	/// @param height		Header Height
	/// @param border_color Border Color
	/// @param default_open	True to collapse by default
	/// @param pre_spacing	8y Spacing in-front of Header
	/// @return				False if Header collapsed
	bool Widget_WrappedCollapsingHeader(const char* title_text, const float height, const ImVec4& border_color, const bool default_open, const bool pre_spacing)
	{
		if (pre_spacing) {
			Spacing(0.0f, 8.0f);
		}

		PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, height));
		PushStyleColor(ImGuiCol_Border, border_color);

		const auto open_flag = default_open ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID storage_id = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasStorageID) ? g.NextItemData.StorageId : window->GetID(title_text);
		const bool is_open = TreeNodeUpdateNextOpen(storage_id, open_flag);

		if (is_open) {
			//PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
			auto header_color_tweak = g.Style.Colors[ImGuiCol_Header];
			header_color_tweak.x += 0.1f;
			header_color_tweak.y += 0.1f;
			header_color_tweak.z += 0.1f;
			header_color_tweak.w += 0.1f;
			PushStyleColor(ImGuiCol_Header, header_color_tweak);
		}

		const auto state = CollapsingHeader(title_text, open_flag | ImGuiTreeNodeFlags_SpanFullWidth);

		if (is_open) {
			PopStyleColor();
		}

		if (IsItemHovered() && IsMouseClicked(ImGuiMouseButton_Middle, false)) {
			SetScrollHereY(0.0f);
		}

		// just toggled
		if (state && is_open != state) {
			//SetScrollHereY(0.0f); 
		}

		PopStyleColor();
		PopStyleVar(2);

		return state;
	}

	float Widget_ContainerWithCollapsingTitle(const char* child_name, const float child_height, const std::function<void()>& callback, const bool default_open, const char* icon, const ImVec4* bg_col, const ImVec4* border_col)
	{
		const std::string child_str = "[ "s + child_name + " ]"s;
		const float child_indent = 2.0f;

		const ImVec4 background_color = bg_col ? *bg_col : ImVec4(0.220f, 0.220f, 0.220f, 0.863f);
		const ImVec4 border_color = border_col ? *border_col : ImVec4(0.099f, 0.099f, 0.099f, 0.901f);

		const auto& style = GetStyle();

		const auto window = GetCurrentWindow();
		const auto min_x = window->WorkRect.Min.x - style.WindowPadding.x * 0.5f + 1.0f;
		const auto max_x = window->WorkRect.Max.x + style.WindowPadding.x * 0.5f - 1.0f;

		PushFont(shared::imgui::font::BOLD);

		const auto spos_pre_header = GetCursorScreenPos();
		const auto expanded = Widget_WrappedCollapsingHeader(child_str.c_str(), 12.0f, border_color, default_open, false);

		PopFont();

		if (icon)
		{
			const auto spos_post_header = GetCursorScreenPos();
			const auto header_dims = GetItemRectSize();
			const auto icon_dims = CalcTextSize(icon);
			SetCursorScreenPos(spos_pre_header + ImVec2(header_dims.x - icon_dims.x - style.WindowPadding.x - 8.0f, header_dims.y * 0.5f - icon_dims.y * 0.5f));
			TextUnformatted(icon);
			SetCursorScreenPos(spos_post_header);
		}

		if (expanded)
		{
			const auto min = ImVec2(min_x, GetCursorScreenPos().y - style.ItemSpacing.y);
			const auto max = ImVec2(max_x, min.y + child_height);

			GetWindowDrawList()->AddRect(min + ImVec2(-1, 1), max + ImVec2(1, 1), ColorConvertFloat4ToU32(border_color), 10.0f, ImDrawFlags_RoundCornersBottom);
			GetWindowDrawList()->AddRectFilled(min, max, ColorConvertFloat4ToU32(background_color), 10.0f, ImDrawFlags_RoundCornersBottom);

			// dropshadow
			{
				const auto dshadow_pmin = GetCursorScreenPos() - ImVec2(GetStyle().WindowPadding.x * 0.5f, 4);
				const auto dshadow_pmax = dshadow_pmin + ImVec2(GetContentRegionAvail().x + GetStyle().WindowPadding.x, 48.0f);

				const auto col_bottom = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.0f));
				const auto col_top = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f));
				GetWindowDrawList()->AddRectFilledMultiColor(dshadow_pmin, dshadow_pmax, col_top, col_top, col_bottom, col_bottom);
			}

			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 4.0f));
			PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6.0f, 8.0f));
			BeginChild(child_name, ImVec2(max.x - min.x - style.FramePadding.x - 2.0f, 0.0f),
				/*ImGuiChildFlags_Borders | */ ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);

			Indent(child_indent);
			PushClipRect(min, max, true);
			if (callback) 
			{
				Indent(4);
				callback();
				Unindent(4);
			}
			PopClipRect();
			Unindent(child_indent);

			EndChild();
			PopStyleVar(2);
		}
		SetCursorScreenPos(GetCursorScreenPos() + ImVec2(0, 8.0f));
		return GetItemRectSize().y + 6.0f/*- 28.0f*/;
	}


	float Widget_ContainerWithDropdownShadow(const float container_height, const std::function<void()>& callback, const ImVec4* bg_col, const ImVec4* border_col)
	{
		const ImVec4 background_color = bg_col ? *bg_col : ImVec4(0.220f, 0.220f, 0.220f, 0.863f);
		const ImVec4 border_color = border_col ? *border_col : ImVec4(0.099f, 0.099f, 0.099f, 0.901f);

		const auto& style = GetStyle();

		const auto window = GetCurrentWindow();
		const auto min_x = window->WorkRect.Min.x - style.WindowPadding.x * 0.5f + 1.0f;
		const auto max_x = window->WorkRect.Max.x + style.WindowPadding.x * 0.5f - 1.0f;

		const auto min = ImVec2(min_x, GetCursorScreenPos().y - style.ItemSpacing.y);
		const auto max = ImVec2(max_x, min.y + container_height);

		GetWindowDrawList()->AddRect(min + ImVec2(-1, -1), max + ImVec2(1, 1), ColorConvertFloat4ToU32(border_color), 10.0f, ImDrawFlags_RoundCornersBottom);
		GetWindowDrawList()->AddRectFilled(min, max, ColorConvertFloat4ToU32(background_color), 10.0f, ImDrawFlags_RoundCornersBottom);

		// dropshadow
		{
			const auto dshadow_pmin = GetCursorScreenPos() - ImVec2(style.WindowPadding.x * 0.5f, 4);
			const auto dshadow_pmax = dshadow_pmin + ImVec2(GetContentRegionAvail().x + style.WindowPadding.x, 48.0f);

			const auto col_bottom = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.0f));
			const auto col_top = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f));
			GetWindowDrawList()->AddRectFilledMultiColor(dshadow_pmin, dshadow_pmax, col_top, col_top, col_bottom, col_bottom);
		}

		Indent(4);
		BeginGroup();
		Spacing(0, 4);
		callback();
		Spacing(0, 4);
		EndGroup();
		Unindent(4);

		return GetItemRectSize().y + 6.0f;
	}

	float Widget_ContainerWithDropdownShadowSquare(const float container_height, const std::function<void()>& callback, const ImVec4* bg_col, const ImVec4* border_col)
	{
		Spacing(0, 6);

		const ImVec4 background_color = bg_col ? *bg_col : ImVec4(0.220f, 0.220f, 0.220f, 0.863f);
		const ImVec4 border_color = border_col ? *border_col : ImVec4(0.099f, 0.099f, 0.099f, 0.901f);

		const auto& style = GetStyle();

		const auto window = GetCurrentWindow();
		const auto min_x = window->WorkRect.Min.x - style.WindowPadding.x * 0.5f + 1.0f;
		const auto max_x = window->WorkRect.Max.x + style.WindowPadding.x * 0.5f - 1.0f;

		const auto min = ImVec2(min_x, GetCursorScreenPos().y - style.ItemSpacing.y);
		const auto max = ImVec2(max_x, min.y + container_height);

		GetWindowDrawList()->AddRect(min + ImVec2(-1, -1), max + ImVec2(1, 1), ColorConvertFloat4ToU32(border_color), 10.0f);
		GetWindowDrawList()->AddRectFilled(min, max, ColorConvertFloat4ToU32(background_color), 10.0f);

		// dropshadow
		{
			const auto dshadow_pmin = GetCursorScreenPos() - ImVec2(style.WindowPadding.x * 0.5f, 4);
			const auto dshadow_pmax = dshadow_pmin + ImVec2(GetContentRegionAvail().x + style.WindowPadding.x, 48.0f);

			const auto col_bottom = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.0f));
			const auto col_top = ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.4f));
			GetWindowDrawList()->AddRectFilledMultiColor(dshadow_pmin, dshadow_pmax, col_top, col_top, col_bottom, col_bottom);
		}

		Indent(4);
		BeginGroup();
		Spacing(0, 4);
		callback();
		Spacing(0, 4);
		EndGroup();
		Unindent(4);

		Spacing(0, 4);

		return GetItemRectSize().y + 6.0f;
	}
}
