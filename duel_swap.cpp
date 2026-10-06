/* Exchange of two hidden cards of one player, for a search that deals again
 * what a player cannot see (see docs/snapshot-restore-prototype.md).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <cstdint>
#include <utility>
#include "ocgapi.h"
#include "card.h"
#include "duel.h"
#include "effect.h"
#include "field.h"

static card* hidden_card_at(field& game_field, uint8_t team, uint32_t loc, uint32_t seq) {
	if(team > 1)
		return nullptr;
	auto& player = game_field.player[team];
	switch(loc) {
	case LOCATION_DECK:
		return seq < player.list_main.size() ? player.list_main[seq] : nullptr;
	case LOCATION_HAND:
		return seq < player.list_hand.size() ? player.list_hand[seq] : nullptr;
	case LOCATION_SZONE:
		return seq < player.list_szone.size() ? player.list_szone[seq] : nullptr;
	default:
		return nullptr;
	}
}

static bool hidden_slot_compatible(const card* pcard, uint32_t loc) {
	if(loc != LOCATION_SZONE)
		return true;
	return (pcard->data.type & (TYPE_SPELL | TYPE_TRAP)) != 0;
}

static bool active_chain_references(const field& game_field, const card* pcard) {
	for(const auto& link : game_field.core.current_chain) {
		const auto* peffect = link.triggering_effect;
		if(peffect && (peffect->handler == pcard || peffect->active_handler == pcard
				|| peffect->owner == pcard))
			return true;
	}
	return false;
}

int OCG_DuelSwapHiddenCards(OCG_Duel ocg_duel, uint8_t team,
		uint32_t loc1, uint32_t seq1, uint32_t loc2, uint32_t seq2) {
	if(!ocg_duel || (loc1 == loc2 && seq1 == seq2))
		return 0;
	auto* pduel = static_cast<duel*>(ocg_duel);
	duel_arena_scope arena_scope(pduel->arena);
	auto& game_field = *(pduel->game_field);
	card* first = hidden_card_at(game_field, team, loc1, seq1);
	card* second = hidden_card_at(game_field, team, loc2, seq2);
	if(!first || !second || first == second)
		return 0;
	if(active_chain_references(game_field, first) || active_chain_references(game_field, second))
		return 0;
	if((loc1 == LOCATION_SZONE && !(first->current.position & POS_FACEDOWN))
			|| (loc2 == LOCATION_SZONE && !(second->current.position & POS_FACEDOWN)))
		return 0;
	if(!hidden_slot_compatible(second, loc1) || !hidden_slot_compatible(first, loc2))
		return 0;

	first->cancel_field_effect();
	second->cancel_field_effect();
	auto replace = [&](uint32_t loc, uint32_t seq, card* value) {
		auto& player = game_field.player[team];
		switch(loc) {
		case LOCATION_DECK: player.list_main[seq] = value; break;
		case LOCATION_HAND: player.list_hand[seq] = value; break;
		case LOCATION_SZONE: player.list_szone[seq] = value; break;
		}
	};
	replace(loc1, seq1, second);
	replace(loc2, seq2, first);
	std::swap(first->current, second->current);
	std::swap(first->previous, second->previous);
	std::swap(first->temp, second->temp);
	std::swap(first->status, second->status);
	std::swap(first->fieldid, second->fieldid);
	std::swap(first->fieldid_r, second->fieldid_r);
	std::swap(first->turnid, second->turnid);
	std::swap(first->turn_counter, second->turn_counter);
	first->apply_field_effect();
	second->apply_field_effect();
	game_field.adjust_instant();
	return 1;
}
