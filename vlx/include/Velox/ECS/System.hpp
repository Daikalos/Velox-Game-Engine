#pragma once

#include <span>
#include <functional>
#include <unordered_set>
#include <algorithm>
#include <cassert>

#include <Velox/System/IDGenerator.h>

#include <Velox/Utility/NonCopyable.h>
#include <Velox/Utility/ContainerUtils.h>

#include <Velox/Config.hpp>

#include "Identifiers.hpp"
#include "Archetype.hpp"
#include "SystemBase.h"
#include "EntityAdmin.h"

namespace vlx
{
	/// Common system that adds behaviour to components
	/// 
	template<class... Cs> requires IsComponents<Cs...>
	class System : public SystemBase
	{
	public:
		using AllFunc			= std::function<void(EntitySpan, Cs*...)>;
		using EachFunc			= std::function<void(EntityID, Cs&...)>;
		using ComponentTypes	= std::tuple<std::remove_const_t<Cs>...>;

		static constexpr ArrComponentIDs<std::remove_const_t<Cs>...> SystemIDs =
			cu::Sort<ArrComponentIDs<std::remove_const_t<Cs>...>>({ id::Type<std::remove_const_t<Cs>>::ID()... });

		static constexpr ArchetypeID SystemID =
			cu::ContainerHash<ComponentTypeID>()(SystemIDs);

	public:
		System(EntityAdmin& entity_admin);
		System(EntityAdmin& entity_admin, LayerType layer, bool add_to_layer = true);

		System() = delete;
		System(System&& rhs) noexcept;
		~System();

		auto operator=(System&& rhs) noexcept -> System&;

	public:
		NODISC const EntityAdmin* GetEntityAdmin() const noexcept;
		NODISC EntityAdmin* GetEntityAdmin() noexcept;

		NODISC virtual ArchetypeID GetIDKey() const override;
		NODISC virtual const ComponentIDs& GetArchKey() const override;

		/// Determines the priority of the system in the layer.
		/// 
		/// /param Value: priority value, for example, high value means that the system will likely be called first
		/// 
		virtual void SetPriority(float val) override;

	public:
		/// Set function to be called for all the entities when system is run.
		/// 
		template<typename Func>
		void All(Func&& func);

		/// Set member function to be called for all the entities when system is run.
		/// 
		template<class T, class U>
		void All(void(T::*f)(EntitySpan, Cs*...), U p);

		/// Set const member function to be called for all the entities when system is run.
		/// 
		template<class T, class U>
		void All(void(T::*f)(EntitySpan, Cs*...) const, U p);

		/// Set function to be called for each entity when system is run.
		/// 
		template<typename Func>
		void Each(Func&& func);

		/// Set member function to be called for each entity when system is run.
		/// 
		template<class T, class U>
		void Each(void(T::*f)(EntityID, Cs&...), U p);

		/// Set const member function to be called for each entity when system is run.
		/// 
		template<class T, class U>
		void Each(void(T::*f)(EntityID, Cs&...) const, U p);

	public:
		/// Forces this system to run alone, ignoring all others in the layer.
		/// 
		void ForceRun();

		/// Forces this system to be added to the entity admin at specified layer.
		/// 
		bool ForceAdd(LayerType layer);

		/// Forces this system to be removed from entity admin.
		/// 
		bool ForceRemove();

	protected:
		virtual void Run(const Archetype* const archetype) const override;

		template<std::size_t Index = 0, typename T, typename... Ts> requires (Index != sizeof...(Cs))
		void Run(const ComponentIDs& component_ids, EntitySpan entities, T& t, Ts... ts) const;

		template<std::size_t Index, typename T, typename... Ts> requires (Index == sizeof...(Cs))
		void Run(const ComponentIDs& component_ids, EntitySpan entities, T& t, Ts... ts) const;

	protected:	
		EntityAdmin*	m_entity_admin	{nullptr};
		LayerType		m_layer			{LYR_NONE};	// controls the overall order of calls
		bool			m_registered	{false};
		AllFunc			m_func;
	};

	template<class... Cs> requires IsComponents<Cs...>
	inline System<Cs...>::System(EntityAdmin& entity_admin) 
		: m_entity_admin(&entity_admin) {}

