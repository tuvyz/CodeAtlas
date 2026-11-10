cmake_minimum_required(VERSION 3.16)
project(VisorGUI LANGUAGES C CXX)

# Библиотека подключается add_subdirectory из папки с именем VisorGUI: заголовки берутся как <VisorGUI/...>.
# Зависимости (SDL2, FreeType, glm, загрузчик OpenGL - по умолчанию GLEW) ищет и подключает программа:
# пути к их заголовкам - в VGUI_DEPENDENCY_INCLUDE_DIRS до add_subdirectory, библиотеки линкует сама программа

file(GLOB VGUI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Widgets/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/guiengine.cpp)

add_library(VisorGUI STATIC ${VGUI_SOURCES})
if(WIN32)
    # Диалог файлов Windows (GetOpenFileNameW); NOMINMAX - windows.h без макросов min и max в единицах библиотеки
    target_link_libraries(VisorGUI PUBLIC comdlg32)
    target_compile_definitions(VisorGUI PRIVATE NOMINMAX)
endif()
target_compile_features(VisorGUI PUBLIC cxx_std_20)
get_filename_component(VGUI_PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(VisorGUI PUBLIC ${VGUI_PARENT_DIR} ${VGUI_DEPENDENCY_INCLUDE_DIRS})
target_compile_definitions(VisorGUI PRIVATE VGUI_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
if(MSVC)
    target_compile_options(VisorGUI PRIVATE /utf-8)
endif()

# Пример: окно с кнопками, полем ввода и списком
option(VGUI_BUILD_EXAMPLE "Собирать пример VisorGUI" OFF)
if(VGUI_BUILD_EXAMPLE)
    add_executable(VisorGUIExample ${CMAKE_CURRENT_SOURCE_DIR}/Example/main.cpp)
    target_link_libraries(VisorGUIExample PRIVATE VisorGUI ${VGUI_EXAMPLE_LIBRARIES})
endif()
