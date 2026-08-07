// Witcher3VR per-panel HUD editor - isolated clean-room prototype.
//
// This script intentionally does not register or transform SubtitlesModule,
// DialogModule, InteractionsModule, EnemyFocusModule, OnelinersModule, or
// RadialMenuModule. Marker projection and the existing cutscene HUD zoom stay
// outside this mod.

function W3VRHudEditor_DebugChannel(): name
{
  return 'W3VRHudEditor';
}

function W3VRHudEditor_Version(): string
{
  return "V956";
}

function W3VRHudEditor_DebugEnabled(): bool
{
  return true;
}

function W3VRHudEditor_DebugInterval(): float
{
  return 5.0f;
}

function W3VRHudEditor_Trace(message: string, notifyPlayer: bool)
{
  var player: CR4Player;

  if (!W3VRHudEditor_DebugEnabled())
  {
    return;
  }

  if (!message)
  {
    return;
  }

  // Log() reaches the general REDengine diagnostic stream, while LogChannel()
  // keeps this mod filterable when channel logging is available.
  Log("[W3VRHudEditor] " + message);
  LogChannel(W3VRHudEditor_DebugChannel(), message);

  if (!notifyPlayer)
  {
    return;
  }

  player = (CR4Player)thePlayer;
  if (player)
  {
    player.DisplayHudMessage("W3VR HUD Editor | " + message);
  }
}

function W3VRHudEditor_PersistPositionTrace(
  stageVar: name,
  sequence: int,
  moduleName: string,
  valueA: float,
  valueB: float,
  valueC: float,
  valueD: float
)
{
  var config: CInGameConfigWrapper;

  config = (CInGameConfigWrapper)theGame.GetInGameConfigWrapper();
  if (!config)
  {
    return;
  }

  config.SetVarValue(
    'W3VRHudEditorDebug', stageVar, IntToString(sequence)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'LastStage', NameToString(stageVar)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'LastSequence', IntToString(sequence)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'LastModule', moduleName
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'ValueA', FloatToString(valueA)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'ValueB', FloatToString(valueB)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'ValueC', FloatToString(valueC)
  );
  config.SetVarValue(
    'W3VRHudEditorDebug', 'ValueD', FloatToString(valueD)
  );
  theGame.SaveUserSettings();
}

function W3VRHudEditor_BeginDerivedPositionTrace(
  module: CR4HudModuleBase,
  originalAnchorX: float,
  originalAnchorY: float,
  adjustedAnchorX: float,
  adjustedAnchorY: float
)
{
  if (!module || !module.w3vr_hud_editor_position_trace_pending)
  {
    return;
  }

  W3VRHudEditor_PersistPositionTrace(
    'WrapperInSeq', module.w3vr_hud_editor_position_trace_sequence,
    module.w3vr_hud_editor_position_trace_name,
    originalAnchorX, originalAnchorY, adjustedAnchorX, adjustedAnchorY
  );
}

function W3VRHudEditor_EndDerivedPositionTrace(module: CR4HudModuleBase)
{
  var root: CScriptedFlashSprite;

  if (!module || !module.w3vr_hud_editor_position_trace_pending)
  {
    return;
  }

  root = module.GetModuleFlash();
  if (root)
  {
    W3VRHudEditor_PersistPositionTrace(
      'WrapperOutSeq', module.w3vr_hud_editor_position_trace_sequence,
      module.w3vr_hud_editor_position_trace_name,
      root.GetX(), root.GetY(),
      module.w3vr_hud_editor_target_x,
      module.w3vr_hud_editor_target_y
    );
  }
  else
  {
    W3VRHudEditor_PersistPositionTrace(
      'WrapperOutSeq', module.w3vr_hud_editor_position_trace_sequence,
      module.w3vr_hud_editor_position_trace_name,
      0.0f, 0.0f,
      module.w3vr_hud_editor_target_x,
      module.w3vr_hud_editor_target_y
    );
  }
  module.w3vr_hud_editor_position_trace_pending = false;
}

class W3VRHudEditorSlot
{
  var moduleName: string;
  var displayName: string;
  var xVar: name;
  var yVar: name;
  var scaleVar: name;
  var module: CR4HudModuleBase;

  var vrX: float;
  var vrY: float;
  var vrScale: float;

  var cinemaX: float;
  var cinemaY: float;
  var cinemaScale: float;

  var forceVisibleWhileSelected: bool;
  var visualStateCaptured: bool;
  var originalAlpha: float;
  var originalVisible: bool;

  public function Configure(
    inModule: CR4HudModuleBase,
    inModuleName: string,
    inDisplayName: string,
    inXVar: name,
    inYVar: name,
    inScaleVar: name,
    inForceVisibleWhileSelected: bool
  )
  {
    ConfigureDescriptor(
      inModuleName, inDisplayName, inXVar, inYVar, inScaleVar,
      inForceVisibleWhileSelected
    );
    BindModule(inModule);
  }

  public function ConfigureDescriptor(
    inModuleName: string,
    inDisplayName: string,
    inXVar: name,
    inYVar: name,
    inScaleVar: name,
    inForceVisibleWhileSelected: bool
  )
  {
    moduleName = inModuleName;
    displayName = inDisplayName;
    xVar = inXVar;
    yVar = inYVar;
    scaleVar = inScaleVar;
    forceVisibleWhileSelected = inForceVisibleWhileSelected;

    vrX = 0.0f;
    vrY = 0.0f;
    vrScale = 1.0f;
    cinemaX = 0.0f;
    cinemaY = 0.0f;
    cinemaScale = 1.0f;
  }

  public function BindModule(inModule: CR4HudModuleBase)
  {
    module = inModule;
  }

  public function GetX(profile: int): float
  {
    if (profile == 1)
    {
      return cinemaX;
    }
    return vrX;
  }

  public function GetY(profile: int): float
  {
    if (profile == 1)
    {
      return cinemaY;
    }
    return vrY;
  }

  public function GetScale(profile: int): float
  {
    if (profile == 1)
    {
      return cinemaScale;
    }
    return vrScale;
  }

  public function SetX(profile: int, value: float)
  {
    if (profile == 1)
    {
      cinemaX = value;
    }
    else
    {
      vrX = value;
    }
  }

  public function SetY(profile: int, value: float)
  {
    if (profile == 1)
    {
      cinemaY = value;
    }
    else
    {
      vrY = value;
    }
  }

  public function SetScale(profile: int, value: float)
  {
    if (profile == 1)
    {
      cinemaScale = value;
    }
    else
    {
      vrScale = value;
    }
  }

  public function Reset(profile: int)
  {
    SetX(profile, 0.0f);
    SetY(profile, 0.0f);
    SetScale(profile, 1.0f);
  }

  public function CaptureVisualState()
  {
    var root: CScriptedFlashSprite;

    if (!module)
    {
      return;
    }

    root = module.GetModuleFlash();
    if (!root)
    {
      return;
    }

    // Selection identity uses the standard HUD message route. Keep its owner
    // readable while other panels retain editor dimming.
    if (moduleName == "MessageModule")
    {
      root.SetVisible(true);
      root.SetAlpha(100.0f);
      return;
    }

    originalAlpha = root.GetAlpha();
    originalVisible = root.GetVisible();
    visualStateCaptured = true;
  }

  public function ApplyEditorVisual(selected: bool)
  {
    var root: CScriptedFlashSprite;
    var selectedAlpha: float;

    if (!visualStateCaptured || !module)
    {
      return;
    }

    root = module.GetModuleFlash();
    if (!root)
    {
      return;
    }

    if (selected)
    {
      if (forceVisibleWhileSelected)
      {
        root.SetVisible(true);
      }
      else
      {
        root.SetVisible(originalVisible);
      }

      selectedAlpha = 100.0f;
      root.SetAlpha(selectedAlpha);
    }
    else
    {
      root.SetVisible(originalVisible);
      root.SetAlpha(originalAlpha * 0.50f);
    }
  }

  public function RestoreVisualState()
  {
    var root: CScriptedFlashSprite;

    if (!visualStateCaptured || !module)
    {
      visualStateCaptured = false;
      return;
    }

    root = module.GetModuleFlash();
    if (root)
    {
      root.SetVisible(originalVisible);
      root.SetAlpha(originalAlpha);
    }
    visualStateCaptured = false;
  }
}

