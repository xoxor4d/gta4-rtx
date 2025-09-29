#pragma once

namespace shared::common
{
	// shader cache and whitelist manager
	class ShaderCache
	{
	public:
		ShaderCache() = default;

		// get or compute shader hash
		uint32_t get_shader_hash(IDirect3DVertexShader9* shader)
		{
			if (!shader) {
				return 0;
			}

			// check cache
			if (const auto it = shader_hash_cache_.find(shader); it != shader_hash_cache_.end()) {
				return it->second;
			}

			// get bytecode
			UINT bytecode_size = 0;
			shader->GetFunction(nullptr, &bytecode_size);

			std::vector<BYTE> bytecode(bytecode_size);
			shader->GetFunction(bytecode.data(), &bytecode_size);

			// compute and cache hash
			const uint32_t hash = shared::utils::data_hash32(bytecode.data(), bytecode_size);
			shader_hash_cache_[shader] = hash;

#if DEBUG
			// log shaders
			static std::set<uint32_t> seen_hashes;
			if (seen_hashes.insert(hash).second) {
				printf("New shader hash: 0x%08X\n", hash);
			}
#endif

			return hash;
		}

		// check if shader is whitelisted
		bool is_shader_whitelisted(IDirect3DVertexShader9* shader)
		{
			const uint32_t hash = get_shader_hash(shader);
			return hash != 0 && shader_whitelist_.contains(hash);
		}

		// clear cache
		void clear_cache() {
			shader_hash_cache_.clear();
		}

		// add hash to whitelist
		void add_to_whitelist(uint32_t hash) {
			shader_whitelist_.insert(hash);
		}

	private:
		std::unordered_map<IDirect3DVertexShader9*, uint32_t> shader_hash_cache_;
		std::set<uint32_t> shader_whitelist_ = {};
	};

	// global instance
	inline ShaderCache g_shader_cache;
}
