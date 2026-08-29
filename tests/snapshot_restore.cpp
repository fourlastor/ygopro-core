#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib>
#include "ocgapi.h"
#include "ocgapi_constants.h"
static void data(void*, uint32_t code, OCG_CardData* d) { *d = {}; d->code=code; d->type=TYPE_MONSTER|TYPE_EFFECT; d->level=4; d->attribute=ATTRIBUTE_LIGHT; d->race=RACE_WARRIOR; d->attack=1000; d->defense=1000; }
static const char effect_script[] = R"(local s={}
function s.initial_effect(c)
 local e=Effect.CreateEffect(c)
 e:SetType(0x10)
 e:SetRange(0x04)
 e:SetOperation(s.op)
 c:RegisterEffect(e)
end
function s.op(e,tp,eg,ep,ev,re,r,rp)
 Duel.SelectMatchingCard(tp,Card.IsAbleToHand,tp,0x01,0,1,1,nil)
end)";
static int script(void*, OCG_Duel d, const char* name) {
 if(std::strcmp(name,"c100.lua")==0) return OCG_LoadScript(d,effect_script,sizeof(effect_script)-1,name);
 return 0;
}
static void effect_fixture(OCG_Duel d);
static OCG_Duel make(bool effect=false) {
	OCG_DuelOptions o{}; o.seed[0]=1; o.team1={8000,0,0}; o.team2={8000,0,0}; o.cardReader=data; o.scriptReader=script;
	OCG_Duel d{}; assert(OCG_CreateDuel(&d,&o)==0); if(effect) effect_fixture(d); OCG_StartDuel(d);
	int s; for(int i=0;i<16 && (s=OCG_DuelProcess(d))==OCG_DUEL_STATUS_CONTINUE;++i) {} assert(s==OCG_DUEL_STATUS_AWAITING); return d;
}
static void effect_fixture(OCG_Duel d) {
	OCG_NewCardInfo monster{}; monster.team=0; monster.code=100; monster.con=0; monster.loc=LOCATION_MZONE; monster.pos=POS_FACEUP_ATTACK;
	OCG_DuelNewCard(d,&monster);
	OCG_NewCardInfo deck{}; deck.team=0; deck.code=101; deck.con=0; deck.loc=LOCATION_DECK; deck.pos=POS_FACEDOWN_DEFENSE;
	OCG_DuelNewCard(d,&deck);
}
static std::vector<uint8_t> branch(OCG_Duel d, uint8_t response, int& status) {
	OCG_DuelSetResponse(d,&response,1); status=OCG_DuelProcess(d); uint32_t n{}; auto* p=(uint8_t*)OCG_DuelGetMessage(d,&n); return {p,p+n};
}
int main() {
	OCG_Duel d=make(); OCG_DuelSnapshot s{}; assert(OCG_DuelCreateSnapshot(d,&s)==0);
	int a{},b{},r{}; auto first=branch(d,7,a); assert(OCG_DuelRestoreSnapshot(d,s)==0); auto same=branch(d,7,b); assert(a==b && first==same);
	assert(OCG_DuelRestoreSnapshot(d,s)==0); auto diverged=branch(d,6,b); OCG_Duel ref=make(); auto expected=branch(ref,6,r); assert(b==r && diverged==expected);
	OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(ref); OCG_DestroyDuel(d);
}
