/*
 * Serializer pattern adapted from CarKey2 by NewbKodii (Arma Public License).
 * Workshop: https://reforger.armaplatform.com/workshop/66D61CE15CC684A6
 * Rewritten by BlackFox for claim fields on the native VehicleControllerComponent.
 */

class BFCG_VehicleClaimComponentSerializer : ScriptedComponentSerializer
{
	override static typename GetTargetType()
	{
		return VehicleControllerComponent;
	}

	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		VehicleControllerComponent claim = VehicleControllerComponent.Cast(component);
		if (!claim)
			return ESerializeResult.ERROR;

		// A save must not miss destruction if a third-party damage event bypassed our callback.
		// Do not request another Save() from inside serialization.
		if (Replication.IsServer() && BFCG_Settings.IsValid() && claim.BFCG_IsDestroyed())
		{
			if (claim.BFCG_GetWreckDeleteAt() == 0)
				claim.BFCG_RestoreWreckDeleteAt(System.GetUnixTime() + BFCG_Settings.Get().destroyedVehicleLifetimeSeconds);
			BFCG_VehicleClaimState.ReleaseDestroyed(claim);
		}

		context.WriteValue("version", 2);
		context.WriteValue("ownerIdentity", claim.BFCG_GetClaimOwnerIdentity());
		context.WriteValue("locked", claim.BFCG_IsClaimLocked());
		context.WriteValue("expiresAt", claim.BFCG_GetClaimExpiresAt());
		context.WriteValue("wreckDeleteAt", claim.BFCG_GetWreckDeleteAt());
		return ESerializeResult.OK;
	}

	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		VehicleControllerComponent claim = VehicleControllerComponent.Cast(component);
		if (!claim)
			return false;

		int version;
		string ownerIdentity;
		bool locked;
		int expiresAt;
		int wreckDeleteAt;
		context.ReadValueDefault("version", version, 1);
		context.ReadValueDefault("ownerIdentity", ownerIdentity, string.Empty);
		context.ReadValueDefault("locked", locked, false);
		context.ReadValueDefault("expiresAt", expiresAt, 0);
		context.ReadValueDefault("wreckDeleteAt", wreckDeleteAt, 0);
		if (version < 1 || version > 2 || wreckDeleteAt < 0)
			return false;
		claim.BFCG_RestoreWreckDeleteAt(wreckDeleteAt);

		// Discard expired claims; valid restored claims deliberately start locked regardless of their saved lock flag.
		if (wreckDeleteAt > 0 || claim.BFCG_IsDestroyed() || ownerIdentity.IsEmpty() || expiresAt <= System.GetUnixTime())
			claim.BFCG_ClearClaimState();
		else
			claim.BFCG_ApplyClaimState(ownerIdentity, true, expiresAt);

		return true;
	}
}
