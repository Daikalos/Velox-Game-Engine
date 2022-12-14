#include <Velox/Graphics/SpriteBatch.h>

using namespace vlx;

SpriteBatch::Triangle::Triangle(sf::Vertex&& v0, sf::Vertex&& v1, sf::Vertex&& v2, const sf::Texture* t, const sf::Shader* s, const float d)
	: vertices{ v0, v1, v2 }, texture(t), shader(s), depth(d) { }

void SpriteBatch::SetBatchMode(const BatchMode batch_mode)
{
	m_batch_mode = batch_mode;
	m_update_required = true;
}

void SpriteBatch::Reserve(const std::size_t size)
{
	m_triangles.reserve(size);
	m_proxy.reserve(size);
}

void SpriteBatch::Shrink()
{
	m_triangles.shrink_to_fit();
	m_proxy.shrink_to_fit();
	m_vertices.clear();
}

void SpriteBatch::AddTriangle(
	const Transform& transform, 
	const sf::Vertex& v0, 
	const sf::Vertex& v1, 
	const sf::Vertex& v2, 
	const sf::Texture* texture, 
	const sf::Shader* shader, 
	const float depth)
{
	assert(m_size <= m_triangles.size() && m_size <= m_proxy.size());

	if (m_size == m_triangles.size())
	{
		m_triangles.emplace_back(
			sf::Vertex(transform.GetTransform() * v0.position, v0.color, v0.texCoords),
			sf::Vertex(transform.GetTransform() * v1.position, v1.color, v1.texCoords),
			sf::Vertex(transform.GetTransform() * v2.position, v2.color, v2.texCoords), texture, shader, depth);

		m_proxy.push_back(m_size++);
	}
	else // reuse
	{
		m_triangles[m_size] = 
		{
			sf::Vertex(transform.GetTransform() * v0.position, v0.color, v0.texCoords),
			sf::Vertex(transform.GetTransform() * v1.position, v1.color, v1.texCoords),
			sf::Vertex(transform.GetTransform() * v2.position, v2.color, v2.texCoords), texture, shader, depth 
		};

		m_proxy[m_size++] = m_size;
	}

	m_update_required = true;
}

void SpriteBatch::Batch(const IBatchable& batchable, const Transform& transform, const float depth)
{
	batchable.Batch(*this, transform, depth);
}

void SpriteBatch::Batch(
	const Transform& transform, 
	const sf::Vertex* vertices, 
	const std::size_t count,
	const sf::PrimitiveType type, 
	const sf::Texture* texture, 
	const sf::Shader* shader, 
	float depth)
{
	switch (type)
	{
	case sf::Triangles:
		for (std::size_t i = 2; i < count; i += 3)
			AddTriangle(transform, vertices[i - 2], vertices[i - 1], vertices[i], texture, shader, depth);
		break;
	case sf::TriangleStrip:
		for (std::size_t i = 2; i < count; ++i)
			AddTriangle(transform, vertices[i - 2], vertices[i - 1], vertices[i], texture, shader, depth);
		break;
	case sf::TriangleFan:
		for (std::size_t i = 2; i < count; ++i)
			AddTriangle(transform, vertices[0], vertices[i - 1], vertices[i], texture, shader, depth);
		break;
	default:
		throw std::runtime_error("this primitive is not supported");
	}
}

void SpriteBatch::draw(sf::RenderTarget& target, const sf::RenderStates& states) const
{
	if (m_update_required)
	{
		SortTriangles();
		CreateBatches();

		m_update_required = false;
	}

	sf::RenderStates states_copy(states);
	for (SizeType i = 0, start = 0; i < m_batches.size(); ++i)
	{
		states_copy.texture = m_batches[i].texture;
		states_copy.shader = m_batches[i].shader;

		target.draw(&m_vertices[start], m_batches[i].count, sf::Triangles, states_copy);

		start += m_batches[i].count;
	}
}

bool SpriteBatch::CompareBackToFront(const Triangle& lhs, const Triangle& rhs) const
{
	if (lhs.depth != rhs.depth)
		return (lhs.depth > rhs.depth);

	return CompareTexture(lhs, rhs);
}
bool SpriteBatch::CompareFrontToBack(const Triangle& lhs, const Triangle& rhs) const
{
	if (lhs.depth != rhs.depth)
		return (lhs.depth < rhs.depth);

	return CompareTexture(lhs, rhs);
}
bool SpriteBatch::CompareTexture(const Triangle& lhs, const Triangle& rhs) const
{
	if (lhs.texture != rhs.texture)
		return (lhs.texture < rhs.texture);

	return (lhs.shader < rhs.shader);
}

void SpriteBatch::SortTriangles() const
{
	switch (m_batch_mode)
	{
	case BatchMode::BackToFront:
		std::stable_sort(m_proxy.begin(), m_proxy.begin() + m_size,
			[this](const auto i0, const auto i1)
			{ 
				return CompareBackToFront(m_triangles[i0], m_triangles[i1]);
			});
		break;
	case BatchMode::FrontToBack:
		std::stable_sort(m_proxy.begin(), m_proxy.begin() + m_size,
			[this](const auto i0, const auto i1)
			{
				return CompareFrontToBack(m_triangles[i0], m_triangles[i1]);
			});
		break;
	case BatchMode::Texture:
		std::stable_sort(m_proxy.begin(), m_proxy.begin() + m_size,
			[this](const auto i0, const auto i1)
			{
				return CompareTexture(m_triangles[i0], m_triangles[i1]);
			});
		break;
	case BatchMode::Deferred:
	default: return; // nothing to do
	}
}

void SpriteBatch::CreateBatches() const
{
	if (!m_size)
		return;

	m_vertices.resize(m_size * TRIANGLE_COUNT);

	auto last_texture = m_triangles[m_proxy.front()].texture;
	auto last_shader = m_triangles[m_proxy.front()].shader;

	SizeType start = 0, next = 0;
	for (; next < m_size; ++next)
	{
		const Triangle& triangle = m_triangles[m_proxy[next]];

		auto next_texture = triangle.texture;
		auto next_shader = triangle.shader;

		if (next_texture != last_texture || next_shader != last_shader)
		{
			m_batches.emplace_back(last_texture, next_shader, (next - start) * TRIANGLE_COUNT);
			last_texture = next_texture;
			start = next;
		}

		for (auto i = 0; i < TRIANGLE_COUNT; ++i)
			m_vertices[next * TRIANGLE_COUNT + i] = triangle.vertices[i];
	}

	if (start != m_size) // deal with leftover
	{
		const Triangle& triangle = m_triangles[m_proxy[start]];
		m_batches.emplace_back(triangle.texture, triangle.shader, (m_size - start) * TRIANGLE_COUNT);
	}
}

void SpriteBatch::Clear()
{
	m_size = 0;
	m_batches.clear();
	m_update_required = false;
}