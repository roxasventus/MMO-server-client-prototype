using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;

public class ChatInputFocusHandler : MonoBehaviour
{
    [SerializeField] private TMP_InputField chatInputField;

    private void Awake()
    {
        if (chatInputField != null)
        {
            chatInputField.onSelect.AddListener(_ => EnterChatMode());
            //chatInputField.onDeselect.AddListener(_ => ExitChatMode());
        }
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.Return))
        {
            if (GameInputModeManager.Instance != null &&
                GameInputModeManager.Instance.IsChatting)
            {
                ExitChatMode();
                EventSystem.current.SetSelectedGameObject(null);
            }
            else
            {
                EnterChatMode();
                chatInputField.Select();
                chatInputField.ActivateInputField();
            }
        }

        if (Input.GetKeyDown(KeyCode.Escape))
        {
            ExitChatMode();
            EventSystem.current.SetSelectedGameObject(null);
        }
    }

    private void EnterChatMode()
    {
        GameInputModeManager.Instance?.SetChatMode();
    }

    private void ExitChatMode()
    {
        GameInputModeManager.Instance?.SetGameplayMode();
    }
}