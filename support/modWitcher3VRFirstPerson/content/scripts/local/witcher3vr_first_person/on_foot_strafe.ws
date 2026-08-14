// Witcher3VR clean-room on-foot strafe/backpedal and stationary body rotation.
//
// This script has no independent enable path. A one-tick sentinel heartbeat
// from the matching DLL is consumed before every update; all other versions
// fail closed and leave vanilla movement untouched. Strafe deliberately covers
// only ordinary Exploration/Idle locomotion--not combat, vehicles, swimming,
// scripted actions, jumps, dodges, rolls, or finishers. V9529 keeps the
// stationary body-facing request as an always-on part of F11 gameplay; its DLL
// heartbeat is a state/ownership safety token, not a user option.

@addField(CR4Player)
var w3vr_fp_strafe_gate_consumed: bool;

@addField(CR4Player)
var w3vr_fp_strafe_requests_active: bool;

@addField(CR4Player)
var w3vr_fp_stationary_turn_gate_consumed: bool;

@addField(CR4Player)
var w3vr_fp_stationary_turn_request_active: bool;

@addField(CR4Player)
var w3vr_fp_stationary_turn_window_active: bool;

@addField(CR4Player)
var w3vr_fp_stationary_turn_slow_until: float;

@addMethod(CR4Player)
function W3VR_FP_StartOnFootStrafe() {
  this.w3vr_fp_native_strafe_heartbeat_sink = false;
  this.w3vr_fp_strafe_gate_consumed = false;
  this.w3vr_fp_strafe_requests_active = false;
  this.w3vr_fp_native_stationary_turn_heartbeat_sink = false;
  this.w3vr_fp_stationary_turn_gate_consumed = false;
  this.w3vr_fp_stationary_turn_request_active = false;
  this.w3vr_fp_stationary_turn_window_active = false;
  this.w3vr_fp_stationary_turn_slow_until = 0.0f;

  // PrePhysics is the ordinary locomotion phase and is intentionally distinct
  // from the PostUpdateTransform publisher. Reinstalling this named timer is
  // idempotent on repeated game-start notifications.
  this.AddTimer(
    'W3VR_FP_UpdateOnFootStrafe',
    0.0f,
    true,
    false,
    TICK_PrePhysics,
    false,
    true
  );
}

@addMethod(CR4Player)
function W3VR_FP_CancelOnFootStrafeRequests() {
  var movementAdjustor: CMovementAdjustor;

  // Never cancel engine or other-mod requests. The two names below are owned
  // exclusively by this trial. Avoid walking the adjustor every frame while
  // a canonical/mismatched DLL keeps the provider dormant.
  if (!this.w3vr_fp_strafe_requests_active) {
    return;
  }
  if (this.GetMovingAgentComponent()) {
    movementAdjustor = this.GetMovingAgentComponent().GetMovementAdjustor();
    if (movementAdjustor) {
      movementAdjustor.CancelByName('W3VR_FP_Strafe_Move');
      movementAdjustor.CancelByName('W3VR_FP_Strafe_Facing');
    }
  }
  this.w3vr_fp_strafe_requests_active = false;
}

@addMethod(CR4Player)
function W3VR_FP_CancelStationaryTurnRequest(resetWindow: bool) {
  var movementAdjustor: CMovementAdjustor;

  if (this.w3vr_fp_stationary_turn_request_active &&
      this.GetMovingAgentComponent()) {
    movementAdjustor = this.GetMovingAgentComponent().GetMovementAdjustor();
    if (movementAdjustor) {
      movementAdjustor.CancelByName('W3VR_FP_Stationary_Facing');
    }
  }
  this.w3vr_fp_stationary_turn_request_active = false;
  if (resetWindow) {
    this.w3vr_fp_stationary_turn_window_active = false;
    this.w3vr_fp_stationary_turn_slow_until = 0.0f;
  }
}

@addMethod(CR4Player)
function W3VR_FP_RefreshStationaryTurnRequest() {
  var movingAgent: CMovingAgentComponent;
  var movementAdjustor: CMovementAdjustor;
  var facingTicket: SMovementAdjustmentRequestTicket;
  var cameraHeading: float;
  var headingError: float;
  var now: float;

  movingAgent = this.GetMovingAgentComponent();
  if (!movingAgent || movingAgent.GetSpeed() > 0.05f) {
    this.W3VR_FP_CancelStationaryTurnRequest(true);
    return;
  }
  movementAdjustor = movingAgent.GetMovementAdjustor();
  if (!movementAdjustor) {
    this.W3VR_FP_CancelStationaryTurnRequest(true);
    return;
  }

  // Cancel only our previous one-shot request. If the engine or another mod
  // currently owns rotation, yield instead of reproducing GFP's CancelAll.
  movementAdjustor.CancelByName('W3VR_FP_Stationary_Facing');
  this.w3vr_fp_stationary_turn_request_active = false;
  if (movementAdjustor.HasAnyActiveRotationRequests()) {
    return;
  }

  cameraHeading = theCamera.GetCameraHeading();
  headingError = AbsF(AngleDistance(cameraHeading, this.GetHeading()));
  if (headingError <= 1.5f) {
    return;
  }

  now = theGame.GetEngineTimeAsSeconds();
  if (!this.w3vr_fp_stationary_turn_window_active) {
    this.w3vr_fp_stationary_turn_window_active = true;
    this.w3vr_fp_stationary_turn_slow_until = now + 0.5f;
  }
  facingTicket = movementAdjustor.CreateNewRequest(
    'W3VR_FP_Stationary_Facing'
  );
  if (now <= this.w3vr_fp_stationary_turn_slow_until) {
    movementAdjustor.MaxRotationAdjustmentSpeed(facingTicket, 750.0f);
  }
  movementAdjustor.RotateTo(facingTicket, cameraHeading);
  this.w3vr_fp_stationary_turn_request_active = true;
}

