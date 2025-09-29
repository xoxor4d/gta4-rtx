#include "std_include.hpp"

namespace shared::common::loader
{
	std::vector<std::unique_ptr<component_module>>* module_loader::modules_ = nullptr;

	void module_loader::register_module(std::unique_ptr<component_module>&& module_)
	{
		if (!modules_)
		{
			modules_ = new std::vector<std::unique_ptr<component_module>>();
			atexit(destroy_modules);
		}

		modules_->push_back(std::move(module_));
	}

	void module_loader::destroy_modules()
	{
		if (!modules_) return;

		delete modules_;
		modules_ = nullptr;
	}
}
