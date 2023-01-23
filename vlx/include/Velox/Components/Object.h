#pragma once

#include <Velox/ECS/IComponent.h>

namespace vlx
{
	/// <summary>
	///		Used for common properties in objects, usually all entities has this component but does not need to
	/// </summary>
	struct Object : public IComponent
	{
		bool IsAlive	{true};
		bool IsStatic	{false};
		bool IsGUI		{false};
		bool IsActive	{true};
		bool IsVisible	{true};
	};
}