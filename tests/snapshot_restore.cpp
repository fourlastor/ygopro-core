#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include "ocgapi.h"
#include "ocgapi_constants.h"
static void data(void*, uint32_t code, OCG_CardData* d) { *d = {}; d->code=code; d->type=TYPE_MONSTER|TYPE_EFFECT; d->level=4; d->attribute=ATTRIBUTE_LIGHT; d->race=RACE_WARRIOR; d->attack=1000; d->defense=1000; }
static const char effect_script[] = R"(local s=c100
function s.initial_effect(c)
 local e=Effect.CreateEffect(c)
 e:SetType(0x40)
 e:SetRange(0x04)
 e:SetOperation(s.op)
 c:RegisterEffect(e)
end
function s.op(e,tp,eg,ep,ev,re,r,rp)
 Duel.SelectMatchingCard(tp,Card.IsAbleToHand,tp,0x01,0,1,1,nil)
end)";
static int script_requests{};
static int script(void*, OCG_Duel d, const char* name) {
	if(std::strcmp(name,"c100.lua")==0) { ++script_requests; return OCG_LoadScript(d,effect_script,sizeof(effect_script)-1,name); }
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
static bool has(const std::vector<uint8_t>& b, uint8_t type) { for(size_t i=0;i+5<=b.size();) { uint32_t n; std::memcpy(&n,&b[i],4); if(n && i+4+n<=b.size() && b[i+4]==type) return true; i+=4+n; } return false; }
static void dump(const std::vector<uint8_t>& b) { for(size_t i=0;i+5<=b.size();) { uint32_t n; std::memcpy(&n,&b[i],4); std::fprintf(stderr,"frame type=%u len=%u\\n",b[i+4],n); i+=4+n; } }
static std::vector<uint8_t> send(OCG_Duel d, uint32_t v, int& status) { OCG_DuelSetResponse(d,&v,4); std::vector<uint8_t> out; do { status=OCG_DuelProcess(d); uint32_t n{}; auto*p=(uint8_t*)OCG_DuelGetMessage(d,&n); out.insert(out.end(),p,p+n); } while(status==OCG_DUEL_STATUS_CONTINUE); return out; }
int main() {
	OCG_Duel d=make(); OCG_DuelSnapshot s{}; assert(OCG_DuelCreateSnapshot(d,&s)==0);
	int a{},b{},r{}; auto first=branch(d,7,a); assert(OCG_DuelRestoreSnapshot(d,s)==0); auto same=branch(d,7,b); assert(a==b && first==same);
	assert(OCG_DuelRestoreSnapshot(d,s)==0); auto diverged=branch(d,6,b); OCG_Duel ref=make(); auto expected=branch(ref,6,r); assert(b==r && diverged==expected);
	OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(ref); OCG_DestroyDuel(d);
	OCG_Duel e=make(true); assert(script_requests); auto f=send(e,5,a); for(int i=0;i<4 && has(f,MSG_SELECT_CHAIN);++i) f=send(e,0xffffffffu,a); if(!has(f,MSG_SELECT_CARD)) dump(f); assert(has(f,MSG_SELECT_CARD)); assert(OCG_DuelCreateSnapshot(e,&s)==0);
	auto chosen=branch(e,1,a); assert(OCG_DuelRestoreSnapshot(e,s)==0); auto repeated=branch(e,1,b); assert(a==b && chosen==repeated);
	assert(OCG_DuelRestoreSnapshot(e,s)==0); auto other=branch(e,0,b); OCG_Duel er=make(true); auto rf=send(er,5,r); if(has(rf,MSG_SELECT_CHAIN)) rf=send(er,0xffffffffu,r); auto expected_other=branch(er,0,r); assert(b==r && other==expected_other);
	OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(er); OCG_DestroyDuel(e);
}
