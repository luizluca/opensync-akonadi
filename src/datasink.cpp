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

#include "datasink.h"

#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/itemdeletejob.h>

// calendar includes
#include <kcal/incidence.h>
#include <kcal/icalformat.h>

// contact includes
#include <kabc/addressee.h>
#include <kabc/vcardconverter.h>
#include <kabc/vcardparser.h>
#include <kabc/vcardformat.h>

// TODO
// notes, todos, journals & includes are done by icalformat
// I am not quite sure about it, but I think
// this needs to be checked when working on todos and notes

#include <KDebug>
#include <KLocale>

using namespace Akonadi;

typedef boost::shared_ptr<KCal::Incidence> IncidencePtr;

DataSink::DataSink ( int type ) :
        SinkBase ( GetChanges | Commit | SyncDone ),
        m_Format("default"),
        m_Enabled(false),
        m_Url("default")
{
    kDebug() << "Constr.objtype:" << type;
    m_type = type;

    m_isEvent = ( type == DataSink::Calendars ) ? true : false;
    m_isContact = ( type == DataSink::Contacts ) ? true : false;
    m_isNote = ( type == DataSink::Notes ) ? true : false;
    m_isTodo = ( type == DataSink::Todos ) ? true : false;

}

DataSink::~DataSink()
{
    kDebug() << "DataSink destructor called"; // TODO still needed
}

bool DataSink::initialize ( OSyncPlugin * plugin, OSyncPluginInfo * info, OSyncObjTypeSink *sink, OSyncError ** error )
{
    kDebug() << "initializing" << osync_objtype_sink_get_name ( sink );
    Q_UNUSED ( plugin );
//     Q_UNUSED ( info );
    Q_UNUSED ( error );


    OSyncPluginConfig *config = osync_plugin_info_get_config ( info );
    if ( !config )
    {
        osync_error_set ( error, OSYNC_ERROR_GENERIC, "Unable to get config." );
        return false;
    }

// FIXME enable checks on sync/commit etc!!
    OSyncPluginResource *resource = osync_plugin_config_find_active_resource ( config, osync_objtype_sink_get_name ( sink ) );
    if ( ! resource || ! osync_plugin_resource_is_enabled(resource) ) {
        m_Enabled = FALSE;
        return false;
    } else
        m_Enabled = TRUE;

    m_Url = osync_plugin_resource_get_url ( resource );

    OSyncList *objfrmtList = osync_plugin_resource_get_objformat_sinks ( resource );
    const char *preferred = osync_plugin_resource_get_preferred_format(resource);
    for ( OSyncList *r = objfrmtList;r;r = r->next )
    {
        OSyncObjFormatSink *objformatsink = ( OSyncObjFormatSink * ) r->data;
        const char* tobjformat = osync_objformat_sink_get_objformat ( objformatsink );

	// TODO how can I negotiate format ... is this here enough?
        switch ( m_type )
        {
        case Contacts:
        {
            if ( !strcmp ( "vcard21", tobjformat ) )
                m_Format = "vcard21";
            // always prefer newer format
            if ( !strcmp ( "vcard30", tobjformat ) )
                m_Format = "vcard30";
            break;
        }
        case Calendars:
        {
            if ( !strcmp ( "vevent10", tobjformat ) )
                m_Format = "vevent10";
            // always prefer newer format
            if ( !strcmp ( "vevent20", tobjformat ) )
                m_Format = "vevent20";
            break;
        }
        case Notes:
        {
            if ( !strcmp ( "vnote11", tobjformat ) )
                m_Format = "vnote11";
            if ( !strcmp ( "vjournal", tobjformat ) )
                m_Format = "vjournal";
            break;
        }
        case Todos:
        {
            if ( !strcmp ( "vtodo10", tobjformat ) )
                m_Format = "vtodo10";
            // always prefer newer format
            if ( !strcmp ( "vtodo20", tobjformat ) )
                m_Format = "vtodo20";
            break;
        }

        default:
            return false;
        }
    }

    if ( ! preferred || strcmp(preferred,m_Format.toLatin1().data() ) )
        osync_plugin_resource_set_preferred_format( resource, m_Format.toLatin1().data() );

    kDebug() << "Has objformat: " << m_Format;

    wrapSink ( sink );

    osync_objtype_sink_set_enabled ( sink, true );
    osync_objtype_sink_set_available ( sink, true );
    osync_objtype_sink_set_userdata ( sink, this );
    osync_objtype_sink_enable_hashtable ( sink , true );

    return true;
}

