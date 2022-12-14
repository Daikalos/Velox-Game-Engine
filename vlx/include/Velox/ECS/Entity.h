#pragma once

#include <Velox/Utilities.hpp>

#include "Identifiers.hpp"
#include "EntityAdmin.h"
#include "ComponentProxy.hpp"

namespace vlx
{
	class Entity
	{
	public:
		VELOX_API explicit Entity(EntityAdmin& entity_admin, const EntityID entity_id);
		VELOX_API explicit Entity(EntityAdmin& entity_admin);
		VELOX_API Entity(Entity&& entity) noexcept;

		VELOX_API virtual ~Entity();

		VELOX_API [[nodiscard]] constexpr EntityID GetID() const noexcept;

	public:
		VELOX_API [[nodiscard]] EntityID Duplicate() const;

		VELOX_API void Destroy();

	public:
		template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
		[[nodiscard]] auto AddComponent(Args&&... args);
		template<IsComponent C>
		[[nodiscard]] auto AddComponent(C&& comp);

		template<IsComponent... Cs>
		void AddComponents();

		template<IsComponent C>
		void RemoveComponent();

		template<IsComponent C>
		[[nodiscard]] auto& GetComponent() const;
		template<IsComponent C>
		[[nodiscard]] auto TryGetComponent() const;

		template<IsComponent C>
		[[nodiscard]] auto& GetComponentProxy() const;
		template<IsComponent C>
		[[nodiscard]] auto TryGetComponentProxy() const;

		template<IsComponent... Cs>
		[[nodiscard]] auto GetComponents() const;

		template<IsComponent C>
		[[nodiscard]] auto HasComponent() const;

	protected:
		EntityID		m_id; // entity is just an id
		EntityAdmin*	m_entity_admin;
	};

	template<IsComponent C, typename... Args> requires std::constructible_from<C, Args...>
	inline auto Entity::AddComponent(Args&&... args)
	{
		return m_entity_admin->AddComponent<C>(m_id, std::forward<Args>(args)...);
	}

	template<IsComponent C>
	inline auto Entity::AddComponent(C&& comp)
	{
		return m_entity_admin->AddComponent<C>(m_id, std::forward<C>(comp));
	}

	template<IsComponent... Cs>
	inline void Entity::AddComponents()
	{
		m_entity_admin->AddComponents<Cs...>(m_id);
	}

	template<IsComponent C>
	inline void Entity::RemoveComponent()
	{
		m_entity_admin->RemoveComponent<C>(m_id);
	}

	template<IsComponent C>
	inline auto& Entity::GetComponent() const
	{
		return m_entity_admin->GetComponent<C>(m_id);
	}

	template<IsComponent C>
	inline auto Entity::TryGetComponent() const
	{
		return m_entity_admin->TryGetComponent<C>(m_id);
	}

	template<IsComponent C>
	inline auto& Entity::GetComponentProxy() const
	{
		return m_entity_admin->GetComponentProxy<C>(m_id);
	}
	template<IsComponent C>
	inline auto Entity::TryGetComponentProxy() const
	{
		return m_entity_admin->TryGetComponentProxy<C>(m_id);
	}

	template<IsComponent... Cs>
	inline auto Entity::GetComponents() const
	{
		return m_entity_admin->GetComponents<Cs...>(m_id);
	}

	template<IsComponent C>
	inline auto Entity::HasComponent() const
	{
		return m_entity_admin->HasComponent<C>(m_id);
	}
}