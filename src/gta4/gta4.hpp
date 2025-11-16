#pragma once

namespace gta4
{
	void force_graphic_settings();

	void on_begin_scene_cb();
	void main();

	extern bool g_installed_signature_patches;
	extern bool g_install_signature_patches_async;
}