class W3VRHudEditorController
{
  private var hud: CR4ScriptedHud;
  private var slots: array<W3VRHudEditorSlot>;
  private var initialized: bool;
  private var inputsRegistered: bool;
  private var editorContextStored: bool;
  private var editing: bool;
  private var activeProfile: int;
  private var selectedIndex: int;
  private var labelRefreshRemaining: float;
  private var driverLastTime: float;
  private var toggleKeyLatched: bool;
  private var runtimeHookProbeShown: bool;
  private var registryReadyProbeShown: bool;
  private var nextDebugTraceAt: float;
  private var lastDebugSlotCount: int;
  private var movementTraceSequence: int;
  private var tutorialPreviewRequested: bool;

  public function BeginHudInitialization(ownerHud: CR4ScriptedHud)
  {
    if (!ownerHud)
    {
      return;
    }

    if (editing)
    {
      StopEditor();
    }

    RemoveRegistryTransforms();
    slots.Clear();
    initialized = false;
    runtimeHookProbeShown = false;
    toggleKeyLatched = false;
    registryReadyProbeShown = false;
    nextDebugTraceAt = 0.0f;
    lastDebugSlotCount = -1;
    movementTraceSequence = 0;
    tutorialPreviewRequested = false;
    hud = ownerHud;
    EnsureRegistryDescriptors();
    RegisterInputs();
  }

  public function Initialize(ownerHud: CR4ScriptedHud)
  {
    if (!ownerHud)
    {
      return;
    }

    hud = ownerHud;
    nextDebugTraceAt = 0.0f;
    lastDebugSlotCount = -1;
    EnsureRegistryDescriptors();
    RegisterInputs();
    TryFinalizeRegistry();
  }

  private function EnsureRegistryDescriptors()
  {
    AddDescriptor("ControlsFeedbackModule", "Controls Feedback", 'ControlsFeedbackX', 'ControlsFeedbackY', 'ControlsFeedbackScale', true);
    AddDescriptor("HorseStaminaBarModule", "Horse Stamina", 'HorseStaminaX', 'HorseStaminaY', 'HorseStaminaScale', true);
    AddDescriptor("HorsePanicBarModule", "Horse Fear", 'HorsePanicX', 'HorsePanicY', 'HorsePanicScale', true);
    AddDescriptor("MessageModule", "HUD Messages", 'MessageX', 'MessageY', 'MessageScale', true);
    AddDescriptor("QuestsModule", "Tracked Quests", 'QuestsX', 'QuestsY', 'QuestsScale', true);
    AddDescriptor("BuffsModule", "Buffs", 'BuffsX', 'BuffsY', 'BuffsScale', true);
    AddDescriptor("WolfHeadModule", "Wolf Medallion and Vitality", 'WolfHeadX', 'WolfHeadY', 'WolfHeadScale', true);
    AddDescriptor("ItemInfoModule", "Equipped Items", 'ItemInfoX', 'ItemInfoY', 'ItemInfoScale', true);
    AddDescriptor("OxygenBarModule", "Oxygen", 'OxygenX', 'OxygenY', 'OxygenScale', true);
    AddDescriptor("BossFocusModule", "Boss Health", 'BossFocusX', 'BossFocusY', 'BossFocusScale', true);
    AddDescriptor("BoatHealthModule", "Boat Health", 'BoatHealthX', 'BoatHealthY', 'BoatHealthScale', true);
    AddDescriptor("ConsoleModule", "Action Log", 'ConsoleX', 'ConsoleY', 'ConsoleScale', true);
    AddDescriptor("JournalUpdateModule", "Journal Updates", 'JournalUpdateX', 'JournalUpdateY', 'JournalUpdateScale', true);
    AddDescriptor("AreaInfoModule", "Area Information", 'AreaInfoX', 'AreaInfoY', 'AreaInfoScale', true);
    AddDescriptor("Minimap2Module", "Minimap", 'MinimapX', 'MinimapY', 'MinimapScale', true);
    AddDescriptor("CompanionModule", "Companion", 'CompanionX', 'CompanionY', 'CompanionScale', true);
    AddDescriptor("DamagedItemsModule", "Damaged Items", 'DamagedItemsX', 'DamagedItemsY', 'DamagedItemsScale', true);
    AddDescriptor("TimeLapseModule", "Time Lapse", 'TimeLapseX', 'TimeLapseY', 'TimeLapseScale', true);
    AddDescriptor("TimeLeftModule", "Time Remaining", 'TimeLeftX', 'TimeLeftY', 'TimeLeftScale', true);
    AddDescriptor("TutorialPopup", "Tutorial Messages", 'TutorialX', 'TutorialY', 'TutorialScale', true);
  }

  private function AddDescriptor(
    moduleName: string,
    displayName: string,
    xVar: name,
    yVar: name,
    scaleVar: name,
    forceVisibleWhileSelected: bool
  )
  {
    var slot: W3VRHudEditorSlot;

    if (HasSlot(moduleName))
    {
      return;
    }

    slot = new W3VRHudEditorSlot in hud;
    slot.ConfigureDescriptor(
      moduleName, displayName, xVar, yVar, scaleVar,
      forceVisibleWhileSelected
    );
    slots.PushBack(slot);
  }

  public function RegisterHudModule(
    ownerHud: CR4ScriptedHud,
    module: CR4HudModuleBase
  )
  {
    var beforeCount: int;

    if (!ownerHud || !module)
    {
      return;
    }

    beforeCount = GetLiveModuleCount();

    hud = ownerHud;

    // Capture the live module reference while vanilla publishes it. On this
    // runtime route CHud.GetHudModule() returns NULL even after loading, while
    // CR4ScriptedHud.AddHudModuleReference() still receives every real module.
    // Dialog, subtitles, markers, radial menu and crosshair stay excluded.
    if ((CR4HudModuleControlsFeedback)module)
      AddSlot(module, "ControlsFeedbackModule", "Controls Feedback", 'ControlsFeedbackX', 'ControlsFeedbackY', 'ControlsFeedbackScale', true);
    else if ((CR4HudModuleHorseStaminaBar)module)
      AddSlot(module, "HorseStaminaBarModule", "Horse Stamina", 'HorseStaminaX', 'HorseStaminaY', 'HorseStaminaScale', true);
    else if ((CR4HudModuleHorsePanicBar)module)
      AddSlot(module, "HorsePanicBarModule", "Horse Fear", 'HorsePanicX', 'HorsePanicY', 'HorsePanicScale', true);
    else if ((CR4HudModuleMessage)module)
      AddSlot(module, "MessageModule", "HUD Messages", 'MessageX', 'MessageY', 'MessageScale', true);
    else if ((CR4HudModuleQuests)module)
      AddSlot(module, "QuestsModule", "Tracked Quests", 'QuestsX', 'QuestsY', 'QuestsScale', true);
    else if ((CR4HudModuleBuffs)module)
      AddSlot(module, "BuffsModule", "Buffs", 'BuffsX', 'BuffsY', 'BuffsScale', true);
    else if ((CR4HudModuleWolfHead)module)
      AddSlot(module, "WolfHeadModule", "Wolf Medallion and Vitality", 'WolfHeadX', 'WolfHeadY', 'WolfHeadScale', true);
    else if ((CR4HudModuleItemInfo)module)
      AddSlot(module, "ItemInfoModule", "Equipped Items", 'ItemInfoX', 'ItemInfoY', 'ItemInfoScale', true);
    else if ((CR4HudModuleOxygenBar)module)
      AddSlot(module, "OxygenBarModule", "Oxygen", 'OxygenX', 'OxygenY', 'OxygenScale', true);
    else if ((CR4HudModuleBossFocus)module)
      AddSlot(module, "BossFocusModule", "Boss Health", 'BossFocusX', 'BossFocusY', 'BossFocusScale', true);
    else if ((CR4HudModuleBoatHealth)module)
      AddSlot(module, "BoatHealthModule", "Boat Health", 'BoatHealthX', 'BoatHealthY', 'BoatHealthScale', true);
    else if ((CR4HudModuleConsole)module)
      AddSlot(module, "ConsoleModule", "Action Log", 'ConsoleX', 'ConsoleY', 'ConsoleScale', true);
    else if ((CR4HudModuleJournalUpdate)module)
      AddSlot(module, "JournalUpdateModule", "Journal Updates", 'JournalUpdateX', 'JournalUpdateY', 'JournalUpdateScale', true);
    else if ((CR4HudModuleAreaInfo)module)
      AddSlot(module, "AreaInfoModule", "Area Information", 'AreaInfoX', 'AreaInfoY', 'AreaInfoScale', true);
    else if ((CR4HudModuleMinimap2)module)
      AddSlot(module, "Minimap2Module", "Minimap", 'MinimapX', 'MinimapY', 'MinimapScale', true);
    else if ((CR4HudModuleCompanion)module)
      AddSlot(module, "CompanionModule", "Companion", 'CompanionX', 'CompanionY', 'CompanionScale', true);
    else if ((CR4HudModuleDamagedItems)module)
      AddSlot(module, "DamagedItemsModule", "Damaged Items", 'DamagedItemsX', 'DamagedItemsY', 'DamagedItemsScale', true);
    else if ((CR4HudModuleTimeLapse)module)
      AddSlot(module, "TimeLapseModule", "Time Lapse", 'TimeLapseX', 'TimeLapseY', 'TimeLapseScale', true);
    else if ((CR4HudModuleTimeLeft)module)
      AddSlot(module, "TimeLeftModule", "Time Remaining", 'TimeLeftX', 'TimeLeftY', 'TimeLeftScale', true);

    if (GetLiveModuleCount() != beforeCount)
    {
      W3VRHudEditor_Trace(
        "capture live=" + IntToString(GetLiveModuleCount()) + "/19" +
        " missing=" + GetMissingModuleName(),
        false
      );
      lastDebugSlotCount = GetLiveModuleCount();

      if (initialized)
      {
        RefreshAllManagedModules();
        W3VRHudEditor_Trace(
          "module became available | live=" +
          IntToString(GetLiveModuleCount()) + "/19",
          true
        );
      }
    }
  }