Akonadi::Collection DataSink::collection() const
{
    kDebug();

    const KUrl url = KUrl ( m_Url );

    if ( url.isEmpty() )
    {
        error ( OSYNC_ERROR_MISCONFIGURATION, i18n ( "Url for object type \"%s\" is not configured.",  m_type) );
        return Collection();
    }

    return Collection::fromUrl ( url );
}


void DataSink::getChanges()
{
    kDebug();
    kDebug() << " DataSink::getChanges() called";
    OSyncError *oerror = 0;

    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

//     if ( !hashtable ) {
//         kDebug() << "No hashtable";
//         osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
//         return;
//     }   
    
// FIXME: I don't understand this completely well
    if ( getSlowSink() ) 
    {
        kDebug() << "we're in the middle of slow-syncing...";
        osync_trace ( TRACE_INTERNAL, "resetting hashtable" );
        if ( ! osync_hashtable_slowsync ( hashtable, &oerror ) )
        {
            osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
//             error ( OSYNC_ERROR_GENERIC, osync_error_print ( &oerror) );
            osync_error_unref(&oerror);
        }
    }

        Akonadi::Collection col = collection() ;
        if ( !col.isValid() )
        {
            kDebug() << "No collection";
            osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
            return;
        }

        ItemFetchJob *job = new ItemFetchJob ( col );
        job->fetchScope().fetchFullPayload();
        kDebug() << "Fetched full payload";

        QObject::connect ( job, SIGNAL ( itemsReceived ( const Akonadi::Item::List & ) ), this, SLOT ( slotItemsReceived ( const Akonadi::Item::List & ) ) );
        QObject::connect ( job, SIGNAL ( result ( KJob * ) ), this, SLOT ( slotGetChangesFinished ( KJob * ) ) );

        if ( !job->exec() )
        {
            error ( OSYNC_ERROR_IO_ERROR, job->errorText() );
            return;
        }
    
}

void DataSink::slotItemsReceived ( const Item::List &items )
{
    kDebug();
    kDebug() << "retrieved" << items.count() << "items";
    Q_FOREACH ( const Item& item, items ) {
	reportChange ( item );
    }
    kDebug() << "slotItemsReceived done";
}

void DataSink::reportChange ( const Item& item )
{
    kDebug();
    kDebug() << "Id:" << item.id() << "\n";
    kDebug() << "RemoteId:" << item.remoteId() << "\n";
    kDebug() << "Mime:" << item.mimeType() << "\n";
    kDebug() << "Revision:" << item.revision() << "\n";
    kDebug() << "StorageCollectionId:" << item.storageCollectionId() << "\n";
    kDebug() << "Url:" << item.url() << "\n";
    kDebug() << "item.payloadData().data()" << "\n" << item.payloadData().data()  ;

    OSyncError *error = 0;
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

    OSyncChange *change = osync_change_new ( &error );
    if ( !change )
    {
        osync_change_unref ( change );
        warning ( error );
        return;
    }
//    TODO Do I need to filter here some items
//     osync_change_get_objformat( change );
//     if ( strcmp(m_MimeType.toLatin1(),item.mimeType().toLatin1()) ) {
//         osync_change_unref ( change );
// //         warning ( error );
//         return;
//     }

    osync_change_set_uid ( change,  item.remoteId().toLatin1().data() );
    osync_change_set_hash ( change, QString::number( item.revision() ).toLatin1() );
    OSyncChangeType changetype = osync_hashtable_get_changetype(hashtable, change);
    osync_change_set_changetype(change, changetype);

    osync_hashtable_update_change ( hashtable, change );

    if ( changetype == OSYNC_CHANGE_TYPE_UNMODIFIED ) {

        osync_change_unref(change);
        return;
    }
    // Now you can set the data for the object
    // Set the last argument to FALSE if the real data
    // should be queried later in a "get_data" function

    OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env ( pluginInfo() );
    OSyncObjFormat *format = osync_format_env_find_objformat ( formatenv, m_Format.toLatin1().data() );

    char *cvtToString  = item.payloadData().data();
    OSyncData *odata = osync_data_new ( cvtToString , strlen(cvtToString), format, &error );
    if ( !odata )
    {
        osync_data_unref ( odata );
        osync_change_unref(change);
        warning(error);
        return;
    }
    osync_data_set_objtype( odata, osync_objtype_sink_get_name( sink() ) );
    osync_change_set_data ( change, odata );

    osync_data_unref ( odata );

    osync_context_report_change ( context(), change );

}

