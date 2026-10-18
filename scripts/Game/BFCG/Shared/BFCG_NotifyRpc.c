/*
 * BlackFox server-to-owner notification channel using player-controller RPCs.
 * Call BFCG_Notify helpers on the server; popup and sound playback run on the receiving client.
 */

modded class SCR_PlayerController
{

	void BFCG_NotifyPlayer(string msg)
	{
		Rpc(BFCG_RpcDo_NotifyPlayer, msg);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void BFCG_RpcDo_NotifyPlayer(string msg)
	{
		SCR_PopUpNotification.GetInstance().PopupMsg(msg);
		AudioSystem.PlaySound("{275A883E38CDEF60}Sounds/UI/Samples/Menu/UI_Task_Failed.wav");
	}
}

class BFCG_Notify
{

	// Server entry point: resolve character to controller and deliver only to its owner.
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
