# Required runtime validation

- Compile with only ArmaReforger + this addon; load both terminal and parking prefab/materials/layout.
- Dedicated server + 2 clients, including join after native load and reconnect.
- Defaults and non-default settings (e.g. limits 2/5); client labels and server enforcement must agree.
- Malformed/out-of-range settings must block operations without rewriting the file.
- First free/current terminal spot, all spots occupied, characters/AI, two simultaneous retrievals, large vehicles.
- Three live claims then fourth rejected; garage records excluded; park releases a world slot.
- Restart a saved native GM/Conflict world: same vehicles, ownership/expiry, fuel/damage/inventory restored.
- World save/autosave is controlled by the host scenario; verify the actual commit/reload, not just Save().
- Garage records across restart; malformed/unwritable garage file; occupied-vehicle refusal; no seated character deletion.
- Forced crash around parking/retrieval on an isolated profile; check loss/duplication before production.
- Optional cleanup disabled causes no unclaimed-vehicle deletion; enabled timeout and exclusion marker tested separately.
- Removal/rollback tested with exported garage records and retained world/profile backups.
