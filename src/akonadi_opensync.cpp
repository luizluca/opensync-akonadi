/*
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010 Emanoil Kotsev <deloptes@yahoo.com>

    $Id$

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
        kDebug();
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

        unsigned int objects_supported = 4;

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
            else if ( sinkName == "note" )
                ds = new DataSink( DataSink::Notes );
            else if ( sinkName == "todo" )
                ds = new DataSink( DataSink::Todos );
            else
                continue;

            // there might be someting more intelligent to check when to return below
            if ( !ds->initialize( plugin, info, sink, error ) ) {
	      osync_objtype_sink_set_enabled(sink, false);
	      osync_objtype_sink_set_available(sink, false);
	      delete ds;
	      objects_supported--;
            }
        }


//        if we support at least one object return the mainSink
        if ( objects_supported >= 1 ) {
            osync_trace(TRACE_EXIT, " %s: %p", __func__, mainSink);
            return mainSink;
        }
        else {
            delete mainSink;
            osync_trace(TRACE_EXIT_ERROR, " %s: NULL", __func__);
            return 0;
        }

    }

    /* FIXME: this is probably a bug in opoensync
     *		replace &
     */
    static QString toXml(QString str) {
        str.replace("<","&lt;").replace(">","&gt;").replace("&","and");
        return str;
    }


    static OSyncPluginResource *create_resource (const char* mType , OSyncError **error ) {
        //TODO
        // check for supported objformat
        kDebug();
        OSyncPluginResource *res= osync_plugin_resource_new( error );
        osync_plugin_resource_set_objtype( res, mType );

        if ( !strcmp(mType,"contact") ) {
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vcard21", error ) );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vcard30", error ) );
            osync_plugin_resource_set_preferred_format( res, "vcard30" );
        } else if  ( !strcmp(mType,"event") ) {
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vevent10", error ) );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vevent20", error ) );
            osync_plugin_resource_set_preferred_format( res, "vevent20" );
        } else if  ( !strcmp(mType,"todo") ) {
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vtodo10", error ) );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vtodo20", error ) );
            osync_plugin_resource_set_preferred_format( res, "vtodo20" );
        } else if  ( !strcmp(mType,"note") ) {
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vnote11", error ) );
            osync_plugin_resource_add_objformat_sink( res, osync_objformat_sink_new( "vjournal", error ) );
            osync_plugin_resource_set_preferred_format( res, "vjournal" );
        } else
            return NULL;
        kDebug() << "create resource for" <<  mType << "done";
        return res;
    }


    /*
    Check for support of following types
    text/directory - this is the addressbook
    application/x-vnd.kde.contactgroup - is contact group ... I'm not sure about it ATM
    text/calendar - this is general mime for the whole calendar, but we are interested in the details
    application/x-vnd.akonadi.calendar.event,
    application/x-vnd.akonadi.calendar.todo,
    application/x-vnd.akonadi.calendar.journal,
    application/x-vnd.akonadi.calendar.freebusy - this will be most probably ignored, so not checking for it
    */

    static osync_bool testSupport(OSyncObjTypeSink *sink, OSyncPluginConfig *config, OSyncError **error ) {

        kDebug();
        QString mimeType;

        const char *myType = osync_objtype_sink_get_name(sink);
        Akonadi::CollectionFetchScope scope;
        scope.setIncludeUnsubscribed( true );
        if ( ! strcmp(myType,"contact") )
            mimeType = "application/x-vnd.kde.contactgroup" ; // text/directory
        else if ( ! strcmp(myType,"event") )
            mimeType = "application/x-vnd.akonadi.calendar.event";
        else if ( ! strcmp(myType,"note") )
            mimeType = "application/x-vnd.akonadi.calendar.journal";
        else if ( ! strcmp(myType,"todo") )
            mimeType = "application/x-vnd.akonadi.calendar.todo";
        else
            return false;

        scope.setContentMimeTypes( QStringList() << mimeType );

        // fetch all akonadi collections for this mimetype
        Akonadi::CollectionFetchJob *jobCol = new Akonadi::CollectionFetchJob(
            Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive );
        jobCol->setFetchScope(scope);
        if ( !jobCol->exec() )
            return false;

        Akonadi::Collection::List colsList = jobCol->collections();
        int col_count = colsList.count();
        kDebug() << "found" << col_count << "collections";
        bool enabled = false;
	bool configured = false;

        foreach ( const Akonadi::Collection &col, colsList ) {
            kDebug() << "processing resource " << col.name() << col.contentMimeTypes();
            kDebug() << "url                 " << col.name() << col.url().url();
	    configured = false;

            OSyncList *resList = osync_plugin_config_get_resources(config);
            for ( OSyncList *r = resList; r; r = r->next ) {
                OSyncPluginResource *myRes = (OSyncPluginResource*) r->data;

                const char *myObjType = osync_plugin_resource_get_objtype(myRes);
                const char *myMimeType = osync_plugin_resource_get_mime(myRes);
                const char *myUrl = osync_plugin_resource_get_url(myRes);

                if ( !strcmp(myObjType, myType) ) {
                    if ( !strcmp(myUrl , "default") ) {
                        osync_plugin_resource_set_name( myRes, toXml(col.name()).toLatin1() );
                        osync_plugin_resource_set_url(myRes, col.url().url().toLatin1());
                        osync_plugin_resource_set_mime(myRes, mimeType.toLatin1() );
			configured = true;
                    } else if ( !strcmp(myUrl, col.url().url().toLatin1()) && !strcmp(myMimeType, mimeType.toLatin1()) ) {
                        kDebug() << "aleady configured" << myObjType;
			configured = true;
                    }
                    if (! enabled )
		      enabled = osync_plugin_resource_is_enabled(myRes);
                }
            }
            
            if ( ! configured  ) {
                        OSyncPluginResource *newRes = create_resource(myType, error ) ;
                        osync_plugin_resource_set_name( newRes, toXml(col.name()).toLatin1() );
                        osync_plugin_resource_set_url(newRes, col.url().url().toLatin1());
                        osync_plugin_resource_set_mime(newRes, mimeType.toLatin1() );
                        if ( ! enabled  ) {
			  osync_plugin_resource_enable( newRes, true );
			  enabled = true;
			}
			else {
			  osync_plugin_resource_enable( newRes, false );
			}
			
                        osync_plugin_config_add_resource(config , newRes);
			configured = true;
	    }
			
        }


        return configured;
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
        if ( !Akonadi::Control::start() ) {
            osync_error_set(error, OSYNC_ERROR_GENERIC, "Akonadi not running.");
            return false;
        }

        OSyncList *sinks = osync_plugin_info_get_objtype_sinks(info);
        for ( OSyncList *s = sinks; s; s = s->next ) {
            OSyncObjTypeSink *sink = (OSyncObjTypeSink*) s->data;
            // check if sync supported
            if ( ! testSupport(sink, config, error) ) {
                osync_objtype_sink_set_available(sink, false);
            }
            else {
                osync_objtype_sink_set_available(sink, true);
            }
            osync_plugin_info_add_objtype( info, sink );
        }
        // set information about the peer (KDE itself)
        {
            OSyncVersion *version = osync_version_new(error);
            osync_version_set_plugin(version, "Akonadi-sync");
            osync_version_set_softwareversion(version, "0.40");
            osync_version_set_identifier(version, "akonadi-sync");
            osync_plugin_info_set_version(info, version);
            osync_version_unref(version);
        }
        osync_list_free(sinks);
        osync_trace(TRACE_EXIT, "%s", __func__);
        return true;
    }

    static void akonadi_finalize(void *userdata)
    {
        osync_trace(TRACE_ENTRY, "%s(%p)", __func__, userdata);
        kDebug();
        AkonadiSink *sink = reinterpret_cast<AkonadiSink*>( userdata );
        sink->disconnect();
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
            return false;
        }

        osync_plugin_set_name(plugin, "akonadi-sync");
        osync_plugin_set_longname(plugin, "Akonadi");
        osync_plugin_set_description(plugin, "Plugin to synchronize with Akonadi");
        osync_plugin_set_config_type(plugin, OSYNC_PLUGIN_OPTIONAL_CONFIGURATION);
        osync_plugin_set_initialize(plugin, akonadi_initialize);
        osync_plugin_set_finalize_timeout(plugin, 5);
        osync_plugin_set_finalize(plugin, akonadi_finalize);
        osync_plugin_set_discover(plugin, akonadi_discover);
        osync_plugin_set_start_type(plugin, OSYNC_START_TYPE_PROCESS);

        if ( ! osync_plugin_env_register_plugin(env, plugin, error) ) {
            osync_trace(TRACE_EXIT_ERROR, "%s: Unable to register: %s", __func__, osync_error_print(error));
            osync_error_unref(error);
            return false;
        }

        osync_plugin_unref(plugin);
        osync_trace(TRACE_EXIT, "%s", __func__);
        return true;

    }

    KDE_EXPORT int get_version(void)
    {
        return 1;
    }

}// extern "C"

