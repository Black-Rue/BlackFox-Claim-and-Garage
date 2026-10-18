// World entities and claim metadata share the vanilla save-game lifecycle.
class BFCG_VehicleClaimPersistence
{
    protected static bool s_Ready;
    static bool IsReady()
    {
        if (!Replication.IsServer() || !s_Ready || !BFCG_Settings.IsValid())
            return false;
        SaveGameManager saves = GetGame().GetSaveGameManager();
        return !saves || !saves.GetActiveSave() || !SCR_PersistenceSystem.IsLoadInProgress();
    }
    static void BeginRestore()
    {
        s_Ready = false;
        GetGame().GetCallqueue().Remove(Restore);
    }
    static void Restore()
    {
        if (!Replication.IsServer() || !BFCG_Settings.IsValid())
            return;
        SaveGameManager saves = GetGame().GetSaveGameManager();
        if (saves && saves.GetActiveSave() && SCR_PersistenceSystem.IsLoadInProgress())
        {
            GetGame().GetCallqueue().CallLater(Restore, 1000, false);
            return;
        }
        s_Ready = true;
        PlayerManager players = GetGame().GetPlayerManager();
        if (players)
        {
            array<int> ids = {};
            players.GetPlayers(ids);
            foreach (int id : ids)
                BFCG_VehicleClaimState.SyncClaimsToPlayer(id);
        }
        Print("[BFCG] Native claim restore ready; garage retrieval enabled");
    }
    static void ReconcileWorldClaims()
    {
        // Native Deserialize registers the exact restored controller; no prefab/position search.
    }
    static void SaveClaim(VehicleControllerComponent component)
    {
        if (!IsReady() || !component || !component.GetOwner() || component.GetOwner().IsDeleted())
            return;
        PersistenceSystem persistence = PersistenceSystem.GetInstance();
        if (persistence && persistence.GetState() == EPersistenceSystemState.ACTIVE)
        {
            if (!persistence.Save(component.GetOwner()))
                Print("[BFCG] Native vehicle save request failed", LogLevel.WARNING);
        }
    }
    static void Snapshot()
    {
        if (!IsReady())
            return;
        array<VehicleControllerComponent> claimed = {};
        BFCG_VehicleClaimState.GetClaimedComponents(claimed);
        foreach (VehicleControllerComponent component : claimed)
            SaveClaim(component);
    }
}
