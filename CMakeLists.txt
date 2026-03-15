cmake_minimum_required(VERSION 3.19)
project(QtChatProgramClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON) # Meta-Object compiler (signals and slots)
set(CMAKE_AUTOUIC ON) # UI compiler (.ui)
set(CMAKE_AUTORCC ON) # Resources compiler (.qrc)

include_directories(header)
file(GLOB HEADERS "header/*.h")
file(GLOB SOURCES "src/*.cpp")
file(GLOB UI_FILES "src/*.ui")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    WIN32 MACOSX_BUNDLE
    ${HEADERS}
    ${SOURCES}
    ${UI_FILES}

)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Network
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
