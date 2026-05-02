using System.Collections;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.UI;

public class ChatUIManager : MonoBehaviour
{

    [Header("Server")]
    [SerializeField] private string serverBaseUrl;

    [Header("UI References")]
    [SerializeField] private TMP_InputField chatInputField;
    [SerializeField] private Button sendButton;
    [SerializeField] private Transform messageContent;
    [SerializeField] private GameObject chatMessageItemPrefab;
    [SerializeField] private ScrollRect scrollRect;

    [Header("Polling")]
    [SerializeField] private float pollingInterval = 1.0f;

    private string authToken = "";
    private int lastSeenChatId = 0;
    private Coroutine pollingCoroutine;
    private bool isSending = false;

    [System.Serializable]
    private class ChatEnterResponse
    {
        public string result;
        public string message;
        public int latest_chat_id;
    }

    [System.Serializable]
    private class ChatMessageData
    {
        public int id;
        public int user_id;
        public string username;
        public string message;
        public string created_at;
    }

    [System.Serializable]
    private class ChatMessagesResponse
    {
        public string result;
        public string message;
        public ChatMessageData[] messages;
    }

    [System.Serializable]
    private class ChatSendRequest
    {
        public string message;
    }

    [System.Serializable]
    private class BasicResponse
    {
        public string result;
        public string message;
    }

    private void Awake()
    {
        string envServerUrl = EnvLoader.Get("SERVER_URL");

        if (!string.IsNullOrEmpty(envServerUrl))
        {
            serverBaseUrl = envServerUrl;
        }

        //Debug.Log($"Server URL: {baseUrl}");
    }

    private void Start()
    {
        string token = PlayerPrefs.GetString("JWT_TOKEN", "");

        if (string.IsNullOrEmpty(token))
        {
            Debug.LogError("No token found.");
            SceneLoader.Instance.Load("CharacterScene");
            return;
        }

        Initialize(token);
    }

    public void Initialize(string token)
    {
        authToken = token;

        if (sendButton != null)
        {
            sendButton.onClick.RemoveAllListeners();
            sendButton.onClick.AddListener(OnClickSendButton);
        }

        if (chatInputField != null)
        {
            chatInputField.onSubmit.RemoveAllListeners();
            chatInputField.onSubmit.AddListener(OnSubmitInputField);
        }

        StartCoroutine(EnterChatCoroutine());
    }

    private IEnumerator EnterChatCoroutine()
    {
        string url = $"{serverBaseUrl}/chat/enter";

        using UnityWebRequest request = UnityWebRequest.Get(url);
        request.SetRequestHeader("Authorization", $"Bearer {authToken}");

        yield return request.SendWebRequest();

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogError($"[Chat Enter] Network Error: {request.error}");
            yield break;
        }

        if (request.responseCode != 200)
        {
            Debug.LogError($"[Chat Enter] HTTP Error: {request.responseCode}, Body: {request.downloadHandler.text}");
            yield break;
        }

        ChatEnterResponse response = JsonUtility.FromJson<ChatEnterResponse>(request.downloadHandler.text);

        if (response == null || response.result != "success")
        {
            Debug.LogError($"[Chat Enter] Invalid Response: {request.downloadHandler.text}");
            yield break;
        }

        lastSeenChatId = response.latest_chat_id;
        Debug.Log($"[Chat Enter] latest_chat_id = {lastSeenChatId}");

        if (pollingCoroutine != null)
        {
            StopCoroutine(pollingCoroutine);
        }

