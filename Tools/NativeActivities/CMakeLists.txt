cmake_minimum_required(VERSION 3.21)
project(CyNativeActivities LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
if(MSVC)
 string(REPLACE "┬á" " " CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
 set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CY_UE_ENGINE "" CACHE PATH "Licensed installed engine; its binaries are not rebuilt or redistributed")
if(NOT EXISTS "${CY_UE_ENGINE}/Source/Developer/TraceAnalysis/Public/Trace/Analysis.h")
 message(FATAL_ERROR "Set CY_UE_ENGINE to the Engine directory of your licensed UE installation")
endif()
set(CY_UE_LIBS "${CY_UE_ENGINE}/Intermediate/Build/Win64/x64/UnrealInsights/Development")
add_executable(CyNativeActivities NativeActivities.cpp)
target_include_directories(CyNativeActivities PRIVATE "${CY_UE_ENGINE}/Source" "${CY_UE_ENGINE}/Source/Developer/TraceServices/Public" "${CY_UE_ENGINE}/Source/Runtime/CoreUObject/Public" "${CY_UE_ENGINE}/Source/Runtime/Core/Public"
 "${CY_UE_ENGINE}/Source/Runtime/TraceLog/Public" "${CY_UE_ENGINE}/Source/Developer/TraceAnalysis/Public"
 "${CY_UE_ENGINE}/Source/Runtime/AutoRTFM/Public")
target_compile_definitions(CyNativeActivities PRIVATE UBT_COMPILED_PLATFORM=Windows PLATFORM_WINDOWS=1
 UE_BUILD_DEVELOPMENT=1 WITH_EDITOR=0 WITH_ENGINE=0 WITH_UNREAL_DEVELOPER_TOOLS=1 WITH_SERVER_CODE=0
 WITH_PLUGIN_SUPPORT=0 IS_MONOLITHIC=0 IS_PROGRAM=1 UE_ENABLE_ICU=0 UE_MERGED_MODULES=0
 CORE_API=__declspec\(dllimport\) COREUOBJECT_API=__declspec\(dllimport\) TRACESERVICES_API=__declspec\(dllimport\) TRACEANALYSIS_API=__declspec\(dllimport\)
 TRACELOG_API=__declspec\(dllimport\) UE_TRACE_ENABLED=1
 COREUOBJECT_NON_ATTRIBUTED_API=__declspec\(dllimport\) CORE_NON_ATTRIBUTED_API=__declspec\(dllimport\)
 _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00 UNICODE _UNICODE)
target_compile_options(CyNativeActivities PRIVATE /utf-8 /EHsc /sdl /guard:cf /Zc:__cplusplus /Zc:preprocessor)
target_link_options(CyNativeActivities PRIVATE /DYNAMICBASE /NXCOMPAT /guard:cf)
target_link_libraries(CyNativeActivities PRIVATE "${CY_UE_LIBS}/TraceServices/UnrealInsights-TraceServices.lib" "${CY_UE_LIBS}/Core/UnrealInsights-Core.lib"
 "${CY_UE_LIBS}/TraceAnalysis/UnrealInsights-TraceAnalysis.lib")
option(CY_ACTIVITY_BUILD_FIXTURE "Build the synthetic native provider test writer" OFF)
if(CY_ACTIVITY_BUILD_FIXTURE)
 add_executable(CyActivityFixture Fixture.cpp)
 get_target_property(CY_ACTIVITY_INCLUDES CyNativeActivities INCLUDE_DIRECTORIES)
 get_target_property(CY_ACTIVITY_DEFINES CyNativeActivities COMPILE_DEFINITIONS)
 target_include_directories(CyActivityFixture PRIVATE ${CY_ACTIVITY_INCLUDES})
 target_compile_definitions(CyActivityFixture PRIVATE ${CY_ACTIVITY_DEFINES})
 target_compile_options(CyActivityFixture PRIVATE /utf-8 /EHsc /Zc:__cplusplus /Zc:preprocessor)
 # The Insights program disables its own trace writer. Use the installed
 # Editor's tracing-enabled Core only for this optional synthetic writer.
 set(CY_ACTIVITY_EDITOR_LIBS "${CY_UE_ENGINE}/Intermediate/Build/Win64/x64/UnrealEditor/Development")
 target_link_libraries(CyActivityFixture PRIVATE "${CY_ACTIVITY_EDITOR_LIBS}/Core/UnrealEditor-Core.lib" "${CY_ACTIVITY_EDITOR_LIBS}/TraceLog/UnrealEditor-TraceLog.lib")
endif()