  public function RegisterAreaInfoModule(
    ownerHud: CR4ScriptedHud,
    module: CR4HudModuleBase
  )
  {
    var beforeCount: int;

    if (!ownerHud || !module)
    {
      return;
    }

    beforeCount = GetLiveModuleCount();
    hud = ownerHud;
    AddSlot(
      module, "AreaInfoModule", "Area Information",
      'AreaInfoX', 'AreaInfoY', 'AreaInfoScale', true
    );

    if (GetLiveModuleCount() != beforeCount)
    {
      W3VRHudEditor_Trace(
        "capture live=" + IntToString(GetLiveModuleCount()) +
        "/19 last=AreaInfoModule missing=" + GetMissingModuleName() +
        " via=AreaInfo.OnConfigUI",
        false
      );
      lastDebugSlotCount = GetLiveModuleCount();

      if (initialized)
      {
        RefreshAllManagedModules();
        W3VRHudEditor_Trace(
          "AreaInfoModule became available dynamically | live=" +
          IntToString(GetLiveModuleCount()) + "/19",
          true
        );
        return;
      }
    }
  }

  private function TryFinalizeRegistry()
  {
    if (initialized)
    {
      return;
    }

    // All 20 logical panels exist before their Flash modules/popups. Hidden or lazy
    // runtime modules bind later without blocking offline configuration.
    if (slots.Size() != 20)
    {
      return;
    }

    LoadSettings();
    RegisterInputs();

    if (selectedIndex < 0 || selectedIndex >= slots.Size())
    {
    selectedIndex = 0;
    }

    initialized = true;
    RefreshAllManagedModules();
    W3VRHudEditor_Trace(
      "panels-ready 20/20 | hud-live=" +
      IntToString(GetLiveModuleCount()) + "/19",
      true
    );
  }

