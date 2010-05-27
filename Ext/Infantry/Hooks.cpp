#include <InfantryClass.h>
#include <BuildingClass.h>
#include <SpecificStructures.h>
#include "../Building/Body.h"
#include "../BuildingType/Body.h"
#include "../Techno/Body.h"
#include "Body.h"
#include "../Rules/Body.h"
#include <GameModeOptionsClass.h>
#include <Misc/Actions.h>

// #664: Advanced Rubble - reconstruction part: Check
/*
A_FINE_HOOK(51E635, InfantryClass_GetCursorOverObject_EngineerOverFriendlyBuilding, 5)
{
	GET(BuildingClass *, Target, ESI);
	GET(InfantryClass *, pThis, EDI);
	FPUControl fp(R->get_EAX()); // (Target->GetHealthPercentage() == RulesClass::Instance->ConditionIdeal)
	switch(control) {
		case Rebuild:
			// fall through - not decided about UI handling yet
		case DecideNormally:
		default:
		return fp.isEqual()
		 ? 0x51E63A
		 : 0x51E659
		;
	}
}
*/

// #664: Advanced Rubble - reconstruction part: Reconstruction
DEFINE_HOOK(519FAF, InfantryClass_UpdatePosition_EngineerRepairsFriendly, 6)
{
	GET(InfantryClass *, pThis, ESI);
	GET(BuildingClass *, Target, EDI);

	BuildingExt::ExtData* TargetExtData = BuildingExt::ExtMap.Find(Target);
	BuildingTypeExt::ExtData* TargetTypeExtData = BuildingTypeExt::ExtMap.Find(Target->Type);
	bool do_normal_repair = true;

	if(TargetTypeExtData->RubbleIntact) {
		do_normal_repair = false;
		bool wasSelected = pThis->IsSelected;
		pThis->Remove();
		TargetExtData->RubbleYell(true);
		CellStruct Cell;
		pThis->GetMapCoords(&Cell);
		Target->KickOutUnit(pThis, &Cell);
		if(wasSelected) {
			pThis->Select();
		}
	}

	return do_normal_repair ? 0 : 0x51A65D; //0x51A010 eats the Engineer, 0x51A65D hopefully does not
}

DEFINE_HOOK(51DF38, InfantryClass_Remove, A)
{
	GET(InfantryClass *, pThis, ESI);
	TechnoExt::ExtData* pData = TechnoExt::ExtMap.Find(pThis);

	if(BuildingClass *Garrison = pData->GarrisonedIn) {
		signed int idx = Garrison->Occupants.FindItemIndex(&pThis);
		if(idx == -1) {
			Debug::Log("Infantry %s was garrisoned in building %s, but building didn't find it. WTF?", pThis->Type->ID, Garrison->Type->ID);
		} else {
			Garrison->Occupants.RemoveItem(idx);
		}
	}

	pData->GarrisonedIn = NULL;

	return 0;
}

DEFINE_HOOK(51DFFD, InfantryClass_Put, 5)
{
	GET(InfantryClass *, pThis, EDI);
	TechnoExt::ExtData* pData = TechnoExt::ExtMap.Find(pThis);
	pData->GarrisonedIn = NULL;

	return 0;
}

DEFINE_HOOK(518434, InfantryClass_ReceiveDamage_SkipDeathAnim, 7)
{
	GET(InfantryClass *, pThis, ESI);
	GET_STACK(ObjectClass *, pAttacker, 0xE0);
//	InfantryExt::ExtData* trooperAres = InfantryExt::ExtMap.Find(pThis);
//	bool skipInfDeathAnim = false; // leaving this default in case this is expanded in the future

	// there is not InfantryExt ExtMap yet!
	// too much space would get wasted since there is only four bytes worth of data we need to store per object
	// so those four bytes get stashed in Techno Map instead. they will get their own map if there's ever enough data to warrant it
	TechnoExt::ExtData* pData = TechnoExt::ExtMap.Find(pThis);

	return pData->GarrisonedIn ? 0x5185F1 : 0;
}

// should correct issue #743
DEFINE_HOOK(51D799, InfantryClass_PlayAnim_WaterSound, 7)
{
	GET(InfantryClass *, I, ESI);
	return (I->Transporter || I->Type->MovementZone != mz_AmphibiousDestroyer)
		? 0x51D8BF
		: 0x51D7A6
	;
}

DEFINE_HOOK(51E5C0, InfantryClass_GetCursorOverObject_MultiEngineerA, 6) {
	// skip old logic's way to determine the cursor
	return 0x51E5D9;
}

DEFINE_HOOK(51E5E1, InfantryClass_GetCursorOverObject_MultiEngineerB, 7) {
	eAction ret = act_Capture;

	// damage if multi engineer and target isn't that low on health
	if(GameModeOptionsClass::Instance->MultiEngineer) {
		GET(TechnoClass *, T, ECX);
		Debug::Log("InfantryClass_GetCursorOverObject_MultiEngineerB: %f %f %d\n", T->GetHealthPercentage(), RulesClass::Global()->EngineerCaptureLevel, T->GetHealthPercentage() >= RulesClass::Global()->EngineerCaptureLevel);
		if(T->GetHealthPercentage() > RulesClass::Global()->EngineerCaptureLevel) {
			ret = (RulesExt::Global()->EngineerDamage > 0 ? act_Damage : act_NoEnter);
			if(ret == act_Damage) {
				Actions::Set(RulesExt::Global()->EngineerDamageCursor);
			}
		}
	}

	//// return our action
	R->EAX(ret);
	return 0;
}

DEFINE_HOOK(519DB6, InfantryClass_UpdatePosition_MultiEngineer, 7) {
	GET(InfantryClass *, pEngi, ESI);
	GET(TechnoClass *, pTarget, EDI);
	if(pTarget->GetHealthPercentage() > RulesClass::Global()->EngineerCaptureLevel) {
		// damage
		int Damage = ceil(pTarget->GetTechnoType()->Strength * RulesExt::Global()->EngineerDamage);
		pTarget->ReceiveDamage(&Damage, 0, RulesClass::Global()->C4Warhead, pEngi, 1, 0, 0);
		return 0x51A010;
	} else {
		// capture
		return 0x519EAA;
	}
}