//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../OutPacket.h"

#include <string>

namespace ms
{
	// Duey (parcels), v83 DUEY_ACTION as Cosmic's DueyHandler reads it.

	// Send an item and/or mesos to a character by name (normal delivery).
	// mode 2: byte invtype (0 = mesos only), short slot, short amount,
	// int mesos, string recipient, byte quick
	class DueySendPacket : public OutPacket
	{
	public:
		DueySendPacket(int8_t invtype, int16_t slot, int16_t amount, int32_t mesos, const std::string& recipient)
			: OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(2);
			write_byte(invtype);
			write_short(slot);
			write_short(amount);
			write_int(mesos);
			write_string(recipient);
			write_byte(0);
		}
	};

	// Take a package's item and mesos. mode 4: int package id
	class DueyClaimPacket : public OutPacket
	{
	public:
		DueyClaimPacket(int32_t packageid) : OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(4);
			write_int(packageid);
		}
	};

	// Throw a package away. mode 5: int package id
	class DueyRemovePacket : public OutPacket
	{
	public:
		DueyRemovePacket(int32_t packageid) : OutPacket(OutPacket::Opcode::DUEY_ACTION)
		{
			write_byte(5);
			write_int(packageid);
		}
	};
}
