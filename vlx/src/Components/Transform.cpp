#include <Velox/Components/Transform.h>

using namespace vlx;

Transform::Transform() : m_global_transform(GetLocalTransform())
{

}

Transform::~Transform()
{
	if (HasParent())
		m_parent->DetachChild(*this);

	for (Transform* child : m_children)
		child->m_parent = nullptr;
}

const sf::Transform& Transform::GetTransform() const
{
	if (m_update_transform)
	{
		auto& parent = *GetTopParent();
		if (!parent.m_update_transform)
		{
			for (const Transform* child : m_children)
				child->UpdateTransforms(parent.m_global_transform);
		}
		else
			parent.UpdateTransforms(sf::Transform::Identity);
	}

	return m_global_transform;
}

const sf::Transform& Transform::GetLocalTransform() const
{
	return m_transform.getTransform();
}

const sf::Vector2f& Transform::GetPosition() const 
{
	return sf::Vector2f();
}
const sf::Vector2f& Transform::GetScale() const 
{
	return sf::Vector2f();
}
const sf::Angle& Transform::GetRotation() const 
{
	return sf::Angle();
}

void Transform::SetOrigin(const sf::Vector2f& origin)
{
	m_transform.setOrigin(origin);
	UpdateRequired();
}
void Transform::SetPosition(const sf::Vector2f& position)
{
	m_transform.setPosition(position);
	UpdateRequired();
}
void Transform::SetScale(const sf::Vector2f& scale) 
{
	m_transform.setScale(scale);
	UpdateRequired();
}
void Transform::SetRotation(float degrees)
{
	m_transform.setRotation(sf::degrees(degrees));
	UpdateRequired();
}

[[nodiscard]] constexpr bool Transform::HasParent() const noexcept
{
	return m_parent != nullptr;
}

const Transform& Transform::GetParent() const
{
	assert(HasParent());
	return *m_parent;
}
Transform& Transform::GetParent()
{
	assert(HasParent());
	return *m_parent;
}

void Transform::AttachChild(Transform& child)
{
	child.m_parent = this;
	m_children.push_back(&child);

	child.m_update_transform = true;
}
Transform* Transform::DetachChild(Transform& child)
{
	auto found = std::find(m_children.begin(), m_children.end(), &child);

	assert(found != m_children.end());

	Transform* result = *found;
	(*found)->m_parent = nullptr;
	m_children.erase(found);

	return result;
}

const Transform* Transform::GetTopParent() const
{
	if (m_parent == nullptr || !m_update_transform)
		return this;

	return m_parent->GetTopParent(); // recursive way to get the top parent
}

void Transform::UpdateTransforms(sf::Transform transform) const
{
	m_global_transform = transform * GetLocalTransform();
	m_update_transform = false;

	for (const Transform* child : m_children)
		child->UpdateTransforms(m_global_transform);
}

void Transform::UpdateRequired() const
{
	m_update_transform = true;

	for (const Transform* transform : m_children) // all of the children needs their transform to be updated
		transform->UpdateRequired();
}