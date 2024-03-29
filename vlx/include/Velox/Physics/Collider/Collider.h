#pragma once

#include <Velox/Algorithms/QTElement.hpp>

#include <Velox/Types.hpp>
#include <Velox/Config.hpp>

#include "../Collision/CollisionLayer.h"

namespace vlx
{
	using QTBody = QTElement<uint32>; 

	class VELOX_API Collider
	{
	public:
		bool GetEnabled() const noexcept;
		void SetEnabled(bool flag);

	public:
		CollisionLayer layer;

	private:
		bool enabled	{true};	// TODO: remove element from quad tree when disabled
		bool dirty		{true}; // if should update the AABB in the quadtree

		friend class PhysicsDirtySystem;
	};
}