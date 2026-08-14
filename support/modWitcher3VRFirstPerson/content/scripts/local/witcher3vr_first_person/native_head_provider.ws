// Witcher3VR native head-position and first-person capability provider.
//
// This script does not create or activate a camera and does not modify the
// current camera, input, FOV, clipping or player transform. It requests
// Geralt's final animated head position during PostUpdateTransform and carries
// fail-closed DLL heartbeats for strafe, stationary rotation, player
// visibility and headset aiming. Without the matching DLL all remain dormant.

@addField(CR4Player)
var w3vr_fp_native_head_position_sink: Vector;

@addField(CR4Player)
var w3vr_fp_native_scene_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_strafe_heartbeat_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_stationary_turn_heartbeat_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_visibility_heartbeat_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_combat_probe_sink: bool;

@addField(CR4Player)
var w3vr_fp_native_combat_target_sink: Vector;

@wrapMethod(CR4Game)
function OnGameStarted(restored: bool) {
  wrappedMethod(restored);

  if (thePlayer) {
    thePlayer.W3VR_FP_StartNativeHeadProvider();
    thePlayer.W3VR_FP_StartOnFootStrafe();
    thePlayer.W3VR_FP_StartPlayerVisibilityClip();
  }
}

@addMethod(CR4Player)
function W3VR_FP_StartNativeHeadProvider() {
  // Reinstalling the same named timer replaces it, so repeated game-start
  // notifications cannot create multiple publishers on the player.
  this.AddTimer(
    'W3VR_FP_UpdateNativeHeadPosition',
    0.0f,
    true,
    false,
    TICK_PostUpdateTransform,
    false,
    true
  );
}

@addMethod(CR4Player)
timer function W3VR_FP_UpdateNativeHeadPosition(dt: float, id: int) {
  var firstHeadIndex: int;
  var confirmedHeadIndex: int;
  var strafeHandshake: int;
  var visibilityHandshake: int;
  var targetHeadIndex: int;
  var targetTorsoIndex: int;
  var combatTarget: CActor;

  // This first native read preserves the V9500 scene/player seed. The pose
  // handshake remains two matching head-index reads. The third read is the
  // strafe capability challenge; the fourth is the independent Geralt-only
  // roll/dodge visibility challenge. Neither is used as a bone index.
  this.w3vr_fp_native_scene_sink = this.IsInNonGameplayCutscene();
  this.w3vr_fp_native_strafe_heartbeat_sink = false;
  this.w3vr_fp_native_stationary_turn_heartbeat_sink = false;
  this.w3vr_fp_native_visibility_heartbeat_sink = false;
  firstHeadIndex = this.GetHeadBoneIndex();
  confirmedHeadIndex = this.GetHeadBoneIndex();
  strafeHandshake = this.GetHeadBoneIndex();
  visibilityHandshake = this.GetHeadBoneIndex();

  // V9529 encodes the two independent third-read permissions without adding
  // another native callback: -9529 is strafe only, -19529 stationary turn
  // only, and -29529 both. Every canonical, older, or mismatched DLL returns
  // the ordinary positive head index and therefore fails closed.
  this.w3vr_fp_native_strafe_heartbeat_sink =
    strafeHandshake == -9529 || strafeHandshake == -29529;
  this.w3vr_fp_native_stationary_turn_heartbeat_sink =
    strafeHandshake == -19529 || strafeHandshake == -29529;
  this.w3vr_fp_native_visibility_heartbeat_sink =
    visibilityHandshake == -9526;
  if (firstHeadIndex < 0 || confirmedHeadIndex != firstHeadIndex) {
    this.W3VR_FP_UpdatePlayerVisibilityClip();
    return;
  }
  this.w3vr_fp_native_head_position_sink =
    this.GetBoneWorldPositionByIndex(confirmedHeadIndex);
  this.W3VR_FP_UpdatePlayerVisibilityClip();

  // This second scene query is an observation-only boundary marker. The
  // matching DLL accepts the immediately following target-bone matrix only
  // when the native hard lock is genuinely active. Older DLLs simply execute
  // the ordinary getters and ignore the result.
  this.w3vr_fp_native_combat_probe_sink = this.IsInNonGameplayCutscene();
  if (!this.IsHardLockEnabled()) {
    return;
  }
  combatTarget = this.GetTarget();
  if (!combatTarget) {
    return;
  }
  targetHeadIndex = combatTarget.GetHeadBoneIndex();
  targetTorsoIndex = combatTarget.GetTorsoBoneIndex();
  if (targetTorsoIndex >= 0) {
    this.w3vr_fp_native_combat_target_sink =
      combatTarget.GetBoneWorldPositionByIndex(targetTorsoIndex);
  } else if (targetHeadIndex >= 0) {
    this.w3vr_fp_native_combat_target_sink =
      combatTarget.GetBoneWorldPositionByIndex(targetHeadIndex);
  }
}
