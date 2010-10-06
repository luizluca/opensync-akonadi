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
// notes includes
// todos includes
// journals includes

#include <KDebug>
#include <KLocale>
#include <KUrl>

using namespace Akonadi;

typedef boost::shared_ptr<KCal::Incidence> IncidencePtr;

DataSink::DataSink ( int type ) :
        SinkBase ( GetChanges | Commit | SyncDone ),
        m_Format(0)
{
    kDebug() << "Constr.objtype:" << type;
    m_type = type;

    m_isEvent = ( type == DataSink::Calendars ) ? true : false;
    m_isContact = ( type == DataSink::Contacts ) ? true : false;
    m_isNote = ( type == DataSink::Notes ) ? true : false;
    m_isTodo = ( type == DataSink::Todos ) ? true : false;
    m_isJournal = ( type == DataSink::Journals ) ? true : false;

}

DataSink::~DataSink()
{
    kDebug() << "DataSink destructor called"; // TODO still needed
}

bool DataSink::initialize ( OSyncPlugin * plugin, OSyncPluginInfo * info, OSyncObjTypeSink *sink, OSyncError ** error )
{
    kDebug() << "initializing" << osync_objtype_sink_get_name ( sink );
    Q_UNUSED ( plugin );
    Q_UNUSED ( info );
    Q_UNUSED ( error );

    OSyncPluginConfig *config = osync_plugin_info_get_config ( info );
    if ( !config )
    {
        osync_error_set ( error, OSYNC_ERROR_GENERIC, "Unable to get config." );
        return false;
    }

    osync_bool enabled = osync_objtype_sink_is_enabled ( sink );
    if ( ! enabled )
    {
        kDebug() << "sink is not enabled..";
// 	osync_objtype_sink_remove_objformat_sink( sink );
        osync_objtype_sink_set_available(sink, FALSE);
//         return false;
    } else {
        osync_objtype_sink_set_available(sink, TRUE);
    }

    OSyncPluginResource *resource = osync_plugin_config_find_active_resource ( config, osync_objtype_sink_get_name ( sink ) );
    OSyncList *objfrmtList = osync_plugin_resource_get_objformat_sinks ( resource );

    OSyncList *r;
//   bool hasObjFormat;
    for ( r = objfrmtList;r;r = r->next )
    {
        OSyncObjFormatSink *objformatsink = ( OSyncObjFormatSink * ) r->data;
        const char* tobjformat = osync_objformat_sink_get_objformat ( objformatsink );

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
//         case Notes:
//         {
//             if ( !strcmp ( "vnote11", tobjformat ) )
//                 m_Format = "vnote11";
//             break;
//         }
        case Journals:
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
        kDebug() << "Has objformat: " << m_Format;
    }


    osync_objtype_sink_set_userdata ( sink, this );
    osync_objtype_sink_enable_hashtable ( sink , TRUE );

    wrapSink ( sink );

    return true;
}

