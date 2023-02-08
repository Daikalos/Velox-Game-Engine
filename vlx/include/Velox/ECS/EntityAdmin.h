#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <execution>
#include <optional>

#include <Velox/Utilities.hpp>
#include <Velox/Config.hpp>

#include "Identifiers.hpp"
#include "Archetype.hpp"
#include "System.hpp"

namespace vlx
{
	// forward declarations

	struct IComponentAlloc;

	template<IsComponent>
	struct ComponentAlloc;

	class IComponentRef;

	template<class>
	class ComponentRef;

	template<class>
	class BaseRef;

	template<class... Cs> requires IsComponents<Cs...>
	class ComponentSet;

	enum class RefFlag
	{
		None = 0,
		Component = 1 << 0,
		Base = 1 << 1,
		All = Component | Base
	};

	inline RefFlag operator|(RefFlag lhs, RefFlag rhs)		{ return static_cast<RefFlag>(static_cast<int>(lhs) | static_cast<int>(rhs)); }
	inline RefFlag operator&(RefFlag lhs, RefFlag rhs)		{ return static_cast<RefFlag>(static_cast<int>(rhs) & static_cast<int>(rhs)); }
	inline RefFlag& operator|=(RefFlag& lhs, RefFlag rhs)	{ return lhs = (lhs | rhs); }

	////////////////////////////////////////////////////////////
	// 
	// ECS based on article by Deckhead:
	// https://indiegamedev.net/2020/05/19/an-entity-component-system-with-data-locality-in-cpp/ 
	// 
	////////////////////////////////////////////////////////////

	/// <summary>
	///		Data-oriented ECS design. Components are stored in contiguous memory inside of archetypes.
	///		Be warned that adding and removing components from entities is an expensive operation. 
	///		Try to avoid performing such operations on runtime as much as possible and look at using pooling 
	///		instead to prevent removing and adding entities often.
	/// </summary>
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
			RefFlag flag {RefFlag::None};

