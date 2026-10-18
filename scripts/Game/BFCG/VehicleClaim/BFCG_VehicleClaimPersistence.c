/*
 * BlackFox integration with the vanilla save-game lifecycle for world vehicles and claim metadata.
 * Native persistence owns world restoration and save commits; this class gates mutations and requests snapshots.
 */

class BFCG_VehicleClaimPersistence
{
    protected static bool s_Ready;
    static bool IsReady()
    {
        if (!Replication.IsServer() || !s_Ready || !BFCG_Settings.IsValid())
            return false;
        // Only an active save undergoing native loading blocks readiness; a session without a save must still work.
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
    // Native deserialization registers the restored controller directly; no prefab/position matching is needed.
    static void ReconcileWorldClaims()
    {

    }
    // Request a native transient save. The game mode's save policy controls durable commits.
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
