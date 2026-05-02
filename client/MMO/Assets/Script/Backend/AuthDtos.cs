using System;

[Serializable]
public class AuthRequest
{
    public string username;
    public string password;
}

[Serializable]
public class AuthResponse
{
    public string result;
    public string message;
    public int user_id;
    public string username;
    public string token;
}

[Serializable]
public class MeResponse
{
    public string result;
    public string message;
    public int user_id;
    public string username;
}