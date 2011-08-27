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
        osync_trace(TRACE_ENTRY, "%s(%p, %p, %p)", __func__, plugin, info, error);

        if ( !app )
            app = new QCoreApplication( fakeArgc, fakeArgv );
        if ( !kcd )
            kcd = new KComponentData( "akonadi-sync" );

        kDebug();
        // main sink
        AkonadiSink *mainSink = new AkonadiSink();
        if ( !mainSink->initialize(plugin, info, error ) ) {
            delete mainSink;
            osync_trace(TRACE_EXIT_ERROR,  "%s: NULL", __func__);
            return 0;
        }
//         QList<DataSink*> sinkList;

        // object type sinks
//   http://www.opensync.org/wiki/devel/pluginPortingGuide-0.40
//   10) List looping is more standard
// ---------------------------------
// osync_plugin_get_config_type();

//         unsigned int objects_supported = 4;
	QList<DataSink*> sinkList;

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

            // there might be something more intelligent to check how to return below
            if ( !ds->initialize(plugin, info, sink, error ) ) {
//                 osync_objtype_sink_set_enabled(sink, false);
                osync_objtype_sink_set_available(sink, false);
                delete ds;
//                 objects_supported--;
		continue;
            }
//             osync_objtype_sink_set_enabled(sink, true);
// 	    mainSink->addSink(ds);
	    sinkList.append(ds);
	    osync_objtype_sink_set_available(sink, true);
        }

//        if we support at least one object return the mainSink
        if ( ! sinkList.isEmpty() ) {
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
    QString toXml(QString str) {
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

    static osync_bool testSupport(OSyncObjTypeSink *sink, OSyncPluginConfig *config, QString mimeType, OSyncError **error ) {

        kDebug();

        Akonadi::CollectionFetchScope scope;
        scope.setIncludeUnsubscribed( true );

        bool configured = false;
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

                if ( !strcmp(myObjType, osync_objtype_sink_get_name(sink)) ) {
                    if ( !strcmp(myUrl , "default") ) {
                        osync_plugin_resource_set_name( myRes, toXml(col.name()).toLatin1().data() );
                        osync_plugin_resource_set_url(myRes, col.url().url().toLatin1().data() );
                        osync_plugin_resource_set_mime(myRes, mimeType.toLatin1().data() );
                        configured = true;
                    } else if ( !strcmp(myUrl, col.url().url().toLatin1().data()) && !strcmp(myMimeType, mimeType.toLatin1().data()) ) {
                        kDebug() << "aleady configured" << myObjType;
                        configured = true;
                    }
                    if (! enabled )
                        enabled = osync_plugin_resource_is_enabled(myRes);
                }
            }

            if ( ! configured  ) {
                OSyncPluginResource *newRes = create_resource(osync_objtype_sink_get_name(sink), error ) ;
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
            const char *myType = osync_objtype_sink_get_name(sink);
            // check if sync supported
            if ( ! strcmp(myType,"contact") ) {
                /* NOTE
                *  for groups I think x-vnd.kde.contactgroup is useful ...
                *  for address book though "text/directory"
                *  so we check here if there are contact groups
                */
                if ( ! testSupport(sink, config, "application/x-vnd.kde.contactgroup", error) )
                    osync_objtype_sink_set_available(sink, false);
                else
                    osync_objtype_sink_set_available(sink, true);
            }
            else if ( ! strcmp(myType,"event") ) {
                if ( ! testSupport(sink, config, "application/x-vnd.akonadi.calendar.event", error) )
                    osync_objtype_sink_set_available(sink, false);
                else
                    osync_objtype_sink_set_available(sink, true);
            }
            else if ( ! strcmp(myType,"note") ) {
                if ( ! testSupport(sink, config, "application/x-vnd.akonadi.calendar.journal", error)
		  && ! testSupport(sink, config, "application/x-vnd.kde.notes", error) )
                    osync_objtype_sink_set_available(sink, false);
                else
                    osync_objtype_sink_set_available(sink, true);
            }
            else if ( ! strcmp(myType,"todo") ) {
                if ( ! testSupport(sink, config, "application/x-vnd.akonadi.calendar.todo", error) )
                    osync_objtype_sink_set_available(sink, false);
                else
                    osync_objtype_sink_set_available(sink, true);
            }

            osync_plugin_info_add_objtype( info, sink );
        }
        // set information about the peer (KDE itself)
        {
            OSyncVersion *version = osync_version_new(error);
            osync_version_set_plugin(version, "akonadi-sync");
	    osync_version_set_modelversion(version, "1");
//             osync_version_set_softwareversion(version, "0.40");
            osync_version_set_vendor(version, "Deloptes");
//             osync_version_set_identifier(version, "akonadi-sync");
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
        AkonadiSink *mainSink = reinterpret_cast<AkonadiSink*>( userdata );
        mainSink->disconnect();
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
        osync_plugin_set_longname(plugin, "Akonadi OpenSync Plugin");
        osync_plugin_set_description(plugin, "Plugin to synchronize with Akonadi");

        osync_plugin_set_config_type(plugin, OSYNC_PLUGIN_OPTIONAL_CONFIGURATION);
        osync_plugin_set_initialize_func(plugin, akonadi_initialize);
        osync_plugin_set_finalize_func(plugin, akonadi_finalize);
        osync_plugin_set_finalize_timeout(plugin, 5);
        osync_plugin_set_discover_func(plugin, akonadi_discover);
        osync_plugin_set_discover_timeout(plugin, 5);
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

