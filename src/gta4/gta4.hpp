#pragma once

namespace gta4
{
	void force_graphic_settings();

	void on_begin_scene_cb();
	void main();

	namespace anticull_inspector
	{
		void update();
	}

	namespace game_pause
	{
		void update();
		void on_game_frame();
	}

	extern bool g_installed_signature_patches;
	extern bool g_install_signature_patches_async;
}
