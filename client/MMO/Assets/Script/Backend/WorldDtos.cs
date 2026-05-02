[System.Serializable]
public class WorldMoveRequest
{
    public int character_id;
    public float pos_x;
    public float pos_y;
    public float pos_z;
    public float yaw;
}

[System.Serializable]
public class WorldMoveResponse
{
    public string result;
    public string message;
    public int character_id;
    public float pos_x;
    public float pos_y;
    public float pos_z;
    public float yaw;
}