	template<class... Cs> requires IsComponents<Cs...>
	inline System<Cs...>::System(EntityAdmin& entity_admin, LayerType layer, bool add_to_layer)
		: m_entity_admin(&entity_admin), m_layer(layer)
	{
		if (m_layer != LYR_NONE && add_to_layer)
		{
			m_entity_admin->RegisterSystem(m_layer, this);
			m_registered = true;
		}
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline System<Cs...>::System(System&& rhs) noexcept
		: m_entity_admin(rhs.m_entity_admin), m_layer(rhs.m_layer), m_registered(rhs.m_registered), m_func(std::move(rhs.m_func))
	{
		rhs.m_entity_admin	= nullptr;
		rhs.m_layer			= LYR_NONE;
		rhs.m_registered	= false;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline System<Cs...>::~System()
	{
		if (m_registered && m_entity_admin != nullptr)
			m_entity_admin->RemoveSystem(m_layer, this);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline auto System<Cs...>::operator=(System&& rhs) noexcept -> System&
	{
		m_entity_admin	= rhs.m_entity_admin;
		m_layer			= rhs.m_layer;
		m_registered	= rhs.m_registered;
		m_func			= std::move(m_func);

		rhs.m_entity_admin	= nullptr;
		rhs.m_layer			= LYR_NONE;
		rhs.m_registered	= false;

		return *this;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline const EntityAdmin* System<Cs...>::GetEntityAdmin() const noexcept
	{
		return m_entity_admin;
	}
	template<class... Cs> requires IsComponents<Cs...>
	inline EntityAdmin* System<Cs...>::GetEntityAdmin() noexcept
	{
		return m_entity_admin;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline ArchetypeID System<Cs...>::GetIDKey() const
	{
		return SystemID;
	}	

	template<class... Cs> requires IsComponents<Cs...>
	inline const ComponentIDs& System<Cs...>::GetArchKey() const
	{
		static ComponentIDs	arch_key = { SystemIDs.begin(), SystemIDs.end() };
		return arch_key;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void System<Cs...>::SetPriority(float val)
	{
		SystemBase::SetPriority(val);
		m_entity_admin->SortSystems(m_layer);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void System<Cs...>::ForceRun()
	{
		m_entity_admin->RunSystem(this);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool System<Cs...>::ForceAdd(LayerType layer)
	{
		if (!m_registered && layer != LYR_NONE)
		{
			m_entity_admin->RegisterSystem(layer, this);

			m_layer = layer;
			m_registered = true;

			return true;
		}

		return false;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool System<Cs...>::ForceRemove()
	{
		if (m_registered && m_layer != LYR_NONE)
		{
			m_entity_admin->RemoveSystem(m_layer, this);

			m_layer = LYR_NONE;
			m_registered = false;

			return true;
		}

		return false;
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<typename Func>
	inline void System<Cs...>::All(Func&& func)
	{
		m_func = std::forward<Func>(func);
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<class T, class U>
	inline void System<Cs...>::All(void(T::*f)(EntitySpan e, Cs*...), U p)
	{
		m_func = [f, p](EntitySpan entities, Cs*... cs)
		{
			(p->*f)(entities, cs...);
		};
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<class T, class U>
	inline void System<Cs...>::All(void(T::*f)(EntitySpan, Cs *...) const, U p)
	{
		m_func = [f, p](EntitySpan entities, Cs*... cs)
		{
			(p->*f)(entities, cs...);
		};
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<typename Func>
	inline void System<Cs...>::Each(Func&& func)
	{
		m_func = [func = std::forward<Func>(func)](EntitySpan entities, Cs*... cs)
		{
			for (std::size_t i = 0; i < entities.size(); ++i)
				func(entities[i], cs[i]...);
		};
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<class T, class U>
	inline void System<Cs...>::Each(void(T::*f)(EntityID, Cs&...), U p)
	{
		m_func = [f, p](EntitySpan entities, Cs*... cs)
		{
			for (std::size_t i = 0; i < entities.size(); ++i)
				(p->*f)(entities[i], cs[i]...);
		};
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<class T, class U>
	inline void System<Cs...>::Each(void(T::*f)(EntityID, Cs&...) const, U p)
	{
		m_func = [f, p](EntitySpan entities, Cs*... cs)
		{
			for (std::size_t i = 0; i < entities.size(); ++i)
				(p->*f)(entities[i], cs[i]...);
		};
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void System<Cs...>::Run(const Archetype* const archetype) const
	{
		assert(IsEnabled() && "System is disabled and cannot be run (EntityAdmin checks for this condition beforehand)");

		if (m_func) // check if func stores callable object
		{
			Run(archetype->type, 
				archetype->entities,
				archetype->component_data);
		}
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<std::size_t Index, typename T, typename... Ts> requires (Index != sizeof...(Cs))
	inline void System<Cs...>::Run(const ComponentIDs& component_ids, EntitySpan entities, T& c, Ts... cs) const
	{
		using ComponentType = std::tuple_element_t<Index, ComponentTypes>; // get type of component at index in system components
		static constexpr auto component_id = EntityAdmin::GetComponentID<ComponentType>();

		std::size_t i = 0;
		ComponentTypeID archetype_comp_id = component_ids[i];
		while (archetype_comp_id != component_id && i < component_ids.size())	// iterate until matching component is found
		{
			archetype_comp_id = component_ids[++i];
		}

		assert(i != component_ids.size() && "System was ran against an invalid archetype");

		Run<Index + 1>(component_ids, entities, c, cs..., reinterpret_cast<ComponentType*>(&c[i][0])); // run again on next component, or call final Run
	}

	template<class... Cs> requires IsComponents<Cs...>
	template<std::size_t Index, typename T, typename... Ts> requires (Index == sizeof...(Cs))
	inline void System<Cs...>::Run(const ComponentIDs& component_ids, EntitySpan entities, T& t, Ts... ts) const
	{
		if (m_func)
			m_func(entities, ts...);
	}
}

