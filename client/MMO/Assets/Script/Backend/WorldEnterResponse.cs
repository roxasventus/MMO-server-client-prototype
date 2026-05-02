using System;

[Serializable]
public class WorldCharacterData
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

    public int attack;
    public int defense;

    public float pos_x;
    public float pos_y;
    public float pos_z;
    public float yaw;

    public string created_at;
    public string updated_at;
}

[Serializable]
public class WorldEnterResponse
{
    public string result;
    public string message;
    public WorldCharacterData character;
    public WorldCharacterData[] nearby_players;
}