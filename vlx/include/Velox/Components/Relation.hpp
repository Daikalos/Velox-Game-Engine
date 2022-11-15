#pragma once

#include <unordered_set>
#include <type_traits>
#include <functional>

#include <Velox/ECS/EntityAdmin.h>
#include <Velox/ECS/Identifiers.hpp>
#include <Velox/Utilities.hpp>
#include <Velox/Config.hpp>

namespace vlx
{
	/// <summary>
	///		Represents the relationship between entities that allows for scene graphs and possibly other things... 
	///		Is NOT a standard component, rather a property that other components can inherit to allow this feature
	/// </summary>
	template<class T>
	class Relation : public IComponent
	{
	private:
		template<class C>
		using CompFunc = std::function<void(C&)>;

		template<class C>
		using SortFunc = std::function<bool(const C&, const C&)>;

	public:
		virtual ~Relation() = 0; // to make abstract

	protected:
		/// <summary>
		///		When the relation is destroyed, we need to detach it accordingly
		/// </summary>
		virtual void Destroyed(const EntityAdmin& entity_admin, const EntityID entity_id) override;
		virtual void Modified(const EntityAdmin& entity_admin, const EntityID entity_id, const IComponent& new_data) override;

		virtual void OnAttach(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child) {}
		virtual void OnDetach(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child) {}

	public:
		void AttachChild(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child);
		const EntityID DetachChild(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child);

		[[nodiscard]] constexpr bool HasParent() const noexcept;

		template<class C>
		void IterateChildren(const CompFunc<C>& func, const EntityAdmin& entity_admin, bool include_descendants = true);

		template<class C>
		void SortChildren(const SortFunc<C>& func, const EntityAdmin& entity_admin, bool include_descendants = true);

	private:
		void PropagateAttach(const EntityAdmin& entity_admin, const EntityID child_id, const Relation<T>& child);
		void PropagateDetach(const EntityAdmin& entity_admin, const EntityID child_id, const Relation<T>& child);

	protected:
		EntityID						m_parent	{NULL_ENTITY};
		std::vector<EntityID>			m_children;

	private:
		std::unordered_set<EntityID>	m_closed; // list of all descendants (to prevent parenting them when attaching)
	};

	template<class T>
	inline Relation<T>::~Relation() = default;

	template<class T>
	inline void Relation<T>::Destroyed(const EntityAdmin& entity_admin, const EntityID entity_id)
	{
		if (HasParent())
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(m_parent))
				.DetachChild(entity_admin, m_parent, entity_id, *this);
		}

		for (const EntityID child : m_children)
		{
			DetachChild(entity_admin, entity_id, child, static_cast<Relation<T>&>(entity_admin.GetComponent<T>(child)));
		}
	}

	template<class T>
	inline void Relation<T>::Modified(const EntityAdmin& entity_admin, const EntityID entity_id, const IComponent& new_data)
	{
		Destroyed(entity_admin, entity_id);

		const Relation<T>& new_relation = static_cast<const Relation<T>&>(new_data);

		if (new_relation.HasParent())
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(new_relation.m_parent))
				.AttachChild(entity_admin, new_relation.m_parent, entity_id, *this);
		}

		for (const EntityID child : new_relation.m_children)
		{
			AttachChild(entity_admin, entity_id, child, entity_admin.GetComponent<T>(child));
		}
	}

	template<class T>
	inline void Relation<T>::AttachChild(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child)
	{
		if (m_closed.contains(child_id))
			throw std::runtime_error("The new parent cannot be a descendant of the child");

		if (child.m_parent == entity_id) // child is already correctly parented
			return;

		if (m_parent == child_id) // special case
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(m_parent))
				.DetachChild(entity_admin, m_parent, entity_id, *this);
		}

		if (child.m_parent != NULL_ENTITY) // if child already has an attached parent we need to detach it
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(child.m_parent))
				.DetachChild(entity_admin, child.m_parent, child_id, child);
		}

		child.m_parent = entity_id;
		m_children.push_back(child_id);

		PropagateAttach(entity_admin, child_id, child);
		child.OnAttach(entity_admin, entity_id, child_id, child);
	}

	template<class T>
	inline const EntityID Relation<T>::DetachChild(const EntityAdmin& entity_admin, const EntityID entity_id, const EntityID child_id, Relation<T>& child)
	{
		auto found = std::find(m_children.begin(), m_children.end(), child_id);

		if (found == m_children.end())
			return NULL_ENTITY;

		PropagateDetach(entity_admin, child_id, child);
		child.OnDetach(entity_admin, entity_id, child_id, child);

		child.m_parent = NULL_ENTITY;

		*found = m_children.back();
		m_children.pop_back();

		return child_id;
	}

	template<class T>
	template<class C>
	inline void Relation<T>::IterateChildren(const CompFunc<C>& func, const EntityAdmin& entity_admin, bool include_descendants)
	{
		for (const EntityID child_id : m_children)
		{
			auto [component, success] = entity_admin.TryGetComponent<C>(child_id);

			if (success)
			{
				func(*component);
			}

			if (include_descendants)
			{
				static_cast<Relation<T>&>(entity_admin.GetComponent<T>(child_id))
					.IterateChildren<C>(entity_admin, func, include_descendants);
			}
		}
	}

	template<class T>
	template<class C>
	inline void Relation<T>::SortChildren(const SortFunc<C>& func, const EntityAdmin& entity_admin, bool include_descendants)
	{
		std::sort(m_children.begin(), m_children.end(),
			[&func, &entity_admin](const EntityID lhs, const EntityID rhs)
			{
				const auto [lhs_comp, lhs_success] = entity_admin.TryGetComponent<C>(lhs);

				if (!lhs_success)
					return false;

				const auto [rhs_comp, rhs_success] = entity_admin.TryGetComponent<C>(rhs);

				if (!rhs_success)
					return false;

				return func(*lhs_comp, *rhs_comp);
			});

		if (include_descendants)
		{
			for (const EntityID child_id : m_children)
			{
				static_cast<Relation<T>&>(entity_admin.GetComponent<T>(child_id))
					.SortChildren(func, entity_admin, include_descendants);
			}
		}
	}

	template<class T>
	inline constexpr bool Relation<T>::HasParent() const noexcept
	{
		return m_parent != NULL_ENTITY;
	}

	template<class T>
	inline void Relation<T>::PropagateAttach(const EntityAdmin& entity_admin, const EntityID child_id, const Relation<T>& child)
	{
#ifdef VELOX_DEBUG // check so that it does not exist
		assert(!m_closed.contains(child_id));
		for (const EntityID entity : child.m_closed)
			assert(!m_closed.contains(entity));
#endif

		m_closed.insert(child_id); // add child and all of its descendants
		m_closed.insert(child.m_closed.begin(), child.m_closed.end());

		if (HasParent())
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(m_parent))
				.PropagateAttach(entity_admin, child_id, child);
		}
	}

	template<class T>
	inline void Relation<T>::PropagateDetach(const EntityAdmin& entity_admin, const EntityID child_id, const Relation<T>& child)
	{
#ifdef VELOX_DEBUG // check so that they do exist
		assert(m_closed.contains(child_id));
		for (const EntityID entity : child.m_closed)
			assert(m_closed.contains(entity));
#endif

		m_closed.erase(child_id); // remove child and all of its descendants
		m_closed.erase(child.m_closed.begin(), child.m_closed.end());

		if (HasParent())
		{
			static_cast<Relation<T>&>(entity_admin.GetComponent<T>(m_parent))
				.PropagateDetach(entity_admin, child_id, child);
		}
	}
}