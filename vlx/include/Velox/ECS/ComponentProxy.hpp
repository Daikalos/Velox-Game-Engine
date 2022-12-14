#pragma once

#include <Velox/Utilities.hpp>

#include "Identifiers.hpp"

namespace vlx
{
	class EntityAdmin;

	class IComponentProxy : private NonCopyable
	{
	public:
		virtual ~IComponentProxy() = default;

		virtual bool IsValid() const noexcept = 0;

		virtual void Reset() = 0;
	};

	/// <summary>
	///		The ComponentProxy is to ensure that the component pointers remains valid even after the internal data of the ECS
	///		has been modified. This is to prevent having to write GetComponent everywhere all the time.
	/// </summary>
	template<IsComponent C>
	class ComponentProxy final : public IComponentProxy
	{
	public:
		ComponentProxy(const EntityAdmin& entity_admin, const EntityID entity_id);

	public:
		C* operator->();
		const C* operator->() const;

		C& operator*();
		const C& operator*() const;

		operator bool() const noexcept;

	public:
		C* Get();
		const C* Get() const;

	public:
		[[nodiscard]] bool IsValid() const noexcept override;
		[[nodiscard]] constexpr EntityID GetEntityID() const noexcept;

		void Reset() override;

	private:
		const EntityAdmin*	m_entity_admin	{nullptr};
		EntityID			m_entity_id		{NULL_ENTITY};

		C*					m_component		{nullptr};
	};

	template<IsComponent C>
	inline ComponentProxy<C>::ComponentProxy(const EntityAdmin& entity_admin, const EntityID entity_id)
		: m_entity_admin(&entity_admin), m_entity_id(entity_id) { }

	template<IsComponent C>
	inline C* ComponentProxy<C>::operator->()
	{
		return Get();
	}

	template<IsComponent C>
	inline const C* ComponentProxy<C>::operator->() const
	{
		return Get();
	}

	template<IsComponent C>
	inline C& ComponentProxy<C>::operator*()
	{
		return *Get();
	}

	template<IsComponent C>
	inline const C& ComponentProxy<C>::operator*() const
	{
		return *Get();
	}

	template<IsComponent C>
	inline ComponentProxy<C>::operator bool() const noexcept
	{
		return IsValid();
	}

	template<IsComponent C>
	inline constexpr EntityID ComponentProxy<C>::GetEntityID() const noexcept
	{
		return m_entity_id;
	}

	template<IsComponent C>
	inline C* ComponentProxy<C>::Get()
	{
		assert(m_entity_admin != nullptr && m_entity_id != NULL_ENTITY);

		if (!IsValid())
		{
			auto [component, success] = m_entity_admin->TryGetComponent<C>(m_entity_id);

			if (!success)
				throw std::runtime_error(std::format("the entity [{}] does not exist or does not have the [{}] component", m_entity_id, typeid(C).name()));

			m_component = component;
		}

		return m_component;
	}

	template<IsComponent C>
	inline const C* ComponentProxy<C>::Get() const
	{
		return const_cast<ComponentProxy<C>&>(*this).Get();
	}

	template<IsComponent C>
	inline bool ComponentProxy<C>::IsValid() const noexcept
	{
		return m_component != nullptr;
	}

	template<IsComponent C>
	inline void ComponentProxy<C>::Reset()
	{
		m_component = nullptr;
	}
}