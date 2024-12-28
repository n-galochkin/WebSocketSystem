using UnrealBuildTool;

public class BFHubSockets : ModuleRules
{
	public BFHubSockets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"WebSockets",
				"Json",
				"JsonUtilities",
				"BFCoreTasks", 
				"BFHubServices",
				//TODO: Remove this dependency when implemented socket for client
				// used only for getting settings from config file
				"HelpersPlugin",
				"DeveloperSettings",
				"BFHttpModule", 
				"GameplayMessageRuntime", "BFCoreTags"
			}
		);
	}
}