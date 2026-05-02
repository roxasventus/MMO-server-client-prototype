using TMPro;
using UnityEngine;

public class ChatMessageItem : MonoBehaviour
{
    [SerializeField] private TMP_Text messageText;

    public void SetMessage(string username, string message)
    {
        messageText.text = $"[{username}] {message}";
    }
}