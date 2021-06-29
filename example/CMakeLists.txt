#set include dir for mochiController
add_subdirectory(basic)
add_subdirectory(icetExample)
add_subdirectory(MandelbulbCatalystExample)
add_subdirectory(MandelbulbColza)
add_subdirectory(GrayScottColza)
if(${ENABLE_DAMARIS})
  add_subdirectory(MandelbulbDamaris)
endif()
