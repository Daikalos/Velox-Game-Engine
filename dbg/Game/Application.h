#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <string_view>

#include <Velox/Utilities.hpp>
#include <Velox/Input.hpp>

#include <Velox/ECS.hpp>
#include <Velox/Scene/StateStack.h>
#include <Velox/Graphics/ResourceHolder.hpp>
#include <Velox/Graphics/ResourceHolder.hpp>

#include <Velox/Window/Camera.h>
#include <Velox/Window/Window.h>

#include "Binds.h"
#include "scenes/StateTest.h"

#include "Cameras/CameraDrag.h"
#include "Cameras/CameraZoom.h"

static const std::string DATA_FOLDER = "../data/";
static const std::string AUDIO_FOLDER = DATA_FOLDER + "audio/";
static const std::string TEXTURE_FOLDER = DATA_FOLDER + "textures/";

namespace vlx
{
	class Application final
	{
	public:
		Application(std::string_view name);
		~Application();

		void Run();

	private:
		void ProcessEvents();

		void PreUpdate();
		void Update();
		void FixedUpdate();
		void PostUpdate();

		void Draw();
		
		void RegisterStates();
		void RegisterControls();
		void LoadMainTextures();

		void test();

	private:
		Window			m_window;
		Camera			m_camera;
		StateStack		m_state_stack;
		TextureHolder	m_texture_holder;
		FontHolder		m_font_holder;
		Time			m_time;
		ControlMap		m_controls;
		EntityAdmin		m_entity_admin;

		BtnFunc<KeyboardInput, sf::Keyboard::Key> m_button_funcs;
	};
}
