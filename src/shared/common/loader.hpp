#pragma once

namespace shared::common::loader
{
	class component_module
	{
	public:

	};

	class module_loader final
	{
	public:
		template <typename T>
		class installer final
		{
			static_assert(std::is_base_of<component_module, T>::value, "Module has invalid base class");

		public:
			installer() {
				register_module(std::make_unique<T>());
			}
		};

		template <typename T>
		static T* get()
		{
			for (const auto& module_ : *modules_)
			{
				if (typeid(*module_.get()) == typeid(T)) {
					return reinterpret_cast<T*>(module_.get());
				}
			}

			return nullptr;
		}

		static void register_module(std::unique_ptr<component_module>&& component_module);

	private:
		static std::vector<std::unique_ptr<component_module>>* modules_;
		static void destroy_modules();
	};
}