			std::weak_ptr<IComponent*> component_ptr;
			struct BaseData
			{
				std::weak_ptr<void*> ptr;
				std::uint32_t offset {0};
			} base;
		};

		using ComponentPtr				= std::unique_ptr<IComponentAlloc>;
		using ArchetypePtr				= std::unique_ptr<Archetype>;

		using SystemsArrayMap			= std::unordered_map<LayerType, std::vector<ISystem*>>;
		using ArchetypesArray			= std::vector<ArchetypePtr>;
		using ArchetypeMap				= std::unordered_map<ArchetypeID, Archetype*>;
		using EntityArchetypeMap		= std::unordered_map<EntityID, Record>;
		using EntityComponentRefMap		= std::unordered_map<EntityID, std::unordered_map<ComponentTypeID, DataRef>>;
		using ComponentTypeIDBaseMap	= std::unordered_map<ComponentTypeID, ComponentPtr>;
		using ComponentArchetypesMap	= std::unordered_map<ComponentTypeID, std::unordered_map<ArchetypeID, ArchetypeRecord>>;
		using ArchetypeCache			= std::unordered_map<ArchetypeID, std::vector<Archetype*>>;

		template<IsComponent>
		friend struct ComponentAlloc;

	public:
		VELOX_API EntityAdmin();
		VELOX_API ~EntityAdmin();

	public: 
		/// <summary>
		///		Register the component for later usage. Has to be done before the component can be 
		///		employed in the ECS.
		/// </summary>
		template<IsComponent C>
		void RegisterComponent();

		/// <summary>
		///		Shortcut for registering multiple components.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		void RegisterComponents();

		/// <summary>
		///		Shortcut for registering multiple components.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		void RegisterComponents(std::tuple<Cs...>&& tuple);

		/// <summary>
		///		Adds a component to the specified entity. Can also pass optional constructor arguments. 
		///		Returns pointer to the added component if it was successful, otherwise nullptr.
		/// </summary>
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		C* AddComponent(const EntityID entity_id, Args&&... args);

		/// <summary>
		///		Optimized for quickly adding multiple components to an entity. Cannot pass constructor arguments and returns void.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		void AddComponents(const EntityID entity_id);

		/// <summary>
		///		Optimized for quickly adding multiple components to an entity. Cannot pass constructor arguments and returns void.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		void AddComponents(const EntityID entity_id, std::tuple<Cs...>&& tuple);

		/// <summary>
		///		Removes a component from the specified entity. Will return true if it succeeded in doing such,
		///		otherwise false.
		/// </summary>
		template<IsComponent C>
		bool RemoveComponent(const EntityID entity_id);

		/// <summary>
		///		Optimized for quickly removing multiple components to an entity. If the entity does not hold a given component, it will be skipped.
		///		Returns whether if it was able to remove any component on the entity.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		bool RemoveComponents(const EntityID entity_id);

		/// <summary>
		///		Optimized for quickly removing multiple components to an entity. If the entity does not hold a given component, it will be skipped.
		///		Returns whether if it was able to remove any component on the entity.
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		bool RemoveComponents(const EntityID entity_id, std::tuple<Cs...>&& tuple);

		/// <summary>
		///		GetComponent is designed to be as fast as possible without checks to
		///		see if it exists, otherwise, will throw error. Therefore, take some caution when 
		///		using this function. Use e.g. TryGetComponent or GetComponentRef for better safety.
		/// </summary>
		template<IsComponent C>
		NODISC C& GetComponent(const EntityID entity_id) const;

		/// <summary>
		///		Tries to get the component and returns a pair containing the component ptr and success. If it fails, 
		///		it returns std::nullopt.
		/// </summary>
		template<IsComponent C>
		NODISC std::optional<C*> TryGetComponent(const EntityID entity_id) const;

		/// <summary>
		///		
		/// </summary>
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
		NODISC std::tuple<Cs*...> GetComponents(const EntityID entity_id) const;

		/// <summary>
		///		
		/// </summary>
		template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
		NODISC std::tuple_element_t<0, std::tuple<Cs...>>& GetComponents(const EntityID entity_id) const;

		/// <summary>
		///		Returns of a component set constructed from specified component types
		/// </summary>
		template<class... Cs> requires IsComponents<Cs...>
		NODISC ComponentSet<Cs...> GetComponentsRef(const EntityID entity_id) const;

		/// <summary>
		///		Allows for you to retrieve any base class without having to know the type of the child.
		/// 
		/// 	[Incredibly risky, requires base to be first in inheritance, other base classes cannot be 
		///		automatically found without using voodoo magic, for now, offset can be specified to find 
		///		the correct base class in the inheritance order, for example, "class Component : B, A", to find A 
		///		you pass offset with sizeof(B)]
		/// </summary>
		template<class B>
		NODISC B& GetBase(const EntityID entity_id, const ComponentTypeID child_component_id, const std::size_t offset = 0) const;

		/// <summary>
		/// 	Attempts to get base and returns a pair containing the base ptr and success. If it fails, 
		///		it returns std::nullopt.
		/// </summary>
		template<class B>
		NODISC std::optional<B*> TryGetBase(const EntityID entity_id, const ComponentTypeID child_component_id, const std::size_t offset = 0) const;

		/// <summary>
		///		Sets the component for the entity directly, component is assumed to exist. Returns the 
		///		constructed component.
		/// </summary>
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		C& SetComponent(const EntityID entity_id, Args&&... args);

		/// <summary>
		///		Tries to set the component for entity, will return std::nullopt if it fails.
		/// </summary>
		template<IsComponent C, typename... Args>  requires std::constructible_from<C, Args...>
		std::optional<C*> TrySetComponent(const EntityID entity_id, Args&&... args);

		/// <summary>
		///		Returns a reference for the component whose pointer will remain valid even when the internal data is 
		///		modified. The entity admin will make sure to update the internal data after it has been modified.
		/// </summary>
		template<IsComponent C>
		NODISC ComponentRef<C> GetComponentRef(const EntityID entity_id, C* component = nullptr) const;

		/// <summary>
		///		Tries to return a component proxy, will most likely always succeed, and will only return false if the 
		///		entity does not exist or other unknown error occurs.
		/// </summary>
		template<IsComponent C>
		NODISC std::optional<ComponentRef<C>> TryGetComponentRef(const EntityID entity_id, C* component = nullptr) const;

		/// <summary>
		///		Returns a reference for the base whose pointer will remain valid even when the internal data is 
		///		modified. The proxy will internally get the base's new data location once it has been modified.
		/// </summary>
		template<class B>
		NODISC BaseRef<B> GetBaseRef(const EntityID entity_id, const ComponentTypeID child_component_id, const std::uint32_t offset = 0, B* base = nullptr) const;

		/// <summary>
		///		Tries to return a base reference, will most likely always succeed, and will only return false if the 
		///		entity does not exist or other unknown error occurs. If the component even exists will be checked in the proxy can be extracted with IsExpired().
		/// </summary>
		template<class B>
		NODISC std::optional<BaseRef<B>> TryGetBaseRef(const EntityID entity_id, const ComponentTypeID child_component_id, const std::uint32_t offset = 0, B* base = nullptr) const;

		/// <summary>
		///		Returns true if the entity has the component C, otherwise false.
		/// </summary>
		template<IsComponent C>
		NODISC bool HasComponent(const EntityID entity_id) const;

		/// <summary>
		///		Returns true if the component has been registered in the ECS, otherwise false.
		/// </summary>
		template<IsComponent C>
		NODISC bool IsComponentRegistered() const;

		/// <summary>
		///		Returns the unique ID of a component
		/// </summary>
		template<IsComponent C>
		NODISC static constexpr ComponentTypeID GetComponentID();

	public:
		/// <summary>
		///		Sorts the components for all entities that exactly contains the specified components. 
		///		The components is sorted according to the comparison function. Do note that it will 
		///		also sort all other components the entities may contain to maintain order.
		/// </summary>
		template<class... Cs, class Comp> requires IsComponents<Cs...>
		void SortComponents(Comp&& comparison) requires SameTypeParameter<Comp, std::tuple_element_t<0, std::tuple<Cs...>>, 0, 1>;

		/// <summary>
		///		Sorts the components for a specific entity, will also sort the components for all other entities
		///		that holds the same components as the specified entity. Will also sort all other components that 
		///		the entities may contain.
		/// </summary>
		template<IsComponent C, class Comp> requires SameTypeParameter<Comp, C, 0, 1>
		void SortComponents(const EntityID entity, Comp&& comparison);

		/// <summary>
		///		Get all entities that contain the provided components
		/// </summary>
		/// <param name="restricted:">
		///		Get all entities that exactly match the provided components
		/// </param>
		/// <returns></returns>
		template<class... Cs> requires IsComponents<Cs...>
		NODISC std::vector<EntityID> GetEntitiesWith(bool restricted = false) const;

		/// <summary>
		///		Increases the capacity of the archetype containing an exact match of the specified components.
		/// </summary>
		/// <param name="component_count:">
		///		Number of components to reserve for in the archetypes
		/// </param>
		template<class... Cs> requires IsComponents<Cs...>
		void Reserve(const std::size_t component_count);

	public:
		template<IsContainer T>
		NODISC std::vector<EntityID> GetEntitiesWith(const T& component_ids, const ArchetypeID archetype_id, bool restricted = false) const;

		template<IsContainer T>
		void Reserve(const T& component_ids, const ArchetypeID archetype_id, const std::size_t component_count);

		template<IsContainer T>
		NODISC const std::vector<Archetype*>& GetArchetypes(const T& component_ids, const ArchetypeID archetype_id) const;

	private:
		template<IsContainer T>
		NODISC Archetype* GetArchetype(const T& component_ids, const ArchetypeID archetype_id);
		template<IsContainer T>
		Archetype* CreateArchetype(const T& component_ids, const ArchetypeID archetype_id);

		template<IsComponent C>
		void EraseComponentRef(const EntityID entity_id) const;

		template<IsComponent C>
		void UpdateComponentRef(const EntityID entity_id, C* new_component) const;

	public:
		VELOX_API NODISC EntityID GetNewEntityID();

		VELOX_API NODISC bool IsEntityRegistered(const EntityID entity_id) const;
		VELOX_API NODISC bool HasComponent(const EntityID entity_id, const ComponentTypeID component_id) const;

		VELOX_API auto RegisterEntity(const EntityID entity_id) -> Record&;
		VELOX_API void RegisterSystem(const LayerType layer, ISystem* system);

		VELOX_API void RemoveSystem(const LayerType layer, ISystem* system);
		VELOX_API bool RemoveEntity(const EntityID entity_id);

		VELOX_API void RunSystems(const LayerType layer) const;
		VELOX_API void SortSystems(const LayerType layer);

		VELOX_API void AddComponent(const EntityID entity_id, const ComponentTypeID add_component_id);
		VELOX_API bool RemoveComponent(const EntityID entity_id, const ComponentTypeID rmv_component_id);

		VELOX_API void AddComponents(const EntityID entity_id, const ComponentIDs& component_ids, const ArchetypeID archetype_id);
		VELOX_API bool RemoveComponents(const EntityID entity_id, const ComponentIDs& component_ids, const ArchetypeID archetype_id);

	public:
		/// <summary>
		///		Returns a duplicated entity with the same properties as the specified one
		/// </summary>
		VELOX_API NODISC EntityID Duplicate(const EntityID entity_id);

		/// <summary>
		///		Shrinks the ECS by removing all the empty archetypes
		/// </summary>
		/// <param name="extensive:"> 
		///		Perform a complete shrink of the ECS by removing all the extra data space, will most likely 
		///		invalidate all the existing pointers from e.g., GetComponent
		/// </param>
		VELOX_API void Shrink(bool extensive = false);

	private:
		VELOX_API void EraseComponentRef(const EntityID entity_id, const ComponentTypeID component_id) const;
		VELOX_API void UpdateComponentRef(const EntityID entity_id, const ComponentTypeID component_id, IComponent* new_component) const;

		VELOX_API void MakeRoom(Archetype* archetype, const IComponentAlloc* component, const std::size_t data_size, const std::size_t i);

	private:
		EntityID				m_entity_id_counter;	// current id counter for entities
		std::queue<EntityID>	m_reusable_entity_ids;	// reusable ids of entities that have been destroyed

		SystemsArrayMap			m_systems;						// map layer to array of systems (layer allows for controlling the order of calls)
		ArchetypesArray			m_archetypes;					// find matching archetype to update matching entities
		ArchetypeMap			m_archetype_map;				// map set of components to matching archetype that contains such components
		EntityArchetypeMap		m_entity_archetype_map;			// map entity to where its data is located at in the archetype
		ComponentArchetypesMap	m_component_archetypes_map;		// map component to the archetypes it exists in and where all of the components data in the archetype is located at
		ComponentTypeIDBaseMap	m_component_map;				// access to helper functions for modifying each unique component
		
		mutable ArchetypeCache			m_archetype_cache;
		mutable EntityComponentRefMap	m_entity_component_ref_map;

	};
}

