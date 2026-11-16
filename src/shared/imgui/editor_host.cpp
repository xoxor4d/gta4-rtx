#include "std_include.hpp"
#include "editor_host.hpp"

#include "shared/globals.hpp"

namespace shared::imgui::editor_host
{
	namespace
	{
		std::vector<editor_s>& editors()
		{
			static std::vector<editor_s> list;
			return list;
		}
	}

	void register_editor(const editor_s& editor)
	{
		if (editor.is_open && editor.draw) {
			editors().emplace_back(editor);
		}
	}

	bool any_open()
	{
		for (const auto& editor : editors())
		{
			if (editor.is_open()) {
				return true;
			}
		}

		return false;
	}

	void sync_open_state() {
		globals::imgui_editor_open = any_open();
	}

	void draw()
	{
		for (const auto& editor : editors())
		{
			if (editor.is_open()) {
				editor.draw();
			}
		}
	}

	bool allows_game_input_on_rmb()
	{
		bool allowed = false;
		for (const auto& editor : editors())
		{
			if (!editor.is_open()) {
				continue;
			}

			if (!editor.allows_game_input_on_rmb || !editor.allows_game_input_on_rmb()) {
				return false;
			}

			allowed = true;
		}

		return allowed;
	}
}
