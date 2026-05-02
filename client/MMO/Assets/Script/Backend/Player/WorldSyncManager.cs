using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;

public class WorldSyncManager : MonoBehaviour
{

    [SerializeField] private string baseUrl;
    [SerializeField] private float syncInterval = 0.1f;
    [SerializeField] private WorldEnterManager worldEnterManager;

    private string jwtToken;
    private bool isSyncing;

    private void Awake()
    {
        string envServerUrl = EnvLoader.Get("SERVER_URL");

        if (!string.IsNullOrEmpty(envServerUrl))
        {
            baseUrl = envServerUrl;
        }

        //Debug.Log($"Server URL: {baseUrl}");
    }
    private void Start()
    {
        jwtToken = PlayerPrefs.GetString("JWT_TOKEN", "");

        if (worldEnterManager == null)
        {
            Debug.LogError("WorldSyncManager: WorldEnterManager is not assigned.");
            return;
        }

        //StartSync();
    }

    public void StartSync()
    {
        if (isSyncing)
            return;

        isSyncing = true;
        StartCoroutine(WorldPlayersSyncLoop());
    }

    public void StopSync()
    {
        isSyncing = false;
    }

    private IEnumerator WorldPlayersSyncLoop()
    {
        while (isSyncing)
        {
            yield return SyncPlayersCoroutine();
            yield return new WaitForSeconds(syncInterval);
        }
    }

    private IEnumerator SyncPlayersCoroutine()
    {

        if (string.IsNullOrEmpty(jwtToken))
        {
            Debug.LogError("WorldSyncManager: JWT token not found.");
            yield break;
        }

        using UnityWebRequest request = UnityWebRequest.Get(baseUrl + "/world/players");
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogWarning($"WorldSyncManager: /world/players failed\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        WorldPlayersResponse response = JsonUtility.FromJson<WorldPlayersResponse>(responseText);
        if (response == null)
        {
            Debug.LogWarning("WorldSyncManager: response parse failed.");
            yield break;
        }

        if (response.result != "success")
        {
            Debug.LogWarning($"WorldSyncManager: sync failed - {response.message}");
            yield break;
        }
        //Debug.Log(response.players.Length);
        ApplyPlayersSnapshot(response.players);
    }


    private void ApplyPlayersSnapshot(WorldCharacterData[] players)
    {
        HashSet<int> latestIds = new HashSet<int>();

        if (players != null)
        {
            foreach (WorldCharacterData player in players)
            {
                if (player == null)
                    continue;

                latestIds.Add(player.id);
                worldEnterManager.UpsertRemotePlayer(player);
            }
        }

        List<int> toRemove = new List<int>();

        foreach (var pair in worldEnterManager.RemotePlayers)
        {
            int characterId = pair.Key;

            if (!latestIds.Contains(characterId))
            {
                toRemove.Add(characterId);
            }
        }

        foreach (int characterId in toRemove)
        {
            worldEnterManager.RemoveRemotePlayer(characterId);
        }
    }
    /*
    private void ApplyPlayersSnapshot(WorldCharacterData[] players)
    {
        HashSet<int> latestIds = new HashSet<int>();

        if (players != null)
        {
            foreach (WorldCharacterData player in players)
            {
                if (player == null)
                    continue;

                latestIds.Add(player.id);

                if (worldEnterManager.TryGetRemotePlayer(player.id, out GameObject remoteObject))
                {
                    RemotePlayerController remoteController = remoteObject.GetComponent<RemotePlayerController>();
                    if (remoteController != null)
                    {
                        remoteController.SetTargetPosition(new Vector3(player.pos_x, player.pos_y, player.pos_z));
                        remoteController.SetTargetYaw(player.yaw);
                    }
                }
                else
                {
                    worldEnterManager.SpawnRemotePlayerFromSync(player);
                }
            }
        }

        // 서버 최신 목록에 없는 플레이어 제거
        List<int> toRemove = new List<int>();

        foreach (var pair in worldEnterManager.RemotePlayers)
        {
            int characterId = pair.Key;

            if (!latestIds.Contains(characterId))
            {
                toRemove.Add(characterId);
            }
        }

        foreach (int characterId in toRemove)
        {
            worldEnterManager.RemoveRemotePlayer(characterId);
        }
    }
    */
}