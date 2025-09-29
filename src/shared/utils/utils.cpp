#include "std_include.hpp"

#include "shared/globals.hpp"

#define VA_BUFFER_COUNT		64
#define VA_BUFFER_SIZE		65536

namespace shared::utils
{
	float rad_to_deg(const float radians) {
		return radians * (180.0f / M_PI);
	}

	float deg_to_rad(const float degrees) {
		return degrees * M_PI / 180.0f;
	}

	int try_stoi(const std::string& str, const int& default_return_val)
	{
		int ret = default_return_val;

		try {
			ret = std::stoi(str);
		}
		catch (const std::invalid_argument) { }

		return ret;
	}

	float try_stof(const std::string& str, const float& default_return_val)
	{
		float ret = default_return_val;

		try {
			ret = std::stof(str);
		}
		catch (const std::invalid_argument) { }

		return ret;
	}

	std::string split_string_between_delims(const std::string& str, const char delim_start, const char delim_end)
	{
		const auto first = str.find_last_of(delim_start);
		if (first == std::string::npos) return "";

		const auto last = str.find_first_of(delim_end, first);
		if (last == std::string::npos) return "";

		return str.substr(first + 1, last - first - 1);
	}

	bool starts_with(std::string_view haystack, std::string_view needle)
	{
		return (haystack.size() >= needle.size() && !strncmp(needle.data(), haystack.data(), needle.size()));
	}

	bool string_contains(const std::string_view& s1, const std::string_view s2)
	{
		const auto it = s1.find(s2);

		if (it != std::string::npos) {
			return true;
		}

		return false;
	}

	void replace_all(std::string& source, const std::string_view& from, const std::string_view& to)
	{
		std::string new_string;
		new_string.reserve(source.length());  // avoids a few memory allocations

		std::string::size_type last_pos = 0;
		std::string::size_type find_pos;

		while (std::string::npos != (find_pos = source.find(from, last_pos)))
		{
			new_string.append(source, last_pos, find_pos - last_pos);
			new_string += to;
			last_pos = find_pos + from.length();
		}

		// Care for the rest after last occurrence
		new_string += source.substr(last_pos);

		source.swap(new_string);
	}

	bool erase_substring(std::string& base, const std::string& replace)
	{
		if (const auto it = base.find(replace);
			it != std::string::npos)
		{
			base.erase(it, replace.size());
			return true;
		}

		return false;
	}

	std::string str_to_lower(std::string input)
	{
		std::ranges::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return input;
	}

	std::string convert_wstring(const std::wstring& wstr)
	{
		std::string result;
		result.reserve(wstr.size());

		for (const auto& chr : wstr) {
			result.push_back(static_cast<char>(chr));
		}

		return result;
	}

	int is_space(int c)
	{
		if (c < -1) {
			return 0;
		}

		return _isspace_l(c, nullptr);
	}

