option(PLUGIN_SAMPLE_EDITOR_PLUGIN "Build SamplePlugin" OFF)

set(CMAKE_AUTOMOC_RELAXED_MODE TRUE)

#modules
include("${TOOLS_CMAKE_DIR}/modules/FbxSdk.cmake")
#---

add_subdirectory("Code/Sandbox/Libs/CryQt")

set(EDITOR_DIR "Code/Sandbox/EditorQt" )
add_subdirectory("Code/Sandbox/EditorQt")
add_subdirectory("Code/Sandbox/Plugins/3DConnexionPlugin")
add_subdirectory("Code/Sandbox/Plugins/EditorConsole")

add_subdirectory("Code/Sandbox/Plugins/EditorCommon")
add_subdirectory("Code/Sandbox/EditorInterface")

add_subdirectory("Code/Sandbox/Plugins/CryDesigner")
add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor")
add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/common")
if(AUDIO_FMOD)
	add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorFmod")
endif()
if(AUDIO_SDL_MIXER)
	add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorSDLMixer")
endif()
if(AUDIO_PORTAUDIO)
	add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorPortAudio")
endif()
if (AUDIO_WWISE)
	add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorWwise")
endif()
if (AUDIO_ADX2)
	add_subdirectory("Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorAdx2")
endif()
add_subdirectory("Code/Sandbox/Plugins/EditorAnimation")
add_subdirectory("Code/Sandbox/Plugins/EditorDynamicResponseSystem")
add_subdirectory("Code/Sandbox/Plugins/EditorEnvironment")
add_subdirectory("Code/Sandbox/Plugins/EditorParticle")
if (PLUGIN_SCHEMATYC_EXPERIMENTAL)
	add_subdirectory("Code/Sandbox/Plugins/EditorSchematyc")
endif()
if (PLUGIN_SCHEMATYC)
	add_subdirectory("Code/Sandbox/Plugins/EditorSchematyc2")
	add_subdirectory("Code/Sandbox/Plugins/SchematycEditor")
endif()
add_subdirectory("Code/Sandbox/Plugins/EditorTrackView")
add_subdirectory("Code/Sandbox/Plugins/EditorInterimBehaviorTree")
add_subdirectory("Code/Sandbox/Plugins/EditorUDR")
add_subdirectory("Code/Sandbox/Plugins/EditorGameSDK")
add_subdirectory("Code/Sandbox/Plugins/FbxPlugin")
add_subdirectory("Code/Sandbox/Plugins/MeshImporter")
add_subdirectory("Code/Sandbox/Plugins/PerforcePlugin")
add_subdirectory("Code/Sandbox/Plugins/PerforcePlugin_Legacy")
add_subdirectory("Code/Sandbox/Plugins/SandboxPythonBridge")
if (PLUGIN_SAMPLE_EDITOR_PLUGIN)
	add_subdirectory("Code/Sandbox/Plugins/SamplePlugin")
endif()
add_subdirectory("Code/Sandbox/Plugins/VehicleEditor")
add_subdirectory("Code/Sandbox/Plugins/SmartObjectEditor")
add_subdirectory("Code/Sandbox/Plugins/DialogEditor")
add_subdirectory("Code/Sandbox/Plugins/MFCToolsPlugin")
add_subdirectory("Code/Sandbox/Plugins/FacialEditorPlugin")
add_subdirectory("Code/Sandbox/Plugins/DependencyGraph")
add_subdirectory("Code/Sandbox/Plugins/MaterialEditorPlugin")
add_subdirectory("Code/Sandbox/Plugins/PrefabAssetType")
add_subdirectory("Code/Sandbox/Plugins/CryTestRunnerPlugin")
if(EXISTS "${CRYENGINE_DIR}/Code/Sandbox/Plugins/LodGeneratorPlugin")
  add_subdirectory("Code/Sandbox/Plugins/LodGeneratorPlugin")
endif()
if(OPTION_SANDBOX_SUBSTANCE)
	include("${CRYENGINE_DIR}/Tools/CMake/modules/Substance.cmake")
	add_subdirectory("Code/Sandbox/Libs/CrySubstance")
	add_subdirectory("Code/Sandbox/Plugins/EditorSubstance")
endif()
if(OPTION_CRYMONO)
	add_subdirectory("Code/Sandbox/Plugins/EditorCSharp")
endif()
if(OPTION_UNIT_TEST AND PLUGIN_SAMPLE_EDITOR_PLUGIN)
	add_subdirectory("${CRYENGINE_DIR}/Code/Sandbox/UnitTests/SamplePluginUnitTest")
endif()
#libs
add_subdirectory ("Code/Libs/prt")
add_subdirectory ("Code/Libs/python")
