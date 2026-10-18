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

		context.WriteValue("version", 1);
		context.WriteValue("ownerIdentity", claim.BFCG_GetClaimOwnerIdentity());
		context.WriteValue("locked", claim.BFCG_IsClaimLocked());
		context.WriteValue("expiresAt", claim.BFCG_GetClaimExpiresAt());
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
		context.ReadValueDefault("version", version, 1);
		context.ReadValueDefault("ownerIdentity", ownerIdentity, string.Empty);
		context.ReadValueDefault("locked", locked, false);
		context.ReadValueDefault("expiresAt", expiresAt, 0);

		// Discard expired claims; valid restored claims deliberately start locked regardless of their saved lock flag.
		if (ownerIdentity.IsEmpty() || expiresAt <= System.GetUnixTime())
			claim.BFCG_ClearClaimState();
		else
			claim.BFCG_ApplyClaimState(ownerIdentity, true, expiresAt);

		return true;
	}
}
