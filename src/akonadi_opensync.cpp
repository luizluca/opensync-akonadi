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
#include <akonadi/collectionfetchscope.h>
// #include <akonadi/collectionfilterproxymodel.h>
#include <akonadi/mimetypechecker.h>

#include <kabc/addressee.h>

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
// osync_plugin_get_config_type();
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
//             else if ( sinkName == "todo" )
//                 ds = new DataSink( DataSink::Todos );
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

        OSyncPluginConfig *config = osync_plugin_info_get_config(info);

        if (!config) {
            osync_error_set(error, OSYNC_ERROR_GENERIC, "Unable to get config.");
            return false;
        }
        if ( !Akonadi::Control::start() )
            return false;

        // fetch all akonadi calendar collections
        Akonadi::CollectionFetchScope scope;
        scope.setIncludeUnsubscribed( true );
        scope.setContentMimeTypes( QStringList() << "text/calendar" );

        Akonadi::CollectionFetchJob *jobCal = new Akonadi::CollectionFetchJob(
            Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive );
        jobCal->setFetchScope(scope);
        if ( !jobCal->exec() )
            return false;

        Akonadi::Collection::List colsCal = jobCal->collections();
        kDebug() << "found" << colsCal.count() << "collections";

// 	OSyncFormatEnv *formatEnv = osync_plugin_info_get_format_env(info);
        OSyncObjTypeSink *sinkEvent = osync_objtype_sink_new("event", error);
        foreach ( const Akonadi::Collection &col, colsCal ) {
            kDebug() << "processing resource " << col.name() << col.contentMimeTypes();
            kDebug() << "                    " << col.name() << col.url().url();
            OSyncPluginResource *res = osync_plugin_config_find_active_resource(config ,"event");
            if ( res )
                osync_plugin_config_remove_resource(config,res);
            res = osync_plugin_resource_new( error );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vevent20", error ) );


            osync_plugin_resource_enable(res,TRUE);
            osync_plugin_resource_set_objtype( res, "event" );
            osync_plugin_resource_set_name( res, col.name().toUtf8() ); // TODO: full path instead of the name
            osync_plugin_resource_set_url( res, col.url().url().toLatin1() );

            osync_plugin_config_add_resource( config, res );
// 	    osync_plugin_resource_set_mime( config, "text/calendar" );
        }
	osync_objtype_sink_add_objformat_sink(sinkEvent,osync_objformat_sink_new ("vevent20", error));
        osync_objtype_sink_set_enabled( sinkEvent, TRUE );
        osync_objtype_sink_set_available( sinkEvent, TRUE );
//         osync_objtype_sink_add_objformat_sink( sinkEvent, "vevent20" );
        osync_plugin_info_add_objtype( info, sinkEvent );
            

        // fetch all address books
        Akonadi::CollectionFetchJob *jobAddr = new Akonadi::CollectionFetchJob(
            Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive );
        scope.setContentMimeTypes( QStringList() << KABC::Addressee::mimeType() );
        jobAddr->setFetchScope(scope);
        if ( !jobAddr->exec() )
            return false;

        Akonadi::Collection::List colsAddr = jobAddr->collections();
        kDebug() << "found" << colsAddr.count() << "collections";
        OSyncObjTypeSink *sinkAddr = osync_objtype_sink_new("contact", error);
        foreach ( const Akonadi::Collection &col, colsAddr ) {
            kDebug() << "processing resource " << col.name() << col.contentMimeTypes();
            kDebug() << "                    " << col.name() << col.url().url();
            OSyncPluginResource *res = osync_plugin_config_find_active_resource(config ,"contact");
            if ( res )
                osync_plugin_config_remove_resource(config,res);
            res = osync_plugin_resource_new( error );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vcard30", error ) );
            osync_plugin_resource_enable(res,TRUE);
            osync_plugin_resource_set_objtype( res, "contact" );
            osync_plugin_resource_set_name( res, col.name().toUtf8() ); // TODO: full path instead of the name
            osync_plugin_resource_set_url( res, col.url().url().toLatin1() );

            osync_plugin_config_add_resource( config, res );
// 	    osync_plugin_resource_set_mime( config, "text/calendar" );
        }
	osync_objtype_sink_add_objformat_sink(sinkAddr,osync_objformat_sink_new ("vcard30", error));
        osync_objtype_sink_set_enabled( sinkAddr, TRUE );
        osync_objtype_sink_set_available( sinkAddr, TRUE );
//         osync_objtype_sink_add_objformat_sink( sinkAddr, "vcard30" );
        osync_plugin_info_add_objtype( info, sinkAddr );

        // fetch all notes
        Akonadi::CollectionFetchJob *jobNotes = new Akonadi::CollectionFetchJob(
            Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive );
        scope.setContentMimeTypes( QStringList() << "application/x-vnd.kde.notes" );
        jobNotes->setFetchScope(scope);
        if ( !jobNotes->exec() )
            return false;

        Akonadi::Collection::List colsNotes = jobNotes->collections();
        kDebug() << "found" << colsNotes.count() << "collections";
        OSyncObjTypeSink *sinkNotes = osync_objtype_sink_new("note", error);

        foreach ( const Akonadi::Collection &col, colsNotes ) {
            kDebug() << "processing resource " << col.name() << col.contentMimeTypes();
            kDebug() << "                    " << col.name() << col.url().url();
            OSyncPluginResource *res = osync_plugin_config_find_active_resource(config ,"note");
            if ( res )
                osync_plugin_config_remove_resource(config,res);
            res = osync_plugin_resource_new( error );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vnote11", error ) );

            osync_plugin_resource_enable(res,TRUE);
            osync_plugin_resource_set_objtype( res, "note" );
            osync_plugin_resource_set_name( res, col.name().toUtf8() ); // TODO: full path instead of the name
            osync_plugin_resource_set_url( res, col.url().url().toLatin1() );

            osync_plugin_config_add_resource( config, res );
// 	    osync_plugin_resource_set_mime( config, "text/calendar" );
        }

	osync_objtype_sink_add_objformat_sink(sinkNotes,osync_objformat_sink_new ("vnote11", error));
        osync_objtype_sink_set_enabled( sinkNotes, TRUE );
        osync_objtype_sink_set_available( sinkNotes, TRUE );
//         osync_objtype_sink_add_objformat_sink( sinkNotes, "vjournal" );
        osync_plugin_info_add_objtype( info, sinkNotes );
        // set information about the peer (KDE itself)
        {
            OSyncVersion *version = osync_version_new(error);
            osync_version_set_plugin(version, "Akonadi-sync");
            osync_version_set_softwareversion(version, "4.5");
            osync_version_set_identifier(version, "akonadi-sync");
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
        osync_plugin_set_config_type(plugin, OSYNC_PLUGIN_NEEDS_CONFIGURATION);
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

