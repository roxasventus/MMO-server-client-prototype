using System;

[Serializable]
public class WorldPlayersResponse
{
    public string result;
    public string message;
    public WorldCharacterData[] players;
}