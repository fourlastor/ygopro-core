/*
 * Copyright (c) 2019, Dylam De La Torre, (DyXel)
 * Copyright (c) 2019-2025, Edoardo Lolletti (edo9300) <edoardo762@gmail.com>
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef OCGAPI_H
#define OCGAPI_H
#include "ocgapi_types.h"

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

#if defined(OCGCORE_EXPORT_FUNCTIONS)
#if defined(_WIN32)
#define OCGAPI EXTERN_C __declspec(dllexport)
#else
#define OCGAPI EXTERN_C __attribute__ ((visibility ("default")))
#endif
#else
#define OCGAPI EXTERN_C
#endif

/*** CORE INFORMATION ***/
OCGAPI void OCG_GetVersion(int* major, int* minor);

/*** DUEL CREATION AND DESTRUCTION ***/
OCGAPI int OCG_CreateDuel(OCG_Duel* out_ocg_duel, const OCG_DuelOptions* options_ptr);
OCGAPI void OCG_DestroyDuel(OCG_Duel ocg_duel);
OCGAPI void OCG_DuelNewCard(OCG_Duel ocg_duel, const OCG_NewCardInfo* info_ptr);
OCGAPI void OCG_StartDuel(OCG_Duel ocg_duel);

/*** PORTABLE SNAPSHOT/RESTORE PROTOTYPE ***/
/* Snapshots are valid only after OCG_DuelProcess returns AWAITING and before a
 * response is submitted. Restore copies duel-owned arena state back to the
 * same OCG_Duel address. */
OCGAPI int OCG_DuelCreateSnapshot(OCG_Duel ocg_duel, OCG_DuelSnapshot* out_snapshot);
OCGAPI int OCG_DuelRestoreSnapshot(OCG_Duel ocg_duel, OCG_DuelSnapshot snapshot);
OCGAPI void OCG_DuelDestroySnapshot(OCG_DuelSnapshot snapshot);
OCGAPI uint64_t OCG_DuelArenaUsed(OCG_Duel ocg_duel);
OCGAPI uint64_t OCG_DuelSnapshotSize(OCG_DuelSnapshot snapshot);
/* Exchange two cards of one player (team) between the deck, the hand, the
 * Spell & Trap Zone and the Monster Zone: the places a card can be hidden in.
 * Returns 1, or 0 when it refuses: an empty slot, the same slot twice, a card
 * a link of the current chain is handled by or targets, a card a process under
 * way moves or acts on (a summon waiting on its position), a Spell & Trap Zone
 * card that is not face-down, a card that is neither Spell nor Trap landing in
 * that zone, or a card that is not a Main Deck monster landing in the Monster
 * Zone.  A Monster Zone card must be face-down, Normal Set without a Tribute
 * or Special Summoned face-down, with nothing attached to it or pointing at it
 * (counters, equips, materials, targets, effects of other cards), and so must
 * the card it is exchanged with be free of those; the Monster Zone never
 * exchanges with the Spell & Trap Zone. */
OCGAPI int OCG_DuelSwapHiddenCards(OCG_Duel ocg_duel, uint8_t team, uint32_t loc1, uint32_t seq1, uint32_t loc2, uint32_t seq2);
/* Give a simulated branch chance events of its own (coin tosses, dice,
 * shuffles), from a generator seeded with seed.  Only after taking a snapshot:
 * restoring it restores the duel's own generator too.  Returns 1, or 0 for a
 * NULL duel. */
OCGAPI int OCG_DuelReseed(OCG_Duel ocg_duel, uint64_t seed);
/* Log through *handler and *payload from now on, and put the handler and
 * payload the duel logged through until now in their place: a search playing
 * branches of a host's duel keeps their script errors to itself, then swaps
 * the host's back.  The handler is part of what a snapshot keeps: swap before
 * taking the first snapshot of the branches, and back after the last restore.
 * Returns 1, or 0 (and swaps nothing) for a NULL duel or handler. */
OCGAPI int OCG_DuelSwapLogHandler(OCG_Duel ocg_duel, OCG_LogHandler* handler, void** payload);
/* Call append(payload, effect) for each effect the initial_effect of card code
 * declares, without calling any condition, cost, target, value or operation
 * function.  Only on a duel that has not started: the card is made, and stays
 * off the field.  Returns 1, or 0 when the card cannot be made. */
OCGAPI int OCG_DuelInitialEffects(OCG_Duel ocg_duel, uint32_t code, void* payload, OCG_InitialEffectHandler append);

/*** DUEL PROCESSING AND QUERYING ***/
OCGAPI int OCG_DuelProcess(OCG_Duel ocg_duel);
OCGAPI void* OCG_DuelGetMessage(OCG_Duel ocg_duel, uint32_t* length);
OCGAPI void OCG_DuelSetResponse(OCG_Duel ocg_duel, const void* buffer, uint32_t length);
OCGAPI int OCG_LoadScript(OCG_Duel ocg_duel, const char* buffer, uint32_t length, const char* name);

OCGAPI uint32_t OCG_DuelQueryCount(OCG_Duel ocg_duel, uint8_t team, uint32_t loc);
OCGAPI void* OCG_DuelQuery(OCG_Duel ocg_duel, uint32_t* length, const OCG_QueryInfo* info_ptr);
OCGAPI void* OCG_DuelQueryLocation(OCG_Duel ocg_duel, uint32_t* length, const OCG_QueryInfo* info_ptr);
OCGAPI void* OCG_DuelQueryField(OCG_Duel ocg_duel, uint32_t* length);

#undef OCGAPI
#undef EXTERN_C

#endif /* OCGAPI_H */
