#pragma once

#include <Velox/ECS/IComponent.h>
#include <Velox/Graphics/Components/GlobalTransform.h>
#include <Velox/System/Rectangle.hpp>
#include <Velox/System/Event.hpp>
#include <Velox/System/Mat2f.hpp>
#include <Velox/Config.hpp>

#include "../CollisionResult.h"
#include "../PhysicsBody.h"

namespace vlx
{
	class VELOX_API Shape : public IComponent
	{
	public:
		enum Type : short
		{
			None = -1,
			Circle,
			Box,
			Point,
			Convex,

			// considering adding line, triangle and capsule in the future

			Count // always keep last
		};

	public:
		// virtual ~Shape() = default; // i dont think this is needed

	public:
		const Mat2f& GetOrientation() const;
		const RectFloat& GetAABB() const;
		Vector2f GetPosition() const;
		Vector2f GetCenter() const;

	private:
		void UpdateTransform(const GlobalTransform& transform);

	protected:
		mutable Mat2f	m_orientation; // rotation matrix
		RectFloat		m_aabb;
		sf::Angle		m_angle;
		mutable bool	m_update {true};

		friend class PhysicsDirtySystem;
	};

	template<class S>
	class ShapeCRTP : public Shape
	{
		constexpr auto GetType() const noexcept -> Shape::Type;
		void Initialize(PhysicsBody& body) const;
		void UpdateAABB(const GlobalTransform& transform);

		friend class PhysicsDirtySystem;
	};

	template<class S>
	inline constexpr auto ShapeCRTP<S>::GetType() const noexcept -> Type
	{
		return static_cast<const S*>(this)->GetType();
	}

	template<class S>
	inline void ShapeCRTP<S>::Initialize(PhysicsBody& body) const
	{
		static_cast<const S*>(this)->InitializeImpl(body);
	}

	template<class S>
	inline void ShapeCRTP<S>::UpdateAABB(const GlobalTransform& transform)
	{
		static_cast<S*>(this)->UpdateAABBImpl(transform);
	}
}