#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <execution>
#include <optional>
#include <cassert>
#include <tuple>

#include <Velox/System/Event.hpp>
#include <Velox/System/EventID.h>
#include <Velox/System/IDGenerator.h>
#include <Velox/Utility/NonCopyable.h>
#include <Velox/Utility/ContainerUtils.h>
#include <Velox/Types.hpp>
#include <Velox/Config.hpp>

#include "Identifiers.hpp"
#include "ComponentRef.hpp"
#include "ComponentSet.hpp"
#include "Archetype.hpp"
#include "SystemBase.h"
#include "ComponentEvents.h"
#include "IComponentAlloc.hpp"

namespace vlx
{
	// forward declarations

	template<IsComponent>
	struct ComponentAlloc;

	////////////////////////////////////////////////////////////
	// 
	// ECS based on article by Deckhead:
	// https://indiegamedev.net/2020/05/19/an-entity-component-system-with-data-locality-in-cpp/ 
	// 
	////////////////////////////////////////////////////////////

	///	Data-oriented ECS design. Components are stored in contiguous memory inside of archetypes to improve cache locality.
	/// 
	class EntityAdmin final : private NonCopyable
	{
	private:
		struct Record
		{
			Archetype*	archetype	{nullptr};
			IDType		index		{0}; // where in the archetype entity array is the entity located at
		};

		struct ArchetypeRecord
		{
			ColumnType	column		{0}; // where in the archetype is the components data located at
		};

		struct DataRef
		{
			enum
			{
				R_None		= 0,
				R_Component	= 1 << 0,
				R_Base		= 1 << 1,
				R_All		= R_Component | R_Base
			};

			std::weak_ptr<void*>	component_ptr;
			std::weak_ptr<void*>	base_ptr;
			uint16					base_offset		{0};
			uint16					flag			{0};
		};

		using ComponentPtr				= std::unique_ptr<IComponentAlloc>;
		using ArchetypePtr				= std::unique_ptr<Archetype>;

		using SystemsArrayMap			= std::unordered_map<LayerType, std::vector<SystemBase*>>;
		using ArchetypesArray			= std::vector<ArchetypePtr>;
		using ArchetypeMap				= std::unordered_map<ArchetypeID, Archetype*>;
		using EntityArchetypeMap		= std::unordered_map<EntityID, Record>;
		using EntityComponentRefMap		= std::unordered_map<EntityID, std::unordered_map<ComponentTypeID, DataRef>>;
		using ComponentTypeIDBaseMap	= std::unordered_map<ComponentTypeID, ComponentPtr>;
		using ComponentArchetypesMap	= std::unordered_map<ComponentTypeID, std::unordered_map<ArchetypeID, ArchetypeRecord>>;
		using ArchetypeCache			= std::unordered_map<ArchetypeID, std::vector<Archetype*>>;
		using EventMap					= std::unordered_map<ComponentTypeID, Event<EntityID, void*>>;
		using GenerationCountMap		= std::unordered_map<EntityID, std::size_t>;

		template<IsComponent>
		friend struct ComponentAlloc;

	public:
		EntityAdmin() = default;
		VELOX_API ~EntityAdmin();

		// TODO: Add constructors and assignment operators

	public: 
		/// \returns The ID of the component
		///
		template<IsComponent C>
		NODISC static consteval ComponentTypeID GetComponentID();

		/// Register the component for later usage. Has to be done before the component can be used in the ECS.
		///
		template<IsComponent C>
		void RegisterComponent();

		///	Shortcut for registering multiple components.
		///
		template<class... Cs> requires IsComponents<Cs...>
		void RegisterComponents();

		///	Shortcut for registering multiple components.
		///
		template<class... Cs> requires IsComponents<Cs...>
		void RegisterComponents(std::type_identity<std::tuple<Cs...>>);

		///	Adds a component to the specified entity.
		/// 
		/// \param EntityID: ID of the entity to add the component to
		/// \param Args: Optional constructor arguments
		///	
		/// \returns Pointer to the added component if it was successful, otherwise nullptr
		///
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		C* AddComponent(EntityID entity_id, Args&&... args);

		///	Optimized for quickly adding multiple components to an entity.
		/// 
		/// \param EntityID: ID of the entity to add the component to
		///
		template<class... Cs> requires IsComponents<Cs...>
		void AddComponents(EntityID entity_id);

		///	Optimized for quickly adding multiple components to an entity.
		/// 
		/// \param EntityID: ID of the entity to add the component to
		/// \param Tuple: To automatically deduce the template arguments
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		void AddComponents(EntityID entity_id, std::type_identity<std::tuple<Cs...>>);

		/// Removes a component from the specified entity. Will return true if it succeeded in doing such, otherwise false.
		/// 
		/// \param EntityID: ID of the entity to remove the component from
		/// 
		/// \returns True if it succeeded in removing component, otherwise false
		/// 
		template<IsComponent C>
		bool RemoveComponent(EntityID entity_id);

		/// Optimized for quickly removing multiple components to an entity. If the entity does not hold a given component, it will be skipped.
		///	
		/// \param EntityID: ID of the entity to remove the components from
		/// 
		/// \returns Whether if it was able to remove any component from the entity
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		bool RemoveComponents(EntityID entity_id);

		///	Optimized for quickly removing multiple components to an entity. If the entity does not hold a given component, it will be skipped.
		///	
		/// \param EntityID: ID of the entity to remove the components from
		/// \param Tuple: To automatically deduce the template arguments
		/// 
		/// \returns Whether if it was able to remove any component from the entity
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		bool RemoveComponents(EntityID entity_id, std::type_identity<std::tuple<Cs...>>);

