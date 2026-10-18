/* What a search needs besides snapshots and the hidden-card swap: chance
 * events of its own on a simulated branch, the branches' log kept to itself,
 * and the effects a card declares (see docs/snapshot-restore-prototype.md).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <cstdint>
#include "ocgapi.h"
#include "card.h"
#include "duel.h"
#include "duel_arena.h"
#include "effect.h"

int OCG_DuelReseed(OCG_Duel ocg_duel, uint64_t seed) {
	if(!ocg_duel)
		return 0;
	// SplitMix64 spreads the seed over the generator's four words.
	RNG::Xoshiro256StarStar::StateType state{};
	for(auto& word : state) {
		seed += UINT64_C(0x9e3779b97f4a7c15);
		uint64_t z = seed;
		z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
		z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
		word = z ^ (z >> 31);
	}
	static_cast<duel*>(ocg_duel)->reseed(state);
	return 1;
}

int OCG_DuelSwapLogHandler(OCG_Duel ocg_duel, OCG_LogHandler* handler, void** payload) {
	if(!ocg_duel || !handler || !*handler || !payload)
		return 0;
	static_cast<duel*>(ocg_duel)->swap_log_handler(*handler, *payload);
	return 1;
}

int OCG_DuelInitialEffects(OCG_Duel ocg_duel, uint32_t code, void* payload, OCG_InitialEffectHandler append) {
	if(!ocg_duel || !code || !append)
		return 0;
	auto* game = static_cast<duel*>(ocg_duel);
	duel_arena_scope scope(game->arena);
	try {
		// Making the card runs its initial_effect as OCG_DuelNewCard does,
		// without putting it on the field or adjusting anything there.
		auto* definition = game->new_card(code);
		for(const auto* container : {&definition->single_effect,
				&definition->field_effect, &definition->equip_effect,
				&definition->target_effect, &definition->xmaterial_effect}) {
			for(const auto& entry : *container) {
				const auto* peffect = entry.second;
				OCG_InitialEffect described{};
				described.category = peffect->category;
				described.description = peffect->description;
				described.flags = uint64_t(peffect->flag[0]) | (uint64_t(peffect->flag[1]) << 32);
				described.event = peffect->code;
				described.effect_type = peffect->type;
				described.range = peffect->range;
				described.has_cost = peffect->cost != 0;
				described.card_target = peffect->is_flag(EFFECT_FLAG_CARD_TARGET);
				append(payload, &described);
			}
		}
		return 1;
	} catch(...) {
		return 0;
	}
}