  private function AddSlot(
    module: CR4HudModuleBase,
    moduleName: string,
    displayName: string,
    xVar: name,
    yVar: name,
    scaleVar: name,
    forceVisibleWhileSelected: bool
  )
  {
    var i: int;
    var slot: W3VRHudEditorSlot;

    if (!module)
    {
      return;
    }

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i].moduleName == moduleName)
      {
        // Preserve settings already edited while this lazy module was offline.
        slots[i].BindModule(module);
        return;
      }
    }

    slot = new W3VRHudEditorSlot in hud;
    slot.Configure(
      module,
      moduleName,
      displayName,
      xVar,
      yVar,
      scaleVar,
      forceVisibleWhileSelected
    );
    slots.PushBack(slot);
  }

  private function HasSlot(moduleName: string): bool
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i].moduleName == moduleName)
      {
        return true;
      }
    }
    return false;
  }

  private function FindSlot(moduleName: string): W3VRHudEditorSlot
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i].moduleName == moduleName)
      {
        return slots[i];
      }
    }
    return NULL;
  }

  private function IsSlotLive(slot: W3VRHudEditorSlot): bool
  {
    if (!slot)
    {
      return false;
    }
    if (slot.module)
    {
      return true;
    }
    if (
      slot.moduleName == "TutorialPopup" &&
      theGame.GetGuiManager() &&
      theGame.GetGuiManager().GetPopup('TutorialPopup')
    )
    {
      return true;
    }
    return false;
  }

  private function HasLiveSlot(moduleName: string): bool
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i].moduleName == moduleName && slots[i].module)
      {
        return true;
      }
    }
    return false;
  }

  private function GetLiveModuleCount(): int
  {
    var i: int;
    var count: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i].module)
      {
        count += 1;
      }
    }
    return count;
  }

  private function GetMissingModuleName(): string
  {
    if (!HasLiveSlot("ControlsFeedbackModule")) return "ControlsFeedbackModule";
    if (!HasLiveSlot("HorseStaminaBarModule")) return "HorseStaminaBarModule";
    if (!HasLiveSlot("HorsePanicBarModule")) return "HorsePanicBarModule";
    if (!HasLiveSlot("MessageModule")) return "MessageModule";
    if (!HasLiveSlot("QuestsModule")) return "QuestsModule";
    if (!HasLiveSlot("BuffsModule")) return "BuffsModule";
    if (!HasLiveSlot("WolfHeadModule")) return "WolfHeadModule";
    if (!HasLiveSlot("ItemInfoModule")) return "ItemInfoModule";
    if (!HasLiveSlot("OxygenBarModule")) return "OxygenBarModule";
    if (!HasLiveSlot("BossFocusModule")) return "BossFocusModule";
    if (!HasLiveSlot("BoatHealthModule")) return "BoatHealthModule";
    if (!HasLiveSlot("ConsoleModule")) return "ConsoleModule";
    if (!HasLiveSlot("JournalUpdateModule")) return "JournalUpdateModule";
    if (!HasLiveSlot("AreaInfoModule")) return "AreaInfoModule";
    if (!HasLiveSlot("Minimap2Module")) return "Minimap2Module";
    if (!HasLiveSlot("CompanionModule")) return "CompanionModule";
    if (!HasLiveSlot("DamagedItemsModule")) return "DamagedItemsModule";
    if (!HasLiveSlot("TimeLapseModule")) return "TimeLapseModule";
    if (!HasLiveSlot("TimeLeftModule")) return "TimeLeftModule";
    return "none";
  }

  private function RegisterInputs()
  {
    if (inputsRegistered)
    {
      return;
    }

    // Toggle is polled from the HUD tick. This avoids depending on the helper
    // listener being registered during the one-shot HUD OnConfigUI lifecycle.
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorExit', 'W3VRHudEditorExit'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorSelect', 'W3VRHudEditorPrevious'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorSelect', 'W3VRHudEditorNext'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorScale', 'W3VRHudEditorScale'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorReset', 'W3VRHudEditorResetCurrent'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorReset', 'W3VRHudEditorResetProfile'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorProfile', 'W3VRHudEditorProfile'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorNudge', 'W3VRHudEditorMoveLeft'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorNudge', 'W3VRHudEditorMoveRight'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorNudge', 'W3VRHudEditorMoveUp'
    );
    theInput.RegisterListener(
      this, 'OnW3VRHudEditorNudge', 'W3VRHudEditorMoveDown'
    );
    inputsRegistered = true;
  }

  public function Tick(timeDelta: float)
  {
    var now: float;
    var stateLabel: string;
    var toggleDown: bool;

    // Ensure the controller/input side exists even before every child module
    // has published its live reference through AddHudModuleReference().
    if (!initialized && hud)
    {
      Initialize(hud);
    }

    now = theGame.GetEngineTimeAsSeconds();
    if (nextDebugTraceAt <= 0.0f || now >= nextDebugTraceAt)
    {
      if (initialized)
      {
        stateLabel = "ready";
      }
      else
      {
        stateLabel = "waiting";
      }

      W3VRHudEditor_Trace(
        "tick-state " + stateLabel +
        " panels=" + IntToString(slots.Size()) + "/20" +
        " hud-live=" + IntToString(GetLiveModuleCount()) + "/19" +
        " context=" + NameToString(theInput.GetContext()),
        false
      );
      nextDebugTraceAt = now + W3VRHudEditor_DebugInterval();
    }

    // Retail REDscript logging has no reliable file sink. Emit one proven HUD
    // breadcrumb so the test exposes both the universal driver and registry
    // state instead of failing silently.
    if (!runtimeHookProbeShown && thePlayer)
    {
      W3VRHudEditor_Trace(
        "hook-bound panels=" + IntToString(slots.Size()) +
        "/20 hud-live=" + IntToString(GetLiveModuleCount()) + "/19",
        true
      );
      runtimeHookProbeShown = true;
    }

    if (initialized && !registryReadyProbeShown && thePlayer)
    {
      thePlayer.DisplayHudMessage(
        "W3VR HUD Editor ready | panels 20/20 | HUD live " +
        IntToString(GetLiveModuleCount()) + "/19 | context " +
        NameToString(theInput.GetContext())
      );
      registryReadyProbeShown = true;
    }


    // GetActionValue plus an explicit latch is the project-validated route for
    // custom REDengine actions. It also remains deterministic if this driver
    // is called more than once during one rendered frame.
    toggleDown =
      theInput.GetActionValue('W3VRHudEditorToggle') > 0.50f;

    if (!initialized)
    {
      if (toggleDown && !toggleKeyLatched && thePlayer)
      {
        thePlayer.DisplayHudMessage(
          "W3VR HUD Editor | Insert received, panels waiting " +
          IntToString(slots.Size()) + "/20 | hud-live=" +
          IntToString(GetLiveModuleCount()) + "/19 | missing=" +
          GetMissingModuleName()
        );
      }
      toggleKeyLatched = toggleDown;
      return;
    }

    if (toggleDown && !toggleKeyLatched)
    {
      if (thePlayer)
      {
        thePlayer.DisplayHudMessage("W3VR HUD Editor Insert received");
      }
      ToggleEditor();
    }
    toggleKeyLatched = toggleDown;

    if (!editing)
    {
      return;
    }

    // Do not carry the editor pause/input lock across a world or player
    // teardown.
    if (!thePlayer)
    {
      StopEditor();
      return;
    }

    PollMovementInput(timeDelta);

    // Some vanilla modules update their root alpha during OnTick. Reassert the
    // editor-only dimming after the wrapped HUD tick, then restore on exit.
    RefreshEditorVisuals();
    ShowConditionalPreview(GetSelectedSlot());

    labelRefreshRemaining -= timeDelta;
    if (labelRefreshRemaining <= 0.0f)
    {
      ShowEditorLabel();
      labelRefreshRemaining = 0.25f;
    }
  }

  public function Drive(ownerHud: CR4ScriptedHud)
  {
    var currentTime: float;
    var timeDelta: float;

    if (!hud && ownerHud)
    {
      hud = ownerHud;
    }

    currentTime = theGame.GetEngineTimeAsSeconds();
    if (driverLastTime > 0.0f)
    {
      timeDelta = currentTime - driverLastTime;
      if (timeDelta < 0.0f)
      {
        timeDelta = 0.0f;
      }
      else if (timeDelta > 0.25f)
      {
        timeDelta = 0.25f;
      }
    }
    else
    {
      timeDelta = 0.0f;
    }
    driverLastTime = currentTime;

    Tick(timeDelta);
  }

  public function PostHudTick()
  {
    if (!initialized)
    {
      return;
    }

    if (editing)
    {
      RefreshEditorVisuals();
    }
  }

  private function PollMovementInput(timeDelta: float)
  {
    var moveX: float;
    var moveY: float;
    var frameDelta: float;

    if (!editing)
    {
      return;
    }

    moveX = 0.0f;
    moveY = 0.0f;
    if (theInput.IsActionPressed('W3VRHudEditorMoveLeft'))
    {
      moveX -= 1.0f;
    }
    if (theInput.IsActionPressed('W3VRHudEditorMoveRight'))
    {
      moveX += 1.0f;
    }
    if (theInput.IsActionPressed('W3VRHudEditorMoveUp'))
    {
      moveY -= 1.0f;
    }
    if (theInput.IsActionPressed('W3VRHudEditorMoveDown'))
    {
      moveY += 1.0f;
    }
    if (AbsF(moveX) < 0.01f && AbsF(moveY) < 0.01f)
    {
      return;
    }

    frameDelta = timeDelta;
    if (frameDelta <= 0.0f || frameDelta > 0.05f)
    {
      frameDelta = 0.016f;
    }

    MoveSelected(
      moveX * 240.0f * frameDelta,
      moveY * 240.0f * frameDelta
    );
  }

  event OnW3VRHudEditorToggle(action: SInputAction)
  {
    if (!IsPressed(action))
    {
      return false;
    }

    ToggleEditor();
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorExit(action: SInputAction)
  {
    if (!editing || !IsPressed(action))
    {
      return false;
    }

    StopEditor();
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorSelect(action: SInputAction)
  {
    if (!editing || !IsPressed(action))
    {
      return false;
    }

    if (action.aName == 'W3VRHudEditorPrevious')
    {
      SelectRelative(-1);
    }
    else
    {
      SelectRelative(1);
    }
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorScale(action: SInputAction)
  {
    if (!editing)
    {
      return false;
    }

    if (action.value > 0.01f)
    {
      ScaleSelected(0.10f);
    }
    else if (action.value < -0.01f)
    {
      ScaleSelected(-0.10f);
    }
    else
    {
      return false;
    }
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorNudge(action: SInputAction)
  {
    if (!editing || !IsPressed(action))
    {
      return false;
    }

    W3VRHudEditor_Trace(
      "POSITION INPUT action=" + NameToString(action.aName) +
      " value=" + FloatToString(action.value),
      false
    );
    W3VRHudEditor_PersistPositionTrace(
      'InputSeq', movementTraceSequence + 1,
      NameToString(action.aName), action.value, 0.0f, 0.0f, 0.0f
    );

    if (action.aName == 'W3VRHudEditorMoveLeft')
    {
      MoveSelected(-10.0f, 0.0f);
    }
    else if (action.aName == 'W3VRHudEditorMoveRight')
    {
      MoveSelected(10.0f, 0.0f);
    }
    else if (action.aName == 'W3VRHudEditorMoveUp')
    {
      MoveSelected(0.0f, -10.0f);
    }
    else
    {
      MoveSelected(0.0f, 10.0f);
    }

    ShowEditorLabel();
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorReset(action: SInputAction)
  {
    if (!editing || !IsPressed(action))
    {
      return false;
    }

    if (action.aName == 'W3VRHudEditorResetCurrent')
    {
      ResetSelected();
    }
    else
    {
      ResetActiveProfile();
    }
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  event OnW3VRHudEditorProfile(action: SInputAction)
  {
    if (!editing || !IsPressed(action))
    {
      return false;
    }

    if (activeProfile == 0)
    {
      SetActiveProfile(1);
    }
    else
    {
      SetActiveProfile(0);
    }
    theInput.SuppressPropagatingEventAfterAction(action.aName);
    return true;
  }

  public function SetActiveProfile(profile: int)
  {
    if (profile != 1)
    {
      profile = 0;
    }

    if (activeProfile == profile)
    {
      return;
    }

    activeProfile = profile;
    RefreshAllManagedModules();
    if (theGame.GetGuiManager())
    {
      ApplyTutorialPopupLayout(
        (CR4TutorialPopup)theGame.GetGuiManager().GetPopup('TutorialPopup'),
        true
      );
    }
    ShowEditorLabel();
  }

  public function GetActiveProfile(): int
  {
    return activeProfile;
  }

  private function ToggleEditor()
  {
    if (!initialized)
    {
      return;
    }

    if (editing)
    {
      StopEditor();
    }
    else
    {
      StartEditor();
    }
  }

  private function StartEditor()
  {
    var i: int;

    if (editing || slots.Size() == 0 || !thePlayer)
    {
      return;
    }

    editing = true;

    for (i = 0; i < slots.Size(); i += 1)
    {
      slots[i].CaptureVisualState();
    }

    // The dedicated context isolates editor input without freezing Scaleform
    // or the HUD message queue.
    theInput.StoreContext('W3VRHudEditor');
    editorContextStored = true;

    RefreshEditorVisuals();
    ShowConditionalPreview(GetSelectedSlot());
    ShowEditorLabel();
    AnnounceSelectedSlot();
    labelRefreshRemaining = 0.25f;
  }

  private function StopEditor()
  {
    var i: int;

    if (!editing)
    {
      return;
    }

    HideConditionalPreview(GetSelectedSlot());
    SaveSettings();
    editing = false;

    for (i = 0; i < slots.Size(); i += 1)
    {
      slots[i].RestoreVisualState();
    }

    // Remove the stack entry we created even if loading or a scene transition
    // changed the current context before teardown.
    if (editorContextStored)
    {
      theInput.RestoreContext('W3VRHudEditor', true);
      editorContextStored = false;
    }

  }

  private function SelectRelative(direction: int)
  {
    if (slots.Size() == 0)
    {
      return;
    }

    HideConditionalPreview(GetSelectedSlot());
    selectedIndex += direction;
    if (selectedIndex < 0)
    {
      selectedIndex = slots.Size() - 1;
    }
    else if (selectedIndex >= slots.Size())
    {
      selectedIndex = 0;
    }

    RefreshEditorVisuals();
    ShowConditionalPreview(GetSelectedSlot());
    ShowEditorLabel();
    AnnounceSelectedSlot();
  }

  private function ShowConditionalPreview(slot: W3VRHudEditorSlot)
  {
    var consoleModule: CR4HudModuleConsole;
    var journalModule: CR4HudModuleJournalUpdate;
    var areaModule: CR4HudModuleAreaInfo;

    if (!slot)
    {
      return;
    }

    if (slot.moduleName == "TutorialPopup")
    {
      ShowTutorialPreview(slot);
      return;
    }

    if (!slot.module)
    {
      return;
    }

    if (slot.moduleName == "ConsoleModule")
    {
      consoleModule = (CR4HudModuleConsole)slot.module;
      if (consoleModule)
      {
        consoleModule.W3VRHudEditorShowPreview();
      }
    }
    else if (slot.moduleName == "JournalUpdateModule")
    {
      journalModule = (CR4HudModuleJournalUpdate)slot.module;
      if (journalModule)
      {
        journalModule.W3VRHudEditorShowPreview();
      }
    }
    else if (slot.moduleName == "AreaInfoModule")
    {
      areaModule = (CR4HudModuleAreaInfo)slot.module;
      if (areaModule)
      {
        areaModule.W3VRHudEditorShowPreview();
      }
    }
  }

  private function HideConditionalPreview(slot: W3VRHudEditorSlot)
  {
    var consoleModule: CR4HudModuleConsole;
    var journalModule: CR4HudModuleJournalUpdate;
    var areaModule: CR4HudModuleAreaInfo;

    if (!slot)
    {
      return;
    }

    if (slot.moduleName == "TutorialPopup")
    {
      HideTutorialPreview();
      return;
    }

    if (!slot.module)
    {
      return;
    }

    if (slot.moduleName == "ConsoleModule")
    {
      consoleModule = (CR4HudModuleConsole)slot.module;
      if (consoleModule)
      {
        consoleModule.W3VRHudEditorHidePreview();
      }
    }
    else if (slot.moduleName == "JournalUpdateModule")
    {
      journalModule = (CR4HudModuleJournalUpdate)slot.module;
      if (journalModule)
      {
        journalModule.W3VRHudEditorHidePreview();
      }
    }
    else if (slot.moduleName == "AreaInfoModule")
    {
      areaModule = (CR4HudModuleAreaInfo)slot.module;
      if (areaModule)
      {
        areaModule.W3VRHudEditorHidePreview();
      }
    }
  }

  private function ShowTutorialPreview(slot: W3VRHudEditorSlot)
  {
    var popup: CR4TutorialPopup;
    var previewData: W3TutorialPopupData;

    if (!slot || !theGame.GetGuiManager())
    {
      return;
    }

    popup = (CR4TutorialPopup)theGame.GetGuiManager().GetPopup(
      'TutorialPopup'
    );
    if (popup)
    {
      popup.W3VRHudEditorApplyLayout(
        slot.GetX(activeProfile), slot.GetY(activeProfile), false
      );
      return;
    }

    if (tutorialPreviewRequested)
    {
      return;
    }

    previewData = new W3TutorialPopupData in hud;
    previewData.scriptTag = 'W3VRHudEditorPreview';
    previewData.messageTitle = "TUTORIAL MESSAGES";
    previewData.messageText = "HUD EDITOR PREVIEW";
    previewData.duration = -1.0f;
    previewData.autosize = true;
    previewData.enableGlossoryLink = false;
    previewData.enableAcceptButton = false;
    previewData.blockInput = false;
    previewData.pauseGame = false;
    previewData.fullscreen = false;
    previewData.canBeShownInMenus = true;
    theGame.RequestPopup('TutorialPopup', previewData);
    tutorialPreviewRequested = true;
  }

  private function HideTutorialPreview()
  {
    var popup: CR4TutorialPopup;

    tutorialPreviewRequested = false;
    if (!theGame.GetGuiManager())
    {
      return;
    }

    popup = (CR4TutorialPopup)theGame.GetGuiManager().GetPopup(
      'TutorialPopup'
    );
    if (popup)
    {
      popup.W3VRHudEditorClosePreview();
    }
  }

  public function ApplyTutorialPopupLayout(
    popup: CR4TutorialPopup,
    refresh: bool
  )
  {
    var slot: W3VRHudEditorSlot;

    if (!popup)
    {
      return;
    }
    slot = FindSlot("TutorialPopup");
    if (!slot)
    {
      return;
    }
    popup.W3VRHudEditorApplyLayout(
      slot.GetX(activeProfile), slot.GetY(activeProfile), refresh
    );
  }

  private function MoveSelected(deltaX: float, deltaY: float)
  {
    var slot: W3VRHudEditorSlot;
    var oldX: float;
    var oldY: float;
    var newX: float;
    var newY: float;
    var liveState: string;

    slot = GetSelectedSlot();
    if (!slot)
    {
      return;
    }

    oldX = slot.GetX(activeProfile);
    oldY = slot.GetY(activeProfile);
    newX = ClampOffset(oldX + deltaX, 3840.0f);
    newY = ClampOffset(oldY + deltaY, 2160.0f);
    slot.SetX(activeProfile, newX);
    slot.SetY(activeProfile, newY);

    if (slot.module)
    {
      liveState = "1";
    }
    else
    {
      liveState = "0";
    }

    movementTraceSequence += 1;
    W3VRHudEditor_Trace(
      "POSITION MOVE seq=" + IntToString(movementTraceSequence) +
      " module=" + slot.moduleName +
      " delta=" + FloatToString(deltaX) + "," + FloatToString(deltaY) +
      " stored=" + FloatToString(oldX) + "," + FloatToString(oldY) +
      "->" + FloatToString(newX) + "," + FloatToString(newY) +
      " live=" + liveState,
      false
    );
    W3VRHudEditor_PersistPositionTrace(
      'MoveSeq', movementTraceSequence, slot.moduleName,
      oldX, oldY, newX, newY
    );

    if (slot.module)
    {
      slot.module.w3vr_hud_editor_position_trace_pending = true;
      slot.module.w3vr_hud_editor_position_trace_sequence =
        movementTraceSequence;
      slot.module.w3vr_hud_editor_position_trace_name = slot.moduleName;
    }
    RefreshSlot(slot);
  }

  private function ScaleSelected(delta: float)
  {
    var slot: W3VRHudEditorSlot;

    slot = GetSelectedSlot();
    if (!slot)
    {
      return;
    }

    slot.SetScale(
      activeProfile,
      ClampScale(slot.GetScale(activeProfile) + delta)
    );
    RefreshSlot(slot);
    ShowEditorLabel();
  }

  private function ResetSelected()
  {
    var slot: W3VRHudEditorSlot;

    slot = GetSelectedSlot();
    if (!slot)
    {
      return;
    }

    slot.Reset(activeProfile);
    RefreshSlot(slot);
    ShowEditorLabel();
  }

  private function ResetActiveProfile()
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      slots[i].Reset(activeProfile);
    }
    RefreshAllManagedModules();
    ShowEditorLabel();
  }

  private function GetSelectedSlot(): W3VRHudEditorSlot
  {
    if (selectedIndex < 0 || selectedIndex >= slots.Size())
    {
      return NULL;
    }
    return slots[selectedIndex];
  }

  private function RefreshSlot(slot: W3VRHudEditorSlot)
  {
    var root: CScriptedFlashSprite;

    if (!slot)
    {
      return;
    }

    if (slot.moduleName == "TutorialPopup")
    {
      if (theGame.GetGuiManager())
      {
        ApplyTutorialPopupLayout(
          (CR4TutorialPopup)theGame.GetGuiManager().GetPopup(
            'TutorialPopup'
          ),
          true
        );
      }
      return;
    }

    if (!slot.module)
    {
      return;
    }

    SetModuleTarget(slot);
    slot.module.SnapToAnchorPosition();

    if (slot.module.w3vr_hud_editor_position_trace_pending)
    {
      root = slot.module.GetModuleFlash();
      if (root)
      {
        W3VRHudEditor_Trace(
          "POSITION SNAP_RETURN seq=" +
          IntToString(slot.module.w3vr_hud_editor_position_trace_sequence) +
          " module=" + slot.moduleName +
          " base_wrapper=MISS_OR_BYPASSED" +
          " flash=" + FloatToString(root.GetX()) + "," +
          FloatToString(root.GetY()),
          false
        );
        W3VRHudEditor_PersistPositionTrace(
          'SnapMissSeq',
          slot.module.w3vr_hud_editor_position_trace_sequence,
          slot.moduleName,
          root.GetX(), root.GetY(),
          slot.module.w3vr_hud_editor_target_x,
          slot.module.w3vr_hud_editor_target_y
        );
      }
      else
      {
        W3VRHudEditor_Trace(
          "POSITION SNAP_RETURN seq=" +
          IntToString(slot.module.w3vr_hud_editor_position_trace_sequence) +
          " module=" + slot.moduleName +
          " base_wrapper=MISS_OR_BYPASSED flash=missing",
          false
        );
        W3VRHudEditor_PersistPositionTrace(
          'SnapMissSeq',
          slot.module.w3vr_hud_editor_position_trace_sequence,
          slot.moduleName,
          0.0f, 0.0f,
          slot.module.w3vr_hud_editor_target_x,
          slot.module.w3vr_hud_editor_target_y
        );
      }
    }
  }

  private function RefreshAllManagedModules()
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      RefreshSlot(slots[i]);
    }
  }

  private function SetModuleTarget(slot: W3VRHudEditorSlot)
  {
    slot.module.w3vr_hud_editor_managed = true;
    slot.module.w3vr_hud_editor_target_x = slot.GetX(activeProfile);
    slot.module.w3vr_hud_editor_target_y = slot.GetY(activeProfile);
    slot.module.w3vr_hud_editor_target_scale =
      ClampScale(slot.GetScale(activeProfile));
  }

  private function RemoveRegistryTransforms()
  {
    var i: int;

    for (i = 0; i < slots.Size(); i += 1)
    {
      if (slots[i] && slots[i].module)
      {
        slots[i].RestoreVisualState();
        slots[i].module.w3vr_hud_editor_managed = false;
        slots[i].module.SnapToAnchorPosition();
      }
    }
  }

  private function RefreshEditorVisuals()
  {
    var i: int;

    if (!editing || !thePlayer)
    {
      return;
    }

    for (i = 0; i < slots.Size(); i += 1)
    {
      slots[i].ApplyEditorVisual(i == selectedIndex);
    }
  }

  private function ShowEditorLabel()
  {
    var slot: W3VRHudEditorSlot;
    var profileName: string;
    var message: string;
    var guiManager: CR4GuiManager;

    if (!editing || !thePlayer)
    {
      return;
    }

    slot = GetSelectedSlot();
    if (!slot)
    {
      return;
    }

    if (activeProfile == 1)
    {
      profileName = "Cinema3D";
    }
    else
    {
      profileName = "VR";
    }

    message =
      "W3VR HUD Editor [" + profileName + "] " +
      IntToString(selectedIndex + 1) + "/" + IntToString(slots.Size()) +
      " - " + slot.displayName +
      " | X " + FloatToString(slot.GetX(activeProfile)) +
      " Y " + FloatToString(slot.GetY(activeProfile)) +
      " Scale " + FloatToString(slot.GetScale(activeProfile));

    if (!IsSlotLive(slot))
    {
      message += " | OFFLINE - settings will apply when available";
    }

    guiManager = theGame.GetGuiManager();
    if (guiManager)
    {
      // Overlay notifications use millisecond-like values in the vanilla UI.
      // A short owned-by-time label avoids hiding another system's message.
      guiManager.ShowNotification(message, 400.0f, false);
    }
  }

  private function AnnounceSelectedSlot()
  {
    var slot: W3VRHudEditorSlot;
    var availability: string;

    if (!editing || !thePlayer)
    {
      return;
    }

    slot = GetSelectedSlot();
    if (!slot)
    {
      return;
    }

    if (IsSlotLive(slot))
    {
      availability = "LIVE";
    }
    else
    {
      availability = "OFFLINE";
    }

    thePlayer.DisplayHudMessage(
      "HUD panel " + IntToString(selectedIndex + 1) + "/" +
      IntToString(slots.Size()) + " | " + slot.displayName +
      " [" + slot.moduleName + "] | " + availability
    );

    if (theGame.GetGuiManager())
    {
      theGame.GetGuiManager().ShowNotification(
        "HUD panel " + IntToString(selectedIndex + 1) + "/" +
        IntToString(slots.Size()) + " | " + slot.displayName +
        " [" + slot.moduleName + "] | " + availability,
        3000.0f,
        false
      );
    }
  }

  private function LoadSettings()
  {
    var i: int;
    var config: CInGameConfigWrapper;

    config = (CInGameConfigWrapper)theGame.GetInGameConfigWrapper();
    // Until the renderer becomes authoritative, always start gameplay on the
    // safe VR bank. Tab changes only the current runtime/editor preview.
    activeProfile = 0;
    if (!config)
    {
      return;
    }

    for (i = 0; i < slots.Size(); i += 1)
    {
      slots[i].vrX = ReadConfigFloat(
        config, 'W3VRHudEditorVR', slots[i].xVar, 0.0f
      );
      slots[i].vrY = ReadConfigFloat(
        config, 'W3VRHudEditorVR', slots[i].yVar, 0.0f
      );
      slots[i].vrScale = ClampScale(ReadConfigFloat(
        config, 'W3VRHudEditorVR', slots[i].scaleVar, 1.0f
      ));

      slots[i].cinemaX = ReadConfigFloat(
        config, 'W3VRHudEditorCinema3D', slots[i].xVar, 0.0f
      );
      slots[i].cinemaY = ReadConfigFloat(
        config, 'W3VRHudEditorCinema3D', slots[i].yVar, 0.0f
      );
      slots[i].cinemaScale = ClampScale(ReadConfigFloat(
        config, 'W3VRHudEditorCinema3D', slots[i].scaleVar, 1.0f
      ));
    }
  }

  private function SaveSettings()
  {
    var i: int;
    var config: CInGameConfigWrapper;

    config = (CInGameConfigWrapper)theGame.GetInGameConfigWrapper();
    if (!config)
    {
      return;
    }

    for (i = 0; i < slots.Size(); i += 1)
    {
      WriteSlotProfile(config, slots[i], 0);
      WriteSlotProfile(config, slots[i], 1);
    }
    theGame.SaveUserSettings();
  }

  private function WriteSlotProfile(
    config: CInGameConfigWrapper,
    slot: W3VRHudEditorSlot,
    profile: int
  )
  {
    var groupName: name;

    if (profile == 1)
    {
      groupName = 'W3VRHudEditorCinema3D';
    }
    else
    {
      groupName = 'W3VRHudEditorVR';
    }

    config.SetVarValue(
      groupName, slot.xVar, FloatToString(slot.GetX(profile))
    );
    config.SetVarValue(
      groupName, slot.yVar, FloatToString(slot.GetY(profile))
    );
    config.SetVarValue(
      groupName, slot.scaleVar, FloatToString(slot.GetScale(profile))
    );
  }

  private function ReadConfigFloat(
    config: CInGameConfigWrapper,
    groupName: name,
    varName: name,
    fallback: float
  ): float
  {
    var value: string;

    value = config.GetVarValue(groupName, varName);
    if (value == "")
    {
      return fallback;
    }
    return StringToFloat(value);
  }

  private function ClampScale(value: float): float
  {
    if (value < 0.10f)
    {
      return 0.10f;
    }
    if (value > 5.00f)
    {
      return 5.00f;
    }
    return value;
  }

  private function ClampOffset(value: float, limit: float): float
  {
    if (value < -limit)
    {
      return -limit;
    }
    if (value > limit)
    {
      return limit;
    }
    return value;
  }
}

@addField(CR4ScriptedHud)
var w3vr_hud_editor_controller: W3VRHudEditorController;

@addField(CR4Game)
var w3vr_hud_editor_script_probe_shown: bool;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_managed: bool;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_target_x: float;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_target_y: float;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_target_scale: float;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_position_trace_pending: bool;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_position_trace_sequence: int;

@addField(CR4HudModuleBase)
var w3vr_hud_editor_position_trace_name: string;

@addField(CR4HudModuleConsole)
var w3vr_hud_editor_preview_active: bool;

@addField(CR4HudModuleConsole)
var w3vr_hud_editor_preview_next_refresh: float;

@addField(CR4HudModuleJournalUpdate)
var w3vr_hud_editor_preview_active: bool;

@addField(CR4HudModuleAreaInfo)
var w3vr_hud_editor_preview_active: bool;

@addField(CR4TutorialPopup)
var w3vr_hud_editor_last_data: W3TutorialPopupData;

@addField(CR4TutorialPopup)
var w3vr_hud_editor_applied_x: float;

@addField(CR4TutorialPopup)
var w3vr_hud_editor_applied_y: float;

@addMethod(CR4TutorialPopup)
function W3VRHudEditorApplyLayout(
  offsetX: float,
  offsetY: float,
  refresh: bool
)
{
  if (!this.m_DataObject)
  {
    return;
  }

  if (this.w3vr_hud_editor_last_data != this.m_DataObject)
  {
    this.w3vr_hud_editor_last_data = this.m_DataObject;
    this.w3vr_hud_editor_applied_x = 0.0f;
    this.w3vr_hud_editor_applied_y = 0.0f;
  }

  this.m_DataObject.posX -= this.w3vr_hud_editor_applied_x;
  this.m_DataObject.posY -= this.w3vr_hud_editor_applied_y;
  this.w3vr_hud_editor_applied_x = offsetX / 1920.0f;
  this.w3vr_hud_editor_applied_y = offsetY / 1080.0f;
  this.m_DataObject.posX += this.w3vr_hud_editor_applied_x;
  this.m_DataObject.posY += this.w3vr_hud_editor_applied_y;

  if (refresh)
  {
    this.CreateTutorialHint(false);
  }
}

@addMethod(CR4TutorialPopup)
function W3VRHudEditorClosePreview()
{
  if (
    this.m_DataObject &&
    this.m_DataObject.scriptTag == 'W3VRHudEditorPreview'
  )
  {
    this.RequestClose();
  }
}

@addMethod(CR4HudModuleConsole)
function W3VRHudEditorShowPreview()
{
  var now: float;

  // Never cover a real Action Log message that is already on screen. Pending
  // messages do not block the dedicated vanilla debug preview route.
  if (this._iDuringDisplay > 0)
  {
    return;
  }

  now = theGame.GetEngineTimeAsSeconds();
  if (
    this.w3vr_hud_editor_preview_active &&
    now < this.w3vr_hud_editor_preview_next_refresh
  )
  {
    return;
  }

  this.ConsoleTest();
  this.ShowElement(true, true);
  this.w3vr_hud_editor_preview_active = true;
  this.w3vr_hud_editor_preview_next_refresh = now + 0.75f;
}

@addMethod(CR4HudModuleConsole)
function W3VRHudEditorHidePreview()
{
  if (!this.w3vr_hud_editor_preview_active)
  {
    return;
  }
  this.ConsoleCleanup();
  this.w3vr_hud_editor_preview_active = false;
  this.w3vr_hud_editor_preview_next_refresh = 0.0f;
}

@addMethod(CR4HudModuleJournalUpdate)
function W3VRHudEditorShowPreview()
{
  if (this.w3vr_hud_editor_preview_active)
  {
    return;
  }
  if (this._bDuringDisplay || this.journalUpdates.Size() > 0)
  {
    return;
  }

  this.m_fxSetJournalUpdateStatusSFF.InvokeSelfOneArg(FlashArgInt(1));
  this.m_fxShowJournalUpdateSFF.InvokeSelfThreeArgs(
    FlashArgString("HUD EDITOR PREVIEW"),
    FlashArgString("JOURNAL UPDATES"),
    FlashArgNumber(600000.0f)
  );
  this.ShowElement(true, true);
  this.w3vr_hud_editor_preview_active = true;
}

@addMethod(CR4HudModuleJournalUpdate)
function W3VRHudEditorHidePreview()
{
  if (!this.w3vr_hud_editor_preview_active)
  {
    return;
  }
  this.m_fxClearJournalUpdateSFF.InvokeSelf();
  this.ShowElement(false, true);
  this.w3vr_hud_editor_preview_active = false;
}

@addMethod(CR4HudModuleAreaInfo)
function W3VRHudEditorShowPreview()
{
  if (this.w3vr_hud_editor_preview_active || this.bShow)
  {
    return;
  }

  this.m_fxSetTextSFF.InvokeSelfOneArg(
    FlashArgString("HUD EDITOR PREVIEW - AREA INFORMATION")
  );
  this.ShowElement(true, true);
  this.w3vr_hud_editor_preview_active = true;
}

@addMethod(CR4HudModuleAreaInfo)
function W3VRHudEditorHidePreview()
{
  if (!this.w3vr_hud_editor_preview_active)
  {
    return;
  }
  this.ShowElement(false, true);
  this.w3vr_hud_editor_preview_active = false;
}

@wrapMethod(CR4ScriptedHud)
function OnConfigUI()
{
  if (!this.w3vr_hud_editor_controller)
  {
    this.w3vr_hud_editor_controller =
      new W3VRHudEditorController in this;
  }
  this.w3vr_hud_editor_controller.BeginHudInitialization(this);

  wrappedMethod();

  this.w3vr_hud_editor_controller.Initialize(this);
}

@wrapMethod(CR4ScriptedHud)
function AddHudModuleReference(hudModule: CR4HudModuleBase)
{
  wrappedMethod(hudModule);

  if (!this.w3vr_hud_editor_controller)
  {
    this.w3vr_hud_editor_controller =
      new W3VRHudEditorController in this;
  }
  this.w3vr_hud_editor_controller.RegisterHudModule(this, hudModule);
}

@wrapMethod(CR4ScriptedHud)
function OnTick(timeDelta: float)
{
  wrappedMethod(timeDelta);

  if (this.w3vr_hud_editor_controller)
  {
    this.w3vr_hud_editor_controller.PostHudTick();
  }
}

@wrapMethod(CR4HudModuleAreaInfo)
function OnConfigUI()
{
  var hud: CR4ScriptedHud;

  wrappedMethod();

  hud = (CR4ScriptedHud)theGame.GetHud();
  if (hud)
  {
    if (!hud.w3vr_hud_editor_controller)
    {
      hud.w3vr_hud_editor_controller =
        new W3VRHudEditorController in hud;
    }
    hud.w3vr_hud_editor_controller.RegisterAreaInfoModule(hud, this);
  }
}

@wrapMethod(CR4HudModuleConsole)
function OnMessageHidden(value: string)
{
  wrappedMethod(value);

  // The Flash animation owns its short lifetime. Re-arm only the editor
  // preview flag; the controller recreates it while this slot stays selected.
  if (this.w3vr_hud_editor_preview_active)
  {
    this.w3vr_hud_editor_preview_active = false;
  }
}

@wrapMethod(CR4TutorialPopup)
function CreateTutorialHint(optional showAnimation: bool)
{
  var ownerHud: CR4ScriptedHud;

  ownerHud = (CR4ScriptedHud)theGame.GetHud();
  if (ownerHud && ownerHud.w3vr_hud_editor_controller)
  {
    ownerHud.w3vr_hud_editor_controller.ApplyTutorialPopupLayout(
      this, false
    );
  }

  wrappedMethod(showAnimation);
}

function W3VRHudEditor_Drive()
{
  var hud: CR4ScriptedHud;

  hud = (CR4ScriptedHud)theGame.GetHud();
  if (!hud)
  {
    return;
  }

  if (!hud.w3vr_hud_editor_controller)
  {
    hud.w3vr_hud_editor_controller =
      new W3VRHudEditorController in hud;
  }
  hud.w3vr_hud_editor_controller.Drive(hud);
}

@wrapMethod(CR4Game)
function OnTick()
{
  wrappedMethod();

  // This probe is deliberately owned by CR4Game, not by the HUD controller:
  // if it appears, the script and global tick wrapper are alive even when HUD
  // module discovery is still incomplete.
  if (!this.w3vr_hud_editor_script_probe_shown && thePlayer)
  {
    if (theGame.GetHud())
    {
      W3VRHudEditor_Trace(
        W3VRHudEditor_Version() + " script-loaded | hud=available",
        true
      );
    }
    else
    {
      W3VRHudEditor_Trace(
        W3VRHudEditor_Version() + " script-loaded | hud=missing",
        true
      );
    }
    this.w3vr_hud_editor_script_probe_shown = true;
  }

  W3VRHudEditor_Drive();
}

@wrapMethod(CR4HudModuleBase)
function UpdateScale(
  scale: float,
  flashModule: CScriptedFlashSprite
): bool
{
  if (this.w3vr_hud_editor_managed)
  {
    scale *= this.w3vr_hud_editor_target_scale;
  }
  return wrappedMethod(scale, flashModule);
}

@wrapMethod(CR4HudModuleBase)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var horizontalFrameScale: float;
  var verticalFrameScale: float;
  var tracePending: bool;
  var traceSequence: int;
  var traceName: string;
  var originalAnchorX: float;
  var originalAnchorY: float;
  var root: CScriptedFlashSprite;

  tracePending = this.w3vr_hud_editor_position_trace_pending;
  traceSequence = this.w3vr_hud_editor_position_trace_sequence;
  traceName = this.w3vr_hud_editor_position_trace_name;
  originalAnchorX = anchorX;
  originalAnchorY = anchorY;

  if (this.w3vr_hud_editor_managed)
  {
    horizontalFrameScale = theGame.GetUIHorizontalFrameScale();
    verticalFrameScale = theGame.GetUIVerticalFrameScale();

    if (AbsF(horizontalFrameScale) > 0.001f)
    {
      anchorX +=
        this.w3vr_hud_editor_target_x / horizontalFrameScale;
    }
    else
    {
      anchorX += this.w3vr_hud_editor_target_x;
    }

    if (AbsF(verticalFrameScale) > 0.001f)
    {
      anchorY +=
        this.w3vr_hud_editor_target_y / verticalFrameScale;
    }
    else
    {
      anchorY += this.w3vr_hud_editor_target_y;
    }
  }

  if (tracePending)
  {
    W3VRHudEditor_Trace(
      "POSITION WRAPPER_IN seq=" + IntToString(traceSequence) +
      " module=" + traceName +
      " anchor=" + FloatToString(originalAnchorX) + "," +
      FloatToString(originalAnchorY) +
      " adjusted=" + FloatToString(anchorX) + "," +
      FloatToString(anchorY) +
      " target=" + FloatToString(this.w3vr_hud_editor_target_x) + "," +
      FloatToString(this.w3vr_hud_editor_target_y) +
      " frame=" + FloatToString(horizontalFrameScale) + "," +
      FloatToString(verticalFrameScale),
      false
    );
    W3VRHudEditor_PersistPositionTrace(
      'WrapperInSeq', traceSequence, traceName,
      originalAnchorX, originalAnchorY, anchorX, anchorY
    );
  }

  wrappedMethod(anchorX, anchorY);

  if (tracePending)
  {
    root = this.GetModuleFlash();
    if (root)
    {
      W3VRHudEditor_Trace(
        "POSITION WRAPPER_OUT seq=" + IntToString(traceSequence) +
        " module=" + traceName +
        " flash=" + FloatToString(root.GetX()) + "," +
        FloatToString(root.GetY()),
        false
      );
      W3VRHudEditor_PersistPositionTrace(
        'WrapperOutSeq', traceSequence, traceName,
        root.GetX(), root.GetY(),
        this.w3vr_hud_editor_target_x,
        this.w3vr_hud_editor_target_y
      );
    }
    else
    {
      W3VRHudEditor_Trace(
        "POSITION WRAPPER_OUT seq=" + IntToString(traceSequence) +
        " module=" + traceName + " flash=missing",
        false
      );
      W3VRHudEditor_PersistPositionTrace(
        'WrapperOutSeq', traceSequence, traceName,
        0.0f, 0.0f,
        this.w3vr_hud_editor_target_x,
        this.w3vr_hud_editor_target_y
      );
    }
    this.w3vr_hud_editor_position_trace_pending = false;
  }
}

