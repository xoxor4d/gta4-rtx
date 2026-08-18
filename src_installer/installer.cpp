#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <conio.h>

#pragma comment(lib, "winhttp.lib")

static const char* line_pad = "  ";
#define PAD line_pad
#define PAD_INL "  "

inline void log_red(bool highlight = false)
{
	WORD color = FOREGROUND_RED;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

inline void log_green(bool highlight = false)
{
	WORD color = FOREGROUND_GREEN;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_blue(bool highlight = false)
{
	WORD color = FOREGROUND_BLUE;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_yellow(bool highlight = false)
{
	WORD color = FOREGROUND_RED | FOREGROUND_GREEN;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_default(bool highlight = false)
{
	WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_error(const std::string& msg)
{
	log_red(true);
	std::cout << "\n" << PAD << "[!] " << msg << "\n";
	log_default();
}

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

bool file_exists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Initialize zip archive from file path
bool init_zip_from_file(const std::filesystem::path& zip_path, mz_zip_archive& zip)
{
	// Convert to string for miniz API (miniz uses char* paths)
	std::string zip_path_str = zip_path.string();
	return mz_zip_reader_init_file(&zip, zip_path_str.c_str(), 0) == MZ_TRUE;
}

std::string read_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip)
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip)) {
		return "";
	}

	size_t file_size = 0;
	void* file_data = mz_zip_reader_extract_file_to_heap(&zip, file_path_in_zip.c_str(), &file_size, 0);
	
	std::string result;
	if (file_data && file_size > 0) 
	{
		result.assign(static_cast<const char*>(file_data), file_size);
		mz_free(file_data);
	}

	mz_zip_reader_end(&zip);
	return result;
}

std::string read_file_from_disk(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (!file.is_open()) {
		return "";
	}
	
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return content;
}

// Trim whitespace from string
std::string trim_whitespace(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\n\r");
	if (first == std::string::npos) {
		return "";
	}

	size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, (last - first + 1));
}

// Parse version string like "1.1.8" into a vector of integers
std::vector<int> parse_version(const std::string& version_str)
{
	std::vector<int> parts;
	std::string current;
	
	for (char c : version_str) 
	{
		if (c == '.') 
		{
			if (!current.empty()) 
			{
				try {
					parts.push_back(std::stoi(current));
				} catch (...) {
					parts.push_back(0);
				}
				current.clear();
			}
		} 
		else if (isdigit(c)) {
			current += c;
		}
	}
	
	if (!current.empty()) 
	{
		try {
			parts.push_back(std::stoi(current));
		} catch (...) {
			parts.push_back(0);
		}
	}
	
	return parts;
}

// Compare two version vectors, returns true if v1 > v2
bool version_greater_than(const std::vector<int>& v1, const std::vector<int>& v2)
{
	size_t max_len = max(v1.size(), v2.size());
	
	for (size_t i = 0; i < max_len; i++) 
	{
		int a = (i < v1.size()) ? v1[i] : 0;
		int b = (i < v2.size()) ? v2[i] : 0;
		
		if (a > b) return true;
		if (a < b) return false;
	}
	
	return false; // equal
}

// Get short SHA (first 7 characters) from full SHA
std::string get_short_sha(const std::string& full_sha)
{
	if (full_sha.length() >= 7) {
		return full_sha.substr(0, 7);
	}
	return full_sha;
}

