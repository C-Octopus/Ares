#ifndef SUPERTYPE_EXT_EMPULSE_H
#define SUPERTYPE_EXT_EMPULSE_H

#include "../SWTypes.h"

class SW_EMPulse : public NewSWType
{
public:
	SW_EMPulse() : NewSWType()
		{ };

	virtual const char* GetTypeString() const override
	{
		return "EMPulse";
	}

	virtual void LoadFromINI(SWTypeExt::ExtData *pData, SuperWeaponTypeClass *pSW, CCINIClass *pINI) override;
	virtual void Initialize(SWTypeExt::ExtData *pData, SuperWeaponTypeClass *pSW) override;
	virtual bool Activate(SuperClass* pThis, const CellStruct &Coords, bool IsPlayer) override;

private:
	virtual bool IsLaunchSite(SWTypeExt::ExtData* pSWType, BuildingClass* pBuilding) const override;
	virtual std::pair<double, double> GetLaunchSiteRange(SWTypeExt::ExtData* pSWType, BuildingClass* pBuilding) const override;
};

#endif