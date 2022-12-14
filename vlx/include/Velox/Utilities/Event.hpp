#pragma once

#include <list>
#include <mutex>
#include <shared_mutex>

#include "EventHandler.hpp"

namespace vlx
{
	template<typename... Args>
	class Event
	{
	public:
		using HandlerType = EventHandler<Args...>;

	protected:
		using HandlerList = std::vector<HandlerType>;

	public:
		Event() = default;

		Event(const Event& other);
		Event(Event&& other) noexcept;

		auto operator=(const Event& other) -> Event&;
		auto operator=(Event&& other) -> Event&;

		void operator()(Args... params);

		auto operator+=(const HandlerType& handler) -> typename HandlerType::IDType;
		auto operator+=(const typename HandlerType::FuncType& handler) -> typename HandlerType::IDType;

		auto operator-=(const HandlerType& handler) -> typename HandlerType::IDType;
		auto operator-=(const typename HandlerType::IDType handler_id) -> typename HandlerType::IDType;

	public:
		constexpr std::size_t Count() const noexcept;
		constexpr bool Empty() const noexcept;

		void Reserve(const std::size_t size);
		void Clear() noexcept;

		auto Add(const HandlerType& handler) -> typename HandlerType::IDType;
		auto Add(const typename HandlerType::FuncType& handler) -> typename HandlerType::IDType;

		bool Remove(const HandlerType& handler);
		bool RemoveID(const typename HandlerType::IDType handler_id);

		void call(Args... params) const;
		std::future<void> call_async(Args... params) const;

	private:
		void call_impl(const HandlerList& handlers, Args... params) const;
		auto GetHandlersCopy() const -> HandlerList;

	private:
		HandlerList					m_handlers;
		mutable std::shared_mutex	m_lock;
	};

	template<typename... Args>
	inline constexpr std::size_t Event<Args...>::Count() const noexcept
	{
		return m_handlers.size();
	}
	template<typename... Args>
	inline constexpr bool Event<Args...>::Empty() const noexcept
	{
		return m_handlers.empty();
	}

	template<typename... Args>
	inline void Event<Args...>::Reserve(const std::size_t size)
	{
		m_handlers.reserve(size);
	}

	template<typename... Args>
	inline void Event<Args...>::Clear() noexcept
	{
		m_handlers.clear();
	}

	template<typename... Args>
	inline Event<Args...>::Event(const Event& other)
	{
		std::shared_lock lock(other.m_lock);
		m_handlers = other.m_handlers;
	}
	template<typename... Args>
	inline Event<Args...>::Event(Event&& other) noexcept
	{
		std::lock_guard lock(other.m_lock);
		m_handlers = std::move(other.m_handlers);
	}

	template<typename... Args>
	inline auto Event<Args...>::operator=(const Event& other) -> Event&
	{
		std::lock_guard lock1(m_lock);
		std::shared_lock lock2(other.m_lock);

		m_handlers = other.m_handlers;

		return *this;
	}
	template<typename... Args>
	inline auto Event<Args...>::operator=(Event&& other) -> Event&
	{
		std::lock_guard lock1(m_lock);
		std::lock_guard lock2(other.m_lock);

		m_handlers = std::move(other.m_handlers);

		return *this;
	}

	template<typename... Args>
	inline void Event<Args...>::operator()(Args... params)
	{
		call(params...);
	}

	template<typename... Args>
	inline auto Event<Args...>::operator+=(const HandlerType& handler) -> typename HandlerType::IDType
	{
		return Add(handler);
	}
	template<typename... Args>
	inline auto Event<Args...>::operator+=(const typename HandlerType::FuncType& handler) -> typename HandlerType::IDType
	{
		return Add(handler);
	}

	template<typename... Args>
	inline auto Event<Args...>::operator-=(const HandlerType& handler) -> typename HandlerType::IDType
	{
		return Remove(handler);
	}
	template<typename... Args>
	inline auto Event<Args...>::operator-=(const typename HandlerType::IDType handler_id) -> typename HandlerType::IDType
	{
		return Remove(handler_id);
	}

	template<typename... Args>
	inline auto Event<Args...>::Add(const HandlerType& handler) -> typename HandlerType::IDType
	{
		std::lock_guard lock(m_lock);

		m_handlers.push_back(handler);
		return handler.GetID();
	}
	template<typename... Args>
	inline auto Event<Args...>::Add(const typename HandlerType::FuncType& handler) -> typename HandlerType::IDType
	{
		return Add(HandlerType(handler));
	}

	template<typename... Args>
	inline bool Event<Args...>::Remove(const HandlerType& handler)
	{
		std::lock_guard lock(m_lock);

		const auto it = std::find(m_handlers.begin(), m_handlers.end(), handler);

		if (it == m_handlers.end())
			return false;

		m_handlers.erase(it);

		return true;
	}
	template<typename... Args>
	inline bool Event<Args...>::RemoveID(const typename HandlerType::IDType handler_id)
	{
		std::lock_guard lock(m_lock);

		const auto it = std::find_if(m_handlers.begin(), m_handlers.end(),
			[handler_id](const HandlerType& handler)
			{
				return handler.GetID() == handler_id;
			});

		if (it == m_handlers.end())
			return false;

		m_handlers.erase(it);

		return true;
	}

	template<typename... Args>
	inline void Event<Args...>::call(Args... params) const
	{
		HandlerList handlers_copy = GetHandlersCopy();
		call_impl(handlers_copy, params...);
	}
	template<typename... Args>
	inline void Event<Args...>::call_impl(const HandlerList& handlers, Args... params) const
	{
		for (const auto& handler : m_handlers)
			handler(params...);
	}
	template<typename... Args>
	inline std::future<void> Event<Args...>::call_async(Args... params) const
	{
		return std::async(std::launch::async,
			[this](Args... async_params)
			{
				call(async_params...);
			}, params...);
	}

	template<typename... Args>
	inline auto Event<Args...>::GetHandlersCopy() const -> HandlerList
	{
		std::shared_lock lock(m_lock);
		return m_handlers;
	}
}