# CMakeList.txt : projet CMake pour cli-client, incluez la source et définissez
# la logique spécifique au projet ici.
#
cmake_minimum_required (VERSION 3.8)

# Activez Rechargement à chaud pour les compilateurs MSVC si cela est pris en charge.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("nwa-cli")


include_directories("../lib" "./")
# Ajoutez une source à l'exécutable de ce projet.
add_executable (nwa-cli "cli-client.cpp" "../lib/nwaasiaoclient.cpp"  "../lib/nwaasio.cpp")

target_link_libraries(nwa-cli -static)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET nwa-cli PROPERTY CXX_STANDARD 17)
endif()

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
