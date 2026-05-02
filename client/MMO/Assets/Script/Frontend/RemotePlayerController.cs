using UnityEngine;

public class RemotePlayerController : MonoBehaviour
{
    [SerializeField] private float positionLerpSpeed = 10f;
    [SerializeField] private float rotationLerpSpeed = 10f;

    private Vector3 targetPosition;
    private float targetYaw;

    private void Awake()
    {
        targetPosition = transform.position;
        targetYaw = transform.eulerAngles.y;
    }

    private void Update()
    {
        transform.position = Vector3.Lerp(
            transform.position,
            targetPosition,
            positionLerpSpeed * Time.deltaTime
        );

        Quaternion targetRotation = Quaternion.Euler(0f, targetYaw, 0f);
        transform.rotation = Quaternion.Lerp(
            transform.rotation,
            targetRotation,
            rotationLerpSpeed * Time.deltaTime
        );
    }

    public void SetTargetPosition(Vector3 newPosition)
    {
        targetPosition = newPosition;
    }

    public void SnapToPosition(Vector3 newPosition)
    {
        transform.position = newPosition;
        targetPosition = newPosition;
    }

    public void SetTargetYaw(float yaw)
    {
        targetYaw = yaw;
    }

    public void SnapYaw(float yaw)
    {
        targetYaw = yaw;
        transform.rotation = Quaternion.Euler(0f, yaw, 0f);
    }
}