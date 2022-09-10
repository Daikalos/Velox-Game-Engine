#include <Velox/Graphics/SpriteBatch.h>

using namespace vlx;

void SpriteBatch::SetSortMode(const SortMode sort_mode)
{
	m_sort_mode = sort_mode;
	m_update_required = true;
}

void SpriteBatch::AddTriangle(const sf::Vertex& v0, const sf::Vertex& v1, const sf::Vertex& v2, const sf::Transform& transform, const sf::Texture& texture, float depth)
{
	m_glyphs.emplace_back(texture, depth);

	m_vertices.append({ });

	m_update_required = true;
}

void SpriteBatch::Batch(const Sprite& sprite, float depth)
{
	//m_glyphs.emplace_back(sprite.GetTexture(), depth);

	//m_vertices.append({});
	//m_vertices.append({});
	//m_vertices.append({});
	//m_vertices.append({});
}

void SpriteBatch::draw(sf::RenderTarget& target, const sf::RenderStates& states) const
{
	if (m_update_required)
	{
		SortGlyphs();
		CreateBatches();

		m_update_required = false;
	}

	sf::RenderStates states_copy(states);
	for (std::size_t i = 0, start = 0; i < m_batches.size(); ++i)
	{
		states_copy.texture = m_batches[i].texture;
		states_copy.shader = m_batches[i].shader;

		target.draw(&m_sorted_vertices[start], m_batches[i].count, sf::Triangles, states_copy);

		start += m_batches[i].count;
	}
}

void SpriteBatch::SortGlyphs() const
{
	switch (m_sort_mode)
	{
	case SortMode::Deferred:



		break;
	case SortMode::BackToFront:



		break;
	case SortMode::FrontToBack:



		break;
	case SortMode::Texture:



		break;
	}
}

void SpriteBatch::CreateBatches() const
{
	//std::vector<sf::Vertex> vertices;
	//vertices.resize(m_glyps.size() * 6);

	//if (!m_glyps.empty())
	//	return;

	//int offset = 0;
	//int cv = 0;

	//m_batches.emplace_back(m_glyps.front().m_texture, offset, 6);

	//std::vector<sf::Vertex>& quad_vertices = m_glyps.front().m_vertices;
	//vertices[cv++] = quad_vertices[0];
	//vertices[cv++] = quad_vertices[2];
	//vertices[cv++] = quad_vertices[3];
	//vertices[cv++] = quad_vertices[3];
	//vertices[cv++] = quad_vertices[1];
	//vertices[cv++] = quad_vertices[0];

	//for (int i = 1; i < m_glyps.size(); ++i)
	//{
	//	if (m_quads[i].m_texture != m_quads[i - 1].m_texture)
	//	{
	//		m_batches.emplace_back(m_quads[i].m_texture, offset, 6);
	//	}
	//	else
	//	{
	//		m_batches.back().m_count += 6;
	//	}
	//}
}

void SpriteBatch::Clear()
{

}