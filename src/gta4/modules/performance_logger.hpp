#pragma once

#include <array>
#include <cstdint>
#include <cfloat>
#include <windows.h>

#include "shared/common/loader.hpp"

namespace gta4
{
	enum class performance_section : std::uint8_t
	{
		BeginScene = 0,
		ImGui,
		RemixVars,
		RemixLights,
		RemixMarkers,
		GameLights,
		AntiCull,
		DrawIndexedPrim,
		RendererApi,
		ApiPassSkip,
		ShaderPassSetup,
		Count
	};

	class performance_logger final : public shared::common::loader::component_module
	{
	public:
		performance_logger();

		static inline performance_logger* p_this = nullptr;
		static performance_logger* get() { return p_this; }

		static bool is_initialized()
		{
			if (const auto mod = get(); mod && mod->m_initialized) {
				return true;
			}
			return false;
		}

		static bool is_logger_enabled() {
			return get()->m_enabled;
		}

		static bool is_section_table_visible() {
			return get()->m_display_section_table;
		}

		struct section_metrics
		{
			double last_ms = 0.0;
			double min_ms = DBL_MAX;
			double max_ms = 0.0;
			double total_ms = 0.0;
			std::uint64_t samples = 0;
		};

		struct display_metrics
		{
			double last_ms = 0.0;
			double avg_ms = 0.0;
			double max_ms = 0.0;
		};

		class scoped_section
		{
		public:
			explicit scoped_section(performance_section section);
			~scoped_section();

		private:
			performance_section m_section;
			LARGE_INTEGER m_start = {};
			bool m_valid = false;
		};

		static const char* section_name(const performance_section section)
		{
			switch (section)
			{
			default:
			case performance_section::BeginScene: return "BeginScene";
			case performance_section::ImGui: return "ImGui";
			case performance_section::RemixVars: return "RemixVars";
			case performance_section::RemixLights: return "RemixLights";
			case performance_section::RemixMarkers: return "RemixMarkers";
			case performance_section::GameLights: return "GameLights";
			case performance_section::AntiCull: return "AntiCull";
			case performance_section::DrawIndexedPrim: return "DrawIndexedPrim";
			case performance_section::RendererApi: return "RendererApi";
			case performance_section::ApiPassSkip: return "ApiPassSkip";
			case performance_section::ShaderPassSetup: return "ShaderPassSetup";
			}
		}

		void begin_frame();
		void end_frame(float frame_time_ms);
		void add_section_time(performance_section section, double ms);
		static void perf_begin(performance_section section);
		static void perf_end(performance_section section);
		void draw_imgui_panel_embedded();
		float get_display_fps();
		void draw_imgui_popout_window();

		bool m_enabled = false;
		bool m_popout_enabled = false;

		float m_spike_hard_ms = 35.0f;
		float m_spike_ratio = 1.5f;
		float m_window_bg_alpha = 0.35f;
		float m_window_width = 600.0f;
		float m_graph_height = 40.0f;
		float m_graph_bg_alpha = 0.50f;
		bool m_display_section_table = false;

	private:
		static constexpr std::size_t FRAME_HISTORY = 256;
		static constexpr std::size_t SECTION_COUNT = static_cast<std::size_t>(performance_section::Count);

		double get_rolling_median() const;
		std::uint64_t get_visible_spike_count() const;
		float get_latest_frame_time_ms() const;
		//static const char* section_name(performance_section section);
		display_metrics get_section_display_metrics(std::size_t section_index) const;
		float get_frame_history_max() const;
		

		LARGE_INTEGER m_qpf = {};
		std::array<section_metrics, SECTION_COUNT> m_sections = {};
		std::array<section_metrics, SECTION_COUNT> m_last_spike_sections = {};
		std::array<float, FRAME_HISTORY> m_frame_history = {};
		std::array<std::array<float, FRAME_HISTORY>, SECTION_COUNT> m_section_history = {};
		std::array<LARGE_INTEGER, SECTION_COUNT> m_section_start = {};
		std::size_t m_frame_history_write = 0;
		std::size_t m_frame_history_count = 0;
		std::uint64_t m_frame_index = 0;
		float m_last_spike_frame_ms = 0.0f;
		float m_last_spike_residual_ms = 0.0f;

		bool m_initialized = false;
	};
}

#define GTA4_PERF_SCOPE(SECTION) ::gta4::performance_logger::scoped_section perf_scope_##__LINE__(SECTION)
#define GTA4_PERF_BEGIN(SECTION) ::gta4::performance_logger::perf_begin(SECTION)
#define GTA4_PERF_END(SECTION) ::gta4::performance_logger::perf_end(SECTION)
