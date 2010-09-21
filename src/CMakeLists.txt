

#
#    Copyright (c) 2010 Emanoil Kotsev <deloptes@yahoo.com>
#    $Id: deloptes$

#INCLUDE_DIRECTORIES( ${OPENSYNC_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src )
# 

SET( AKONADY_OPENSYNC_SRCS
  akonadi_opensync.cpp
  akonadisink.cpp
  datasink.cpp
  sinkbase.cpp
)


include_directories( ${AKONADI_INCLUDE_DIR} ${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${KDEPIMLIBS_INCLUDE_DIR}  )
include_directories( ${QT_INCLUDE_DIR} )
# 
include_directories( ${OPENSYNC_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src"  ${GLIB2_INCLUDE_DIR} ${GLIB2_MAIN_INCLUDE_DIR})
include_directories( "../build/src") 
# 
link_directories( ${AKONADI_LIB_DIR} ${KDE4_LIB_DIR} ${KDEPIMLIBS_LIB_DIR} ${OPENSYNC_LIBRARIES_DIR} ${glib2LibDir} )

AUTOMOC4( akonadi-sync AKONADY_OPENSYNC_SRCS )


OPENSYNC_PLUGIN_ADD( akonadi-sync ${AKONADY_OPENSYNC_SRCS} )

# ADD_DEPENDENCIES( akonadi-opensync akonadi_opensync.o akonadisink.o datasink.o sinkbase.o )

TARGET_LINK_LIBRARIES( akonadi-sync
  ${OPENSYNC_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${KDE4_KDECORE_LIBS}
  ${KDEPIMLIBS_AKONADI_LIBS} 
  ${KDEPIMLIBS_AKONADI_CONTACT_LIBS}
  ${KDEPIMLIBS_KCAL_LIBS}
)

###### INSTALL ###################
OPENSYNC_PLUGIN_INSTALL( akonadi-sync )
OPENSYNC_PLUGIN_CONFIG( akonadi-sync )
#INSTALL( FILES akonadi_opensync_plugin DESTINATION "/opt/testing/opensync" )
