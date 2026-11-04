//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileUserList.h"

#include "UIBuddyGroup.h"
#include "UIBuddyNickname.h"
#include "UINotice.h"
#include "UIPartyInvite.h"
#include "UIPartySearchStart.h"
#include "UIPartySettings.h"
#include "UIWhisper.h"

#include "../NotificationCenter.h"
#include "../UI.h"

#include "../../Character/BuddyMemoStore.h"
#include "../../Character/Job.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Net/Packets/BuddyPackets.h"
#include "../../Net/Packets/GameplayPackets.h"

#include <algorithm>
#include <map>

namespace ms
{
	namespace
	{
		const char* TAB_NAMES[] = { "Buddies", "Online", "Party" };
		const char* DEFAULT_GROUP = "Default Group";

		constexpr int16_t MARGIN = 16;
		constexpr int16_t ROW_H = 48;
		constexpr int16_t GAP = 4;
		constexpr int16_t TAB_H = 40;
		constexpr int16_t BUTTON_H = 42;
		constexpr int16_t BUTTON_MAX_W = 200;
		constexpr int16_t BUTTON_GAP = 8;
		constexpr size_t MAX_PARTY = 6;

		constexpr int16_t ADD_W = 440;
		constexpr int16_t ADD_H = 188;
		constexpr int16_t ADD_TOP = 8;
		constexpr int16_t ADD_FIELD_Y = 40;
		constexpr int16_t ADD_FIELD_H = 40;
		constexpr int16_t ADD_GROUP_Y = 88;
		constexpr int16_t ADD_GROUP_H = 40;
		constexpr int16_t ADD_BUTTON_Y = 136;
		constexpr int16_t ADD_BUTTON_W = 132;
		constexpr size_t MAX_NAME_LEN = 13;

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		void set_text(Text& text, const std::string& value)
		{
			if (text.get_text() != value)
				text.change_text(value);
		}

