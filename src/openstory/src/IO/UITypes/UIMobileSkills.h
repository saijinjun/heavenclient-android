//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"

#include "../../Character/CharStats.h"
#include "../../Character/Job.h"
#include "../../Character/SkillBook.h"
#include "../../Graphics/Text.h"

#include <array>
#include <map>
#include <vector>

namespace ms
{
	// The skill book as a full-screen panel for touch: a tab per job
	// advancement, big rows with a + to spend SP, and the selected skill's
	// details with buttons that put it on one of the touch quick slots.
	class UIMobileSkills : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILESKILLS;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileSkills(const CharStats& stats, const SkillBook& skillbook);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		static constexpr size_t NUM_TABS = 5;
		// The touch controls show this many quick slots
		static constexpr size_t NUM_SLOTS = 6;

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			Rectangle<int16_t> classic;
			std::array<Rectangle<int16_t>, NUM_TABS> tabs;
			Rectangle<int16_t> list;
			Rectangle<int16_t> detail;
			std::array<Rectangle<int16_t>, NUM_SLOTS> slots;
			int16_t rows;
		};

		struct Row
		{
			int32_t id;
			int32_t level;
			int32_t masterlevel;
		};

		Layout layout() const;
		size_t num_tabs() const;
		Job::Level tab_level() const;
		void rebuild();
		int16_t remaining_sp() const;
		bool can_raise(const Row& row) const;
		bool can_bind(const Row& row) const;
		const Row* selected_row() const;
		Rectangle<int16_t> row_rect(const Layout& l, int16_t index) const;
		Rectangle<int16_t> plus_rect(const Rectangle<int16_t>& row) const;
		void clamp_scroll();
		// The touch quick slot that holds this skill, or -1
		int16_t bound_slot(int32_t skill_id) const;
		void bind(size_t slot, int32_t skill_id) const;
		void open_classic();

		void draw_icon(int32_t skill_id, bool learned, Rectangle<int16_t> box) const;
		void draw_label(const Text& text, Rectangle<int16_t> box) const;
		Text& row_name(int32_t skill_id) const;

		const CharStats& stats;
		const SkillBook& skillbook;
		Job job;

		size_t tab = 0;
		uint16_t shown_job = 0;
		int32_t selected = 0;
		int16_t first_row = 0;
		bool was_down = false;
		std::vector<Row> rows;

		Text title;
		mutable std::array<Text, NUM_TABS> tab_labels;
		Text sp_text;
		Text plus_label;
		Text close_label;
		Text classic_label;
		Text empty_hint;
		Text slot_hint;
		std::array<Text, NUM_SLOTS> slot_labels;
		mutable Text name;
		mutable Text level_text;
		mutable Text desc;
		mutable Text level_desc;
		mutable int32_t shown_skill = -1;
		mutable int32_t shown_level = -1;
		mutable std::map<int32_t, Text> names;
		mutable std::map<int32_t, std::pair<int32_t, Text>> levels;
	};
}
