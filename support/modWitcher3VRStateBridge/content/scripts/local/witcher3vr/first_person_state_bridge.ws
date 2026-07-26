// Witcher3VR first-person locomotion state bridge.
//
// This script never changes camera position, rotation, distance or animation.
// It publishes only one instantaneous state enum through a reversible FOV tag;
// dxgi.dll removes the tag before REDengine rebuilds the render view.
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

function W3VR_RestoreStateBridgeFov(camera: CCustomCamera) {
  if (!camera) {
    return;
  }

  // Marker base 4096, state stride 128, native FOV retained in the remainder.
  if (camera.fov >= 5120.0f && camera.fov < 5248.0f) {
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
  }
}

function W3VR_PublishStateBridgeFov(camera: CCustomCamera, stateCode: int) {
  if (!camera || stateCode < 1 || stateCode > 8) {
    return;
  }

  W3VR_RestoreStateBridgeFov(camera);
  if (stateCode == 1) {
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
  } else {
    camera.fov += 5120.0f;
  }
}

function W3VR_GetFootState(player: CR4Player): int {
  if (player.GetIsSprinting()) {
    return 4;
  }
  if (player.GetIsRunning()) {
    return 3;
  }
  if (player.GetIsWalking()) {
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
  var camera: CCustomCamera;

  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  W3VR_RestoreStateBridgeFov(camera);

  result = wrappedMethod(moveData, dt);

  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  W3VR_PublishStateBridgeFov(camera, W3VR_GetFootState(this));
  return result;
}

@wrapMethod(HorseRiding)
function OnGameCameraPostTick(
  out moveData: SCameraMovementData,
  dt: float
) {
  var result: bool;
  var camera: CCustomCamera;
  var horse: W3HorseComponent;

  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  W3VR_RestoreStateBridgeFov(camera);

  result = wrappedMethod(moveData, dt);

  horse = (W3HorseComponent)vehicle;
  camera = (CCustomCamera)theCamera.GetTopmostCameraObject();
  W3VR_PublishStateBridgeFov(camera, W3VR_GetHorseState(horse));
  return result;
}
