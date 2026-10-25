#include "std_include.hpp"
#include "performance_logger.hpp"
#include "shared/imgui/imgui_helper.hpp"

namespace gta4
{
	performance_logger::scoped_section::scoped_section(const performance_section section)
		: m_section(section)
	{
		if (is_initialized() && is_logger_enabled() && is_section_table_visible())
		{
			QueryPerformanceCounter(&m_start);
			m_valid = true;
		}
	}

	performance_logger::scoped_section::~scoped_section()
	{
		if (!m_valid) {
			return;
		}

		const auto h = performance_logger::get();
		if (!is_initialized() || !is_logger_enabled() || !is_section_table_visible() || h->m_qpf.QuadPart <= 0) {
			return;
		}

		LARGE_INTEGER end = {};
		QueryPerformanceCounter(&end);
		const auto ticks = static_cast<double>(end.QuadPart - m_start.QuadPart);
		const auto ms = (ticks * 1000.0) / static_cast<double>(h->m_qpf.QuadPart);
		h->add_section_time(m_section, ms);
	}


	void performance_logger::perf_begin(const performance_section section)
	{
		const auto h = performance_logger::get();
		if (!is_initialized() || !is_logger_enabled() || !is_section_table_visible() || !h) {
			return;
		}

		QueryPerformanceCounter(&h->m_section_start[static_cast<std::size_t>(section)]);
	}

	void performance_logger::perf_end(const performance_section section)
	{
		const auto h = performance_logger::get();
		if (!is_initialized() || !is_logger_enabled() || !is_section_table_visible() || !h || h->m_qpf.QuadPart <= 0) {
			return;
		}

		const auto idx = static_cast<std::size_t>(section);
		const auto start = h->m_section_start[idx].QuadPart;
		if (start <= 0) {
			return;
		}

		LARGE_INTEGER end = {};
		QueryPerformanceCounter(&end);
		const auto ticks = static_cast<double>(end.QuadPart - start);
		const auto ms = (ticks * 1000.0) / static_cast<double>(h->m_qpf.QuadPart);
		h->add_section_time(section, ms);
		h->m_section_start[idx].QuadPart = 0;
	}
	void performance_logger::begin_frame()
	{
		m_enabled = m_popout_enabled;
		if (!m_enabled) {
			return;
		}

		for (auto& s : m_sections) {
			s.last_ms = 0.0;
		}
	}

	void performance_logger::end_frame(const float frame_time_ms)
	{
		if (!is_logger_enabled() || frame_time_ms <= 0.0f) {
			return;
		}

		m_frame_history[m_frame_history_write] = frame_time_ms;
		for (std::size_t i = 0; i < SECTION_COUNT; ++i) {
			m_section_history[i][m_frame_history_write] = static_cast<float>(m_sections[i].last_ms);
		}

		m_frame_history_write = (m_frame_history_write + 1u) % FRAME_HISTORY;
		if (m_frame_history_count < FRAME_HISTORY) {
			++m_frame_history_count;
		}

		++m_frame_index;

		double measured_sum = 0.0;
		for (const auto& s : m_sections) {
			measured_sum += s.last_ms;
		}

		const auto residual_ms = static_cast<float>(std::max(0.0, static_cast<double>(frame_time_ms) - measured_sum));
		const auto median = static_cast<float>(get_rolling_median());
		const auto dynamic_threshold = std::max(m_spike_hard_ms, median * m_spike_ratio);
		const bool is_spike = frame_time_ms >= dynamic_threshold;

		if (is_spike)
		{
			m_last_spike_frame_ms = frame_time_ms;
			m_last_spike_residual_ms = residual_ms;
			m_last_spike_sections = m_sections;
		}
	}

	void performance_logger::add_section_time(const performance_section section, const double ms)
	{
		auto& s = m_sections[static_cast<std::size_t>(section)];
		s.last_ms += ms;
		s.total_ms += ms;
		s.samples++;

		if (ms < s.min_ms) {
			s.min_ms = ms;
		}

		if (ms > s.max_ms) {
			s.max_ms = ms;
		}
	}