// Get the latest commit SHA from a GitHub repository
// Returns empty string on failure, or the commit SHA on success
std::string get_latest_github_commit_sha(const std::string& owner, const std::string& repo, const std::string& branch = "main")
{
	// GitHub API endpoint: https://api.github.com/repos/{owner}/{repo}/commits/{branch}
	std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/commits/" + branch;
	
	HINTERNET session = WinHttpOpen(L"GTAIV-Remix-Installer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		return "";
	}

	// Convert URL to wide string
	std::wstring wurl(url.begin(), url.end());
	HINTERNET connect = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	
	if (!connect) {

		WinHttpCloseHandle(session);
		return "";
	}

	// Convert path to wide string
	std::wstring wpath = L"/repos/" + std::wstring(owner.begin(), owner.end()) + L"/" + std::wstring(repo.begin(), repo.end()) + L"/commits/" + std::wstring(branch.begin(), branch.end());
	HINTERNET request = WinHttpOpenRequest(connect, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	
	if (!request) 
	{
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Add User-Agent header (GitHub API requires this)
	WinHttpAddRequestHeaders(request, L"User-Agent: GTAIV-Remix-Installer", -1, WINHTTP_ADDREQ_FLAG_ADD);

	// Send request
	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Receive response
	if (!WinHttpReceiveResponse(request, nullptr))
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Read response data
	std::string response;
	DWORD bytes_available = 0;
	DWORD bytes_read = 0;
	char buffer[4096] = { 0 };

	do 
	{
		if (!WinHttpQueryDataAvailable(request, &bytes_available)) {
			break;
		}

		if (bytes_available == 0) {
			break;
		}

		if (!WinHttpReadData(request, buffer, sizeof(buffer) - 1, &bytes_read)) {
			break;
		}

		if (bytes_read > 0) 
		{
			buffer[bytes_read] = '\0';
			response += buffer;
		}
	} while (bytes_read > 0);

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);

	// Parse JSON response to extract SHA
	// GitHub API returns: {"sha":"abc123...","commit":{...},...}
	size_t sha_pos = response.find("\"sha\":\"");
	if (sha_pos == std::string::npos) {
		return "";
	}

	sha_pos += 7; // Skip past "sha":"
	size_t sha_end = response.find("\"", sha_pos);

	if (sha_end == std::string::npos) {
		return "";
	}

	return response.substr(sha_pos, sha_end - sha_pos);
}

// Compare local commit SHA file with GitHub latest commit
// Returns: -1 if local is newer/unknown, 0 if same, 1 if GitHub is newer
// Returns -2 on error (file doesn't exist, network error, etc.)
int compare_commit_sha(const std::string& local_sha_file_path, const std::string& github_owner, const std::string& github_repo, const std::string& branch = "main")
{
	// Read local SHA
	std::string local_sha = read_file_from_disk(local_sha_file_path);

	if (local_sha.empty()) {
		return -2; // File doesn't exist or is empty
	}

	local_sha = trim_whitespace(local_sha);

	// Get GitHub SHA
	std::string github_sha = get_latest_github_commit_sha(github_owner, github_repo, branch);

	if (github_sha.empty()) {
		return -2; // Network error or API failure
	}

	github_sha = trim_whitespace(github_sha);

	// Compare (case-insensitive)
	std::string local_lower = local_sha;
	std::string github_lower = github_sha;
	std::transform(local_lower.begin(), local_lower.end(), local_lower.begin(), ::tolower);
	std::transform(github_lower.begin(), github_lower.end(), github_lower.begin(), ::tolower);

	if (local_lower == github_lower) {
		return 0; // Same commit
	}

	// For simplicity, if they differ, assume GitHub is newer
	// (In a real scenario, you'd need to query commit dates or use Git API to determine order)
	return 1; // GitHub is newer (or different)
}

// Write string content to a file
bool write_file_to_disk(const std::string& file_path, const std::string& content)
{
	std::ofstream file(file_path);
	if (!file.is_open()) {
		return false;
	}

	file << content;
	return file.good();
}

// Download a file from URL to a local path with progress and speed display
bool download_file_to_path(const std::wstring& url, const std::filesystem::path& target_path)
{
	// Parse URL
	std::wstring host, path;
	size_t protocol_end = url.find(L"://");
	if (protocol_end == std::wstring::npos) {
		return false;
	}
	
	size_t host_start = protocol_end + 3;
	size_t path_start = url.find(L"/", host_start);

	if (path_start == std::wstring::npos) 
	{
		host = url.substr(host_start);
		path = L"/";
	} 
	else 
	{
		host = url.substr(host_start, path_start - host_start);
		path = url.substr(path_start);
	}

	HINTERNET session = WinHttpOpen(L"GTAIV-Remix-Installer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		return false;
	}

	HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!connect) 
	{
		WinHttpCloseHandle(session);
		return false;
	}

	// Make the GET request to download the file
	HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!request)
	{
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Enable redirect following
	DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

	// Add User-Agent header
	WinHttpAddRequestHeaders(request, L"User-Agent: GTAIV-Remix-Installer", -1, WINHTTP_ADDREQ_FLAG_ADD);

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Create target directory if needed
	try {
		std::filesystem::create_directories(target_path.parent_path());
	} 
	catch (...) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Open output file
	std::ofstream out_file(target_path, std::ios::binary);
	if (!out_file.is_open()) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Read and write data with progress tracking
	DWORD bytes_available = 0;
	DWORD bytes_read = 0;
	char buffer[8192] = { 0 };
	bool success = true;
	
	ULONGLONG total_bytes_downloaded = 0;
	DWORD start_time = GetTickCount();
	DWORD last_update_time = start_time;

	log_yellow(true);
	do 
	{
		if (!WinHttpQueryDataAvailable(request, &bytes_available)) 
		{
			success = false;
			break;
		}

		if (bytes_available == 0) {
			break;
		}

		if (!WinHttpReadData(request, buffer, sizeof(buffer), &bytes_read)) 
		{
			success = false;
			break;
		}

		if (bytes_read > 0) 
		{
			out_file.write(buffer, bytes_read);
			if (!out_file.good()) 
			{
				success = false;
				break;
			}
			
			total_bytes_downloaded += bytes_read;
			DWORD current_time = GetTickCount();
			
			// Update progress every 500ms
			if (current_time - last_update_time >= 500) 
			{
				// Calculate speed (bytes per second)
				DWORD elapsed = current_time - start_time;
				float speed = 0.0f;

				if (elapsed > 0) {
					speed = (float)total_bytes_downloaded / (float)elapsed * 1000.0f; // bytes per second
				}

				float speed_MBps = speed / (1024.0f * 1024.0f);
				float downloaded_MB = (float)total_bytes_downloaded / (1024.0f * 1024.0f);
				
				// Display downloaded size and speed
				std::cout << "\r"
					<< PAD << "Downloading: " << std::fixed << std::setprecision(1) << downloaded_MB << " MB - " << speed_MBps << " MB/s    ";

				std::cout.flush();
				
				last_update_time = current_time;
			}
		}
	} while (bytes_read > 0);

	log_default();

	// Final progress update
	if (success && total_bytes_downloaded > 0)
	{
		float downloadedMB = (float)total_bytes_downloaded / (1024.0f * 1024.0f);
		DWORD elapsed = GetTickCount() - start_time;
		float speed = 0.0f;

		if (elapsed > 0) {
			speed = (float)total_bytes_downloaded / (float)elapsed * 1000.0f;
		}

		float speed_MBps = speed / (1024.0f * 1024.0f);

		std::cout << "\r"
			<< PAD << "Downloaded: " << std::fixed << std::setprecision(1) << downloadedMB << " MB - " << speed_MBps << " MB/s    \n";

		std::cout.flush();
	}

	out_file.close();
	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);

	return success;
}

bool extract_single_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip, const std::filesystem::path& target_path)
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip)) {
		return false;
	}

	// Find the file in the zip
	int file_index = mz_zip_reader_locate_file(&zip, file_path_in_zip.c_str(), nullptr, 0);
	if (file_index < 0) 
	{
		mz_zip_reader_end(&zip);
		return false;
	}

	// Extract directly to file (more efficient for large files)
	std::string target_path_str = target_path.string();
	if (mz_zip_reader_extract_to_file(&zip, file_index, target_path_str.c_str(), 0)) 
	{
		mz_zip_reader_end(&zip);
		return true;
	}

	mz_zip_reader_end(&zip);
	return false;
}

bool extract_zip(const std::filesystem::path& zip_path, const std::string& target_dir, const std::string& inner_folder = "")
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip))
	{
		log_error("Failed to init zip."); 
		return false;
	}

	bool result = true;
	mz_uint file_count = mz_zip_reader_get_num_files(&zip);

	if (!file_count) {
		log_error("Failed to get total amount of files in zip.");
	}

	for (mz_uint i = 0u; i < file_count; i++)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&zip, i, &stat)) 
		{
			log_error("Failed to read file info " + std::to_string(i) + "/" + std::to_string(file_count));
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
		}
		catch (const std::exception&) 
		{
			log_error("Failed to create directory: " + out_path.parent_path().string());
			result = false;
			continue;
		}

		if (i > 0 && (i % 30 == 0)) {
			Sleep(10);
		}

		if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0))
		{
			const mz_zip_error error = mz_zip_get_last_error(&zip);
			const char* error_string = mz_zip_get_error_string(error);

			std::ostringstream log;
			log << "Failed to extract ZIP entry\n"
				<< "  index: " << i << "\n"
				<< "  archive entry: " << stat.m_filename << "\n"
				<< "  output path: " << out_path.string() << "\n"
				<< "  miniz error: " << static_cast<int>(error)
				<< " (" << (error_string ? error_string : "unknown") << ")\n"
				<< "  compressed size: " << stat.m_comp_size << "\n"
				<< "  uncompressed size: " << stat.m_uncomp_size << "\n"
				<< "  compression method: " << stat.m_method << "\n"
				<< "  flags: 0x" << std::hex << stat.m_bit_flag << std::dec << "\n"
				<< "  CRC32: 0x" << std::hex << stat.m_crc32 << std::dec << "\n"
				<< "  encrypted: " << (stat.m_is_encrypted ? "yes" : "no") << "\n"
				<< "  supported: " << (stat.m_is_supported ? "yes" : "no");

			log_error(log.str());
			result = false;
		}
	}

	mz_zip_reader_end(&zip);
	return result;
}