        pollingCoroutine = StartCoroutine(PollingChatCoroutine());
    }

    private IEnumerator PollingChatCoroutine()
    {
        while (true)
        {
            yield return StartCoroutine(GetNewMessagesCoroutine());
            yield return new WaitForSeconds(pollingInterval);
        }
    }

    private IEnumerator GetNewMessagesCoroutine()
    {
        string url = $"{serverBaseUrl}/chat/messages?after_id={lastSeenChatId}";

        using UnityWebRequest request = UnityWebRequest.Get(url);
        request.SetRequestHeader("Authorization", $"Bearer {authToken}");

        yield return request.SendWebRequest();

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogWarning($"[Chat Polling] Network Error: {request.error}");
            yield break;
        }

        if (request.responseCode != 200)
        {
            Debug.LogWarning($"[Chat Polling] HTTP Error: {request.responseCode}, Body: {request.downloadHandler.text}");
            yield break;
        }

        ChatMessagesResponse response = JsonUtility.FromJson<ChatMessagesResponse>(request.downloadHandler.text);

        if (response == null || response.result != "success")
        {
            Debug.LogWarning($"[Chat Polling] Invalid Response: {request.downloadHandler.text}");
            yield break;
        }

        if (response.messages == null || response.messages.Length == 0)
        {
            yield break;
        }

        for (int i = 0; i < response.messages.Length; i++)
        {
            ChatMessageData msg = response.messages[i];
            AddMessageToUI(msg);

            if (msg.id > lastSeenChatId)
            {
                lastSeenChatId = msg.id;
            }
        }

        ScrollToBottom();
    }

    private void AddMessageToUI(ChatMessageData msg)
    {
        if (chatMessageItemPrefab == null || messageContent == null)
        {
            Debug.LogError("[Chat UI] Missing prefab or content reference");
            return;
        }

        GameObject itemObj = Instantiate(chatMessageItemPrefab, messageContent);
        ChatMessageItem item = itemObj.GetComponent<ChatMessageItem>();

        if (item == null)
        {
            Debug.LogError("[Chat UI] ChatMessageItem component not found on prefab");
            return;
        }

        item.SetMessage(msg.username, msg.message);
    }

    public void OnClickSendButton()
    {
        if (isSending)
        {
            return;
        }

        string text = chatInputField.text;

        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        StartCoroutine(SendMessageCoroutine(text));
    }

    private void OnSubmitInputField(string text)
    {
        if (isSending)
        {
            return;
        }

        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        StartCoroutine(SendMessageCoroutine(text));
    }

    private IEnumerator SendMessageCoroutine(string text)
    {
        isSending = true;

        string trimmedText = text.Trim();
        string url = $"{serverBaseUrl}/chat/send";

        ChatSendRequest sendRequest = new ChatSendRequest
        {
            message = trimmedText
        };

        string jsonBody = JsonUtility.ToJson(sendRequest);
        byte[] bodyRaw = Encoding.UTF8.GetBytes(jsonBody);

        using UnityWebRequest request = new UnityWebRequest(url, "POST");
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);
        request.downloadHandler = new DownloadHandlerBuffer();

        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", $"Bearer {authToken}");

        yield return request.SendWebRequest();

        if (request.result != UnityWebRequest.Result.Success)
        {
            Debug.LogError($"[Chat Send] Network Error: {request.error}");
            isSending = false;
            yield break;
        }

        if (request.responseCode != 201)
        {
            Debug.LogError($"[Chat Send] HTTP Error: {request.responseCode}, Body: {request.downloadHandler.text}");
            isSending = false;
            yield break;
        }

        BasicResponse response = JsonUtility.FromJson<BasicResponse>(request.downloadHandler.text);

        if (response == null || response.result != "success")
        {
            Debug.LogError($"[Chat Send] Invalid Response: {request.downloadHandler.text}");
            isSending = false;
            yield break;
        }

        chatInputField.text = "";
        chatInputField.ActivateInputField();

        isSending = false;
    }

    private void ScrollToBottom()
    {
        if (scrollRect == null)
        {
            return;
        }

        Canvas.ForceUpdateCanvases();
        scrollRect.verticalNormalizedPosition = 0f;
    }

    private void OnDestroy()
    {
        if (sendButton != null)
        {
            sendButton.onClick.RemoveListener(OnClickSendButton);
        }

        if (chatInputField != null)
        {
            chatInputField.onSubmit.RemoveListener(OnSubmitInputField);
        }

        if (pollingCoroutine != null)
        {
            StopCoroutine(pollingCoroutine);
            pollingCoroutine = null;
        }
    }
}