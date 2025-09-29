#pragma once

#define TOML_ERROR(TITLE, ENTRY, MSG, ...) \
	shared::common::set_console_color_red(true); \
	shared::common::console(); std::cout << toml::format_error(toml::make_error_info(#TITLE, (ENTRY), shared::utils::va(#MSG, __VA_ARGS__))) << std::endl; \
	shared::common::set_console_color_default(); \

namespace shared::common::toml
{
	
}