if(NOT DEFINED HUD_EDITOR_SCRIPT OR NOT EXISTS "${HUD_EDITOR_SCRIPT}")
    message(FATAL_ERROR "HUD editor script was not provided")
endif()

file(READ "${HUD_EDITOR_SCRIPT}" hud_editor)

set(required_fragments
    "return \"V1260\";"
    "function BootstrapSubtitlePreviewLayout("
    "slot.moduleName != \"SubtitlesModule\""
    "temporaryY = savedY + 1.0f;"
    "temporaryY = savedY - 1.0f;"
    "slot.SetY(activeProfile, temporaryY);"
    "slot.SetY(activeProfile, savedY);"
    "subtitlePreviewWasActive ="
    "!subtitlePreviewWasActive &&"
    "BootstrapSubtitlePreviewLayout(slot);"
    "[FIX:SUBTITLE-PREVIEW-NUDGE-BOOTSTRAP V1247]"
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${hud_editor}" "${fragment}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing subtitle preview nudge contract: ${fragment}")
    endif()
endforeach()

string(FIND "${hud_editor}" "ReassertSubtitleLayout();" continuous_reassert)
if(NOT continuous_reassert EQUAL -1)
    message(FATAL_ERROR "V1247 must not retain V1246's per-frame reassert")
endif()

string(FIND "${hud_editor}" "slot.SetY(activeProfile, temporaryY);" temporary_position)
string(FIND "${hud_editor}" "slot.SetY(activeProfile, savedY);" restore_position)
if(temporary_position GREATER restore_position)
    message(FATAL_ERROR "Temporary subtitle nudge must precede exact restoration")
endif()

message(STATUS "HUD subtitle preview edge-nudge contract verified")