std::filesystem::path get_installer_dir()
{
	wchar_t buf[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	return std::filesystem::path(buf).parent_path();
}

std::string quote_arg(const std::string& arg)
{
	std::string quoted = "\"";
	for (char c : arg)
	{
		if (c == '"') {
			quoted += "\\\"";
		} else {
			quoted += c;
		}
	}

	quoted += "\"";
	return quoted;
}

bool run_process(const std::string& command, const std::filesystem::path& working_dir, DWORD* exit_code = nullptr, bool capture_output = false, bool quiet = false)
{
	STARTUPINFOA si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);

	std::string mutable_command = command;
	std::string working_dir_str = working_dir.empty() ? "" : working_dir.string();
	char* cwd = working_dir_str.empty() ? nullptr : working_dir_str.data();

	HANDLE read_pipe = nullptr;
	HANDLE write_pipe = nullptr;

	if (capture_output)
	{
		SECURITY_ATTRIBUTES sa = {};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;

		if (CreatePipe(&read_pipe, &write_pipe, &sa, 0))
		{
			SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
			si.dwFlags |= STARTF_USESTDHANDLES;
			si.hStdOutput = write_pipe;
			si.hStdError = write_pipe;
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		}
		else {
			capture_output = false;
		}
	}

	if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, 0, nullptr, cwd, &si, &pi))
	{
		if (exit_code) {
			*exit_code = GetLastError();
		}

		if (read_pipe) {
			CloseHandle(read_pipe);
		}
		if (write_pipe) {
			CloseHandle(write_pipe);
		}

		return false;
	}

	if (capture_output)
	{
		// Close our copy of the write end so reads finish when the child exits
		CloseHandle(write_pipe);
		write_pipe = nullptr;

		bool at_line_start = true;
		char buffer[4096];
		DWORD bytes_read = 0;

		while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0)
		{
			if (quiet) {
				continue;
			}

			for (DWORD i = 0; i < bytes_read; i++)
			{
				const char c = buffer[i];

				// Pad the first visible character of each line/redraw
				if (at_line_start && c != '\n' && c != '\r') {
					std::cout << PAD;
					at_line_start = false;
				}

				std::cout << c;

				if (c == '\n' || c == '\r') {
					at_line_start = true;
				}
			}

			std::cout.flush();
		}

		CloseHandle(read_pipe);
		read_pipe = nullptr;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	if (exit_code) {
		*exit_code = code;
	}

	return code == 0;
}

int select_console_option(const std::string& prompt, const std::vector<std::string>& options)
{
	if (options.empty()) {
		return -1;
	}

	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD old_mode = 0;
	bool restore_mode = GetConsoleMode(input, &old_mode);

	if (restore_mode) {
		SetConsoleMode(input, old_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
	}

	log_default(true);
	std::cout << PAD << prompt << "\n";
	log_default(false);

	CONSOLE_SCREEN_BUFFER_INFO csbi = {};
	GetConsoleScreenBufferInfo(output, &csbi);
	COORD start = csbi.dwCursorPosition;
	int selected = 0;

	auto render = [&]()
	{
		SetConsoleCursorPosition(output, start);

		for (size_t i = 0; i < options.size(); i++) {
			std::cout << PAD << " - [" << (selected == (int)i ? "x" : " ") << "] " << options[i] << "\n";
		}

		log_blue(true);
		std::cout << "\n" << PAD << " >>> Use Up/Down and Enter to select.                      \n";
		log_default();
		std::cout.flush();
	};

	render();

	while (true)
	{
		int key = _getch();

		if (key == 13) {
			break;
		}

		if (key == 224 || key == 0)
		{
			key = _getch();

			if (key == 72) {
				selected = (selected + (int)options.size() - 1) % (int)options.size();
			} else if (key == 80) {
				selected = (selected + 1) % (int)options.size();
			}

			render();
		}
	}

	if (restore_mode) {
		SetConsoleMode(input, old_mode);
	}

	std::cout << "\n";
	return selected;
}

enum class DeliveryMode
{
	Zip,
	Git
};

struct RemoteModConfig
{
	std::string display_name;
	std::string repo_owner;
	std::string repo_name;
	std::string branch;
	std::string repo_url;
	std::string zip_url;
	std::string zip_inner_mods_github;
	std::string zip_inner_mods_flat;
	std::string mod_folder_name;
	std::string commit_file_name;
	std::string delivery_file_name;
	bool required = false;
	std::string missing_prompt;
	std::string missing_description;
	std::string update_prompt;
};

std::filesystem::path remote_mods_dir(const std::string& game_dir) {
	return std::filesystem::path(game_dir) / "rtx-remix" / "mods";
}

std::filesystem::path remote_mod_target_dir(const std::string& game_dir, const RemoteModConfig& config) {
	return remote_mods_dir(game_dir) / config.mod_folder_name;
}

std::filesystem::path remote_mod_commit_file(const std::string& game_dir, const RemoteModConfig& config) {
	return remote_mods_dir(game_dir) / config.commit_file_name;
}

std::filesystem::path remote_mod_delivery_file(const std::string& game_dir, const RemoteModConfig& config) {
	return remote_mods_dir(game_dir) / config.delivery_file_name;
}

std::filesystem::path remote_mod_git_dir(const std::string& game_dir, const RemoteModConfig& config) {
	return std::filesystem::path(game_dir) / "rtx-remix" / ".installer_git" / (config.repo_name + ".git");
}

bool directory_has_entries(const std::filesystem::path& dir)
{
	if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
		return false;
	}

	return std::filesystem::directory_iterator(dir) != std::filesystem::directory_iterator();
}

bool sparse_git_metadata_exists(const std::string& game_dir, const RemoteModConfig& config) {
	return std::filesystem::exists(remote_mod_git_dir(game_dir, config) / "config");
}

std::string delivery_mode_to_string(DeliveryMode mode) {
	return mode == DeliveryMode::Git ? "git" : "zip";
}

bool read_delivery_mode(const std::string& game_dir, const RemoteModConfig& config, DeliveryMode& mode)
{
	const std::string content = trim_whitespace(read_file_from_disk(remote_mod_delivery_file(game_dir, config).string()));

	if (content == "delivery=git" || content == "git")
	{
		mode = DeliveryMode::Git;
		return true;
	}

	if (content == "delivery=zip" || content == "zip")
	{
		mode = DeliveryMode::Zip;
		return true;
	}

	return false;
}

bool write_delivery_mode(const std::string& game_dir, const RemoteModConfig& config, DeliveryMode mode)
{
	return write_file_to_disk(remote_mod_delivery_file(game_dir, config).string(), "delivery=" + delivery_mode_to_string(mode) + "\n");
}

DeliveryMode select_delivery_mode()
{
	const int choice = select_console_option(
		"\n  Select how the base and optional AutoPBR mods should be installed/updated:",
		{
			"Use Git checkout (RECOMMENDED, only download new/modified files without redownloading everything.",
			"Use GitHub zip downloads (Downloads the complete mod even if only a single file has changed since the installer was last run.)"
		});

	return choice == 0 ? DeliveryMode::Git : DeliveryMode::Zip;
}