@addMethod(CR4Player)
function W3VR_FP_OnFootStrafeStateAllowed(): bool {
  // Treat ordinary on-foot Exploration/Idle as a whitelist, not a blacklist.
  // The explicit guards document every authority excluded from V9526 and keep
  // transitional frames closed even if the outer state has not changed yet.
  if (!theGame.IsActive() || theGame.IsPaused() ||
      theGame.GetPhotomodeEnabled()) {
    return false;
  }
  if (theGame.IsDialogOrCutscenePlaying() || theGame.IsFading() ||
      theGame.IsBlackscreen()) {
    return false;
  }
  if (this.IsInNonGameplayCutscene() || this.IsInGameplayScene() ||
      theGame.IsCurrentlyPlayingNonGameplayScene()) {
    return false;
  }
  if (!this.IsAlive() || !this.GetBIsInputAllowed() ||
      !this.GetIsMovable() || this.IsUITakeInput()) {
    return false;
  }
  if (this.GetCurrentStateName() != 'Exploration' ||
      !this.substateManager ||
      this.substateManager.GetStateCur() != 'Idle') {
    return false;
  }
  if (this.IsInCombat() || this.IsInCombatState() ||
      this.IsInCombatAction() || this.IsCurrentlyDodging()) {
    return false;
  }
  if (this.IsUsingVehicle() || this.GetHorseCurrentlyMounted() ||
      this.IsUsingBoat() || this.IsOnBoat() ||
      this.IsSwimming() || this.OnCheckDiving()) {
    return false;
  }
  if (this.GetTraverser() || this.GetPlayerAction() != PEA_None) {
    return false;
  }
  if (this.isInFinisher || this.IsCameraControlDisabled('Finisher')) {
    return false;
  }
  if (theInput.GetContext() == 'ScriptedAction' ||
      theInput.GetContext() == 'Scene' ||
      theInput.GetContext() == 'RadialMenu' ||
      theInput.GetContext() == 'QuickInventory' ||
      theInput.GetContext() == 'EMPTY_CONTEXT') {
    return false;
  }
  return true;
}

@addMethod(CR4Player)
function W3VR_FP_OnFootStrafeAllowed(): bool {
  return this.w3vr_fp_strafe_gate_consumed &&
    this.W3VR_FP_OnFootStrafeStateAllowed();
}

@addMethod(CR4Player)
function W3VR_FP_OnFootStrafeOutsideRunCone(): bool {
  var inputVector: Vector;

  inputVector.X = theInput.GetActionValue('GI_AxisLeftX');
  inputVector.Y = theInput.GetActionValue('GI_AxisLeftY');
  inputVector.Z = 0.0f;
  inputVector.W = 0.0f;
  if (AbsF(inputVector.X) < 0.001f && AbsF(inputVector.Y) < 0.001f) {
    return false;
  }
  return AbsF(VecHeading(inputVector)) > 60.0f;
}

@addMethod(CR4Player)
function W3VR_FP_RefreshOnFootStrafeRequests(
  moveHeading: float,
  facingHeading: float
) {
  var movingAgent: CMovingAgentComponent;
  var movementAdjustor: CMovementAdjustor;
  var moveTicket: SMovementAdjustmentRequestTicket;
  var facingTicket: SMovementAdjustmentRequestTicket;

  movingAgent = this.GetMovingAgentComponent();
  if (!movingAgent) {
    this.W3VR_FP_CancelOnFootStrafeRequests();
    return;
  }
  movementAdjustor = movingAgent.GetMovementAdjustor();
  if (!movementAdjustor) {
    this.W3VR_FP_CancelOnFootStrafeRequests();
    return;
  }

  // Short requests are recreated every valid tick. If script execution stops
  // unexpectedly, both expire instead of retaining movement authority.
  movementAdjustor.CancelByName('W3VR_FP_Strafe_Move');
  moveTicket = movementAdjustor.CreateNewRequest('W3VR_FP_Strafe_Move');
  movementAdjustor.Continuous(moveTicket);
  movementAdjustor.LockMovementInDirection(moveTicket, moveHeading);
  movementAdjustor.KeepActiveFor(moveTicket, 0.10f);

  movementAdjustor.CancelByName('W3VR_FP_Strafe_Facing');
  facingTicket = movementAdjustor.CreateNewRequest('W3VR_FP_Strafe_Facing');
  movementAdjustor.RotateTo(facingTicket, facingHeading);
  movementAdjustor.KeepActiveFor(facingTicket, 0.10f);
  this.w3vr_fp_strafe_requests_active = true;
}

