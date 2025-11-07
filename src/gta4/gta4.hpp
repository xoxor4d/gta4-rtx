#pragma once

namespace gta4
{
	void on_begin_scene_cb();
	void translate_and_apply_timecycle_settings();
	void main();

	extern int g_is_rendering_static;
	extern int g_is_rendering_vehicle;
	extern bool g_is_rendering_phone;

	extern bool g_rendered_first_primitive;

	extern bool g_installed_signature_patches;
	extern bool g_install_signature_patches_async;
}
