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

	inline void print_error(const char* msg)
    {
		console();
		set_console_color_red(true);
		std::cout << msg << "\n";
		set_console_color_default();
    }
}
