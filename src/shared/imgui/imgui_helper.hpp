#pragma once
#include <array>
#include "font_defines.hpp"

namespace shared::imgui
{
	//bool world2screen(const Vector& in, Vector& out);

	void get_and_add_integers_to_set(char* str, std::unordered_set<std::uint32_t>& set, const std::uint32_t& buf_len = 0u, bool clear_buf = false);
	void get_and_remove_integers_from_set(char* str, std::unordered_set<std::uint32_t>& set, const std::uint32_t& buf_len = 0u, bool clear_buf = false);

	// https://github.com/3r4y/imgui-blur-effect/blob/main/LICENSE
	void draw_window_blur();
	inline std::function draw_window_blur_callback = draw_window_blur;
	void draw_background_blur();

	namespace blur
	{
		template <std::size_t N>
		constexpr auto decode_base85(const char(&input)[N]) noexcept
		{
			std::array<char, N * 4 / 5> out = {};
			constexpr auto decode85_byte = [](const char c) constexpr -> unsigned int { return c >= '\\' ? c - 36 : c - 35; };

			for (std::size_t i = 0, j = 0; i < N - 1; i += 5, j += 4) 
			{
				const unsigned int tmp = decode85_byte(input[i])
						+ 85 * (decode85_byte(input[i + 1]) 
						+ 85 * (decode85_byte(input[i + 2]) 
						+ 85 * (decode85_byte(input[i + 3]) 
						+ 85 *  decode85_byte(input[i + 4]))));

				out[j + 0] = ((tmp >>  0) & 0xFF);
				out[j + 1] = ((tmp >>  8) & 0xFF);
				out[j + 2] = ((tmp >> 16) & 0xFF);
				out[j + 3] = ((tmp >> 24) & 0xFF);
			}

			return out;
		}

		// File: 'blur_x' (600 bytes)
		// Exported using binary_to_compressed_c.cpp
		inline constexpr auto blur_x = decode_base85(
			"%/P:vP>$(#>T$<8?####Q$###%/P:v%####?####$&###J$###h####&####$####t#########.$###%####$####:$########tn=j=$8HlEQ2TuY3l:$#%),##$#########0g'WC"
			"`-.:CVjSuY&5>##%),##$#########C,.33UnSc;'AViF6JrEH<Sk:0+bVe=K&&PDlf1eGdfX1F$*fUCs'el1K>,C5AH3I3b48(#$QUV$)%XHVd;#K7#####X/'.7`7r'7$QUV$*%XHV"
			"d:i[7bmhf6##########D5YY#NSi.L,nHS[D5YY#_9r:Q0=XHVi>uu#^XF0LdfIl[[BA`V&5YY#W.]/Lpu$GV+>uu#PYr.LOV%JLou$GV&5YY#Q`%/Lpv*PV(>uu#Sf./L5hJcLdfIl["
			"(>uu#Rf./L4_/GLdfIl[&5YY#Y.]/Lqu$GV+>uu#RYr.LQV%JLou$GV&5YY#S`%/Lpv*PV(>uu#Uf./L7hJcLdfIl[(>uu#Tf./L6_/GLdfIl[i>uu#_XF0L4_/GL[BA`Vi>uu#`XF0L"
			"5_/GL[BA`Vi>uu#aXF0L6_/GL[BA`Vi>uu#bXF0L7_/GL[BA`V+>uu#W(S/L5_/GLpw0YV+G:;$W(S/L3_/GLpx6cV5_/GL+G:;$V(S/L4_/GLpw0YV5_/GL+G:;$V(S/L7_/GLqv*PV"
			"4_/GL+G:;$U(S/L6_/GLqv*PV4_/GL&5YY#fqF0L3_/GL#),##");

