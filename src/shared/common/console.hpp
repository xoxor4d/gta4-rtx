#pragma once

namespace shared::common
{
	inline bool g_external_console_created = false;
    inline void console()
    {
        if (!g_external_console_created)
        {
			g_external_console_created = true;
            
            setvbuf(stdout, nullptr, _IONBF, 0);
            if (AllocConsole())
            {
                FILE* file = nullptr;
                freopen_s(&file, "CONIN$", "r", stdin);
                freopen_s(&file, "CONOUT$", "w", stdout);
                freopen_s(&file, "CONOUT$", "w", stderr);
                SetConsoleTitleA("RTX-Comp Debug Console");
            }

			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			CONSOLE_SCREEN_BUFFER_INFO info;
			GetConsoleScreenBufferInfo(hOut, &info);

			// Desired sizes
			SHORT newWidth = 500;
			SHORT newHeight = std::max((SHORT)(info.srWindow.Bottom + 1), (SHORT)300);

			// Step 1: Shrink window temporarily to avoid SetConsoleScreenBufferSize failure
			SMALL_RECT rect = { 0, 0, 1, 1 };
			SetConsoleWindowInfo(hOut, TRUE, &rect);

			// Step 2: Apply new buffer size
			COORD newSize = { newWidth, newHeight };
			SetConsoleScreenBufferSize(hOut, newSize);

			// Step 3: Resize visible window (optional)
			rect = { 0, 0, (SHORT)(120 - 1), (SHORT)(40 - 1) };
			SetConsoleWindowInfo(hOut, TRUE, &rect);
        }
    }

	inline void set_console_color_red(bool highlight = false)
	{
		if (g_external_console_created) 
		{
			WORD color = FOREGROUND_RED;
			if (highlight) {
				color |= FOREGROUND_INTENSITY;
			}
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		}
	}

	inline void set_console_color_green(bool highlight = false)
	{
		if (g_external_console_created) 
		{
			WORD color = FOREGROUND_GREEN;
			if (highlight) {
				color |= FOREGROUND_INTENSITY;
			}
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		}
	}

	inline void set_console_color_blue(bool highlight = false)
	{
		if (g_external_console_created) 
		{
			WORD color = FOREGROUND_BLUE;
			if (highlight) {
				color |= FOREGROUND_INTENSITY;
			}
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		}
	}

	inline void set_console_color_yellow(bool highlight = false)
	{
		if (g_external_console_created)
		{
			WORD color = FOREGROUND_RED | FOREGROUND_GREEN;
			if (highlight) {
				color |= FOREGROUND_INTENSITY;
			}
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		}
	}

	inline void set_console_color_default(bool highlight = false)
	{
		if (g_external_console_created)
		{
			WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
			if (highlight) {
				color |= FOREGROUND_INTENSITY;
			}
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		}
	}

	enum class LOG_TYPE
	{
		LOG_TYPE_DEFAULT,
		LOG_TYPE_STATUS,
		LOG_TYPE_GREEN,
		LOG_TYPE_WARN,
		LOG_TYPE_ERROR,
    };

	inline void log(const std::string_view& module_str, const std::string_view& msg, LOG_TYPE type = LOG_TYPE::LOG_TYPE_DEFAULT, bool highlight = false, bool newline_infront = false)
	{
		auto colorize = [](const LOG_TYPE& t, const bool h)
			{
				switch (t)
				{
				case LOG_TYPE::LOG_TYPE_DEFAULT:
					set_console_color_default(h);
					break;
				case LOG_TYPE::LOG_TYPE_STATUS:
					set_console_color_blue(h);
					break;
				case LOG_TYPE::LOG_TYPE_GREEN:
					set_console_color_green(h);
					break;
				case LOG_TYPE::LOG_TYPE_WARN:
					set_console_color_yellow(h);
					break;
				case LOG_TYPE::LOG_TYPE_ERROR:
					set_console_color_red(h);
					break;
				default:
					break;
				}
			};
		
		// width of the inner module field
		constexpr int inner_width = 14;

		std::cout << (newline_infront ? "\n" : "")
			<< std::setw(2) << (type == LOG_TYPE::LOG_TYPE_ERROR ? "!" : " ") << "[ ";

		colorize(type, true);
		std::cout << std::format("{:>{}}", module_str, inner_width);
		set_console_color_default();

		std::cout << " ]  ";

		colorize(type, highlight);
		std::cout << msg << '\n';
		set_console_color_default();
	}
}
