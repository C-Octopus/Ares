#include "Body.h"
#include "../TechnoType/Body.h"
#include "../../Misc/SWTypes.h"
#include "../../Misc/PoweredUnitClass.h"

#include <HouseClass.h>
#include <BuildingClass.h>
#include <GeneralStructures.h>
#include <Helpers/Template.h>

template<> const DWORD Extension<TechnoClass>::Canary = 0x55555555;
Container<TechnoExt> TechnoExt::ExtMap;

template<> TechnoExt::TT *Container<TechnoExt>::SavingObject = NULL;
template<> IStream *Container<TechnoExt>::SavingStream = NULL;

FireError::Value TechnoExt::FiringStateCache = FireError::NotAValue;

bool TechnoExt::NeedsRegap = false;

void TechnoExt::SpawnSurvivors(FootClass *pThis, TechnoClass *pKiller, bool Select, bool IgnoreDefenses)
{
	TechnoTypeClass *Type = pThis->GetTechnoType();

	HouseClass *pOwner = pThis->Owner;
	TechnoTypeExt::ExtData *pData = TechnoTypeExt::ExtMap.Find(Type);
	TechnoExt::ExtData *pSelfData = TechnoExt::ExtMap.Find(pThis);

	CoordStruct loc = pThis->Location;
	int chance = 0;

	// always eject passengers, but crew only if not already processed.
	if(!pSelfData->Survivors_Done && !pSelfData->DriverKilled && !IgnoreDefenses) {
		// save this, because the hijacker can kill people
		int PilotCount = pData->Survivors_PilotCount;

		// process the hijacker
		if(InfantryClass *Hijacker = RecoverHijacker(pThis)) {
			TechnoTypeExt::ExtData* pExt = TechnoTypeExt::ExtMap.Find(Hijacker->Type);

			if(!EjectRandomly(Hijacker, loc, 144, Select)) {
				Hijacker->RegisterDestruction(pKiller);
				GAME_DEALLOC(Hijacker);
			} else {
				// the hijacker will now be controlled instead of the unit
				if(TechnoClass* pController = pThis->MindControlledBy) {
					++Unsorted::IKnowWhatImDoing; // disables sound effects
					pController->CaptureManager->FreeUnit(pThis);
					pController->CaptureManager->CaptureUnit(Hijacker); // does the immunetopsionics check for us
					--Unsorted::IKnowWhatImDoing;
					Hijacker->QueueMission(mission_Guard, true); // override the fate the AI decided upon
					
				}
				VocClass::PlayAt(pExt->HijackerLeaveSound, &pThis->Location, 0);

				// lower than 0: kill all, otherwise, kill n pilots
				PilotCount = ((pExt->HijackerKillPilots < 0) ? 0 : (PilotCount - pExt->HijackerKillPilots));
			}
		}

		// possibly eject up to PilotChance crew members
		if(Type->Crewed && chance) {
			for(int i = 0; i < PilotCount; ++i) {
				if(ScenarioClass::Instance->Random.RandomRanged(1, 100) <= chance) {
					InfantryTypeClass *PilotType = NULL;
					signed int idx = pOwner->SideIndex;
					auto Pilots = &pData->Survivors_Pilots;
					if(Pilots->ValidIndex(idx)) {
						if(InfantryTypeClass *PilotType = Pilots->GetItem(idx)) {
							InfantryClass *Pilot = reinterpret_cast<InfantryClass *>(PilotType->CreateObject(pOwner));
							Pilot->Health = (PilotType->Strength / 2);
							Pilot->Veterancy.Veterancy = pThis->Veterancy.Veterancy;

							if(!EjectRandomly(Pilot, loc, 144, Select)) {
								Pilot->RegisterDestruction(pKiller); //(TechnoClass *)R->get_StackVar32(0x54));
								GAME_DEALLOC(Pilot);
							} else {
								if(pThis->AttachedTag && pThis->AttachedTag->IsTriggerRepeating()) {
									Pilot->ReplaceTag(pThis->AttachedTag);
								}
							}
						}
					}
				}
			}
		}
	}

	// passenger escape chances
	chance = pData->Survivors_PassengerChance.BindTo(pThis)->Get();

	// eject or kill all passengers. if defenses are to be ignored, passengers
	// killed no matter what the odds are.
	while(pThis->Passengers.FirstPassenger) {
		bool toDelete = 1;
		FootClass *passenger = pThis->RemoveFirstPassenger();
		bool toSpawn = false;
		if(chance > 0) {
			toSpawn = ScenarioClass::Instance->Random.RandomRanged(1, 100) <= chance;
		} else if(chance == -1 && pThis->WhatAmI() == abs_Unit) {
			int occupation = passenger->IsCellOccupied(pThis->GetCell(), -1, -1, 0, 1);
			toSpawn = (occupation == 0 || occupation == 2);
		}
		if(toSpawn && !IgnoreDefenses) {
			toDelete = !EjectRandomly(passenger, loc, 128, Select);
		}
		if(toDelete) {
			passenger->RegisterDestruction(pKiller); //(TechnoClass *)R->get_StackVar32(0x54));
			passenger->UnInit();
		}
	}

	// do not ever do this again for this unit
	pSelfData->Survivors_Done = 1;
}
/**
	\param Survivor Passenger to eject
	\param loc Where to put the passenger
	\param Select Whether to select the Passenger afterwards
*/
bool TechnoExt::EjectSurvivor(FootClass *Survivor, CoordStruct *loc, bool Select)
{
	bool success;
	CoordStruct tmpCoords;
	CellClass * pCell = MapClass::Instance->GetCellAt(loc);
	pCell->GetCoordsWithBridge(&tmpCoords);
	Survivor->OnBridge = pCell->ContainsBridge();

	int floorZ = tmpCoords.Z;
	if(loc->Z - floorZ > 208) {
		success = Survivor->SpawnParachuted(loc);
	} else {
		loc->Z = floorZ;
		success = Survivor->Put(loc, ScenarioClass::Instance->Random.RandomRanged(0, 7));
	}
	RET_UNLESS(success);
	Survivor->Transporter = NULL;
	Survivor->Scatter(0xB1CFE8, 1, 0);
	Survivor->QueueMission(Survivor->Owner->ControlledByHuman() ? mission_Guard : mission_Hunt, 0);
	if(Select) {
		Survivor->Select();
	}
	return 1;
	//! \todo Tag
}

