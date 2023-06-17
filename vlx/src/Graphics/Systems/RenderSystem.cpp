#include <Velox/Graphics/Systems/RenderSystem.h>

using namespace vlx;

RenderSystem::RenderSystem(EntityAdmin& entity_admin, LayerType id, Time& time)
	: SystemAction(entity_admin, id), 

	m_sprites(entity_admin, id), 
	m_meshes(entity_admin, id),

	m_sprites_bodies(entity_admin, id),
	m_meshes_bodies(entity_admin, id)
{
	m_sprites.Each(
		[this](EntityID eid, Renderable& r, Sprite& s, GlobalTransform& gt)
		{
			BatchEntity(r, s, gt.GetTransform(), s.GetDepth());
		});

	m_meshes.Each(
		[this](EntityID eid, Renderable& r, Mesh& m, GlobalTransform& gt)
		{
			BatchEntity(r, m, gt.GetTransform(), m.GetDepth());
		});

	m_sprites_bodies.Each(
		[this, &time](EntityID eid, Renderable& r, Sprite& s, PhysicsBody& pb, Transform& t)
		{
			BatchBody(time, r, s, pb, t, s.GetDepth());
		});

	m_meshes_bodies.Each(
		[this, &time](EntityID eid, Renderable& r, Mesh& m, PhysicsBody& pb, Transform& t)
		{
			BatchBody(time, r, m, pb, t, m.GetDepth());
		});

	m_sprites.Exclude<PhysicsBody>();
	m_meshes.Exclude<PhysicsBody>();
}

bool RenderSystem::IsRequired() const noexcept
{
	return true;
}

void RenderSystem::SetBatchMode(BatchMode batch_mode)
{
	m_static_batch.SetBatchMode(batch_mode);
	m_dynamic_batch.SetBatchMode(batch_mode);
}

void RenderSystem::SetBatchingEnabled(const bool flag)
{
	m_batching_enabled = flag;
}

void RenderSystem::UpdateStaticBatch()
{
	m_update_static_batch = true;
}

void RenderSystem::SetGUIBatchMode(BatchMode batch_mode)
{
	m_static_gui_batch.SetBatchMode(batch_mode);
	m_dynamic_gui_batch.SetBatchMode(batch_mode);
}

void RenderSystem::SetGUIBatchingEnabled(bool flag)
{
	m_gui_batching_enabled = flag;
}

void RenderSystem::UpdateStaticGUIBatch()
{
	m_update_static_gui_batch = true;
}

void RenderSystem::Start()
{

}

void RenderSystem::PreUpdate()
{
	if (m_update_static_batch)
		m_static_batch.Clear();
	if (m_update_static_gui_batch)
		m_static_gui_batch.Clear();

	m_dynamic_batch.Clear();
	m_dynamic_gui_batch.Clear();
}

void RenderSystem::Update()
{
	m_sprites.ForceRun();
	m_meshes.ForceRun();
}

void RenderSystem::FixedUpdate()
{

}

void RenderSystem::PostUpdate()
{
	m_sprites_bodies.ForceRun();
	m_meshes_bodies.ForceRun();

	m_update_static_batch = false;
	m_update_static_gui_batch = false;
}

void RenderSystem::BatchEntity(const Renderable& renderable, const IBatchable& batchable, const Mat4f& transform, float depth)
{
	if (!renderable.IsVisible)
		return;

	if (!renderable.IsGUI)
	{
		if (renderable.IsStatic)
		{
			if (m_update_static_batch)
				m_static_batch.Batch(batchable, transform, depth);
		}
		else
		{
			m_dynamic_batch.Batch(batchable, transform, depth);
		}
	}
	else
	{
		if (renderable.IsStatic)
		{
			if (m_update_static_gui_batch)
				m_static_gui_batch.Batch(batchable, transform, depth);
		}
		else
		{
			m_dynamic_gui_batch.Batch(batchable, transform, depth);
		}
	}
}

void RenderSystem::BatchBody(const Time& time, const Renderable& renderable, const IBatchable& batchable, const PhysicsBody& body, const Transform& t, float depth)
{
	if (body.GetType() != BodyType::Dynamic || !body.IsAwake() || !body.IsEnabled()) // draw normally if not moved by physics
	{
		BatchEntity(renderable, batchable, t.GetTransform(), depth);
	}
	else if (body.position == body.last_pos && body.rotation == body.last_rot) // draw normally if havent moved at all
	{
		BatchEntity(renderable, batchable, t.GetTransform(), depth);
	}
	else
	{
		Vector2f lerp_pos = Vector2f::Lerp(body.last_pos, body.position, time.GetAlpha());
		sf::Angle lerp_rot = au::Lerp(body.last_rot, body.rotation, time.GetAlpha());

		Mat4f transform;
		transform.Build(lerp_pos, t.GetOrigin(), t.GetScale(), lerp_rot);

		BatchEntity(renderable, batchable, transform, depth);
	}
}

void RenderSystem::Draw(Window& window) const
{
	window.draw(m_static_batch);
	window.draw(m_dynamic_batch);
}

void RenderSystem::DrawGUI(Window& window) const
{
	window.draw(m_static_gui_batch);
	window.draw(m_dynamic_gui_batch);
}