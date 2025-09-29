#pragma once

namespace shared::utils
{
	class memory
	{
	public:
		class allocator
		{
		public:
			typedef void(*free_callback)(void*);

			allocator()
			{
				this->pool.clear();
				this->ref_memory_.clear();
			}
			~allocator()
			{
				this->clear();
			}

			void clear()
			{
				std::lock_guard<std::mutex> _(this->mutex);

				for (auto i = this->ref_memory_.begin(); i != this->ref_memory_.end(); ++i)
				{
					if (i->first && i->second)
					{
						i->second(i->first);
					}
				}

				this->ref_memory_.clear();

				for (auto& data : this->pool)
				{
					memory::free_(data);
				}

				this->pool.clear();
			}

			void free(void* data)
			{
				std::lock_guard<std::mutex> _(this->mutex);

				auto i = this->ref_memory_.find(data);
				if (i != this->ref_memory_.end())
				{
					i->second(i->first);
					this->ref_memory_.erase(i);
				}

				auto j = std::ranges::find(this->pool.begin(), this->pool.end(), data);
				if (j != this->pool.end())
				{
					memory::free_(data);
					this->pool.erase(j);
				}
			}

			void free(const void* data)
			{
				this->free(const_cast<void*>(data));
			}

			void reference(void* memory, free_callback callback)
			{
				std::lock_guard<std::mutex> _(this->mutex);

				this->ref_memory_[memory] = callback;
			}

			void* allocate(size_t length)
			{
				std::lock_guard<std::mutex> _(this->mutex);

				void* data = memory::allocate(length);
				this->pool.push_back(data);
				return data;
			}
			template <typename T> T* allocate()
			{
				return this->allocate_array<T>(1);
			}
			template <typename T> T* allocate_array(size_t count = 1)
			{
				return static_cast<T*>(this->allocate(count * sizeof(T)));
			}

			bool empty()
			{
				return (this->pool.empty() && this->ref_memory_.empty());
			}

			bool is_pointer_mapped(void* ptr)
			{
				return this->ptr_map_.contains(ptr);
			}

			template <typename T> T* get_pointer(void* oldPtr)
			{
				if (this->is_pointer_mapped(oldPtr))
				{
					return static_cast<T*>(this->ptr_map_[oldPtr]);
				}

				return nullptr;
			}

			void mapPointer(void* oldPtr, void* newPtr)
			{
				this->ptr_map_[oldPtr] = newPtr;
			}

		private:
			std::mutex mutex;
			std::vector<void*> pool;
			std::unordered_map<void*, void*> ptr_map_;
			std::unordered_map<void*, free_callback> ref_memory_;
		};

		static void* allocate_align(size_t length, size_t alignment);
		static void* allocate(size_t length);
		template <typename T> static T* allocate()
		{
			return allocate_array<T>(1);
		}
		template <typename T> static T* allocate_array(size_t count = 1)
		{
			return static_cast<T*>(allocate(count * sizeof(T)));
		}

		static void free_(void* data);
		static void free_(const void* data);

		static void free_align(void* data);
		static void free_align(const void* data);

		static bool is_bad_read_ptr(const void* ptr);
		static bool is_bad_code_ptr(const void* ptr);

		static allocator* get_allocator();

	private:
		static allocator mem_allocator_;
	};
}