		///	GetComponent is designed to be as fast as possible without checks to see if it exists, otherwise, will throw error. 
		/// Therefore, take some caution when using this function. Use instead: TryGetComponent or GetComponentRef for better safety.
		/// 
		/// \param EntityID: ID of the entity to retrieve the component from
		/// 
		/// \returns Reference to component
		/// 
		template<IsComponent C>
		NODISC C& GetComponent(EntityID entity_id) const;

		/// Tries to get the component. May fail if the entity does not exist or hold the component.
		/// 
		/// \param EntityID: ID of the entity to retrieve the component from
		/// 
		/// \returns Pointer to component, otherwise nullptr
		/// 
		template<IsComponent C>
		NODISC C* TryGetComponent(EntityID entity_id) const;

		/// Retrieves multiple components from an entity.
		/// 
		/// \param EntityID: ID of the entity to retrieve the components from.
		/// 
		/// \returns Tuple containing references to component
		/// 
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
		NODISC std::tuple<Cs&...> GetComponents(EntityID entity_id) const;

		/// Defaults to GetComponent if GetComponents only specifies one type.
		/// 
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
		NODISC std::tuple_element_t<0, std::tuple<Cs...>>& GetComponents(EntityID entity_id) const;

		/// Attempts to retrieve multiple components from an entity.
		/// 
		/// \param EntityID: ID of the entity to retrieve the components from.
		/// 
		/// \returns Tuple containing pointers to component
		/// 
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
		NODISC std::tuple<Cs*...> TryGetComponents(EntityID entity_id) const;

		/// Defaults to TryGetComponent if TryGetComponents only specifies one type.
		/// 
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
		NODISC std::tuple_element_t<0, std::tuple<Cs...>>* TryGetComponents(EntityID entity_id) const;

		///	Constructs a ComponentSet that contains a set of component references that ensures that they remain valid.
		///
		/// \param EntityID: ID of the entity to retrieve the components from
		///
		/// \returns A ComponentSet constructed from the specified components
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		NODISC ComponentSet<Cs...> GetComponentsRef(EntityID entity_id) const;

		///	Constructs a ComponentSet that contains a set of component references that ensures that they remain valid.
		///
		/// \param EntityID: ID of the entity to retrieve the components from
		/// \param Tuple: To automatically deduce the template arguments
		/// 
		/// \returns A ComponentSet constructed from the specified components
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		NODISC ComponentSet<Cs...> GetComponentsRef(EntityID entity_id, std::type_identity<std::tuple<Cs...>>) const;

		///	Allows for you to retrieve any base class without having to know the type of the child.
		/// 
		/// [Incredibly risky, requires base to be first in inheritance, other base classes cannot be automatically 
		/// found without using black magic, for now, offset can be specified to find the correct base class in the 
		/// inheritance order, for example, "class Component : B, A", to find A you pass offset with sizeof(B)]
		/// 
		/// \param EntityID: ID of the entity to retrieve the child from
		/// \param ChildComponentID: ID of the child component to upcast to base
		/// \param Offset: Offset in bytes to specify the location of base in the inheritance order
		/// 
		/// \returns Reference to base
		/// 
		template<class B>
		NODISC B& GetBase(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset = 0) const;

		/// Tries to get the base. May fail if entity does not exist or hold the child component.
		/// 
		/// \param EntityID: ID of the entity to retrieve the child from
		/// \param ChildComponentID: ID of the child component to upcast to base
		/// \param Offset: Offset in bytes to specify the location of base in the inheritance order
		/// 
		/// \returns Pointer to base, otherwise nullptr
		/// 
		template<class B>
		NODISC B* TryGetBase(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset = 0) const;

		/// Modifies the existing component with a newly constructed one.
		/// 
		/// \param EntityID: ID of the entity to retrieve the component from.
		/// \param Args: Optional constructor parameters.
		/// 
		/// \returns Reference to the component that was modified
		/// 
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		C& SetComponent(EntityID entity_id, Args&&... args);

		/// Tries to modify the existing component with a newly constructed one. May fail if the entity does 
		/// not exist or hold the component.
		///
		/// \param EntityID: ID of the entity to retrieve the component from.
		/// \param Args: Optional constructor parameters.
		/// 
		/// \returns Pointer to the component that was modified, otherwise std::nullopt
		/// 
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		C* TrySetComponent(EntityID entity_id, Args&&... args);

		///	Returns a reference for the component whose pointer will remain valid even when the archetype is modified.
		/// 
		///	\param EntityID: ID of the entity to retrieve the component from.
		/// \param Component: Pointer to component, leave nullptr to automatically retrieve.
		/// 
		/// \returns A component reference
		/// 
		template<IsComponent C>
		NODISC ComponentRef<C> GetComponentRef(EntityID entity_id, C* component = nullptr) const;

		///	Returns a reference for the base whose pointer will remain valid even when the archetype is modified. 
		/// 
		///	\param EntityID: ID of the entity to retrieve the base from.
		/// \param ChildComponentID: ID of the child component to upcast to base.
		/// \param Offset: Offset in bytes to specify the location of base in the inheritance order.
		/// \param Base: Pointer to base, leave nullptr to automatically retrieve.
		/// 
		/// \returns A base reference if succesful, will return std::nullopt otherwise.
		/// 
		template<class B>
		NODISC ComponentRef<B> GetBaseRef(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset = 0, B* base = nullptr) const;

		///	Checks if the entity holds the specified component.
		/// 
		/// \param EntityID: ID of the entity to check.
		/// 
		/// \returns True if the entity contains the component C, otherwise false.
		/// 
		template<IsComponent C>
		NODISC bool HasComponent(EntityID entity_id) const;

