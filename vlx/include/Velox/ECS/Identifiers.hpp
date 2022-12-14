#pragma once

#include <cstdint>
#include <vector>

namespace vlx
{
	using IDType			= std::uint32_t;
	using LayerType			= std::uint16_t;
	using ColumnType		= std::uint16_t;

	using ByteArray			= std::byte[];
	using DataPtr			= std::byte*;

	using EntityID			= IDType;
	using ComponentTypeID	= IDType;
	using ArchetypeID		= IDType;
	using ComponentIDs		= std::vector<ComponentTypeID>;

	static constexpr EntityID NULL_ENTITY		= NULL;
	static constexpr ArchetypeID NULL_ARCHETYPE	= NULL;

	enum SystemLayers : LayerType
	{
		LYR_NONE = 0,
		LYR_OBJECTS = 997,
		LYR_TRANSFORM = 998,
		LYR_ANCHOR = 999,
		LYR_RENDERING = 1000,
		LYR_GUI_RENDERING = 1001
	};
}