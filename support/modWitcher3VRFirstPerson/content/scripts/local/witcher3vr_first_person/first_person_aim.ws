// Witcher3VR projectile/reticle convergence and headset arm IK.
//
// REDengine keeps its native aiming state, crosshair, weapon animation and
// projectile code. During the real PlayerAiming/Aiming state, this wrapper
// replaces only the free-camera look-at point with the latest complete
// cyclopean headset ray published by the matching DLL. Actor-target look-at
// remains untouched, and every older or missing DLL fails closed.

@addField(CR4Player)
var w3vr_fp_native_aim_scene_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_aim_origin_sink: Vector;

@addField(CR4Player)
var w3vr_fp_native_aim_target_sink: Vector;

@addField(CR4Player)
var w3vr_fp_native_aim_reticle_transport_sink: bool;

@addMethod(CR4Player)
function W3VR_FP_ResolveAimConvergence(
  rayOrigin: Vector,
  farTarget: Vector
) : Vector {
  var direction: Vector;
  var traceStart: Vector;
  var hitPosition: Vector;
  var hitNormal: Vector;
  var hitMaterial: name;
  var hitComponent: CComponent;
  var collisionGroups: array<name>;
  var tracePass: int;

  direction = VecNormalize(farTarget - rayOrigin);
  // Begin just in front of the eyes. If Geralt is still intersected, advance
  // past only his own shapes and trace again; never globally disable Character
  // collisions, because the sight ray must converge on nearby actors too.
  traceStart = rayOrigin + direction * 0.20f;
  collisionGroups.PushBack('Ragdoll');
  collisionGroups.PushBack('Static');
  collisionGroups.PushBack('Terrain');
  collisionGroups.PushBack('Character');
  collisionGroups.PushBack('Destructible');

  for (tracePass = 0; tracePass < 4; tracePass += 1) {
    hitComponent = NULL;
    if (!theGame.GetWorld().StaticTraceWithAdditionalInfo(
          traceStart,
          farTarget,
          hitPosition,
          hitNormal,
          hitMaterial,
          hitComponent,
          collisionGroups)) {
      return farTarget;
    }
    if (!hitComponent || hitComponent.GetEntity() != this) {
      return hitPosition;
    }
    traceStart = hitPosition + direction * 0.05f;
  }

  // A pathological sequence of self hits must fail to the proven 100 m ray,
  // never leave the weapon pointing at Geralt.
  return farTarget;
}

@addMethod(CR4Player)
function W3VR_FP_AcquireAimConvergence(out target: Vector) : bool {
  var firstHeadIndex: int;
  var confirmedHeadIndex: int;
  var ignoredStrafeHandshake: int;
  var ignoredVisibilityHandshake: int;
  var aimHandshake: int;
  var projectedX: float;
  var projectedY: float;

  // This helper is called both while updating arm IK and at the exact bolt
  // release boundary. Five consecutive reads grant one origin+far-point pair
  // only from the matching V9526 DLL; every older combination fails closed.
  this.w3vr_fp_native_aim_scene_sink =
    this.IsInNonGameplayCutscene();
  firstHeadIndex = this.GetHeadBoneIndex();
  confirmedHeadIndex = this.GetHeadBoneIndex();
  ignoredStrafeHandshake = this.GetHeadBoneIndex();
  ignoredVisibilityHandshake = this.GetHeadBoneIndex();
  aimHandshake = this.GetHeadBoneIndex();

  if (this.w3vr_fp_native_aim_scene_sink ||
      aimHandshake != -9526 ||
      firstHeadIndex < 0 ||
      confirmedHeadIndex != firstHeadIndex) {
    return false;
  }

  this.w3vr_fp_native_aim_origin_sink =
    this.GetBoneWorldPositionByIndex(confirmedHeadIndex);
  target = this.GetBoneWorldPositionByIndex(confirmedHeadIndex);
  target = this.W3VR_FP_ResolveAimConvergence(
    this.w3vr_fp_native_aim_origin_sink,
    target
  );
  // The matching DLL consumes this otherwise ordinary projection query as a
  // one-shot reverse transport. It captures the already-resolved world point
  // and publishes only its cyclopean distance to the retained-HUD compositor.
  // Older DLLs simply perform the harmless native projection and ignore it.
  this.w3vr_fp_native_aim_reticle_transport_sink =
    theCamera.WorldVectorToViewRatio(target, projectedX, projectedY);
  return true;
}

@wrapMethod(CR4Player)
function UpdateLookAtTarget() {
  var orientationTarget: EOrientationTarget;
  var headsetTarget: Vector;

  wrappedMethod();

  if (this.playerAiming.GetCurrentStateName() != 'Aiming') {
    return;
  }
  orientationTarget = this.GetOrientationTarget();
  if (orientationTarget != OT_Camera &&
      orientationTarget != OT_CameraOffset) {
    return;
  }

  if (!this.W3VR_FP_AcquireAimConvergence(headsetTarget)) {
    return;
  }

  this.w3vr_fp_native_aim_target_sink = headsetTarget;
  this.SetLookAtPosition(this.w3vr_fp_native_aim_target_sink);
  // V9522 proved that SetLookAtPosition alone updates the projectile target
  // but leaves the animation graph on the old over-shoulder target. Mirror
  // REDengine's normal paired update so lookAtTarget/lookAtTarget2 and their
  // yaw/pitch variables drive Geralt's arm toward the same HMD ray.
  this.UpdateLookAtVariables(
    1.0f,
    this.w3vr_fp_native_aim_target_sink
  );
}

@wrapMethod(W3BoltProjectile)
function ThrowProjectile(targetPosIn: Vector) {
  var ownerPlayer: CR4Player;
  var orientationTarget: EOrientationTarget;
  var aimingState: name;
  var headsetTarget: Vector;
  var effectiveTarget: Vector;

  effectiveTarget = targetPosIn;

  ownerPlayer = (CR4Player)this.GetOwner();
  if (ownerPlayer) {
    aimingState = ownerPlayer.playerAiming.GetCurrentStateName();
    orientationTarget = ownerPlayer.GetOrientationTarget();
    if ((aimingState == 'Aiming' || aimingState == 'Waiting') &&
        (orientationTarget == OT_Camera ||
         orientationTarget == OT_CameraOffset) &&
        ownerPlayer.W3VR_FP_AcquireAimConvergence(headsetTarget)) {
      // OnProcessThrowEvent normally derives targetPosIn from the persistent
      // player look-at and may already have switched to Waiting or clamped the
      // far point around Geralt's root. Replace it only here, immediately
      // before W3AdvancedProjectile builds CFixedTarget and launch velocity.
      effectiveTarget = headsetTarget;
    }
  }

  // REDscript's wrapper expander requires one wrappedMethod call site.
  wrappedMethod(effectiveTarget);
}
