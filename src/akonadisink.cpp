/*
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010 Emanoil Kotsev <deloptes@yahoo.com>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "akonadisink.h"

#include <akonadi/control.h>

#include <KDebug>

#include <opensync/opensync-plugin.h>
#include <opensync/opensync-data.h>
#include <opensync/opensync-format.h>

AkonadiSink::AkonadiSink() :
    SinkBase( Connect )
{
}

AkonadiSink::~AkonadiSink()
{
}

bool AkonadiSink::initialize(OSyncPlugin * plugin, OSyncPluginInfo * info, OSyncError ** error)
{
  kDebug();
  OSyncObjTypeSink *sink = osync_objtype_main_sink_new( error );
          if (!sink) {
	    
	kDebug() << "No sink ";
	return false;
	  }
  osync_plugin_info_set_main_sink( info, sink );
  wrapSink( sink );
//   osync_objtype_sink_unref(sink);
//       OSyncHashTable * hashtable = osync_objtype_sink_get_hashtable(sink);
//       if( hashtable )
// 	osync_objtype_sink_enable_hashtable(sink, TRUE);
//       if( ! hashtable ) 
//         kDebug() << "No hashtable for sync";
  return true;
}

void AkonadiSink::connect()
{
  osync_trace(TRACE_ENTRY, "%s(%p, %p)", __PRETTY_FUNCTION__, pluginInfo(), context());
  kDebug();

  if ( !Akonadi::Control::start() ) {
    error( OSYNC_ERROR_NO_CONNECTION, "Could not start Akonadi." );
    osync_trace(TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, "Could not start Akonadi.");
    return;
  }
  success();
  osync_trace(TRACE_EXIT, "%s", __PRETTY_FUNCTION__);
}


#include "akonadisink.moc"
