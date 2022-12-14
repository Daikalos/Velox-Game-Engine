#pragma once

#include <Velox/Scene/State.hpp>
#include <Velox/Utilities.hpp>

class StateLoading final : public vlx::State<>
{
public:
	bool HandleEvent(const sf::Event& event) override;
	bool Update(vlx::Time& time) override;
	void Draw() override;



};