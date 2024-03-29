#
#
# (c) Deloptes deloptes@yahoo.com
# $Id$
# 
#

CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )
PROJECT( top-level C CXX )

SET( VERSION "0.40" )
SET ( PROJECT_NAME akonadi_opensync_plugin )

SET( CMAKE_CXX_FLAGS  ${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS} )
# SET( CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -ggdb -g -O2 -Wall -W " ${CMAKE_CXX_FLAGS_DEBUG} ${KDE4_ENABLE_EXCEPTIONS} )

#ADD_DEFINITIONS(-g -O2 -fsigned-char -freg-struct-return -Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Winline -Werror)
# ADD_DEFINITIONS( -DDEBUG -g -O2 -Wall -W -fexceptions)
# -Werror
ADD_DEFINITIONS( -ggdb -O2 -Wall -W -fexceptions)

FIND_PACKAGE( KdepimLibs REQUIRED )
FIND_PACKAGE( Akonadi REQUIRED )
FIND_PACKAGE( GLIB2 REQUIRED )
FIND_PACKAGE( KDE4 REQUIRED )
FIND_PACKAGE( OpenSync REQUIRED )
FIND_PACKAGE( Automoc4 REQUIRED )

INCLUDE( CheckIncludeFile )
INCLUDE( Documentation )
INCLUDE( OpenSyncDefaults )
INCLUDE( OpenSyncPlugin )
INCLUDE( OpenSyncInternal )
INCLUDE( OpenSyncPackaging )



include_directories( ${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${KDEPIMLIBS_INCLUDE_DIR} ${OPENSYNC_INCLUDE_DIR} ${OPENSYNC_INCLUDE_DIR}/helper ${OPENSYNC_INCLUDE_DIR}/plugin )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src )
link_directories( ${KDE4_LIB_DIR} ${KDEPIMLIBS_LIB_DIR} ${OPENSYNC_LIBRARIES_DIR} )

ADD_SUBDIRECTORY( src )

OPENSYNC_PACKAGE( ${PROJECT_NAME} ${VERSION} )
