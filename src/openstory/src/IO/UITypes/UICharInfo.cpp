//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "UICharInfo.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "UIChatBar.h"

#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/ClassicStyle.h"

#include "../../Net/Packets/PlayerInteractionPackets.h"
#include "../../Net/Packets/TradePackets.h"
#include "../../Net/Packets/SocialPackets.h"
#include "../../Net/Packets/BuddyPackets.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "UIGuild.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/BotInventoryPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UICharInfo::UICharInfo(int32_t cid) : UIDragElement<PosCHARINFO>(Point<int16_t>(250, 20)), bot_tab_active(false), target_char_id(cid), bot_scroll_offset(0)
	{
		nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
		nl::node UserInfo = nl::nx::ui["UIWindow.img"]["UserInfo"];

		// Use UIWindow2.img backgrounds (correct layer structure) with UIWindow.img buttons
		nl::node UserInfo2 = nl::nx::ui["UIWindow2.img"]["UserInfo"];
		nl::node character = UserInfo2["character"];

		sprites.emplace_back(character["backgrnd"]);
		sprites.emplace_back(character["backgrnd2"]);
		sprites.emplace_back(character["name"]);

		Point<int16_t> backgrnd_dim = Texture(character["backgrnd"]).get_dimensions();
		base_dim = backgrnd_dim;

		pet_bg = UserInfo2["pet"]["backgrnd"];
		pet_bg2 = UserInfo2["pet"]["backgrnd2"];
		ride_bg = UserInfo2["ride"]["backgrnd"];
		ride_bg2 = UserInfo2["ride"]["backgrnd2"];
		wish_bg = UserInfo2["wish"]["backgrnd"];
		wish_bg2 = UserInfo2["wish"]["backgrnd2"];
		panel_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		Point<int16_t> close_dimensions = Point<int16_t>(backgrnd_dim.x() - 21, 6);
		buttons[Buttons::BtClose] = std::make_unique<MapleButton>(close, close_dimensions);

		auto add_button = [&](uint16_t id, nl::node src) {
			if (src.size() > 0)
				buttons[id] = std::make_unique<MapleButton>(src);
		};

		// Buttons from UIWindow2.img/UserInfo/character (have built-in positions)
		add_button(Buttons::BtFamily, character["BtFamily"]);
		add_button(Buttons::BtParty, character["BtParty"]);
		add_button(Buttons::BtPet, character["BtPet"]);
		add_button(Buttons::BtRide, character["BtRide"]);
		add_button(Buttons::BtTrad, character["BtTrad"]);
		add_button(Buttons::BtItem, character["BtItem"]);
		add_button(Buttons::BtPopUp, character["BtPopUp"]);
		add_button(Buttons::BtPopDown, character["BtPopDown"]);
		add_button(Buttons::BtGuild, character["BtGuild"]);

		name = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE);
		job = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		level = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		fame = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		guild = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		alliance = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		if (buttons.count(Buttons::BtPet))
			buttons[Buttons::BtPet]->set_state(Button::State::DISABLED);
		if (buttons.count(Buttons::BtRide))
			buttons[Buttons::BtRide]->set_state(Button::State::DISABLED);

		dimension = backgrnd_dim;
		charinfo_dim = backgrnd_dim;
		dragarea = Point<int16_t>(dimension.x(), 20);


		// Bot inventory labels
		bot_name_label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE);
		bot_level_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);
		bot_meso_label = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::EMPEROR);

		// Load NX sprites for bot inventory (reuse Item inventory assets)
		nl::node Item = nl::nx::ui["UIWindow2.img"]["Item"];
		bot_backgrnd = Item["productionBackgrnd"];
		bot_backgrnd2 = Item["productionBackgrnd2"];
		bot_backgrnd3 = Item["productionBackgrnd3"];
		bot_slot_disabled = Item["disabled"];

#ifdef __ANDROID__
		// Drawn and tapped by hand, in the navy style of the phone panels
		sprites.clear();
		buttons.clear();

		dimension = Point<int16_t>(PHONE_W, PHONE_H);
		charinfo_dim = dimension;
		dragarea = Point<int16_t>(0, 0);
		position = Point<int16_t>(
			(Constants::Constants::get().get_viewwidth() - PHONE_W) / 2,
			(Constants::Constants::get().get_viewheight() - PHONE_H) / 2);

		job.change_color(Color::Name::WHITE);
		level.change_color(Color::Name::WHITE);
		fame.change_color(Color::Name::WHITE);
		guild.change_color(Color::Name::WHITE);
		alliance.change_color(Color::Name::WHITE);
		panel_label.change_color(Color::Name::WHITE);
		bot_level_label.change_color(Color::Name::WHITE);
		bot_meso_label.change_color(Color::Name::HUD_EXP);

		phone_title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Character Info");
		phone_bot_title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Items");
		phone_wish_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, "Wish list");

		// Social actions first: on a phone this window is how you meet someone,
		// so saying hi, partying and adding a buddy are all one tap from a tap on them
		const char* button_names[PHONE_BUTTONS] = { "Say hi", "Whisper", "Party", "Buddy +", "Trade", "Fame +", "Fame -", "Guild", "Items", "Pet", "Mount",
			"Follow me", "Fight here", "Free roam" };

		for (size_t i = 0; i < PHONE_BUTTONS; i++)
			phone_button_labels[i] = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, button_names[i]);

		const char* row_names[PHONE_ROWS] = { "Level", "Job", "Fame", "Guild", "Alliance" };

		for (size_t i = 0; i < PHONE_ROWS; i++)
			phone_row_labels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::HUD_EXP, row_names[i]);

		const char* tab_names[PHONE_TABS] = { "Equip", "Use", "Set-up", "Etc", "Worn" };

		for (size_t i = 0; i < PHONE_TABS; i++)
			phone_tab_labels[i] = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, tab_names[i]);