bool ensure_mingit_available(std::filesystem::path& git_exe)
{
	const std::filesystem::path mingit_dir = get_installer_dir() / "mingit";
	git_exe = mingit_dir / "cmd" / "git.exe";

	if (std::filesystem::exists(git_exe)) {
		return true;
	}

	static const wchar_t* mingit_url = L"https://github.com/git-for-windows/git/releases/download/v2.54.0.windows.1/MinGit-2.54.0-64-bit.zip";
	const std::filesystem::path mingit_zip = get_installer_dir() / "MinGit-2.54.0-64-bit.zip";

	log_yellow(true);
	std::cout << "\n" << PAD << "MinGit was not found next to the installer. Downloading standalone Git ...\n";
	log_default();

	if (!download_file_to_path(mingit_url, mingit_zip))
	{
		log_error("Failed to download MinGit. Git install/update mode cannot continue.");
		return false;
	}

	log_yellow(true);
	std::cout << "\n" << PAD << "Extracting MinGit ...\n\n";
	log_default();

	try {
		std::filesystem::create_directories(mingit_dir);
	} catch (...)
	{
		log_error("Failed to create MinGit extraction folder.");
		return false;
	}

	if (!extract_zip(mingit_zip, mingit_dir.string()))
	{
		log_error("Failed to extract MinGit. Git install/update mode cannot continue.");
		return false;
	}

	if (!std::filesystem::exists(git_exe))
	{
		log_error("MinGit was extracted, but cmd\\git.exe was not found.");
		return false;
	}

	return true;
}

bool run_git(const std::filesystem::path& git_exe, const std::filesystem::path& git_dir, const std::filesystem::path& work_tree, const std::string& args, DWORD* exit_code = nullptr, bool quiet = false)
{
	std::string command = quote_arg(git_exe.string()) +
		" --git-dir=" + quote_arg(git_dir.string()) +
		" --work-tree=" + quote_arg(work_tree.string()) +
		" " + args;

	return run_process(command, work_tree, exit_code, true, quiet);
}

bool git_has_head(const std::filesystem::path& git_exe, const std::filesystem::path& git_dir, const std::filesystem::path& work_tree)
{
	DWORD code = 1;
	run_git(git_exe, git_dir, work_tree, "rev-parse --verify HEAD", &code, true);
	return code == 0;
}

bool git_path_has_local_changes(const std::filesystem::path& git_exe, const std::filesystem::path& git_dir, const std::filesystem::path& work_tree, const std::string& sparse_path)
{
	DWORD code = 0;

	run_git(git_exe, git_dir, work_tree, "diff --quiet -- " + quote_arg(sparse_path), &code, true);
	if (code != 0) {
		return true;
	}

	run_git(git_exe, git_dir, work_tree, "diff --cached --quiet -- " + quote_arg(sparse_path), &code, true);
	return code != 0;
}

bool ensure_sparse_git_repo(const std::filesystem::path& git_exe, const std::string& game_dir, const RemoteModConfig& config)
{
	const std::filesystem::path work_tree = std::filesystem::path(game_dir) / "rtx-remix";
	const std::filesystem::path git_dir = remote_mod_git_dir(game_dir, config);

	try 
	{
		std::filesystem::create_directories(work_tree);
		std::filesystem::create_directories(git_dir);
		std::filesystem::create_directories(git_dir / "info");
	} catch (...) 
	{
		log_error("Failed to create Git metadata folders.");
		return false;
	}

	if (!std::filesystem::exists(git_dir / "config"))
	{
		std::string command = quote_arg(git_exe.string()) + " --git-dir=" + quote_arg(git_dir.string()) + " init --initial-branch=installer";
		if (!run_process(command, work_tree, nullptr, true)) 
		{
			log_error("Failed to initialize Git metadata for " + config.display_name + ".");
			return false;
		}
	}

	run_git(git_exe, git_dir, work_tree, "config advice.defaultBranchName false", nullptr, true);
	run_git(git_exe, git_dir, work_tree, "config core.worktree " + quote_arg(work_tree.string()));
	run_git(git_exe, git_dir, work_tree, "config core.sparseCheckout true");
	run_git(git_exe, git_dir, work_tree, "config core.sparseCheckoutCone false");

	const std::string sparse_path = "mods/" + config.mod_folder_name;
	if (!write_file_to_disk((git_dir / "info" / "sparse-checkout").string(), "/" + sparse_path + "/\n"))
	{
		log_error("Failed to write sparse checkout configuration.");
		return false;
	}

	DWORD code = 0;
	run_git(git_exe, git_dir, work_tree, "remote get-url origin", &code, true);

	if (code == 0) {
		run_git(git_exe, git_dir, work_tree, "remote set-url origin " + quote_arg(config.repo_url));
	} else {
		run_git(git_exe, git_dir, work_tree, "remote add origin " + quote_arg(config.repo_url));
	}

	return true;
}

bool install_or_update_git_mod(const std::string& game_dir, const RemoteModConfig& config, const std::string& latest_sha, bool replace_existing_confirmed = false)
{
	std::filesystem::path git_exe;
	if (!ensure_mingit_available(git_exe)) {
		return false;
	}

	if (!ensure_sparse_git_repo(git_exe, game_dir, config)) {
		return false;
	}

	const std::filesystem::path work_tree = std::filesystem::path(game_dir) / "rtx-remix";
	const std::filesystem::path git_dir = remote_mod_git_dir(game_dir, config);
	const std::string sparse_path = "mods/" + config.mod_folder_name;
	const std::filesystem::path target_dir = remote_mod_target_dir(game_dir, config);

	bool force_update = false;
	const bool has_head = git_has_head(git_exe, git_dir, work_tree);

	if (!has_head && directory_has_entries(target_dir))
	{
		std::string message = config.mod_folder_name + " already exists.\n\n";
					message += "Replace this folder so Git can manage future updates?";

		const int res = replace_existing_confirmed ? IDYES : MessageBoxA(nullptr, message.c_str(), (config.display_name + " Existing Files").c_str(), MB_YESNO | MB_ICONQUESTION);

		if (res != IDYES)
		{
			std::cout << PAD << "Skipping " << config.display_name << " Git install because existing files were kept.\n\n";
			return false;
		}

		try {
			std::filesystem::remove_all(target_dir);
		} catch (...) 
		{
			log_error("Failed to clear existing " + config.mod_folder_name + " folder.");
			return false;
		}

		force_update = true;
	}

	if (has_head && git_path_has_local_changes(git_exe, git_dir, work_tree, sparse_path))
	{
		std::string message = "Local changes were detected in " + config.mod_folder_name + ".\n\n";
					message += "Force update and overwrite Git-managed files for this mod?";

		const int res = MessageBoxA(nullptr, message.c_str(), (config.display_name + " Local Changes").c_str(), MB_YESNO | MB_ICONQUESTION);

		if (res != IDYES)
		{
			std::cout << PAD << "Skipping " << config.display_name << " update because local changes were detected.\n\n";
			return false;
		}

		force_update = true;
	}

	std::cout << "\n" << PAD << "Fetching " << config.display_name << " via Git sparse checkout ...\n";
	if (!run_git(git_exe, git_dir, work_tree, "fetch --progress --depth=1 origin " + quote_arg(config.branch)))
	{
		log_error("Failed to fetch " + config.display_name + " from GitHub.");
		return false;
	}

	const std::string checkout_args = std::string("checkout --progress ") + (force_update ? "-f " : "") + "-B installer FETCH_HEAD";
	if (!run_git(git_exe, git_dir, work_tree, checkout_args))
	{
		std::string message = "Git could not update " + config.display_name + ".\n\n";
					message += "Force update and overwrite Git-managed files for this mod?";

		const int res = MessageBoxA(nullptr, message.c_str(), (config.display_name + " Update Failed").c_str(), MB_YESNO | MB_ICONQUESTION);

		if (res != IDYES) {
			return false;
		}

		if (!run_git(git_exe, git_dir, work_tree, "checkout --progress -f -B installer FETCH_HEAD"))
		{
			log_error("Force update failed for " + config.display_name + ".");
			return false;
		}
	}

	if (!write_file_to_disk(remote_mod_commit_file(game_dir, config).string(), latest_sha)) 
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Failed to update commit file.\n\n";
		log_default();
	}

	if (!write_delivery_mode(game_dir, config, DeliveryMode::Git)) 
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Failed to write delivery marker.\n\n";
		log_default();
	}

	log_default(true);
	std::cout << "\n" << PAD << "> Updated " << config.display_name << " via Git.\n\n";
	log_default(false);
	return true;
}