/**
	This function ejects a given number of passengers from the passed transporter.

	\param pThis Pointer to the transporter
	\param howMany How many passengers to eject - pass negative number for "all"
	\author Renegade
	\date 27.05.2010
*/
void TechnoExt::EjectPassengers(FootClass *pThis, signed short howMany) {
	if(howMany == 0 || !pThis->Passengers.NumPassengers) {
		return;
	}

	short limit = (howMany < 0 || howMany > pThis->Passengers.NumPassengers) ? pThis->Passengers.NumPassengers : howMany;

	for(short i = 0; (i < limit) && pThis->Passengers.FirstPassenger; ++i) {
		FootClass *passenger = pThis->RemoveFirstPassenger();
		if(!EjectRandomly(passenger, pThis->Location, 128, false)) {
			passenger->UnInit();
		}
	}
	return;
}


/**
	This function drops the coordinates of an infantry subposition into the target parameter.
	Could probably work for vehicles as well, though they'd be off-center.

	\param current The current position of the transporter, the starting point to look from
	\param target A CoordStruct to save the finally computed position to
	\param distance The distance in leptons from the current position
	\author Renegade
	\date 27.05.2010
*/
void TechnoExt::GetPutLocation(CoordStruct const &current, CoordStruct &target, int distance) {
	// this whole thing does not at all account for cells which are completely occupied.
	CoordStruct tmpLoc = current;
	CellStruct tmpCoords = CellSpread::GetCell(ScenarioClass::Instance->Random.RandomRanged(0, 7));

	tmpLoc.X += tmpCoords.X * distance;
	tmpLoc.Y += tmpCoords.Y * distance;

	CellClass * tmpCell = MapClass::Instance->GetCellAt(&tmpLoc);

	tmpCell->FindInfantrySubposition(&target, &tmpLoc, 0, 0, 0);

	target.Z = current.Z;
	return;
}

//! Places a unit next to a given location on the battlefield.
/**
	
	\param pEjectee The FootClass to be ejected.
	\param location The current position of the transporter, the starting point to look from
	\param distance The distance in leptons from the current position
	\param select Whether the placed FootClass should be selected
	\author AlexB
	\date 12.04.2011
*/
bool TechnoExt::EjectRandomly(FootClass* pEjectee, CoordStruct const &location, int distance, bool select) {
	CoordStruct destLoc;
	GetPutLocation(location, destLoc, distance);
	return TechnoExt::EjectSurvivor(pEjectee, &destLoc, select);
}