		///	Checks if the component is registered
		/// 
		///	\returns True if the component has been registered in the ECS, otherwise false.
		/// 
		template<IsComponent C>
		NODISC bool IsComponentRegistered() const;

		///	Checks if all the components is registered
		/// 
		///	\returns True if all the components has been registered in the ECS, otherwise false.
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		NODISC bool IsComponentsRegistered() const;

		///	Checks if the component is registered
		/// 
		/// \param ComponentID: ID of the component
		/// 
		///	\returns True if the component has been registered in the ECS, otherwise false.
		/// 
		NODISC VELOX_API bool IsComponentRegistered(ComponentTypeID component_id) const;

		///	Checks if all the components is registered
		/// 
		/// \param ComponentIDSpan: The IDs of the components to check for
		/// 
		///	\returns True if all the components has been registered in the ECS, otherwise false.
		/// 
		NODISC VELOX_API bool IsComponentsRegistered(ComponentIDSpan component_ids) const;

	public:
		///	Sorts the components for all entities that exactly contains the specified components. The components 
		/// is sorted according to the comparison function. Do note that it will also sort all other components 
		/// the entities may contain to maintain order.
		/// 
		/// \param Comparison: Comparison function between two components that will determine the order.
		/// 
		/// \returns True if sorting was succesful, otherwise false
		/// 
		template<class... Cs, class Comp> requires IsComponents<Cs...>
		bool SortComponents(Comp&& comparison) requires SameTypeParamDecay<Comp, std::tuple_element_t<0, std::tuple<Cs...>>, 0, 1>;

		///	Sorts the components for a specific entity, will also sort the components for all other entities
		///	that holds the same components as the specified entity. Will also sort all other components that 
		///	the entities may contain.
		/// 
		/// \param EntityID: ID of the entity to sort by.
		/// \param Comparison: Comparison function between two components that will determine the order.
		/// 
		/// \returns True if sorting was succesful, otherwise false
		/// 
		template<IsComponent C, class Comp> requires SameTypeParamDecay<Comp, C, 0, 1>
		bool SortComponents(EntityID entity_id, Comp&& comparison);

		///	Returns a duplicated entity with the same properties as the specified one.
		/// 
		/// \param EntityID: entity id of the one to copy the components from
		/// 
		/// \returns ID of the newly created entity containing the copied components
		///
		NODISC VELOX_API EntityID Duplicate(EntityID entity_id);

		/// Searches for entities that contains the specified components.
		/// 
		/// \param Restricted: Returns all entities that exactly match the provided components
		/// 
		/// \returns Entities that contains the components
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		NODISC std::vector<EntityID> GetEntitiesWith(bool restricted = false) const;

		/// Searches for entities that contains the specified components.
		/// 
		/// \param Restricted: Returns all entities that exactly match the provided components
		/// \param Tuple: To automatically deduce the template arguments
		/// 
		/// \returns Entities that contains the components
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		NODISC std::vector<EntityID> GetEntitiesWith(std::type_identity<std::tuple<Cs...>>, bool restricted = false) const;

		///	Shrinks the ECS by removing all the extra archetypes.
		/// 
		/// \param Extensive: Perform a complete shrink of the ECS by removing all the extra data space
		/// 
		VELOX_API void Shrink(bool extensive = false);

		///	Increases the capacity of the archetype containing an exact match of the specified components.
		/// 
		/// \param ComponentCount: Number of components to reserve for in the archetypes
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		void Reserve(std::size_t component_count);

		///	Increases the capacity of the archetype containing an exact match of the specified components.
		/// 
		/// \param ComponentCount: Number of components to reserve for in the archetypes
		/// \param Tuple: To automatically deduce the template arguments.
		/// 
		template<class... Cs> requires IsComponents<Cs...>
		void Reserve(std::size_t component_count, std::type_identity<std::tuple<Cs...>>);

		/// Searches for entities that contains the specified components
		/// 
		/// \param ComponentIDSpan: The IDs of the components to search the entity for
		/// \param ArchetypeID: Combined hash of the component IDs
		/// \param Restricted: Returns all entities that exactly match the provided components
		/// 
		/// \returns Entities that contains the components
		/// 
		NODISC VELOX_API std::vector<EntityID> GetEntitiesWith(ComponentIDSpan component_ids, ArchetypeID archetype_id, bool restricted = false) const;

		///	Increases the capacity of the archetype containing an exact match of the specified components.
		/// 
		/// \param ComponentIDSpan: The IDs of the components to search the entity for
		/// \param ArchetypeID: Combined hash of the component IDs
		/// \param ComponentCount: Number of components to reserve for in the archetypes
		/// \param Tuple: To automatically deduce the template arguments.
		/// 
		VELOX_API void Reserve(ComponentIDSpan component_ids, ArchetypeID archetype_id, std::size_t component_count);

		///	Register listener for when a specific component is added to an entity.
		/// 
		/// \param Func: Function that is called when said event occurs
		/// 
		/// \returns ID for the event, so that it may also be deregistered
		/// 
		template<IsComponent C, typename Func>
		NODISC EventID RegisterOnAddListener(Func&& func);

		///	Register listener for when a specific component is moved in memory for an entity.
		/// 
		/// \param Func: Function that is called when said event occurs
		/// 
		/// \returns ID for the event, so that it may also be deregistered
		/// 
		template<IsComponent C, typename Func>
		NODISC EventID RegisterOnMoveListener(Func&& func);

		///	Register listener for when a specific component is removed from an entity.
		/// 
		/// \param Func: Function that is called when said event occurs
		/// 
		/// \returns ID for the event, so that it may also be deregistered
		/// 
		template<IsComponent C, typename Func>
		NODISC EventID RegisterOnRemoveListener(Func&& func);

