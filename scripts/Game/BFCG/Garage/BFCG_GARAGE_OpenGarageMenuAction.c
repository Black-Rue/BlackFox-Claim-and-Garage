//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_OpenGarageMenuAction.c
//! BFCG_Garage - UserAction "Open Garage": открывает меню гаража (fullscreen UI)
//!
//! Based on GTG_Garage by Maj0rrush / GTG-Team (Arma Public License) - modified.
//! Чисто клиентское действие (HasLocalEffectOnlyScript=true).
//------------------------------------------------------------------------------------------------

class BFCG_GARAGE_OpenGarageMenuAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Меню открываем только на клиенте выполнившего игрока
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		// Контекст гаража для меню: Id RplComponent пульта
		// (именно rplComp.Id(), не Replication.FindId(entity))
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

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return BFCG_GARAGE_PlayerHelper.IsUserNearOwner(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Open Garage";
		return true;
	}
}
