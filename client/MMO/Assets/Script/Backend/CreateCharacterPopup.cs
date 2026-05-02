using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class CreateCharacterPopup : MonoBehaviour
{
    [SerializeField] private TMP_InputField nameInputField;
    [SerializeField] private TMP_Text messageText;
    [SerializeField] private Button createButton;
    [SerializeField] private Button cancelButton;

    private CharacterSelectUIManager owner;

    public void Initialize(CharacterSelectUIManager uiManager)
    {
        owner = uiManager;

        if (createButton != null)
        {
            createButton.onClick.RemoveAllListeners();
            createButton.onClick.AddListener(OnClickCreate);
        }

        if (cancelButton != null)
        {
            cancelButton.onClick.RemoveAllListeners();
            cancelButton.onClick.AddListener(Close);
        }

        ClearUI();
    }

    public void Open()
    {
        gameObject.SetActive(true);
        ClearUI();

        if (nameInputField != null)
            nameInputField.ActivateInputField();
    }

    public void Close()
    {
        gameObject.SetActive(false);
    }

    public void SetMessage(string message)
    {
        if (messageText != null)
            messageText.text = message;
    }

    public string GetCharacterName()
    {
        if (nameInputField == null)
            return string.Empty;

        return nameInputField.text.Trim();
    }

    private void OnClickCreate()
    {
        if (owner == null)
            return;

        owner.RequestCreateCharacterFromPopup();
    }

    private void ClearUI()
    {
        if (nameInputField != null)
            nameInputField.text = string.Empty;

        if (messageText != null)
            messageText.text = string.Empty;
    }
}