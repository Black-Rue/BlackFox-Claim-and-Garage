/*
 * Vehicle actions and access overrides adapted and rewritten from CarKey2 by NewbKodii (APL).
 * Workshop: https://reforger.armaplatform.com/workshop/66D61CE15CC684A6
 * BlackFox replaces physical key/code checks with persistent owner identity checks.
 */

class BFCG_VehicleClaimAccess
{
	static VehicleControllerComponent FindState(IEntity entity)
	{
		if (!entity)
			return null;

		IEntity root = entity.GetRootParent();
		if (!root)
			root = entity;

		return VehicleControllerComponent.Cast(root.FindComponent(VehicleControllerComponent));
	}

	static bool HasClaim(IEntity entity)
	{
		return BFCG_VehicleClaimState.HasClaim(FindState(entity));
	}

	static bool IsDenied(IEntity entity, IEntity user)
	{
		return BFCG_VehicleClaimState.IsAccessDenied(FindState(entity), user);
	}

	// Remaining-time formatting is adapted from CarKey2 Key_DecayTime; expiry comes from server state.
	static string GetToggleName(VehicleControllerComponent state)
	{
		string operation = "Lock Vehicle";
		if (BFCG_VehicleClaimState.IsLocked(state))
			operation = "Unlock Vehicle";

		int remainingTime = BFCG_VehicleClaimState.GetExpiresAt(state) - System.GetUnixTime();
		if (remainingTime <= 0)
			return operation + " | Lock Decay: Expired";

		int days;
		int hours;
		int minutes;
		int seconds;
		SCR_DateTimeHelper.GetDayHourMinuteSecondFromSeconds(remainingTime, days, hours, minutes, seconds);

		string decay = string.Format("Lock Decay: %1 Seconds", remainingTime);
		if (minutes > 0)
			decay = string.Format("Lock Decay: %1 Mins", minutes);
		if (hours > 0)
			decay = string.Format("Lock Decay: %1 Hours %2 Mins", hours, minutes);
		if (days > 0)
			decay = string.Format("Lock Decay: %1 Days %2 Hours", days, hours);

		return operation + " | " + decay;
	}
}

class BFCG_ToggleVehicleLockUserAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		VehicleControllerComponent state = BFCG_VehicleClaimAccess.FindState(pOwnerEntity);
		if (!state)
			return;

		if (BFCG_VehicleClaimState.HasClaim(state))
			BFCG_VehicleClaimState.Toggle(state, pUserEntity);
		else
			BFCG_VehicleClaimState.TryClaim(state, pUserEntity);
	}

	override bool CanBeShownScript(IEntity user)
	{
		VehicleControllerComponent state = BFCG_VehicleClaimAccess.FindState(GetOwner());
		if (!state || BFCG_ExcludedVehicleComponent.IsExcluded(state.GetOwner()))
			return false;

		if (!BFCG_VehicleClaimState.HasClaim(state))
			return true;

		return BFCG_VehicleClaimState.IsOwner(state, user);
	}

	override bool GetActionNameScript(out string outName)
	{
		VehicleControllerComponent state = BFCG_VehicleClaimAccess.FindState(GetOwner());
		if (!state || BFCG_ExcludedVehicleComponent.IsExcluded(state.GetOwner()))
			return false;
		if (!BFCG_VehicleClaimState.HasClaim(state))
		{
			outName = "Claim Vehicle";
			return true;
		}

		outName = BFCG_VehicleClaimAccess.GetToggleName(state);

		return true;
	}
}

modded class SCR_BaseLockComponent
{
	override bool IsLocked(IEntity user, BaseCompartmentSlot compartmentSlot)
	{
		VehicleControllerComponent state = BFCG_VehicleClaimAccess.FindState(GetOwner());
		if (BFCG_VehicleClaimState.HasClaim(state))
			return BFCG_VehicleClaimState.IsAccessDenied(state, user);

		return super.IsLocked(user, compartmentSlot);
	}
}

modded class SCR_GetInUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

modded class SCR_TurretGetInUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

modded class SCR_CompartmentUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

modded class SCR_OpenVehicleDoorUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

modded class SCR_OpenVehicleStorageAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

modded class SCR_PushVehicleAction
{
	override bool CanBeShownScript(IEntity user)
	{
		if (BFCG_VehicleClaimAccess.HasClaim(GetOwner()) && BFCG_VehicleClaimAccess.IsDenied(GetOwner(), user))
			return false;

		return super.CanBeShownScript(user);
	}
}

// Append to native vehicle actions so the proxy needs no competing interaction collider.
modded class SCR_InteractionHandlerComponent
{
	override void AggregateActions(array<BaseUserAction> actionsList, array<bool> canPerformList)
	{
		super.AggregateActions(actionsList, canPerformList);

		if (actionsList.IsEmpty() || !m_ControlledEntity)
			return;

		VehicleControllerComponent vehicle = BFCG_VehicleClaimAccess.FindState(actionsList[0].GetOwner());
		if (!vehicle)
			return;

		BFCG_ToggleVehicleLockUserAction claimAction = vehicle.BFCG_GetClaimAction();
		if (!claimAction || actionsList.Contains(claimAction))
			return;

		claimAction.SetActiveContext(GetCurrentContext());
		if (!claimAction.CanBeShown(m_ControlledEntity))
			return;

		actionsList.Insert(claimAction);
		canPerformList.Insert(claimAction.CanBePerformed(m_ControlledEntity));
	}
}
