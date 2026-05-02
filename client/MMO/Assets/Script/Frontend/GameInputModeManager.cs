using System;
using UnityEngine;

public class GameInputModeManager : MonoBehaviour
{
    public static GameInputModeManager Instance { get; private set; }

    public bool IsUIFocused { get; private set; }
    public bool IsChatting { get; private set; }

    public event Action<bool> OnUIModeChanged;
    public event Action<bool> OnChatModeChanged;

    private void Awake()
    {
        Instance = this;
        SetGameplayMode();
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.LeftAlt) || Input.GetKeyDown(KeyCode.RightAlt))
        {
            if (IsUIFocused)
                SetGameplayMode();
            else
                SetUIMode();
        }

        if (Input.GetKeyDown(KeyCode.Escape))
        {
            SetGameplayMode();
        }
    }

    public void SetChatMode()
    {
        IsChatting = true;
        SetUIMode();
        OnChatModeChanged?.Invoke(true);
    }

    public void SetUIMode()
    {
        IsUIFocused = true;

        Cursor.lockState = CursorLockMode.None;
        Cursor.visible = true;

        OnUIModeChanged?.Invoke(true);
    }

    public void SetGameplayMode()
    {
        IsChatting = false;
        IsUIFocused = false;

        Cursor.lockState = CursorLockMode.Locked;
        Cursor.visible = false;

        OnChatModeChanged?.Invoke(false);
        OnUIModeChanged?.Invoke(false);
    }
}