#endif

		CharInfoRequestPacket(cid).dispatch();
	}

	void UICharInfo::draw(float inter) const
	{
#ifdef __ANDROID__
		if (bot_tab_active && bot_inventory.is_valid())
			draw_phone_bot();
		else
			draw_phone();

		return;
#endif

		if (bot_tab_active && bot_inventory.is_valid())
		{
			draw_bot_inventory(inter);

			auto it = buttons.find(static_cast<uint16_t>(Buttons::BtClose));
			if (it != buttons.end() && it->second)
				it->second->draw(position);
			return;
		}

		UIElement::draw_sprites(inter);

		for (auto& btn : buttons)
		{
			if (btn.first < Buttons::BtBotEquip && btn.second)
				btn.second->draw(position);
		}

		/// Main Window
		int16_t row_height = 18;
		Point<int16_t> text_pos = position + Point<int16_t>(153, 65);

		// Drawn from the snapshot, not the live Char, so the portrait survives the
		// target walking out of view or the map changing while this stays open.
		if (has_preview)
			preview_look.draw(position + Point<int16_t>(63, 129), false,
				Stance::Id::STAND1, Expression::Id::DEFAULT);

		name.draw(position + Point<int16_t>(59, 131));
		level.draw(text_pos + Point<int16_t>(0, row_height * 0));
		job.draw(text_pos + Point<int16_t>(0, row_height * 1));
		fame.draw(text_pos + Point<int16_t>(0, row_height * 2));
		guild.draw(text_pos + Point<int16_t>(0, row_height * 3) + Point<int16_t>(0, 1));
		alliance.draw(text_pos + Point<int16_t>(0, row_height * 4));

		if (panel_mode == 1)
		{
			Point<int16_t> ppos = position + Point<int16_t>(0, base_dim.y());
			pet_bg.draw(ppos);
			pet_bg2.draw(ppos);

			for (size_t i = 0; i < pets_info.size() && i < 3; i++)
			{
				const CharPetInfo& pi = pets_info[i];
				const ItemData& pd = ItemData::get(pi.itemid);
				int16_t row_y = 34 + static_cast<int16_t>(i) * 46;

				if (i == 0)
				{
					Texture stand = nl::nx::item["Pet"][std::to_string(pi.itemid) + ".img"]["stand0"]["0"];

					if (stand.is_valid())
						stand.draw(DrawArgument(ppos + Point<int16_t>(52, 118)));
					else if (pd.is_valid())
						pd.get_icon(false).draw(DrawArgument(ppos + Point<int16_t>(38, 110)));
				}

				if (pd.is_valid())
					pd.get_icon(false).draw(DrawArgument(ppos + Point<int16_t>(110, row_y + 28)));

				panel_label.change_text(pi.name);
				panel_label.draw(ppos + Point<int16_t>(153, row_y - 5));
			}

			if (!pets_info.empty())
			{
				const CharPetInfo& pi = pets_info[0];
				const ItemData& pd = ItemData::get(pi.itemid);
				std::string type = pd.is_valid() ? pd.get_name() : "";

				if (type.size() > 12)
					type = type.substr(0, 11) + "..";

				panel_label.change_text(type);
				panel_label.draw(ppos + Point<int16_t>(52, 148));
				panel_label.change_text(std::to_string(pi.level));
				panel_label.draw(ppos + Point<int16_t>(183, 147));
				panel_label.change_text(std::to_string(pi.closeness));
				panel_label.draw(ppos + Point<int16_t>(52, 167));
				panel_label.change_text(std::to_string(pi.fullness));
				panel_label.draw(ppos + Point<int16_t>(179, 167));
			}
		}
		else if (panel_mode == 2)
		{
			Point<int16_t> rpos = position + Point<int16_t>(0, base_dim.y());
			ride_bg.draw(rpos);
			ride_bg2.draw(rpos);

			panel_label.change_text("Mount Level: " + std::to_string(mount_level));
			panel_label.draw(rpos + Point<int16_t>(24, 28));
			panel_label.change_text("Mount EXP: " + std::to_string(mount_exp));
			panel_label.draw(rpos + Point<int16_t>(24, 48));
			panel_label.change_text("Tiredness: " + std::to_string(mount_tired));
			panel_label.draw(rpos + Point<int16_t>(24, 68));
		}

		if (wish_open)
		{
			Point<int16_t> wpos = position + Point<int16_t>(base_dim.x() + 2, 0);
			wish_bg.draw(wpos);
			wish_bg2.draw(wpos);

			int16_t shown = 0;

			for (size_t i = 0; i < wishlist.size() && shown < 3; i++, shown++)
			{
				const ItemData& wd = ItemData::get(wishlist[i]);
				int16_t wy = 32 + shown * 38;

				if (wd.is_valid())
				{
					wd.get_icon(false).draw(DrawArgument(wpos + Point<int16_t>(14, wy + 30)));
					std::string wn = wd.get_name();

					if (wn.size() > 16)
						wn = wn.substr(0, 15) + "..";

					panel_label.change_text(wn);
					panel_label.draw(wpos + Point<int16_t>(48, wy + 8));
				}
			}

			if (wishlist.empty())
			{
				panel_label.change_text("No wish list items.");
				panel_label.draw(wpos + Point<int16_t>(30, 70));
			}
			else if (wishlist.size() > 3)
			{
				panel_label.change_text("+" + std::to_string(wishlist.size() - 3) + " more");
				panel_label.draw(wpos + Point<int16_t>(60, 140));
			}
		}
	}

	void UICharInfo::update()
	{
		// Keep a copy of the look while the target is still on screen. Once they
		// despawn their Char is gone, and this snapshot is all the portrait has.
		if (Char* c = target())
		{
			preview_look = c->get_look_preview();
			has_preview = true;
		}

#ifdef __ANDROID__
		// Joining or leaving the party adds or drops the order row while the window is open,
		// and taps are only taken inside the window's dimension
		if (!bot_tab_active)
			dimension = Point<int16_t>(PHONE_W, phone_height());
#endif
	}

	// Already on the buddy list: a second request would only get an error back
	bool UICharInfo::target_is_buddy() const
	{
		const Char* chr = target();

		if (!chr)
			return false;

		for (const auto& entry : Stage::get().get_player().get_buddylist().get_entries())
			if (entry.second.name == chr->get_name())
				return true;

		return false;
	}

	bool UICharInfo::target_in_party() const
	{
		if (target_char_id == Stage::get().get_player().get_oid())
			return false;

		for (const PartyMember& member : Stage::get().get_player().get_party().get_members())
			if (member.cid == target_char_id)
				return true;

		return false;
	}

	// Bot companions take orders from party chat, and lead with a name to pick one
	// bot out of the party. Naming the target keeps the others doing what they were.
	void UICharInfo::order_in_party(const std::string& order)
	{
		Char* chr = target();

		if (!chr || !target_in_party())
			return;

		if (auto chatbar = UI::get().get_element<UIChatBar>())
			chatbar->say_in_party(chr->get_name() + " " + order);
	}

	Button::State UICharInfo::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BtOrderFollow:
			order_in_party("follow me");
			return Button::State::NORMAL;
		case Buttons::BtOrderStay:
			order_in_party("stay here");
			return Button::State::NORMAL;
		case Buttons::BtOrderRoam:
			// Free roam takes the bot out of the party, so the order buttons would go stale
			order_in_party("free roam");
			deactivate();
			return Button::State::NORMAL;
		case Buttons::BtWhisper:
			if (target())
			{
				std::string whisper_name = target()->get_name();

				deactivate();

				if (auto chatbar = UI::get().get_element<UIChatBar>())
					chatbar->whisper_to(whisper_name);
			}

			return Button::State::NORMAL;
		case Buttons::BtSayHi:
			if (target())
			{
				// Said aloud with their name in it: bots start a conversation when they hear
				// their name, and to a real player it is simply a greeting. The window closes
				// so the reply and any tap-to-answer options are in view.
				GeneralChatPacket("hi " + target()->get_name(), true).dispatch();
				deactivate();
			}

			return Button::State::NORMAL;
		case Buttons::BtBuddy:
			if (target() && !target_is_buddy())
			{
				// Straight to the request with the default group: the add-buddy form would
				// only ask for a name we already have, and typing is the slow part on a phone
				AddBuddyPacket(target()->get_name(), "Default Group").dispatch();
				chat::log("Buddy request sent to " + target()->get_name() + ".", chat::LineType::YELLOW);
			}

			return Button::State::NORMAL;
		case Buttons::BtClose:
			if (bot_tab_active)
			{
				bot_tab_active = false;
				dimension = charinfo_dim;
				dragarea = Point<int16_t>(dimension.x(), 20);
			}
			else
			{
				deactivate();
			}
			return Button::State::NORMAL;
		case Buttons::BtFamily:
			break;
		case Buttons::BtParty:
			if (target())
				InviteToPartyPacket(target()->get_name()).dispatch();
			deactivate();
			return Button::State::NORMAL;
		case Buttons::BtTrad:
			if (target())
			{
				TradeCreatePacket().dispatch();
				TradeInvitePacket(target()->get_oid()).dispatch();
			}
			deactivate();
			return Button::State::NORMAL;
		case Buttons::BtGuild:
		{
			auto myguild = UI::get().get_element<UIGuild>();

			if (!myguild || !myguild->has_guild())
			{
				chat::log("You must be in a guild (with invite rights) to invite someone.", chat::LineType::RED);
				return Button::State::NORMAL;
			}

			if (target())
			{
				GuildInvitePacket(target()->get_name()).dispatch();
				chat::log("Sent a guild invite to " + target()->get_name() + ".", chat::LineType::YELLOW);
			}
			return Button::State::NORMAL;
		}
		case Buttons::BtItem:
		{
			if (bot_inventory.is_valid())
			{
				bot_tab_active = true;
				dimension = bot_backgrnd.get_dimensions();
				dragarea = Point<int16_t>(dimension.x(), 20);
				load_bot_icons();
			}
			else
			{
				wish_open = !wish_open;
			}
			return Button::State::NORMAL;
		}
		case Buttons::BtRide:
			panel_mode = (panel_mode == 2) ? 0 : 2;
			dimension = Point<int16_t>(base_dim.x(), panel_mode != 0 ? base_dim.y() + 190 : base_dim.y());
			return Button::State::NORMAL;
		case Buttons::BtPet:
			panel_mode = (panel_mode == 1) ? 0 : 1;
			dimension = Point<int16_t>(base_dim.x(), panel_mode != 0 ? base_dim.y() + 200 : base_dim.y());
			return Button::State::NORMAL;
		case Buttons::BtPopUp:
		case Buttons::BtPopDown:
			// Cosmic silently drops fame packets from characters below
			// level 15 (and self-fames) — tell the player instead of
			// looking like a dead button.
			if (Stage::get().get_player().get_level() < 15)
			{
				chat::log("You must be at least Level 15 to raise or drop someone's fame.", chat::LineType::RED);
			}
			else if (target())
			{
				GiveFamePacket(target()->get_oid(), buttonid == Buttons::BtPopUp).dispatch();
			}
			else
			{
				chat::log("That character is no longer nearby.", chat::LineType::RED);
			}
			return Button::State::NORMAL;
		case Buttons::BtBotEquip:
			bot_sub_tab = 0;
			bot_scroll_offset = 0;
			load_bot_icons();
			return Button::State::NORMAL;
		case Buttons::BtBotUse:
			bot_sub_tab = 1;
			bot_scroll_offset = 0;
			load_bot_icons();
			return Button::State::NORMAL;
		case Buttons::BtBotSetup:
			bot_sub_tab = 2;
			bot_scroll_offset = 0;
			load_bot_icons();
			return Button::State::NORMAL;
		case Buttons::BtBotEtc:
			bot_sub_tab = 3;
			bot_scroll_offset = 0;
			load_bot_icons();
			return Button::State::NORMAL;
		case Buttons::BtBotEquipped:
			bot_sub_tab = 4;
			bot_scroll_offset = 0;
			load_bot_icons();
			return Button::State::NORMAL;
		default:
			break;
		}

		return Button::State::DISABLED;
	}

	bool UICharInfo::is_in_range(Point<int16_t> cursorpos) const
	{
		Rectangle<int16_t> bounds = Rectangle<int16_t>(position, position + dimension);
		return bounds.contains(cursorpos);
	}

	void UICharInfo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			if (bot_tab_active)
			{
				bot_tab_active = false;
				dimension = charinfo_dim;
				dragarea = Point<int16_t>(dimension.x(), 20);
			}
			else
			{
				deactivate();
			}
		}
	}

	UIElement::Type UICharInfo::get_type() const
	{
		return TYPE;
	}

	void UICharInfo::update_stats(int32_t character_id, int16_t job_id, int8_t lv, int16_t f, std::string g, std::string a)
	{
		int32_t player_id = Stage::get().get_player().get_oid();

		auto disable_button = [&](uint16_t id) {
			if (buttons.count(id) && buttons[id])
				buttons[id]->set_state(Button::State::DISABLED);
		};

		if (character_id == player_id)
		{
			disable_button(Buttons::BtParty);
			disable_button(Buttons::BtTrad);
			disable_button(Buttons::BtPopUp);
			disable_button(Buttons::BtPopDown);
		}

		Job character_job = Job(job_id);

		if (target())
			name.change_text(target()->get_name());

		job.change_text(character_job.get_name());
		level.change_text(std::to_string(lv));
		fame.change_text(std::to_string(f));
		guild.change_text((g == "" ? "-" : g));
		alliance.change_text(a);

		if (buttons.count(Buttons::BtPet))
		{
			if (target() && target()->has_pet())
				buttons[Buttons::BtPet]->set_state(Button::State::NORMAL);
			else
				buttons[Buttons::BtPet]->set_state(Button::State::DISABLED);
		}

		if (buttons.count(Buttons::BtRide))
		{
			if (target() && target()->has_mount())
				buttons[Buttons::BtRide]->set_state(Button::State::NORMAL);
			else
				buttons[Buttons::BtRide]->set_state(Button::State::DISABLED);
		}
	}

	void UICharInfo::update_fame(int16_t f)
	{
		fame.change_text(std::to_string(f));
	}

	void UICharInfo::set_extra_info(std::vector<CharPetInfo> pets, bool mount,
		int32_t m_level, int32_t m_exp, int32_t m_tired, std::vector<int32_t> wish)
	{
		pets_info = std::move(pets);
		has_mount = mount;
		mount_level = m_level;
		mount_exp = m_exp;
		mount_tired = m_tired;
		wishlist = std::move(wish);

		if (buttons.count(Buttons::BtPet))
			buttons[Buttons::BtPet]->set_state(pets_info.empty()
				? Button::State::DISABLED : Button::State::NORMAL);

		if (buttons.count(Buttons::BtRide))
			buttons[Buttons::BtRide]->set_state(has_mount
				? Button::State::NORMAL : Button::State::DISABLED);
	}

	Char* UICharInfo::target() const
	{
		return Stage::get().get_character(target_char_id).get();
	}

	int32_t UICharInfo::get_char_id() const
	{
		return target_char_id;
	}

	void UICharInfo::set_bot_inventory(BotInventoryData data)
	{
		bot_inventory = std::move(data);
		bot_scroll_offset = 0;

#ifndef __ANDROID__
		// Create tab buttons if they don't exist
		if (!buttons.count(Buttons::BtBotEquip))
		{
			nl::node Item = nl::nx::ui["UIWindow2.img"]["Item"];
			nl::node Tab = Item["Tab"];
			nl::node taben = Tab["enabled"];
			nl::node tabdis = Tab["disabled"];

			buttons[Buttons::BtBotEquip] = std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
			buttons[Buttons::BtBotUse] = std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
			buttons[Buttons::BtBotSetup] = std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"]);
			buttons[Buttons::BtBotEtc] = std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
			buttons[Buttons::BtBotEquipped] = std::make_unique<TwoSpriteButton>(tabdis["4"], taben["4"]);
		}
#endif

		bot_sub_tab = 4;
		bot_tab_active = true;
#ifndef __ANDROID__
		dimension = bot_backgrnd.get_dimensions();
		dragarea = Point<int16_t>(dimension.x(), 20);
#endif

		bot_name_label.change_text(bot_inventory.name);
		bot_level_label.change_text("Lv. " + std::to_string(bot_inventory.level));

		std::string meso_str = std::to_string(bot_inventory.meso);
		string_format::split_number(meso_str);
		bot_meso_label.change_text("Meso: " + meso_str);

		load_bot_icons();
	}

	void UICharInfo::draw_bot_inventory(float inter) const
	{
		bot_backgrnd.draw(position);
		bot_backgrnd2.draw(position);
		bot_backgrnd3.draw(position);

		for (uint16_t i = Buttons::BtBotEquip; i <= Buttons::BtBotEquipped; i++)
		{
			auto it = buttons.find(i);
			if (it != buttons.end() && it->second)
				it->second->draw(position);
		}

		const auto& items = bot_inventory.get_tab(bot_sub_tab);
		draw_bot_item_grid(items, BOT_GRID_Y - bot_scroll_offset);

		bot_name_label.draw(position + Point<int16_t>(10, 262));
		bot_level_label.draw(position + Point<int16_t>(80, 262));
		bot_meso_label.draw(position + Point<int16_t>(10, 278));
	}

	void UICharInfo::draw_bot_item_grid(const std::vector<BotItem>& items, int16_t start_y) const
	{
		for (size_t i = 0; i < BOT_MAX_SLOTS; i++)
		{
			Point<int16_t> slotpos = position + get_bot_slotpos(static_cast<int16_t>(i));

			if (i >= items.size())
			{
				bot_slot_disabled.draw(slotpos);
			}
			else
			{
				auto it = bot_icons.find(static_cast<int16_t>(i));
				if (it != bot_icons.end() && it->second)
					it->second->draw(slotpos);
				else
					bot_slot_disabled.draw(slotpos);
			}
		}
	}

	Cursor::State UICharInfo::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
