// Witcher3VR first-person locomotion state bridge.
//
// This script never changes camera position, rotation, distance or animation.
// It publishes one instantaneous state enum, or the dedicated zero code while
// combat is active, through a reversible FOV tag. Publication happens only on
// a state edge; dxgi.dll removes the tag before REDengine rebuilds the render
// view and retains the last decoded state.
//
// State codes:
//   1 foot idle
//   2 foot slow walk
//   3 foot normal movement
//   4 foot sprint
//   5 horse idle
//   6 horse walk
//   7 horse trot
//   8 horse gallop/canter
//   9 foot indoor walk (running is disabled by the interior)
//  10 unsupported first-person locomotion (swimming/diving)
//   0 combat (locomotion is irrelevant after the DLL leaves first person)

@addField(CR4Player)
var w3vr_state_bridge_last_code: int;

@addField(CR4Player)
var w3vr_state_bridge_last_combat: bool;

@addField(CR4Player)
var w3vr_state_bridge_initialized: bool;

@addField(CR4Player)
var w3vr_state_bridge_tag_pending: bool;

@addField(CR4Player)
var w3vr_native_scene_seeded: bool;

function W3VR_RestoreStateBridgeFov(camera: CCustomCamera) {
  if (!camera) {
    return;
  }

  // Marker base 4096, state stride 128, native FOV retained in the remainder.
  if (camera.fov >= 5376.0f && camera.fov < 5504.0f) {
    camera.fov -= 5376.0f;
  } else if (camera.fov >= 5248.0f && camera.fov < 5376.0f) {
    camera.fov -= 5248.0f;
  } else if (camera.fov >= 5120.0f && camera.fov < 5248.0f) {
    camera.fov -= 5120.0f;
  } else if (camera.fov >= 4992.0f && camera.fov < 5120.0f) {
    camera.fov -= 4992.0f;
  } else if (camera.fov >= 4864.0f && camera.fov < 4992.0f) {
    camera.fov -= 4864.0f;
  } else if (camera.fov >= 4736.0f && camera.fov < 4864.0f) {
    camera.fov -= 4736.0f;
  } else if (camera.fov >= 4608.0f && camera.fov < 4736.0f) {
    camera.fov -= 4608.0f;
  } else if (camera.fov >= 4480.0f && camera.fov < 4608.0f) {
    camera.fov -= 4480.0f;
  } else if (camera.fov >= 4352.0f && camera.fov < 4480.0f) {
    camera.fov -= 4352.0f;
  } else if (camera.fov >= 4224.0f && camera.fov < 4352.0f) {
    camera.fov -= 4224.0f;
  } else if (camera.fov >= 4096.0f && camera.fov < 4224.0f) {
    camera.fov -= 4096.0f;
  }
}

function W3VR_PublishStateBridgeFov(
  camera: CCustomCamera,
  stateCode: int,
  inCombat: bool
) {
  if (!camera || stateCode < 1 || stateCode > 10) {
    return;
  }

  W3VR_RestoreStateBridgeFov(camera);
  if (inCombat) {
    // Keep the original V705 marker layout; states 9 and 10 extend it for
    // walk-only interiors and unsupported first-person locomotion.
    camera.fov += 4096.0f;
  } else if (stateCode == 1) {
    camera.fov += 4224.0f;
  } else if (stateCode == 2) {
    camera.fov += 4352.0f;
  } else if (stateCode == 3) {
    camera.fov += 4480.0f;
  } else if (stateCode == 4) {
    camera.fov += 4608.0f;
  } else if (stateCode == 5) {
    camera.fov += 4736.0f;
  } else if (stateCode == 6) {
    camera.fov += 4864.0f;
  } else if (stateCode == 7) {
    camera.fov += 4992.0f;
  } else if (stateCode == 8) {
    camera.fov += 5120.0f;
  } else if (stateCode == 9) {
    camera.fov += 5248.0f;
  } else {
    camera.fov += 5376.0f;
  }
}

