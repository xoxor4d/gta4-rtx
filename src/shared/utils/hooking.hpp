#pragma once

#define HOOK_JUMP true
#define HOOK_CALL false

#define HOOK_RETN_PLACE_DEF(NAME)		DWORD (NAME) = 0u
#define HOOK_RETN_PLACE(NAME, OFFSET)	(NAME) = (OFFSET)

// creates typdef for detour - creates var NAME_og
#define DETOUR_TYPEDEF(NAME, RETN_TYPE, CALLING_CONV, ...) \
		typedef RETN_TYPE (CALLING_CONV* NAME##_t)(__VA_ARGS__); \
		NAME##_t NAME##_og = nullptr;

#define DETOUR_CAST(FN) reinterpret_cast<LPVOID*>(&(FN))

namespace shared::utils
{
	namespace mem
	{
		template <typename ptr_type = std::uintptr_t>
		struct memory_address_t
		{
		public:
			//
			// constructors etc...
			memory_address_t() : m_ptr(ptr_type(0)) {};
			memory_address_t(ptr_type v) : m_ptr(v) {};
			memory_address_t(void* v) : m_ptr(ptr_type(v)) {};
			memory_address_t(const void* v) : m_ptr(ptr_type(v)) {};
			~memory_address_t() = default;

			//
			// operators
			//
			inline operator ptr_type() {
				return m_ptr;
			}

			inline operator void* () {
				return static_cast<void*>(m_ptr);
			}

			inline memory_address_t& operator+=(ptr_type offset) {
				m_ptr += offset;
				return *this;
			}

			inline memory_address_t& operator-=(ptr_type offset) {
				m_ptr -= offset;
				return *this;
			}

			inline memory_address_t operator-(ptr_type offset) {
				return memory_address_t<ptr_type>(m_ptr - offset);
			}

			inline memory_address_t operator+(ptr_type offset) {
				return memory_address_t<ptr_type>(m_ptr + offset);
			}

			//
			// utils
			//
			memory_address_t<ptr_type> offset(ptrdiff_t off) {
				if (!m_ptr)
					return m_ptr;
				return memory_address_t<ptr_type>(m_ptr + off);
			}

			template <typename T>
			T read() {
				return *ptr<T>();
			}

			template <typename T>
			T* ptr() {
				return reinterpret_cast<T*>(m_ptr);
			}

			template <typename T>
			T cast() {
				return reinterpret_cast<T>(m_ptr);
			}

			memory_address_t<ptr_type>& self_get(ptr_type count = 1) {
				if (!m_ptr)
					return *this;

				for (ptr_type i = 0; i < count; i++)
					m_ptr = *reinterpret_cast<ptr_type*>(m_ptr);
				return *this;
			}

			template <typename T = std::int32_t>
			__forceinline memory_address_t<ptr_type> jmp(ptrdiff_t offset = 0x1) const {
				if (!m_ptr)
					return ptr_type(0);

				ptr_type base = m_ptr + offset;
				auto disp = *reinterpret_cast<T*>(base);

				base += sizeof(T);
				base += disp;

				return ptr_type(base);
			}
		private:
			ptr_type m_ptr;
		};
		using addr_t = mem::memory_address_t<std::uintptr_t>;


		void** virtual_table(mem::addr_t inst);

		template <typename Fn>
		Fn virtual_function(void* inst, size_t index) {
			return reinterpret_cast<Fn>(virtual_table(inst)[index]);
		}

		// ------

		DWORD find_pattern_in_module(const HMODULE module_name, const std::string_view& signature, DWORD offset = 0u, [[maybe_unused]] const char* description = nullptr);
		DWORD find_pattern(const std::string_view& signature, const DWORD& offset, [[maybe_unused]] const char* description = nullptr, bool is_active = true, const DWORD& inactive_offset = 0u);
		DWORD find_import_addr(const HMODULE hmodule, const char* dll_name, const char* func_name);
		uint32_t resolve_relative_call_address(uint32_t call_instruction_addr);
		uint32_t resolve_relative_jump_address(uint32_t instruction_addr, uint32_t instruction_size, uint32_t bytes_until_relative_addr);
	}

	class hook
	{
	public:

		hook() : initialized(false), installed(false), place(nullptr), stub(nullptr), original(nullptr), trampoline(nullptr), useJump(false), protection(0) { ZeroMemory(this->buffer, sizeof(this->buffer)); }

		hook(void* place, void* stub, bool useJump = true) : hook() { this->initialize(place, stub, useJump); }
		hook(void* place, void(*stub)(), bool useJump = true) : hook(place, reinterpret_cast<void*>(stub), useJump) {}

		hook(DWORD place, void* stub, bool useJump = true) : hook(reinterpret_cast<void*>(place), stub, useJump) {}
		hook(DWORD place, DWORD stub, bool useJump = true) : hook(reinterpret_cast<void*>(place), reinterpret_cast<void*>(stub), useJump) {}
		hook(DWORD place, void(*stub)(), bool useJump = true) : hook(reinterpret_cast<void*>(place), reinterpret_cast<void*>(stub), useJump) {}
		~hook();

		hook* initialize(void* place, void* stub, bool useJump = true);
		hook* initialize(DWORD place, void* stub, bool useJump = true);
		hook* initialize(DWORD place, void(*stub)(), bool useJump = true); // For lambdas
		hook* install(bool unprotect = true, bool keepUnportected = false);
		hook* uninstall(bool unprotect = true);

		void* get_address();
		hook* quick();

		DWORD create_trampoline();
		void* get_trampoline() { return this->trampoline; }

		template <typename T> static std::function<T> call(DWORD function)
		{
			return std::function<T>(reinterpret_cast<T*>(function));
		}

		template <typename T> static std::function<T> call(FARPROC function)
		{
			return call<T>(reinterpret_cast<DWORD>(function));
		}

		template <typename T> static std::function<T> call(void* function)
		{
			return call<T>(reinterpret_cast<DWORD>(function));
		}

		static void set_wstring(void* place, const wchar_t* string, size_t length);
		static void set_wstring(DWORD place, const wchar_t* string, size_t length);
		static void set_wstring(void* place, const wchar_t* string);
		static void set_wstring(DWORD place, const wchar_t* string);

		static void set_string(void* place, const char* string, size_t length);
		static void set_string(DWORD place, const char* string, size_t length);

		static void set_string(void* place, const char* string);
		static void set_string(DWORD place, const char* string);

		static void write_string(void* place, const std::string& string);
		static void write_string(DWORD place, const std::string& string);

		static void nop(void* place, size_t length);
		static void nop(DWORD place, size_t length);

		static void redirect_jump(void* place, void* stub);
		static void redirect_jump(DWORD place, void* stub);

		static bool conditional_jump_to_jmp(DWORD place);
		static bool detour(const DWORD& offset, void* stub, void** original);

		template <typename T> static void set(void* place, T value)
		{
			DWORD oldProtect;
			VirtualProtect(place, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);

			*static_cast<T*>(place) = value;

			VirtualProtect(place, sizeof(T), oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, sizeof(T));
		}

		template <typename T> static void set(DWORD place, T value)
		{
			return set<T>(reinterpret_cast<void*>(place), value);
		}

		// set multiple bytes
		static void set(void* place, const BYTE* bytes, size_t size)
		{
			DWORD oldProtect;
			VirtualProtect(place, size, PAGE_EXECUTE_READWRITE, &oldProtect);
			memcpy(place, bytes, size);
			VirtualProtect(place, size, oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, size);
		}

		// Variadic template to accept multiple BYTE arguments
		template <typename... Args>
		static void set(void* place, BYTE first, Args... rest)
		{
			BYTE bytes[] = { first, static_cast<BYTE>(rest)... };
			set(place, bytes, sizeof(bytes));
		}

		template <typename... Args>
		static void set(DWORD place, BYTE first, Args... rest)
		{
			set(reinterpret_cast<void*>(place), first, rest...);
		}

		template <std::size_t Index, typename ReturnType, typename... Args>
		__forceinline static ReturnType call_virtual(void* instance, Args... args)
		{
			using Fn = ReturnType(__thiscall*)(void*, Args...);

			auto function = (*static_cast<Fn**>(instance))[Index];
			return function(instance, args...);
		}

	private:
		bool initialized;
		bool installed;

		void* place;
		void* stub;
		void* original;
		void* trampoline;
		char buffer[5];
		bool useJump;

		DWORD protection;

		std::mutex stateMutex;
	};


	// #
	// #

	class vtable
	{
	public:
		bool init(const void* table)
		{
			m_base_pointer_ = (unsigned int**)(table);
			while ((*m_base_pointer_)[m_size_])
			{
				m_size_ += 1u;
			}

			m_originals_ = std::make_unique<void* []>(m_size_);
			return (m_base_pointer_ && m_size_);
		}

		bool hook(void* place, const unsigned int index)
		{
			if (m_base_pointer_ && m_size_)
			{
				return (MH_CreateHook((*reinterpret_cast<void***>(m_base_pointer_))[index], place, &m_originals_[index]) == MH_STATUS::MH_OK);
			}

			return false;
		}

		template<typename FN>
		FN original(const unsigned int index) const
		{
			return reinterpret_cast<FN>(m_originals_[index]);
		}

	private:
		unsigned int**				m_base_pointer_ = nullptr;
		unsigned int				m_size_ = 0u;
		std::unique_ptr<void* []>	m_originals_ = { };
	};


	// #
	// #

	class cinterface
	{
	public:
		template<typename T>
		T get(const char* const sz_module, const char* const sz_object)
		{
			if (const auto hmodule = GetModuleHandleA(sz_module); hmodule)
			{
				if (auto* interface_ret = get_interface(hmodule, sz_object); interface_ret)
				{
					return static_cast<T>(interface_ret);
				}
				
				MessageBoxA(HWND_DESKTOP, sz_object, "Failed to find interface:", MB_ICONERROR);
				return NULL;
			}

			return NULL;
		}

	private:
		void* get_interface(HMODULE hmodule, const char* sz_object);
	};

	inline cinterface module_interface;
}
