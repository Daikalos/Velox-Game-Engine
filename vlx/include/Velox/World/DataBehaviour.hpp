#pragma once

#include <type_traits>

#include <Velox/ECS/System.hpp>
#include <Velox/ECS/SystemEvent.hpp>

#include "Object.h"

namespace vlx
{
	/// Allows for the user to quickly add game-related behaviour for the data
	/// 
	template<class T, class U> requires std::is_standard_layout_v<U>
	class DataBehaviour
	{
	public:
		DataBehaviour(EntityAdmin& entity_admin);

	protected:
		const EntityAdmin* GetEntityAdmin() const noexcept;
		EntityAdmin* GetEntityAdmin() noexcept;

	private:
		SystemEvent<U>		m_start;
		System<Object, U>	m_pre_update;
		System<Object, U>	m_update;
		System<Object, U>	m_fixed_update;
		System<Object, U>	m_post_update;
		EventID				m_on_add_id;
		EventID				m_on_rmv_id;
	};

	template<class T, class U> requires std::is_standard_layout_v<U>
	inline DataBehaviour<T, U>::DataBehaviour(EntityAdmin& entity_admin)
		: m_start(entity_admin), m_pre_update(entity_admin), m_update(entity_admin), m_fixed_update(entity_admin), m_post_update(entity_admin)
	{
		entity_admin.RegisterComponent<U>();

		if constexpr (requires(T t, U& u) { t.Start(EntityID(), u); })
		{
			m_start.ForceAdd(LYR_OBJECTS_START);
			m_start.Each(
				[this](EntityID entity_id, U& data)
				{
					static_cast<T*>(this)->Start(entity_id, data);
				});

			m_start.OnEnd += [this]() 
			{
				if (m_on_add_id.IsConnected())
					return;

				// start listening for add events
				m_on_add_id = GetEntityAdmin()->template RegisterOnAddListener<U>(
					[this](EntityID entity_id, U& data)
					{
						static_cast<T*>(this)->Start(entity_id, data);
					});
			};
		}

		if constexpr (requires(T t, U& u) { t.Start(u); })
		{
			m_start.ForceAdd(LYR_OBJECTS_START);
			m_start.Each(
				[this](U& data)
				{
					static_cast<T*>(this)->Start(data);
				});

			m_start.OnEnd += [this]() 
			{
				if (m_on_add_id.IsConnected())
					return;

				// start listening for add events
				m_on_add_id = GetEntityAdmin()->template RegisterOnAddListener<U>(
					[this](EntityID entity_id, U& data)
					{
						static_cast<T*>(this)->Start(data);
					});
			};
		}

		if constexpr (requires(T t, U& u) { t.PreUpdate(EntityID(), u); })
		{
			m_pre_update.ForceAdd(LYR_OBJECTS_PRE);
			m_pre_update.Each(
				[this](EntityID entity_id, Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->PreUpdate(entity_id, data);
				});
		}

		if constexpr (requires(T t, U & u) { t.PreUpdate(u); })
		{
			m_pre_update.ForceAdd(LYR_OBJECTS_PRE);
			m_pre_update.Each(
				[this](Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->PreUpdate(data);
				});
		}

		if constexpr (requires(T t, U& u) { t.Update(EntityID(), u); })
		{
			m_update.ForceAdd(LYR_OBJECTS_UPDATE);
			m_update.Each(
				[this](EntityID entity_id, Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->Update(entity_id, data);
				});
		}

		if constexpr (requires(T t, U & u) { t.Update(u); })
		{
			m_update.ForceAdd(LYR_OBJECTS_UPDATE);
			m_update.Each(
				[this](Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->Update(data);
				});
		}

		if constexpr (requires(T t, U& u) { t.FixedUpdate(EntityID(), u); })
		{
			m_fixed_update.ForceAdd(LYR_OBJECTS_FIXED);
			m_fixed_update.Each(
				[this](EntityID entity_id, Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->FixedUpdate(entity_id, data);
				});
		}

		if constexpr (requires(T t, U & u) { t.FixedUpdate(u); })
		{
			m_fixed_update.ForceAdd(LYR_OBJECTS_FIXED);
			m_fixed_update.Each(
				[this](Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->FixedUpdate(data);
				});
		}
		
		if constexpr (requires(T t, U& u) { t.PostUpdate(EntityID(), u); })
		{
			m_post_update.ForceAdd(LYR_OBJECTS_POST);
			m_post_update.Each(
				[this](EntityID entity_id, Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->PostUpdate(entity_id, data);
				});
		}

		if constexpr (requires(T t, U & u) { t.PostUpdate(u); })
		{
			m_post_update.ForceAdd(LYR_OBJECTS_POST);
			m_post_update.Each(
				[this](Object& obj, U& data)
				{
					if (!obj.GetActive())
						return;

					static_cast<T*>(this)->PostUpdate(data);
				});
		}

		if constexpr (requires(T t, U& u) { t.Destroy(EntityID(), u); })
		{
			m_on_rmv_id = GetEntityAdmin()->template RegisterOnRemoveListener<U>(
				[this](EntityID entity_id, U& data)
				{
					static_cast<T*>(this)->Destroy(entity_id, data);
				});
		}

		if constexpr (requires(T t, U & u) { t.Destroy(u); })
		{
			m_on_rmv_id = GetEntityAdmin()->template RegisterOnRemoveListener<U>(
				[this](EntityID entity_id, U& data)
				{
					static_cast<T*>(this)->Destroy(data);
				});
		}
	}

	template<class T, class U> requires std::is_standard_layout_v<U>
	inline const EntityAdmin* DataBehaviour<T, U>::GetEntityAdmin() const noexcept
	{
		return m_update.GetEntityAdmin();
	}
	template<class T, class U> requires std::is_standard_layout_v<U>
	inline EntityAdmin* DataBehaviour<T, U>::GetEntityAdmin() noexcept
	{
		return m_update.GetEntityAdmin();
	}
}