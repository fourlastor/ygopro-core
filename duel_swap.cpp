/* Exchange of two hidden cards of one player, for a search that deals again
 * what a player cannot see (see docs/snapshot-restore-prototype.md).
 *
 * A face-down monster nothing has touched since it reached the field (Normal
 * Set without a Tribute, or Special Summoned face-down by an effect) can take
 * part too, exchanged with a monster of the hand or the Deck, or with another
 * such monster.  Its place on the field (zone, position, the turn it arrived,
 * how it was summoned) stays where it is; only which card holds it changes.
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
#include "group.h"

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
	case LOCATION_MZONE:
		return seq < player.list_mzone.size() ? player.list_mzone[seq] : nullptr;
	default:
		return nullptr;
	}
}

static bool hidden_slot_compatible(const card* pcard, uint32_t loc) {
	if(loc == LOCATION_SZONE)
		return (pcard->data.type & (TYPE_SPELL | TYPE_TRAP)) != 0;
	if(loc == LOCATION_MZONE)
		return (pcard->data.type & TYPE_MONSTER) != 0
			&& (pcard->data.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ | TYPE_LINK | TYPE_RITUAL)) == 0;
	return true;
}

// A link of the current chain is handled by the card, or targets it.
static bool active_chain_references(const field& game_field, const card* pcard) {
	for(const auto& link : game_field.core.current_chain) {
		const auto* peffect = link.triggering_effect;
		if(peffect && (peffect->handler == pcard || peffect->active_handler == pcard
				|| peffect->owner == pcard))
			return true;
		if(link.target_cards && link.target_cards->has_card(const_cast<card*>(pcard)))
			return true;
	}
	return false;
}

// Nothing but the card's own script has put an effect on it, and nothing
// points at it: no counters, equips, materials, targets or relations.
static bool unattached(const card* pcard) {
	for(const auto& entry : pcard->single_effect)
		if(entry.second->owner != pcard)
			return false;
	return pcard->relations.empty() && pcard->counters.empty() && pcard->equiping_cards.empty()
		&& !pcard->equiping_target && !pcard->overlay_target && pcard->xyz_materials.empty()
		&& pcard->material_cards.empty() && pcard->effect_target_owner.empty()
		&& pcard->effect_target_cards.empty() && pcard->relate_effect.empty()
		&& pcard->indestructable_effects.empty() && pcard->announced_cards.empty()
		&& pcard->attacked_cards.empty() && pcard->battled_cards.empty() && !pcard->unique_effect;
}

// A face-down monster that was Normal Set without a Tribute, or Special
// Summoned face-down by an effect (Morphing Jar #2), and untouched since.
static bool untouched_set_monster(const card* pcard) {
	const auto kind = pcard->summon.type;
	return (pcard->current.position & POS_FACEDOWN)
		&& (kind == SUMMON_TYPE_NORMAL || (kind & 0xff000000) == SUMMON_TYPE_SPECIAL)
		&& unattached(pcard);
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
	const bool monsters = loc1 == LOCATION_MZONE || loc2 == LOCATION_MZONE;
	if(monsters) {
		// A monster never passes through the Spell & Trap Zone, and both
		// cards must be free of anything that would not follow the swap.
		if(loc1 == LOCATION_SZONE || loc2 == LOCATION_SZONE)
			return 0;
		if((loc1 == LOCATION_MZONE ? !untouched_set_monster(first) : !unattached(first))
				|| (loc2 == LOCATION_MZONE ? !untouched_set_monster(second) : !unattached(second)))
			return 0;
	}

	first->cancel_field_effect();
	second->cancel_field_effect();
	auto replace = [&](uint32_t loc, uint32_t seq, card* value) {
		auto& player = game_field.player[team];
		switch(loc) {
		case LOCATION_DECK: player.list_main[seq] = value; break;
		case LOCATION_HAND: player.list_hand[seq] = value; break;
		case LOCATION_SZONE: player.list_szone[seq] = value; break;
		case LOCATION_MZONE: player.list_mzone[seq] = value; break;
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
	if(monsters) {
		// What belongs to the place on the field rather than to the card.
		std::swap(first->summon, second->summon);
		std::swap(first->cover, second->cover);
		std::swap(first->attack_announce_count, second->attack_announce_count);
		std::swap(first->direct_attackable, second->direct_attackable);
		std::swap(first->announce_count, second->announce_count);
		std::swap(first->attacked_count, second->attacked_count);
		std::swap(first->attack_all_target, second->attack_all_target);
		std::swap(first->attack_controler, second->attack_controler);
		std::swap(first->position_param, second->position_param);
		std::swap(first->to_field_param, second->to_field_param);
	}
	first->apply_field_effect();
	second->apply_field_effect();
	game_field.adjust_instant();
	return 1;
}
