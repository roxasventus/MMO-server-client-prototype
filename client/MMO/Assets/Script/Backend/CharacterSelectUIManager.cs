using System.Collections;
using System.Collections.Generic;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.UI;
using UnityEngine.SceneManagement;

public class CharacterSelectUIManager : MonoBehaviour
{
    [Header("Network")]
    [SerializeField] private string baseUrl;

    [Header("List")]
    [SerializeField] private Transform listContent;
    [SerializeField] private CharacterListItemUI listItemPrefab;

    [Header("Top UI")]
    [SerializeField] private TMP_Text accountText;
    [SerializeField] private TMP_Text statusText;

    [Header("Detail UI")]
    [SerializeField] private TMP_Text detailNameText;
    [SerializeField] private TMP_Text detailLevelText;
    [SerializeField] private TMP_Text detailExpText;
    [SerializeField] private TMP_Text detailGoldText;
    [SerializeField] private TMP_Text detailHpText;
    [SerializeField] private TMP_Text detailMpText;
    [SerializeField] private TMP_Text detailAttackText;
    [SerializeField] private TMP_Text detailDefenseText;

    [Header("Buttons")]
    [SerializeField] private Button createButton;
    [SerializeField] private Button deleteButton;
    [SerializeField] private Button enterWorldButton;
    [SerializeField] private Button logoutButton;

    [Header("Popup")]
    [SerializeField] private CreateCharacterPopup createCharacterPopup;

    private readonly List<CharacterListItemUI> spawnedItems = new();
    private CharacterData selectedCharacter;
    private CharacterListItemUI selectedItem;

    private string jwtToken;
    private string username;

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
        username = PlayerPrefs.GetString("LOGGED_IN_USERNAME", "");

        if (accountText != null)
            accountText.text = string.IsNullOrEmpty(username) ? "Account: Unknown" : $"Account: {username}";

        BindButtons();

        if (createCharacterPopup != null)
        {
            createCharacterPopup.Initialize(this);
            createCharacterPopup.Close();
        }

