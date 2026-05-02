using UnityEngine;

[RequireComponent(typeof(CharacterController))]
public class LocalPlayerController : MonoBehaviour
{
    [Header("Move")]
    [SerializeField] private float moveSpeed = 5f;
    [SerializeField] private float rotationSpeed = 10f;
    [SerializeField] private Transform cameraTransform;

    [Header("Jump / Gravity")]
    [SerializeField] private float gravity = -9.81f;
    [SerializeField] private float jumpHeight = 1.2f;
    [SerializeField] private float groundedStickForce = -2f;

    [Header("Ground Check")]
    [SerializeField] private Transform footPoint;
    [SerializeField] private float groundCheckRadius = 0.25f;
    [SerializeField] private LayerMask groundLayer;

    private CharacterController controller;
    private Animator animator;
    private Vector3 velocity;

    private void Awake()
    {
        controller = GetComponent<CharacterController>();
        animator = GetComponent<Animator>();


        if (animator != null)
        {
            animator.applyRootMotion = false;
        }
    }

    private void Update()
    {
        if (GameInputModeManager.Instance != null &&
            GameInputModeManager.Instance.IsUIFocused)
        {
            if (animator != null)
                animator.SetBool("isMove", false);

            bool groundedWhileUI = IsGrounded();

            if (groundedWhileUI && velocity.y < 0f)
                velocity.y = groundedStickForce;

            velocity.y += gravity * Time.deltaTime;
            controller.Move(velocity * Time.deltaTime);

            return;
        }

        bool isGrounded = IsGrounded();

        // 착지 상태에서 아래로 살짝 붙여주기
        if (isGrounded && velocity.y < 0f)
        {
            velocity.y = groundedStickForce;
        }

        float h = Input.GetAxisRaw("Horizontal");
        float v = Input.GetAxisRaw("Vertical");

        Vector3 inputDirection = new Vector3(h, 0f, v).normalized;

        Vector3 moveDirection = Vector3.zero;

        if (inputDirection.sqrMagnitude > 0.001f)
        {
            animator.SetBool("isMove", true);

            if (cameraTransform != null)
            {
                Vector3 cameraForward = cameraTransform.forward;
                Vector3 cameraRight = cameraTransform.right;

                cameraForward.y = 0f;
                cameraRight.y = 0f;

                cameraForward.Normalize();
                cameraRight.Normalize();

                moveDirection = (cameraForward * v + cameraRight * h).normalized;
            }
            else
            {
                moveDirection = inputDirection;
            }

            Quaternion targetRotation = Quaternion.LookRotation(moveDirection);
            transform.rotation = Quaternion.Slerp(
                transform.rotation,
                targetRotation,
                rotationSpeed * Time.deltaTime
            );
        }
        else {
            animator.SetBool("isMove", false);
        }

        controller.Move(moveDirection * moveSpeed * Time.deltaTime);
  

        // 점프 입력
        if (isGrounded && Input.GetButtonDown("Jump"))
        {
            Debug.Log("점프 눌림");

            velocity.y = Mathf.Sqrt(jumpHeight * -2f * gravity);

            
            if (animator != null)
            {
                animator.SetBool("isJump", true);
            }
            
        }

        // 중력 적용
        velocity.y += gravity * Time.deltaTime;
        controller.Move(velocity * Time.deltaTime);

        // 착지 중일 때, 점프 상태 해제
        if (isGrounded && velocity.y <= 0f)
        {
            animator.SetBool("isJump", false);
        }
    }

    public void SetCameraTransform(Transform targetCamera)
    {
        cameraTransform = targetCamera;
    }

    private bool IsGrounded()
    {
        return Physics.CheckSphere(footPoint.position, groundCheckRadius, groundLayer);
    }

    private void OnDrawGizmosSelected()
    {
        if (footPoint == null) return;

        Gizmos.color = Color.yellow;
        Gizmos.DrawWireSphere(footPoint.position, groundCheckRadius);
    }

    public void SetJumpAnimatorFalse() {
        animator.SetBool("isJump", false);
    }
}