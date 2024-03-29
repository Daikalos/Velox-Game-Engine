#include <Velox/ECS/Entity.h>

using namespace vlx;

Entity::Entity(EntityAdmin& entity_admin)
	: m_id(entity_admin.GetNewEntityID()), m_entity_admin(&entity_admin)
{
	m_entity_admin->RegisterEntity(m_id);
}
Entity::Entity(EntityAdmin& entity_admin, EntityID entity_id)
	: m_id(entity_id), m_entity_admin(&entity_admin)
{
	if (entity_id != NULL_ENTITY && !m_entity_admin->IsEntityRegistered(entity_id)) // register if it has not been already
		m_entity_admin->RegisterEntity(entity_id);
}
Entity::Entity(Entity&& entity) noexcept
	: m_id(entity.m_id), m_entity_admin(entity.m_entity_admin)
{
	entity.m_id = NULL_ENTITY;
	entity.m_entity_admin = nullptr;
}

Entity::~Entity()
{
	if (m_id != NULL_ENTITY && m_entity_admin) // TODO: do we really want to call remove entity when on shutdown? EntityAdmin will destroy all components anyways
		m_entity_admin->RemoveEntity(m_id);
}

Entity::operator EntityID() const noexcept
{
	return m_id;
}
EntityID Entity::GetID() const noexcept
{
	return m_id;
}

Entity& Entity::operator=(Entity&& rhs) noexcept
{
	m_id = rhs.m_id;
	m_entity_admin = rhs.m_entity_admin;

	rhs.m_id = NULL_ENTITY;
	rhs.m_entity_admin = nullptr;

	return *this;
}

Entity Entity::Duplicate() const
{
	return Entity(*m_entity_admin, m_entity_admin->Duplicate(m_id));
}

void Entity::Destroy()
{
	if (m_id != NULL_ENTITY && m_entity_admin)
		m_entity_admin->RemoveEntity(m_id);

	m_id = NULL_ENTITY;
	m_entity_admin = nullptr;
}