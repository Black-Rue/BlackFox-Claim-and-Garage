/*
 * BlackFox opt-out marker for claiming, garage storage and optional unclaimed cleanup.
 * This marker does not disable vanilla vehicle persistence.
 */

[ComponentEditorProps(category: "BFCG", description: "Exclude this vehicle from Claim, Garage and optional cleanup")]
class BFCG_ExcludedVehicleComponentClass : ScriptComponentClass {}
class BFCG_ExcludedVehicleComponent : ScriptComponent
{
    static bool IsExcluded(IEntity entity)
    {
        return entity && entity.FindComponent(BFCG_ExcludedVehicleComponent);
    }
}
