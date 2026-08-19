// Witcher3VR first-person head visibility and Geralt-only evade clip.
//
// This is not a camera near-plane change. While native F11 first person owns
// gameplay, one owner persistently hides Geralt's head, hair, beard and mounted
// head accessories. A separate owner keeps the validated roll/dodge behavior:
// it temporarily hides the remaining player and mounted/held equipment. Each
// owner records only drawables that were visible when it claimed them and
// restores only its own list.

@addField(CR4Player)
var w3vr_fp_player_clip_active: bool;

@addField(CR4Player)
var w3vr_fp_player_clip_hidden_drawables: array<CDrawableComponent>;

@addField(CR4Player)
var w3vr_fp_head_clip_active: bool;

@addField(CR4Player)
var w3vr_fp_head_clip_hidden_drawables: array<CDrawableComponent>;

@addField(CR4Player)
var w3vr_fp_head_clip_entities: array<CEntity>;

@addField(CR4Player)
var w3vr_fp_head_clip_next_entity_scan: float;

@addMethod(CR4Player)
function W3VR_FP_StartPlayerVisibilityClip() {
  this.W3VR_FP_RestorePlayerVisibilityClip();
  this.W3VR_FP_RestoreHeadVisibilityClip();
  this.w3vr_fp_native_visibility_heartbeat_sink = false;
}

