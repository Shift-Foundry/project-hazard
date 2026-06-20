using UnrealBuildTool;
using System.Collections.Generic;

public class HazardEditorTarget : TargetRules
{
	public HazardEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Hazard");
	}
}
