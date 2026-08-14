// Witcher3VR Geralt-only roll/dodge visibility clip.
//
// This is not a camera near-plane change. It hides only Geralt's drawable
// components and his currently mounted/held equipment while an evade is in
// progress. Components already hidden by the game or another mod are never
// claimed, and only components claimed by this owner are restored.

@addField(CR4Player)
var w3vr_fp_player_clip_active: bool;

@addField(CR4Player)
var w3vr_fp_player_clip_hidden_drawables: array<CDrawableComponent>;

@addMethod(CR4Player)
function W3VR_FP_StartPlayerVisibilityClip() {
  this.W3VR_FP_RestorePlayerVisibilityClip();
  this.w3vr_fp_native_visibility_heartbeat_sink = false;
}

@addMethod(CR4Player)
function W3VR_FP_PlayerVisibilityClipWanted(): bool {
  if (!this.w3vr_fp_native_visibility_heartbeat_sink) {
    return false;
  }
  if (!theGame.IsActive() || theGame.IsPaused() ||
      theGame.GetPhotomodeEnabled()) {
    return false;
  }
  if (theGame.IsDialogOrCutscenePlaying() || theGame.IsFading() ||
      theGame.IsBlackscreen()) {
    return false;
  }
  // Do not call IsInNonGameplayCutscene here: that exact native getter is the
  // provider's challenge boundary. The DLL heartbeat already rejects Cinema;
  // these independent script guards cover scene ownership as a second layer.
  if (this.IsInGameplayScene() ||
      theGame.IsCurrentlyPlayingNonGameplayScene()) {
    return false;
  }
  if (!this.IsAlive()) {
    return false;
  }

  // IsCurrentlyDodging covers the ordinary evade path. The explicit Roll
  // state and behavior-variable checks keep the owner active across the short
  // transition frames where the high-level flag can change first.
  return this.IsCurrentlyDodging() ||
    (this.substateManager && this.substateManager.GetStateCur() == 'Roll') ||
    this.GetBehaviorVariable('isRolling') > 0.5f;
}

@addMethod(CR4Player)
function W3VR_FP_HideVisibleDrawables(entity: CEntity) {
  var components: array<CComponent>;
  var drawable: CDrawableComponent;
  var i: int;

  if (!entity) {
    return;
  }
  components = entity.GetComponentsByClassName('CDrawableComponent');
  for (i = 0; i < components.Size(); i += 1) {
    drawable = (CDrawableComponent)components[i];
    if (!drawable || drawable.GetName() == "shadow_capsule") {
      continue;
    }
    if (drawable.IsVisible()) {
      this.w3vr_fp_player_clip_hidden_drawables.PushBack(drawable);
      drawable.SetVisible(false);
    }
  }
}

@addMethod(CR4Player)
function W3VR_FP_HideMountedCategory(category: name) {
  var items: array<SItemUniqueId>;
  var itemEntity: CItemEntity;
  var i: int;

  if (!this.inv) {
    return;
  }
  items = this.inv.GetItemsByCategory(category);
  for (i = 0; i < items.Size(); i += 1) {
    if (!this.inv.IsItemMounted(items[i]) &&
        !this.inv.IsItemHeld(items[i])) {
      continue;
    }
    itemEntity = (CItemEntity)this.inv.GetItemEntityUnsafe(items[i]);
    if (itemEntity) {
      this.W3VR_FP_HideVisibleDrawables(itemEntity);
    }
  }
}

@addMethod(CR4Player)
function W3VR_FP_HidePlayerVisibilityClip() {
  if (this.w3vr_fp_player_clip_active) {
    return;
  }

  // The player entity owns the body meshes. Equipped appearance pieces and
  // weapons are separate item entities, so include only their mounted/held
  // instances to make the visual clip cover Geralt rather than the world.
  this.W3VR_FP_HideVisibleDrawables(this);
  this.W3VR_FP_HideMountedCategory('head');
  this.W3VR_FP_HideMountedCategory('hair');
  this.W3VR_FP_HideMountedCategory('mask');
  this.W3VR_FP_HideMountedCategory('hood');
  this.W3VR_FP_HideMountedCategory('armor');
  this.W3VR_FP_HideMountedCategory('gloves');
  this.W3VR_FP_HideMountedCategory('pants');
  this.W3VR_FP_HideMountedCategory('boots');
  this.W3VR_FP_HideMountedCategory('crossbow');
  this.W3VR_FP_HideMountedCategory('silversword');
  this.W3VR_FP_HideMountedCategory('steelsword');
  this.W3VR_FP_HideMountedCategory('silver_scabbards');
  this.W3VR_FP_HideMountedCategory('steel_scabbards');
  this.w3vr_fp_player_clip_active = true;
}

@addMethod(CR4Player)
function W3VR_FP_RestorePlayerVisibilityClip() {
  var drawable: CDrawableComponent;
  var i: int;

  for (i = 0; i < this.w3vr_fp_player_clip_hidden_drawables.Size(); i += 1) {
    drawable = this.w3vr_fp_player_clip_hidden_drawables[i];
    if (drawable) {
      drawable.SetVisible(true);
    }
  }
  this.w3vr_fp_player_clip_hidden_drawables.Clear();
  this.w3vr_fp_player_clip_active = false;
}

@addMethod(CR4Player)
function W3VR_FP_UpdatePlayerVisibilityClip() {
  if (this.W3VR_FP_PlayerVisibilityClipWanted()) {
    this.W3VR_FP_HidePlayerVisibilityClip();
  } else {
    this.W3VR_FP_RestorePlayerVisibilityClip();
  }

  // The DLL publishes a one-update capability pulse. Consume it here so a
  // missing/mismatched DLL, F11 exit, menu or Cinema transition fails visible.
  this.w3vr_fp_native_visibility_heartbeat_sink = false;
}
