# Add script files
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/reforge_scripts.cpp")

# Add scripts to script loader
AC_ADD_SCRIPT_LOADER("Reforge" "${CMAKE_CURRENT_LIST_DIR}/src/reforge_loader.cpp")

