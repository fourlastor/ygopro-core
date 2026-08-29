#include <cassert>
#include <cstring>
#include <vector>
#include "ocgapi.h"
static void data(void*, uint32_t, OCG_CardData* d) { *d = {}; }
static int script(void*, OCG_Duel, const char*) { return 0; }
static OCG_Duel make() {
	OCG_DuelOptions o{}; o.seed[0]=1; o.team1={8000,0,0}; o.team2={8000,0,0}; o.cardReader=data; o.scriptReader=script;
	OCG_Duel d{}; assert(OCG_CreateDuel(&d,&o)==0); OCG_StartDuel(d);
	int s; for(int i=0;i<16 && (s=OCG_DuelProcess(d))==OCG_DUEL_STATUS_CONTINUE;++i) {} assert(s==OCG_DUEL_STATUS_AWAITING); return d;
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