void DataSink::slotGetChangesFinished ( KJob * )
{
    kDebug();
    OSyncError *error = 0;

    OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env( pluginInfo() );
    OSyncObjFormat *format = osync_format_env_find_objformat( formatenv, m_Format.toLatin1().data() );
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
    OSyncList *u, *uids = osync_hashtable_get_deleted ( hashtable );
    for ( u = uids; u; u = u->next )
    {
        QString uid ( ( char * ) u->data );
        kDebug() << "going to delete with uid:" << uid;

        OSyncChange *change = osync_change_new ( &error );
        if ( !change )
        {
            osync_change_unref ( change );
            warning ( error );
            continue;
        }
        error = 0;
        OSyncData *data = osync_data_new( NULL, 0, format, &error );
        if ( !data ) {
	    osync_data_unref ( data );
            osync_change_unref( change );
            warning( error );
            continue;
        }

        osync_change_set_uid ( change, uid.toLatin1().data() );
        osync_change_set_changetype ( change, OSYNC_CHANGE_TYPE_DELETED );

        osync_data_set_objtype( data, osync_objtype_sink_get_name( sink() ) );
        osync_change_set_data( change, data );

        osync_hashtable_update_change ( hashtable, change );

        osync_context_report_change ( context(), change );

        osync_data_unref(data);
        osync_change_unref ( change );
    }
    osync_list_free ( uids );

        success();
	kDebug() << "got all changes success().";
}

//     NOTE "application/x-vnd.kde.contactgroup" is used for contact groups ... we can use it probably later

QString DataSink::getMimeWithFormat ( OSyncObjFormat * format ) {
    const char* name = osync_objformat_get_name (format);
    if (!strcmp(name,"vcard21") || !strcmp(name,"vcard30") )
        return "text/directory";
    else if (!strcmp(name,"vevent10") || !strcmp(name,"vevent10") )
//         return "text/calendar";
        return "application/x-vnd.akonadi.calendar.event";
    else if (!strcmp(name,"vtodo10") || !strcmp(name,"vtodo20"))
//         return "text/calendar";
        return "application/x-vnd.akonadi.calendar.todo";
    else if (!strcmp(name,"vnote11") )
        return "application/x-vnd.kde.notes";
    else if (!strcmp(name,"vjournal"))
//         return "text/calendar";
        return "application/x-vnd.akonadi.calendar.journal";
    else
        return false;
}

