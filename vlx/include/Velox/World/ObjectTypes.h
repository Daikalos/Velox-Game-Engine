#pragma once

#include <tuple>
#include <type_traits>

#include <Velox/Graphics/Components/Renderable.h>
#include <Velox/Graphics/Components/GlobalTransformDirty.h>
#include <Velox/Graphics/Components/GlobalTransformMatrix.h>
#include <Velox/Graphics/Components/GlobalTransformMatrixInverse.h>
#include <Velox/Graphics/Components/GlobalTransformTranslation.h>
#include <Velox/Graphics/Components/GlobalTransformRotation.h>
#include <Velox/Graphics/Components/GlobalTransformScale.h>
#include <Velox/Graphics/Components/Transform.h>
#include <Velox/Graphics/Components/TransformMatrix.h>
#include <Velox/Graphics/Components/TransformMatrixInverse.h>
#include <Velox/Graphics/Components/Relation.h>
#include <Velox/Graphics/Components/Sprite.h>
#include <Velox/Graphics/Components/Mesh.h>

#include <Velox/UI/Components/Container.h>
#include <Velox/UI/Components/Button.h>
#include <Velox/UI/Components/Text.h>
#include <Velox/UI/Components/TextMesh.h>
#include <Velox/UI/Components/TextBox.h>
#include <Velox/UI/Components/UIBase.h>

#include <Velox/Algorithms/QTElement.hpp>

#include <Velox/Physics/Shapes/Circle.h>
#include <Velox/Physics/Shapes/Box.h>
#include <Velox/Physics/Shapes/Polygon.h>
#include <Velox/Physics/Shapes/Point.h>

#include <Velox/Physics/PhysicsBody.h>
#include <Velox/Physics/BodyTransform.h>
#include <Velox/Physics/BodyLastTransform.h>

#include <Velox/Physics/Collider/Collider.h>
#include <Velox/Physics/Collider/ColliderEvents.h>
#include <Velox/Physics/Collider/ColliderAABB.h>

#include <Velox/World/Object.h>

// TODO: maybe expand this to return constructed object with default parameters set, e.g., 
// return a button object with sprite, events, and default colors set

namespace vlx
{
	using AllTypes = std::type_identity<std::tuple<
		Object, Renderable, Relation, Sprite, Mesh,
		Transform, TransformMatrix, TransformMatrixInverse, 
		GlobalTransformTranslation, GlobalTransformRotation, GlobalTransformScale,
		GlobalTransformDirty, GlobalTransformMatrix, GlobalTransformMatrixInverse,
		Circle, Box, Polygon, Point, QTBody, Collider, ColliderAABB, 
		PhysicsBody, BodyTransform, BodyLastTransform,
		ColliderEnter, ColliderExit, ColliderOverlap,
		ui::Button, ui::Text, ui::TextMesh, ui::UIBase,
		ui::ButtonClick, ui::ButtonPress, ui::ButtonRelease, ui::ButtonEnter, ui::ButtonExit>>;

	using ObjectType = std::type_identity<std::tuple<
		Object, Renderable, Sprite, Relation,
		Transform, TransformMatrix, TransformMatrixInverse, 
		GlobalTransformTranslation, GlobalTransformDirty, GlobalTransformMatrix, GlobalTransformMatrixInverse>>;

	using PhysicsType = std::type_identity<std::tuple<Collider, ColliderAABB, QTBody, PhysicsBody, BodyTransform, BodyLastTransform>>;

	namespace ui
	{
		using ContainerType		= std::type_identity<std::tuple<
			Transform, TransformMatrix, GlobalTransformTranslation, GlobalTransformDirty, GlobalTransformMatrix, 
			Object, Renderable, Relation, ui::UIBase>>;

		using ImageType			= std::type_identity<std::tuple<
			Transform, TransformMatrix, GlobalTransformTranslation, GlobalTransformDirty, GlobalTransformMatrix, 
			Object, Renderable, Relation, Sprite>>;

		using ButtonType		= std::type_identity<std::tuple<
			Transform, TransformMatrix, GlobalTransformTranslation, GlobalTransformDirty, GlobalTransformMatrix, 
			Object, Renderable, Relation, Sprite, ui::UIBase, ui::Button>>;

		using TextType			= std::type_identity<std::tuple<
			Transform, TransformMatrix, GlobalTransformTranslation, GlobalTransformDirty, GlobalTransformMatrix, 
			Object, Renderable, Relation, ui::UIBase, ui::Text, ui::TextMesh>>;
	}
}