		///	Deregister on add listener.
		/// 
		/// \param IDType: ID of the event to be removed
		/// 
		template<IsComponent C>
		void DeregisterOnAddListener(evnt::IDType id);

		///	Deregister on move listener.
		/// 
		/// \param IDType: ID of the event to be removed
		/// 
		template<IsComponent C>
		void DeregisterOnMoveListener(evnt::IDType id);

		///	Deregister on remove listener.
		/// 
		/// \param IDType: ID of the event to be removed
		/// 
		template<IsComponent C>
		void DeregisterOnRemoveListener(evnt::IDType id);

	public:
		NODISC VELOX_API EntityID GetNewEntityID();
		NODISC VELOX_API std::size_t GetGenerationCount(EntityID entity_id) const;

		NODISC VELOX_API bool IsEntityRegistered(EntityID entity_id) const;
		NODISC VELOX_API bool HasComponent(EntityID entity_id, ComponentTypeID component_id) const;

		VELOX_API auto RegisterEntity(EntityID entity_id) -> Record&;
		VELOX_API bool RegisterSystem(LayerType layer, SystemBase* system);

		VELOX_API bool RemoveSystem(LayerType layer, SystemBase* system);
		VELOX_API bool RemoveEntity(EntityID entity_id);

		VELOX_API void RunSystems(LayerType layer) const;
		VELOX_API void SortSystems(LayerType layer);

		VELOX_API void RunSystem(const SystemBase* system) const;

		VELOX_API void AddComponent(EntityID entity_id, ComponentTypeID add_component_id);
		VELOX_API bool RemoveComponent(EntityID entity_id, ComponentTypeID rmv_component_id);

		VELOX_API void AddComponents(EntityID entity_id, ComponentIDSpan component_ids, ArchetypeID archetype_id);
		VELOX_API bool RemoveComponents(EntityID entity_id, ComponentIDSpan component_ids, ArchetypeID archetype_id);

		VELOX_API void DeregisterOnAddListener(ComponentTypeID component_id, evnt::IDType id);
		VELOX_API void DeregisterOnMoveListener(ComponentTypeID component_id, evnt::IDType id);
		VELOX_API void DeregisterOnRemoveListener(ComponentTypeID component_id, evnt::IDType id);

		NODISC VELOX_API bool HasShutdown() const;
		VELOX_API void Shutdown();

	private:
		template<IsComponent C, class Comp>
		bool SortComponents(Archetype* archetype, Comp&& comp);

		template<IsComponent C>
		void EraseComponentRef(EntityID entity_id) const;

		template<IsComponent C>
		void UpdateComponentRef(EntityID entity_id, C* new_component) const;

	private:
		NODISC VELOX_API Archetype* GetArchetype(ComponentIDSpan component_ids, ArchetypeID archetype_id);

		NODISC VELOX_API const std::vector<Archetype*>& GetArchetypes(ComponentIDSpan component_ids, ArchetypeID archetype_id) const;

		VELOX_API Archetype* CreateArchetype(ComponentIDSpan component_ids, ArchetypeID archetype_id);

		VELOX_API void CallOnAddEvent(ComponentTypeID component_id, EntityID eid, void* data) const;
		VELOX_API void CallOnMoveEvent(ComponentTypeID component_id, EntityID eid, void* data) const;
		VELOX_API void CallOnRemoveEvent(ComponentTypeID component_id, EntityID eid, void* data) const;

		VELOX_API void EraseComponentRef(EntityID entity_id, ComponentTypeID component_id) const;
		VELOX_API void UpdateComponentRef(EntityID entity_id, ComponentTypeID component_id, void* new_component) const;

		VELOX_API void ClearEmptyEntityArchetypes();
		VELOX_API void ClearEmptyTypeArchetypes();

		VELOX_API void ConstructSwap(Archetype* new_archetype, Archetype* old_archetype, EntityID entity_id, const Record& record, EntityID last_entity_id, Record& last_record) const;
		VELOX_API void Construct(Archetype* new_archetype, Archetype* old_archetype, EntityID entity_id, const Record& record) const;

		VELOX_API void DestructSwap(Archetype* old_archetype, Archetype* new_archetype, EntityID entity_id, const Record& record, EntityID last_entity_id, Record& last_record) const;
		VELOX_API void Destruct(Archetype* old_archetype, Archetype* new_archetype, EntityID entity_id, const Record& record) const;

		VELOX_API void MakeRoom(Archetype* archetype, const IComponentAlloc* component, std::size_t data_size, std::size_t i) const;

		VELOX_API void Destroy();

	private:
		EntityID				m_entity_id_counter {1};	// current id counter for entities
		std::vector<EntityID>	m_reusable_entity_ids;		// reusable ids of entities that have been destroyed

		SystemsArrayMap			m_systems;						// map layer to array of systems (layer allows for controlling the order of calls)
		ArchetypesArray			m_archetypes;					// find matching archetype to update matching entities
		ArchetypeMap			m_archetype_map;				// map set of components to matching archetype that contains such components
		EntityArchetypeMap		m_entity_archetype_map;			// map entity to where its data is located at in the archetype
		ComponentArchetypesMap	m_component_archetypes_map;		// map component to the archetypes it exists in and where all of the components data in the archetype is located at
		ComponentTypeIDBaseMap	m_component_map;				// access to helper functions for modifying each unique component
		GenerationCountMap		m_generation_count_map;			// tracks how many times a particular entity id has been generated

		EventMap				m_events_add;
		EventMap				m_events_move;
		EventMap				m_events_remove;
		
		mutable ArchetypeCache			m_archetype_cache;
		mutable EntityComponentRefMap	m_entity_component_ref_map;

