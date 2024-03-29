#pragma once

#include <tuple>
#include <array>
#include <memory>

#include <Velox/System/Concepts.h>
#include <Velox/Config.hpp>

#include "ComponentRef.hpp"

namespace vlx
{
	///	ComponentSet is used to prevent having to write many ComponentRef to access the components of an object.
	/// 
	template<class... Cs> requires IsComponents<Cs...>
	class ComponentSet final
	{
	private:
		using ComponentTypes	= std::tuple<Cs...>;
		using ComponentRefs		= std::array<std::shared_ptr<void*>, sizeof...(Cs)>;

		template<std::size_t N>
		using ComponentType = std::tuple_element_t<N, ComponentTypes>;

	public:
		ComponentSet() = delete;
		ComponentSet(ComponentRef<Cs>&&... refs);

		bool operator==(const ComponentSet& other) const;
		bool operator!=(const ComponentSet& other) const;

	public:
		NODISC bool IsAnyValid() const;
		NODISC bool IsAllValid() const;

		template<std::size_t N>
		NODISC bool IsValid() const
		{
			return Get<N>() != nullptr;
		}

		template<IsComponent C> requires Contains<C, Cs...>
		NODISC bool IsValid() const
		{
			return Get<C>() != nullptr;
		}

	public:
		template<std::size_t N>
		NODISC auto Get() const -> const ComponentType<N>*
		{
			using ComponentType = std::tuple_element_t<N, ComponentTypes>;
			return static_cast<ComponentType*>(*m_components[N].get());
		}

		template<std::size_t N>
		NODISC auto Get() -> ComponentType<N>*
		{
			return const_cast<ComponentType<N>*>(std::as_const(*this).template Get<N>());
		}

		template<IsComponent C> requires Contains<C, Cs...>
		NODISC const C* Get() const
		{
			return Get<traits::IndexInTuple<C, ComponentTypes>::value>();
		}

		template<IsComponent C> requires Contains<C, Cs...>
		NODISC C* Get()
		{
			return Get<traits::IndexInTuple<C, ComponentTypes>::value>();
		}

	private:
		ComponentRefs m_components;
	};

	template<class... Cs> requires IsComponents<Cs...>
	inline ComponentSet<Cs...>::ComponentSet(ComponentRef<Cs>&&... refs)
		: m_components{ std::forward<ComponentRef<Cs>>(refs).m_component... } { }

	template<class... Cs> requires IsComponents<Cs...>
	inline bool ComponentSet<Cs...>::operator==(const ComponentSet& other) const
	{
		return m_components == other.m_components;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool ComponentSet<Cs...>::operator!=(const ComponentSet& other) const
	{
		return !(*this == other);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool ComponentSet<Cs...>::IsAnyValid() const
	{
		for (std::size_t i = 0; i < sizeof...(Cs); ++i)
		{
			if (m_components[i] != nullptr)
				return true;
		}

		return false;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool ComponentSet<Cs...>::IsAllValid() const
	{
		for (std::size_t i = 0; i < sizeof...(Cs); ++i)
		{
			if (m_components[i] == nullptr)
				return false;
		}

		return true;
	}
}