void DataSink::commit ( OSyncChange *change )
{
    kDebug();

    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
    char *plain = 0;
    osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
    QString str = QString::fromLatin1 ( plain );
    QString id = QString::fromLatin1 ( osync_change_get_uid ( change ) );
    QString mimeType = getMimeWithFormat(osync_change_get_objformat(change));

    kDebug() << "change uid:" << id;
    kDebug() << "objform:" << osync_objformat_get_name ( osync_change_get_objformat ( change ) );
    kDebug();
    kDebug() << "data" << str;

    Akonadi::Collection col = collection();

    if ( !col.isValid() ) {
        error( OSYNC_ERROR_GENERIC, "Invalid collction.");
        return;
    }

    switch ( osync_change_get_changetype ( change ) )
    {
    case OSYNC_CHANGE_TYPE_ADDED:
    {
        Item item;
        setPayload ( &item, mimeType, str );
        item.setRemoteId( id );

        ItemCreateJob *job = new Akonadi::ItemCreateJob ( item, col );
        if ( ! job->exec() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to create job for item.");
            return;
        }

        item = job->item(); // handle !job->exec in return too..
        if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
        }
        osync_change_set_uid ( change, item.remoteId().toLatin1().data() );
        osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1().data() );
        break;
    }

    case OSYNC_CHANGE_TYPE_MODIFIED:
    {
        Item item = fetchItem ( id );

        if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
        }
        setPayload ( &item, mimeType, str );

        ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob ( item );
        if ( ! modifyJob->exec() ) {
            error ( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
        }

        item = modifyJob->item();
        if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
        }
        osync_change_set_uid ( change, item.remoteId().toLatin1().data() );
        osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1().data() );
        break;
    }

    case OSYNC_CHANGE_TYPE_DELETED:
    {
        Item item = fetchItem ( id );
        if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item");
            return;
        }

        ItemDeleteJob *job = new ItemDeleteJob( item );
        if ( ! job->exec() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to delete item");
            return;
        }
        break;
    }

    case OSYNC_CHANGE_TYPE_UNMODIFIED:
    {
        kDebug() << "UNMODIFIED";
        // should we do something here?
        break;
    }
    default:
        kDebug() << "got invalid changetype?";
        error(OSYNC_ERROR_GENERIC, "got invalid changetype");
        return;
    }

    osync_hashtable_update_change ( hashtable, change );
    
    success();
}

bool DataSink::setPayload ( Item *item, const QString mimeType, const QString &str )
{
    kDebug();
    item->setMimeType ( mimeType );
    switch ( m_type )
    {
    case Contacts:
    {
        kDebug() << "type = contacts";
        KABC::VCardConverter converter;
        KABC::Addressee vcard = converter.parseVCard ( str.toUtf8() );
        item->setPayload<KABC::Addressee> ( vcard );
        kDebug() << "payload: " << vcard.toString().toUtf8();
        break;
    }
    case Calendars:
    {
        kDebug() << "events";
        KCal::ICalFormat format;
        KCal::Incidence *calEntry = format.fromString ( str.toUtf8() );
        item->setPayload<IncidencePtr> ( IncidencePtr ( calEntry->clone() ) );
        kDebug() << "payload: " << str.toUtf8();
        break;
    }
    case Todos:
    {
        kDebug() << "todos";
        KCal::ICalFormat format;
        KCal::Incidence *todoEntry = format.fromString ( str.toUtf8() );
        item->setPayload<IncidencePtr> ( IncidencePtr ( todoEntry->clone() ) );
        kDebug() << "payload: " << str.toUtf8();
        break;
    }
    case Notes:
    {
        kDebug() << "notes";
        KCal::ICalFormat format;
        KCal::Incidence *noteEntry = format.fromString ( str.toUtf8() );
        item->setPayload<IncidencePtr> ( IncidencePtr ( noteEntry->clone() ) );
        kDebug() << "payload: " << str.toUtf8();
        break;
    }
    default:
        // should not happen
        return false;
    }

    return true;
}

const Item DataSink::fetchItem ( const QString& id )
{
    kDebug();

    ItemFetchJob *fetchJob = new ItemFetchJob ( collection() );
    fetchJob->fetchScope().fetchFullPayload();
// fetchJob->start();
    if ( fetchJob->exec() )
        foreach ( const Item &item, fetchJob->items() )
        if ( !strcmp(item.remoteId().toLatin1().data(), id.toLatin1().data() ))
            return item;
    // no such item found?
    // this will be handled as invalid item
    return Item();
}

void DataSink::syncDone()
{
    kDebug() << "sync for sink member done";
    OSyncError *error = 0;
    osync_objtype_sink_save_hashtable ( sink() , &error );
    if ( error ) {
        warning ( error );
        return;
    }
    success();
}

#include "datasink.moc"