@addMethod(CR4Player)
function W3VR_FP_VisibilityOwnerAllowed(): bool {
  if (!this.w3vr_fp_native_visibility_heartbeat_sink ||
      !this.w3vr_fp_native_first_person_active) {
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
  return this.IsAlive();
}

@addMethod(CR4Player)
function W3VR_FP_HeadVisibilityClipWanted(): bool {
  return this.W3VR_FP_VisibilityOwnerAllowed();
}

@addMethod(CR4Player)
function W3VR_FP_PlayerVisibilityClipWanted(): bool {
  if (!this.W3VR_FP_VisibilityOwnerAllowed()) {
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
function W3VR_FP_HeadClipOwnsDrawable(
  drawable: CDrawableComponent
): bool {
  var i: int;

  for (i = 0; i < this.w3vr_fp_head_clip_hidden_drawables.Size(); i += 1) {
    if (this.w3vr_fp_head_clip_hidden_drawables[i] == drawable) {
      return true;
    }
  }
  return false;
}

@addMethod(CR4Player)
function W3VR_FP_HeadClipTracksEntity(entity: CEntity): bool {
  var i: int;

  for (i = 0; i < this.w3vr_fp_head_clip_entities.Size(); i += 1) {
    if (this.w3vr_fp_head_clip_entities[i] == entity) {
      return true;
    }
  }
  return false;
}

@addMethod(CR4Player)
function W3VR_FP_HideHeadVisibleDrawables(entity: CEntity) {
  var components: array<CComponent>;
  var drawable: CDrawableComponent;
  var i: int;

  if (!entity) {
    return;
  }
  if (!this.W3VR_FP_HeadClipTracksEntity(entity)) {
    this.w3vr_fp_head_clip_entities.PushBack(entity);
  }
  components = entity.GetComponentsByClassName('CDrawableComponent');
  for (i = 0; i < components.Size(); i += 1) {
    drawable = (CDrawableComponent)components[i];
    if (!drawable || drawable.GetName() == "shadow_capsule") {
      continue;
    }
    if (drawable.IsVisible()) {
      if (!this.W3VR_FP_HeadClipOwnsDrawable(drawable)) {
        this.w3vr_fp_head_clip_hidden_drawables.PushBack(drawable);
      }
      drawable.SetVisible(false);
    }
  }
}

@addMethod(CR4Player)
function W3VR_FP_IsHeadCategory(category: name): bool {
  return category == 'head' ||
    category == 'hair' ||
    category == 'beard' ||
    category == 'mask' ||
    category == 'hood' ||
    category == 'helmet' ||
    category == 'headwear' ||
    category == 'glasses' ||
    category == 'eyewear' ||
    category == 'earrings';
}

@addMethod(CR4Player)
function W3VR_FP_DiscoverMountedHeadEntities() {
  var items: array<SItemUniqueId>;
  var itemEntity: CItemEntity;
  var category: name;
  var i: int;

  if (!this.inv) {
    return;
  }
  this.inv.GetHeldAndMountedItems(items);
  for (i = 0; i < items.Size(); i += 1) {
    category = this.inv.GetItemCategory(items[i]);
    if (!this.W3VR_FP_IsHeadCategory(category)) {
      continue;
    }
    itemEntity = (CItemEntity)this.inv.GetItemEntityUnsafe(items[i]);
    if (itemEntity) {
      this.W3VR_FP_HideHeadVisibleDrawables(itemEntity);
    }
  }
}

@addMethod(CR4Player)
function W3VR_FP_RefreshTrackedHeadDrawables() {
  var i: int;

  for (i = 0; i < this.w3vr_fp_head_clip_entities.Size(); i += 1) {
    if (this.w3vr_fp_head_clip_entities[i]) {
      this.W3VR_FP_HideHeadVisibleDrawables(
        this.w3vr_fp_head_clip_entities[i]
      );
    }
  }
}

@addMethod(CR4Player)
function W3VR_FP_HideHeadVisibilityClip() {
  var now: float;

  // Reassert visibility on already-discovered entities every provider update.
  // Mounted-item discovery is deliberately slower because it allocates an
  // inventory array; menu/equipment transitions release this owner and force an
  // immediate fresh scan when first-person gameplay resumes.
  this.W3VR_FP_RefreshTrackedHeadDrawables();
  now = theGame.GetEngineTimeAsSeconds();
  if (!this.w3vr_fp_head_clip_active ||
      now >= this.w3vr_fp_head_clip_next_entity_scan) {
    this.W3VR_FP_DiscoverMountedHeadEntities();
    this.w3vr_fp_head_clip_next_entity_scan = now + 0.25f;
  }
  this.w3vr_fp_head_clip_active = true;
}

@addMethod(CR4Player)
function W3VR_FP_RestoreHeadVisibilityClip() {
  var drawable: CDrawableComponent;
  var i: int;

  if (!this.w3vr_fp_head_clip_active &&
      this.w3vr_fp_head_clip_hidden_drawables.Size() == 0) {
    return;
  }
  for (i = 0; i < this.w3vr_fp_head_clip_hidden_drawables.Size(); i += 1) {
    drawable = this.w3vr_fp_head_clip_hidden_drawables[i];
    if (drawable) {
      drawable.SetVisible(true);
    }
  }
  this.w3vr_fp_head_clip_hidden_drawables.Clear();
  this.w3vr_fp_head_clip_entities.Clear();
  this.w3vr_fp_head_clip_next_entity_scan = 0.0f;
  this.w3vr_fp_head_clip_active = false;
}

@addMethod(CR4Player)
function W3VR_FP_HidePlayerVisibilityClip() {
  if (this.w3vr_fp_player_clip_active) {
    return;
  }

  // The player entity owns the body meshes. Equipped appearance pieces and
  // weapons are separate item entities, so include only their mounted/held
  // instances to make the visual clip cover Geralt rather than the world.
  // Head entities have already been claimed by the persistent owner and are
  // therefore not added to this owner's restore list.
  this.W3VR_FP_HideVisibleDrawables(this);
  this.W3VR_FP_HideMountedCategory('head');
  this.W3VR_FP_HideMountedCategory('hair');
  this.W3VR_FP_HideMountedCategory('beard');
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

  if (!this.w3vr_fp_player_clip_active &&
      this.w3vr_fp_player_clip_hidden_drawables.Size() == 0) {
    return;
  }
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
  // Claim the head first. The whole-player evade owner can then restore its
  // body/equipment list without ever making the persistent head visible.
  if (this.W3VR_FP_HeadVisibilityClipWanted()) {
    this.W3VR_FP_HideHeadVisibilityClip();
  } else {
    this.W3VR_FP_RestoreHeadVisibilityClip();
  }

  if (this.W3VR_FP_PlayerVisibilityClipWanted()) {
    this.W3VR_FP_HidePlayerVisibilityClip();
  } else {
    this.W3VR_FP_RestorePlayerVisibilityClip();
  }

  // The DLL publishes a one-update capability pulse. Consume it here so a
  // missing/mismatched DLL, F11 exit, menu or Cinema transition fails visible.
  this.w3vr_fp_native_visibility_heartbeat_sink = false;
}