		bool m_shutdown			{false};
		bool m_destroyed		{false};

		mutable bool m_system_lock		{false}; // safety check for when systems cannot be altered
		mutable bool m_component_lock	{false}; // safety check for when components memory cannot be altered (don't allow add, remove, sort, reserve, etc.)

		// TODO: maybe make entity admin thread-safe
	};

	template<IsComponent C>
	consteval ComponentTypeID EntityAdmin::GetComponentID()
	{
		return id::Type<C>::ID();
	}

	template<IsComponent C>
	inline void EntityAdmin::RegisterComponent()
	{
		auto [it, inserted] = m_component_map.try_emplace(GetComponentID<C>(), std::make_unique<ComponentAlloc<C>>());
		assert(inserted && "Component is already registered");
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::RegisterComponents()
	{
		(RegisterComponent<Cs>(), ...);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::RegisterComponents(std::type_identity<std::tuple<Cs...>>)
	{
		RegisterComponents<Cs...>();
	}

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline C* EntityAdmin::AddComponent(EntityID entity_id, Args&&... args)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");
		
		if (m_component_lock)
			throw std::runtime_error("Components memory is currently locked from modifications");

		const auto eit = m_entity_archetype_map.find(entity_id);
		if (eit == m_entity_archetype_map.end())
			return nullptr;

		Record& record = eit->second;
		Archetype* old_archetype = record.archetype;

		C* add_component = nullptr;
		Archetype* new_archetype = nullptr; // we are going to be moving to a new archetype

		constexpr ComponentTypeID add_component_id = GetComponentID<C>();

		if (old_archetype) // already has an attached archetype, define a new archetype
		{
			const auto ait = old_archetype->edges.find(add_component_id);
			if (ait == old_archetype->edges.end())
			{
				ComponentIDs new_archetype_id = old_archetype->type; // create copy
				if (!cu::InsertUniqueSorted<ComponentTypeID>(new_archetype_id, add_component_id)) // insert while keeping the vector sorted
					return nullptr;

				new_archetype = GetArchetype(new_archetype_id, cu::ContainerHash<ComponentTypeID>()(new_archetype_id));

				old_archetype->edges[add_component_id].add = new_archetype;
				new_archetype->edges[add_component_id].rmv = old_archetype;

				assert(new_archetype_id != old_archetype->type && "New archetype should not be equal to previous");
				assert(new_archetype_id == new_archetype->type && "New archetype type should remain unchanged");
			}
			else new_archetype = ait->second.add;

			if (!new_archetype) // exit if no archetype was found
				return nullptr;

			EntityID last_entity_id = old_archetype->entities.back();
			assert(last_entity_id != NULL_ENTITY && "There should never exist a null entity");

			if (last_entity_id != entity_id) // not same, we'll swap last to current for faster adding
			{
				Record& last_record = m_entity_archetype_map[last_entity_id];

				for (std::size_t i = 0, j = 0; i < new_archetype->type.size(); ++i) // move all the data from old to new and perform swaps at the same time
				{
					const auto component_id		= new_archetype->type[i];
					const auto component		= m_component_map[component_id].get();
					const auto component_size	= component->GetSize();

					const auto current_size		= new_archetype->entities.size() * component_size;
					const auto new_size			= current_size + component_size;

					if (new_size > new_archetype->component_data_size[i])
						MakeRoom(new_archetype, component, component_size, i);

					if (component_id == add_component_id)
					{
						assert(add_component == nullptr && "Component should ever only be constructed once");

						add_component = new(&new_archetype->component_data[i][current_size])
							C(std::forward<Args>(args)...);
					}
					else
					{
						component->MoveDestroyData(*this, entity_id,
							&old_archetype->component_data[j][record.index * component_size],
							&new_archetype->component_data[i][current_size]);

						component->MoveDestroyData(*this, last_entity_id,
							&old_archetype->component_data[j][last_record.index * component_size],
							&old_archetype->component_data[j][record.index * component_size]); // move data from last to current

						++j;
					}
				}

				old_archetype->entities[record.index] = old_archetype->entities.back();
				last_record.index = record.index;
			}
			else // same, usually means that this entity is at the back, just perform normal moving
			{
				for (std::size_t i = 0, j = 0; i < new_archetype->type.size(); ++i) // move all the data from old to new and perform swaps at the same time
				{
					const auto component_id		= new_archetype->type[i];
					const auto component		= m_component_map[component_id].get();
					const auto component_size	= component->GetSize();

					const auto current_size		= new_archetype->entities.size() * component_size;
					const auto new_size			= current_size + component_size;

					if (new_size > new_archetype->component_data_size[i])
						MakeRoom(new_archetype, component, component_size, i);

					if (component_id == add_component_id)
					{
						assert(add_component == nullptr && "Component should ever only be constructed once");

						add_component = new(&new_archetype->component_data[i][current_size])
							C(std::forward<Args>(args)...);
					}
					else
					{
						component->MoveDestroyData(*this, entity_id,
							&old_archetype->component_data[j][record.index * component_size],
							&new_archetype->component_data[i][current_size]);

						++j;
					}
				}
			}

			assert(add_component != nullptr && "Component should have been constructed");

			old_archetype->entities.pop_back(); // by only removing the last entity, it means that when the next component is added, it will overwrite the previous
		}
		else // if the entity has no archetype, first component
		{
			ComponentIDs new_archetype_id(1, add_component_id);	// construct archetype with the component id
			new_archetype = GetArchetype(new_archetype_id, cu::ContainerHash<ComponentTypeID>()(new_archetype_id)); // construct or get archetype using the id

			const auto component		= m_component_map[add_component_id].get();
			const auto component_size	= component->GetSize();

			const auto current_size	= new_archetype->entities.size() * component_size;
			const auto new_size		= current_size + component_size;

			if (new_size > new_archetype->component_data_size[0])
				MakeRoom(new_archetype, component, component_size, 0); // make room and move over existing data

			add_component = new(&new_archetype->component_data[0][current_size])
				C(std::forward<Args>(args)...);
		}

		new_archetype->entities.emplace_back(entity_id);
		record.index		= static_cast<IDType>(new_archetype->entities.size() - 1);
		record.archetype	= new_archetype;

		if constexpr (HasEvent<C, CreatedEvent>) // call associated event
			add_component->Created(*this, entity_id);

		CallOnAddEvent(add_component_id, entity_id, static_cast<void*>(add_component));

		return add_component;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::AddComponents(EntityID entity_id)
	{
		assert(IsComponentsRegistered<Cs...>() && "Components is not registered");

		static constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		static constexpr auto archetype_id = cu::ContainerHash<ComponentTypeID>()(component_ids);

		AddComponents(entity_id, component_ids, archetype_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::AddComponents(EntityID entity_id, std::type_identity<std::tuple<Cs...>>)
	{
		AddComponents<Cs...>(entity_id);
	}

	template<IsComponent C>
	inline bool EntityAdmin::RemoveComponent(EntityID entity_id)
	{
		return RemoveComponent(entity_id, GetComponentID<C>());
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool EntityAdmin::RemoveComponents(EntityID entity_id)
	{
		assert(IsComponentsRegistered<Cs...>() && "Components is not registered");

		static constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		static constexpr auto archetype_id = cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		return RemoveComponents(entity_id, component_ids, archetype_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool EntityAdmin::RemoveComponents(EntityID entity_id, std::type_identity<std::tuple<Cs...>>)
	{
		return RemoveComponents<Cs...>(entity_id);
	}

	template<IsComponent C>
	inline C& EntityAdmin::GetComponent(EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto& record = m_entity_archetype_map.at(entity_id);
		const auto* archetype = record.archetype;

		const auto& map = m_component_archetypes_map.at(component_id);
		const auto& arch_record = map.at(archetype->id);
		
		C* components = reinterpret_cast<C*>(&archetype->component_data[arch_record.column][0]);
		return components[record.index];
	}

	template<IsComponent C>
	inline C* EntityAdmin::TryGetComponent(EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		const auto eit = m_entity_archetype_map.find(entity_id);
		if (eit == m_entity_archetype_map.end())
			return nullptr;

		const auto& record		= eit->second;
		const auto* archetype	= record.archetype;

		if (archetype == nullptr)
			return nullptr;

		constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto cit = m_component_archetypes_map.find(component_id);
		if (cit == m_component_archetypes_map.end())
			return nullptr;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return nullptr;

		const auto& arch_record = ait->second;

		C* components = reinterpret_cast<C*>(&archetype->component_data[arch_record.column][0]);
		return &components[record.index];
	}

	template<class B>
	inline B& EntityAdmin::GetBase(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset) const
	{
		const auto& record = m_entity_archetype_map.at(entity_id);
		const auto* archetype = record.archetype;

		const auto& map = m_component_archetypes_map.at(child_component_id);
		const auto& arch_record = map.at(archetype->id);

		const auto component = m_component_map.at(child_component_id).get();
		const auto component_size = component->GetSize();

		DataPtr ptr = &archetype->component_data[arch_record.column][record.index * component_size];
		B* base_component = reinterpret_cast<B*>(ptr + offset);

		return *base_component;
	}

	template<class B>
	inline B* EntityAdmin::TryGetBase(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset) const
	{
		const auto eit = m_entity_archetype_map.find(entity_id);
		if (eit == m_entity_archetype_map.end())
			return nullptr;

		const auto& record = eit->second;
		const auto* archetype = record.archetype;

		if (archetype == nullptr)
			return nullptr;

		const auto cit = m_component_archetypes_map.find(child_component_id);
		if (cit == m_component_archetypes_map.end())
			return nullptr;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return nullptr;

		const auto iit = m_component_map.find(child_component_id);
		if (iit == m_component_map.end())
			return nullptr;

		const auto component_size = iit->second->GetSize();

		DataPtr ptr = &archetype->component_data[ait->second.column][record.index * component_size];
		B* base_component = reinterpret_cast<B*>(ptr + offset);

		return base_component;
	}

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline C& EntityAdmin::SetComponent(EntityID entity_id, Args&&... args)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		C& old_component = GetComponent<C>(entity_id);
		C new_component(std::forward<Args>(args)...);

		if constexpr (HasEvent<C, AlteredEvent>)
			old_component.Altered(*this, entity_id, new_component);

		old_component = std::move(new_component);

		return old_component;
	}

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline C* EntityAdmin::TrySetComponent(EntityID entity_id, Args&&... args)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		const auto component = TryGetComponent<C>(entity_id);

		if (component == nullptr)
			return nullptr;

		C* old_component = component;
		C new_component(std::forward<Args>(args)...);

		if constexpr (HasEvent<C, AlteredEvent>)
			old_component->Altered(*this, entity_id, new_component);

		*old_component = std::move(new_component);

		return old_component;
	}

	template<IsComponent C>
	inline ComponentRef<C> EntityAdmin::GetComponentRef(EntityID entity_id, C* component) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto CheckComponentPtr = [this, entity_id, &component]()
		{
			if (component == nullptr)
				component = TryGetComponent<C>(entity_id);
		};

		auto& references = m_entity_component_ref_map[entity_id]; // will construct new if it does not exist

		const auto cit = references.find(component_id);
		if (cit == references.end()) // it does not yet exist
		{
			CheckComponentPtr();

			std::shared_ptr<void*> ptr 
				= std::make_shared<void*>(component);

			DataRef data{};
			data.component_ptr	= ptr;
			data.flag			= DataRef::R_Component;

			references.try_emplace(component_id, data);

			return ComponentRef<C>(ptr);
		}

		DataRef& data = cit->second;
		if ((data.flag & DataRef::R_Component) == DataRef::R_Component && data.component_ptr.expired())
		{
			CheckComponentPtr();

			std::shared_ptr<void*> ptr
				= std::make_shared<void*>(component);

			data.component_ptr = ptr;

			return ComponentRef<C>(ptr);
		}

		return ComponentRef<C>(data.component_ptr.lock());
	}

	template<class B>
	inline ComponentRef<B> EntityAdmin::GetBaseRef(EntityID entity_id, ComponentTypeID child_component_id, uint16 offset, B* base) const
	{
		auto& component_refs = m_entity_component_ref_map[entity_id]; // will construct new if it does not exist

		const auto CheckBasePtr = [this, entity_id, child_component_id, offset, &base]()
		{
			if (base == nullptr)
				base = TryGetBase<B>(entity_id, child_component_id, offset);
		};

		const auto cit = component_refs.find(child_component_id);
		if (cit == component_refs.end()) // it does not yet exist
		{
			CheckBasePtr();

			std::shared_ptr<void*> ptr 
				= std::make_shared<void*>(base);

			DataRef data;
			data.base_ptr = ptr;
			data.base_offset = offset;
			data.flag = DataRef::R_Base;

			component_refs.emplace(child_component_id, data);

			return ComponentRef<B>(ptr);
		}

		DataRef& data = cit->second;
		if ((data.flag & DataRef::R_Base) == DataRef::R_Base && data.base_ptr.expired())
		{
			CheckBasePtr();

			std::shared_ptr<void*> ptr 
				= std::make_shared<void*>(base);

			data.base_ptr = ptr;

			return ComponentRef<B>(ptr);
		}

		return ComponentRef<B>(data.base_ptr.lock());
	}

	template<IsComponent C>
	inline bool EntityAdmin::HasComponent(EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");
		return HasComponent(entity_id, GetComponentID<C>());
	}

	template<IsComponent C>
	inline bool EntityAdmin::IsComponentRegistered() const
	{
		return m_component_map.contains(GetComponentID<C>());
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool EntityAdmin::IsComponentsRegistered() const
	{
		return (IsComponentRegistered<Cs>() && ...);
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
	inline std::tuple<Cs&...> EntityAdmin::GetComponents(EntityID entity_id) const
	{
		const auto& record = m_entity_archetype_map.at(entity_id);

		const auto GetComponent = [this]<class C>(const Record& record) -> C&
		{
			static constexpr ComponentTypeID component_id = GetComponentID<C>();

			const auto& map = m_component_archetypes_map.at(component_id);
			const auto& arch_record = map.at(record.archetype->id);

			C* components = reinterpret_cast<C*>(&record.archetype->component_data[arch_record.column][0]);
			return components[record.index];
		};

		return std::tie(GetComponent.template operator()<Cs>(record)...);
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
	inline std::tuple_element_t<0, std::tuple<Cs...>>& EntityAdmin::GetComponents(EntityID entity_id) const
	{
		return GetComponent<Cs...>(entity_id);
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
	inline std::tuple<Cs*...> EntityAdmin::TryGetComponents(EntityID entity_id) const
	{
		const auto it = m_entity_archetype_map.find(entity_id);
		if (it == m_entity_archetype_map.end())
			return {};

		const auto GetComponent = [this]<class C>(const Record& record) -> C*
		{
			static constexpr ComponentTypeID component_id = GetComponentID<C>();

			const auto cit = m_component_archetypes_map.find(component_id);
			if (cit == m_component_archetypes_map.end())
				return nullptr;

			const auto ait = cit->second.find(record.archetype->id);
			if (ait == cit->second.end())
				return nullptr;

			C* components = reinterpret_cast<C*>(&record.archetype->component_data[ait->second.column][0]);
			return &components[record.index];
		};

		return std::make_tuple(GetComponent.template operator()<Cs>(it->second)...);
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
	inline std::tuple_element_t<0, std::tuple<Cs...>>* EntityAdmin::TryGetComponents(EntityID entity_id) const
	{
		return TryGetComponent<Cs...>(entity_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline ComponentSet<Cs...> EntityAdmin::GetComponentsRef(EntityID entity_id) const
	{
		return ComponentSet<Cs...>(GetComponentRef<Cs>(entity_id)...);
	}

	template<class ...Cs> requires IsComponents<Cs...>
	inline ComponentSet<Cs...> EntityAdmin::GetComponentsRef(EntityID entity_id, std::type_identity<std::tuple<Cs...>>) const
	{
		return GetComponentsRef<Cs...>(entity_id);
	}

	template<class... Cs, class Comp> requires IsComponents<Cs...>
	inline bool EntityAdmin::SortComponents(Comp&& comparison) requires SameTypeParamDecay<Comp, std::tuple_element_t<0, std::tuple<Cs...>>, 0, 1>
	{
		using C = std::tuple_element_t<0, std::tuple<Cs...>>; // the component that is meant to be sorted

		static constexpr auto component_ids	= cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		static constexpr auto archetype_id	= cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		const auto it = m_archetype_map.find(archetype_id);
		if (it == m_archetype_map.end())
			return false;

		Archetype* archetype = it->second;

		return SortComponents<C>(archetype, std::forward<Comp>(comparison));
	}

	template<IsComponent C, class Comp> requires SameTypeParamDecay<Comp, C, 0, 1>
	inline bool EntityAdmin::SortComponents(EntityID entity_id, Comp&& comparison)
	{
		const auto it = m_entity_archetype_map.find(entity_id);
		if (it == m_entity_archetype_map.end())
			return false;

		Archetype* archetype = it->second.archetype;

		return SortComponents<C>(archetype, std::forward<Comp>(comparison));
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline std::vector<EntityID> EntityAdmin::GetEntitiesWith(bool restricted) const
	{
		static constexpr auto component_ids	= cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		static constexpr auto archetype_id	= cu::ContainerHash<ComponentTypeID>()(component_ids);

		return GetEntitiesWith(component_ids, archetype_id, restricted);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline std::vector<EntityID> EntityAdmin::GetEntitiesWith(std::type_identity<std::tuple<Cs...>>, bool restricted) const
	{
		return GetEntitiesWith<Cs...>(restricted);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::Reserve(std::size_t component_count)
	{
		static constexpr auto component_ids	= cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		static constexpr auto archetype_id	= cu::ContainerHash<ComponentTypeID>()(component_ids);

		Reserve(component_ids, archetype_id, component_count);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::Reserve(std::size_t component_count, std::type_identity<std::tuple<Cs...>>)
	{
		Reserve<Cs...>(component_count);
	}

	template<IsComponent C, typename Func>
	inline EventID EntityAdmin::RegisterOnAddListener(Func&& func)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		auto& event = m_events_add[component_id];

		auto id = event.Add(
			[func = std::forward<Func>(func)](EntityID entity_id, void* ptr)
			{
				func(entity_id, *reinterpret_cast<C*>(ptr));
			});

		return EventID(event, id);
	}

	template<IsComponent C, typename Func>
	inline EventID EntityAdmin::RegisterOnMoveListener(Func&& func)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		auto& event = m_events_move[component_id];

		auto id = event.Add(
			[func = std::forward<Func>(func)](EntityID entity_id, void* ptr)
			{
				func(entity_id, *reinterpret_cast<C*>(ptr));
			});

		return EventID(event, id);
	}

	template<IsComponent C, typename Func>
	inline EventID EntityAdmin::RegisterOnRemoveListener(Func&& func)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		auto& event = m_events_remove[component_id];

		auto id = event.Add(
			[func = std::forward<Func>(func)](EntityID entity_id, void* ptr)
			{
				func(entity_id, *reinterpret_cast<C*>(ptr));
			});

		return EventID(event, id);
	}

	template<IsComponent C>
	inline void EntityAdmin::DeregisterOnAddListener(evnt::IDType id)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		DeregisterOnAddListener(component_id, id);
	}

	template<IsComponent C>
	inline void EntityAdmin::DeregisterOnMoveListener(evnt::IDType id)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		DeregisterOnMoveListener(component_id, id);
	}

	template<IsComponent C>
	inline void EntityAdmin::DeregisterOnRemoveListener(evnt::IDType id)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		static constexpr auto component_id = GetComponentID<C>();

		DeregisterOnRemoveListener(component_id, id);
	}

	template<IsComponent C, class Comp>
	inline bool EntityAdmin::SortComponents(Archetype* archetype, Comp&& comparison)
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");

		if (m_component_lock)
			throw std::runtime_error("Components memory is currently locked from modifications");

		if (archetype == nullptr)
			return false;

		const auto cit = m_component_archetypes_map.find(GetComponentID<C>());
		if (cit == m_component_archetypes_map.end())
			return false;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return false;

		const ArchetypeRecord& a_record = ait->second;

		C* components = reinterpret_cast<C*>(&archetype->component_data[a_record.column][0]);

		std::vector<uint32> indices(archetype->entities.size());
		std::iota(indices.begin(), indices.end(), 0);

		std::ranges::sort(indices,
			[&comparison, &components](uint32 lhs, uint32 rhs)
			{
				return std::forward<Comp>(comparison)(components[lhs], components[rhs]);
			});

		for (std::size_t i = 0; i < archetype->type.size(); ++i) // sort the components, all need to be sorted
		{
			const auto component_id		= archetype->type[i];
			const auto component		= m_component_map[component_id].get();
			const auto component_size	= component->GetSize();

			ComponentData new_data = std::make_unique<ByteArray>(archetype->component_data_size[i]);

			for (std::size_t j = 0; j < archetype->entities.size(); ++j)
			{
				component->MoveDestroyData(*this, archetype->entities[j],
					&archetype->component_data[i][indices[j] * component_size],
					&new_data[j * component_size]);
			}

			archetype->component_data[i] = std::move(new_data);
		}

		decltype(archetype->entities) new_entities;
		for (std::size_t i = 0; i < archetype->entities.size(); ++i) // now swap the entities
		{
			std::size_t index = indices[i];
			EntityID entity_id = archetype->entities[index];

			auto it = m_entity_archetype_map.find(entity_id);
			assert(it != m_entity_archetype_map.end()); // should never happen

			it->second.index = IDType(i);
			new_entities.push_back(entity_id);
		}

		archetype->entities = new_entities;

		return true;
	}

	template<IsComponent C>
	inline void EntityAdmin::EraseComponentRef(EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");
		EraseComponentRef(entity_id, GetComponentID<C>());
	}

	template<IsComponent C>
	inline void EntityAdmin::UpdateComponentRef(EntityID entity_id, C* new_component) const
	{
		assert(IsComponentRegistered<C>() && "Component is not registered");
		UpdateComponentRef(entity_id, GetComponentID<C>(), static_cast<void*>(new_component)); // TODO: CHECK OUT
	}
}