		void draw_centered(const Text& text, const Rectangle<int16_t>& r)
		{
			text.draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(text, r.top() + r.height() / 2)));
		}

		const char* action_name(uint8_t action)
		{
			static const char* NAMES[] = {
				"Add Friend", "Groups", "Whisper", "Party Invite", "Nickname", "Move", "Remove",
				"Create Party", "Invite", "Settings", "Find Members", "Leave Party",
				"Add Friend", "Make Leader", "Kick", "Join", "Decline"
			};

			return NAMES[action];
		}
	}

	UIMobileUserList::UIMobileUserList(uint8_t t) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), tab(t < NUM_TABS ? t : BUDDIES)
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Community");
		count = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::HUD_EXP, "");
		empty = Text(Text::Font::A13M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "", 480);
		close_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "X");

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, TAB_NAMES[i]);

		for (size_t i = 0; i < MAX_ROWS; i++)
		{
			row_names[i] = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, "");
			row_details[i] = Text(Text::Font::A13M, Text::Alignment::RIGHT, Color::Name::SILVERCHALICE, "");
		}

		for (size_t i = 0; i < MAX_ACTIONS; i++)
			action_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "");

		refresh();
	}

	UIMobileUserList::Layout UIMobileUserList::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 640);
		int16_t ph = std::min<int16_t>(vh - 24, 400);
		int16_t px = (vw - pw) / 2;
		int16_t py = (vh - ph) / 2;

		l.panel = rect(px, py, pw, ph);
		l.close = rect(px + pw - 40, py + 4, 34, 34);

		int16_t inner_w = pw - MARGIN * 2;
		int16_t tab_w = inner_w / NUM_TABS;

		for (size_t i = 0; i < NUM_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * tab_w, py + 40, tab_w - 2, TAB_H);

		l.footer_y = py + ph - MARGIN - BUTTON_H;

		int16_t list_top = py + 40 + TAB_H + GAP * 2;
		int16_t list_h = l.footer_y - GAP * 2 - list_top;

		l.list = rect(px + MARGIN, list_top, inner_w, list_h);
		l.rows = std::clamp<int16_t>((list_h + GAP) / (ROW_H + GAP), 1, static_cast<int16_t>(MAX_ROWS));

		return l;
	}

	Rectangle<int16_t> UIMobileUserList::row_rect(const Layout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - first_row) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 10, ROW_H);
	}

	Rectangle<int16_t> UIMobileUserList::action_rect(const Layout& l, size_t index) const
	{
		int16_t n = static_cast<int16_t>(actions.size());
		int16_t w = std::min<int16_t>(BUTTON_MAX_W, (l.list.width() - (n - 1) * BUTTON_GAP) / std::max<int16_t>(n, 1));
		int16_t total = n * w + (n - 1) * BUTTON_GAP;
		int16_t x = l.list.left() + (l.list.width() - total) / 2 + static_cast<int16_t>(index) * (w + BUTTON_GAP);

		return rect(x, l.footer_y, w, BUTTON_H);
	}

	void UIMobileUserList::build_buddies()
	{
		const auto& entries = Stage::get().get_player().get_buddylist().get_entries();

		std::map<std::string, std::vector<const BuddyEntry*>> groups;
		size_t online = 0;

		for (const auto& kv : entries)
		{
			const BuddyEntry& e = kv.second;

			if (e.online())
				online++;

			if (tab == BUDDIES || e.online())
				groups[e.group.empty() ? std::string(DEFAULT_GROUP) : e.group].push_back(&e);
		}

		set_text(count, std::to_string(online) + " of " + std::to_string(entries.size()) + " online");

		for (auto& group : groups)
		{
			std::sort(group.second.begin(), group.second.end(), [](const BuddyEntry* a, const BuddyEntry* b)
			{
				if (a->online() != b->online())
					return a->online();

				return a->name < b->name;
			});
		}

		const auto add_group = [&](const std::string& name, const std::vector<const BuddyEntry*>& members)
		{
			bool shut = tab == BUDDIES && collapsed.count(name) > 0;

			// The online tab is a plain list; the groups are for keeping
			// the full list short
			if (tab == BUDDIES)
			{
				Row header;
				header.kind = RowKind::GROUP;
				header.group = name;
				header.label = (shut ? "+ " : "- ") + name;
				header.detail = std::to_string(members.size());
				header.selectable = false;
				rows.push_back(header);
			}

			if (shut)
				return;

			for (const BuddyEntry* e : members)
			{
				const std::string& nickname = BuddyMemoStore::get().get_memo(e->cid);

				Row row;
				row.kind = RowKind::BUDDY;
				row.id = e->cid;
				row.name = e->name;
				row.group = name;
				row.online = e->online();
				row.label = nickname.empty() ? e->name : nickname + " (" + e->name + ")";
				row.detail = e->online() ? "Channel " + std::to_string(e->channel + 1) : "Offline";
				rows.push_back(row);
			}
		};

		auto first = groups.find(DEFAULT_GROUP);

		if (first != groups.end())
			add_group(first->first, first->second);

		for (const auto& group : groups)
			if (group.first != DEFAULT_GROUP)
				add_group(group.first, group.second);
	}

	void UIMobileUserList::build_party()
	{
		Player& me = Stage::get().get_player();
		Party& party = me.get_party();
		int32_t my_cid = me.get_oid();

		if (party.is_in_party())
			set_text(count, std::to_string(party.get_members().size()) + " of " + std::to_string(MAX_PARTY) + " in party");
		else
			set_text(count, "");

		for (const PartyMember& m : party.get_members())
		{
			Row row;
			row.kind = RowKind::MEMBER;
			row.id = m.cid;
			row.name = m.name;
			row.online = m.online;
			row.selectable = m.cid != my_cid;
			row.label = m.name;

			if (m.cid == party.get_leader())
				row.label += " (Leader)";

			row.detail = m.online
				? "Lv. " + std::to_string(m.level) + " " + Job(m.job).get_name()
				: "Offline";

			rows.push_back(row);
		}

		// The server has no list of parties to browse. What it does send is
		// invites, so those are what can be joined from here.
		for (const auto& n : NotificationCenter::get().list())
		{
			if (n.title.find("Party") == std::string::npos)
				continue;

			Row row;
			row.kind = RowKind::INVITE;
			row.id = n.id;
			row.online = true;
			row.label = n.body;

			auto cut = row.label.find(" has invited");

			if (cut != std::string::npos)
				row.label = row.label.substr(0, cut);

			row.detail = "Party invite";
			rows.push_back(row);
		}
	}

	const UIMobileUserList::Row* UIMobileUserList::selected_row() const
	{
		if (selected_kind == RowKind::NONE)
			return nullptr;

		for (const Row& row : rows)
			if (row.kind == selected_kind && row.id == selected_id)
				return &row;

		return nullptr;
	}

	void UIMobileUserList::build_actions()
	{
		actions.clear();

		Player& me = Stage::get().get_player();
		Party& party = me.get_party();
		bool leader = party.is_in_party() && party.get_leader() == me.get_oid();

		if (const Row* row = selected_row())
		{
			switch (row->kind)
			{
			case RowKind::BUDDY:
				if (row->online)
					actions = { Action::WHISPER, Action::INVITE_BUDDY, Action::NICKNAME, Action::MOVE_GROUP, Action::REMOVE_BUDDY };
				else
					actions = { Action::NICKNAME, Action::MOVE_GROUP, Action::REMOVE_BUDDY };
				return;
			case RowKind::MEMBER:
				actions = { Action::WHISPER, Action::ADD_MEMBER };

				if (leader)
				{
					actions.push_back(Action::MAKE_LEADER);
					actions.push_back(Action::KICK);
				}

				return;
			case RowKind::INVITE:
				actions = { Action::JOIN_PARTY, Action::DECLINE_PARTY };
				return;
			default:
				break;
			}
		}

		if (tab != PARTY)
			actions = { Action::ADD_FRIEND, Action::GROUPS };
		else if (!party.is_in_party())
			actions = { Action::CREATE_PARTY };
		else if (leader)
			actions = { Action::INVITE_PLAYER, Action::PARTY_SETTINGS, Action::FIND_MEMBERS, Action::LEAVE_PARTY };
		else
			actions = { Action::INVITE_PLAYER, Action::PARTY_SETTINGS, Action::LEAVE_PARTY };
	}

	void UIMobileUserList::refresh()
	{
		rows.clear();

		if (tab == PARTY)
			build_party();
		else
			build_buddies();

		if (!selected_row())
			selected_kind = RowKind::NONE;

		build_actions();

		Layout l = layout();
		int16_t max_first = std::max<int16_t>(0, static_cast<int16_t>(rows.size()) - l.rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i].change_color(i == tab ? Color::Name::HUD_PANEL : Color::Name::WHITE);

		for (int16_t i = 0; i < l.rows; i++)
		{
			size_t index = static_cast<size_t>(first_row + i);

			if (index >= rows.size())
				break;

			const Row& row = rows[index];
			bool selected = row.kind == selected_kind && row.id == selected_id;
			Color::Name normal = row.kind == RowKind::GROUP ? Color::Name::HUD_EXP
				: row.online ? Color::Name::WHITE : Color::Name::SILVERCHALICE;

			set_text(row_names[i], row.label);
			set_text(row_details[i], row.detail);
			row_names[i].change_color(classic::row_text(selected, normal));
			row_details[i].change_color(classic::row_text(selected, Color::Name::SILVERCHALICE));
		}

		for (size_t i = 0; i < actions.size() && i < MAX_ACTIONS; i++)
			set_text(action_labels[i], action_name(static_cast<uint8_t>(actions[i])));

		if (tab == PARTY)
			set_text(empty, "You are not in a party.");
		else if (tab == ONLINE)
			set_text(empty, "None of your buddies are online.");
		else
			set_text(empty, "Your buddy list is empty.");
	}

	void UIMobileUserList::update()
	{
		refresh();
	}

	void UIMobileUserList::draw(float inter) const
	{
		Layout l = layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());
		title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 6));
		count.draw(Point<int16_t>(l.panel.left() + MARGIN, l.panel.top() + 10));

		if (!classic::close_button(l.close.get_left_top(), l.close.width()))
		{
			classic::button(l.close.get_left_top(), l.close.width(), l.close.height());
			close_label.draw(Point<int16_t>(l.close.left() + l.close.width() / 2, l.close.top() + 8));
		}

		for (size_t i = 0; i < NUM_TABS; i++)
		{
			classic::tab(l.tabs[i].get_left_top(), l.tabs[i].width(), l.tabs[i].height(), i == tab);
			draw_centered(tab_labels[i], l.tabs[i]);
		}

		if (rows.empty())
			draw_centered(empty, l.list);

		for (int16_t i = 0; i < l.rows; i++)
		{
			size_t index = static_cast<size_t>(first_row + i);

			if (index >= rows.size())
				break;

			const Row& row = rows[index];
			Rectangle<int16_t> r = row_rect(l, first_row + i);
			int16_t cy = r.top() + r.height() / 2;

			if (row.kind != RowKind::GROUP)
				classic::row(r.get_left_top(), r.width(), r.height(), row.kind == selected_kind && row.id == selected_id);

			row_names[i].draw(Point<int16_t>(r.left() + 10, classic::text_y(row_names[i], cy)));
			row_details[i].draw(Point<int16_t>(r.right() - 10, classic::text_y(row_details[i], cy)));
		}

		int16_t total = static_cast<int16_t>(rows.size());

		if (total > l.rows)
		{
			int16_t track_h = l.list.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / total);
			int16_t thumb_y = l.list.top() + (track_h - thumb_h) * first_row / std::max<int16_t>(1, total - l.rows);

			classic::scrollbar(l.list.right() - 6, l.list.top(), 6, track_h, thumb_y, thumb_h);
		}

		for (size_t i = 0; i < actions.size() && i < MAX_ACTIONS; i++)
		{
			Rectangle<int16_t> r = action_rect(l, i);
			classic::button(r.get_left_top(), r.width(), r.height());
			draw_centered(action_labels[i], r);
		}

		UIElement::draw(inter);
	}

	void UIMobileUserList::run(Action action)
	{
		const Row* picked = selected_row();
		std::string name = picked ? picked->name : std::string();
		std::string group = picked ? picked->group : std::string();
		int32_t id = picked ? picked->id : 0;

		switch (action)
		{
		case Action::ADD_FRIEND:
			UI::get().emplace<UIMobileAddBuddy>(std::string(), std::string(DEFAULT_GROUP));
			break;
		case Action::GROUPS:
			UI::get().emplace<UIBuddyGroup>();
			break;
		case Action::WHISPER:
			UI::get().emplace<UIWhisper>(name);
			break;
		case Action::INVITE_BUDDY:
			InviteToPartyPacket(name).dispatch();
			break;
		case Action::NICKNAME:
			UI::get().emplace<UIBuddyNickname>(id, name);
			break;
		case Action::MOVE_GROUP:
			UI::get().emplace<UIMobileAddBuddy>(name, group);
			break;
		case Action::REMOVE_BUDDY:
			UI::get().emplace<UIYesNo>("Remove " + name + " from your buddy list?", [id](bool yes)
			{
				if (yes)
					DeleteBuddyPacket(id).dispatch();
			});
			break;
		case Action::CREATE_PARTY:
			CreatePartyPacket().dispatch();
			break;
		case Action::INVITE_PLAYER:
			UI::get().emplace<UIPartyInvite>();
			break;
		case Action::PARTY_SETTINGS:
			// This window doesn't take the touches for itself, so it can't
			// open on top of a panel that does
			deactivate();
			UI::get().emplace<UIPartySettings>(false);
			break;
		case Action::FIND_MEMBERS:
			UI::get().emplace<UIPartySearchStart>();
			break;
		case Action::LEAVE_PARTY:
			UI::get().emplace<UIYesNo>("Are you sure you want to leave the party?", [](bool yes)
			{
				if (yes)
					LeavePartyPacket().dispatch();
			});
			break;
		case Action::ADD_MEMBER:
			AddBuddyPacket(name).dispatch();
			break;
		case Action::MAKE_LEADER:
			UI::get().emplace<UIYesNo>("Pass party leader to " + name + "?", [id](bool yes)
			{
				if (yes)
					ChangePartyLeaderPacket(id).dispatch();
			});
			break;
		case Action::KICK:
			UI::get().emplace<UIYesNo>("Kick " + name + " from the party?", [id](bool yes)
			{
				if (yes)
					ExpelFromPartyPacket(id).dispatch();
			});
			break;
		case Action::JOIN_PARTY:
			NotificationCenter::get().resolve(id, true);
			break;
		case Action::DECLINE_PARTY:
			NotificationCenter::get().resolve(id, false);
			break;
		}
	}

	Cursor::State UIMobileUserList::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Stay on top and keep the touches after a dialog over this closes
		UI::get().bring_to_front(TYPE, true);

		// Act when the finger lifts where it landed, so a swipe to scroll
		// never presses what it started on
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		cursorpos = at;

		Layout l = layout();

		if (l.close.contains(cursorpos) || !l.panel.contains(cursorpos))
		{
			deactivate();
			return Cursor::State::IDLE;
		}

		for (size_t i = 0; i < NUM_TABS; i++)
		{
			if (l.tabs[i].contains(cursorpos))
			{
				if (tab != i)
				{
					tab = static_cast<uint8_t>(i);
					first_row = 0;
					selected_kind = RowKind::NONE;
				}

				return Cursor::State::CLICKING;
			}
		}

		for (size_t i = 0; i < actions.size() && i < MAX_ACTIONS; i++)
		{
			if (action_rect(l, i).contains(cursorpos))
			{
				run(actions[i]);
				return Cursor::State::CLICKING;
			}
		}

		for (int16_t i = 0; i < l.rows; i++)
		{
			size_t index = static_cast<size_t>(first_row + i);

			if (index >= rows.size() || !row_rect(l, first_row + i).contains(cursorpos))
				continue;

			const Row& row = rows[index];

			if (row.kind == RowKind::GROUP)
			{
				if (collapsed.count(row.group))
					collapsed.erase(row.group);
				else
					collapsed.insert(row.group);
			}
			else if (row.kind == selected_kind && row.id == selected_id)
			{
				selected_kind = RowKind::NONE;
			}
			else if (row.selectable)
			{
				selected_kind = row.kind;
				selected_id = row.id;
			}

			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}

	void UIMobileUserList::send_scroll(double yoffset)
	{
		gesture.cancel();

		first_row += yoffset > 0 ? -1 : 1;

		int16_t max_first = std::max<int16_t>(0, static_cast<int16_t>(rows.size()) - layout().rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);
	}

	void UIMobileUserList::send_key(int32_t, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIMobileUserList::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIMobileUserList::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileUserList::get_type() const
	{
		return TYPE;
	}

	UIMobileAddBuddy::UIMobileAddBuddy(const std::string& name, const std::string& grp)
		: UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), group(grp.empty() ? std::string(DEFAULT_GROUP) : grp)
	{
		int16_t width = std::min<int16_t>(ADD_W, Constants::Constants::get().get_viewwidth() - 24);
		int16_t top = ADD_FIELD_Y + (ADD_FIELD_H - 20) / 2;

		dimension = Point<int16_t>(width, ADD_H);
		position = Point<int16_t>((Constants::Constants::get().get_viewwidth() - width) / 2, ADD_TOP);

		bool adding = name.empty();

		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, adding ? "Add Friend" : "Move to Group");
		placeholder = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::DUSTYGRAY, "Character name");
		group_label = Text(Text::Font::A13M, Text::Alignment::CENTER, Color::Name::WHITE, "");
		ok_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "OK");
		cancel_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Cancel");

		name_field = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::BLACK,
			Rectangle<int16_t>(MARGIN + 12, width - MARGIN - 12, top, top + 20), MAX_NAME_LEN);

		name_field.set_enter_callback([this](std::string) { commit(); });
		name_field.set_key_callback(KeyAction::Id::ESCAPE, [this]() { close(); });

		if (adding)
			name_field.set_state(Textfield::State::FOCUSED);
		else
			name_field.change_text(name);
	}

	void UIMobileAddBuddy::next_group()
	{
		std::set<std::string> names;
		names.insert(DEFAULT_GROUP);

		for (const auto& kv : Stage::get().get_player().get_buddylist().get_entries())
			if (!kv.second.group.empty())
				names.insert(kv.second.group);

		auto it = names.upper_bound(group);
		group = it == names.end() ? *names.begin() : *it;
	}

	void UIMobileAddBuddy::commit()
	{
		std::string name = name_field.get_text();

		if (name.empty())
			return;

		AddBuddyPacket(name, group).dispatch();
		close();
	}

	void UIMobileAddBuddy::close()
	{
		// Let go of the keyboard too. A hidden window stays around, and the
		// keyboard stays up for as long as a text field holds on to it.
		name_field.set_state(Textfield::State::DISABLED);
		deactivate();
	}

	void UIMobileAddBuddy::update()
	{
		// The usable width can change while the keyboard is up
		position = Point<int16_t>((Constants::Constants::get().get_viewwidth() - dimension.x()) / 2, ADD_TOP);

		set_text(group_label, "Group: " + group);
		name_field.update(position);
	}

	void UIMobileAddBuddy::draw(float inter) const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t w = dimension.x();
		int16_t inner_w = w - MARGIN * 2;

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(position, w, dimension.y());
		title.draw(position + Point<int16_t>(w / 2, 6));

		Point<int16_t> field = position + Point<int16_t>(MARGIN, ADD_FIELD_Y);

		if (!classic::input(field, inner_w, ADD_FIELD_H))
		{
			classic::box(field.x(), field.y(), inner_w, ADD_FIELD_H, Color::Name::GALLERY, 0.97f);
			classic::frame(field.x(), field.y(), inner_w, ADD_FIELD_H, Color::Name::HUD_EXP);
		}

		if (name_field.get_text().empty() && name_field.get_state() != Textfield::State::FOCUSED)
			placeholder.draw(position + Point<int16_t>(MARGIN + 12, ADD_FIELD_Y + (ADD_FIELD_H - 20) / 2));

		name_field.draw(position);

		Rectangle<int16_t> g = rect(position.x() + MARGIN, position.y() + ADD_GROUP_Y, inner_w, ADD_GROUP_H);
		classic::button(g.get_left_top(), g.width(), g.height());
		draw_centered(group_label, g);

		Rectangle<int16_t> ok = rect(position.x() + w / 2 - ADD_BUTTON_W - 9, position.y() + ADD_BUTTON_Y, ADD_BUTTON_W, BUTTON_H);
		Rectangle<int16_t> cancel = rect(position.x() + w / 2 + 9, position.y() + ADD_BUTTON_Y, ADD_BUTTON_W, BUTTON_H);

		classic::button(ok.get_left_top(), ok.width(), ok.height());
		draw_centered(ok_label, ok);
		classic::button(cancel.get_left_top(), cancel.width(), cancel.height());
		draw_centered(cancel_label, cancel);

		UIElement::draw(inter);
	}

	Cursor::State UIMobileAddBuddy::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Act on lift. A press outside the text field makes that field let go
		// (UI::send_cursor), so giving it focus during the press undoes itself.
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		int16_t w = dimension.x();
		int16_t inner_w = w - MARGIN * 2;

		if (rect(position.x() + MARGIN, position.y() + ADD_FIELD_Y, inner_w, ADD_FIELD_H).contains(at))
		{
			name_field.set_state(Textfield::State::FOCUSED);
			return Cursor::State::CLICKING;
		}

		if (rect(position.x() + MARGIN, position.y() + ADD_GROUP_Y, inner_w, ADD_GROUP_H).contains(at))
		{
			next_group();
			return Cursor::State::CLICKING;
		}

		if (rect(position.x() + w / 2 - ADD_BUTTON_W - 9, position.y() + ADD_BUTTON_Y, ADD_BUTTON_W, BUTTON_H).contains(at))
		{
			commit();
			return Cursor::State::CLICKING;
		}

		if (rect(position.x() + w / 2 + 9, position.y() + ADD_BUTTON_Y, ADD_BUTTON_W, BUTTON_H).contains(at))
		{
			close();
			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}

	void UIMobileAddBuddy::send_key(int32_t, bool pressed, bool escape)
	{
		if (pressed && escape)
			close();
	}

	bool UIMobileAddBuddy::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIMobileAddBuddy::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileAddBuddy::get_type() const
	{
		return TYPE;
	}
}