Akonadi::Collection DataSink::collection() const
{
    kDebug();
    OSyncPluginConfig *config = osync_plugin_info_get_config ( pluginInfo() );
    Q_ASSERT ( config );

    const char *objtype = osync_objtype_sink_get_name ( sink() );

    OSyncPluginResource *res = osync_plugin_config_find_active_resource ( config, objtype );

    if ( !res )
    {
        error ( OSYNC_ERROR_MISCONFIGURATION, i18n ( "No active resource for type \"%1\" found", objtype ) );
        return Collection();
    }

    const KUrl url = KUrl ( osync_plugin_resource_get_url ( res ) );
    // TODO osync_plugin_resource_get_mime() ;
    if ( url.isEmpty() )
    {
        error ( OSYNC_ERROR_MISCONFIGURATION, i18n ( "Url for object type \"%1\" is not configured.", objtype ) );
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

    if ( !hashtable )
    {
        kDebug() << "No hashtable";
        osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
        return;
    }

    Collection col = collection();
    if ( !col.isValid() )
    {
        kDebug() << "No collection";
        osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
        return;
    }
// FIXME
    if ( getSlowSink() )
    {
        kDebug() << "we're in the middle of slow-syncing...";
        osync_trace ( TRACE_INTERNAL, "resetting hashtable" );
        if ( ! osync_hashtable_slowsync ( hashtable, &oerror ) )
        {
            warning ( oerror );
            osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
            return;
        }
    }

    ItemFetchJob *job = new ItemFetchJob ( col );
    job->fetchScope().fetchFullPayload ( true );
    kDebug() << "Fetched FullPayload" ;

    QObject::connect ( job, SIGNAL ( itemsReceived ( const Akonadi::Item::List & ) ), this, SLOT ( slotItemsReceived ( const Akonadi::Item::List & ) ) );
    QObject::connect ( job, SIGNAL ( result ( KJob * ) ), this, SLOT ( slotGetChangesFinished ( KJob * ) ) );

    if ( !job->exec() )
    {
        error ( OSYNC_ERROR_IO_ERROR, job->errorText() );
        return;
    }

    kDebug() << "success()";
    success();
}

void DataSink::slotItemsReceived ( const Item::List &items )
{
    kDebug();
    kDebug() << "retrieved" << items.count() << "items";
    Q_FOREACH ( const Item& item, items ) {
        reportChange ( item );
    }
    kDebug() << "done";
}

void DataSink::reportChange ( const Item& item )
{
    kDebug();
    kDebug() << "Id:" << item.id() << "\n";
    kDebug() << "Mime:" << item.mimeType() << "\n";
    kDebug() << "Revision:" << item.revision() << "\n";
    kDebug() << "StorageCollectionId:" << item.storageCollectionId() << "\n";
    kDebug() << "Url:" << item.url() << "\n";
    kDebug() << "item.payloadData().data()" << "\n" << item.payloadData().data()  ;

    OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env ( pluginInfo() );
    OSyncObjFormat *format = osync_format_env_find_objformat ( formatenv, m_Format.toLatin1() );

    OSyncError *error = 0;
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

    OSyncChange *change = osync_change_new ( &error );
    if ( !change )
    {
        osync_change_unref ( change );
        warning ( error );
        return;
    }

    // Now you can set the data for the object
    // Set the last argument to FALSE if the real data
    // should be queried later in a "get_data" function
//     odata = osync_data_new(NULL, 0, format, &error);
    QString cvtToString = item.payloadData().data() ;
    OSyncData *odata = osync_data_new ( cvtToString.toLatin1().data() , cvtToString.size(), format, &error );
    if ( !odata )
    {
        osync_data_unref ( odata );
        return;
    }

    osync_change_set_uid ( change,  item.remoteId().toLatin1() );
    osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
    osync_data_set_objtype( odata, osync_objtype_sink_get_name( sink() ) );
    osync_change_set_data ( change, odata );

    osync_data_unref ( odata );

    OSyncChangeType changetype = osync_hashtable_get_changetype ( hashtable, change );
    osync_change_set_changetype ( change, changetype );
    osync_hashtable_update_change ( hashtable, change );

    if ( changetype != OSYNC_CHANGE_TYPE_UNMODIFIED )
        osync_context_report_change ( context(), change );

    /*
    kDebug()
    << "changeid:"    << osync_change_get_uid ( change )  << "; "
    << "itemid:"      << item.id() << "; "
    << "revision:"    << item.revision() << "; "
    << "changetype:"  << changetype << "; "
    << "hash:"        << osync_hashtable_get_hash ( hashtable, osync_change_get_uid ( change ) ) << "; "
    << "objtype:"     << osync_change_get_objtype ( change ) << "; "
    << "m_Format:"    << m_Format << "; "
    << "objform:"     << osync_objformat_get_name ( osync_change_get_objformat ( change ) ) << "; "
    << "sinkname:"    << osync_objtype_sink_get_name ( sink() );
    */
//     osync_error_unref(&error);
//     osync_objformat_unref(format);
//     osync_format_env_unref(formatenv);
}

void DataSink::slotGetChangesFinished ( KJob * )
{
    kDebug();
    OSyncError *error = 0;
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
    OSyncList *u, *uids = osync_hashtable_get_deleted ( hashtable );

        OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env( pluginInfo() );
        OSyncObjFormat *format = osync_format_env_find_objformat( formatenv, m_Format.toLatin1() );
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
            osync_change_unref( change );
            warning( error );
            continue;
        }

        osync_change_set_uid ( change, uid.toLatin1() );
        osync_change_set_changetype ( change, OSYNC_CHANGE_TYPE_DELETED );

        osync_data_set_objtype( data, osync_objtype_sink_get_name( sink() ) );
        osync_change_set_data( change, data );
        osync_hashtable_update_change ( hashtable, change );
	//FIXME raport if change is at our side
        osync_context_report_change ( context(), change );
        osync_change_unref ( change );
        osync_data_unref(data);
    }
    osync_list_free ( uids );

    kDebug() << "got all changes.";
}

