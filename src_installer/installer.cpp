#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <filesystem>
#include <iostream>

std::string open_file_dialog()
{
	char filename[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "GTA IV Executable\0GTAIV.exe\0All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Select your GTAIV.exe";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrInitialDir = nullptr;

	if (GetOpenFileNameA(&ofn)) {
		return filename;
	}
		

	return "";
}

bool file_exists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool extract_zip(const std::string & zip_path, const std::string & target_dir, const std::string & inner_folder = "")
{
	mz_zip_archive zip = {};
	if (!mz_zip_reader_init_file(&zip, zip_path.c_str(), 0)) {
		return false;
	}

	bool result = true;
	mz_uint file_count = mz_zip_reader_get_num_files(&zip);
	
	for (mz_uint i = 0u; i < file_count; i++)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
			continue;
		}

		auto entry_path = std::filesystem::path(stat.m_filename);
		if (!inner_folder.empty())
		{
			std::filesystem::path inner(inner_folder);
			if (!entry_path.native().starts_with(inner.native())) {
				continue;
			}
			entry_path = entry_path.lexically_relative(inner);
		}

		if (stat.m_is_directory) {
			continue;
		}

		std::filesystem::path out_path = std::filesystem::path(target_dir) / entry_path;
		
		// Validate output path to prevent directory traversal
		std::filesystem::path canonical_target = std::filesystem::canonical(std::filesystem::path(target_dir));
		std::filesystem::path canonical_output = std::filesystem::absolute(out_path);
		if (!canonical_output.native().starts_with(canonical_target.native())) {
			continue; // Skip paths outside target directory
		}

		try {
			create_directories(out_path.parent_path());
		} catch (const std::exception&) {
			MessageBoxA(nullptr, ("Failed to create directory: " + out_path.parent_path().string()).c_str(), "Error", MB_ICONERROR);
			result = false;
			continue;
		}

		if (i > 0 && (i % 30 == 0)) {
			Sleep(10);
		}

		if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0))
		{
			MessageBoxA(nullptr, ("Failed to extract: " + std::string(stat.m_filename)).c_str(), "Error", MB_ICONERROR);
			result = false;
		}
	}

	mz_zip_reader_end(&zip);
	return result;
}

