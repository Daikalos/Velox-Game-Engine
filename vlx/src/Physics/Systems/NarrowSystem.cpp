#include <Velox/Physics/Systems/NarrowSystem.h>

using namespace vlx;

NarrowSystem::NarrowSystem(EntityAdmin& entity_admin, const LayerType id) 
	: m_entity_admin(&entity_admin)
{

}

void NarrowSystem::Update(std::span<typename BroadSystem::CollisionPair> pairs, std::span<uint32> indices)
{
	m_arbiters.clear();

	for (const auto i : indices)
		CheckCollision(pairs[i]);

	const auto cmp = [](const EntityPair& lhs, const EntityPair& rhs)
	{
		return (lhs.first < rhs.first) || (lhs.first == rhs.first && lhs.second < rhs.second);
	};

	std::ranges::sort(
		m_curr_collisions.begin(), m_curr_collisions.end(), cmp); // sort to quickly allow for finding the differences between previous and current

	std::ranges::set_difference(
		m_prev_collisions.begin(), m_prev_collisions.end(),
		m_curr_collisions.begin(), m_curr_collisions.end(), std::inserter(m_difference, m_difference.begin()), cmp);

	for (const EntityPair& pair : m_difference)
	{
		const auto it = m_collisions.find(pair);
		if (it != m_collisions.end())
		{
			if (it->lhs_collider) it->lhs_collider->OnExit(it->rhs_eid);
			if (it->rhs_collider) it->rhs_collider->OnExit(it->lhs_eid);

			m_collisions.erase(it);
		}
	}

	m_prev_collisions = m_curr_collisions;
	m_curr_collisions.clear();

	m_difference.clear();
}

std::span<const CollisionArbiter> NarrowSystem::GetArbiters() const noexcept
{
	return m_arbiters;
}
std::span<CollisionArbiter> NarrowSystem::GetArbiters() noexcept
{
	return m_arbiters;
}

void NarrowSystem::CheckCollision(const typename BroadSystem::CollisionPair& pair)
{
	const CollisionObject& A = pair.first;
	const CollisionObject& B = pair.second;

	CollisionArbiter arbiter;
	CollisionTable::Collide(arbiter, *A.shape, A.type, *B.shape, B.type);

	if (arbiter.contacts_count) // successfully collided if not null
	{
		PhysicsBody* AB = A.body;
		PhysicsBody* BB = B.body;

		if (AB && BB && (AB->GetType() == BodyType::Dynamic || BB->GetType() == BodyType::Dynamic)) // only resolve if both entities has a physics body and either is dynamic
		{
			arbiter.A = AB;
			arbiter.B = BB;

			m_arbiters.emplace_back(arbiter);
		}

		Collider& AC = *A.collider; // dont worry if nullptr, should only ever collide if both have colliders anyways
		Collider& BC = *B.collider;

		bool has_enter		= AC.OnEnter	|| BC.OnEnter;
		bool has_exit		= AC.OnExit		|| BC.OnExit;
		bool has_overlap	= AC.OnOverlap	|| BC.OnOverlap;

		if (has_enter || has_exit || has_overlap)
		{
			EntityPair pair = (A.entity_id < B.entity_id) ?
				EntityPair(A.entity_id, B.entity_id) :
				EntityPair(B.entity_id, A.entity_id);

			CollisionResult a_result(B.entity_id); // store other entity
			CollisionResult b_result(A.entity_id);

			for (std::size_t i = 0; i < arbiter.contacts_count; ++i)
			{
				a_result.contacts[i].hit			= arbiter.contacts[i].position;
				a_result.contacts[i].normal			= arbiter.contacts[i].normal;
				a_result.contacts[i].penetration	= arbiter.contacts[i].penetration;

				b_result.contacts[i].hit			= arbiter.contacts[i].position;
				b_result.contacts[i].normal			= -arbiter.contacts[i].normal; // flip normal for other
				b_result.contacts[i].penetration	= arbiter.contacts[i].penetration;
			}

			if (has_enter || has_exit)
			{
				const auto ait = m_collisions.find(pair);
				if (ait == m_collisions.end()) // first time
				{
					AC.OnEnter(a_result);
					BC.OnEnter(b_result);

					m_collisions.emplace(
						m_entity_admin->GetComponentRef(A.entity_id, A.collider),
						m_entity_admin->GetComponentRef(B.entity_id, B.collider));
				}

				if (has_exit) // only bother recording if tracking OnExit
					m_curr_collisions.emplace_back(pair);
			}

			if (has_overlap)
				AC.OnOverlap(a_result); // only needs to call first
		}
	}
}