	double performance_logger::get_rolling_median() const
	{
		if (!m_frame_history_count) {
			return 0.0;
		}

		std::vector<float> values;
		values.reserve(m_frame_history_count);
		for (std::size_t i = 0; i < m_frame_history_count; ++i) {
			values.push_back(m_frame_history[i]);
		}

		std::sort(values.begin(), values.end());

		const auto mid = values.size() / 2;
		if ((values.size() & 1u) == 0u) {
			return (values[mid - 1] + values[mid]) * 0.5;
		}

		return values[mid];
	}

	std::uint64_t performance_logger::get_visible_spike_count() const
	{
		if (!m_frame_history_count) {
			return 0;
		}

		const auto median = static_cast<float>(get_rolling_median());
		const auto dynamic_threshold = std::max(m_spike_hard_ms, median * m_spike_ratio);

		std::uint64_t count = 0;
		for (std::size_t i = 0; i < m_frame_history_count; ++i)
		{
			if (m_frame_history[i] >= dynamic_threshold) {
				++count;
			}
		}

		return count;
	}

	float performance_logger::get_latest_frame_time_ms() const
	{
		if (!m_frame_history_count) {
			return 0.0f;
		}

		const std::size_t idx = (m_frame_history_write + FRAME_HISTORY - 1u) % FRAME_HISTORY;
		return m_frame_history[idx];
	}

