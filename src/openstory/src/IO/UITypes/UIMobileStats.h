//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"
#include "../Components/TapGesture.h"

#include "../../Character/CharStats.h"
#include "../../Graphics/Text.h"

#include <array>

namespace ms
{
	// Character stats as a full-screen panel for touch: big rows, and a big
	// + beside each main stat while there is AP to spend, plus Auto assign.
	class UIMobileStats : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESTATS;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileStats(const CharStats& stats);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		static constexpr size_t NUM_INFO = 6;
		static constexpr size_t NUM_MAIN = 4;

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			std::array<Rectangle<int16_t>, NUM_INFO> info;
			std::array<Rectangle<int16_t>, NUM_MAIN> main;
			std::array<Rectangle<int16_t>, NUM_MAIN> plus;
			Rectangle<int16_t> ap;
			Rectangle<int16_t> autoassign;
		};

		Layout layout() const;
		void spend(MapleStat::Id stat) const;
		void auto_assign() const;

		const CharStats& stats;
		TapGesture gesture;

		Text title;
		Text header;
		std::array<Text, NUM_INFO> info_names;
		std::array<Text, NUM_INFO> info_values;
		std::array<Text, NUM_MAIN> main_names;
		std::array<Text, NUM_MAIN> main_values;
		Text ap_text;
		Text plus_label;
		Text auto_label;
		Text close_label;
	};
}
