cmake_minimum_required(VERSION 3.20)
project(CodeAtlas LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Подмодули (Libs/VisorGUI): клон без --recurse-submodules получает их здесь, а submodule.recurse в настройках клона
# обновляет их при git pull и checkout - глобальная настройка git на каждом компьютере не нужна
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} config submodule.recurse true WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Libs/VisorGUI/CMakeLists.txt")
        message(STATUS "Подмодули git: подтягиваю Libs/VisorGUI")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()

# Исходники программы
file(GLOB_RECURSE CODEATLAS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Analyzer/*.cpp"
    "${CMAKE_SOURCE_DIR}/Graph/*.cpp"
    "${CMAKE_SOURCE_DIR}/Project/*.cpp"
    "${CMAKE_SOURCE_DIR}/Layout/*.cpp"
    "${CMAKE_SOURCE_DIR}/Git/*.cpp"
    "${CMAKE_SOURCE_DIR}/Canvas/*.cpp"
    "${CMAKE_SOURCE_DIR}/App/*.cpp"
    "${CMAKE_SOURCE_DIR}/Settings/*.cpp"
    "${CMAKE_SOURCE_DIR}/Ide/*.cpp"
)
list(APPEND CODEATLAS_SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

if(WIN32)
    # Иконка exe - ресурсом (путь к .ico полный: rc ищет относительные пути от своей папки сборки)
    set(CODEATLAS_ICON_PATH "${CMAKE_SOURCE_DIR}/Resources/Icons/CodeAtlas.ico")
    configure_file(${CMAKE_SOURCE_DIR}/Resources/CodeAtlas.rc.in ${CMAKE_BINARY_DIR}/CodeAtlas.rc @ONLY)
    list(APPEND CODEATLAS_SOURCES ${CMAKE_BINARY_DIR}/CodeAtlas.rc)
    add_executable(CodeAtlas WIN32 ${CODEATLAS_SOURCES})
else()
    add_executable(CodeAtlas ${CODEATLAS_SOURCES})
endif()

target_include_directories(CodeAtlas PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Libs
    ${CMAKE_SOURCE_DIR}/Libs/glm
    ${CMAKE_SOURCE_DIR}/Libs/fmt/include
)
target_compile_definitions(CodeAtlas PRIVATE
    FMT_HEADER_ONLY
    GLM_ENABLE_EXPERIMENTAL
    GLEW_STATIC
    CODEATLAS_RESOURCE_DIR="${CMAKE_SOURCE_DIR}/Resources"
)

if(WIN32)
    # libclang - из установленного LLVM (https://github.com/llvm/llvm-project/releases, LLVM-<версия>-win64.exe)
    set(CODEATLAS_LLVM_DIR "C:/Program Files/LLVM" CACHE PATH "Папка установленного LLVM (lib/libclang.lib, bin/libclang.dll)")
    if(NOT EXISTS "${CODEATLAS_LLVM_DIR}/lib/libclang.lib")
        message(FATAL_ERROR "Нет ${CODEATLAS_LLVM_DIR}/lib/libclang.lib: поставить LLVM установщиком LLVM-<версия>-win64.exe с https://github.com/llvm/llvm-project/releases/latest или указать CODEATLAS_LLVM_DIR")
    endif()
    target_compile_definitions(CodeAtlas PRIVATE CODEATLAS_LLVM_DIR="${CODEATLAS_LLVM_DIR}" _CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
    target_compile_options(CodeAtlas PRIVATE /utf-8 /MP)

    set(VGUI_DEPENDENCY_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/Libs/glm
        ${CMAKE_SOURCE_DIR}/Libs/SDL2/include
        ${CMAKE_SOURCE_DIR}/Libs/glew-2.2.0/include
        ${CMAKE_SOURCE_DIR}/Libs/freetype/include
    )
    add_subdirectory(Libs/VisorGUI)
    target_compile_options(VisorGUI PRIVATE /utf-8)
    target_compile_definitions(VisorGUI PUBLIC GLEW_STATIC)

    target_include_directories(CodeAtlas PRIVATE ${VGUI_DEPENDENCY_INCLUDE_DIRS})
    target_link_libraries(CodeAtlas PRIVATE
        VisorGUI
        ${CMAKE_SOURCE_DIR}/Libs/SDL2/lib/x64/SDL2.lib
        ${CMAKE_SOURCE_DIR}/Libs/SDL2/lib/x64/SDL2main.lib
        ${CMAKE_SOURCE_DIR}/Libs/glew-2.2.0/lib/Release/x64/glew32s.lib
        ${CMAKE_SOURCE_DIR}/Libs/freetype/lib/freetype.lib
        "${CODEATLAS_LLVM_DIR}/lib/libclang.lib"
        opengl32
        Comdlg32
        Shell32
        Ole32
        OleAut32
    )

    # SDL2.dll и libclang.dll рядом с exe (встроенные заголовки clang ищутся в CODEATLAS_LLVM_DIR - см. Analyzer::findClangResourceDir)
    add_custom_command(TARGET CodeAtlas POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/Libs/SDL2/lib/x64/SDL2.dll $<TARGET_FILE_DIR:CodeAtlas>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CODEATLAS_LLVM_DIR}/bin/libclang.dll" $<TARGET_FILE_DIR:CodeAtlas>
    )
else()
    # Linux: системные SDL2, GLEW, OpenGL, FreeType и libclang (пути - CMAKE_PREFIX_PATH)
    find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2 REQUIRED)
    find_library(SDL2_LIBRARY SDL2 REQUIRED)
    find_library(GLEW_LIBRARY NAMES libGLEW.a GLEW REQUIRED)
    find_library(OPENGL_LIBRARY NAMES GL REQUIRED)
    find_package(Freetype REQUIRED)
    find_package(Threads REQUIRED)
    set(CODEATLAS_LLVM_DIR "/usr/lib/llvm-18" CACHE PATH "Папка LLVM (lib/libclang, lib/clang/<версия>/include)")
    find_library(LIBCLANG_LIBRARY NAMES clang libclang.so.1 clang-18 libclang-18.so.1 PATHS ${CODEATLAS_LLVM_DIR}/lib REQUIRED)
    target_compile_definitions(CodeAtlas PRIVATE CODEATLAS_LLVM_DIR="${CODEATLAS_LLVM_DIR}")

    set(VGUI_DEPENDENCY_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/Libs/glm ${SDL2_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/Libs/glew-2.2.0/include ${FREETYPE_INCLUDE_DIRS})
    add_subdirectory(Libs/VisorGUI)
    target_compile_definitions(VisorGUI PUBLIC GLEW_STATIC GLEW_NO_GLU)

    target_include_directories(CodeAtlas PRIVATE ${VGUI_DEPENDENCY_INCLUDE_DIRS})
    target_link_libraries(CodeAtlas PRIVATE VisorGUI ${SDL2_LIBRARY} ${GLEW_LIBRARY} ${OPENGL_LIBRARY} ${FREETYPE_LIBRARIES} ${LIBCLANG_LIBRARY} Threads::Threads ${CMAKE_DL_LIBS})
endif()

# Шрифт рядом с exe: программа берёт его из исходников, а если их нет (exe перенесли) - из Resources рядом с собой
add_custom_command(TARGET CodeAtlas POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:CodeAtlas>/Resources
)
