// Add this optional marker to rental/event/temporary vehicles that must not be claimed or stored.
[ComponentEditorProps(category: "BFCG", description: "Exclude this vehicle from Claim, Garage and optional cleanup")]
class BFCG_ExcludedVehicleComponentClass : ScriptComponentClass {}
class BFCG_ExcludedVehicleComponent : ScriptComponent
{
    static bool IsExcluded(IEntity entity)
    {
        return entity && entity.FindComponent(BFCG_ExcludedVehicleComponent);
    }
}