function W3VR_RestorePendingStateBridgeFov(player: CR4Player) {
  var camera: CCustomCamera;

  if (!player || !player.w3vr_state_bridge_tag_pending) {
    return;
  }

  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  if (!camera) {
    return;
  }

  W3VR_RestoreStateBridgeFov(camera);
  player.w3vr_state_bridge_tag_pending = false;
}

function W3VR_PublishStateBridgeChange(
  player: CR4Player,
  stateCode: int,
  inCombat: bool
) {
  var camera: CCustomCamera;

  if (!player || stateCode < 1 || stateCode > 10) {
    return;
  }
  if (
    player.w3vr_state_bridge_initialized &&
    player.w3vr_state_bridge_last_code == stateCode &&
    player.w3vr_state_bridge_last_combat == inCombat
  ) {
    return;
  }

  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  if (!camera) {
    return;
  }

  W3VR_PublishStateBridgeFov(camera, stateCode, inCombat);
  player.w3vr_state_bridge_last_code = stateCode;
  player.w3vr_state_bridge_last_combat = inCombat;
  player.w3vr_state_bridge_initialized = true;
  player.w3vr_state_bridge_tag_pending = true;
}

function W3VR_GetFootState(player: CR4Player): int {
  // Do not let swimming/diving inherit the retained ordinary foot-idle state.
  // The DLL uses this dedicated edge only to suppress the snap-turn basis fix.
  if (player.IsSwimming() || player.OnCheckDiving()) {
    return 10;
  }
  if (player.GetIsSprinting()) {
    return 4;
  }
  if (player.GetIsRunning()) {
    return 3;
  }
  if (player.GetIsWalking()) {
    // [FEATURE:FIRST-PERSON-INDOOR-WALK 1/2] REDengine explicitly locks
    // running in walk-only interiors. Publish a distinct state so the DLL can
    // remove the outdoor forward offset without affecting normal walking.
    if (
      player.IsInInterior() &&
      !player.IsActionAllowed(EIAB_RunAndSprint)
    ) {
      return 9;
    }
    return 2;
  }
  return 1;
}

function W3VR_GetHorseState(horse: W3HorseComponent): int {
  var speed: float;

  if (!horse) {
    return 5;
  }

  speed = horse.InternalGetSpeed();
  if (speed <= 0.10f) {
    return 5;
  }
  if (speed < 1.50f) {
    return 6;
  }
  if (speed < 2.50f) {
    return 7;
  }
  return 8;
}

@wrapMethod(CR4Player)
function OnGameCameraTick(out moveData: SCameraMovementData, dt: float) {
  var result: bool;
  var nativeNonGameplayCutscene: bool;
  var nativeGlobalNonGameplayScene: bool;

  // [FIX:FIRST-PERSON-STATE-FLOOD 1/2] Clear the previous one-shot marker,
  // then leave the camera untouched until locomotion or combat actually
  // changes. HorseRiding owns publication while mounted.
  W3VR_RestorePendingStateBridgeFov(this);

  result = wrappedMethod(moveData, dt);

  // Seed the two native objects once. The DLL subsequently reads their two
  // scene bytes directly; this causes no camera or FOV write.
  if (!this.w3vr_native_scene_seeded) {
    nativeNonGameplayCutscene = this.IsInNonGameplayCutscene();
    nativeGlobalNonGameplayScene =
      theGame.IsCurrentlyPlayingNonGameplayScene();
    this.w3vr_native_scene_seeded = true;
  }

  if (!this.GetHorseCurrentlyMounted()) {
    W3VR_PublishStateBridgeChange(
      this,
      W3VR_GetFootState(this),
      this.IsInCombat()
    );
  }
  return result;
}

@wrapMethod(HorseRiding)
function OnGameCameraPostTick(
  out moveData: SCameraMovementData,
  dt: float
) {
  var result: bool;
  var horse: W3HorseComponent;

  W3VR_RestorePendingStateBridgeFov(parent);

  result = wrappedMethod(moveData, dt);

  horse = (W3HorseComponent)vehicle;
  W3VR_PublishStateBridgeChange(
    parent,
    W3VR_GetHorseState(horse),
    parent.IsInCombat()
  );
  return result;
}
