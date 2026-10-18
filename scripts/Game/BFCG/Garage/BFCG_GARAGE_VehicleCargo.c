//------------------------------------------------------------------------------------------------
//! File: BFCG_GARAGE_VehicleCargo.c
//! BFCG_Garage - сериализация/восстановление содержимого багажника ТС
//! при постановке в гараж и вызове из него.
//!
//! Перенос прод-решения Everon (BlackFox 2026-07-31, BFCG_VehicleCargo из
//! BFCG_VehiclePersistence.c + интеграция BFCG_GarageCarKeyGate).
//! Отличия от Everon-версии:
//!  - убрана привязка CarKey2 (Key_LockComponent) - мода нет на Merak;
//!  - имена/логи в неймспейсе BFCG_GARAGE.
//!
//! Семантика (как в Everon v2.2, BF-010 «трещотка» префабного груза):
//!  - RestoreCargo ПЕРЕД вставкой сохранённого полностью снимает дефолтный
//!    груз свежего префаба (ТС и вложенных контейнеров). Иначе каждый
//!    цикл парк/анпарк добавлял бы префабный набор к багажнику.
//!  - Пустой сохранённый trunk значим: сознательно опустошённый багажник
//!    остаётся пустым. ВНИМАНИЕ: ТС, поставленные в гараж ДО появления
//!    этого кода, имеют пустой trunk в старых записях - при первом вызове
//!    после обновления они придут БЕЗ дефолтного префабного груза.
//!
//! Двойная защита от дублей лута: при сериализации ведётся список
//! посещённых энтити (пересечения между хранилищами возможны).
//!
//! Ограничения (как в Everon): не сохраняются патроны в магазинах,
//! состояние/прочность предметов, обвес на оружии в багажнике (префабы
//! восстанавливаются, внутреннее состояние - нет).
//!
//! Только сервер (вызывается из GarageManager.ParkVehicle/UnparkVehicle,
//! которые сами серверные). Статические поля: скриптовая VM однопоточна -
//! общий список visited безопасен (очищается на каждый вызов).
//------------------------------------------------------------------------------------------------

//! Узел лута: предмет + вложенные предметы (рюкзак с содержимым и т.п.)
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

//------------------------------------------------------------------------------------------------
//! Общий сериализатор багажника ТС (префабы, рекурсивно)
sealed class BFCG_GARAGE_VehicleCargo
{
	protected static ref array<IEntity> s_Visited;

	//------------------------------------------------------------------------------------------------
	//! Сериализация всего содержимого хранилищ ТС (null - хранилищ нет)
	static array<ref BFCG_GARAGE_ItemNode> SerializeCargo(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		//--- Базовый класс: контейнеры несут InventoryStorageManagerComponent,
		//--- а не SCR_-вариант (каст к SCR_ обрывал рекурсию - проверено в проде)
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

	//------------------------------------------------------------------------------------------------
	//! Рекурсивная сериализация предмета и его содержимого
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

	//------------------------------------------------------------------------------------------------
	//! Восстановление содержимого в хранилища ТС.
	//! ПЕРЕД вставкой сохранённого дефолтный груз свежего префаба снимается
	//! полностью - иначе каждый цикл парк/анпарк добавлял бы префабный набор.
	//! Пустой trunk значим: сознательно опустошённый багажник остаётся пустым.
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

		//--- Снести дефолтный груз префаба ДО вставки сохранённого
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

	//------------------------------------------------------------------------------------------------
	//! Спавн предмета у ТС и вставка в хранилище (рекурсивно для содержимого)
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

		//--- Не вставилось - остаётся лежать у ТС, предмет не теряется
		if (!InsertAnywhere(parentInv, item))
		{
			Print(string.Format("[BFCG_GARAGE] VehicleCargo RESTORE: предмет %1 не поместился в хранилище", node.prefab), LogLevel.WARNING);
			return false;
		}

		InventoryStorageManagerComponent itemInv = InventoryStorageManagerComponent.Cast(item.FindComponent(InventoryStorageManagerComponent));
		if (itemInv)
		{
			//--- Дефолтное содержимое контейнера снять ДО восстановления
			//--- сохранённого (та же «трещотка», что и на уровне ТС)
			StripStorageContent(itemInv);

			foreach (BFCG_GARAGE_ItemNode childNode : node.items)
				RestoreItem(childNode, itemInv);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Снять ВСЁ текущее содержимое хранилищ менеджера (дефолтный груз
	//! свежего префаба). Возвращает число удалённых корневых предметов.
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

	//------------------------------------------------------------------------------------------------
	//! Вставка предмета: сначала штатный TryInsertItem (SCR_-менеджер ТС),
	//! при недоступности - перебор хранилищ TryInsertItemInStorage
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

	//------------------------------------------------------------------------------------------------
	//! Имя префаба для логов (безопасно к null)
	protected static string BFCG_PrefabName(IEntity ent)
	{
		if (!ent || !ent.GetPrefabData())
			return "<?>";

		return ent.GetPrefabData().GetPrefabName();
	}
}