// ControlsFeedback owns a derived UpdatePosition() implementation and never
// dispatches through CR4HudModuleBase.UpdatePosition(). Apply the same target
// at the derived owner, in final HUD pixels because this override writes the
// incoming anchors directly after its own frame-scale compensation.
@wrapMethod(CR4HudModuleControlsFeedback)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var tracePending: bool;
  var traceSequence: int;
  var traceName: string;
  var originalAnchorX: float;
  var originalAnchorY: float;
  var root: CScriptedFlashSprite;

  tracePending = this.w3vr_hud_editor_position_trace_pending;
  traceSequence = this.w3vr_hud_editor_position_trace_sequence;
  traceName = this.w3vr_hud_editor_position_trace_name;
  originalAnchorX = anchorX;
  originalAnchorY = anchorY;

  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }

  if (tracePending)
  {
    W3VRHudEditor_PersistPositionTrace(
      'WrapperInSeq', traceSequence, traceName,
      originalAnchorX, originalAnchorY, anchorX, anchorY
    );
  }

  wrappedMethod(anchorX, anchorY);

  if (tracePending)
  {
    root = this.GetModuleFlash();
    if (root)
    {
      W3VRHudEditor_PersistPositionTrace(
        'WrapperOutSeq', traceSequence, traceName,
        root.GetX(), root.GetY(),
        this.w3vr_hud_editor_target_x,
        this.w3vr_hud_editor_target_y
      );
    }
    else
    {
      W3VRHudEditor_PersistPositionTrace(
        'WrapperOutSeq', traceSequence, traceName,
        0.0f, 0.0f,
        this.w3vr_hud_editor_target_x,
        this.w3vr_hud_editor_target_y
      );
    }
    this.w3vr_hud_editor_position_trace_pending = false;
  }
}

