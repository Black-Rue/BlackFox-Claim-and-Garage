/*
 * Standalone server settings by BlackFox.
 * Configuration: $profile:BlackFoxClaimGarage/settings.json. Restart the session after editing.
 * Invalid or unreadable settings disable garage/claim operations without overwriting the existing file.
 */

class BFCG_Settings : JsonApiStruct
{
    static const string DATA_DIR = "$profile:BlackFoxClaimGarage";
    static const string FILE = DATA_DIR + "/settings.json";
    protected static ref BFCG_Settings s_Instance;
    protected static bool s_Valid;
	int maxWorldClaims = 3;
	int maxGarageVehicles = 12;
	int claimDurationHours = 72;
	bool enableUnclaimedCleanup = false;
	int unclaimedLifetimeHours = 6;
	int destroyedVehicleLifetimeSeconds = 1200;
	int initialRestoreDelaySeconds = 30;
	int proxyScanSeconds = 30;
	float spawnClearanceMeters = 3.0;
	int spawnReservationMilliseconds = 1000;
	float maxGarageRequestDistanceMeters = 10.0;
    void BFCG_Settings()
    {
		RegV("maxWorldClaims");
		RegV("maxGarageVehicles");
		RegV("claimDurationHours");
		RegV("enableUnclaimedCleanup");
		RegV("unclaimedLifetimeHours");
		RegV("destroyedVehicleLifetimeSeconds");
		RegV("initialRestoreDelaySeconds");
		RegV("proxyScanSeconds");
		RegV("spawnClearanceMeters");
		RegV("spawnReservationMilliseconds");
		RegV("maxGarageRequestDistanceMeters");
    }
    static BFCG_Settings Get()
    {
        if (!s_Instance)
            s_Instance = new BFCG_Settings();
        return s_Instance;
    }
    static bool IsValid()
    {
        return s_Valid;
    }
    static void Load()
    {
        s_Valid = false;
        s_Instance = new BFCG_Settings();
        FileIO.MakeDirectory(DATA_DIR);
        if (FileIO.FileExists(FILE))
        {
            if (!s_Instance.LoadFromFile(FILE))
            {
                Print("[BFCG] Cannot read settings.json; garage/claim disabled", LogLevel.ERROR);
                return;
            }
        }
        else if (!s_Instance.SaveToFile(FILE))
        {
            Print("[BFCG] Cannot create settings.json; garage/claim disabled", LogLevel.ERROR);
            return;
        }
        BFCG_Settings cfg = s_Instance;
        bool valid = true;
        if (cfg.maxWorldClaims < 1 || cfg.maxWorldClaims > 1000)
            valid = false;
        if (cfg.maxGarageVehicles < 1 || cfg.maxGarageVehicles > 1000)
            valid = false;
        if (cfg.claimDurationHours < 1 || cfg.claimDurationHours > 8760)
            valid = false;
        if (cfg.unclaimedLifetimeHours < 1 || cfg.unclaimedLifetimeHours > 8760)
            valid = false;
        if (cfg.destroyedVehicleLifetimeSeconds < 60 || cfg.destroyedVehicleLifetimeSeconds > 86400)
            valid = false;
        if (cfg.initialRestoreDelaySeconds < 0 || cfg.initialRestoreDelaySeconds > 300)
            valid = false;
        if (cfg.proxyScanSeconds < 5 || cfg.proxyScanSeconds > 3600)
            valid = false;
        if (cfg.spawnClearanceMeters < 1 || cfg.spawnClearanceMeters > 50)
            valid = false;
        if (cfg.spawnReservationMilliseconds < 100 || cfg.spawnReservationMilliseconds > 10000)
            valid = false;
        if (cfg.maxGarageRequestDistanceMeters < 2.5 || cfg.maxGarageRequestDistanceMeters > 100)
            valid = false;
        if (!valid)
        {
            Print("[BFCG] Invalid settings.json values; garage/claim disabled", LogLevel.ERROR);
            return;
        }
        s_Valid = true;
        Print("[BFCG] Settings loaded: world=" + cfg.maxWorldClaims + " garage=" + cfg.maxGarageVehicles);
    }
    // Owner RPC updates display values only; server authority never reads this client cache.
    static void ApplyDisplaySettings(int worldLimit, int garageLimit, int hours)
    {

        if (Replication.IsServer())
            return;
        Get().maxWorldClaims = worldLimit;
        Get().maxGarageVehicles = garageLimit;
        Get().claimDurationHours = hours;
    }
}
