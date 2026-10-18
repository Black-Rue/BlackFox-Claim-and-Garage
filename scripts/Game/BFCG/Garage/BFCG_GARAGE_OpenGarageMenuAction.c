/*
 * Local terminal action that supplies the replicated terminal ID before opening the menu.
 * Based on GTG_Garage by Maj0rrush / GTG-Team; modified by BlackFox.
 * Original licence: Arma Public License (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/1CF644CD7D02B7F2
 */

class BFCG_GARAGE_OpenGarageMenuAction : ScriptedUserAction
{

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	// Use the terminal RplComponent ID so the server can resolve and validate the selected terminal.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{

		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		RplComponent rplComp = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (!rplComp)
		{
			Print("[BFCG_GARAGE_OpenGarageMenuAction] No RplComponent on garage entity - menu will not open", LogLevel.ERROR);
			return;
		}

		RplId garageId = rplComp.Id();
		if (!garageId.IsValid())
		{
			Print("[BFCG_GARAGE_OpenGarageMenuAction] RplComponent.Id() invalid - menu will not open", LogLevel.ERROR);
			return;
		}

		BFCG_GARAGE_GarageMenuUI.SetGarageContext(garageId);
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.BFCG_GarageMenu);
	}

	override bool CanBeShownScript(IEntity user)
	{
		return BFCG_GARAGE_PlayerHelper.IsUserNearOwner(user, GetOwner());
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Open Garage";
		return true;
	}
}