@wrapMethod(CR4HudModuleHorseStaminaBar)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var originalAnchorX: float;
  var originalAnchorY: float;

  originalAnchorX = anchorX;
  originalAnchorY = anchorY;
  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }
  W3VRHudEditor_BeginDerivedPositionTrace(
    this, originalAnchorX, originalAnchorY, anchorX, anchorY
  );
  wrappedMethod(anchorX, anchorY);
  W3VRHudEditor_EndDerivedPositionTrace(this);
}

@wrapMethod(CR4HudModuleHorsePanicBar)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var originalAnchorX: float;
  var originalAnchorY: float;

  originalAnchorX = anchorX;
  originalAnchorY = anchorY;
  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }
  W3VRHudEditor_BeginDerivedPositionTrace(
    this, originalAnchorX, originalAnchorY, anchorX, anchorY
  );
  wrappedMethod(anchorX, anchorY);
  W3VRHudEditor_EndDerivedPositionTrace(this);
}

@wrapMethod(CR4HudModuleBuffs)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var originalAnchorX: float;
  var originalAnchorY: float;

  originalAnchorX = anchorX;
  originalAnchorY = anchorY;
  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }
  W3VRHudEditor_BeginDerivedPositionTrace(
    this, originalAnchorX, originalAnchorY, anchorX, anchorY
  );
  wrappedMethod(anchorX, anchorY);
  W3VRHudEditor_EndDerivedPositionTrace(this);
}

