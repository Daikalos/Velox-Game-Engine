#include <Velox/Physics/Systems/BroadSystem.h>

#include <Velox/ECS/EntityAdmin.h>

#include <Velox/Physics/BodyTransform.h>
#include <Velox/Physics/PhysicsBody.h>

#include <Velox/Physics/Collision/CollisionBody.h>

#include <Velox/Physics/Collider/ColliderEvents.h>
#include <Velox/Physics/Collider/ColliderAABB.h>

using namespace vlx;

BroadSystem::BroadSystem(EntityAdmin& entity_admin) :
	m_entity_admin(&entity_admin), m_insert(entity_admin),

	m_quad_tree({ -4096, -4096, 4096 * 2, 4096 * 2 }) // hard set size for now
{
	m_insert.Each(&BroadSystem::InsertAABB, this);

	RegisterEvents();
}

void BroadSystem::Update()
{
	m_collisions.clear();

	m_insert.ForceRun(); // insert/erase AABBs in quadtree

	m_quad_tree.Cleanup(); // have to cleanup in case of erase

	GatherCollisions();

	CullDuplicates();
}

auto BroadSystem::GetBodies() const noexcept -> const BodyList&
{
	return m_bodies;
}
auto BroadSystem::GetBodies() noexcept -> BodyList&
{
	return m_bodies;
}

auto BroadSystem::GetCollisions() const noexcept -> const CollisionList&
{
	return m_collisions;
}
auto BroadSystem::GetCollisions() noexcept -> CollisionList&
{
	return m_collisions;
}

const CollisionBody& BroadSystem::GetBody(uint32 i) const
{
	return m_bodies[i];
}

CollisionBody& BroadSystem::GetBody(uint32 i)
{
	return m_bodies[i];
}

void BroadSystem::InsertAABB(EntityID entity_id, ColliderAABB& ab, QTBody& qtb)
{
	if (!qtb.GetEnabled())
		return;

	if (!qtb.Contains(ab.GetAABB()))
	{
		const auto it = m_entity_body_map.find(entity_id);
		if (it == m_entity_body_map.end())
			return;

		assert(it->second != BroadSystem::NULL_BODY);

		qtb.Erase();
		qtb.Insert(m_quad_tree, ab.GetAABB().Inflate(P_AABB_INFLATE), it->second); // TODO: inflate based on velocity
	}
}

void BroadSystem::GatherCollisions()
{
	const auto QueryResult = [this](const CollisionBody& object)
	{
		switch (object.type)
		{
		case Shape::Point:
			return m_quad_tree.Query(object.transform->GetPosition());
		default:
			return m_quad_tree.Query(object.aabb->GetAABB());
		}
	};

	for (std::size_t i = 0; i < m_bodies.size(); ++i)
	{
		const auto& lhs = m_bodies[i];

		if (!HasDataForCollision(lhs)) [[unlikely]] // unlikely since if you added a shape, you will likely have the other data as well
			continue;

		if (!lhs.collider->GetEnabled())
			continue;

		// if both have a physics body, do an early check to see if valid
		const bool lhs_active = (lhs.body ? (lhs.body->IsAwake() && lhs.body->IsEnabled()) : 
			(lhs.enter || lhs.overlap || lhs.exit));

		for (const auto j : QueryResult(lhs))
		{
			const auto& rhs = m_bodies[m_quad_tree.Get(j)];

			if (lhs.entity_id == rhs.entity_id) // skip same body
				continue;

			if (!HasDataForCollision(rhs)) [[unlikely]]
				continue;

			if (!rhs.collider->GetEnabled() || !lhs.collider->layer.HasAny(rhs.collider->layer)) // enabled and matching layer
				continue;

			const bool rhs_active = (rhs.body ? (rhs.body->IsAwake() && rhs.body->IsEnabled()) : 
				(rhs.enter || rhs.overlap || rhs.exit));

			// skip attempting collision if both are either asleep or disabled
			// in the case of no body, skip collision if both have no listeners
			if (!lhs_active && !rhs_active) 
				continue;
		
			m_collisions.emplace_back(i, j);
		}
	}
}

void BroadSystem::CullDuplicates()
{
	std::ranges::sort(m_collisions.begin(), m_collisions.end(),
		[](const CollisionPair& x, const CollisionPair& y)
		{
			return (x.first < y.first) || (x.first == y.first && x.second < y.second);
		});

	const auto [first, last] = std::ranges::unique(m_collisions.begin(), m_collisions.end(),
		[](const CollisionPair& x, const CollisionPair& y)
		{
			return x == y;
		});

	m_collisions.erase(first, last);
}

