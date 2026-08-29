#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <chrono>
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
 local g=Duel.SelectMatchingCard(tp,Card.IsAbleToHand,tp,0x01,0,1,1,nil)
 Duel.SendtoHand(g,nil,0x40)
end)";
static const char chain_script[] = R"(local s=c102
function s.initial_effect(c)
 local e=Effect.CreateEffect(c)
 e:SetType(0x100)
 e:SetCode(1027)
 e:SetRange(0x04)
 e:SetCondition(function() return true end)
 e:SetOperation(function() end)
 c:RegisterEffect(e)
end)";
static int script_requests{};
static int script(void*, OCG_Duel d, const char* name) {
 if(std::strcmp(name,"c100.lua")==0) { ++script_requests; return OCG_LoadScript(d,effect_script,sizeof(effect_script)-1,name); }
	if(std::strcmp(name,"c102.lua")==0) return OCG_LoadScript(d,chain_script,sizeof(chain_script)-1,name);
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
	deck.code=103; deck.seq=1; OCG_DuelNewCard(d,&deck);
	OCG_NewCardInfo chain{}; chain.team=1; chain.code=102; chain.con=1; chain.loc=LOCATION_MZONE; chain.pos=POS_FACEUP_ATTACK;
	OCG_DuelNewCard(d,&chain);
}
static std::vector<uint8_t> branch(OCG_Duel d, uint8_t response, int& status) {
	OCG_DuelSetResponse(d,&response,1); status=OCG_DuelProcess(d); uint32_t n{}; auto* p=(uint8_t*)OCG_DuelGetMessage(d,&n); return {p,p+n};
}
static std::vector<uint8_t> select_card(OCG_Duel d, uint32_t index, int& status) { uint32_t r[3]={0,1,index}; OCG_DuelSetResponse(d,r,sizeof(r)); status=OCG_DuelProcess(d); uint32_t n{}; auto*p=(uint8_t*)OCG_DuelGetMessage(d,&n); return {p,p+n}; }
static bool has(const std::vector<uint8_t>& b, uint8_t type) { for(size_t i=0;i+5<=b.size();) { uint32_t n; std::memcpy(&n,&b[i],4); if(n && i+4+n<=b.size() && b[i+4]==type) return true; i+=4+n; } return false; }
static void dump(const std::vector<uint8_t>& b) { for(size_t i=0;i+5<=b.size();) { uint32_t n; std::memcpy(&n,&b[i],4); std::fprintf(stderr,"frame type=%u len=%u\\n",b[i+4],n); i+=4+n; } }
static std::vector<uint8_t> send(OCG_Duel d, uint32_t v, int& status) { OCG_DuelSetResponse(d,&v,4); std::vector<uint8_t> out; do { status=OCG_DuelProcess(d); uint32_t n{}; auto*p=(uint8_t*)OCG_DuelGetMessage(d,&n); out.insert(out.end(),p,p+n); } while(status==OCG_DUEL_STATUS_CONTINUE); return out; }
int main() {
	OCG_Duel d=make(); OCG_DuelSnapshot s{}; assert(OCG_DuelCreateSnapshot(d,&s)==0);
	OCG_Duel foreign=make(); assert(OCG_DuelRestoreSnapshot(foreign,s)==OCG_DUEL_SNAPSHOT_INVALID_OWNER); int foreign_status{}; auto foreign_output=branch(foreign,7,foreign_status); assert(!foreign_output.empty()); OCG_DestroyDuel(foreign);
	int a{},b{},r{}; auto first=branch(d,7,a); assert(OCG_DuelRestoreSnapshot(d,s)==0); auto same=branch(d,7,b); assert(a==b && first==same);
	assert(OCG_DuelRestoreSnapshot(d,s)==0); auto diverged=branch(d,6,b); OCG_Duel ref=make(); auto expected=branch(ref,6,r); assert(b==r && diverged==expected);
	OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(ref); OCG_DestroyDuel(d);
	OCG_Duel chain=make(true); auto cp=send(chain,5,a); assert(has(cp,MSG_SELECT_CHAIN)); assert(OCG_DuelCreateSnapshot(chain,&s)==0); auto pass=send(chain,0xffffffffu,a); assert(OCG_DuelRestoreSnapshot(chain,s)==0); auto pass2=send(chain,0xffffffffu,b); assert(a==b && pass==pass2); assert(OCG_DuelRestoreSnapshot(chain,s)==0); auto activate=send(chain,0,b); OCG_Duel chain_ref=make(true); auto cref=send(chain_ref,5,r); assert(has(cref,MSG_SELECT_CHAIN)); auto activate_ref=send(chain_ref,0,r); assert(b==r && activate==activate_ref); OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(chain_ref); OCG_DestroyDuel(chain);
	OCG_Duel e=make(true); assert(script_requests); auto f=send(e,5,a); for(int i=0;i<4 && has(f,MSG_SELECT_CHAIN);++i) f=send(e,0xffffffffu,a); if(!has(f,MSG_SELECT_CARD)) dump(f); assert(has(f,MSG_SELECT_CARD)); assert(OCG_DuelCreateSnapshot(e,&s)==0);
	auto chosen=select_card(e,0,a); assert(OCG_DuelRestoreSnapshot(e,s)==0); auto repeated=select_card(e,0,b); assert(a==b && chosen==repeated);
	assert(OCG_DuelRestoreSnapshot(e,s)==0); auto other=select_card(e,1,b); OCG_Duel er=make(true); auto rf=send(er,5,r); for(int i=0;i<4&&has(rf,MSG_SELECT_CHAIN);++i) rf=send(er,0xffffffffu,r); auto expected_other=select_card(er,1,r); assert(chosen!=other && b==r && other==expected_other);
	OCG_DuelDestroySnapshot(s); OCG_DestroyDuel(er); OCG_DestroyDuel(e);
	constexpr int N=32; auto bench=[](bool effect) { OCG_Duel d=make(effect); int z{}; if(effect) { auto q=send(d,5,z); for(int i=0;i<4&&has(q,MSG_SELECT_CHAIN);++i) q=send(d,0xffffffffu,z); } OCG_DuelSnapshot x{}; auto t0=std::chrono::steady_clock::now(); for(int i=0;i<N;++i) { assert(OCG_DuelCreateSnapshot(d,&x)==0); OCG_DuelDestroySnapshot(x); } auto t1=std::chrono::steady_clock::now(); assert(OCG_DuelCreateSnapshot(d,&x)==0); auto bytes=OCG_DuelSnapshotSize(x), used=OCG_DuelArenaUsed(d); for(int i=0;i<N;++i) assert(OCG_DuelRestoreSnapshot(d,x)==0); auto t2=std::chrono::steady_clock::now(); OCG_DuelDestroySnapshot(x); auto t3=std::chrono::steady_clock::now(); for(int i=0;i<N;++i) { OCG_Duel r=make(effect); if(effect) { auto q=send(r,5,z); for(int j=0;j<4&&has(q,MSG_SELECT_CHAIN);++j) q=send(r,0xffffffffu,z); assert(has(q,MSG_SELECT_CARD)); } OCG_DestroyDuel(r); } auto t4=std::chrono::steady_clock::now(); std::printf("bench %s n=%d used=%llu snapshot=%llu create_us=%.1f restore_us=%.1f fresh_us=%.1f\n",effect?"selection":"idle",N,(unsigned long long)used,(unsigned long long)bytes,std::chrono::duration<double,std::micro>(t1-t0).count()/N,std::chrono::duration<double,std::micro>(t2-t1).count()/N,std::chrono::duration<double,std::micro>(t4-t3).count()/N); OCG_DestroyDuel(d); };
	bench(false); bench(true);
}