@addMethod(CR4Player)
timer function W3VR_FP_UpdateOnFootStrafe(dt: float, id: int) {
  var inputVector: Vector;
  var inputHeading: float;
  var cameraHeading: float;

  // Consume the pulse before any early return. It cannot remain true if the
  // PostUpdateTransform provider or matching DLL stops publishing.
  this.w3vr_fp_strafe_gate_consumed =
    this.w3vr_fp_native_strafe_heartbeat_sink;
  this.w3vr_fp_native_strafe_heartbeat_sink = false;
  this.w3vr_fp_stationary_turn_gate_consumed =
    this.w3vr_fp_native_stationary_turn_heartbeat_sink;
  this.w3vr_fp_native_stationary_turn_heartbeat_sink = false;

  if (!this.W3VR_FP_OnFootStrafeStateAllowed()) {
    this.W3VR_FP_CancelOnFootStrafeRequests();
    this.W3VR_FP_CancelStationaryTurnRequest(true);
    return;
  }

  inputVector.X = theInput.GetActionValue('GI_AxisLeftX');
  inputVector.Y = theInput.GetActionValue('GI_AxisLeftY');
  inputVector.Z = 0.0f;
  inputVector.W = 0.0f;
  if (AbsF(inputVector.X) < 0.001f && AbsF(inputVector.Y) < 0.001f) {
    this.W3VR_FP_CancelOnFootStrafeRequests();
    if (this.w3vr_fp_stationary_turn_gate_consumed) {
      this.W3VR_FP_RefreshStationaryTurnRequest();
    } else {
      this.W3VR_FP_CancelStationaryTurnRequest(true);
    }
    return;
  }

  this.W3VR_FP_CancelStationaryTurnRequest(true);
  if (!this.W3VR_FP_OnFootStrafeAllowed()) {
    this.W3VR_FP_CancelOnFootStrafeRequests();
    return;
  }

  inputHeading = VecHeading(inputVector);
  cameraHeading = theCamera.GetCameraHeading();
  this.W3VR_FP_RefreshOnFootStrafeRequests(
    AngleNormalize180(cameraHeading + inputHeading),
    cameraHeading
  );
}

@wrapMethod(W3PlayerWitcher)
function CanSprint(speed: float): bool {
  var result: bool;

  result = wrappedMethod(speed);
  if (this.W3VR_FP_OnFootStrafeAllowed() &&
      this.W3VR_FP_OnFootStrafeOutsideRunCone()) {
    return false;
  }
  return result;
}

@wrapMethod(CR4LocomotionPlayerControllerScript)
function CalculateMoveSpeed(): float {
  var result: float;

  result = wrappedMethod();
  if (thePlayer && thePlayer.W3VR_FP_OnFootStrafeAllowed() &&
      thePlayer.W3VR_FP_OnFootStrafeOutsideRunCone()) {
    // Vanilla and GFP both use 0.6 for speedWalkingMax. Keep the clean-room
    // wrapper independent from the controller's package-private field.
    return MinF(result, 0.6f);
  }
  return result;
}

// GFP prevents the vanilla exploration corrector from rotating the body back
// toward the locomotion input while its camera-facing strafe owner is active.
// Without this one-frame block, the two yaw owners alternate most visibly at
// full run speed and while the mouse turns, and the head anchor exposes that
// fight as rapid left/right camera shake. PostUpdate clears this native flag.
@wrapMethod(CExplorationMovementCorrector)
function UpdateTurnAdjustment(dt: float) {
  if (thePlayer && thePlayer.W3VR_FP_OnFootStrafeAllowed()) {
    turnAdjustBlocked = true;
  }
  wrappedMethod(dt);
}

// GFP keeps the moving-agent gameplay direction on the camera-facing body and
// suppresses vanilla actorMoveDirection while its strafe owner is active.
// Vanilla writes both from the requested travel direction, so a continuous
// backward-to-forward reversal can jump their animation authority by roughly
// 180 degrees even though physical movement and body yaw remain correct. This
// post-wrapper changes only those two outputs after native locomotion math.
@wrapMethod(CR4LocomotionPlayerControllerScript)
function UpdateLocomotion() {
  wrappedMethod();
  if (thePlayer && thePlayer.W3VR_FP_OnFootStrafeAllowed()) {
    // GFP keeps the moving-agent gameplay direction aligned with the body's
    // camera-facing heading. Vanilla instead writes the requested travel
    // direction, which flips by 180 degrees during a continuous W/S reversal
    // and can make the locomotion graph alternate between turn branches.
    thePlayer.GetMovingAgentComponent().SetGameplayMoveDirection(
      thePlayer.GetHeading()
    );
    thePlayer.SetBehaviorVariable('actorMoveDirection', 0.0f);
  }
}
