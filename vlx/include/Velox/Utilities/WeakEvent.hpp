#pragma once

#include <memory>

#include <Velox/Utilities.hpp>

namespace vlx
{
	class WeakEvent final : private NonCopyable
	{
	public:
		template<class Func, class T>
		static std::function<bool()> Weak(Func&& func, const std::shared_ptr<T>& obj);

		struct Wrapper
		{
			template<class T>
			bool operator()(const std::weak_ptr<T>& obj, std::function<void()>& func);
		};
	};

	template<class Func, class T>
	static std::function<bool()> WeakEvent::Weak(Func&& func, const std::shared_ptr<T>& obj)
	{
		return std::bind(Wrapper(), std::weak_ptr<T>(obj), std::function<void()>(std::bind(std::forward<Func>(func), obj.get())));
	}

	template<class T>
	bool WeakEvent::Wrapper::operator()(const std::weak_ptr<T>& obj, std::function<void()>& func)
	{
		if (!obj.expired())
		{
			func();
			return true;
		}

		return false;
	}
}

