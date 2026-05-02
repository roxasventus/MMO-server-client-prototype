using System.Collections;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;

public class LocalPlayerNetworkSender : MonoBehaviour
{
    [SerializeField] private string baseUrl;
    [SerializeField] private float sendInterval = 0.05f;

    private string jwtToken;
    private int characterId;
    private bool isSending;

    public void Initialize(string serverBaseUrl, int localCharacterId)
    {
        baseUrl = serverBaseUrl;
        characterId = localCharacterId;
        jwtToken = PlayerPrefs.GetString("JWT_TOKEN", "");

        if (string.IsNullOrEmpty(jwtToken))
        {
            Debug.LogError("LocalPlayerNetworkSender: JWT token not found.");
            return;
        }

        if (characterId <= 0)
        {
            Debug.LogError("LocalPlayerNetworkSender: characterId is invalid.");
            return;
        }

        StartSending();
    }

    public void StartSending()
    {
        if (isSending)
            return;

        isSending = true;
        StartCoroutine(SendMoveLoop());
    }

    public void StopSending()
    {
        isSending = false;
    }

    private IEnumerator SendMoveLoop()
    {
        while (isSending)
        {
            yield return SendMoveCoroutine();
            yield return new WaitForSeconds(sendInterval);
        }
    }

    private IEnumerator SendMoveCoroutine()
    {
        WorldMoveRequest moveRequest = new WorldMoveRequest
        {
            character_id = characterId,
            pos_x = transform.position.x,
            pos_y = transform.position.y,
            pos_z = transform.position.z,
            yaw = transform.eulerAngles.y
        };

        string json = JsonUtility.ToJson(moveRequest);

        using UnityWebRequest request = new UnityWebRequest(baseUrl + "/world/move", "POST");
        request.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(json));
        request.downloadHandler = new DownloadHandlerBuffer();

        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        yield return request.SendWebRequest();

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogWarning(
                $"LocalPlayerNetworkSender: /world/move failed\n" +
                $"Code: {request.responseCode}\n" +
                $"{request.downloadHandler.text}"
            );

            yield break;
        }
    }

    private void OnDisable()
    {
        StopSending();
    }
}