#ifdef __ANDROID__
		if (bot_tab_active && bot_inventory.is_valid())
		{
			int16_t slot_index = phone_slot_at(cursorpos);

			if (Icon* icon = get_bot_icon(slot_index))
			{
				if (clicked)
				{
					icon->start_drag(cursorpos - position - phone_icon_pos(slot_index));
					UI::get().drag_icon(icon);
					gesture.reset();

					return Cursor::State::GRABBING;
				}

				const auto& items = bot_inventory.get_tab(bot_sub_tab);

				if (slot_index < static_cast<int16_t>(items.size()))
					UI::get().show_item(Tooltip::Parent::CHARINFO, items[slot_index].item_id);
			}
		}

		// Act when the finger lifts where it landed
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		return phone_tap(at);
#endif

		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		if (bot_tab_active && bot_inventory.is_valid())
		{
			Point<int16_t> cursor_rel = cursorpos - position;
			int16_t slot_index = bot_slot_by_position(cursor_rel);
			Icon* icon = get_bot_icon(slot_index);

			if (icon)
			{
				if (clicked)
				{
					Point<int16_t> slotpos = get_bot_slotpos(slot_index);
					icon->start_drag(cursor_rel - slotpos);
					UI::get().drag_icon(icon);

					return Cursor::State::GRABBING;
				}
				else
				{
					const auto& items = bot_inventory.get_tab(bot_sub_tab);
					if (slot_index < static_cast<int16_t>(items.size()))
						UI::get().show_item(Tooltip::Parent::CHARINFO, items[slot_index].item_id);

					return Cursor::State::CANGRAB;
				}
			}
		}

		return dstate;
	}

	void UICharInfo::send_scroll(double yoffset)
	{
		if (bot_tab_active && bot_inventory.is_valid())
		{
			const auto& items = bot_inventory.get_tab(bot_sub_tab);
			int rows = (static_cast<int>(items.size()) + BOT_COLS - 1) / BOT_COLS;
			int16_t max_scroll = std::max(0, rows * BOT_ICON_H - (dimension.y() - BOT_GRID_Y - 10));

			bot_scroll_offset -= static_cast<int16_t>(yoffset * BOT_ICON_H);

			if (bot_scroll_offset < 0)
				bot_scroll_offset = 0;
			else if (bot_scroll_offset > max_scroll)
				bot_scroll_offset = max_scroll;
		}
	}

	bool UICharInfo::send_icon(const Icon& icon, Point<int16_t> cursorpos)
	{
		if (!bot_tab_active || !bot_inventory.is_valid())
			return false;

		InventoryType::Id src_tab = icon.get_source_tab();
		int16_t src_slot = icon.get_source_slot();

		if (src_tab == InventoryType::Id::NONE || src_slot < 0)
			return false;

		int8_t inv_type = static_cast<int8_t>(src_tab);
		BotInvGivePacket(bot_inventory.char_id, inv_type, src_slot).dispatch();

		return true;
	}

	void UICharInfo::load_bot_icons()
	{
		bot_icons.clear();
		const auto& items = bot_inventory.get_tab(bot_sub_tab);
		int8_t inv_type = bot_inventory.get_inv_type(bot_sub_tab);

		for (size_t i = 0; i < items.size(); i++)
		{
			const auto& bi = items[i];
			int32_t item_prefix = bi.item_id / 10000;
			bool is_card = (item_prefix == 238 || item_prefix == 239);
			const Texture& texture = ItemData::get(bi.item_id).get_icon(is_card);
			int16_t count = (bot_sub_tab == 0 || bot_sub_tab == 4) ? -1 : bi.count;

			bot_icons[static_cast<int16_t>(i)] = std::make_unique<Icon>(
				std::make_unique<BotItemIcon>(bot_inventory.char_id, inv_type, bi.slot, bi.item_id, bi.count, bot_sub_tab),
				texture, count
			);
		}
	}

	Point<int16_t> UICharInfo::get_bot_slotpos(int16_t slot_index) const
	{
		div_t d = std::div(slot_index, BOT_COLS);
		int16_t start_y = BOT_GRID_Y - bot_scroll_offset;
		return Point<int16_t>(BOT_GRID_X + BOT_ICON_W * d.rem, start_y + BOT_ICON_H * d.quot);
	}

	int16_t UICharInfo::bot_slot_by_position(Point<int16_t> cursor_rel) const
	{
		int16_t start_y = BOT_GRID_Y - bot_scroll_offset;
		int16_t rx = cursor_rel.x() - BOT_GRID_X;
		int16_t ry = cursor_rel.y() - start_y;

		if (rx < 0 || ry < 0)
			return -1;

		int16_t col = rx / BOT_ICON_W;
		int16_t row = ry / BOT_ICON_H;

		if (col >= BOT_COLS || col < 0)
			return -1;

		return row * BOT_COLS + col;
	}

	Icon* UICharInfo::get_bot_icon(int16_t slot_index)
	{
		if (slot_index < 0)
			return nullptr;

		auto it = bot_icons.find(slot_index);
		if (it != bot_icons.end())
			return it->second.get();

		return nullptr;
	}

	void UICharInfo::remove_cursor()
	{
#ifdef __ANDROID__
		gesture.reset();
#endif

		UIDragElement::remove_cursor();
		UI::get().clear_tooltip(Tooltip::Parent::CHARINFO);
	}

