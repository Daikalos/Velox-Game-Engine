#pragma once

#include <SFML/Graphics.hpp>

#include <Velox/Components/Object.h>
#include <Velox/Components/Sprite.h>
#include <Velox/Components/Transform.h>

#include <Velox/Graphics/SpriteBatch.h>

#include <Velox/ECS.hpp>
#include <Velox/Config.hpp>
#include <Velox/Window.hpp>

#include <Velox/World/SystemObject.h>

namespace vlx
{
	class VELOX_API RenderSystem : public SystemObject
	{
	private:
		using System = System<Object, Transform, Sprite>;

	public:
		RenderSystem(EntityAdmin& entity, const LayerType id);

	public:
		void SetBatchMode(const BatchMode batch_mode);
		void SetBatchingEnabled(const bool flag);

		void UpdateStaticBatch();

	public:
		void SetGUIBatchMode(const BatchMode batch_mode);
		void SetGUIBatchingEnabled(const bool flag);

		void UpdateStaticGUIBatch();

	public:
		void Update() override;

		void Draw(Window& window) const;
		void DrawGUI(Window& window) const;

	private:
		void PreUpdate();
		void PostUpdate();

	private:
		System			m_system;

		SpriteBatch		m_static_batch;
		SpriteBatch		m_dynamic_batch;

		SpriteBatch		m_static_gui_batch;
		SpriteBatch		m_dynamic_gui_batch;

		bool			m_batching_enabled			{true};
		bool			m_update_static_bash		{true};

		bool			m_gui_batching_enabled		{true};
		bool			m_update_static_gui_bash	{true};
	};
}