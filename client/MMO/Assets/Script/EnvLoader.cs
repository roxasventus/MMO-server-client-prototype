using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public static class EnvLoader
{
    private static Dictionary<string, string> values;

    public static string Get(string key, string defaultValue = "")
    {
        if (values == null)
        {
            Load();
        }

        if (values.TryGetValue(key, out string value))
        {
            return value;
        }

        return defaultValue;
    }

    private static void Load()
    {
        values = new Dictionary<string, string>();

        string projectRootPath = Directory.GetParent(Application.dataPath).FullName;
        string path = Path.Combine(projectRootPath, "unity.env");

        if (!File.Exists(path))
        {
            Debug.LogWarning($".env file not found: {path}");
            return;
        }

        foreach (string line in File.ReadAllLines(path))
        {
            string trimmed = line.Trim();

            if (string.IsNullOrEmpty(trimmed))
            {
                continue;
            }

            if (trimmed.StartsWith("#"))
            {
                continue;
            }

            int index = trimmed.IndexOf('=');

            if (index <= 0)
            {
                continue;
            }

            string key = trimmed.Substring(0, index).Trim();
            string value = trimmed.Substring(index + 1).Trim();

            values[key] = value;
        }
    }
}