#ifdef __ANDROID__
	Rectangle<int16_t> UICharInfo::phone_close() const
	{
		Point<int16_t> tl = position + Point<int16_t>(PHONE_W - 36, 6);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(30, 30));
	}

	Rectangle<int16_t> UICharInfo::phone_button(size_t index) const
	{
		constexpr int16_t W = 62;
		constexpr int16_t H = 34;
		constexpr int16_t GAP = 8;

		int16_t col = static_cast<int16_t>(index % PHONE_BUTTON_COLS);
		int16_t row = static_cast<int16_t>(index / PHONE_BUTTON_COLS);

		// The orders get a row of their own under the social and info buttons
		if (index >= PHONE_BUTTONS - PHONE_ORDER_BUTTONS)
		{
			col = static_cast<int16_t>(index - (PHONE_BUTTONS - PHONE_ORDER_BUTTONS));
			row = 2;
		}

		Point<int16_t> tl = position + Point<int16_t>(14 + col * (W + GAP), 216 + row * PHONE_ROW_STEP);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(W, H));
	}

	Rectangle<int16_t> UICharInfo::phone_tab(size_t index) const
	{
		constexpr int16_t W = 78;
		constexpr int16_t H = 30;
		constexpr int16_t GAP = 6;

		Point<int16_t> tl = position + Point<int16_t>(12 + static_cast<int16_t>(index) * (W + GAP), 40);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(W, H));
	}

	Rectangle<int16_t> UICharInfo::phone_slot(int16_t index) const
	{
		constexpr int16_t COLS = 8;
		constexpr int16_t STEP = 46;
		constexpr int16_t SIZE = 42;

		Point<int16_t> tl = position + Point<int16_t>(36 + (index % COLS) * STEP, 76 + (index / COLS) * STEP);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(SIZE, SIZE));
	}

	// Where an item's icon draws in its slot, relative to the window
	Point<int16_t> UICharInfo::phone_icon_pos(int16_t index) const
	{
		Rectangle<int16_t> slot = phone_slot(index);
		Point<int16_t> at = slot.get_left_top() - position;
		const auto& items = bot_inventory.get_tab(bot_sub_tab);

		if (index < 0 || index >= static_cast<int16_t>(items.size()))
			return at;

		const Texture& texture = ItemData::get(items[index].item_id).get_icon(false);
		Point<int16_t> dim = texture.get_dimensions();
		Point<int16_t> origin = texture.get_origin();

		return at + Point<int16_t>((slot.width() - dim.x()) / 2 + origin.x(), (slot.height() - dim.y()) / 2 + origin.y());
	}

	int16_t UICharInfo::phone_slot_at(Point<int16_t> cursorpos) const
	{
		for (int16_t i = 0; i < BOT_MAX_SLOTS; i++)
			if (phone_slot(i).contains(cursorpos))
				return i;

		return -1;
	}

	uint16_t UICharInfo::phone_button_id(size_t index) const
	{
		const uint16_t ids[PHONE_BUTTONS] = {
			Buttons::BtSayHi, Buttons::BtWhisper, Buttons::BtParty, Buttons::BtBuddy, Buttons::BtTrad, Buttons::BtPopUp,
			Buttons::BtPopDown, Buttons::BtGuild, Buttons::BtItem, Buttons::BtPet, Buttons::BtRide,
			Buttons::BtOrderFollow, Buttons::BtOrderStay, Buttons::BtOrderRoam
		};

		return ids[index];
	}

	// The order row only exists while the target is in your party
	size_t UICharInfo::phone_button_count() const
	{
		return target_in_party() ? PHONE_BUTTONS : PHONE_BUTTONS - PHONE_ORDER_BUTTONS;
	}

	int16_t UICharInfo::phone_height() const
	{
		return target_in_party() ? PHONE_H + PHONE_ROW_STEP : PHONE_H;
	}

	bool UICharInfo::phone_button_enabled(size_t index) const
	{
		switch (phone_button_id(index))
		{
		case Buttons::BtBuddy:
			return target_char_id != Stage::get().get_player().get_oid() && !target_is_buddy();
		case Buttons::BtSayHi:
		case Buttons::BtWhisper:
		case Buttons::BtParty:
		case Buttons::BtTrad:
		case Buttons::BtPopUp:
		case Buttons::BtPopDown:
			return target_char_id != Stage::get().get_player().get_oid();
		case Buttons::BtPet:
			return !pets_info.empty();
		case Buttons::BtRide:
			return has_mount;
		default:
			return true;
		}
	}

	bool UICharInfo::phone_button_lit(size_t index) const
	{
		switch (phone_button_id(index))
		{
		case Buttons::BtPet:
			return panel_mode == 1;
		case Buttons::BtRide:
			return panel_mode == 2;
		case Buttons::BtItem:
			return wish_open;
		default:
			return false;
		}
	}

	void UICharInfo::draw_phone() const
	{
		classic::panel(position, PHONE_W, phone_height());
		phone_title.draw(position + Point<int16_t>(PHONE_W / 2, 6));

		Rectangle<int16_t> close = phone_close();

		if (!classic::close_button(close.get_left_top(), close.width()))
			classic::button(close.get_left_top(), close.width(), close.height());

		// Portrait
		classic::box(position.x() + 16, position.y() + 40, 136, 166, Color::Name::BLACK, 0.3f);

		if (has_preview)
			preview_look.draw(position + Point<int16_t>(84, 176), false, Stance::Id::STAND1, Expression::Id::DEFAULT);

		name.draw(position + Point<int16_t>(84, 180));

		// Info, or what the Pet, Mount or Wish list button opened
		if (panel_mode != 0 || wish_open)
		{
			draw_phone_details();
		}
		else
		{
			const Text* values[PHONE_ROWS] = { &level, &job, &fame, &guild, &alliance };

			for (size_t i = 0; i < PHONE_ROWS; i++)
			{
				Point<int16_t> at = position + Point<int16_t>(168, 44 + static_cast<int16_t>(i) * 32);

				phone_row_labels[i].draw(at);

				if (values[i]->get_text().empty())
				{
					panel_label.change_text("-");
					panel_label.draw(at + Point<int16_t>(72, 0));
				}
				else
				{
					values[i]->draw(at + Point<int16_t>(72, 0));
				}
			}
		}

		bool shows_wish_list = !bot_inventory.is_valid();

		for (size_t i = 0; i < phone_button_count(); i++)
		{
			Rectangle<int16_t> r = phone_button(i);
			bool enabled = phone_button_enabled(i);

			if (enabled)
				classic::button(r.get_left_top(), r.width(), r.height(), phone_button_lit(i));
			else
				classic::cell(r.get_left_top(), r.width(), r.height(), false);

			const Text& label = phone_button_id(i) == Buttons::BtItem && shows_wish_list ? phone_wish_label : phone_button_labels[i];
			Point<int16_t> at(r.left() + r.width() / 2, classic::text_y(label, r.top() + r.height() / 2));

			if (enabled)
				classic::shadowed(label, at);
			else
				label.draw(DrawArgument(at, 0.4f));
		}
	}

	void UICharInfo::draw_phone_details() const
	{
		Point<int16_t> tl = position + Point<int16_t>(168, 44);

		auto line = [&](const std::string& text, int16_t x, int16_t y)
		{
			panel_label.change_text(text);
			panel_label.draw(tl + Point<int16_t>(x, y));
		};

		auto icon = [&](int32_t itemid, int16_t y)
		{
			const ItemData& data = ItemData::get(itemid);

			if (data.is_valid())
			{
				const Texture& texture = data.get_icon(false);
				texture.draw(DrawArgument(tl + texture.get_origin() + Point<int16_t>(0, y)));
			}
		};

		if (panel_mode == 1)
		{
			for (size_t i = 0; i < pets_info.size() && i < 3; i++)
			{
				const CharPetInfo& pet = pets_info[i];
				int16_t y = static_cast<int16_t>(i) * 54;

				icon(pet.itemid, y);
				line(pet.name, 42, y);
				line("Lv. " + std::to_string(pet.level) + "   Closeness " + std::to_string(pet.closeness) + "   Fullness " + std::to_string(pet.fullness), 42, y + 20);
			}
		}
		else if (panel_mode == 2)
		{
			line("Mount level: " + std::to_string(mount_level), 0, 0);
			line("Mount EXP: " + std::to_string(mount_exp), 0, 32);
			line("Tiredness: " + std::to_string(mount_tired), 0, 64);
		}
		else if (wishlist.empty())
		{
			line("No wish list items.", 0, 0);
		}
		else
		{
			constexpr size_t SHOWN = 4;

			for (size_t i = 0; i < wishlist.size() && i < SHOWN; i++)
			{
				const ItemData& data = ItemData::get(wishlist[i]);
				int16_t y = static_cast<int16_t>(i) * 38;

				icon(wishlist[i], y);
				line(data.is_valid() ? data.get_name() : std::string("?"), 42, y + 8);
			}

			if (wishlist.size() > SHOWN)
				line("+" + std::to_string(wishlist.size() - SHOWN) + " more", 0, 152);
		}
	}

	void UICharInfo::draw_phone_bot() const
	{
		classic::panel(position, PHONE_W, PHONE_H);
		phone_bot_title.draw(position + Point<int16_t>(PHONE_W / 2, 6));

		Rectangle<int16_t> close = phone_close();

		if (!classic::close_button(close.get_left_top(), close.width()))
			classic::button(close.get_left_top(), close.width(), close.height());

		for (size_t i = 0; i < PHONE_TABS; i++)
		{
			Rectangle<int16_t> r = phone_tab(i);

			classic::tab(r.get_left_top(), r.width(), r.height(), static_cast<int8_t>(i) == bot_sub_tab);
			phone_tab_labels[i].draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(phone_tab_labels[i], r.top() + r.height() / 2)));
		}

		for (int16_t i = 0; i < BOT_MAX_SLOTS; i++)
		{
			Rectangle<int16_t> r = phone_slot(i);

			classic::slot(r.get_left_top(), r.width(), r.height(), false);

			auto it = bot_icons.find(i);

			if (it != bot_icons.end() && it->second)
				it->second->draw(position + phone_icon_pos(i));
		}

		bot_name_label.draw(position + Point<int16_t>(36, 224));
		bot_level_label.draw(position + Point<int16_t>(36, 246));
		bot_meso_label.draw(position + Point<int16_t>(120, 246));
	}

	Cursor::State UICharInfo::phone_tap(Point<int16_t> at)
	{
		if (phone_close().contains(at))
		{
			button_pressed(Buttons::BtClose);
			return Cursor::State::IDLE;
		}

		if (bot_tab_active && bot_inventory.is_valid())
		{
			for (size_t i = 0; i < PHONE_TABS; i++)
			{
				if (phone_tab(i).contains(at))
				{
					button_pressed(static_cast<uint16_t>(Buttons::BtBotEquip + i));
					return Cursor::State::CLICKING;
				}
			}

			return Cursor::State::IDLE;
		}

		for (size_t i = 0; i < phone_button_count(); i++)
		{
			if (!phone_button(i).contains(at) || !phone_button_enabled(i))
				continue;

			uint16_t id = phone_button_id(i);

			// One thing at a time shows in the info column
			if (id == Buttons::BtItem)
				panel_mode = 0;
			else if (id == Buttons::BtPet || id == Buttons::BtRide)
				wish_open = false;

			button_pressed(id);

			// The desktop window grows for these; this one doesn't
			if (!bot_tab_active)
				dimension = Point<int16_t>(PHONE_W, phone_height());
			dragarea = Point<int16_t>(0, 0);

			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}
#endif

	// BotItemIcon implementation
	UICharInfo::BotItemIcon::BotItemIcon(int32_t bid, int8_t it, int16_t s, int32_t iid, int16_t c, int8_t st)
		: bot_id(bid), inv_type(it), slot(s), item_id(iid), count(c), sub_tab(st) {}

	void UICharInfo::BotItemIcon::drop_on_stage() const
	{
		if (sub_tab == 4)
		{
			BotInvEquipPacket(bot_id, slot, static_cast<int16_t>(-slot)).dispatch();
		}
		else
		{
			BotInvTakePacket(bot_id, inv_type, slot).dispatch();
		}
	}

	void UICharInfo::BotItemIcon::drop_on_equips(EquipSlot::Id) const {}

	bool UICharInfo::BotItemIcon::drop_on_items(InventoryType::Id, EquipSlot::Id, int16_t, bool) const
	{
		if (sub_tab == 4)
		{
			BotInvEquipPacket(bot_id, slot, static_cast<int16_t>(-slot)).dispatch();
		}
		else
		{
			BotInvTakePacket(bot_id, inv_type, slot).dispatch();
		}
		return true;
	}

	void UICharInfo::BotItemIcon::drop_on_bindings(Point<int16_t>, bool) const {}
	void UICharInfo::BotItemIcon::set_count(int16_t c) { count = c; }
	Icon::IconType UICharInfo::BotItemIcon::get_type() { return Icon::IconType::ITEM; }
}