	void performance_logger::draw_imgui_panel_embedded()
	{
		ImGui::Style_ColorButtonPush(m_popout_enabled ? ImVec4(0.72f, 0.5f, 0.26f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button), true);
		if (ImGui::Button(m_popout_enabled ? "Hide Performance Overlay" : "Show Performance Overlay", ImVec2(ImGui::GetContentRegionAvail().x, 48))) {
			m_popout_enabled = !m_popout_enabled;
		}
		ImGui::Style_ColorButtonPop();

		if constexpr (shared::common::profiler::enabled) {
			ImGui::TextUnformatted(shared::common::profiler::is_connected() ? "Tracy: profiler connected" : "Tracy: waiting for a profiler connection");
		}

		if (ImGui::TreeNode("Performance Overlay Settings ..."))
		{
			ImGui::Spacing(0, 4);
			ImGui::SliderFloat("Hard Threshold (ms)", &m_spike_hard_ms, 8.0f, 100.0f, "%.1f");
			ImGui::SliderFloat("Median Ratio", &m_spike_ratio, 1.1f, 3.0f, "%.2f");
			ImGui::Spacing(0, 4);

			ImGui::SliderFloat("Window Alpha", &m_window_bg_alpha, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Graph Alpha", &m_graph_bg_alpha, 0.0f, 1.0f, "%.2f");
			ImGui::Spacing(0, 4);

			ImGui::SliderFloat("Window X", &m_window_x, 0.0f, 5000.0f, "%.2f");
			ImGui::SliderFloat("Window Y", &m_window_y, 0.0f, 5000.0f, "%.2f");
			ImGui::SliderFloat("Window Width", &m_window_width, 360.0f, 5120.0f, "%.2f");
			ImGui::SliderFloat("Graph Height", &m_graph_height, 16.0f, 90.0f, "%.2f");
			ImGui::Checkbox("Display Section Table", &m_display_section_table);
			ImGui::TreePop();
		}
	}

	float performance_logger::get_display_fps()
	{
		static float displayed_fps = 0.0f;
		static float time_sum_ms = 0.0f;
		static int frame_count = 0;

		const float frame_ms = get_latest_frame_time_ms();
		if (frame_ms <= 0.0f) {
			return displayed_fps;
		}

		time_sum_ms += frame_ms;
		++frame_count;

		if (time_sum_ms >= 250.0f) // update ~4 times/sec from the mean frame time over the window
		{
			displayed_fps = static_cast<float>(frame_count) * 1000.0f / time_sum_ms;
			time_sum_ms = 0.0f;
			frame_count = 0;
		}

		return displayed_fps;
	}

	void performance_logger::draw_imgui_popout_window()
	{
		if (!m_popout_enabled) {
			return;
		}

		const ImGuiWindowFlags flags = 
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;

		ImGui::SetNextWindowBgAlpha(m_window_bg_alpha);
		ImGui::SetNextWindowPos(ImVec2(m_window_x, m_window_y), ImGuiCond_Always);
		if (ImGui::Begin("Performance Overlay", &m_popout_enabled, flags))
		{
			ImGui::PushID("overlay");
			const auto values_count = static_cast<int>(m_frame_history_count);
			const auto graph_scale_max = get_frame_history_max();
			const auto graph_size = ImVec2(m_window_width, m_graph_height);

			if (values_count > 0)
			{
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.02f, 0.02f, 0.02f, m_graph_bg_alpha));
				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				ImGui::PlotLines("##FrameTimeGraph", m_frame_history.data(), values_count, static_cast<int>(m_frame_history_write), nullptr, 0.0f, graph_scale_max, graph_size);
				ImGui::PopStyleColor(2);

				const ImVec2 rect_min = ImGui::GetItemRectMin();
				const ImVec2 rect_max = ImGui::GetItemRectMax();

				char scale_top[32] = {};
				char scale_bottom[32] = {};
				snprintf(scale_top, sizeof(scale_top), "%.1f ms", graph_scale_max);
				snprintf(scale_bottom, sizeof(scale_bottom), "0.0 ms");

				const auto scale_color = ImGui::GetColorU32(ImGuiCol_Text);
				const auto bottom_size = ImGui::CalcTextSize(scale_bottom);
				draw_list->AddText(ImVec2(rect_max.x - ImGui::CalcTextSize(scale_top).x - 4.0f, rect_min.y + 2.0f), scale_color, scale_top);
				draw_list->AddText(ImVec2(rect_max.x - bottom_size.x - 4.0f, rect_max.y - bottom_size.y - 2.0f), scale_color, scale_bottom);
			}

			ImGui::PushFont(shared::imgui::font::BOLD_LARGE);
			ImGui::Text("Spikes: %llu  --  Last Spike: %.2f ms (Residual %.2f ms)", get_visible_spike_count(), m_last_spike_frame_ms, m_last_spike_residual_ms);
			ImGui::Text("FPS: %.2f", get_display_fps());
			ImGui::PopFont();

			if (m_display_section_table)
			{
				if (ImGui::BeginTable("performance_sections_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Section");
					ImGui::TableSetupColumn("Last");
					ImGui::TableSetupColumn("Avg");
					ImGui::TableSetupColumn("Max");
					ImGui::TableHeadersRow();

					for (std::size_t i = 0; i < SECTION_COUNT; ++i)
					{
						const auto s = get_section_display_metrics(i);
						ImGui::TableNextRow();
						ImGui::TableNextColumn(); ImGui::TextUnformatted(section_name(static_cast<performance_section>(i)));
						ImGui::TableNextColumn(); ImGui::Text("%.3f", s.last_ms);
						ImGui::TableNextColumn(); ImGui::Text("%.3f", s.avg_ms);
						ImGui::TableNextColumn(); ImGui::Text("%.3f", s.max_ms);
					}

					ImGui::EndTable();
				}
			}

			ImGui::PopID();
		}
		ImGui::End();
	}

	

	performance_logger::display_metrics performance_logger::get_section_display_metrics(const std::size_t section_index) const
	{
		display_metrics out = {};
		if (section_index >= SECTION_COUNT || m_frame_history_count == 0) {
			return out;
		}

		out.last_ms = m_sections[section_index].last_ms;

		double sum = 0.0;
		double max = 0.0;
		for (std::size_t i = 0; i < m_frame_history_count; ++i)
		{
			const auto v = static_cast<double>(m_section_history[section_index][i]);
			sum += v;
			max = std::max(max, v);
		}

		out.avg_ms = sum / static_cast<double>(m_frame_history_count);
		out.max_ms = max;
		return out;
	}

	float performance_logger::get_frame_history_max() const
	{
		float max = 1.0f;

		for (std::size_t i = 0; i < m_frame_history_count; ++i) {
			max = std::max(max, m_frame_history[i]);
		}

		return max + 0.5f;
	}

	performance_logger::performance_logger()
	{
		p_this = this;

		// -----
		QueryPerformanceFrequency(&m_qpf);

		m_initialized = true;
		shared::common::log("PerfLogger", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}
}