@wrapMethod(CR4HudModuleItemInfo)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var originalAnchorX: float;
  var originalAnchorY: float;

  originalAnchorX = anchorX;
  originalAnchorY = anchorY;
  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }
  W3VRHudEditor_BeginDerivedPositionTrace(
    this, originalAnchorX, originalAnchorY, anchorX, anchorY
  );
  wrappedMethod(anchorX, anchorY);
  W3VRHudEditor_EndDerivedPositionTrace(this);
}

@wrapMethod(CR4HudModuleConsole)
function UpdatePosition(anchorX: float, anchorY: float)
{
  var originalAnchorX: float;
  var originalAnchorY: float;

  originalAnchorX = anchorX;
  originalAnchorY = anchorY;
  if (this.w3vr_hud_editor_managed)
  {
    anchorX += this.w3vr_hud_editor_target_x;
    anchorY += this.w3vr_hud_editor_target_y;
  }
  W3VRHudEditor_BeginDerivedPositionTrace(
    this, originalAnchorX, originalAnchorY, anchorX, anchorY
  );
  wrappedMethod(anchorX, anchorY);
  W3VRHudEditor_EndDerivedPositionTrace(this);
}

// Reserved clean integration seam. No DLL invokes this during isolated work.
// 0 = VR, 1 = Cinema3D.
function W3VRHudEditor_SetRendererProfile(profile: int)
{
  var hud: CR4ScriptedHud;

  hud = (CR4ScriptedHud)theGame.GetHud();
  if (hud && hud.w3vr_hud_editor_controller)
  {
    hud.w3vr_hud_editor_controller.SetActiveProfile(profile);
  }
}
