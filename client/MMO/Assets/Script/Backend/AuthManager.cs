using System.Collections;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;

public class AuthManager : MonoBehaviour
{
    [Header("Server")]
    [SerializeField] private string baseUrl;

    [Header("UI")]
    [SerializeField] private TMP_InputField usernameInput;
    [SerializeField] private TMP_InputField passwordInput;
    [SerializeField] private TMP_Text resultText;

    private string jwtToken = "";

    private const string TokenKey = "JWT_TOKEN";
    private const string UsernameKey = "LOGGED_IN_USERNAME";
    private const string UserIdKey = "LOGGED_IN_USER_ID";

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
        LoadSavedToken();
        UpdateResultText("Ready");
    }

    public void OnClickRegister()
    {
        string username = usernameInput.text.Trim();
        string password = passwordInput.text;

        StartCoroutine(RegisterCoroutine(username, password));
    }

    public void OnClickLogin()
    {
        string username = usernameInput.text.Trim();
        string password = passwordInput.text;

        StartCoroutine(LoginCoroutine(username, password));
    }

    public void OnClickGetMe()
    {
        StartCoroutine(GetMeCoroutine());
    }

    public void OnClickLogout()
    {
        ClearSavedToken();
        UpdateResultText("Logged out");
    }

    private IEnumerator RegisterCoroutine(string username, string password)
    {
        if (!ValidateInput(username, password))
        {
            yield break;
        }

        string url = baseUrl + "/register";

        AuthRequest requestData = new AuthRequest
        {
            username = username,
            password = password
        };

        string json = JsonUtility.ToJson(requestData);

        using UnityWebRequest request = new UnityWebRequest(url, "POST");

        // C#에서 JSON 문자열(string)을 UTF-8로 인코딩된 바이트 배열(byte[])로 변환하는 메서드입니다.
        // 네트워크 전송, 파일 저장, 또는 MemoryStream에 데이터를 쓸 때 한글 깨짐 없이 데이터를 처리하기 위해 주로 사용
        byte[] bodyRaw = Encoding.UTF8.GetBytes(json);

        request.uploadHandler = new UploadHandlerRaw(bodyRaw); // UnityWebRequest 시스템 내에서 바이트 배열 데이터를 서버로 전송하는 역할, UploadHandlerRaw는 주로 JSON이나 텍스트 데이터를 전송할 때 사용
        request.downloadHandler = new DownloadHandlerBuffer(); // DownloadHandler는 서버로부터 데이터를 받아 클라이언트로 "다운로드"하는 역할, 서버의 응답 데이터를 처리하고, 필요한 형태로 변환하여 애플리케이션에 제공
                                                               // DownloadHandlerBuffer는 서버의 응답을 메모리 버퍼에 저장합니다. 이는 주로 텍스트나 바이너리 데이터를 처리할 때 사용
        request.SetRequestHeader("Content-Type", "application/json");

        UpdateResultText("Registering...");

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            UpdateResultText($"Register HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        AuthResponse response = ParseAuthResponse(responseText);
        if (response == null)
        {
            UpdateResultText("Register parse failed");
            yield break;
        }

        UpdateResultText(
            $"Register Result: {response.result}\n" +
            $"Message: {response.message}\n" +
            $"UserId: {response.user_id}"
        );
    }

    private IEnumerator LoginCoroutine(string username, string password)
    {
        if (!ValidateInput(username, password))
        {
            yield break;
        }

        string url = baseUrl + "/login";

        AuthRequest requestData = new AuthRequest
        {
            username = username,
            password = password
        };

        string json = JsonUtility.ToJson(requestData);

        using UnityWebRequest request = new UnityWebRequest(url, "POST");
        byte[] bodyRaw = Encoding.UTF8.GetBytes(json);

        request.uploadHandler = new UploadHandlerRaw(bodyRaw);
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Content-Type", "application/json");

        UpdateResultText("Logging in...");

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            UpdateResultText($"Login HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        AuthResponse response = ParseAuthResponse(responseText);
        if (response == null)
        {
            UpdateResultText("Login parse failed");
            yield break;
        }

        if (response.result == "success")
        {
            jwtToken = response.token;
            SaveToken(jwtToken, response.username, response.user_id);

            UpdateResultText(
                $"Login Success\n" +
                $"UserId: {response.user_id}\n" +
                $"Username: {response.username}\n" +
                $"Token Saved: {!string.IsNullOrEmpty(jwtToken)}"
            );

            SceneLoader.Instance.Load("CharacterScene");
        }
        else
        {
            UpdateResultText(
                $"Login Failed\n" +
                $"Message: {response.message}"
            );
        }
    }

    private IEnumerator GetMeCoroutine()
    {
        if (string.IsNullOrEmpty(jwtToken))
        {
            UpdateResultText("No JWT token. Please login first.");
            yield break;
        }

        string url = baseUrl + "/me";

        using UnityWebRequest request = UnityWebRequest.Get(url);
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        UpdateResultText("Checking token with /me ...");

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            UpdateResultText($"GET /me HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        MeResponse response = ParseMeResponse(responseText);
        if (response == null)
        {
            UpdateResultText("Me parse failed");
            yield break;
        }

        UpdateResultText(
            $"Token Valid\n" +
            $"UserId: {response.user_id}\n" +
            $"Username: {response.username}"
        );

    }

    private bool ValidateInput(string username, string password)
    {
        if (string.IsNullOrWhiteSpace(username))
        {
            UpdateResultText("Username is empty");
            return false;
        }

        if (string.IsNullOrEmpty(password))
        {
            UpdateResultText("Password is empty");
            return false;
        }

        return true;
    }

    private AuthResponse ParseAuthResponse(string json)
    {
        if (string.IsNullOrEmpty(json))
        {
            return null;
        }

        try
        {
            return JsonUtility.FromJson<AuthResponse>(json);
        }
        catch
        {
            Debug.LogError("Failed to parse AuthResponse: " + json);
            return null;
        }
    }

    private MeResponse ParseMeResponse(string json)
    {
        if (string.IsNullOrEmpty(json))
        {
            return null;
        }

        try
        {
            return JsonUtility.FromJson<MeResponse>(json);
        }
        catch
        {
            Debug.LogError("Failed to parse MeResponse: " + json);
            return null;
        }
    }

    private void SaveToken(string token, string username, int userId)
    {
        PlayerPrefs.SetString(TokenKey, token);
        PlayerPrefs.SetString(UsernameKey, username);
        PlayerPrefs.SetInt(UserIdKey, userId);
        PlayerPrefs.Save();
    }

    private void LoadSavedToken()
    {
        jwtToken = PlayerPrefs.GetString(TokenKey, "");

        if (!string.IsNullOrEmpty(jwtToken))
        {
            string username = PlayerPrefs.GetString(UsernameKey, "");
            int userId = PlayerPrefs.GetInt(UserIdKey, 0);

            UpdateResultText(
                $"Saved token loaded\n" +
                $"UserId: {userId}\n" +
                $"Username: {username}"
            );
        }
    }

    private void ClearSavedToken()
    {
        jwtToken = "";
        PlayerPrefs.DeleteKey(TokenKey);
        PlayerPrefs.DeleteKey(UsernameKey);
        PlayerPrefs.DeleteKey(UserIdKey);
        PlayerPrefs.Save();
        SceneLoader.Instance.Load("LoginScene");
    }

    private void UpdateResultText(string message)
    {
        if (resultText != null)
        {
            resultText.text = message;
        }

        Debug.Log("[AuthManager] " + message);
    }
}