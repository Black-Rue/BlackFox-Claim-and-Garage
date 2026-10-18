/**
 * BFCG_NotifyRpc.c
 *
 * BlackFox 2026-08-22: общий канал уведомлений «сервер → конкретный
 * игрок» для BFCG-скриптов Merak.
 *
 * ЗАЧЕМ: BaseChatComponent.SendMessage(msg, 0) с сервера НЕ доходит до
 * чат-панели игрока (доказано на проде Merak 22.08: серверные Print
 * срабатывали, игрок ничего не видел). Этот файл использует доказанный
 * на проде путь FuelPayment: RplRpc на SCR_PlayerController → клиент
 * владельца → SCR_PopUpNotification + звук.
 *
 * Использование: BFCG_Notify.ToPlayer(pUserEntity, "текст") — вызывать
 * ТОЛЬКО на сервере. Внутри: playerId → PlayerController → RPC Owner.
 *
 * ОТКАТ: удалить файл; вызовы BFCG_Notify в BuildNotifications.c и
 * SafeZoneBuildBlock.c тогда тоже убрать (см. их заголовки).
 */

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//! Сервер: отправить уведомление владельцу этого контроллера.
	void BFCG_NotifyPlayer(string msg)
	{
		Rpc(BFCG_RpcDo_NotifyPlayer, msg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_RpcDo_NotifyPlayer(string msg)
	{
		SCR_PopUpNotification.GetInstance().PopupMsg(msg);
		AudioSystem.PlaySound("{275A883E38CDEF60}Sounds/UI/Samples/Menu/UI_Task_Failed.wav");
	}
}

//------------------------------------------------------------------------------------------------
//! Хелпер: доставка уведомления игроку по его персонажу.
class BFCG_Notify
{
	//------------------------------------------------------------------------------------------------
	//! ВЫЗЫВАТЬ ТОЛЬКО НА СЕРВЕРЕ. В лог пишутся только отказы
	//! доставки (WARNING) — штатная работа молчит.
	static void ToPlayer(IEntity pUserEntity, string msg)
	{
		if (!pUserEntity)
		{
			Print("[BFCG] Notify: нет pUserEntity, сообщение не доставлено: " + msg, LogLevel.WARNING);
			return;
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
		{
			Print("[BFCG] Notify: нет PlayerManager: " + msg, LogLevel.WARNING);
			return;
		}

		int playerId = pm.GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
		{
			Print("[BFCG] Notify: playerId=" + playerId + " (IA/нет контроллера): " + msg, LogLevel.WARNING);
			return;
		}

		PlayerController pc = pm.GetPlayerController(playerId);
		SCR_PlayerController spc = SCR_PlayerController.Cast(pc);
		if (!spc)
		{
			Print("[BFCG] Notify: нет PlayerController для playerId=" + playerId + ": " + msg, LogLevel.WARNING);
			return;
		}

		spc.BFCG_NotifyPlayer(msg);
	}

	//------------------------------------------------------------------------------------------------
	//! Всем игрокам (для GM-рассылок). ВЫЗЫВАТЬ ТОЛЬКО НА СЕРВЕРЕ.
	static void ToAll(string msg)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> players = {};
		pm.GetPlayers(players);
		foreach (int pid : players)
		{
			SCR_PlayerController spc = SCR_PlayerController.Cast(pm.GetPlayerController(pid));
			if (spc)
				spc.BFCG_NotifyPlayer(msg);
		}
	}
}
