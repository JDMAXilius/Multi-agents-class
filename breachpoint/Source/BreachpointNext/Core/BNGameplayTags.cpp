#include "BNGameplayTags.h"

namespace BNTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look, "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(Input_Jump, "Input.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Next, "Input.Weapon.Next");
	UE_DEFINE_GAMEPLAY_TAG(Input_Weapon_Previous, "Input.Weapon.Previous");
	UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Input_Lean_Left, "Input.Lean.Left");
	UE_DEFINE_GAMEPLAY_TAG(Input_Lean_Right, "Input.Lean.Right");

	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Jumping, "State.Movement.Jumping");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_InAir, "State.Movement.InAir");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Crouching, "State.Movement.Crouching");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Sprinting, "State.Movement.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Lean_Left, "State.Lean.Left");
	UE_DEFINE_GAMEPLAY_TAG(State_Lean_Right, "State.Lean.Right");
}