#include "ComponentAlloc.hpp"

namespace vlx
{
	template<IsComponent C>
	inline void EntityAdmin::RegisterComponent()
	{
		auto insert = m_component_map.try_emplace(GetComponentID<C>(), std::make_unique<ComponentAlloc<C>>());
		assert(insert.second);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::RegisterComponents()
	{
		(RegisterComponent<Cs>(), ...);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::RegisterComponents(std::tuple<Cs...>&& tuple)
	{
		RegisterComponents<Cs...>();
	}

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline C* EntityAdmin::AddComponent(const EntityID entity_id, Args&&... args)
	{
		assert(IsComponentRegistered<C>()); // component should be registered

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
				if (!cu::InsertSorted(new_archetype_id, add_component_id)) // insert while keeping the vector sorted (this should ensure that the archetype is always sorted)
					return nullptr;

				new_archetype = GetArchetype(new_archetype_id, cu::ContainerHash<ComponentIDs>()(new_archetype_id));
				old_archetype->edges[add_component_id].add = new_archetype;

				assert(new_archetype_id != old_archetype->type);
				assert(new_archetype_id == new_archetype->type);
			}
			else new_archetype = ait->second.add;

			const EntityID last_entity_id = old_archetype->entities.back();
			Record& last_record = m_entity_archetype_map[last_entity_id];

			assert(last_entity_id != NULL_ENTITY);

			const bool same_entity = (last_entity_id == entity_id);

			for (std::size_t i = 0, j = 0; i < new_archetype->type.size(); ++i) // move all the data from old to new and perform swaps at the same time
			{
				const auto component_id		= new_archetype->type[i];
				const auto component		= m_component_map[component_id].get();
				const auto component_size	= component->GetSize();

				const auto current_size	= new_archetype->entities.size() * component_size;
				const auto new_size		= current_size + component_size;

				if (new_size > new_archetype->component_data_size[i])
					MakeRoom(new_archetype, component, component_size, i);

				if (component_id == add_component_id)
				{
					assert(add_component == nullptr); // should never happen twice

					add_component = new(&new_archetype->component_data[i][current_size])
						C(std::forward<Args>(args)...);
				}
				else
				{
					component->MoveDestroyData(*this, entity_id,
						&old_archetype->component_data[j][record.index * component_size],
						&new_archetype->component_data[i][current_size]);

					if (!same_entity)
					{
						component->MoveDestroyData(*this, last_entity_id,
							&old_archetype->component_data[j][last_record.index * component_size],
							&old_archetype->component_data[j][record.index * component_size]); // move data to last
					}

					++j;
				}
			}

			assert(add_component != nullptr); // a new component should have been added

			if (!same_entity) // move back to current
			{
				old_archetype->entities[record.index] = old_archetype->entities.back();
				last_record.index = record.index;
			}

			old_archetype->entities.pop_back(); // by only removing the last entity, it means that when the next component is added, it will overwrite the previous
		}
		else // if the entity has no archetype, first component
		{
			ComponentIDs new_archetype_id(1, add_component_id);	// construct archetype with the component id
			new_archetype = GetArchetype(new_archetype_id, cu::ContainerHash<ComponentIDs>()(new_archetype_id)); // construct or get archetype using the id

			const auto component		= m_component_map[add_component_id].get();
			const auto component_size	= component->GetSize();

			const auto current_size	= new_archetype->entities.size() * component_size;
			const auto new_size		= current_size + component_size;

			if (new_size > new_archetype->component_data_size[0])
				MakeRoom(new_archetype, component, component_size, 0); // make room and move over existing data

			add_component = new(&new_archetype->component_data[0][current_size])
				C(std::forward<Args>(args)...);
		}

		add_component->Created(*this, entity_id);

		new_archetype->entities.push_back(entity_id);
		record.index = IDType(new_archetype->entities.size() - 1);
		record.archetype = new_archetype;

		return add_component;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::AddComponents(const EntityID entity_id)
	{
		constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		constexpr auto archetype_id = cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		AddComponents(entity_id, { component_ids.begin(), component_ids.end() }, archetype_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::AddComponents(const EntityID entity_id, std::tuple<Cs...>&& tuple)
	{
		AddComponents<Cs...>(entity_id);
	}

	template<IsComponent C>
	inline bool EntityAdmin::RemoveComponent(const EntityID entity_id)
	{
		return RemoveComponent(entity_id, GetComponentID<C>());
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool EntityAdmin::RemoveComponents(const EntityID entity_id)
	{
		constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		constexpr auto archetype_id = cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		return RemoveComponents(entity_id, { component_ids.begin(), component_ids.end() }, archetype_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline bool EntityAdmin::RemoveComponents(const EntityID entity_id, std::tuple<Cs...>&& tuple)
	{
		return RemoveComponents<Cs...>(entity_id);
	}

	template<IsComponent C>
	inline C& EntityAdmin::GetComponent(const EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>()); // component should be registered

		constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto& record = m_entity_archetype_map.at(entity_id);
		const auto* archetype = record.archetype;

		const auto& map = m_component_archetypes_map.at(component_id);
		const auto& arch_record = map.at(archetype->id);
		
		C* components = reinterpret_cast<C*>(&archetype->component_data[arch_record.column][0]);
		return components[record.index];
	}

	template<IsComponent C>
	inline std::optional<C*> EntityAdmin::TryGetComponent(const EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>()); // component should be registered

		const auto eit = m_entity_archetype_map.find(entity_id);
		if (eit == m_entity_archetype_map.end())
			return std::nullopt;

		const auto& record		= eit->second;
		const auto* archetype	= record.archetype;

		if (archetype == nullptr)
			return std::nullopt;

		constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto cit = m_component_archetypes_map.find(component_id);
		if (cit == m_component_archetypes_map.end())
			return std::nullopt;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return std::nullopt;

		const auto& arch_record = ait->second;

		C* components = reinterpret_cast<C*>(&archetype->component_data[arch_record.column][0]);
		return &components[record.index];
	}

	template<class B>
	inline B& EntityAdmin::GetBase(const EntityID entity_id, const ComponentTypeID child_component_id, const std::size_t offset) const
	{
		const auto& record = m_entity_archetype_map.at(entity_id);
		const auto* archetype = record.archetype;

		const auto& map = m_component_archetypes_map.at(child_component_id);
		const auto& arch_record = map.at(archetype->id);

		const auto component = m_component_map.at(child_component_id).get();
		const auto component_size	= component->GetSize();

		auto ptr = &archetype->component_data[arch_record.column][record.index * component_size];
		B* base_component = reinterpret_cast<B*>(ptr + offset);	

		return *base_component;
	}

	template<class B>
	inline std::optional<B*> EntityAdmin::TryGetBase(const EntityID entity_id, const ComponentTypeID child_component_id, const std::size_t offset) const
	{
		const auto eit = m_entity_archetype_map.find(entity_id);
		if (eit == m_entity_archetype_map.end())
			return std::nullopt;

		const auto& record = eit->second;
		const auto* archetype = record.archetype;

		if (archetype == nullptr)
			return std::nullopt;

		const auto cit = m_component_archetypes_map.find(child_component_id);
		if (cit == m_component_archetypes_map.end())
			return std::nullopt;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return std::nullopt;

		const auto iit = m_component_map.find(child_component_id);
		if (iit == m_component_map.end())
			return std::nullopt;

		const auto component_size = iit->second->GetSize();

		auto ptr = &archetype->component_data[ait->second.column][record.index * component_size];
		B* base_component = reinterpret_cast<B*>(ptr + offset);

		return base_component;
	}

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline C& EntityAdmin::SetComponent(const EntityID entity_id, Args&&... args)
	{
		C& old_component = GetComponent<C>(entity_id);
		C new_component(std::forward<Args>(args)...);

		static_cast<IComponent&>(old_component).Modified(
			*this, entity_id, static_cast<IComponent&>(new_component));

		old_component = std::move(new_component);

		return old_component;
	}

	template<IsComponent C, typename ...Args> requires std::constructible_from<C, Args...>
	inline std::optional<C*> EntityAdmin::TrySetComponent(const EntityID entity_id, Args&&... args)
	{
		assert(IsComponentRegistered<C>()); // component should be registered

		const auto opt_component = TryGetComponent<C>(entity_id);

		if (!opt_component)
			return std::nullopt;

		C* old_component = opt_component.value();
		C new_component(std::forward<Args>(args)...);

		static_cast<IComponent&>(*old_component).Modified(
			*this, entity_id, static_cast<IComponent&>(new_component));

		*old_component = std::move(new_component);

		return old_component;
	}

	template<IsComponent C>
	inline ComponentRef<C> EntityAdmin::GetComponentRef(const EntityID entity_id, C* component) const
	{
		assert(IsComponentRegistered<C>());

		auto& component_refs = m_entity_component_ref_map[entity_id]; // will construct new if it does not exist
		constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto cit = component_refs.find(component_id);
		if (cit == component_refs.end() || ((cit->second.flag & RefFlag::Component) == RefFlag::Component && cit->second.component_ptr.expired())) // it does not yet exist or has expired
		{
			if (component == nullptr)
				component = &GetComponent<C>(entity_id);

			auto ptr = std::make_shared<IComponent*>(component);

			auto& entry = component_refs[component_id];
			entry.flag |= RefFlag::Component;
			entry.component_ptr = ptr;

			return ComponentRef<C>(entity_id, ptr);
		}

		return ComponentRef<C>(entity_id, cit->second.component_ptr.lock());
	}

	template<IsComponent C>
	inline std::optional<ComponentRef<C>> EntityAdmin::TryGetComponentRef(const EntityID entity_id, C* component) const
	{
		if (!IsEntityRegistered(entity_id) || !HasComponent<C>(entity_id))
			return std::nullopt;

		return GetComponentRef<C>(entity_id, component);
	}

	template<class B>
	inline BaseRef<B> EntityAdmin::GetBaseRef(const EntityID entity_id, const ComponentTypeID child_component_id, const std::uint32_t offset, B* base) const
	{
		auto& component_refs = m_entity_component_ref_map[entity_id]; // will construct new if it does not exist

		const auto cit = component_refs.find(child_component_id);
		if (cit == component_refs.end() || ((cit->second.flag & RefFlag::Base) == RefFlag::Base && cit->second.base.ptr.expired())) // it does not yet exist, create new one
		{
			if (base == nullptr)
				base = &GetBase<B>(entity_id, child_component_id, offset);

			auto ptr = std::make_shared<void*>(static_cast<void*>(base));

			auto& entry = component_refs[child_component_id];
			entry.flag |= RefFlag::Base;
			entry.base.ptr = ptr;
			entry.base.offset = offset;

			return BaseRef<B>(entity_id, ptr);
		}

		return BaseRef<B>(entity_id, cit->second.base.ptr.lock());
	}

	template<class B>
	inline std::optional<BaseRef<B>> EntityAdmin::TryGetBaseRef(const EntityID entity_id, const ComponentTypeID child_component_id, const std::uint32_t offset, B* base) const
	{
		if (!IsEntityRegistered(entity_id) || !HasComponent(entity_id, child_component_id)) // check if entity exists and has component
			return std::nullopt;

		return GetBaseRef<B>(entity_id, child_component_id, offset, base);
	}

	template<IsComponent C>
	inline bool EntityAdmin::HasComponent(const EntityID entity_id) const
	{
		assert(IsComponentRegistered<C>()); // component should be registered
		return HasComponent(entity_id, GetComponentID<C>());
	}

	template<IsComponent C>
	inline bool EntityAdmin::IsComponentRegistered() const
	{
		return m_component_map.contains(GetComponentID<C>());
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) > 1)
	inline std::tuple<Cs*...> EntityAdmin::GetComponents(const EntityID entity_id) const
	{
		using ComponentTypes = std::tuple<Cs...>;

		std::tuple<Cs*...> result;

		const auto& record = m_entity_archetype_map.at(entity_id);

		(([this, &result]<class C, std::size_t N>(const Record& record) -> void
		{
			constexpr ComponentTypeID component_id = GetComponentID<C>();

			const auto& map = m_component_archetypes_map.at(component_id);
			const auto& arch_record = map.at(record.archetype->id);

			C* components = reinterpret_cast<C*>(&record.archetype->component_data[arch_record.column][0]);
			std::get<N>(result) = &components[record.index];
		}.operator()<Cs, traits::IndexInTuple<Cs, ComponentTypes>::value>(record)), ...);

		return result;
	}

	template<class... Cs> requires (IsComponents<Cs...> && sizeof...(Cs) == 1)
	inline std::tuple_element_t<0, std::tuple<Cs...>>& EntityAdmin::GetComponents(const EntityID entity_id) const
	{
		return GetComponent<Cs...>(entity_id);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline ComponentSet<Cs...> EntityAdmin::GetComponentsRef(const EntityID entity_id) const
	{
		return ComponentSet<Cs...>(GetComponentRef<Cs>(entity_id, &GetComponent<Cs>(entity_id))...);
	}

	template<IsComponent C>
	inline static constexpr ComponentTypeID EntityAdmin::GetComponentID()
	{
		return ComponentAlloc<C>::GetTypeID();
	}

	template<class... Cs, class Comp> requires IsComponents<Cs...>
	inline void EntityAdmin::SortComponents(Comp&& comparison) requires SameTypeParameter<Comp, std::tuple_element_t<0, std::tuple<Cs...>>, 0, 1>
	{
		constexpr auto component_ids	= cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		constexpr auto archetype_id		= cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		assert(cu::IsSorted(component_ids));

		const auto it = m_archetype_map.find(archetype_id);
		if (it == m_archetype_map.end())
			return;

		Archetype* archetype = it->second;

		if (archetype->id != archetype_id)
			throw std::runtime_error("the specified archetype does not exist");

		using C = std::tuple_element_t<0, std::tuple<Cs...>>; // the component that is meant to be sorted
		constexpr ComponentTypeID component_id = GetComponentID<C>();

		const auto cit = m_component_archetypes_map.find(component_id);
		if (cit == m_component_archetypes_map.end())
			return;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return;

		const ArchetypeRecord& a_record = ait->second;

		C* components = reinterpret_cast<C*>(&archetype->component_data[a_record.column][0]);

		std::vector<std::size_t> indices(archetype->entities.size());
		std::iota(indices.begin(), indices.end(), 0);

		std::stable_sort(indices.begin(), indices.end(),
			[&comparison, &components](const std::size_t lhs, std::size_t rhs)
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
			const auto index		= indices[i];
			const auto entity_id	= archetype->entities[index];

			auto it = m_entity_archetype_map.find(entity_id);
			assert(it != m_entity_archetype_map.end()); // should never happen

			it->second.index = i;
			new_entities.push_back(entity_id);
		}

		archetype->entities = new_entities;
	}

	template<IsComponent C, class Comp> requires SameTypeParameter<Comp, C, 0, 1>
	inline void EntityAdmin::SortComponents(const EntityID entity, Comp&& comparison)
	{
		const auto it = m_entity_archetype_map.find(entity);
		if (it == m_entity_archetype_map.end())
			return;

		Archetype* archetype = it->second.archetype;

		if (archetype == nullptr)
			return;

		constexpr auto component_id = GetComponentID<C>();

		const auto cit = m_component_archetypes_map.find(component_id);
		if (cit == m_component_archetypes_map.end())
			return;

		const auto ait = cit->second.find(archetype->id);
		if (ait == cit->second.end())
			return;

		const ArchetypeRecord& a_record = ait->second;

		C* components = reinterpret_cast<C*>(&archetype->component_data[a_record.column][0]);

		std::vector<std::size_t> indices(archetype->entities.size());
		std::iota(indices.begin(), indices.end(), 0);

		std::stable_sort(indices.begin(), indices.end(),
			[&comparison, &components](const std::size_t lhs, std::size_t rhs)
			{
				return std::forward<Comp>(comparison)(components[lhs], components[rhs]);
			});

		for (std::size_t i = 0; i < archetype->type.size(); ++i) // sort the components, all need to be sorted
		{
			const auto component_id = archetype->type[i];
			const auto component = m_component_map[component_id].get();
			const auto component_size = component->GetSize();

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
			const std::size_t index = indices[i];
			const EntityID entity_id = archetype->entities[index];

			auto it = m_entity_archetype_map.find(entity_id);
			assert(it != m_entity_archetype_map.end()); // should never happen

			it->second.index = i;
			new_entities.push_back(entity_id);
		}

		archetype->entities = new_entities;
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline void EntityAdmin::Reserve(const std::size_t component_count)
	{
		constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		constexpr auto archetype_id = cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		Reserve(component_ids, archetype_id, component_count);
	}

	template<class... Cs> requires IsComponents<Cs...>
	inline std::vector<EntityID> EntityAdmin::GetEntitiesWith(bool restricted) const
	{
		constexpr auto component_ids = cu::Sort<ArrComponentIDs<Cs...>>({ GetComponentID<Cs>()... });
		constexpr auto archetype_id = cu::ContainerHash<ArrComponentIDs<Cs...>>()(component_ids);

		return GetEntitiesWith(component_ids, archetype_id, restricted);
	}

	template<IsContainer T>
	inline std::vector<EntityID> EntityAdmin::GetEntitiesWith(const T& component_ids, const ArchetypeID archetype_id, bool restricted) const
	{
		assert(cu::IsSorted(component_ids));

		std::vector<EntityID> entities;

		if (!restricted)
		{
			for (const Archetype* archetype : GetArchetypes(component_ids, archetype_id))
			{
				entities.insert(entities.end(),
					archetype->entities.begin(),
					archetype->entities.end());
			}
		}
		else
		{
			const auto it = m_archetype_map.find(archetype_id);
			if (it == m_archetype_map.end())
				return entities;

			entities = it->second->entities;
		}

		return entities;
	}

	template<IsContainer T>
	inline void EntityAdmin::Reserve(const T& component_ids, const ArchetypeID archetype_id, const std::size_t component_count)
	{
		assert(cu::IsSorted(component_ids));

		Archetype* archetype = GetArchetype(component_ids, archetype_id);
		for (std::size_t i = 0; i < archetype->type.size(); ++i)
		{
			const auto component		= m_component_map[archetype->type[i]].get();
			const auto component_size	= component->GetSize();

			const auto current_size	= archetype->component_data_size[i];
			const auto new_size		= component_count * component_size;

			if (new_size > current_size) // only reserve if larger than current size
			{
				ComponentData new_data = std::make_unique<ByteArray>(new_size);

				for (std::size_t j = 0; j < archetype->entities.size(); ++j)
				{
					component->MoveDestroyData(*this, archetype->entities[j],
						&archetype->component_data[i][j * component_size],
						&new_data[j * component_size]);
				}

				archetype->component_data_size[i]	= new_size;
				archetype->component_data[i]		= std::move(new_data);
			}
		}
	}

	template<IsContainer T>
	inline const std::vector<Archetype*>& EntityAdmin::GetArchetypes(const T& component_ids, const ArchetypeID archetype_id) const
	{
		const auto it = m_archetype_cache.find(archetype_id);
		if (it != m_archetype_cache.end())
			return it->second;

		std::vector<Archetype*> result;
		for (const ArchetypePtr& archetype : m_archetypes)
		{
			if (std::includes(archetype->type.begin(), archetype->type.end(), component_ids.begin(), component_ids.end()))
				result.push_back(archetype.get());
		}

		return m_archetype_cache.emplace(archetype_id, result).first->second;
	}

	template<IsContainer T>
	inline Archetype* EntityAdmin::GetArchetype(const T& component_ids, const ArchetypeID archetype_id)
	{
		assert(cu::IsSorted(component_ids));

		const auto it = m_archetype_map.find(archetype_id);
		if (it != m_archetype_map.end())
			return it->second;

		return CreateArchetype(component_ids, archetype_id); // archetype does not exist, create new one
	}

	template<IsContainer T>
	inline Archetype* EntityAdmin::CreateArchetype(const T& component_ids, const ArchetypeID archetype_id)
	{
		assert(cu::IsSorted(component_ids));

		ArchetypePtr new_archetype = std::make_unique<Archetype>();

		new_archetype->id	= archetype_id;
		new_archetype->type = { component_ids.begin(), component_ids.end() };

		m_archetype_map[archetype_id] = new_archetype.get();

		//new_archetype->component_data.reserve(new_archetype->type.size()); // prevent any reallocations
		//new_archetype->component_data_size.reserve(new_archetype->type.size());

		for (std::size_t i = 0; i < new_archetype->type.size(); ++i) // add empty array for each component in type
		{
			constexpr std::size_t DEFAULT_SIZE = 64; // default size in bytes to reduce number of reallocations

			new_archetype->component_data.push_back(std::make_unique<ByteArray>(DEFAULT_SIZE));
			new_archetype->component_data_size.push_back(DEFAULT_SIZE);

			m_component_archetypes_map[new_archetype->type[i]][archetype_id].column = ColumnType(i);
		}

#if defined(VELOX_DEBUG)
		for (const auto& archetype : m_archetypes)
		{
			for (const auto& archetype1 : m_archetypes)
			{
				if (archetype.get() == archetype1.get())
					continue;

				assert(archetype->type != archetype1->type); // no duplicates
			}
		}
#endif

		m_archetype_cache.clear(); // unfortunately for now, we'll have to clear the cache whenever an archetype has been added

		return m_archetypes.emplace_back(std::move(new_archetype)).get();
	}

	template<IsComponent C>
	inline void EntityAdmin::EraseComponentRef(const EntityID entity_id) const
	{
		EraseComponentRef(entity_id, GetComponentID<C>());
	}

	template<IsComponent C>
	inline void EntityAdmin::UpdateComponentRef(const EntityID entity_id, C* new_component) const
	{
		UpdateComponentRef(entity_id, GetComponentID<C>(), static_cast<IComponent*>(new_component));
	}
}