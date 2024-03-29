#pragma once

#include "Binds.hpp"
#include "JoystickInput.h"

namespace vlx
{
	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	class JoystickBindable : public Binds<Bind, uint32>
	{
	public:
		using ButtonType = Bind;

	public:
		explicit JoystickBindable(const JoystickInput& joystick);

	public:
		NODISC bool Held(		uint32 id, const ButtonType& name) const;
		NODISC bool Pressed(	uint32 id, const ButtonType& name) const;
		NODISC bool Released(	uint32 id, const ButtonType& name) const;
		NODISC float Axis(		uint32 id, const ButtonType& name) const;

	private:
		const JoystickInput* m_joystick {nullptr};
	};

	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	inline JoystickBindable<Bind>::JoystickBindable(const JoystickInput& joystick)
		: m_joystick(&joystick) { }

	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	inline bool JoystickBindable<Bind>::Held(uint32 id, const ButtonType& name) const
	{
		return this->GetEnabled() && this->m_joystick->Held(id, At(name));
	}

	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	inline bool JoystickBindable<Bind>::Pressed(uint32 id, const ButtonType& name) const
	{
		return this->GetEnabled() && this->m_joystick->Pressed(id, At(name));
	}

	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	inline bool JoystickBindable<Bind>::Released(uint32 id, const ButtonType& name) const
	{
		return this->GetEnabled() && this->m_joystick->Released(id, At(name));
	}

	template<typename Bind> requires (!std::same_as<Bind, uint32>)
	inline float JoystickBindable<Bind>::Axis(uint32 id, const ButtonType& name) const
	{
		return this->GetEnabled() && this->m_joystick->Axis(id, At(name));
	}
}