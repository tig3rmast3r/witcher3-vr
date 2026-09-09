foreach(required_variable IN ITEMS
        LAUNCHER_MAIN
        LAUNCHER_CONFIG
        LAUNCHER_HEADER
        LAUNCHER_DEFAULTS
        STARTUP_CHECKS
        DXGI_PROXY_SOURCE)
    if(NOT DEFINED ${required_variable} OR
            NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR "${required_variable} was not provided")
    endif()
endforeach()

file(READ "${LAUNCHER_MAIN}" main)
file(READ "${LAUNCHER_CONFIG}" config)
file(READ "${LAUNCHER_HEADER}" header)
file(READ "${LAUNCHER_DEFAULTS}" defaults)
file(READ "${STARTUP_CHECKS}" startup_checks)
file(READ "${DXGI_PROXY_SOURCE}" dxgi_proxy)

foreach(required IN ITEMS
        "IdIntegrationMode"
        "L\"Integration\""
        "Select one complete integration state."
        "DLSS5 reference is read-only"
        "F2  Toggle between Symmetric and Asymmetric projection"
        "Toggle between symmetric and asymmetric projection."
        "HUD Editor bindings"
        "Open the HUD Editor; press Insert again"
        "Select the previous or next HUD panel"
        "Move the selected HUD panel"
        "Resize the selected HUD panel"
        "Reset the selected HUD panel"
        "Reset the complete active HUD profile"
        "F7  Toggle Layout A/B"
        "Toggle the HUD Editor preview between layout A and layout B."
        "state.integration_mode = static_cast<IntegrationMode>(integration_mode)"
        "L\"Debug and integrations\""
        "Diagnostic Logging"
        "L\"Route Log\""
        "Performance Log"
        "IdRenderDoc"
        "L\"RenderDoc\""
        "dedicated package on the release page"
        "F3  Fast capture: Route / Performance / RenderDoc"
        "Ctrl+F6  AFW visual debug"
        "do not impact performance"
        "can contaminate results"
        "Additional cutscene-only HUD and subtitle scale applied on top of the HUD Editor profile"
        "Additional cutscene-only Cinema3D depth correction applied on top of the HUD Editor profile"
        "Additional cutscene-only Full VR depth correction applied on top of the HUD Editor profile"
        "Witcher 3 VR Launcher - V"
        "ShowStartupWarnings();"
        "constexpr int kClientWidth = 1180;"
    "constexpr int kClientHeight = 782;"
        "600, 18, 560, 178"
        "600, 462, 560, 160"
        "https://ko-fi.com/tig3rmast3r")
    string(FIND "${main}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 launcher UI contract: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "IdOptiscaler"
        "IdEnableReshade"
        "IdEnableDlss5"
        "UpdateManagedIntegrationControls"
        "IdRayTracing"
        "IdNativeStereo"
        "Ray Tracing (AER + AFW)"
        "Asymmetric Projection (Experimental)"
        "vertical mouse/pad pitch (Experimental)"
        "First Person (Experimental)"
        "F2  Toggle SYM / ASYM"
        "F7  Switch Full VR / Cinema3D"
        "when Diagnostic or Performance logging is enabled"
        "Route Log (default ON)")
    string(FIND "${main}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Superseded V1414 launcher UI remains: ${forbidden}")
    endif()
endforeach()

string(TOLOWER "${main}" main_lower)
foreach(forbidden_default_phrase IN ITEMS
        "enabled by default"
        "disabled by default")
    string(FIND "${main_lower}" "${forbidden_default_phrase}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "Default-state wording remains in a V1414 tooltip: ${forbidden_default_phrase}")
    endif()
endforeach()

foreach(required IN ITEMS
        "IntegrationMode integration_mode{IntegrationMode::Off};"
        "bool route_logging{};"
        "bool performance_logging{};"
        "bool renderdoc_enabled{};")
    string(FIND "${header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 launcher state: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "vr_ini.Set(\"openxr\", \"native_stereo\", \"1\")"
        "paths.optiscaler_bridge_ini"
        "IntegrationModeUsesOptiscaler(state.integration_mode)"
        "vr_ini.Set(\"launcher\", \"integration_mode\""
        "state.route_logging ? \"1\" : \"0\""
        "state.performance_logging ? \"1\" : \"0\""
        "vr_ini.Set(\"renderdoc\", \"enabled\""
        "state.renderdoc_enabled ? \"1\" : \"0\"")
    string(FIND "${config}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 launcher config contract: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "config_version=19"
        "[launcher]"
        "integration_mode=off"
        "[renderdoc]"
        "streamline_device_bridge=0"
        "route_flight_recorder=0"
        "pipeline_flight_recorder=0")
    string(FIND "${defaults}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 launcher default: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "IsWindowsHdrActive"
        "DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO"
        "IsRivaTunerStatisticsServerRunning"
        "RTSS.exe"
        "FindForeignDx12Dlls"
        "ignored_foreign_dll_signature"
        "ForeignDllWarningSignature"
        "OptiScaler.dll"
        "renderdoc.dll")
    string(FIND "${startup_checks}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 startup guard contract: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "[TRIAL:RESHADE-OPAQUE-SUBMIT-CLEANUP V23011]"
        "if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0 ||"
        "(GetAsyncKeyState(VK_F6) & 1) == 0)")
    string(FIND "${dxgi_proxy}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing V1414 always-available AFW debug contract: ${required}")
    endif()
endforeach()

string(FIND "${dxgi_proxy}"
    "if (!(g_config.runtime_diagnostics ||" stale_afw_gate)
if(NOT stale_afw_gate EQUAL -1)
    message(FATAL_ERROR "AFW Ctrl+F6 still depends on Diagnostic or Performance logging")
endif()

message(STATUS "V1414 launcher UI/config/startup guard contract verified")