		// File: 'blur_y' (656 bytes)
		// Exported using binary_to_compressed_c.cpp
		inline constexpr auto blur_y = decode_base85(
			"%/P:vP>$(#>T$<8?####Q$###%/P:v%####?####$&###J$###h####&####$####t#########.$###%####$####:$########tn=j=$8HlEQ2TuY3l:$#%),##$#########0g'WC"
			"Qk;nDhpF/#&5>##%),##$#########C,.33UnSc;'AViF6JrEH<Sk:0+bVe=K&&PDlf1eGdfX1F$*fUCs'el1K>,C5AH3I3b48(#$QUV$)%XHVd;#K7NSi.LX/'.7`7r'7$QUV$*%XHV"
			"d:i[7bmhf6##########D5YY#NSi.L,nHS[D5YY#_9r:Q0=XHVi>uu#^XF0LdfIl[[BA`V&5YY#W.]/Lpu$GV+>uu#PYr.LOV%JLou$GV&5YY#Q`%/LP].JL&5YY#PYr.Lpv*PV(>uu#"
			"Rf./L4_/GLdfIl[&5YY#QYr.L)[-S[+G:;$R`%/Lou$GVOV%JL)]3][&5YY#Y.]/Lqu$GV+>uu#Sl7/LQV%JLou$GV&5YY#S`%/LP_:]L&5YY#RYr.Lpv*PV(>uu#Tf./L6_/GLdfIl["
			"&5YY#SYr.L)[-S[+G:;$T`%/Lou$GVQV%JL)]3][i>uu#_XF0L4_/GL[BA`Vi>uu#`XF0L5_/GL[BA`Vi>uu#aXF0L6_/GL[BA`Vi>uu#bXF0L7_/GL[BA`V+>uu#V(S/L4_/GLpw0YV"
			"+G:;$V(S/L3_/GLpx6cV4_/GL+G:;$V(S/L5_/GLpw0YV4_/GL+G:;$V(S/L6_/GLqv*PV4_/GL+G:;$U(S/L7_/GLqv*PV4_/GL&5YY#fqF0L3_/GL#),##");
	}
}

namespace ImGui
{
	void Spacing(const float& x, const float& y);
	void PushFont(shared::imgui::font::FONTS font);
	void TextWrapped_IntegersFromUnorderedSet(const std::unordered_set<std::uint32_t>& set);

	enum Widget_UnorderedSetModifierFlags : std::uint8_t
	{
		Widget_UnorderedSetModifierFlags_Leaf = 0,
		Widget_UnorderedSetModifierFlags_Area = 1 << 0,
	};

	//void Widget_UnorderedSetModifier(const char* id, Widget_UnorderedSetModifierFlags flag, std::unordered_set<std::uint32_t>& set, char* buffer, std::uint32_t buffer_len);

	void Style_DeleteButtonPush();
	void Style_DeleteButtonPop();
	void Style_ColorButtonPush(const ImVec4& base_color, bool black_border = false);
	void Style_ColorButtonPop();
	void Style_InvisibleSelectorPush();
	void Style_InvisibleSelectorPop();

	// #

	void SetItemTooltipBlur(const char* fmt, ...);
	void TableHeadersRowWithTooltip(const char** tooltip_strings);

	// #

	float CalcWidgetWidthForChild(float label_width);
	void CenterText(const char* text, bool disabled = false);
	bool TextUnformatted_ClippedByColumnTooltip(const char* str);

	void Draw3DCircle(ImDrawList* draw_list, const Vector& world_pos, const Vector& normal, float radius, bool filled, const ImColor& color, const float& thickness, int num_points = 200);
	void TableHeaderDropshadow(float height = 12.0f, float max_alpha = 0.6f, float neg_y_offset = 0.0f, float custom_width = 0.0f);

	bool Widget_PrettyDragVec3(const char* ID, float* vec_in, bool show_label = false, float label_size = 80.0f, float speed = 0.25f, float min = -FLT_MAX, float max = FLT_MAX,
		const char* x_str = "X", const char* y_str = "Y", const char* z_str = "Z");
	bool Widget_PrettyStepVec3(const char* ID, float* vec_in, bool show_labels = false, float label_size = 80.0f, float step_amount = 1.0f,
		const char* x_str = "X", const char* y_str = "Y", const char* z_str = "Z");

	bool Widget_WrappedCollapsingHeader(const char* title_text, float height, const ImVec4& border_color, bool default_open = true, bool pre_spacing = false);
	float Widget_ContainerWithCollapsingTitle(const char* child_name, float child_height, const std::function<void()>& callback, bool default_open = true, const char* icon = nullptr, const ImVec4* bg_col = nullptr, const ImVec4* border_col = nullptr);

	float Widget_ContainerWithDropdownShadow(const float container_height, const std::function<void()>& callback, const ImVec4* bg_col = nullptr, const ImVec4* border_col = nullptr);
}
