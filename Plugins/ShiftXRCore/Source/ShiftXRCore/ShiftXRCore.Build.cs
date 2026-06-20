using System.IO;
using UnrealBuildTool;

public class ShiftXRCore : ModuleRules
{
	public ShiftXRCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Android XR manifest injection (uses-feature openxr + hand-tracking permission).
		// UPL only applies to the Android cook; Win64 editor builds ignore it.
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "ShiftXRCore_UPL_Android.xml"));
		}

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HeadMountedDisplay"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Android XR / OpenXR scene-understanding & anchor backends are compiled behind this
		// gate. Defined as 0 by default (stock UE 5.8 has no scene/anchor runtime); flip to 1
		// when a 5.8-compatible Android XR backend is present (see ROADMAP.md "Engine-gap strategy").
		// PublicDefinitions so public dependents (the Hazard module) see the same value.
		PublicDefinitions.Add("WITH_ANDROIDXR=0");
	}
}
