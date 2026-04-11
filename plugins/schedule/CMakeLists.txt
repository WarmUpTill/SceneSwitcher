cmake_minimum_required(VERSION 3.14)
project(advanced-scene-switcher-macro-schedule)

add_library(${PROJECT_NAME} MODULE)

target_sources(
  ${PROJECT_NAME}
  PRIVATE macro-schedule.cpp
          macro-schedule.hpp
          macro-schedule-entry-dialog.cpp
          macro-schedule-entry-dialog.hpp
          macro-schedule-tab.cpp
          macro-schedule-tab.hpp
          calendar/calendar-event.hpp
          calendar/calendar-view.hpp
          calendar/calendar-month-view.hpp
          calendar/calendar-month-view.cpp
          calendar/calendar-day-view.hpp
          calendar/calendar-day-view.cpp
          calendar/calendar-week-view.hpp
          calendar/calendar-week-view.cpp
          calendar/calendar-widget.hpp
          calendar/calendar-widget.cpp)

setup_advss_plugin(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
install_advss_plugin(${PROJECT_NAME})