//! Breaks the link between DrainTarget and DrainingMe.
/*!
	The links between the drainer and its victim are removed and the draining
	animation is no longer played.

	\param Drainer The Techno that drains power or credits.
	\param Drainee The Techno that power or credits get gets drained from.

	\author AlexB
	\date 2010-04-27
*/
void TechnoExt::StopDraining(TechnoClass *Drainer, TechnoClass *Drainee) {
	// fill the gaps
	if(Drainer && !Drainee)
		Drainee = Drainer->DrainTarget;

	if(!Drainer && Drainee)
		Drainer = Drainee->DrainingMe;

	// sanity check, then end draining.
	if(Drainer && Drainee) {
		// stop the animation.
		if (Drainer->DrainAnim) {
			Drainer->DrainAnim->UnInit();
			Drainer->DrainAnim = NULL;
		}

		// remove links.
		Drainee->DrainingMe = NULL;
		Drainer->DrainTarget = NULL;

		// tell the game to recheck the drained
		// player's tech level.
		if (Drainee->Owner) {
			Drainee->Owner->ShouldRecheckTechTree = true;
		}
	}
}

unsigned int TechnoExt::ExtData::AlphaFrame(SHPStruct *Image) {
	int countFrames = Conversions::Int2Highest(Image->Frames);
	DWORD Facing;
	this->AttachedToObject->Facing.GetFacing(&Facing);
	WORD F = (WORD)Facing;
	return (F >> (16 - countFrames));
}

bool TechnoExt::ExtData::DrawVisualFX() {
	TechnoClass * Object = this->AttachedToObject;
	if(Object->VisualCharacter(true, Object->Owner) == VisualType::Normal) {
		if(!Object->Disguised) {
			return true;
		}
	}
	return false;
}

UnitTypeClass * TechnoExt::ExtData::GetUnitType() {
	if(UnitClass * U = specific_cast<UnitClass *>(this->AttachedToObject)) {
		TechnoTypeExt::ExtData * pData = TechnoTypeExt::ExtMap.Find(U->Type);
		if(pData->WaterImage && !U->OnBridge && U->GetCell()->LandType == lt_Water) {
			return pData->WaterImage;
		}
	}
	return NULL;
}

void Container<TechnoExt>::InvalidatePointer(void *ptr) {
	AnnounceInvalidPointerMap(TechnoExt::AlphaExt, ptr);
	AnnounceInvalidPointerMap(TechnoExt::SpotlightExt, ptr);
	AnnounceInvalidPointer(TechnoExt::ActiveBuildingLight, ptr);
}

/*! This function checks if this object can currently be used, in terms of having or needing an operator.
	\return true if it needs an operator and has one, <b>or if it doesn't need an operator in the first place</b>. false if it needs an operator and doesn't have one.
	\author Renegade
	\date 27.04.10
*/
bool TechnoExt::ExtData::IsOperated() {
	TechnoTypeExt::ExtData* TypeExt = TechnoTypeExt::ExtMap.Find(this->AttachedToObject->GetTechnoType());

	if(TypeExt->Operator) {
		if(this->AttachedToObject->Passengers.NumPassengers) {
			// loop & condition come from D
			for(ObjectClass* O = this->AttachedToObject->Passengers.GetFirstPassenger(); O; O = O->NextObject) {
				if(FootClass *F = generic_cast<FootClass *>(O)) {
					if(F->GetType() == TypeExt->Operator) {
						// takes a specific operator and someone is present AND that someone is the operator, therefore it is operated
						return true;
					}
				}
			}
			// takes a specific operator and someone is present, but it's not the operator, therefore it's not operated
			return false;
		} else {
			// takes a specific operator but no one is present, therefore it's not operated
			return false;
		}
	} else if(TypeExt->IsAPromiscuousWhoreAndLetsAnyoneRideIt) {
		// takes anyone, therefore it's operated if anyone is there
		return (this->AttachedToObject->Passengers.NumPassengers > 0);
	} else {
		/* Isn't even set as an Operator-using object, therefore we are returning TRUE,
		 since, logically, if it doesn't need operators, it can be/is operated, no matter if there are passengers or not.
		 (Also, if we didn't do this, Reactivate() would fail for for non-Operator-units, for example.) */
		return true;
	}
}

