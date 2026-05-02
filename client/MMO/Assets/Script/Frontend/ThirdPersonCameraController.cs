using UnityEngine;
using Unity.Cinemachine;

public class ThirdPersonCameraController : MonoBehaviour
{
    [SerializeField] private CinemachineCamera cinemachineCamera;
    [SerializeField] private float xSensitivity = 250f;
    [SerializeField] private float ySensitivity = 1.5f;

    private CinemachineOrbitalFollow orbitalFollow;

    private void Awake()
    {
        if (cinemachineCamera != null)
        {
            orbitalFollow = cinemachineCamera.GetComponent<CinemachineOrbitalFollow>();
        }

        //Cursor.lockState = CursorLockMode.Locked;
        //Cursor.visible = false;
    }

    private void Update()
    {
        if (GameInputModeManager.Instance != null && GameInputModeManager.Instance.IsUIFocused)
        {
            return;
        }

        if (orbitalFollow == null)
            return;

        float mouseX = Input.GetAxis("Mouse X");
        float mouseY = Input.GetAxis("Mouse Y");

        // 좌우 회전
        orbitalFollow.HorizontalAxis.Value += mouseX * xSensitivity * Time.deltaTime;

        // 상하 회전
        orbitalFollow.VerticalAxis.Value -= mouseY * ySensitivity * Time.deltaTime;
    }

    public void SetTarget(Transform target)
    {
        if (cinemachineCamera == null)
            return;

        cinemachineCamera.Target.TrackingTarget = target;
    }

    public Transform GetCameraTransform()
    {
        return Camera.main != null ? Camera.main.transform : null;
    }
}