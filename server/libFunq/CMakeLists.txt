set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

set(
  FUNQ_SOURCES
  delayedresponse.cpp
  delayedresponse.h
  dragndropresponse.cpp
  dragndropresponse.h
  funq.cpp
  funq.h
  json.cpp
  json.h
  jsonclient.cpp
  jsonclient.h
  objectpath.cpp
  objectpath.h
  pick.cpp
  pick.h
  player.cpp
  player_commands_gitems.cpp
  player_commands_itemmodel.cpp
  player_commands_quickitems.cpp
  protocole.cpp
  protocole.h
  player_utils.cpp
  player_utils.h
  shortcutresponse.cpp
  shortcutresponse.h
)
if(FUNQ_BUILD_INJECTOR)
  if(WIN32)
    list(APPEND FUNQ_SOURCES WindowsInjector.cpp WindowsInjector.h)
  else()
    list(APPEND FUNQ_SOURCES ldPreloadInjector.cpp)
  endif()
endif()

set(
  FUNQ_DEPENDENCIES
  ${QT}::Core
  ${QT}::Gui
  ${QT}::Network
  ${QT}::Widgets
  ${QT}::Test
  $<$<BOOL:${WITH_QTQUICK}>:${QT}::Quick>
)

add_library(FunqStatic STATIC ${FUNQ_SOURCES})
target_link_libraries(FunqStatic PUBLIC ${FUNQ_DEPENDENCIES})

if(FUNQ_BUILD_INJECTOR)
  add_library(Funq SHARED ${FUNQ_SOURCES})
  target_link_libraries(Funq PUBLIC ${FUNQ_DEPENDENCIES})
  if(WIN32)
      set_target_properties(Funq PROPERTIES OUTPUT_NAME "libFunq")
  endif()
endif()
