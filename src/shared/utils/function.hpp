#pragma once

namespace shared::utils
{
	template <typename T>
	class function
	{
	public:
		using result_type = typename std::function<T>::result_type;

		function() : func_(reinterpret_cast<T*>(nullptr))
		{
		};

		function(const std::uintptr_t& address) : func_(reinterpret_cast<T*>(address))
		{
		}

		function(const function<T>& function) : func_(function.func_)
		{
		}

		function& operator=(const std::uintptr_t& address)
		{
			this->func_ = std::function<T>(reinterpret_cast<T*>(address));
			return *this;
		}

		function& operator=(const function<T>& function)
		{
			this->func_ = function.func_;
			return *this;
		}

		template <typename ...Args>
		result_type operator()(Args&& ...args)
		{
			return this->func_(args...);
		}

		result_type operator()(void)
		{
			return this->func_();
		}

		T* get() const
		{
			return func_;
		}

		operator void*() const
		{
			return this->func_;
		}

	protected:
		T* func_;
	};
}
