using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;

public class WorldEnterManager : MonoBehaviour
{
    [Header("Network")]
    [SerializeField] private string baseUrl;

    [Header("Prefabs")]
    [SerializeField] private GameObject localPlayerPrefab;
    [SerializeField] private GameObject remotePlayerPrefab;

    [Header("Spawn Parents")]
    [SerializeField] private Transform playerRoot;

    [Header("Camera")]
    [SerializeField] private ThirdPersonCameraController cameraController;

    public GameObject LocalPlayerObject { get; private set; }
    public int LocalCharacterId { get; private set; }

    private readonly Dictionary<int, GameObject> remotePlayers = new();

    private string jwtToken;
    private int selectedCharacterId;

    [SerializeField] private WorldSyncManager worldSyncManager;

    public IReadOnlyDictionary<int, GameObject> RemotePlayers => remotePlayers;

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
        selectedCharacterId = PlayerPrefs.GetInt("selected_character_id", 0);

        StartCoroutine(EnterWorldCoroutine());
    }

    private IEnumerator EnterWorldCoroutine()
    {
        if (string.IsNullOrEmpty(jwtToken))
        {
            Debug.LogError("WorldEnterManager: JWT token not found.");
            yield break;
        }

        if (selectedCharacterId <= 0)
        {
            Debug.LogError("WorldEnterManager: selected_character_id is invalid.");
            yield break;
        }

        string url = baseUrl + "/world/enter";

        StringBuilder bodyBuilder = new StringBuilder();
        bodyBuilder.Append("{");
        bodyBuilder.AppendFormat("\"character_id\":{0}", selectedCharacterId);
        bodyBuilder.Append("}");

        using UnityWebRequest request = new UnityWebRequest(url, "POST");
        request.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(bodyBuilder.ToString()));
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        Debug.Log("Entering world...");
        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogError($"World enter HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        WorldEnterResponse response = JsonUtility.FromJson<WorldEnterResponse>(responseText);
        if (response == null)
        {
            Debug.LogError("World enter parse failed.");
            yield break;
        }

        if (response.result != "success")
        {
            Debug.LogError($"World enter failed: {response.message}");
            yield break;
        }

        SpawnLocalPlayer(response.character);

        if (response.nearby_players != null)
        {
            foreach (WorldCharacterData other in response.nearby_players)
            {
                //SpawnRemotePlayer(other);
                UpsertRemotePlayer(other);
            }
        }

        Debug.Log("World enter success.");

        if (worldSyncManager != null)
        {
            worldSyncManager.StartSync();
        }
        else
        {
            Debug.LogError("WorldSyncManager not found in scene.");
        }
    }

    private void SpawnLocalPlayer(WorldCharacterData character)
    {
        if (character == null)
        {
            Debug.LogError("SpawnLocalPlayer: character is null.");
            return;
        }

        if (localPlayerPrefab == null)
        {
            Debug.LogError("SpawnLocalPlayer: localPlayerPrefab is not assigned.");
            return;
        }

        Vector3 spawnPosition = new Vector3(character.pos_x, character.pos_y, character.pos_z);
        Quaternion spawnRotation = Quaternion.Euler(0f, character.yaw, 0f);

        Transform parent = playerRoot != null ? playerRoot : null;
        LocalPlayerObject = Instantiate(localPlayerPrefab, spawnPosition, spawnRotation, parent);
        LocalPlayerObject.name = $"LocalPlayer_{character.name}";

        LocalCharacterId = character.id;

        NetworkPlayerIdentity identity = LocalPlayerObject.GetComponent<NetworkPlayerIdentity>();
        if (identity != null)
        {
            identity.Initialize(character.id, character.user_id, character.name);
        }
        else
        {
            Debug.LogWarning("SpawnLocalPlayer: NetworkPlayerIdentity not found.");
        }

        // 카메라 연결
        if (cameraController != null)
        {
            Transform cameraTarget = LocalPlayerObject.transform.Find("CameraTarget");

            if (cameraTarget != null)
            {
                cameraController.SetTarget(cameraTarget);
            }
            else
            {
                Debug.LogError("SpawnLocalPlayer: CameraTarget not found.");
            }

            LocalPlayerController localController = LocalPlayerObject.GetComponent<LocalPlayerController>();
            if (localController != null)
            {
                localController.SetCameraTransform(cameraController.GetCameraTransform());
            }
        }
        else
        {
            Debug.LogError("SpawnLocalPlayer: cameraController is not assigned.");
        }
        // LocalPlayerNetworkSender(플레이어 좌표 sender) 초기화
        LocalPlayerNetworkSender sender = LocalPlayerObject.GetComponent<LocalPlayerNetworkSender>();
        if (sender != null)
        {
            sender.Initialize(baseUrl, LocalCharacterId);
        }
        else
        {
            Debug.LogWarning("SpawnLocalPlayer: LocalPlayerNetworkSender not found.");
        }

        //WorldSyncManager.Instance.StartSync();
    }
    /*
    private void SpawnRemotePlayer(WorldCharacterData character)
    {
        if (character == null)
        {
            Debug.LogWarning("SpawnRemotePlayer: character is null.");
            return;
        }

        if (remotePlayers.ContainsKey(character.id))
        {
            Debug.LogWarning($"SpawnRemotePlayer: character {character.id} already exists.");
            return;
        }

        if (remotePlayerPrefab == null)
        {
            Debug.LogError("SpawnRemotePlayer: remotePlayerPrefab is not assigned.");
            return;
        }

        Vector3 spawnPosition = new Vector3(character.pos_x, character.pos_y, character.pos_z);
        Quaternion spawnRotation = Quaternion.Euler(0f, character.yaw, 0f);

        Transform parent = playerRoot != null ? playerRoot : null;
        GameObject remoteObject = Instantiate(remotePlayerPrefab, spawnPosition, spawnRotation, parent);
        remoteObject.name = $"RemotePlayer_{character.name}_{character.id}";

        NetworkPlayerIdentity identity = remoteObject.GetComponent<NetworkPlayerIdentity>();
        if (identity != null)
        {
            identity.Initialize(character.id, character.user_id, character.name);
        }
        else
        {
            Debug.LogWarning("SpawnRemotePlayer: NetworkPlayerIdentity not found.");
        }

        RemotePlayerController remoteController = remoteObject.GetComponent<RemotePlayerController>();
        if (remoteController != null)
        {
            remoteController.SnapToPosition(spawnPosition);
            remoteController.SetTargetYaw(character.yaw);
            remoteController.SnapYaw(character.yaw);
        }

        remotePlayers.Add(character.id, remoteObject);
    }
    */
    // Upsert(업서트)는 Update와 Insert의 합성어로, 데이터베이스에서 고유 키(Unique Key) 또는 기본 키(Primary Key)를 기준으로 데이터가
    // 이미 존재하면 UPDATE(수정)하고, 존재하지 않으면 INSERT(삽입)하는 작업
    public void UpsertRemotePlayer(WorldCharacterData character)
    {
        if (character == null)
            return;

        if (character.id == LocalCharacterId)
            return;

        if (remotePlayers.TryGetValue(character.id, out GameObject existingObject))
        {
            // 존재하지 않는 플레이어이면 플레이어 목록에서 제거
            if (existingObject == null)
            {
                remotePlayers.Remove(character.id);
            }
            // 존재하는 플레이어이면 플레이어 정보 갱신
            else
            {
                RemotePlayerController remoteController = existingObject.GetComponent<RemotePlayerController>();
                if (remoteController != null)
                {
                    remoteController.SetTargetPosition(new Vector3(character.pos_x, character.pos_y, character.pos_z));
                    remoteController.SetTargetYaw(character.yaw);
                }

                NetworkPlayerIdentity identity = existingObject.GetComponent<NetworkPlayerIdentity>();
                if (identity != null)
                {
                    identity.Initialize(character.id, character.user_id, character.name);
                }

                return;
            }
        }

        // 플레이어 목록에 없는 새로운 유저면 스폰 후, 플레이어 목록에 추가
        SpawnRemotePlayerInternal(character);
    }

    private void SpawnRemotePlayerInternal(WorldCharacterData character)
    {
        if (remotePlayerPrefab == null)
        {
            Debug.LogError("SpawnRemotePlayerInternal: remotePlayerPrefab is not assigned.");
            return;
        }

        Vector3 spawnPosition = new Vector3(character.pos_x, character.pos_y, character.pos_z);
        Quaternion spawnRotation = Quaternion.Euler(0f, character.yaw, 0f);

        Transform parent = playerRoot != null ? playerRoot : null;
        GameObject remoteObject = Instantiate(remotePlayerPrefab, spawnPosition, spawnRotation, parent);
        remoteObject.name = $"RemotePlayer_{character.name}_{character.id}";

        NetworkPlayerIdentity identity = remoteObject.GetComponent<NetworkPlayerIdentity>();
        if (identity != null)
        {
            identity.Initialize(character.id, character.user_id, character.name);
        }

        RemotePlayerController remoteController = remoteObject.GetComponent<RemotePlayerController>();
        if (remoteController != null)
        {
            remoteController.SnapToPosition(spawnPosition);
            remoteController.SnapYaw(character.yaw);
        }

        remotePlayers[character.id] = remoteObject;
    }

    public void RemoveRemotePlayer(int characterId)
    {
        if (!remotePlayers.TryGetValue(characterId, out GameObject remoteObject))
            return;

        if (remoteObject != null)
        {
            Destroy(remoteObject);
        }

        remotePlayers.Remove(characterId);
    }

    public bool TryGetRemotePlayer(int characterId, out GameObject remoteObject)
    {
        return remotePlayers.TryGetValue(characterId, out remoteObject);
    }
    /*
    public void RemoveRemotePlayer(int characterId)
    {
        if (remotePlayers.TryGetValue(characterId, out GameObject remoteObject))
        {
            if (remoteObject != null)
            {
                Destroy(remoteObject);
            }

            remotePlayers.Remove(characterId);
        }
    }
    */
    public void ClearAllRemotePlayers()
    {
        foreach (var pair in remotePlayers)
        {
            if (pair.Value != null)
            {
                Destroy(pair.Value);
            }
        }

        remotePlayers.Clear();
    }
    /*
    public void SpawnRemotePlayerFromSync(WorldCharacterData character)
    {
        SpawnRemotePlayer(character);
    }
    */


    public void LeaveWorld()
    {
        if (LocalCharacterId <= 0)
        {
            Debug.LogWarning("LeaveWorld: LocalCharacterId is invalid.");
            return;
        }

        if (worldSyncManager != null)
        {
            worldSyncManager.StopSync();
        }


        StartCoroutine(LeaveWorldCoroutine());
    }

    private IEnumerator LeaveWorldCoroutine()
    {
        if (string.IsNullOrEmpty(jwtToken))
        {
            Debug.LogError("LeaveWorldCoroutine: JWT token not found.");
            yield break;
        }

        if (LocalCharacterId <= 0)
        {
            Debug.LogError("LeaveWorldCoroutine: LocalCharacterId is invalid.");
            yield break;
        }

        string url = baseUrl + "/world/leave";

        string json = $"{{\"character_id\":{LocalCharacterId}}}";

        using UnityWebRequest request = new UnityWebRequest(url, "POST");
        request.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(json));
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        Debug.Log("Leaving world...");

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogWarning($"Leave world HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        Debug.Log($"Leave world success: {responseText}");

        ClearAllRemotePlayers();

        if (LocalPlayerObject != null)
        {
            Destroy(LocalPlayerObject);
            LocalPlayerObject = null;
        }

        LocalCharacterId = 0;

        SceneLoader.Instance.Load("CharacterScene");
    }
}