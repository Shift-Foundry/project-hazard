#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HazardTypes.h"
#include "ClassLibrary.generated.h"

class UDataTable;

/** Player-class definitions from DT_Classes (with built-in fallbacks). Classes are unlocked with
 *  banked coins in the lobby and applied at run start (slots / starting weapon / stat multipliers). */
UCLASS()
class HAZARD_API UClassLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** All class rows (DT_Classes if present, else built-in defaults), as parallel arrays. */
	static void GetAllClasses(TArray<FName>& OutRows, TArray<FClassDef>& OutDefs);

	/** Resolve a class by row name; returns the free Recruit baseline if not found. */
	static FClassDef FindClass(FName Row);

private:
	static UDataTable* GetClassTable();
	static void DefaultClasses(TArray<FName>& OutRows, TArray<FClassDef>& OutDefs);
};
