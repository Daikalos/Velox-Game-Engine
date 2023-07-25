#include <Velox/Physics/Systems/PhysicsDirtySystem.h>

using namespace vlx;

PhysicsDirtySystem::PhysicsDirtySystem(EntityAdmin& entity_admin, LayerType id)
	: SystemAction(entity_admin, id, true),

	m_dirty_transform(	entity_admin, LYR_TRANSFORM), // hijack the transform layer muhahaha
	m_dirty_physics(	entity_admin, id),

	m_circles(			entity_admin, id),
	m_boxes(			entity_admin, id),
	m_points(			entity_admin, id),
	m_polygons(			entity_admin, id)

{
	m_dirty_transform.Each([](Collider& c, Transform& t)
		{
			if (t.m_dirty)
				c.dirty = true;
		});

	m_dirty_physics.Each([](Collider& c, Transform& t)
		{
			if (t.m_dirty)
				c.dirty = true;
		});

	m_circles.Each([](Circle& s, Collider& c, Transform& t)
		{
			if (c.dirty)
			{
				s.UpdateAABB(s.ComputeAABB(t));
				// no need to update the orientation matrix
				s.UpdateCenter(t.GetPosition());

				c.dirty = false;
			}
		});

	m_boxes.Each([](Box& b, Collider& c, Transform& t, TransformMatrix& tm)
		{
			if (c.dirty)
			{
				b.UpdateAABB(b.ComputeAABB(tm.matrix));
				b.UpdateOrientation(t.GetRotation().wrapUnsigned());
				b.UpdateCenter(t.GetPosition());

				c.dirty = false;
			}
		});

	m_points.Each([](Point& p, Collider& c, Transform& t)
		{
			if (c.dirty)
			{
				// no need to update the aabb or orientation matrix
				p.UpdateCenter(t.GetPosition());

				c.dirty = false;
			}
		});

	m_polygons.Each([](Polygon& p, Collider& c, Transform& t, TransformMatrix& tm)
		{
			if (c.dirty)
			{
				p.UpdateAABB(p.ComputeAABB(tm.matrix));
				p.UpdateOrientation(t.GetRotation().wrapUnsigned());
				p.UpdateCenter(t.GetPosition());

				c.dirty = false;
			}
		});

	m_dirty_transform.SetPriority(10000.0f);
}

void PhysicsDirtySystem::FixedUpdate()
{
	Execute();
}
