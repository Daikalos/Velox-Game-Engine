#pragma once

#include <vector>
#include <array>
#include <span>

#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>

#include <Velox/System/Vector2.hpp>
#include <Velox/System/Rectangle.hpp>
#include <Velox/VeloxTypes.hpp>
#include <Velox/Config.hpp>

#include "IBatchable.h"

namespace vlx
{
	class VELOX_API Mesh : public IBatchable
	{
	private:
		using VertexList = std::vector<sf::Vertex>;
		using VertexSpan = std::span<const sf::Vertex>;

		static constexpr uint8 TRIANGLE_COUNT = 3;

		using Triangle = std::array<sf::Vertex, TRIANGLE_COUNT>;

	public:
		Mesh();
		Mesh(const sf::Texture& texture, float depth = 0.0f);
		Mesh(VertexSpan vertices, const sf::Texture& texture, float depth = 0.0f);

	public:
		sf::Vertex& operator[](std::size_t i);
		const sf::Vertex& operator[](std::size_t i) const;

	public:
		NODISC const sf::Texture*			GetTexture() const noexcept;
		NODISC const sf::Shader*			GetShader() const noexcept;
		NODISC auto							GetVertices() const noexcept -> const VertexList&;
		NODISC float						GetDepth() const noexcept;
		NODISC float						GetOpacity() const noexcept;
		NODISC constexpr sf::PrimitiveType	GetPrimitive() const noexcept;

		void SetTexture(const sf::Texture& texture);
		void SetColor(const sf::Color& color);
		void SetDepth(float value);
		void SetOpacity(float opacity);

	public:
		std::size_t GetSize() const noexcept;

		void Reserve(std::size_t capacity);
		void Resize(std::size_t size);

		void Assign(VertexSpan vertices);
		void Assign(std::span<const Vector2f> polygon);

		void Push(Triangle&& triangle);
		void Push(const Triangle& triangle);

		void Push(sf::Vertex&& v0, sf::Vertex&& v1, sf::Vertex&& v2);
		void Push(const sf::Vertex& v0, const sf::Vertex& v1, const sf::Vertex& v2);

		void Remove(std::size_t i);

	public:
		void Batch(SpriteBatch& sprite_batch, const Mat4f& transform, float depth) const override;

	private:
		const sf::Texture*	m_texture	{nullptr};
		const sf::Shader*	m_shader	{nullptr};
		VertexList			m_vertices;
		float				m_depth		{0.0f};
	};

	constexpr sf::PrimitiveType Mesh::GetPrimitive() const noexcept
	{
		return sf::Triangles;
	}
}