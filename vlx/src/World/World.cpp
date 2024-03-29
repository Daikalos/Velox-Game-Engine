#include <Velox/World/World.h>

using namespace vlx;

World::World(std::string name) : 

	m_time(),
	m_window(std::move(name), sf::VideoMode().getDesktopMode()),

	m_inputs(m_window),
	m_keyboard(m_inputs.Keyboard()),
	m_mouse(m_inputs.Mouse()),

	m_textures(),
	m_fonts(),

	m_camera(CameraBehavior::Context(m_window, m_inputs)),

	m_entity_admin(),

	m_systems(), 
	m_system_table(),

	m_state_stack(*this)

{
	m_entity_admin.RegisterComponents(AllTypes{});

	m_window.Initialize();

	m_mouse.Set(ebn::Button::GUIButton, sf::Mouse::Left);

	AddSystem<ObjectSystem>(			m_entity_admin,	LYR_NONE);
	AddSystem<RelationSystem>(			m_entity_admin,	LYR_NONE);
	AddSystem<LocalTransformSystem>(	m_entity_admin, LYR_LOCAL_TRANSFORM);
	AddSystem<GlobalTransformSystem>(	m_entity_admin,	LYR_GLOBAL_TRANSFORM);
	AddSystem<CullingSystem>(			m_entity_admin, LYR_CULLING, m_camera);
	AddSystem<AnchorSystem>(			m_entity_admin,	LYR_ANCHOR, m_window);
	AddSystem<ui::ButtonSystem>(		m_entity_admin,	LYR_GUI, m_camera, m_mouse, m_inputs.Cursor());
	AddSystem<RenderSystem>(			m_entity_admin, LYR_RENDERING, m_time);
	AddSystem<PhysicsDirtySystem>(		m_entity_admin, LYR_DIRTY_PHYSICS);
	AddSystem<PhysicsSystem>(			m_entity_admin,	LYR_PHYSICS, m_time);
	AddSystem<AnimationSystem>(			m_entity_admin, LYR_ANIMATION, m_time);
}

const InputHolder& World::GetInputs() const noexcept			{ return m_inputs; }
InputHolder& World::GetInputs() noexcept						{ return m_inputs; }

const Window& World::GetWindow() const noexcept					{ return m_window; }
Window& World::GetWindow() noexcept								{ return m_window; }

const Camera& World::GetCamera() const noexcept					{ return m_camera; }
Camera& World::GetCamera() noexcept								{ return m_camera; }

const TextureHolder& World::GetTextureHolder() const noexcept	{ return m_textures; }
TextureHolder& World::GetTextureHolder() noexcept				{ return m_textures; }

const FontHolder& World::GetFontHolder() const noexcept			{ return m_fonts; }
FontHolder& World::GetFontHolder() noexcept						{ return m_fonts; }

const Time& World::GetTime() const noexcept						{ return m_time; }
Time& World::GetTime() noexcept									{ return m_time; }

const StateStack& World::GetStateStack() const noexcept			{ return m_state_stack; }
StateStack& World::GetStateStack() noexcept						{ return m_state_stack; }

EntityAdmin& World::GetEntityAdmin() noexcept					{ return m_entity_admin; }
const EntityAdmin& World::GetEntityAdmin() const noexcept		{ return m_entity_admin; }

void World::Run()
{
	m_camera.SetSize(Vector2f(m_window.getSize()));
	m_camera.SetPosition(m_camera.GetSize() / 2.0f);

	float accumulator = 0.0f;

	Start();

	while (m_window.isOpen())
	{
		m_time.Update();

		m_inputs.Update(m_time, m_window.hasFocus());

		ProcessEvents();

		if (m_shutdown)
			break;

		PreUpdate();

		Update();

		accumulator += m_time.GetRealDT();
		accumulator = std::min(accumulator, 0.2f); // clamp accumulator to prevent over-shooting

		while (accumulator >= m_time.GetFixedDT())
		{
			accumulator -= m_time.GetFixedDT();
			FixedUpdate();
		}

		m_time.SetAlpha(accumulator / m_time.GetFixedDT());

		PostUpdate();

		Draw();
	}
}

void World::Start()
{
	m_state_stack.Start(m_time);
	m_camera.Start(m_time);

	for (const auto& pair : m_systems)
		pair.second->Start();
}

void World::PreUpdate()
{
	m_state_stack.PreUpdate(m_time);
	m_camera.PreUpdate(m_time);

	for (const auto& pair : m_systems)
		pair.second->PreUpdate();
}

void World::Update()
{
	m_state_stack.Update(m_time);
	m_camera.Update(m_time);

	for (const auto& pair : m_systems)
		pair.second->Update();
}

void World::FixedUpdate()
{
	m_state_stack.FixedUpdate(m_time);
	m_camera.FixedUpdate(m_time);

	for (const auto& pair : m_systems)
		pair.second->FixedUpdate();
}

void World::PostUpdate()
{
	m_state_stack.PostUpdate(m_time);
	m_camera.PostUpdate(m_time);

	for (const auto& pair : m_systems)
		pair.second->PostUpdate();

	if (m_state_stack.IsEmpty())
		m_window.close();
}

void World::ProcessEvents()
{
	sf::Event event;
	while (m_window.pollEvent(event))
	{
		m_inputs.HandleEvent(event);
		m_window.HandleEvent(event);
		m_camera.HandleEvent(event);
		m_state_stack.HandleEvent(event);

		if (event.type == sf::Event::Closed)
		{
			// TODO: perform necessary cleanup
			m_entity_admin.Shutdown();
			m_shutdown = true;

			return;
		}
	}
}

void World::Draw()
{
	m_window.clear(sf::Color(53, 81, 92));
	m_window.setView(m_camera);

	for (const auto& pair : m_systems)
		pair.second->Draw(m_window);

	m_state_stack.Draw();

	m_window.setView(m_window.getDefaultView()); // draw hud ontop of everything else

	for (const auto& pair : m_systems)
		pair.second->DrawGUI(m_window);

	m_window.display();
}