void DataSink::commit ( OSyncChange *change )
{
//     kDebug();
//     kDebug() << "change uid:" << osync_change_get_uid ( change );
//     kDebug() << "objtype:" << osync_change_get_objtype ( change );
//     kDebug() << "objform:" << osync_objformat_get_name ( osync_change_get_objformat ( change ) );
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
    char *plain = 0;
    osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
    QString str = QString::fromLatin1 ( plain );
    QString id = QString::fromLatin1 ( osync_change_get_uid ( change ) );
    Collection col = collection();

    switch ( osync_change_get_changetype ( change ) )
    {
    case OSYNC_CHANGE_TYPE_ADDED:
    {
        // TODO: proper error handling (report errors to the sync engine)
        if ( !col.isValid() ) // error handling
            return;

        Item item;
        setPayload ( &item, str );
        item.setRemoteId( id.toLatin1() );

        ItemCreateJob *job = new Akonadi::ItemCreateJob ( item, col );
        if ( ! job->exec() )
            return;
        item = job->item(); // handle !job->exec in return too..
        if ( ! item.isValid() ) // error handling
            return;
        osync_change_set_uid ( change, item.remoteId().toLatin1() );
        osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
        break;
    }

    case OSYNC_CHANGE_TYPE_MODIFIED:
    {
        Item item = fetchItem (  id );
        setPayload ( &item, str );

        if ( ! item.isValid() ) // TODO proper error handling
            return;

        ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob ( item );
        if ( ! modifyJob->exec() )
            return;
        else
            item = modifyJob->item();

        osync_change_set_uid ( change, item.remoteId().toLatin1() );
        osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
        break;
    }

    case OSYNC_CHANGE_TYPE_DELETED:
    {
        Item item = fetchItem ( id );
        if ( ! item.isValid() ) // TODO proper error handling
            return;
        kDebug() << "delete with id: " << item.id();
        ItemDeleteJob *job = new ItemDeleteJob( item );

        if ( ! job->exec() ) {
            kDebug() << "unable to delete item";
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
        return;
    }

    osync_hashtable_update_change ( hashtable, change );
    success();
}

bool DataSink::setPayload ( Item *item, const QString &str )
{
    kDebug();
    switch ( m_type )
    {
    case Contacts:
    {
        kDebug() << "type = contacts";
        KABC::VCardConverter converter;
        KABC::Addressee vcard = converter.parseVCard ( str.toUtf8() );
        item->setMimeType ( "text/directory" );
        item->setPayload<KABC::Addressee> ( vcard );
        kDebug() << "payload: " << vcard.toString().toUtf8();
        break;
    }
    case Calendars:
    {
        kDebug() << "events";
        KCal::ICalFormat format;
        KCal::Incidence *calEntry = format.fromString ( str.toUtf8() );
        item->setMimeType ( "application/x-vnd.akonadi.calendar.event" );
        item->setPayload<IncidencePtr> ( IncidencePtr ( calEntry->clone() ) );

        break;
    }
    case Todos:
    {
        kDebug() << "todos";
        KCal::ICalFormat format;
        KCal::Incidence *todoEntry = format.fromString ( str.toUtf8() );
        item->setMimeType ( "application/x-vnd.akonadi.calendar.todo" );
        item->setPayload<IncidencePtr> ( IncidencePtr ( todoEntry->clone() ) );

        break;
    }
    case Notes:
    {
        kDebug() << "notes";
        KCal::ICalFormat format;
        KCal::Incidence *noteEntry = format.fromString ( str.toUtf8() );
        item->setMimeType ( "application/x-vnd.kde.notes" );
        item->setPayload<IncidencePtr> ( IncidencePtr ( noteEntry->clone() ) );

        break;
    }
    case Journals:
    {
        kDebug() << "journals";
        KCal::ICalFormat format;
        KCal::Incidence *journalEntry = format.fromString ( str.toUtf8() );
        item->setMimeType ( "application/x-vnd.akonadi.calendar.journal" );
        item->setPayload<IncidencePtr> ( IncidencePtr ( journalEntry->clone() ) );

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
    fetchJob->fetchScope().fullPayload();

    if ( fetchJob->exec() )
        foreach ( const Item &item, fetchJob->items() )
        if ( !strcmp(item.remoteId().toLatin1(), id.toLatin1() ))
            return item;

    // no such item found?
    return Item();
}

void DataSink::syncDone()
{
    kDebug() << "sync for sink member done";
    OSyncError *error = 0;
    osync_objtype_sink_save_hashtable ( sink() , &error );
    //TODO check for errors
    if ( error )
    {
        warning ( error );
        return;
    }
    success();
}

#include "datasink.moc"
