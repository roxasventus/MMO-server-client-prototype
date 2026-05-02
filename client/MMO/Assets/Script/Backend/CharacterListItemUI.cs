using System;
using TMPro;
using UnityEngine;
using UnityEngine.UI;

public class CharacterListItemUI : MonoBehaviour
{
    [SerializeField] private TMP_Text nameText;
    [SerializeField] private TMP_Text levelText;
    [SerializeField] private GameObject selectedMark;
    [SerializeField] private Image backgroundImage;
    [SerializeField] private Button button;

    private CharacterData characterData;
    private Action<CharacterData, CharacterListItemUI> onClick;

    public CharacterData CharacterData => characterData;

    public void Setup(CharacterData data, Action<CharacterData, CharacterListItemUI> clickCallback)
    {
        characterData = data;
        onClick = clickCallback;

        if (nameText != null)
            nameText.text = data.name;

        if (levelText != null)
            levelText.text = $"Lv. {data.level}";

        if (button != null)
        {
            button.onClick.RemoveAllListeners();
            button.onClick.AddListener(HandleClick);
        }

        SetSelected(false);
    }

    public void SetSelected(bool isSelected)
    {
        if (selectedMark != null)
            selectedMark.SetActive(isSelected);

        if (backgroundImage != null)
        {
            backgroundImage.color = isSelected
                ? new Color(0.85f, 0.9f, 1f, 1f)
                : Color.white;
        }
    }

    private void HandleClick()
    {
        onClick?.Invoke(characterData, this);
    }
}