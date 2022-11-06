#pragma once

#include <Velox/Utilities.hpp>
#include <Velox/ECS/Identifiers.hpp>

#include <unordered_map>
#include <memory>

#include "ComponentProxy.hpp"
#include "ComponentAlloc.hpp"

namespace vlx
{
	/// <summary>
	///		ComponentSet is used to prevent having to write many ComponentProxy to access
	///		the components of an object.
	/// </summary>
	template<IsComponent... Cs>
	class ComponentSet
	{
	public:
		ComponentSet(ComponentProxy<Cs>*... proxy);

		template<IsComponent C> requires Contains<C, Cs...>
		C& Get();

	private:
		std::unordered_map<ComponentTypeID, IComponentProxy*> m_components;
	};

	template<IsComponent... Cs>
	inline ComponentSet<Cs...>::ComponentSet(ComponentProxy<Cs>*... proxy)
	{
		(m_components.try_emplace(ComponentAlloc<Cs>::GetTypeID(), proxy), ...);
	}

	template<IsComponent... Cs>
	template<IsComponent C> requires Contains<C, Cs...>
	inline C& ComponentSet<Cs...>::Get()
	{
		return *static_cast<ComponentProxy<C>&>(*m_components.at(ComponentAlloc<C>::GetTypeID())).Get();
	}
}