bool download_and_extract_zip_mod(const std::string& game_dir, const RemoteModConfig& config, const std::string& sha)
{
	const std::filesystem::path mods_dir = remote_mods_dir(game_dir);
	const std::string short_sha = get_short_sha(sha);
	const std::filesystem::path zip_path = get_installer_dir() / (config.repo_name + "-" + short_sha + ".zip");

	std::cout << "\n" << PAD << "Downloading " << config.display_name << " zip to:\n" << PAD << "> " << zip_path.string() << "\n\n";

	std::wstring zip_url_w(config.zip_url.begin(), config.zip_url.end());
	if (!download_file_to_path(zip_url_w, zip_path))
	{
		log_error(
			"Download failed.\n"
			PAD_INL "Manually download from:\n" PAD_INL + config.zip_url + "\n\n"
			PAD_INL "Extract contents to:\n" PAD_INL + mods_dir.string());

		MessageBoxA(nullptr,
					("Failed to download " + config.display_name + ".\n\nPlease try again or proceed manually.\nCheck the console for more details.").c_str(),
					"Error",
					MB_ICONERROR);

		return false;
	}

	log_yellow(true);
	std::cout << "\n" << PAD << "Extracting " << config.display_name << " into rtx-remix/mods ...\n";
	log_default();

	bool ok_extract = extract_zip(zip_path, mods_dir.string(), config.zip_inner_mods_github);

	if (!ok_extract) {
		ok_extract = extract_zip(zip_path, mods_dir.string(), config.zip_inner_mods_flat);
	}

	if (!ok_extract)
	{
		log_error(
			"Failed to extract " + config.display_name + ".\n"
			PAD_INL "You can extract it manually from:\n" PAD_INL + zip_path.string() + "\n\n"
			PAD_INL "Extract contents to:\n" PAD_INL + mods_dir.string());

		MessageBoxA(nullptr,
					("[!] Failed to extract " + config.display_name + ".\n\nCheck the console for more details.\n").c_str(),
					"Error",
					MB_ICONERROR);

		return false;
	}

	if (!write_file_to_disk(remote_mod_commit_file(game_dir, config).string(), sha)) 
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Failed to update commit file.\n\n";
		log_default();
	}

	if (!write_delivery_mode(game_dir, config, DeliveryMode::Zip)) 
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Failed to write delivery marker.\n\n";
		log_default();
	}

	std::cout << PAD << "> Done!\n\n";
	return true;
}

bool install_or_update_remote_mod(const std::string& game_dir, const RemoteModConfig& config, DeliveryMode selected_mode, bool selected_mode_valid)
{
	const std::filesystem::path mods_dir = remote_mods_dir(game_dir);
	const std::filesystem::path commit_file = remote_mod_commit_file(game_dir, config);
	const std::filesystem::path target_dir = remote_mod_target_dir(game_dir, config);

	try {
		std::filesystem::create_directories(mods_dir);
	} catch (...) 
	{
		log_error("Failed to create rtx-remix\\mods.");
		return false;
	}

	log_default(true);
	std::cout << "\n" << PAD << "----------- " << config.display_name << " ----------------------------------------------- \n";
	log_default(false);

	DeliveryMode mode = selected_mode;
	if (!selected_mode_valid && !read_delivery_mode(game_dir, config, mode)) {
		mode = select_delivery_mode();
	}

	if (const bool commit_file_exists = std::filesystem::exists(commit_file) && std::filesystem::is_regular_file(commit_file); 
		!commit_file_exists)
	{
		log_blue(true);
		std::cout << PAD << config.missing_prompt << "\n";
		log_default();

		std::cout
			<< PAD << config.missing_description << "\n\n"
			<< PAD << "Repo:\n" << PAD << "> " << config.repo_url << "\n\n"
			<< PAD << "This will install into:\n" << PAD << "> " << target_dir.string() << "\n";

		const int user_choice = MessageBoxA(nullptr, config.missing_prompt.c_str(), config.display_name.c_str(), MB_YESNO | MB_ICONQUESTION);
		if (user_choice != IDYES)
		{
			if (config.required)
			{
				MessageBoxA(nullptr,
							"The base-remix-mod is required for the game to function properly.\n\n"
							"Make sure to install it via the installer or manually.",
							"(See release notes on GitHub)",
							MB_OK);
			} else {
				std::cout << PAD << "Skipping " << config.display_name << " installation.\n\n";
			}

			return false;
		}

		std::cout << PAD << "Fetching latest commit SHA from GitHub...\n\n";
		const auto latest_sha = get_latest_github_commit_sha(config.repo_owner, config.repo_name, config.branch);

		if (latest_sha.empty())
		{
			log_error("Failed to fetch commit SHA from GitHub. Network error or API failure.");
			return false;
		}

		if (mode == DeliveryMode::Git) {
			return install_or_update_git_mod(game_dir, config, latest_sha);
		}

		return download_and_extract_zip_mod(game_dir, config, latest_sha);
	}

	// Offer to migrate an existing zip install to Git management
	bool convert_zip_to_git = false;
	if (mode == DeliveryMode::Zip)
	{
		const int convert_choice = MessageBoxA(nullptr,
			(config.display_name + " is currently installed via zip download.\n\n"
			"Convert it to be Git-managed so future updates only download changed files?").c_str(),
			(config.display_name + " - Convert to Git").c_str(),
			MB_YESNO | MB_ICONQUESTION);

		if (convert_choice == IDYES)
		{
			mode = DeliveryMode::Git;
			convert_zip_to_git = true;
			std::cout << PAD << "Converting " << config.display_name << " to Git management ...\n";
		}
	}

	std::cout << "\n" << PAD << "Checking latest " << config.repo_name << " commit SHA...\n";
	int comparison = compare_commit_sha(commit_file.string(), config.repo_owner, config.repo_name, config.branch);

	if (comparison == 0)
	{
		std::cout << PAD << "Installed " << config.display_name << " matches GitHub (up to date).\n";

		if (mode == DeliveryMode::Git &&
			(!sparse_git_metadata_exists(game_dir, config) || !std::filesystem::exists(target_dir)))
		{
			const std::string latest_sha = trim_whitespace(read_file_from_disk(commit_file.string()));
			if (!latest_sha.empty()) {
				return install_or_update_git_mod(game_dir, config, latest_sha, convert_zip_to_git);
			}
		}

		write_delivery_mode(game_dir, config, mode);
		return true;
	}

	if (comparison == -2)
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Could not check " << config.display_name << " commit SHA (network error or file issue).\n\n";
		log_default();
		return false;
	}

	std::string local_sha = trim_whitespace(read_file_from_disk(commit_file.string()));
	std::string latest_sha = get_latest_github_commit_sha(config.repo_owner, config.repo_name, config.branch);

	if (latest_sha.empty())
	{
		log_yellow(true);
		std::cout << PAD << "[WARN] Could not fetch latest commit SHA for comparison.\n\n";
		log_default();
		return false;
	}

	std::cout << "\n"
		<< PAD << "A newer version of the " << config.repo_name << " is available on GitHub.\n"
		<< PAD << "Current commit: " + (local_sha.empty() ? "Unknown" : local_sha) + "\n"
		<< PAD << "Latest commit: " + latest_sha + "\n"
		<< PAD << "Repo: " + config.repo_url + "\n\n";

	const int user_choice = MessageBoxA(nullptr, config.update_prompt.c_str(), (config.display_name + " Update Available").c_str(), MB_YESNO | MB_ICONQUESTION);
	if (user_choice != IDYES)
	{
		std::cout << PAD << "Skipping " << config.display_name << " update.\n\n";
		return true;
	}

	if (mode == DeliveryMode::Git) {
		return install_or_update_git_mod(game_dir, config, latest_sha, convert_zip_to_git);
	}

	return download_and_extract_zip_mod(game_dir, config, latest_sha);
}

