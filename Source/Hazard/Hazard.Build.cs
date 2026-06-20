using UnrealBuildTool;

public class Hazard : ModuleRules
{
	public Hazard(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			"ShiftXRCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
