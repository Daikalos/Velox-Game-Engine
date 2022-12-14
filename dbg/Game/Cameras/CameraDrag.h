#pragma once

#include <Velox/Window/CameraBehavior.h>
#include <Velox/Window/Camera.h>

namespace vlx
{
	class CameraDrag final : public CameraBehavior
	{
	public:
		using CameraBehavior::CameraBehavior;

	private:
		bool HandleEvent(const sf::Event& event) override 
		{ 
			return true; 
		}

		bool Update(const Time& time) override
		{
			if (!GetContext().controls->Exists<MouseCursor>()) 
				return true;

			const Window* window = GetContext().window;

			const auto& mouse_input = GetContext().controls->Get<MouseInput>();
			const auto& mouse_cursor = GetContext().controls->Get<MouseCursor>();

			sf::Vector2f position = GetCamera().GetPosition();

			bool pressed	= mouse_input.Pressed(bn::Button::Drag);
			bool held		= mouse_input.Held(bn::Button::Drag);

			if (pressed || held)
			{
				sf::Vector2f cursor_pos = mouse_cursor.GetPosition() 
					/ GetCamera().GetScale() / window->GetRatioCmp();

				if (pressed)
					m_drag_pos = position + cursor_pos;

				if (held)
				{
					if (m_prev_scale != GetCamera().GetScale())
						m_drag_pos = position + cursor_pos;

					position = (m_drag_pos - cursor_pos);
				}
			}

			GetCamera().SetPosition(position);
			m_prev_scale = GetCamera().GetScale();

			return true;
		}

	private:
		sf::Vector2f	m_drag_pos;
		sf::Vector2f	m_prev_scale;
	};
}