/*! This function checks if this object can currently be used, in terms of having or needing a powering structure and that structure's status.
	\return true if it needs a structure and has an active one, <b>or if it doesn't need a structure in the first place</b>. false if it needs a structure and doesn't have an active one.
	\author Renegade
	\date 27.04.10
*/
bool TechnoExt::ExtData::IsPowered() {
	TechnoTypeClass *TT = this->AttachedToObject->GetTechnoType();
	if(TT && TT->PoweredUnit) {
		HouseClass* Owner = this->AttachedToObject->Owner;
		for(int i = 0; i < Owner->Buildings.Count; ++i) {
			BuildingClass* Building = Owner->Buildings.GetItem(i);
			if(Building->Type->PowersUnit) {
				if(Building->Type->PowersUnit == TT) {
					return Building->RegisteredAsPoweredUnitSource && !Building->IsUnderEMP(); // alternatively, HasPower, IsPowerOnline()
				}
			}
		}
		// if we reach this, we found no building that currently powers this object
		return false;
	// #617
	} else if(this->PoweredUnit) {
		return this->PoweredUnit->IsPowered();
	} else {
		// object doesn't need a particular powering structure, therefore, for the purposes of the game, it IS powered
		return true;
	}
}

/*
 * Object should NOT be placed on the map (->Remove() it or don't Put in the first place)
 * otherwise Bad Things (TM) will happen. Again.
 */
bool TechnoExt::CreateWithDroppod(FootClass *Object, CoordStruct *XYZ) {
	auto MyCell = MapClass::Instance->GetCellAt(XYZ);
	if(Object->IsCellOccupied(MyCell, -1, -1, 0, 0)) {
//		Debug::Log("Cell occupied... poof!\n");
		return false;
	} else {
//		Debug::Log("Destinating %s @ {%d, %d, %d}\n", Object->GetType()->ID, XYZ->X, XYZ->Y, XYZ->Z);
		LocomotionClass::ChangeLocomotorTo(Object, &LocomotionClass::CLSIDs::Droppod);
		CoordStruct xyz = *XYZ;
		xyz.Z = 0;
		Object->SetLocation(&xyz);
		Object->SetDestination(MyCell, 1);
		Object->Locomotor->Move_To(*XYZ);
		FacingStruct::Facet Facing = {0, 0, 0};
		Object->Facing.SetFacing(&Facing);
		if(!Object->InLimbo) {
			Object->See(0, 0);
			Object->QueueMission(mission_Guard, 0);
			Object->NextMission();
			return true;
		}
		//Debug::Log("InLimbo... failed?\n");
		return false;
	}
}

// If available, creates an InfantryClass instance and removes the hijacker from the victim.
InfantryClass* TechnoExt::RecoverHijacker(FootClass* pThis) {
	InfantryClass* Hijacker = NULL;
	if(pThis && pThis->HijackerInfantryType != -1) {
		if(InfantryTypeClass* HijackerType = InfantryTypeClass::Array->GetItem(pThis->HijackerInfantryType)) {
			TechnoExt::ExtData* pExt = TechnoExt::ExtMap.Find(pThis);
			TechnoTypeExt::ExtData* pTypeExt = TechnoTypeExt::ExtMap.Find(HijackerType);
			HouseClass* HijackerOwner = pExt->HijackerHouse ? pExt->HijackerHouse : pThis->Owner;
			if(!pTypeExt->HijackerOneTime && HijackerOwner && !HijackerOwner->Defeated) {
				Hijacker = reinterpret_cast<InfantryClass *>(HijackerType->CreateObject(HijackerOwner));
				Hijacker->Health = std::max(pExt->HijackerHealth / 2, 5);
			}
			pThis->HijackerInfantryType = -1;
			pExt->HijackerHealth = -1;
		}
	}
	return Hijacker;
}

