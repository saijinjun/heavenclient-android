//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"
#include "../Components/TapGesture.h"

#include "../../Character/QuestLog.h"
#include "../../Graphics/Text.h"

#include <array>
#include <vector>

namespace ms
{
	// The quest log as a full-screen panel for touch: tabs for quests in
	// progress, ones the character can start, and finished ones; big rows;
	// and the selected quest's story, what is still needed, who to talk to,
	// the rewards, and Give up.
	class UIMobileQuests : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEQUESTS;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileQuests(const QuestLog& questlog);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		enum Tab : size_t
		{
			IN_PROGRESS,
			AVAILABLE,
			DONE,
			NUM_TABS
		};

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			Rectangle<int16_t> classic;
			std::array<Rectangle<int16_t>, NUM_TABS> tabs;
			Rectangle<int16_t> list;
			Rectangle<int16_t> detail;
			Rectangle<int16_t> giveup;
			int16_t rows;
		};

		struct Entry
		{
			int16_t id;
			int32_t level;
			Text name;
			Text note;
		};

		struct Line
		{
			Text text;
			bool done;
		};

		Layout layout() const;
		void load();
		void load_available(std::vector<Entry>& out) const;
		void select(int16_t id);
		size_t signature() const;
		Rectangle<int16_t> row_rect(const Layout& l, int16_t index) const;
		void clamp_scroll();
		void open_classic();
		void draw_label(const Text& text, Rectangle<int16_t> box) const;

		const QuestLog& questlog;

		size_t tab = IN_PROGRESS;
		int16_t selected = 0;
		int16_t first_row = 0;
		TapGesture gesture;
		size_t loaded_signature = 0;
		int16_t loaded_level = 0;
		int16_t refresh_ticks = 0;
		mutable std::array<std::vector<Entry>, NUM_TABS> entries;

		Text title;
		mutable std::array<Text, NUM_TABS> tab_labels;
		Text close_label;
		Text classic_label;
		Text giveup_label;
		Text empty_hint;
		Text empty_list;
		Text needed_title;
		Text rewards_title;

		// The selected quest
		Text name;
		Text npc;
		Text desc;
		std::vector<Line> needed;
		std::vector<Text> rewards;
	};
}
