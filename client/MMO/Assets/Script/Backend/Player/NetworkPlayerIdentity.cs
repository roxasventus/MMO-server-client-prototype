using TMPro;
using UnityEngine;

public class NetworkPlayerIdentity : MonoBehaviour
{
    [SerializeField] private TMP_Text nameText;

    public int CharacterId { get; private set; }
    public int UserId { get; private set; }
    public string CharacterName { get; private set; }

    public void Initialize(int characterId, int userId, string characterName)
    {
        CharacterId = characterId;
        UserId = userId;
        CharacterName = characterName;

        if (nameText != null)
        {
            nameText.text = characterName;
        }
    }
}