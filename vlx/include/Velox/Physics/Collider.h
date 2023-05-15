#pragma once

#include <Velox/Algorithms/QTElement.hpp>
#include <Velox/System/Event.hpp>
#include <Velox/VeloxTypes.hpp>
#include <Velox/Config.hpp>

#include "CollisionResult.h"
#include "CollisionLayer.h"

namespace vlx
{
	using QTCollider = QTElement<uint32>;

	class VELOX_API Collider : public QTCollider
	{
	public:
		Event<const CollisionResult&>	OnEnter;	// called when collider enters another collider
		Event<EntityID>					OnExit;		// called when collider exits another collider
		Event<const CollisionResult&>	OnOverlap;	// called when collider overlaps another collider

	public:
		bool GetEnabled() const noexcept;
		void SetEnabled(bool flag);

	public:
		CollisionLayer layer;

	private:
		bool enabled	{true};
		bool dirty		{true}; // if should update the AABB in the quadtree

		friend class PhysicsDirtySystem;
		friend class BroadSystem;
		friend class NarrowSystem;
		friend class PhysicsSystem;
	};
}