int main()
{
	Sleep(200);
	
	std::cout << "Select the GTAIV directory by selecting your GTAIV.exe ...\n";
	Sleep(500);

	// select GTAIV.exe
    std::string gtaiv_exe_path = open_file_dialog();
	if (gtaiv_exe_path.empty()) 
	{
		std::cout << "Path invalid. Exiting ...\n";
		return 0;
	}

    const std::string game_dir = std::filesystem::path(gtaiv_exe_path).parent_path().string();
	
	// Validate game directory exists
	if (!std::filesystem::exists(game_dir) || !std::filesystem::is_directory(game_dir)) {
		MessageBoxA(nullptr, "Invalid game directory selected.", "Error", MB_ICONERROR);
		return 1;
	}
	
	std::cout << "Using Path: '" << game_dir << "'\n\n";
	std::cout << "Checking for FusionFix presence ...\n";
	Sleep(500);


    // check if any FusionFix version exists
    const bool has_fusion_fix = file_exists(game_dir + "\\d3d9.dll") &&
								file_exists(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi");

	// check if original FusionFix version exists
	const bool has_original_fusion_fix = has_fusion_fix && file_exists(game_dir + "\\vulkan.dll");

	// check if comp mod and remix are installed -> update
	const bool has_remix_comp_mod = file_exists(game_dir + "\\d3d9.dll") &&
									file_exists(game_dir + "\\a_gta4-rtx.asi");

	if (has_remix_comp_mod) {
		std::cout << "Detected another version of the RTX Remix Compatibility Mod. Updating ... \n";
	}

    bool opt_install_fusion_fix_fork = false;
    if (has_original_fusion_fix)
    {
		const auto res = MessageBoxA(nullptr, "FusionFix detected. Replace with a fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
        opt_install_fusion_fix_fork = (res == IDYES);

		if (!opt_install_fusion_fix_fork) {
			std::cout << "Not replacing installed FusionFix version. This might lead to issues.\n\n";
		} else {
			std::cout << "Installing RTXRemix FusionFix Fork.\n\n";
		}
    }
    else
    {
		if (!has_remix_comp_mod || !has_fusion_fix)
		{
			const auto res = MessageBoxA(nullptr, "Install FusionFix fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
			opt_install_fusion_fix_fork = res == IDYES;
			std::cout << (opt_install_fusion_fix_fork ? "Installing FusionFix RTXRemix Fork." : "Not installing FusionFix RTXRemix Fork.") << "\n\n";
		}
    }

	// ask for fullscreen / windowed
	bool fullscreen = true;

	if (!has_remix_comp_mod)
	{
		if (const auto res = MessageBoxA(nullptr, "Setup GTA IV to run in fullscreen/borderless mode?\n(Choose No if you want to run the game in windowed mode)", "Display mode", MB_YESNO | MB_ICONQUESTION))
		{
			if (res == IDNO) {
				fullscreen = false;
			}
			else {
				std::cout << "If you are having trouble with launching the game in fullscreen:\n> Go into 'rtx_comp/game_settings.toml'\n> Set 'manual_game_resolution_enabled' to 'true'\n> Set your desired resolution via 'manual_game_resolution'\n\n";
			}
		}

		// steam launch args warning
		MessageBoxA(nullptr, "Make sure to remove ALL launch arguments from Steam properties for GTA IV!\n", "IMPORTANT", MB_OK | MB_ICONWARNING);
	}

	// extract comp files
	static const wchar_t* zip_prefix = L"GTAIV-Remix-CompatibilityMod";
	std::filesystem::path found_zip;

	auto installer_path = []()
		{
			wchar_t buf[MAX_PATH] = { 0 };
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf).parent_path();
		};

	for (const auto& entry : std::filesystem::directory_iterator(installer_path()))
	{
		if (!entry.is_regular_file()) {
			continue;
		}

		const auto& p = entry.path();

		if (p.extension() == L".zip" && p.stem().wstring().starts_with(zip_prefix))
		{
			found_zip = p;
			break;  // take the first match
		}
	}

	if (found_zip.empty()) 
	{
		std::cout << "[ERR] Could not find any zip starting with 'GTAIV-Remix-CompatibilityMod'.\n";
		MessageBoxA(nullptr, "Could not find 'GTAIV-Remix-CompatibilityMod.zip' in the installer directory.", "Error", MB_ICONERROR);
		return 1;
	}
	
	// Validate zip file exists and is readable
	if (!std::filesystem::exists(found_zip) || !std::filesystem::is_regular_file(found_zip)) 
	{
		std::cout << "[ERR] Found zip file but it is not accessible: " << found_zip.string() << "\n";
		MessageBoxA(nullptr, "The zip file found is not accessible.", "Error", MB_ICONERROR);
		return 1;
	}

	std::cout << "Extracting zip ...\n";
	Sleep(100); // Small delay before extraction

	if (!extract_zip(found_zip.string(), game_dir, "GTAIV-Remix-CompatibilityMod"))
	{
		std::cout << "[ERR] Failed to extract 'GTAIV-Remix-CompatibilityMod' files from 'GTAIV-Remix-CompatibilityMod.zip'\n";
		std::cout << "> Aborting installation. Please extract files manually.\n";
		return 0;
	}

	Sleep(100); // Small delay between extractions

	if (!has_remix_comp_mod)
	{
		// extract fullscreen or windowed files
		std::string windowed_or_fullscreen_path = fullscreen ? "_installer_options/mode_fullscreen/" : "_installer_options/mode_windowed/";
		if (!extract_zip(found_zip.string(), game_dir, windowed_or_fullscreen_path)) {
			std::cout << "[ERR] Failed to extract '" << windowed_or_fullscreen_path << "' files from 'GTAIV-Remix-CompatibilityMod.zip'\n";
		}

		Sleep(100); // Small delay before next operation
	}

    // install FusionFix fork if requested
    if (opt_install_fusion_fix_fork)
    {
		Sleep(100); // Small delay before FusionFix installation
        if (has_original_fusion_fix) 
		{
			if (MoveFileExA(
				(game_dir + "\\vulkan.dll").c_str(),
				(game_dir + "\\vulkan.dll.originalFF").c_str(),
				MOVEFILE_REPLACE_EXISTING))
			{
				std::cout << "Renamed 'vulkan.dll' to 'vulkan.dll.originalFF'\n";
			}
			Sleep(25);

			if (MoveFileExA(
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi").c_str(),
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi.originalFF").c_str(),
				MOVEFILE_REPLACE_EXISTING))
			{
				std::cout << "Renamed 'GTAIV.EFLC.FusionFix.asi' to 'GTAIV.EFLC.FusionFix.asi.originalFF'\n";
			}
			Sleep(25);

			if (MoveFileExA(
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg").c_str(),
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg.originalFF").c_str(),
				MOVEFILE_REPLACE_EXISTING))
			{
				std::cout << "Renamed 'GTAIV.EFLC.FusionFix.cfg' to 'GTAIV.EFLC.FusionFix.cfg.originalFF'\n";
			}
			Sleep(25);

			if (MoveFileExA(
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini").c_str(),
				(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini.originalFF").c_str(),
				MOVEFILE_REPLACE_EXISTING))
			{
				std::cout << "Renamed 'GTAIV.EFLC.FusionFix.ini' to 'GTAIV.EFLC.FusionFix.ini.originalFF'\n";
			}
			Sleep(25);
        }

        if (!extract_zip(found_zip.string(), game_dir, "_installer_options/FusionFix_RTXRemixFork/")) {
			std::cout << "[ERR] Failed to extract '_installer_options/FusionFix_RTXRemixFork/' files from 'GTAIV-Remix-CompatibilityMod.zip'\n";
        }
    }

	if (!has_remix_comp_mod)
	{
		// DX9 June 2010 runtime
		if (MessageBoxA(nullptr, "It's recommended to install Microsoft DirectX June 2010 Redistributable.\nDo you want to open a link to the installer?\n(https://www.microsoft.com/en-us/download/details.aspx?id=8109)", "DirectX Runtime", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			ShellExecuteA(nullptr, "open", "https://www.microsoft.com/en-us/download/details.aspx?id=8109", nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	std::cout << "If you run into issues, please create an issue on the GitHub repository.\n> Please include the external console log when the game starts\n> The log files from 'rtx-remix/logs'\n> A short description and anything else that might help to identify the issue.\n";

	MessageBoxA(nullptr, "Installation complete!\nYou can now launch GTA IV.", "Success", MB_ICONINFORMATION);
    return 0;
}