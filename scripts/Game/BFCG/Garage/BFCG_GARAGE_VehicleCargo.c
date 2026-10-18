/*
 * Legacy cargo-tree serialization helpers by BlackFox.
 * Retained for the garage record format; current parking and retrieval do not call these helpers.
 * Only prefab nesting is represented; item condition, ammunition and other runtime state are not saved.
 */

class BFCG_GARAGE_ItemNode : JsonApiStruct
{
	string prefab;
	ref array<ref BFCG_GARAGE_ItemNode> items;

	void BFCG_GARAGE_ItemNode()
	{
		RegV("prefab");
		RegV("items");
		items = new array<ref BFCG_GARAGE_ItemNode>();
	}
}

sealed class BFCG_GARAGE_VehicleCargo
{
	protected static ref array<IEntity> s_Visited;

	static array<ref BFCG_GARAGE_ItemNode> SerializeCargo(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		InventoryStorageManagerComponent vehInv = InventoryStorageManagerComponent.Cast(vehicle.FindComponent(InventoryStorageManagerComponent));
		if (!vehInv)
		{
			Print(string.Format("[BFCG_GARAGE] VehicleCargo DUMP: %1 - InventoryStorageManagerComponent НЕ найден, багажник не сохранён",
				BFCG_PrefabName(vehicle)), LogLevel.WARNING);
			return null;
		}

		if (!s_Visited)
			s_Visited = {};
		s_Visited.Clear();

		array<ref BFCG_GARAGE_ItemNode> trunk = new array<ref BFCG_GARAGE_ItemNode>();
		int storagesCount = 0;
		int rawCount = 0;

		array<BaseInventoryStorageComponent> storages = {};
		vehInv.GetStorages(storages);
		storagesCount = storages.Count();

		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items);
			rawCount += items.Count();

			foreach (IEntity item : items)
			{
				BFCG_GARAGE_ItemNode node = SerializeItem(item, 0);
				if (node)
					trunk.Insert(node);
			}
		}

		Print(string.Format("[BFCG_GARAGE] VehicleCargo DUMP: %1 - хранилищ: %2, предметов сырьём: %3, узлов в снапшот: %4",
			BFCG_PrefabName(vehicle), storagesCount, rawCount, trunk.Count()));

		return trunk;
	}

	// Limit recursion and skip previously visited items to avoid duplicate records from shared storages.
	protected static BFCG_GARAGE_ItemNode SerializeItem(IEntity item, int depth)
	{
		if (!item || !item.GetPrefabData() || depth > 4)
			return null;

		if (s_Visited.Contains(item))
			return null;
		s_Visited.Insert(item);

		BFCG_GARAGE_ItemNode node = new BFCG_GARAGE_ItemNode();
		node.prefab = item.GetPrefabData().GetPrefabName();

		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(item.FindComponent(InventoryStorageManagerComponent));
		if (inv)
		{
			array<BaseInventoryStorageComponent> storages = {};
			inv.GetStorages(storages);

			foreach (BaseInventoryStorageComponent storage : storages)
			{
				if (!storage)
					continue;

				array<IEntity> children = {};
				storage.GetAll(children);
				foreach (IEntity child : children)
				{
					BFCG_GARAGE_ItemNode childNode = SerializeItem(child, depth + 1);
					if (childNode)
						node.items.Insert(childNode);
				}
			}
		}

		return node;
	}

	// This legacy helper clears prefab cargo before restoration; current garage retrieval does not call it.
	static void RestoreCargo(IEntity vehicle, array<ref BFCG_GARAGE_ItemNode> trunk)
	{
		if (!vehicle || !trunk)
			return;

		InventoryStorageManagerComponent vehInv = InventoryStorageManagerComponent.Cast(vehicle.FindComponent(InventoryStorageManagerComponent));
		if (!vehInv)
		{
			Print(string.Format("[BFCG_GARAGE] VehicleCargo RESTORE: %1 - InventoryStorageManagerComponent НЕ найден, %2 узлов не восстановлены",
				BFCG_PrefabName(vehicle), trunk.Count()), LogLevel.WARNING);
			return;
		}

		int stripped = StripStorageContent(vehInv);

		int ok = 0;
		foreach (BFCG_GARAGE_ItemNode node : trunk)
		{
			if (RestoreItem(node, vehInv))
				ok++;
		}

		Print(string.Format("[BFCG_GARAGE] VehicleCargo RESTORE: %1 - дефолтного груза снято: %2, узлов восстановлено: %3 из %4",
			BFCG_PrefabName(vehicle), stripped, ok, trunk.Count()));
	}

	// If insertion fails, the spawned item is left near the target and the failure is logged.
	protected static bool RestoreItem(BFCG_GARAGE_ItemNode node, InventoryStorageManagerComponent parentInv)
	{
		if (!node || node.prefab.IsEmpty() || !parentInv)
			return false;

		Resource resource = Resource.Load(node.prefab);
		if (!resource || !resource.IsValid())
			return false;

		IEntity owner = parentInv.GetOwner();
		if (!owner)
			return false;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		owner.GetTransform(params.Transform);

		IEntity item = GetGame().SpawnEntityPrefab(resource, owner.GetWorld(), params);
		if (!item)
			return false;

		if (!InsertAnywhere(parentInv, item))
		{
			Print(string.Format("[BFCG_GARAGE] VehicleCargo RESTORE: предмет %1 не поместился в хранилище", node.prefab), LogLevel.WARNING);
			return false;
		}

		InventoryStorageManagerComponent itemInv = InventoryStorageManagerComponent.Cast(item.FindComponent(InventoryStorageManagerComponent));
		if (itemInv)
		{

			StripStorageContent(itemInv);

			foreach (BFCG_GARAGE_ItemNode childNode : node.items)
				RestoreItem(childNode, itemInv);
		}

		return true;
	}

	// Deduplicate items exposed by multiple storages before recursive deletion.
	protected static int StripStorageContent(InventoryStorageManagerComponent inv)
	{
		if (!inv)
			return 0;

		if (!s_Visited)
			s_Visited = {};
		s_Visited.Clear();

		SCR_InventoryStorageManagerComponent scrInv = SCR_InventoryStorageManagerComponent.Cast(inv);

		int removed = 0;
		array<BaseInventoryStorageComponent> storages = {};
		inv.GetStorages(storages);

		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items);

			foreach (IEntity item : items)
			{
				if (!item || item.IsDeleted())
					continue;
				if (s_Visited.Contains(item))
					continue;
				s_Visited.Insert(item);

				if (scrInv)
					scrInv.TryRemoveItemFromInventory(item);
				SCR_EntityHelper.DeleteEntityAndChildren(item);
				removed++;
			}
		}

		return removed;
	}

	// Nested containers may have only the base inventory manager; use its storages as a fallback.
	protected static bool InsertAnywhere(InventoryStorageManagerComponent inv, IEntity item)
	{
		SCR_InventoryStorageManagerComponent scrInv = SCR_InventoryStorageManagerComponent.Cast(inv);
		if (scrInv && scrInv.TryInsertItem(item, EStoragePurpose.PURPOSE_ANY, null))
			return true;

		array<BaseInventoryStorageComponent> storages = {};
		inv.GetStorages(storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			if (inv.TryInsertItemInStorage(item, storage))
				return true;
		}

		return false;
	}

	protected static string BFCG_PrefabName(IEntity ent)
	{
		if (!ent || !ent.GetPrefabData())
			return "<?>";

		return ent.GetPrefabData().GetPrefabName();
	}
}