// this isn't called VehicleThief action, because it also includes other logic
// related to infantry getting into an vehicle like CanDrive.
AresAction::Value TechnoExt::ExtData::GetActionHijack(TechnoClass* pTarget) {
	InfantryClass* pThis = specific_cast<InfantryClass*>(this->AttachedToObject);
	if(!pThis || !pTarget || !pThis->IsAlive || !pTarget->IsAlive) {
		return AresAction::None;
	}

	InfantryTypeClass* pType = pThis->Type;
	TechnoTypeClass* pTargetType = pTarget->GetTechnoType();
	TechnoTypeExt::ExtData* pTypeExt = TechnoTypeExt::ExtMap.Find(pType);

	// this can't steal vehicles
	if(!pType->VehicleThief && !pTypeExt->CanDrive) {
		return AresAction::None;
	}

	// I'm in a state that forbids capturing
	if(!this->IsOperated()) {
		return AresAction::None;
	}
	if(pType->Deployer) {
		eSequence sequence = pThis->SequenceAnim;
		if(sequence == seq_Deploy || sequence == seq_Deployed
			|| sequence == seq_DeployedFire || sequence == seq_DeployedIdle) {
				return AresAction::None;
		}
	}

	// target type is not eligible (hijackers can also enter strange buildings)
	eAbstractType absTarget = pTarget->WhatAmI();
	if(absTarget != abs_Aircraft && absTarget != abs_Unit
		&& (!pType->VehicleThief || absTarget != abs_Building)) {
			return AresAction::None;
	}

	// target is bad
	if(pTarget->CurrentMission == mission_Selling || pTarget->IsBeingWarpedOut()
		|| pTargetType->IsTrain || !pTarget->IsStrange() || pTargetType->BalloonHover
		//|| (absTarget == abs_Unit && ((UnitTypeClass*)pTargetType)->NonVehicle) replaced by Hijacker.Allowed
		|| !pTarget->IsOnFloor()) {
			return AresAction::None;
	}

	// a thief that can't break mind control loses without trying further
	if(pType->VehicleThief) { 
		if(pTarget->IsMindControlled() && !pTypeExt->HijackerBreakMindControl) {
			return AresAction::None;
		}
	}

	 //drivers can drive, but only stuff owned by Special. if a driver is a vehicle thief
	 //also, it can reclaim units even if they are immune to hijacking (see below)
	bool specialOwned = !_strcmpi(pTarget->Owner->Type->ID, "Special");
	if(specialOwned && pTypeExt->CanDrive) {
		return AresAction::Drive;
	}

	// hijacking only affects enemies
	if(pType->VehicleThief) {
		// can't steal allied unit (CanDrive and special already handled)
		if(pThis->Owner->IsAlliedWith(pTarget->Owner) || specialOwned) {
			return AresAction::None;
		}

		TechnoTypeExt::ExtData* pTargetTypeExt = TechnoTypeExt::ExtMap.Find(pTargetType);
		if(!pTargetTypeExt->HijackerAllowed) {
			return AresAction::None;
		}

		// allowed to steal from enemy
		return AresAction::Hijack;
	}

	// no hijacking ability
	return AresAction::None;
}

// =============================
// load/save

void Container<TechnoExt>::Load(TechnoClass *pThis, IStream *pStm) {
	TechnoExt::ExtData* pData = this->LoadKey(pThis, pStm);

	SWIZZLE(pData->Insignia_Image);
}

// =============================
// container hooks

DEFINE_HOOK(6F3260, TechnoClass_CTOR, 5)
{
	GET(TechnoClass*, pItem, ESI);

	TechnoExt::ExtMap.FindOrAllocate(pItem);
	return 0;
}

DEFINE_HOOK(6F4500, TechnoClass_DTOR, 5)
{
	GET(TechnoClass*, pItem, ECX);

	SWStateMachine::InvalidatePointer(pItem);
	TechnoExt::ExtMap.Remove(pItem);
	return 0;
}

DEFINE_HOOK(70BF50, TechnoClass_SaveLoad_Prefix, 5)
DEFINE_HOOK_AGAIN(70C250, TechnoClass_SaveLoad_Prefix, 8)
{
	GET_STACK(TechnoExt::TT*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);

	Container<TechnoExt>::SavingObject = pItem;
	Container<TechnoExt>::SavingStream = pStm;

	return 0;
}

DEFINE_HOOK(70C249, TechnoClass_Load_Suffix, 5)
{
	TechnoExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(70C264, TechnoClass_Save_Suffix, 5)
{
	TechnoExt::ExtMap.SaveStatic();
	return 0;
}
