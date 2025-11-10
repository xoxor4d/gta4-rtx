#include "std_include.hpp"
#include <psapi.h>
#include "shared/globals.hpp"
#include "shared/common/flags.hpp"

namespace shared::utils
{
	namespace mem
	{
		void** virtual_table(mem::addr_t inst) {
			return inst.read<void**>();
		}

#pragma optimize("gt", on)
		DWORD find_pattern_in_module(const HMODULE module, const std::string_view& signature, DWORD offset, [[maybe_unused]] const char* description)
		{
			if (!module) {
				throw std::runtime_error("No or invalid module specified");
			}

			DWORD size = 0u;
			uint8_t* base = nullptr;

			{
				const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
				if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
					throw std::runtime_error("Invalid IMAGE_DOS_SIGNATURE");
				}

				const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(module) + dos_header->e_lfanew);

				if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
					throw std::runtime_error("Invalid IMAGE_NT_SIGNATURE");
				}

				size = nt_headers->OptionalHeader.SizeOfImage;
				base = reinterpret_cast<uint8_t*>(module);
			}

			static std::unordered_map<std::string_view, std::pair<std::vector<uint8_t>, std::vector<bool>>> pattern_cache;
			auto& [pattern_bytes, mask] = pattern_cache[signature];

			if (pattern_bytes.empty())
			{
				auto hex_to_byte = [](char h1, char h2) -> uint8_t
					{
						auto to_nibble = [](char c) -> uint8_t
							{
								if (c >= '0' && c <= '9') return c - '0';
								if (c >= 'A' && c <= 'F') return c - 'A' + 10;
								if (c >= 'a' && c <= 'f') return c - 'a' + 10;
								return 0;
							};
						return (to_nibble(h1) << 4) | to_nibble(h2);
					};

				for (size_t i = 0; i < signature.length();)
				{
					if (signature[i] == ' ')
					{
						++i;
						continue;
					}

					if (signature[i] == '?')
					{
						pattern_bytes.push_back(0);
						mask.push_back(true);
						++i;

						if (i < signature.length() && signature[i] == '?') {
							++i;
						}
					}
					else
					{
						if (i + 1 < signature.length())
						{
							pattern_bytes.push_back(hex_to_byte(signature[i], signature[i + 1]));
							mask.push_back(false);
							i += 2;
						}
						else {
							throw std::runtime_error("Malformed signature");
						}
					}
				}
			}

			const size_t pattern_length = pattern_bytes.size();
			if (pattern_length == 0 || pattern_length > size || pattern_length != mask.size()) {
				throw std::runtime_error("Invalid pattern length");
			}

			// linear scan with memcmp for non-wildcard bytes
			size_t first_non_wildcard = 0;
			while (first_non_wildcard < pattern_length && mask[first_non_wildcard]) {
				++first_non_wildcard;
			}

			for (size_t i = 0; i <= size - pattern_length; ++i)
			{
				if (first_non_wildcard < pattern_length && base[i + first_non_wildcard] != pattern_bytes[first_non_wildcard]) {
					continue;
				}

				bool found = true;
				if (first_non_wildcard < pattern_length)
				{
					for (size_t j = 0; j < pattern_length; ++j)
					{
						if (!mask[j] && base[i + j] != pattern_bytes[j])
						{
							found = false;
							break;
						}
					}
				}
				else {
					found = memcmp(base + i, pattern_bytes.data(), pattern_length) == 0;
				}

				if (found)
				{
					const auto place = reinterpret_cast<DWORD>(base + i + offset);
					{
						if (description) {
							shared::common::log("Hook", std::format("Found pattern @ (0x{:X}) ({})", place, description), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
						} else {
							shared::common::log("Hook", std::format("Found pattern @ (0x{:X})", place), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
						}
					}
					return place;
				}
			}

			if (description) {
				shared::common::log("Hook", std::format("Could not find pattern '{}' ({})", signature, description), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			} else {
				shared::common::log("Hook", std::format("Could not find pattern '{}'", signature), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}
			return 0;
		}

		DWORD find_pattern(const std::string_view& signature, const DWORD& offset, const char* description, const bool is_active, const DWORD& inactive_offset)
		{
			if (!is_active) {
				return inactive_offset + offset;
			}
		
			uint8_t* base = (uint8_t*)GetModuleHandle(nullptr);
			if (!base) {
				throw std::runtime_error("Failed to get base module handle");
			}
		
			if (!globals::exe_size) {
				globals::setup_exe_module();
			}

			if (!globals::exe_size) {
				throw std::runtime_error("Invalid exe_size");
			}
		
			static std::unordered_map<std::string_view, std::pair<std::vector<uint8_t>, std::vector<bool>>> pattern_cache;
			auto& [pattern_bytes, mask] = pattern_cache[signature];

			if (pattern_bytes.empty()) 
			{
				auto hex_to_byte = [](char h1, char h2) -> uint8_t
				{
					auto to_nibble = [](char c) -> uint8_t
					{
						if (c >= '0' && c <= '9') return c - '0';
						if (c >= 'A' && c <= 'F') return c - 'A' + 10;
						if (c >= 'a' && c <= 'f') return c - 'a' + 10;
						return 0;
					};
					return (to_nibble(h1) << 4) | to_nibble(h2);
				};
		
				for (size_t i = 0; i < signature.length();) 
				{
					if (signature[i] == ' ') 
					{
						++i;
						continue;
					}

					if (signature[i] == '?') 
					{
						pattern_bytes.push_back(0);
						mask.push_back(true);
						++i;

						if (i < signature.length() && signature[i] == '?') {
							++i;
						}
					}
					else 
					{
						if (i + 1 < signature.length()) 
						{
							pattern_bytes.push_back(hex_to_byte(signature[i], signature[i + 1]));
							mask.push_back(false);
							i += 2;
						} else {
							throw std::runtime_error("Malformed signature");
						}
					}
				}
			}
		
			const size_t pattern_length = pattern_bytes.size();
			if (pattern_length == 0 || pattern_length > globals::exe_size || pattern_length != mask.size()) {
				throw std::runtime_error("Invalid pattern length");
			}
		
			// linear scan with memcmp for non-wildcard bytes
			size_t first_non_wildcard = 0;
			while (first_non_wildcard < pattern_length && mask[first_non_wildcard]) {
				++first_non_wildcard;
			}
		
			for (size_t i = 0; i <= globals::exe_size - pattern_length; ++i) 
			{
				if (first_non_wildcard < pattern_length && base[i + first_non_wildcard] != pattern_bytes[first_non_wildcard]) {
					continue;
				}

				bool found = true;
				if (first_non_wildcard < pattern_length) 
				{
					for (size_t j = 0; j < pattern_length; ++j) 
					{
						if (!mask[j] && base[i + j] != pattern_bytes[j]) 
						{
							found = false;
							break;
						}
					}
				} else {
					found = memcmp(base + i, pattern_bytes.data(), pattern_length) == 0;
				}

				if (found) 
				{
					static bool validate_patterns = common::flags::has_flag("validate_patterns");
					const auto place = reinterpret_cast<DWORD>(base + i + offset);
					{

						if (description) {
							shared::common::log("Hook", std::format("Found pattern @ (0x{:X}) ({})", place, description), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
						}
						else {
							shared::common::log("Hook", std::format("Found pattern @ (0x{:X})", place), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
						}

						if (validate_patterns) 
						{
							if (place == inactive_offset + offset) {
								shared::common::log("Hook", std::format("> Pattern offset validated!", place), shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
							}
							else {
								shared::common::log("Hook", std::format("> Pattern offset invalid!", place), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
							}
						}
					}
					return place;
				}
			}

			if (description) {
				shared::common::log("Hook", std::format("Could not find pattern '{}' ({})", signature, description), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}
			else {
				shared::common::log("Hook", std::format("Could not find pattern '{}'", signature), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}
			return 0;
		}
#pragma optimize("", on)


		DWORD find_import_addr(const HMODULE hmodule, const char* dll_name, const char* func_name)
		{
			BYTE* base = (BYTE*)hmodule;

			IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)base;

			if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
				return 0;
			}

			// Parse NT headers
			IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)(base + dos_header->e_lfanew);
			if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
				return 0;
			}

			// get import directory
			IMAGE_DATA_DIRECTORY import_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if (import_dir.VirtualAddress == 0) {
				return 0;
			}

			// iterate through import descriptors
			IMAGE_IMPORT_DESCRIPTOR* descriptor = (IMAGE_IMPORT_DESCRIPTOR*)(base + import_dir.VirtualAddress);
			while (descriptor->Name) 
			{
				if (const char* module_name = (const char*)(base + descriptor->Name); 
					_stricmp(module_name, dll_name) == 0)
				{
					// found the module (e.g., d3d9.dll)
					IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)(base + descriptor->FirstThunk);
					IMAGE_THUNK_DATA* origThunk = (IMAGE_THUNK_DATA*)(base + (descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk));

					for (; origThunk->u1.AddressOfData; ++thunk, ++origThunk) 
					{
						if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) 
						{
							IMAGE_IMPORT_BY_NAME* imp = (IMAGE_IMPORT_BY_NAME*)(base + origThunk->u1.AddressOfData);

							if (strcmp((const char*)imp->Name, func_name) == 0) {
								return (DWORD)thunk;
							}
						}
					}
				}
				++descriptor;
			}

			return 0;
		}

		// returns actual function address from a relative call of the function
		uint32_t resolve_relative_call_address(uint32_t call_instruction_addr)
		{
			const uint8_t* bytes = (uint8_t*)call_instruction_addr;
			const int32_t offset = *(int32_t*)(bytes + 1); // read 4 bytes after E8
			return call_instruction_addr + 5 + offset;
		}

		/// get final offset from opcode with relative addressing
		/// @param instruction_addr				base address of the instruction
		/// @param instruction_size				total size of instruction in bytes
		/// @param bytes_until_relative_addr	bytes from base address until relative addr begins
		uint32_t resolve_relative_jump_address(uint32_t instruction_addr, uint32_t instruction_size, uint32_t bytes_until_relative_addr)
		{
			const uint8_t* bytes = (uint8_t*)instruction_addr;
			const uint32_t offset_size = instruction_size - bytes_until_relative_addr;

			int32_t offset = 0;
			if      (offset_size == 1) { offset = *(int8_t*) (bytes + bytes_until_relative_addr); }
			else if (offset_size == 2) { offset = *(int16_t*)(bytes + bytes_until_relative_addr); }
			else if (offset_size == 4) { offset = *(int32_t*)(bytes + bytes_until_relative_addr); }
			else 
			{
				shared::common::log("Hook", std::format("Invalid offset size while resolving relative jump address @ (0x{:X})", instruction_addr), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				return 0;
			}
			return instruction_addr + instruction_size + offset;
		}
	}

	hook::~hook()
	{
		if (this->initialized) {
			this->uninstall();
		}
	}

	hook* hook::initialize(DWORD _place, void(*_stub)(), bool _useJump)
	{
		return this->initialize(_place, reinterpret_cast<void*>(_stub), _useJump);
	}

	hook* hook::initialize(DWORD _place, void* _stub, bool _useJump)
	{
		return this->initialize(reinterpret_cast<void*>(_place), _stub, _useJump);
	}

	hook* hook::initialize(void* _place, void* _stub, bool _useJump)
	{
		if (this->initialized) return this;
		this->initialized = true;

		this->useJump = _useJump;
		this->place = _place;
		this->stub = _stub;

		this->original = static_cast<char*>(this->place) + 5 + *reinterpret_cast<DWORD*>((static_cast<char*>(this->place) + 1));

		return this;
	}

	hook* hook::install(bool unprotect, bool keepUnportected)
	{
		std::lock_guard<std::mutex> _(this->stateMutex);

		if (!this->initialized || this->installed) {
			return this;
		}

		this->installed = true;

		if (unprotect) VirtualProtect(this->place, sizeof(this->buffer), PAGE_EXECUTE_READWRITE, &this->protection);
		std::memcpy(this->buffer, this->place, sizeof(this->buffer));

		char* code = static_cast<char*>(this->place);

		*code = static_cast<char>(this->useJump ? 0xE9 : 0xE8);

		*reinterpret_cast<size_t*>(code + 1) = reinterpret_cast<size_t>(this->stub) - (reinterpret_cast<size_t>(this->place) + 5);

		if (unprotect && !keepUnportected) VirtualProtect(this->place, sizeof(this->buffer), this->protection, &this->protection);

		FlushInstructionCache(GetCurrentProcess(), this->place, sizeof(this->buffer));

		return this;
	}

	hook* hook::quick()
	{
		if (hook::installed) {
			hook::installed = false;
		}

		return this;
	}

	DWORD hook::create_trampoline()
	{
		if (this->trampoline) {
			throw std::runtime_error("Already created a trampoline for this hook!");
		}

		// alloc memory for trampoline (original 5 bytes + jmp)
		this->trampoline = VirtualAlloc(nullptr, sizeof(this->buffer) + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!this->trampoline) {
			return 0;
		}

		// copy original 5 bytes
		std::memcpy(this->trampoline, this->buffer, sizeof(this->buffer));

		// append jmp to the address after the hook (place + 5)
		char* trampoline_code = static_cast<char*>(this->trampoline) + sizeof(this->buffer);
		*trampoline_code = static_cast<char>(0xE9); // JMP
		*reinterpret_cast<size_t*>(trampoline_code + 1) = reinterpret_cast<size_t>(this->place) + 5 - (reinterpret_cast<size_t>(trampoline_code) + 5);

		FlushInstructionCache(GetCurrentProcess(), this->trampoline, sizeof(this->buffer) + 5);
		return reinterpret_cast<DWORD>(this->trampoline);
	}

	hook* hook::uninstall(bool unprotect)
	{
		std::lock_guard<std::mutex> _(this->stateMutex);

		if (!this->initialized || !this->installed) {
			return this;
		}

		this->installed = false;

		if (unprotect) {
			VirtualProtect(this->place, sizeof(this->buffer), PAGE_EXECUTE_READWRITE, &this->protection);
		}

		std::memcpy(this->place, this->buffer, sizeof(this->buffer));

		if (unprotect) {
			VirtualProtect(this->place, sizeof(this->buffer), this->protection, &this->protection);
		}

		{
			if (this->trampoline)
			{
				VirtualFree(this->trampoline, 0, MEM_RELEASE);
				this->trampoline = nullptr;
			}
		}

		FlushInstructionCache(GetCurrentProcess(), this->place, sizeof(this->buffer));
		return this;
	}

	void* hook::get_address()
	{
		return this->place;
	}

	void hook::nop(void* place, size_t length)
	{
		DWORD oldProtect;
		VirtualProtect(place, length, PAGE_EXECUTE_READWRITE, &oldProtect);

		memset(place, 0x90, length);

		VirtualProtect(place, length, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), place, length);
	}

	void hook::nop(DWORD place, size_t length)
	{
		nop(reinterpret_cast<void*>(place), length);
	}

	void hook::set_wstring(void* place, const wchar_t* string, size_t length)
	{
		DWORD oldProtect;
		VirtualProtect(place, (length + 1) * sizeof(wchar_t), PAGE_EXECUTE_READWRITE, &oldProtect);

		wcsncpy_s(static_cast<wchar_t*>(place), length + 1, string, length);

		VirtualProtect(place, (length + 1) * sizeof(wchar_t), oldProtect, &oldProtect);
	}

	void hook::set_wstring(DWORD place, const wchar_t* string, size_t length)
	{
		hook::set_wstring(reinterpret_cast<void*>(place), string, length);
	}

	void hook::set_wstring(void* place, const wchar_t* string)
	{
		hook::set_wstring(place, string, wcslen(string));
	}

	void hook::set_wstring(DWORD place, const wchar_t* string)
	{
		hook::set_wstring(reinterpret_cast<void*>(place), string);
	}

	void hook::set_string(void* place, const char* string, size_t length)
	{
		DWORD oldProtect;
		VirtualProtect(place, length + 1, PAGE_EXECUTE_READWRITE, &oldProtect);

		strncpy_s(static_cast<char*>(place), length, string, length);

		VirtualProtect(place, length + 1, oldProtect, &oldProtect);
	}

	void hook::set_string(DWORD place, const char* string, size_t length)
	{
		hook::set_string(reinterpret_cast<void*>(place), string, length);
	}

	void hook::set_string(void* place, const char* string)
	{
		hook::set_string(place, string, strlen(static_cast<char*>(place)));
	}

	void hook::set_string(DWORD place, const char* string)
	{
		hook::set_string(reinterpret_cast<void*>(place), string);
	}

	void hook::write_string(void* place, const std::string& string)
	{
		DWORD old_protect;
		VirtualProtect(place, string.size() + 1, PAGE_EXECUTE_READWRITE, &old_protect);

		memcpy(place, &string[0], string.size() + 1);

		VirtualProtect(place, string.size() + 1, old_protect, &old_protect);
		FlushInstructionCache(GetCurrentProcess(), place, string.size());
	}

	void hook::write_string(const DWORD place, const std::string& string)
	{
		write_string(reinterpret_cast<void*>(place), string);
	}

	void hook::redirect_jump(void* place, void* stub)
	{
		char* operandPtr = static_cast<char*>(place) + 2;
		int newOperand = reinterpret_cast<int>(stub) - (reinterpret_cast<int>(place) + 6);
		utils::hook::set<int>(operandPtr, newOperand);
	}

	void hook::redirect_jump(DWORD place, void* stub)
	{
		hook::redirect_jump(reinterpret_cast<void*>(place), stub);
	}

	bool hook::conditional_jump_to_jmp(const DWORD place)
	{
		// map for long conditional jumps (6 bytes: 2-byte opcode + 4-byte offset)
		static const std::unordered_map<std::uint16_t, const char*> long_jump_opcodes = {
			{ static_cast<std::uint16_t>(0x800F), "JO" },   // 0F 80
			{ static_cast<std::uint16_t>(0x810F), "JNO" },  // 0F 81
			{ static_cast<std::uint16_t>(0x820F), "JB" },   // 0F 82
			{ static_cast<std::uint16_t>(0x830F), "JAE" },  // 0F 83
			{ static_cast<std::uint16_t>(0x840F), "JZ" },   // 0F 84
			{ static_cast<std::uint16_t>(0x850F), "JNZ" },  // 0F 85
			{ static_cast<std::uint16_t>(0x860F), "JBE" },  // 0F 86
			{ static_cast<std::uint16_t>(0x870F), "JA" },   // 0F 87
			{ static_cast<std::uint16_t>(0x880F), "JS" },   // 0F 88
			{ static_cast<std::uint16_t>(0x890F), "JNS" },  // 0F 89
			{ static_cast<std::uint16_t>(0x8A0F), "JP" },   // 0F 8A
			{ static_cast<std::uint16_t>(0x8B0F), "JNP" },  // 0F 8B
			{ static_cast<std::uint16_t>(0x8C0F), "JL" },   // 0F 8C
			{ static_cast<std::uint16_t>(0x8D0F), "JNL" },  // 0F 8D
			{ static_cast<std::uint16_t>(0x8E0F), "JLE" },  // 0F 8E
			{ static_cast<std::uint16_t>(0x8F0F), "JG" }    // 0F 8F
		};

		static const std::unordered_map<std::uint8_t, const char*> short_jump_opcodes = {
			{ static_cast<std::uint8_t>(0x70), "JO" },   // Jump if overflow
			{ static_cast<std::uint8_t>(0x71), "JNO" },  // Jump if not overflow
			{ static_cast<std::uint8_t>(0x72), "JB" },   // JB/JNAE/JC
			{ static_cast<std::uint8_t>(0x73), "JAE" },  // JAE/JNB/JNC
			{ static_cast<std::uint8_t>(0x74), "JZ" },   // JE/JZ
			{ static_cast<std::uint8_t>(0x75), "JNZ" },  // JNE/JNZ
			{ static_cast<std::uint8_t>(0x76), "JBE" },  // JBE/JNA
			{ static_cast<std::uint8_t>(0x77), "JA" },   // JA/JNBE
			{ static_cast<std::uint8_t>(0x78), "JS" },   // Jump if sign
			{ static_cast<std::uint8_t>(0x79), "JNS" },  // Jump if not sign
			{ static_cast<std::uint8_t>(0x7A), "JP" },   // JP/JPE
			{ static_cast<std::uint8_t>(0x7B), "JNP" },  // JNP/JPO
			{ static_cast<std::uint8_t>(0x7C), "JL" },   // JL/JNGE
			{ static_cast<std::uint8_t>(0x7D), "JNL" },  // JGE/JNL
			{ static_cast<std::uint8_t>(0x7E), "JLE" },  // JLE/JNG
			{ static_cast<std::uint8_t>(0x7F), "JG" }    // JG/JNLE
		};

#ifndef DEBUG
		const auto debug_prints = common::flags::has_flag("debug");
#else
		const auto debug_prints = true;
#endif

		// read the first 2 bytes
		const std::uint8_t* code = (std::uint8_t*)place;
		const std::uint16_t first_two_bytes = (code[1] << 8) | code[0]; // Little-endian

		const char* jump_name = nullptr;
		std::int32_t target_address = 0;
		std::uint8_t new_instruction[6] = { 0 };
		size_t instruction_size = 0;

		// Check if it's a long conditional jump (6 bytes)
		if (long_jump_opcodes.contains(first_two_bytes)) 
		{
			jump_name = long_jump_opcodes.at(first_two_bytes);
			instruction_size = 6;

			// read the 4-byte relative offset (little-endian)
			std::int32_t jmp_offset;
			std::memcpy(&jmp_offset, code + 2, sizeof(jmp_offset));

			// calculate target address: instruction_address + 6 + jmp_offset
			target_address = place + 6 + jmp_offset;

			// calculate JMP relative offset: target_address - (instruction_address + 5)
			std::int32_t new_jmp_offset = target_address - (place + 5);

			// prepare new instruction: E9 + new_jmp_offset + 90
			new_instruction[0] = 0xE9; // JMP
			new_instruction[1] = static_cast<std::uint8_t>(new_jmp_offset & 0xFF);
			new_instruction[2] = static_cast<std::uint8_t>((new_jmp_offset >> 8) & 0xFF);
			new_instruction[3] = static_cast<std::uint8_t>((new_jmp_offset >> 16) & 0xFF);
			new_instruction[4] = static_cast<std::uint8_t>((new_jmp_offset >> 24) & 0xFF);
			new_instruction[5] = 0x90; // NOP
		}
		// check if it's a short conditional jump (2 bytes)
		else if (short_jump_opcodes.contains(code[0])) 
		{
			jump_name = short_jump_opcodes.at(code[0]);
			instruction_size = 2;

			// read the 1-byte relative offset (signed)
			std::int8_t jmp_offset = static_cast<std::int8_t>(code[1]);

			// calculate target address: instruction_address + 2 + jmp_offset
			target_address = place + 2 + jmp_offset;

			// calculate short JMP relative offset: target_address - (instruction_address + 2)
			std::int32_t new_jmp_offset = target_address - (place + 2);

			// check if the offset fits in a short jump (±127 bytes)
			if (new_jmp_offset < -128 || new_jmp_offset > 127) 
			{
				shared::common::log("Hook", std::format("CondJmpToJmp: Target address @ (0x{:X}) is out of range for a short JMP @ (0x{:X})", target_address, place), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				return false;
			}

			// Prepare new instruction: EB + 1-byte offset
			new_instruction[0] = 0xEB; // Short JMP
			new_instruction[1] = static_cast<std::uint8_t>(new_jmp_offset & 0xFF);
		}
		else 
		{
			shared::common::log("Hook", std::format("CondJmpToJmp: Instruction @ (0x{:X}) is not a supported conditional jump (opcode: {:X})", place, first_two_bytes), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}

		// change memory protection
		DWORD old_protect;
		if (!VirtualProtect((void*)place, instruction_size, PAGE_EXECUTE_READWRITE, &old_protect)) 
		{
			shared::common::log("Hook", std::format("CondJmpToJmp: Failed to change memory protection @ (0x{:X})", place), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}

		// write new instruction
		std::memcpy((void*)place, new_instruction, instruction_size);

		// restore original protection
		VirtualProtect((void*)place, instruction_size, old_protect, &old_protect);

		// flush instruction cache
		FlushInstructionCache(GetCurrentProcess(), (void*)place, instruction_size);

		if (debug_prints) {
			shared::common::log("Hook", std::format("Patched {} to JMP @ (0x{:X})", jump_name, place), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}
		return true;
	}

	// minhook detour
	bool hook::detour(const DWORD& offset, void* stub, void** original)
	{
		if (const auto state = MH_CreateHook(reinterpret_cast<LPVOID>(offset), stub, original);
			state != MH_OK)
		{
			shared::common::log("Hook", std::format("Failed to install detour @ (0x{:X})", offset), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}

		if (const auto state = MH_EnableHook(reinterpret_cast<LPVOID>(offset));
			state != MH_OK)
		{
			shared::common::log("Hook", std::format("Failed to activate detour @ (0x{:X})", offset), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return false;
		}

		return true;
	}


	// #
	// #

	void* cinterface::get_interface(const HMODULE hmodule, const char* const sz_object)
	{
		if (const auto addr = GetProcAddress(hmodule, "CreateInterface"); addr)
		{
			if (const auto pfCreateInterface = reinterpret_cast<void* (*)(const char*, int*)>(addr); pfCreateInterface) {
				return pfCreateInterface(sz_object, nullptr);
			}
		}
		return nullptr;
	}
}
