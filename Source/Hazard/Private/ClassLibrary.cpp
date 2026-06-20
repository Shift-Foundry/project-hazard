#include "ClassLibrary.h"
#include "Engine/DataTable.h"

UDataTable* UClassLibrary::GetClassTable()
{
	return LoadObject<UDataTable>(nullptr, TEXT("/Game/Hazard/Data/DT_Classes.DT_Classes"));
}

void UClassLibrary::DefaultClasses(TArray<FName>& OutRows, TArray<FClassDef>& OutDefs)
{
	auto Add = [&](const TCHAR* Row, const TCHAR* Name, const TCHAR* Desc, const TCHAR* StartWpn,
		int32 Cost, float Dmg, float Hp, int32 Slots)
	{
		FClassDef D;
		D.DisplayName = Name;
		D.Description = Desc;
		D.StartWeaponRow = FName(StartWpn);
		D.CoinCost = Cost;
		D.DamageMult = Dmg;
		D.HealthBonus = Hp;
		D.WeaponSlots = Slots;
		OutRows.Add(FName(Row));
		OutDefs.Add(D);
	};
	//   Row          Name        Description                          Start    Cost  Dmg    +HP  Slots
	Add(TEXT("Recruit"),  TEXT("Recruit"),  TEXT("Free baseline. Balanced kit."),       TEXT("Pistol"),   0, 1.00f,   0, 2);
	Add(TEXT("Trooper"),  TEXT("Trooper"),  TEXT("Cheap. Sturdier, steadier damage."),  TEXT("Pistol"), 150, 1.05f,  50, 2);
	Add(TEXT("Operator"), TEXT("Operator"), TEXT("Three weapon slots; versatile."),     TEXT("MP5"),    600, 1.00f,  25, 3);
}

void UClassLibrary::GetAllClasses(TArray<FName>& OutRows, TArray<FClassDef>& OutDefs)
{
	OutRows.Reset();
	OutDefs.Reset();
	if (UDataTable* DT = GetClassTable())
	{
		for (const FName& RowName : DT->GetRowNames())
		{
			if (const FClassDef* Row = DT->FindRow<FClassDef>(RowName, TEXT("ClassLibrary")))
			{
				OutRows.Add(RowName);
				OutDefs.Add(*Row);
			}
		}
	}
	if (OutRows.Num() == 0)
	{
		DefaultClasses(OutRows, OutDefs);
	}
}

FClassDef UClassLibrary::FindClass(FName Row)
{
	TArray<FName> Rows;
	TArray<FClassDef> Defs;
	GetAllClasses(Rows, Defs);
	int32 Idx = Rows.IndexOfByKey(Row);
	if (Defs.IsValidIndex(Idx))
	{
		return Defs[Idx];
	}
	// Not found: prefer the free Recruit baseline (DT row order isn't guaranteed), else first/default.
	Idx = Rows.IndexOfByKey(FName("Recruit"));
	if (Defs.IsValidIndex(Idx))
	{
		return Defs[Idx];
	}
	return Defs.Num() > 0 ? Defs[0] : FClassDef();
}
