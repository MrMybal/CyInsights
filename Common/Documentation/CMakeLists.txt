find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_filename_component(CY_HELP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# Generation validates the complete manual. Run each application build so newly
# added Markdown, translations and contract changes cannot leave stale help.
add_custom_target(CyDocumentation
    COMMAND "${Python3_EXECUTABLE}" "${CY_HELP_ROOT}/Tools/build_docs.py"
    WORKING_DIRECTORY "${CY_HELP_ROOT}" VERBATIM)
