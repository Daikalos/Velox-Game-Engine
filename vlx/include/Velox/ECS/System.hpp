#pragma once

#include <functional>
#include <span>

#include <Velox/Utilities.hpp>

#include "Identifiers.hpp"
#include "Archetype.hpp"

namespace vlx
{
	class EntityAdmin;

	class ISystem : private NonCopyable
	{
	public:
		friend class EntityAdmin;

	public:
		virtual ~ISystem() = default;

		virtual bool operator>(const ISystem& rhs) const = 0;

		virtual const ArchetypeID& GetIDKey() const = 0;
		virtual const ComponentIDs& GetArchKey() const = 0;

		virtual float GetPriority() const noexcept = 0;
		virtual void SetPriority(const float val) = 0;

	protected:
		virtual void DoAction(Archetype* archetype) const = 0;
	};

	template<IsComponents... Cs>
	class System : public ISystem
	{
	public:
		using Func = typename std::function<void(std::span<const EntityID>, Cs*...)>;

	public:
		System(EntityAdmin& entity_admin, const LayerType layer);
		~System();

		bool operator>(const ISystem& rhs) const override;

	public:
		const ArchetypeID& GetIDKey() const override;
		const ComponentIDs& GetArchKey() const override;

		[[nodiscard]] float GetPriority() const noexcept override;
		void SetPriority(const float val) override;

		void Action(Func&& func);

	public:
		/// <summary>
		///		Exclude any entities that also holds these components
		/// </summary>
		template<IsComponents... Cs>
		void Exclude();

	protected:
		virtual void DoAction(Archetype* archetype) const override;

		template<std::size_t Index, typename T, typename... Ts> requires (Index != sizeof...(Cs))
		void DoAction(const ComponentIDs& component_ids, std::span<const EntityID> entity_ids, T& t, Ts... ts) const;

		template<std::size_t Index, typename T, typename... Ts> requires (Index == sizeof...(Cs))
		void DoAction(const ComponentIDs& component_ids, std::span<const EntityID> entity_ids, T& t, Ts... ts) const;

	protected:
		EntityAdmin*	m_entity_admin;
		LayerType		m_layer			{LYR_NONE};	// controls the overall order of calls
		float			m_priority		{0.0f};		// priority is for controlling the underlaying order of calls inside a layer

		Func			m_func;

		ComponentIDs	m_exclusion;

		mutable ArchetypeID		m_id_key	{NULL_ARCHETYPE};
		mutable ComponentIDs	m_arch_key;
	};

	template<IsComponents... Cs>
	inline System<Cs...>::System(EntityAdmin& entity_admin, const LayerType layer)
		: m_entity_admin(&entity_admin), m_layer(layer)
	{
		m_entity_admin->RegisterSystem(m_layer, this);
	}

	template<IsComponents... Cs>
	inline System<Cs...>::~System()
	{
		if (m_entity_admin != nullptr)
			m_entity_admin->RemoveSystem(m_layer, this);
	}

	template<IsComponents... Cs>
	inline bool System<Cs...>::operator>(const ISystem& rhs) const
	{
		return GetPriority() > rhs.GetPriority();
	}

	template<IsComponents... Cs>
	inline float System<Cs...>::GetPriority() const noexcept
	{
		return m_priority;
	}
	template<IsComponents... Cs>
	inline void System<Cs...>::SetPriority(const float val)
	{
		m_priority = val;
		m_entity_admin->SortSystems(m_layer);
	}

	template<IsComponents... Cs>
	inline const ArchetypeID& System<Cs...>::GetIDKey() const
	{
		if (m_id_key == NULL_ARCHETYPE)
			m_id_key = cu::VectorHash<ComponentIDs>()(GetArchKey());

		return m_id_key;
	}	

	template<IsComponents... Cs>
	inline const ComponentIDs& System<Cs...>::GetArchKey() const
	{
		if (m_arch_key.empty())
			m_arch_key = cu::Sort<ComponentTypeID>({ { ComponentAlloc<Cs>::GetTypeID()... } });

		return m_arch_key;
	}

	template<IsComponents... Cs>
	inline void System<Cs...>::Action(Func&& func)
	{
		m_func = std::forward<Func>(func);
	}

	template<IsComponents... Cs1>
	template<IsComponents... Cs2>
	inline void System<Cs1...>::Exclude()
	{
		m_exclusion = cu::Sort<ComponentTypeID>({ { ComponentAlloc<Cs2>::GetTypeID()... } });
	}

	template<IsComponents... Cs>
	inline void System<Cs...>::DoAction(Archetype* archetype) const
	{
		if (m_func) // check if func stores callable object
		{
			for (const ComponentTypeID id : m_exclusion)
			{
				const auto it = cu::FindSorted(archetype->type, id);
				if (it != archetype->type.end() && *it == id) // if contains excluded component, return
					return;
			}

			DoAction<0>(
				archetype->type, 
				archetype->entities, 
				archetype->component_data);
		}
	}

	template<IsComponents... Cs>
	template<std::size_t Index, typename T, typename... Ts> requires (Index != sizeof...(Cs))
	inline void System<Cs...>::DoAction(const ComponentIDs& component_ids, std::span<const EntityID> entity_ids, T& c, Ts... cs) const
	{
		using CompType = std::tuple_element_t<Index, std::tuple<Cs...>>;		// get type of element at index in tuple

		std::size_t index2 = 0;

		const ComponentTypeID comp_id = ComponentAlloc<CompType>::GetTypeID();	// get the id for the type of element at index
		ComponentTypeID archetype_comp_id = component_ids[index2];				// id for component in the archetype

		while (archetype_comp_id != comp_id && index2 < component_ids.size())	// iterate until matching component is found
		{
			archetype_comp_id = component_ids[++index2];
		}

		if (index2 == component_ids.size())
			throw std::runtime_error("System was executed against an incorrect Archetype");

		DoAction<Index + 1>(component_ids, entity_ids, c, cs..., reinterpret_cast<CompType*>(&c[index2][0])); // run again on next component, or call final DoAction
	}

	template<IsComponents... Cs>
	template<std::size_t Index, typename T, typename... Ts> requires (Index == sizeof...(Cs))
	inline void System<Cs...>::DoAction(const ComponentIDs& component_ids, std::span<const EntityID> entity_ids, T& t, Ts... ts) const
	{
		m_func(entity_ids, ts...);
	}
}

