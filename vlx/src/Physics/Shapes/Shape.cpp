#include <Velox/Physics/Shapes/Shape.h>

using namespace vlx;

const sf::Transform& Shape::GetTransform() const
{
	return m_transform;
}

const sf::Transform& Shape::GetInverseTransform() const
{
	if (m_update_inverse)
	{
		m_inverse_transform = m_transform.getInverse();
		m_update_inverse = false;
	}

	return m_inverse_transform;
}

const RectFloat& Shape::GetAABB() const
{
	return m_aabb;
}

Vector2f Shape::GetCenter() const
{
	return m_aabb.Center();
}