	// trim from start
	std::string& ltrim(std::string& s)
	{
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int val)
			{
				return !is_space(val);
			}));

		return s;
	}

	// trim from end
	std::string& rtrim(std::string& s)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(), [](int val)
			{
				return !is_space(val);

			}).base(), s.end());

		return s;
	}

	// trim from both ends
	std::string& trim(std::string& s)
	{
		return ltrim(rtrim(s));
	}

	bool has_matching_symbols(const std::string& str, char opening_symbol, char closing_symbol, bool single_only)
	{
		int count = 0;

		for (char c : str)
		{
			if (c == opening_symbol) {
				count++;
			}
			else if (c == closing_symbol)
			{
				count--;

				if (count < 0) {
					return false;  // malformed
				}
			}

			if (single_only && count > 1) {
				return false;
			}
		}

		return count == 0;
	}

	const char* va(const char* fmt, ...)
	{
		static char g_vaBuffer[VA_BUFFER_COUNT][VA_BUFFER_SIZE];
		static int g_vaNextBufferIndex = 0;

		va_list ap;
		va_start(ap, fmt);
		char* dest = g_vaBuffer[g_vaNextBufferIndex];
		vsnprintf(g_vaBuffer[g_vaNextBufferIndex], VA_BUFFER_SIZE, fmt, ap);
		g_vaNextBufferIndex = (g_vaNextBufferIndex + 1) % VA_BUFFER_COUNT;
		va_end(ap);
		return dest;
	}

	void extract_integer_words(const std::string_view& str, std::vector<int>& integers, bool check_for_duplicates)
	{
		std::stringstream ss;

		//Storing the whole string into string stream
		ss << str;

		// Running loop till the end of the stream
		std::string temp;
		int found;

		while (!ss.eof())
		{
			// extracting word by word from stream 
			ss >> temp;

			// Checking the given word is integer or not
			if (std::stringstream(temp) >> found)
			{
				if (check_for_duplicates)
				{
					// check if we added the integer already
					if (std::find(integers.begin(), integers.end(), found) == integers.end())
					{
						// new integer
						integers.push_back(found);
					}
				}

				else
				{
					//cout << found << " ";
					integers.push_back(found);
				}
			}

			// To save from space at the end of string
			temp = "";
		}
	}

	void transpose_float3x4_to_d3dxmatrix(const shared::float3x4& src, D3DXMATRIX& dest)
	{
		dest.m[0][0] = src.m[0][0];
		dest.m[0][1] = src.m[1][0];
		dest.m[0][2] = src.m[2][0];
		dest.m[0][3] = 0.0f;

		dest.m[1][0] = src.m[0][1];
		dest.m[1][1] = src.m[1][1];
		dest.m[1][2] = src.m[2][1];
		dest.m[1][3] = 0.0f;

		dest.m[2][0] = src.m[0][2];
		dest.m[2][1] = src.m[1][2];
		dest.m[2][2] = src.m[2][2];
		dest.m[2][3] = 0.0f;

		dest.m[3][0] = src.m[0][3];
		dest.m[3][1] = src.m[1][3];
		dest.m[3][2] = src.m[2][3];
		dest.m[3][3] = 1.0f;
	}

	void transpose_d3dxmatrix(const D3DXMATRIX* input, D3DXMATRIX* output, const std::uint32_t count)
	{
		for (auto i = 0u; i < count; ++i)
		{
			D3DXMATRIX& out = output[i];

			// column-major D3DXMATRIX from row-major 3x4
			out._11 = input[i].m[0][0]; out._12 = input[i].m[1][0]; out._13 = input[i].m[2][0]; out._14 = input[i].m[3][0];
			out._21 = input[i].m[0][1]; out._22 = input[i].m[1][1]; out._23 = input[i].m[2][1]; out._24 = input[i].m[3][1];
			out._31 = input[i].m[0][2]; out._32 = input[i].m[1][2]; out._33 = input[i].m[2][2]; out._34 = input[i].m[3][2];
			out._41 = input[i].m[0][3]; out._42 = input[i].m[1][3]; out._43 = input[i].m[2][3]; out._44 = input[i].m[3][3];
		}
	}

	void transpose_float4x4(const float* row_major, float* column_major)
	{
		// transpose the matrix by swapping the rows and columns
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j) {
				column_major[j * 4 + i] = row_major[i * 4 + j];
			}
		}
	}

	bool float_equal(const float a, const float b, const float eps)
	{
		return std::fabs(a - b) < eps;
	}

	// https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Source/Runtime/Core/Private/Math/UnrealMath.cpp
	float finterp_to(const float current, const float target, const float delta_time, const float interpolation_speed)
	{
		// If no interp speed, jump to target value
		if (interpolation_speed <= 0.0f) {
			return target;
		}

		// distance to reach
		const float distance = target - current;

		// If distance is too small, just set the desired location
		if (distance * distance < 1.e-8f) {
			return target;
		}

		// Delta Move, Clamp so we do not over shoot.
		const float delta_move = distance * std::clamp(delta_time * interpolation_speed, 0.0f, 1.0f);

		return current + delta_move;
	}

	/**
	* @brief			open handle to a file within the home-path (root)
	* @param sub_dir	sub directory within home-path (root)
	* @param file_name	the file name
	* @param file		in-out file handle
	* @return			file handle state (valid or not)
	*/
	bool open_file_homepath(const std::string& sub_dir, const std::string& file_name, std::ifstream& file)
	{
		file.open(shared::globals::root_path + "\\" + sub_dir + "\\" + file_name);
		if (!file.is_open()) {
			return false;
		}

		return true;
	}

	//fnv1a
	uint32_t data_hash32(const void* data, const size_t size)
	{
		constexpr uint32_t FNV_prime = 16777619u;
		constexpr uint32_t offset_basis = 2166136261u;
		uint32_t hash = offset_basis;
		const uint8_t* bytes = static_cast<const uint8_t*>(data);

		for (size_t i = 0; i < size; ++i) 
		{
			hash ^= bytes[i];
			hash *= FNV_prime;
		}

		return hash;
	}

	//fnv1a
	std::uint64_t string_hash64(const std::string_view& str)
	{
		const uint64_t FNV_prime = 1099511628211u;
		const uint64_t offset_basis = 14695981039346656037u;
		uint64_t hash = offset_basis;

		for (const char c : str)
		{
			hash ^= static_cast<uint64_t>(c);
			hash *= FNV_prime;
		}

		return hash;
	}

	//fnv1a
	std::uint32_t string_hash32(const std::string_view& str)
	{
		const uint32_t FNV_prime = 16777619u;
		const uint32_t offset_basis = 2166136261u;
		uint32_t hash = offset_basis;

		for (const char c : str)
		{
			hash ^= static_cast<uint64_t>(c);
			hash *= FNV_prime;
		}

		return hash;
	}

	uint32_t hash32_combine(uint32_t seed, const char* str)
	{
		while (*str != '\0') 
		{
			seed ^= std::hash<char>{}(*str) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			++str;
		}
		return seed;
	}

	uint32_t hash32_combine(const uint32_t seed, const int val)
	{
		return seed ^ (std::hash<int>{}(val)+0x9e3779b9 + (seed << 6) + (seed >> 2));
	}

	uint32_t hash32_combine(const uint32_t seed, float val)
	{
		const uint32_t* ptr = reinterpret_cast<uint32_t*>(&val);
		return seed ^ (*ptr + 0x9e3779b9 + (seed << 6) + (seed >> 2));
	}

	std::string hash_file_sha1(const char* file_path)
	{
		const auto file = CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) {
			return {};
		}

		HCRYPTPROV prov_handle = 0;
		HCRYPTHASH hash_handle = 0;

		BYTE buffer[4096];
		DWORD bytes_read = 0;

		BYTE hash[20]; // SHA-1 produces a 20-byte hash
		DWORD hash_len = sizeof(hash);

		if (!CryptAcquireContext(&prov_handle, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ||
			!CryptCreateHash(prov_handle, CALG_SHA1, 0, 0, &hash_handle))
		{
			CloseHandle(file);
			return {};
		}

		while (ReadFile(file, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0)
		{
			if (!CryptHashData(hash_handle, buffer, bytes_read, 0))
			{
				CryptDestroyHash(hash_handle);
				CryptReleaseContext(prov_handle, 0);
				CloseHandle(file);
				return {};
			}
		}

		std::string hash_string;
		if (CryptGetHashParam(hash_handle, HP_HASHVAL, hash, &hash_len, 0))
		{
			std::ostringstream oss;
			for (DWORD i = 0; i < hash_len; ++i) {
				oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
			}

			hash_string = oss.str();
		}

		CryptDestroyHash(hash_handle);
		CryptReleaseContext(prov_handle, 0);
		CloseHandle(file);
		return hash_string;
	}


	// 
	// rendering related

	struct COMDeleter
	{
		void operator()(IUnknown* ptr) const {
			if (ptr) ptr->Release();
		}
	};

	bool compare_shader_constant_name(IDirect3DDevice9* dev, const bool is_vs, const UINT& start_register, const std::string_view& constant_name)
	{
		IDirect3DVertexShader9* used_vs = nullptr;
		IDirect3DPixelShader9* used_ps = nullptr;

		auto hr = is_vs ? dev->GetVertexShader(&used_vs) : dev->GetPixelShader(&used_ps);
		if (SUCCEEDED(hr) && is_vs ? used_vs != nullptr : used_ps != nullptr)
		{
			UINT bytecode_size = 0;

			if (FAILED(is_vs ? used_vs->GetFunction(nullptr, &bytecode_size) : used_ps->GetFunction(nullptr, &bytecode_size))) {
				return false;
			}

			// Allocate bytecode on the heap to avoid stack overflow
			auto bytecode = std::make_unique<DWORD[]>(bytecode_size / sizeof(DWORD));
			if (FAILED(is_vs ? used_vs->GetFunction(bytecode.get(), &bytecode_size) : used_ps->GetFunction(bytecode.get(), &bytecode_size))) {
				return false;
			}

			// Get the constant table
			LPD3DXCONSTANTTABLE constant_table = nullptr;
			if (FAILED(D3DXGetShaderConstantTable(bytecode.get(), &constant_table))) {
				return false;
			}

			// Use a scope guard to ensure pConstantTable is released
			std::unique_ptr<ID3DXConstantTable, COMDeleter> constantTableGuard(constant_table);

			// Iterate through constants
			D3DXCONSTANTTABLE_DESC table_desc;
			if (FAILED(constant_table->GetDesc(&table_desc))) {
				return false;
			}

			for (UINT i = 0; i < table_desc.Constants; i++)
			{
				const auto constant = constant_table->GetConstant(nullptr, i);
				if (constant)
				{
					D3DXCONSTANT_DESC constant_desc;
					UINT desc_count = 1;

					if (FAILED(constant_table->GetConstantDesc(constant, &constant_desc, &desc_count))) {
						return false;
					}

					if (constant_desc.RegisterSet == D3DXRS_FLOAT4 &&
						start_register >= constant_desc.RegisterIndex &&
						start_register < constant_desc.RegisterIndex + constant_desc.RegisterCount)
					{
						//printf("Constant at register c%d is named: %s\n", StartRegister, constantDesc.Name);
						//auto x = StartRegister;
						//auto y = constantDesc.Name;

						const auto name = std::string_view(constant_desc.Name);
						if (name == constant_name) {
							return true;
						}
					}
				}
			}

			if (is_vs) {
				used_vs->Release();
			} else {
				used_ps->Release();
			}
		}

		return false;
	}

	bool compare_vs_shader_constant_name(IDirect3DDevice9* dev, const UINT& start_register, const std::string_view& constant_name)
	{
		return compare_shader_constant_name(dev, true, start_register, constant_name);
	}

	bool compare_ps_shader_constant_name(IDirect3DDevice9* dev, const UINT& start_register, const std::string_view& constant_name)
	{
		return compare_shader_constant_name(dev, false, start_register, constant_name);
	}

	// can be used to figure out the layout of the vertex buffer
	void lookat_vertex_decl([[maybe_unused]] IDirect3DDevice9* dev)
	{
#ifdef DEBUG
		IDirect3DVertexDeclaration9* vertex_decl = nullptr;
		dev->GetVertexDeclaration(&vertex_decl);

		enum d3ddecltype : BYTE
		{
			D3DDECLTYPE_FLOAT1 = 0,		// 1D float expanded to (value, 0., 0., 1.)
			D3DDECLTYPE_FLOAT2 = 1,		// 2D float expanded to (value, value, 0., 1.)
			D3DDECLTYPE_FLOAT3 = 2,		// 3D float expanded to (value, value, value, 1.)
			D3DDECLTYPE_FLOAT4 = 3,		// 4D float
			D3DDECLTYPE_D3DCOLOR = 4,	// 4D packed unsigned bytes mapped to 0. to 1. range

			// Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
			D3DDECLTYPE_UBYTE4 = 5,		// 4D unsigned byte
			D3DDECLTYPE_SHORT2 = 6,		// 2D signed short expanded to (value, value, 0., 1.)
			D3DDECLTYPE_SHORT4 = 7,		// 4D signed short

			// The following types are valid only with vertex shaders >= 2.0
			D3DDECLTYPE_UBYTE4N = 8,	// Each of 4 bytes is normalized by dividing to 255.0
			D3DDECLTYPE_SHORT2N = 9,	// 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
			D3DDECLTYPE_SHORT4N = 10,	// 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
			D3DDECLTYPE_USHORT2N = 11,  // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
			D3DDECLTYPE_USHORT4N = 12,  // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
			D3DDECLTYPE_UDEC3 = 13,		// 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
			D3DDECLTYPE_DEC3N = 14,		// 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
			D3DDECLTYPE_FLOAT16_2 = 15,	// Two 16-bit floating point values, expanded to (value, value, 0, 1)
			D3DDECLTYPE_FLOAT16_4 = 16,	// Four 16-bit floating point values
			D3DDECLTYPE_UNUSED = 17,	// When the type field in a decl is unused.
		};

		enum d3ddecluse : BYTE
		{
			D3DDECLUSAGE_POSITION = 0,
			D3DDECLUSAGE_BLENDWEIGHT,   // 1
			D3DDECLUSAGE_BLENDINDICES,  // 2
			D3DDECLUSAGE_NORMAL,        // 3
			D3DDECLUSAGE_PSIZE,         // 4
			D3DDECLUSAGE_TEXCOORD,      // 5
			D3DDECLUSAGE_TANGENT,       // 6
			D3DDECLUSAGE_BINORMAL,      // 7
			D3DDECLUSAGE_TESSFACTOR,    // 8
			D3DDECLUSAGE_POSITIONT,     // 9
			D3DDECLUSAGE_COLOR,         // 10
			D3DDECLUSAGE_FOG,           // 11
			D3DDECLUSAGE_DEPTH,         // 12
			D3DDECLUSAGE_SAMPLE,        // 13
		};

		struct d3dvertelem
		{
			WORD Stream;		// Stream index
			WORD Offset;		// Offset in the stream in bytes
			d3ddecltype Type;	// Data type
			BYTE Method;		// Processing method
			d3ddecluse Usage;	// Semantics
			BYTE UsageIndex;	// Semantic index
		};

		d3dvertelem decl[MAX_FVF_DECL_SIZE]; UINT numElements = 0;
		vertex_decl->GetDeclaration((D3DVERTEXELEMENT9*)decl, &numElements);
		int break_me = 1; // look into decl
#endif
	}
}