        ClearDetailPanel();
        RefreshButtonState();
        StartCoroutine(LoadCharacterListCoroutine());
    }

    private void BindButtons()
    {
        if (createButton != null)
        {
            createButton.onClick.RemoveAllListeners();
            createButton.onClick.AddListener(OpenCreatePopup);
        }

        if (deleteButton != null)
        {
            deleteButton.onClick.RemoveAllListeners();
            deleteButton.onClick.AddListener(OnClickDeleteCharacter);
        }

        if (enterWorldButton != null)
        {
            enterWorldButton.onClick.RemoveAllListeners();
            enterWorldButton.onClick.AddListener(OnClickEnterWorld);
        }

        if (logoutButton != null)
        {
            logoutButton.onClick.RemoveAllListeners();
            logoutButton.onClick.AddListener(OnClickLogout);
        }
    }

    private IEnumerator LoadCharacterListCoroutine()
    {
        if (string.IsNullOrEmpty(jwtToken))
        {
            SetStatus("JWT token not found.");
            yield break;
        }

        SetStatus("Loading character list...");

        using UnityWebRequest request = UnityWebRequest.Get(baseUrl + "/characters");
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Authorization", $"Bearer {jwtToken}");

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            SetStatus($"Character list HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        CharacterListResponse response = JsonUtility.FromJson<CharacterListResponse>(responseText);
        if (response == null)
        {
            SetStatus("Character list parse failed.");
            yield break;
        }

        if (response.result != "success")
        {
            SetStatus($"Character list failed: {response.message}");
            yield break;
        }

        RebuildCharacterList(response.characters);
        SetStatus("Character list loaded.");
    }

    private void RebuildCharacterList(CharacterData[] characters)
    {
        ClearCharacterList();

        selectedCharacter = null;
        selectedItem = null;
        ClearDetailPanel();

        if (characters == null || characters.Length == 0)
        {
            SetStatus("No characters found. Create a new character.");
            RefreshButtonState();
            return;
        }

        foreach (CharacterData character in characters)
        {
            CharacterListItemUI item = Instantiate(listItemPrefab, listContent);
            item.Setup(character, OnCharacterItemClicked);
            spawnedItems.Add(item);
        }

        OnCharacterItemClicked(spawnedItems[0].CharacterData, spawnedItems[0]);
        RefreshButtonState();
    }

    private void ClearCharacterList()
    {
        foreach (CharacterListItemUI item in spawnedItems)
        {
            if (item != null)
                Destroy(item.gameObject);
        }

        spawnedItems.Clear();
    }

    private void OnCharacterItemClicked(CharacterData data, CharacterListItemUI clickedItem)
    {
        if (selectedItem != null)
            selectedItem.SetSelected(false);

        selectedItem = clickedItem;
        selectedCharacter = data;

        if (selectedItem != null)
            selectedItem.SetSelected(true);

        RefreshDetailPanel(data);
        RefreshButtonState();
    }

    private void RefreshDetailPanel(CharacterData character)
    {
        if (character == null)
        {
            ClearDetailPanel();
            return;
        }

        if (detailNameText != null)
            detailNameText.text = $"이름: {character.name}";

        if (detailLevelText != null)
            detailLevelText.text = $"레벨: {character.level}";

        if (detailExpText != null)
            detailExpText.text = $"EXP: {character.exp}";

        if (detailGoldText != null)
            detailGoldText.text = $"골드: {character.gold}";

        if (detailHpText != null)
            detailHpText.text = $"HP: {character.hp} / {character.max_hp}";

        if (detailMpText != null)
            detailMpText.text = $"MP: {character.mp} / {character.max_mp}";

        if (detailAttackText != null)
            detailAttackText.text = $"공격력: {character.attack_power}";

        if (detailDefenseText != null)
            detailDefenseText.text = $"방어력: {character.defense}";

    }

    private void ClearDetailPanel()
    {
        if (detailNameText != null) detailNameText.text = "이름: -";
        if (detailLevelText != null) detailLevelText.text = "레벨: -";
        if (detailExpText != null) detailExpText.text = "EXP: -";
        if (detailGoldText != null) detailGoldText.text = "골드: -";
        if (detailHpText != null) detailHpText.text = "HP: -";
        if (detailMpText != null) detailMpText.text = "MP: -";
        if (detailAttackText != null) detailAttackText.text = "공격력: -";
        if (detailDefenseText != null) detailDefenseText.text = "방어력: -";
    }

    private void RefreshButtonState()
    {
        bool hasSelection = selectedCharacter != null;

        if (deleteButton != null)
            deleteButton.interactable = hasSelection;

        if (enterWorldButton != null)
            enterWorldButton.interactable = hasSelection;
    }

    private void OpenCreatePopup()
    {
        if (createCharacterPopup != null)
            createCharacterPopup.Open();
    }

    public void RequestCreateCharacterFromPopup()
    {
        if (createCharacterPopup == null)
            return;

        string characterName = createCharacterPopup.GetCharacterName();

        if (string.IsNullOrWhiteSpace(characterName))
        {
            createCharacterPopup.SetMessage("Please enter a character name.");
            return;
        }

        StartCoroutine(CreateCharacterCoroutine(characterName));
    }

    private IEnumerator CreateCharacterCoroutine(string characterName)
    {
        createCharacterPopup?.SetMessage("Creating character...");

        CreateCharacterRequest requestData = new CreateCharacterRequest
        {
            name = characterName
        };

        string json = JsonUtility.ToJson(requestData);

        using UnityWebRequest request = new UnityWebRequest(baseUrl + "/characters/create", "POST");
        request.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(json));
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            createCharacterPopup?.SetMessage($"HTTP Error: {request.responseCode}");
            yield break;
        }

        CreateCharacterResponse response = JsonUtility.FromJson<CreateCharacterResponse>(responseText);
        if (response == null)
        {
            createCharacterPopup?.SetMessage("Create parse failed.");
            yield break;
        }

        if (response.result != "success")
        {
            createCharacterPopup?.SetMessage(response.message);
            yield break;
        }

        createCharacterPopup?.Close();
        SetStatus("Character created successfully.");

        yield return LoadCharacterListCoroutine();

        if (response.character != null)
        {
            SelectCharacterById(response.character.id);
        }
    }

    private void SelectCharacterById(int characterId)
    {
        foreach (CharacterListItemUI item in spawnedItems)
        {
            if (item != null && item.CharacterData != null && item.CharacterData.id == characterId)
            {
                OnCharacterItemClicked(item.CharacterData, item);
                return;
            }
        }
    }

    private void OnClickDeleteCharacter()
    {
        if (selectedCharacter == null)
        {
            SetStatus("Please select a character first.");
            return;
        }

        StartCoroutine(DeleteCharacterCoroutine(selectedCharacter.id));
    }

    private IEnumerator DeleteCharacterCoroutine(int characterId)
    {
        SetStatus("Deleting character...");

        DeleteCharacterRequest requestData = new DeleteCharacterRequest
        {
            character_id = characterId
        };

        string json = JsonUtility.ToJson(requestData);

        using UnityWebRequest request = new UnityWebRequest(baseUrl + "/characters/delete", "POST");
        request.uploadHandler = new UploadHandlerRaw(Encoding.UTF8.GetBytes(json));
        request.downloadHandler = new DownloadHandlerBuffer();
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("Authorization", "Bearer " + jwtToken);

        yield return request.SendWebRequest();

        string responseText = request.downloadHandler.text;

        if (request.result != UnityWebRequest.Result.Success)
        {
            SetStatus($"Delete HTTP Error\nCode: {request.responseCode}\n{responseText}");
            yield break;
        }

        CommonResponse response = JsonUtility.FromJson<CommonResponse>(responseText);
        if (response == null)
        {
            SetStatus("Delete parse failed.");
            yield break;
        }

        if (response.result != "success")
        {
            SetStatus($"Delete failed: {response.message}");
            yield break;
        }

        SetStatus("Character deleted.");
        yield return LoadCharacterListCoroutine();
    }

    private void OnClickEnterWorld()
    {
        if (selectedCharacter == null)
        {
            SetStatus("Please select a character first.");
            return;
        }

        PlayerPrefs.SetInt("selected_character_id", selectedCharacter.id);
        PlayerPrefs.Save();

        SetStatus($"Entering world with {selectedCharacter.name}...");
        SceneManager.LoadScene("GameScene");
    }

    private void OnClickLogout()
    {
        PlayerPrefs.DeleteKey("jwt_token");
        PlayerPrefs.DeleteKey("username");
        PlayerPrefs.DeleteKey("user_id");
        PlayerPrefs.DeleteKey("selected_character_id");
        PlayerPrefs.Save();

        SceneLoader.Instance.Load("LoginScene");
    }

    private void SetStatus(string message)
    {
        if (statusText != null)
            statusText.text = message;

        Debug.Log(message);
    }
}