int BroadSystem::CreateBody(EntityID eid, Shape* shape, typename Shape::Type type)
{
	assert(!m_entity_body_map.contains(eid));

	CollisionBody& body = m_bodies.emplace_back(eid, type);

	auto components = m_entity_admin->TryGetComponents<
		Collider, PhysicsBody, BodyTransform, ColliderAABB, 
		ColliderEnter, ColliderExit, ColliderOverlap>(eid);

	body.shape		= shape;
	body.collider	= std::get<Collider*>(components);
	body.body		= std::get<PhysicsBody*>(components);
	body.transform	= std::get<BodyTransform*>(components);
	body.aabb		= std::get<ColliderAABB*>(components);

	body.enter		= std::get<ColliderEnter*>(components);
	body.exit		= std::get<ColliderExit*>(components);
	body.overlap	= std::get<ColliderOverlap*>(components);

	return m_entity_body_map.try_emplace(eid, m_bodies.size() - 1).first->second;
}

int BroadSystem::FindBody(EntityID eid)
{
	if (auto it = m_entity_body_map.find(eid); it != m_entity_body_map.end())
		return it->second;

	return NULL_BODY;
}

void BroadSystem::RemoveBody(EntityID eid)
{
	const auto it1 = m_entity_body_map.find(eid);
	assert(it1 != m_entity_body_map.end() && "Entity should be in the map");

	const auto it2 = m_entity_body_map.find(m_bodies.back().entity_id);
	assert(it2 != m_entity_body_map.end() && "Entity should be in the map");

	if (QTBody* qtb = m_entity_admin->TryGetComponent<QTBody>(it2->second); qtb != nullptr)
		qtb->Update(it1->second); // update the index in the quad tree

	it2->second = it1->second;

	cu::SwapPopAt(m_bodies, it1->second);
	m_entity_body_map.erase(it1);
}

bool BroadSystem::HasDataForCollision(const CollisionBody& object)
{
	return object.shape != nullptr && object.collider != nullptr && object.transform != nullptr && object.aabb != nullptr; // safety checks
}

void BroadSystem::RegisterEvents()
{
	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<Collider>(
		[this](EntityID eid, Collider& c)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].collider = &c;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<PhysicsBody>(
		[this](EntityID eid, PhysicsBody& pb)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				 m_bodies[i].body = &pb;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<BodyTransform>(
		[this](EntityID eid, BodyTransform& t)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].transform = &t;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<ColliderAABB>(
		[this](EntityID eid, ColliderAABB& ab)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].aabb = &ab;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<ColliderEnter>(
		[this](EntityID eid, ColliderEnter& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].enter = &e;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<ColliderExit>(
		[this](EntityID eid, ColliderExit& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].exit = &e;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<ColliderOverlap>(
		[this](EntityID eid, ColliderOverlap& o)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].overlap = &o;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<Collider>(
		[this](EntityID eid, Collider& c)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].collider = &c;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<PhysicsBody>(
		[this](EntityID eid, PhysicsBody& pb)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].body = &pb;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<BodyTransform>(
		[this](EntityID eid, BodyTransform& t)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].transform = &t;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<ColliderAABB>(
		[this](EntityID eid, ColliderAABB& ab)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].aabb = &ab;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<ColliderEnter>(
		[this](EntityID eid, ColliderEnter& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].enter = &e;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<ColliderExit>(
		[this](EntityID eid, ColliderExit& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].exit = &e;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<ColliderOverlap>(
		[this](EntityID eid, ColliderOverlap& o)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].overlap = &o;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<Collider>(
		[this](EntityID eid, Collider& c)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].collider = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<PhysicsBody>(
		[this](EntityID eid, PhysicsBody& pb)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].body = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<BodyTransform>(
		[this](EntityID eid, BodyTransform& t)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].transform = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<ColliderAABB>(
		[this](EntityID eid, ColliderAABB& ab)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].aabb = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<ColliderEnter>(
		[this](EntityID eid, ColliderEnter& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].enter = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<ColliderExit>(
		[this](EntityID eid, ColliderExit& e)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].exit = nullptr;
		}));

	m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<ColliderOverlap>(
		[this](EntityID eid, ColliderOverlap& o)
		{
			if (auto i = FindBody(eid); i != NULL_BODY)
				m_bodies[i].overlap = nullptr;
		}));

	const auto RegisterShapeEvents = [this]<std::derived_from<Shape> S>()
	{
		m_event_ids.emplace_back(m_entity_admin->RegisterOnAddListener<S>(
			[this](EntityID eid, S& s)
			{
				CreateBody(eid, &s, S::GetType());
			}));

		m_event_ids.emplace_back(m_entity_admin->RegisterOnMoveListener<S>(
			[this](EntityID eid, S& s)
			{
				if (auto i = FindBody(eid); i != BroadSystem::NULL_BODY)
					m_bodies[i].shape = &s;
			}));

		m_event_ids.emplace_back(m_entity_admin->RegisterOnRemoveListener<S>(
			[this](EntityID eid, S& s)
			{
				RemoveBody(eid);
			}));
	};

	RegisterShapeEvents.operator()<Box>();
	RegisterShapeEvents.operator()<Circle>();
	RegisterShapeEvents.operator()<Point>();
	RegisterShapeEvents.operator()<Polygon>();
}