/*
    Copyright (c) 2010 Emanoil Kotsev <deloptes@yahoo.com>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>

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
#include "datasink.h"

#include <akonadi/control.h>
#include <akonadi/collection.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/mimetypechecker.h>

#include <kabc/addressee.h>
// #include <akonadi/collectionfilterproxymodel.h>

#include <KComponentData>
#include <KDebug>
#include <KUrl>

#include <QCoreApplication>
#include <QStringList>

#include <opensync/opensync.h>
#include <opensync/opensync-plugin.h>
#include <opensync/opensync-format.h>
#include <opensync/opensync-version.h>

static KComponentData *kcd = 0;
static QCoreApplication *app = 0;

static int fakeArgc = 0;
static char** fakeArgv = 0;

extern "C"
{

    static void* akonadi_initialize(OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncError **error)
    {
        osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, plugin, info, error);

        if ( !app )
            app = new QCoreApplication( fakeArgc, fakeArgv );
        if ( !kcd )
            kcd = new KComponentData( "akonadi_opensync" );

        // main sink
        AkonadiSink *mainSink = new AkonadiSink();
        if ( !mainSink->initialize( plugin, info, error ) ) {
            delete mainSink;
            osync_trace(TRACE_EXIT_ERROR,  "%s: NULL", __func__);
            return 0;
        }

        // object type sinks
//   http://www.opensync.org/wiki/devel/pluginPortingGuide-0.40
//   10) List looping is more standard
// ---------------------------------

        OSyncList *s = NULL, *list = osync_plugin_info_get_objtype_sinks(info);
        for ( s = list; s; s = s->next ) {
            OSyncObjTypeSink *sink = (OSyncObjTypeSink*) s->data;
            QString sinkName( osync_objtype_sink_get_name( sink ) );
            kDebug() << "###" << sinkName;
            osync_trace(TRACE_INTERNAL, "  %s", osync_objtype_sink_get_name( sink ));

            DataSink *ds = NULL;
            if ( sinkName == "event" )
                ds = new DataSink( DataSink::Calendars );
            else if ( sinkName == "contact" )
                ds = new DataSink( DataSink::Contacts );
//       FIXME: implement todos an journal (notes)
            else if ( sinkName == "todo" )
                ds = new DataSink( DataSink::Todos );
            else if ( sinkName == "note" )
                ds = new DataSink( DataSink::Notes );
            else
                continue;

            if ( !ds->initialize( plugin, info, sink, error ) ) {
                delete ds;
                delete mainSink;
                osync_trace(TRACE_EXIT_ERROR, " %s: NULL", __func__);
                return 0;
            }
        }
        osync_trace(TRACE_EXIT, " %s: %p", __func__, mainSink);
        return mainSink;

    }

    static osync_bool akonadi_discover(OSyncPluginInfo *info, void *userdata, OSyncError **error )
    {
        osync_trace(TRACE_ENTRY, " %s(%p, %p, %p)", __func__, userdata, info, error);
        kDebug();

        if ( !Akonadi::Control::start() )
            return false;

        // fetch all akonadi collections
        Akonadi::CollectionFetchJob *job = new Akonadi::CollectionFetchJob(
            Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive );
        if ( !job->exec() )
            return false;

        Akonadi::Collection::List cols = job->collections();
        kDebug() << "found" << cols.count() << "collections";

        foreach ( const Akonadi::Collection &col, cols ) {
            kDebug() << "available resource for" << col.name() << col.contentMimeTypes();
        }

//         Akonadi::MimeTypeChecker todoMimeTypeChecker;
//         todoMimeTypeChecker.addWantedMimeType( QLatin1String( "text/??todo" ) );
//         Akonadi::MimeTypeChecker journalMimeTypeChecker;
//         journalMimeTypeChecker.addWantedMimeType( QLatin1String( "text/??notes" ) );
        //   http://www.opensync.org/wiki/devel/pluginPortingGuide-0.40
// FIXME removing this will make configuration obsolate and we can use the iteration
//	over the akonadi items to build our configuration this however is helping
        OSyncPluginConfig *config = osync_plugin_info_get_config( info );
        Akonadi::MimeTypeChecker contactMimeChecker;
        contactMimeChecker.addWantedMimeType( KABC::Addressee::mimeType() );
        Akonadi::MimeTypeChecker calendarMimeTypeChecker;
        calendarMimeTypeChecker.addWantedMimeType( QLatin1String( "text/calendar" ) );
        foreach ( const Akonadi::Collection &col, cols ) {
            if ( col.contentMimeTypes().isEmpty() )
                continue;
            OSyncPluginResource *res0 = 0;
            OSyncObjTypeSink *sink = osync_objtype_sink_new(col.name().toLatin1(), error);
            if (! sink ) //TODO errors?
                continue;
// //                 QString formatName;
            if ( calendarMimeTypeChecker.isWantedCollection( col ) ) {
                kDebug() << "using resource for calendar" << col.name() << col.contentMimeTypes();
//                 formatName = "vevent20"; //TODO get the Akonadi object format here?
                res0 = osync_plugin_config_find_active_resource(config,"event");
                osync_plugin_resource_set_objtype( res0,"event" );
            }
            else if ( contactMimeChecker.isWantedCollection( col ) ) {
                kDebug() << "using resource contact for" << col.name() << col.contentMimeTypes();
                res0 = osync_plugin_config_find_active_resource(config,"contact");
                osync_plugin_resource_set_objtype( res0, "contact" );
                osync_plugin_resource_set_url( res0, col.url().url().toLatin1() );
            } else {
                kDebug() << "creating resource for" << col.name() << col.contentMimeTypes();
                res0 = osync_plugin_resource_new( error );
                osync_plugin_resource_set_objtype( res0, osync_objtype_sink_get_name( sink ) );
                osync_plugin_resource_add_objformat_sink( res0, osync_objformat_sink_new( "", error ) );
                osync_plugin_config_add_resource( config, res0 );
            }
            osync_plugin_resource_set_name( res0, col.name().toUtf8() ); // TODO: full path instead of the name
            osync_plugin_resource_set_url( res0, col.url().url().toLatin1() );
            osync_plugin_resource_enable( res0, TRUE );
            osync_objtype_sink_set_enabled( sink, TRUE );
            osync_objtype_sink_set_available( sink, TRUE );
            osync_plugin_info_add_objtype( info, sink );
        }

        // set information about the peer (KDE itself)
        {
            OSyncVersion *version = osync_version_new(error);
            osync_version_set_plugin(version, "Akonadi-sync");
            osync_version_set_softwareversion(version, "4.5");
            osync_plugin_info_set_version(info, version);
            osync_version_unref(version);
        }
        osync_trace(TRACE_EXIT, "%s", __func__);
        return TRUE;
    }

    static void akonadi_finalize(void *userdata)
    {
        osync_trace(TRACE_ENTRY, "%s(%p)", __func__, userdata);
        kDebug();
        AkonadiSink *sink = reinterpret_cast<AkonadiSink*>( userdata );
        delete sink;
        delete kcd;
        kcd = 0;
        delete app;
        app = 0;
        osync_trace(TRACE_EXIT, "%s", __func__);
    }

    KDE_EXPORT osync_bool get_sync_info(OSyncPluginEnv *env, OSyncError **error)
    {
        osync_trace(TRACE_ENTRY, "%s(%p)", __func__, env);

        OSyncPlugin *plugin = osync_plugin_new( error );
        if ( !plugin ) {
            osync_trace(TRACE_EXIT_ERROR, "%s: Unable to instantiate: %s", __func__, osync_error_print(error));
            osync_error_unref(error);
            return FALSE;
        }

        osync_plugin_set_name(plugin, "akonadi-sync");
        osync_plugin_set_longname(plugin, "Akonadi");
        osync_plugin_set_description(plugin, "Plugin to synchronize with Akonadi");
        osync_plugin_set_config_type(plugin, OSYNC_PLUGIN_OPTIONAL_CONFIGURATION);
        osync_plugin_set_initialize(plugin, akonadi_initialize);
        osync_plugin_set_finalize(plugin, akonadi_finalize);
        osync_plugin_set_discover(plugin, akonadi_discover);
        osync_plugin_set_start_type(plugin, OSYNC_START_TYPE_PROCESS);

        if ( ! osync_plugin_env_register_plugin(env, plugin, error) ) {
            osync_trace(TRACE_EXIT_ERROR, "%s: Unable to register: %s", __func__, osync_error_print(error));
            osync_error_unref(error);
            return FALSE;
        }

        osync_plugin_unref(plugin);
        osync_trace(TRACE_EXIT, "%s", __func__);
        return TRUE;

    }

    KDE_EXPORT int get_version(void)
    {
        return 1;
    }

}// extern "C"
