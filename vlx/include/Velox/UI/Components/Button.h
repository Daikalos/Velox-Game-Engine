#pragma once

#include <functional>

#include <Velox/Types.hpp>
#include <Velox/Config.hpp>

namespace vlx::ui
{
	///	The button contains data to define certain behaviours, e.g., press, release, and hover.
	/// 
	class VELOX_API Button
	{
	public:
		enum Event : uint8
		{
			E_None		= 0,
			E_Pressed	= 1 << 0,
			E_Clicked	= 1 << 1,
			E_Released	= 1 << 2,
			E_Entered	= 1 << 3,
			E_Exited	= 1 << 4
		};

	public:
		void Click();
		void Press();
		void Release();
		void Enter();
		void Exit();

	private:
		uint8	m_flags		{E_None};

		bool	m_pressed	{false};
		bool	m_entered	{false};

		friend class ButtonSystem;
	};

	// TODO: check out warning C4251

	struct VELOX_API ButtonClick
	{
		std::function<void()> OnClick;
	};

	struct VELOX_API ButtonPress
	{
		std::function<void()> OnPress;
	};

	struct VELOX_API ButtonRelease
	{
		std::function<void()> OnRelease;
	};

	struct VELOX_API ButtonEnter
	{
		std::function<void()> OnEnter;
	};

	struct VELOX_API ButtonExit
	{
		std::function<void()> OnExit;
	};
}