void setup_console_window()
{
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info = {};

	if (!GetConsoleScreenBufferInfo(output, &info)) {
		return;
	}

	const SHORT width = 146;
	const SHORT height = 40;
	const SHORT buffer_height = std::max<SHORT>(height, info.dwSize.Y);

	SMALL_RECT small_window = { 0, 0, 1, 1 };
	SetConsoleWindowInfo(output, TRUE, &small_window);

	COORD buffer_size = { width, buffer_height };
	SetConsoleScreenBufferSize(output, buffer_size);

	SMALL_RECT window = { 0, 0, width - 1, height - 1 };
	SetConsoleWindowInfo(output, TRUE, &window);
}

int main()
{
	setup_console_window();

	std::cout << "\n";
	Sleep(200);

	log_default(true);
	std::cout << PAD << "Grand Theft Auto IV - RTX Remix Compatibility Mod Installer (github.com/xoxor4d/gta4-rtx)\n";
	log_default();
	std::cout << PAD << "- Make sure you've placed both the installer and the 'GTAIV-Remix-CompatibilityMod-X.X.X.zip' into the same folder.\n";
	std::cout << PAD << "- If installation fails, refer to the release page on GitHub for manual install instructions.\n\n\n";

	std::cout << PAD << "Select your GTAIV.exe inside the GTAIV directory to continue ...\n";
	Sleep(500);

	// select GTAIV.exe
    std::string gtaiv_exe_path = open_file_dialog();
	if (gtaiv_exe_path.empty()) 
	{
		log_error("Invalid Path. Exiting ...");
		MessageBoxA(nullptr, "Something went wrong", "Error", MB_ICONERROR);
		return 0;
	}

    const std::string game_dir = std::filesystem::path(gtaiv_exe_path).parent_path().string();
	
	// Validate game directory exists
	if (!std::filesystem::exists(game_dir) || !std::filesystem::is_directory(game_dir)) 
	{
		MessageBoxA(nullptr, "Invalid game directory selected.", "Error", MB_ICONERROR);
		return 1;
	}
	
	std::cout << PAD << "> Using Path: '" << game_dir << "'\n\n\n";

	log_blue(true);
	std::cout << PAD << "Note: The installer can only auto-update remix assets (textures/meshes).\n";
	std::cout << PAD << "To update logic, you'll need to provide a newer 'GTAIV-Remix-CompatibilityMod-X.X.X.zip' file.\n\n";
	log_default();

	bool has_remix_comp_mod = file_exists(game_dir + "\\d3d9.dll") && file_exists(game_dir + "\\a_gta4-rtx.asi");

	bool update_remote_mods_only = false;
	if (has_remix_comp_mod)
	{
		const int reinstall_choice = select_console_option(
			"RTX Remix Compatibility Mod was already detected. What do you want to do?",
			{
				"Full reinstall of the compatibility mod, base mod, and optional AutoPBR mod",
				"Only check for updates for the base remix mod and optional AutoPBR mod"
			});

		update_remote_mods_only = reinstall_choice == 1;
	}

	bool skip_rtx_comp_install = update_remote_mods_only;
	if (!skip_rtx_comp_install)
	{
		// Find zip file first (needed for version comparison)
		static const wchar_t* zip_prefix = L"GTAIV-Remix-CompatibilityMod";
		static const std::string zip_prefix_str = "GTAIV-Remix-CompatibilityMod";
		std::filesystem::path found_zip;

		// Collect all matching zip files with their versions
		std::vector<std::pair<std::filesystem::path, std::vector<int>>> zip_candidates;

		for (const auto& entry : std::filesystem::directory_iterator(get_installer_dir()))
		{
			if (!entry.is_regular_file()) {
				continue;
			}

			const auto& p = entry.path();
			if (p.extension() == L".zip" && p.stem().wstring().starts_with(zip_prefix))
			{
				// Extract version from filename: GTAIV-Remix-CompatibilityMod-X.Y.Z.zip
				std::string stem = p.stem().string();
				std::string version_str;

				if (stem.length() > zip_prefix_str.length())
				{
					// Skip the prefix and any leading dash
					version_str = stem.substr(zip_prefix_str.length());
					if (!version_str.empty() && version_str[0] == '-') {
						version_str = version_str.substr(1);
					}
				}

				std::vector<int> version = parse_version(version_str);
				zip_candidates.push_back({ p, version });
			}
		}

		// Find the zip with the highest version
		if (!zip_candidates.empty())
		{
			found_zip = zip_candidates[0].first;
			std::vector<int> best_version = zip_candidates[0].second;

			for (size_t i = 1; i < zip_candidates.size(); i++)
			{
				if (version_greater_than(zip_candidates[i].second, best_version))
				{
					found_zip = zip_candidates[i].first;
					best_version = zip_candidates[i].second;
				}
			}

			std::cout << PAD << "Using compatibility mod zip: " << found_zip.filename().string() << "\n";
		}

		if (found_zip.empty())
		{
			log_error("Could not find any zip starting with 'GTAIV-Remix-CompatibilityMod'.");
			std::cout << PAD << "Installation might be incomplete. Download and place the .zip next to the installer or install manually.\n";
			std::cout << PAD << "Checking for Remix Base Mod updates ...\n\n";
			skip_rtx_comp_install = true;
		}

		// Validate zip file exists and is readable
		if (!std::filesystem::exists(found_zip) || !std::filesystem::is_regular_file(found_zip))
		{
			log_error("Found zip file but it is not accessible: " + found_zip.string());
			MessageBoxA(nullptr, "The zip file found is not accessible.", "Error", MB_ICONERROR);
			return 1;
		}

		std::cout << PAD << "Checking for FusionFix presence ...\n";
		Sleep(500);


		// check if any FusionFix version exists
		const bool has_fusion_fix = file_exists(game_dir + "\\d3d9.dll") && file_exists(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi");

		// check if original FusionFix version exists
		const bool has_original_fusion_fix = has_fusion_fix && file_exists(game_dir + "\\vulkan.dll");

		// check if comp mod and remix are installed -> update
		has_remix_comp_mod = file_exists(game_dir + "\\d3d9.dll") && file_exists(game_dir + "\\a_gta4-rtx.asi");

		// check if RTXRemix FusionFix fork marker exists
		const bool has_rtxremix_fusionfix_marker = file_exists(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.RTXRemix.txt");

		if (has_remix_comp_mod) {
			std::cout << PAD << "> Detected another version of the RTX Remix Compatibility Mod. Updating ... \n";
		}

		bool opt_install_fusion_fix_fork = false;

		// If RTXRemix FusionFix marker exists, compare versions
		if (has_rtxremix_fusionfix_marker)
		{
			// Read version from existing marker file
			std::string existing_version = read_file_from_disk(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.RTXRemix.txt");

			// Read version from zip file
			std::string zip_version = read_file_from_zip(found_zip, "_installer_options/FusionFix_RTXRemixFork/plugins/GTAIV.EFLC.FusionFix.RTXRemix.txt");

			// Trim whitespace from both versions
			auto trim = [](std::string& s)
				{
					s.erase(0, s.find_first_not_of(" \t\n\r"));
					s.erase(s.find_last_not_of(" \t\n\r") + 1);
				};

			trim(existing_version);
			trim(zip_version);

			if (existing_version == zip_version)
			{
				// Versions match, skip update
				opt_install_fusion_fix_fork = false;
				std::cout << PAD << "> RTXRemix FusionFix fork is up to date (version: " << existing_version << "). Skipping update.\n\n";
			}
			else
			{
				opt_install_fusion_fix_fork = true; 
				std::cout << PAD << "> Updating RTXRemix FusionFix fork from " << existing_version << " to " << zip_version << ".\n\n";
			}
		}
		else if (has_original_fusion_fix)
		{
			const auto res = MessageBoxA(nullptr, "FusionFix detected. Replace with a fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
			opt_install_fusion_fix_fork = (res == IDYES);

			if (!opt_install_fusion_fix_fork) {
				log_error("Not replacing installed FusionFix version. This might lead to issues.\n");
			} else {
				std::cout << PAD << "Installing RTXRemix FusionFix Fork.\n\n";
			}
		}
		else
		{
			// If user has FusionFix but not original FusionFix and no marker, they likely have RTXRemix fork already
			// Extract the marker file from the zip so future updates can detect it
			if (has_remix_comp_mod && !has_rtxremix_fusionfix_marker && !has_original_fusion_fix && has_fusion_fix)
			{
				// Ensure plugins directory exists
				const std::string plugins_dir = game_dir + "\\plugins";
				std::filesystem::create_directories(plugins_dir);

				// Extract the marker file from the zip
				const std::string marker_path = plugins_dir + "\\GTAIV.EFLC.FusionFix.RTXRemix.txt";
				if (extract_single_file_from_zip(found_zip, "_installer_options/FusionFix_RTXRemixFork/plugins/GTAIV.EFLC.FusionFix.RTXRemix.txt", marker_path))
				{
					log_blue(true);
					std::cout << PAD << "Extracted RTXRemix FusionFix marker file.\n\n";
					log_default();
				}
				else
				{
					log_yellow(true);
					std::cout << PAD << "[WARN] Failed to extract RTXRemix FusionFix marker file.\n\n";
					log_default();
				}

				opt_install_fusion_fix_fork = true;
			}

			if (!has_remix_comp_mod || !has_fusion_fix)
			{
				const auto res = MessageBoxA(nullptr, "Install FusionFix fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
				opt_install_fusion_fix_fork = res == IDYES;
				std::cout << PAD << (opt_install_fusion_fix_fork ? "Installing FusionFix RTXRemix Fork." : "Not installing FusionFix RTXRemix Fork.") << "\n\n";
			}
		}

		// ask for fullscreen / windowed
		bool fullscreen = true;

		// Only ask about display mode and Steam args if this is a fresh install (a_gta4-rtx.asi doesn't exist)
		if (!has_remix_comp_mod)
		{
			if (const auto res = MessageBoxA(nullptr, "Setup GTA IV to run in fullscreen-borderless mode?\n(Choose No if you want to run the game in windowed mode)", "Display mode", MB_YESNO | MB_ICONQUESTION))
			{
				if (res == IDNO) {
					fullscreen = false;
				} else 
				{
					std::cout  
						<< PAD << "If you are having trouble with launching the game in fullscreen:\n"
						<< PAD << "> Go into 'rtx_comp/game_settings.toml'\n"
						<< PAD << "> Set 'manual_game_resolution_enabled' to 'true'\n"
						<< PAD << "> Set your desired resolution via 'manual_game_resolution'\n\n";
				}
			}

			// steam launch args warning
			MessageBoxA(nullptr, "If you run the game from Steam, make sure to remove ALL launch arguments from Steam properties for GTA IV!\n", "IMPORTANT", MB_OK | MB_ICONWARNING);
		}

		// backup some files

		if (MoveFileExA(
			(game_dir + "\\rtx_comp\\comp_settings.toml").c_str(),
			(game_dir + "\\rtx_comp\\comp_settings.toml.bak").c_str(),
			MOVEFILE_REPLACE_EXISTING))
		{
			std::cout << PAD << "Renamed 'comp_settings.toml' to 'comp_settings.toml.bak'\n";
		}
		Sleep(25);

		if (MoveFileExA(
			(game_dir + "\\rtx.conf").c_str(),
			(game_dir + "\\rtx.conf.bak").c_str(), MOVEFILE_REPLACE_EXISTING))
		{
			std::cout << PAD << "Renamed 'rtx.conf' to 'rtx.conf.bak'\n";
		}
		Sleep(25);

		// --------------

		// earlier versions shipped files that are deprecated now and can cause issues on newer versions so check
		// if they exist and if they do, rename them
		const auto disable_deprecated_files =
		{
			"\\rtx_comp\\addon_settings\\x_local_tonemapper.conf",
			"\\rtx_comp\\addon_settings\\x_local_tonemapper.toml",
		};

		for (const auto& f : disable_deprecated_files)
		{
			auto str = game_dir + f;
			if (MoveFileExA(
				(game_dir + f).c_str(),
				(game_dir + f + ".deprecated").c_str(), MOVEFILE_REPLACE_EXISTING))
			{
				std::cout << PAD << "Renamed '" << f << "' to '" << f << ".deprecated'\n";
			}
			Sleep(25);
		}

		// --------------

		// extract comp files

		log_yellow(true);
		std::cout << "\n" << PAD << "Extracting compatibility zip ...\n";
		log_default();

		Sleep(100); // Small delay before extraction
		bool zip_failed = false;

		if (!extract_zip(found_zip, game_dir, "GTAIV-Remix-CompatibilityMod"))
		{
			zip_failed = true;
			MessageBoxA(nullptr, "Something went wrong when extracting the zip.\nCheck console.", "Error", MB_ICONERROR);
		}

		std::cout << PAD << "> Done!\n";

		Sleep(100); // Small delay between extractions

		if (!has_remix_comp_mod && !zip_failed)
		{
			// extract fullscreen or windowed files
			std::string windowed_or_fullscreen_path = fullscreen ? "_installer_options/mode_fullscreen/" : "_installer_options/mode_windowed/";
			if (!extract_zip(found_zip, game_dir, windowed_or_fullscreen_path)) 
			{
				zip_failed = true;
				MessageBoxA(nullptr, "Something went wrong when extracting the zip.\nCheck console.", "Error", MB_ICONERROR);
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
					(game_dir + "\\update").c_str(),
					(game_dir + "\\update_originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << PAD << "Renamed 'update' folder to 'update_originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\vulkan.dll").c_str(),
					(game_dir + "\\vulkan.dll.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << PAD << "Renamed 'vulkan.dll' to 'vulkan.dll.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << PAD << "Renamed 'GTAIV.EFLC.FusionFix.asi' to 'GTAIV.EFLC.FusionFix.asi.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << PAD << "Renamed 'GTAIV.EFLC.FusionFix.cfg' to 'GTAIV.EFLC.FusionFix.cfg.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << PAD << "Renamed 'GTAIV.EFLC.FusionFix.ini' to 'GTAIV.EFLC.FusionFix.ini.originalFF'\n";
				}
				Sleep(25);
			}

			if (!zip_failed && !extract_zip(found_zip, game_dir, "_installer_options/FusionFix_RTXRemixFork/"))
			{
				zip_failed = true;
				MessageBoxA(nullptr, "Something went wrong when extracting the zip.\nCheck console.", "Error", MB_ICONERROR);
			}

			std::cout << PAD << "> Extracted FusionFix RTX Remix Fork\n\n";
		}

		if (zip_failed)
		{
			log_error("ZIP Extraction Errors -------- ");
			log_default(true);
			std::cout << PAD << "Please make sure to manually extract the Contents of '_installer_options/FusionFix_RTXRemixFork/' into your GTAIV folder.\n";
			std::cout << PAD << "> Overwrite all when prompted\n\n";

			std::cout << PAD << "If you are experiencing issues, try replacing your 'GTAIV.EFLC.FusionFix.cfg' with a cfg from 'mode_fullscreen' OR 'mode_windowed'.\n";
			log_default();
		}
	}

	std::cout << "\n" << PAD << "Proceeding with remix mods ...\n";

	// --------------
	// Base Remix Mod

	const RemoteModConfig base_mod = 
	{
		"Base Remix-Mod",
		"xoxor4d",
		"gta4-rtx-base-mod",
		"master",
		"https://github.com/xoxor4d/gta4-rtx-base-mod",
		"https://github.com/xoxor4d/gta4-rtx-base-mod/archive/refs/heads/master.zip",
		"gta4-rtx-base-mod-master/mods",
		"mods",
		"gta4rtx",
		"gta4rtx_commit.txt",
		"gta4rtx_delivery.txt",
		true,
		"Required: Download and extract the base remix-mod? You can skip this if you've installed it manually.",
		"It contains actual remix replacements such as PBR textures, mesh fixes etc.\n" PAD_INL "Download size: ~1GB",
		"A newer version of the base remix mod is available on GitHub.\n\n" PAD_INL "Would you like to update?"
	};

	const RemoteModConfig autopbr_mod = 
	{
		"AutoPBR Remix-Mod",
		"xoxor4d",
		"gta4-rtx-autopbr-mod",
		"master",
		"https://github.com/xoxor4d/gta4-rtx-autopbr-mod",
		"https://github.com/xoxor4d/gta4-rtx-autopbr-mod/archive/refs/heads/master.zip",
		"gta4-rtx-autopbr-mod-master/mods",
		"mods",
		"z_gta4rtx_autopbr",
		"gta4rtx_autopbr_commit.txt",
		"gta4rtx_autopbr_delivery.txt",
		false,
		"Optional: Download and extract the AutoPBR remix-mod?\n" PAD_INL "You can skip this if you're not interested or if you've installed it manually.",
		"It contains ~18k automatically converted PBR materials.\n" PAD_INL "Download size: ~1GB",
		"A newer version of the AutoPBR mod is available on GitHub.\n\n" PAD_INL "Would you like to update?"
	};

	bool has_selected_delivery_mode = false;
	DeliveryMode selected_delivery_mode = DeliveryMode::Zip;

	if (!read_delivery_mode(game_dir, base_mod, selected_delivery_mode)) 
	{
		selected_delivery_mode = select_delivery_mode();
		has_selected_delivery_mode = true;
	}

	install_or_update_remote_mod(game_dir, base_mod, selected_delivery_mode, true);
	install_or_update_remote_mod(game_dir, autopbr_mod, selected_delivery_mode, has_selected_delivery_mode);

	// Only prompt about DirectX if this is a fresh install (a_gta4-rtx.asi doesn't exist)
	if (!skip_rtx_comp_install && !has_remix_comp_mod)
	{
		// DX9 June 2010 runtime
		if (MessageBoxA(nullptr, "It's recommended to install Microsoft DirectX June 2010 Redistributable.\nDo you want to open a link to the installer?\n(https://www.microsoft.com/en-us/download/details.aspx?id=8109)", "DirectX Runtime", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			ShellExecuteA(nullptr, "open", "https://www.microsoft.com/en-us/download/details.aspx?id=8109", nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	log_green(true);
	std::cout << "\n"
		<< PAD << "Done! - If you run into issues, visit:\n"
		<< PAD << "https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides\n\n"
		<< PAD << "You can reach out for help on discord or create an issue on the GitHub repository.\n"
		<< PAD << "> Please include the external console log (rtx_comp/logfile.txt)\n"
		<< PAD << "> The log files from 'rtx-remix/logs'\n"
		<< PAD << "> A short description and anything else that might help to identify the issue.\n";
	log_default();

	MessageBoxA(nullptr, "Installation complete!\nYou can now launch GTA IV.", "Success", MB_ICONINFORMATION);
    return 0;
}