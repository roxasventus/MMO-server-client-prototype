using System;

[Serializable]
public class CharacterData
{
    public int id;
    public int user_id;
    public string name;

    public int level;
    public int exp;
    public int gold;

    public int hp;
    public int max_hp;
    public int mp;
    public int max_mp;

    public int attack_power;
    public int defense;

    public float pos_x;
    public float pos_y;
    public float pos_z;

    public float yaw;

    public string created_at;
    public string updated_at;
}

[Serializable]
public class CharacterListResponse
{
    public string result;
    public string message;
    public int user_id;
    public string username;
    public CharacterData[] characters;
}

[Serializable]
public class CreateCharacterRequest
{
    public string name;
}

[Serializable]
public class CreateCharacterResponse
{
    public string result;
    public string message;
    public CharacterData character;
}

[Serializable]
public class DeleteCharacterRequest
{
    public int character_id;
}

[Serializable]
public class CommonResponse
{
    public string result;
    public string message;
}


