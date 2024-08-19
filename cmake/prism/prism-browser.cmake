if(OS_WINDOWS)
  target_sources(
    obs-browser
    PRIVATE interaction/interaction_main_win.cpp
	          interaction/interaction_view_win.cpp
	          interaction/interaction_button.cpp
	          interaction/interaction_manager.cpp
	          interaction/interaction_image.cpp
	          interaction/interaction_delegate.cpp
	          interaction/osr_ime_handler_win.cc
	          interaction/interaction_window_helper.cpp
	          interaction/gdi_plus_helper.cpp

            interaction/interaction_util_win.hpp
	          interaction/interaction_hdc.hpp
	          interaction/interaction_main_win.h
	          interaction/interaction_view.h
	          interaction/interaction_button.h
	          interaction/interaction_manager.h
	          interaction/interaction_image.h
	          interaction/osr_ime_handler_win.h
            interaction/interaction_main.h
            interaction/interaction_view_win.h
            interaction/interaction_delegate.h
            interaction/interaction_def.h
            interaction/interaction_window_helper.h
            interaction/gdi_plus_helper.h

  )

  source_group(
    "interaction\\Source Files"
     FILES interaction/interaction_main_win.cpp
	         interaction/interaction_view.cpp
	         interaction/interaction_button.cpp
	         interaction/interaction_manager.cpp
	         interaction/interaction_image.cpp
	         interaction/osr_ime_handler_win.cc
	         interaction/interaction_view_win.cpp
	         interaction/interaction_delegate.cpp
	         interaction/interaction_window_helper.cpp
	         interaction/gdi_plus_helper.cpp
  )

  source_group(
    "interaction\\Header Files"
    FILES interaction/interaction_util_win.hpp
	        interaction/interaction_hdc.hpp
	        interaction/interaction_main_win.h
	        interaction/interaction_view.h
	        interaction/interaction_button.h
	        interaction/interaction_manager.h
	        interaction/interaction_image.h
	        interaction/osr_ime_handler_win.h
          interaction/interaction_main.h
          interaction/interaction_view_win.h
          interaction/interaction_delegate.h
          interaction/interaction_def.h
          interaction/interaction_window_helper.h
          interaction/gdi_plus_helper.h
  )
endif()

if(OS_MACOS)
target_sources(
    obs-browser
    PRIVATE 
            interaction_mac/PLSInteractionMacWindow.h
            interaction_mac/PLSInteractionMacWindow.mm
            interaction_mac/geometry_util.cc
            interaction_mac/geometry_util.h
            interaction_mac/text_input_client_osr_mac.mm
            interaction_mac/text_input_client_osr_mac.h
            interaction_mac/interaction_delegate.h
            interaction_mac/interaction_delegate.mm
  )

  source_group(
    "interaction_mac\\Source Files"
     FILES  interaction_mac/PLSInteractionMacWindow.h
            interaction_mac/PLSInteractionMacWindow.mm
            interaction_mac/geometry_util.cc
            interaction_mac/geometry_util.h
            interaction_mac/text_input_client_osr_mac.mm
            interaction_mac/text_input_client_osr_mac.h
            interaction_mac/interaction_delegate.h
            interaction_mac/interaction_delegate.mm
  )

set_source_files_properties(interaction_mac/PLSInteractionMacWindow.mm PROPERTIES COMPILE_FLAGS -fobjc-arc)
set_source_files_properties(interaction_mac/text_input_client_osr_mac.mm PROPERTIES COMPILE_FLAGS -fobjc-arc)
set_source_files_properties(interaction_mac/interaction_delegate.mm PROPERTIES COMPILE_FLAGS -fobjc-arc)

endif()