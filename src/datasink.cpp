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
#include <akonadi/itemmodifyjob.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/mimetypechecker.h>


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
        m_Url("default")
{
    m_type = type;
}

DataSink::~DataSink()
{
    kDebug() << "DataSink destructor called"; // TODO still needed
}

bool DataSink::initialize ( OSyncPlugin * plugin, OSyncPluginInfo * info, OSyncObjTypeSink *sink, OSyncError ** error )
{
  m_Name = osync_objtype_sink_get_name ( sink );
    kDebug() << "initializing" << m_Name;
    Q_UNUSED ( plugin );
//     Q_UNUSED ( info );
    Q_UNUSED ( error );

// require configuration
    OSyncPluginConfig *config = osync_plugin_info_get_config ( info );
    if ( !config )
    {
//         osync_error_set ( error, OSYNC_ERROR_GENERIC, "Unable to get config." );
        return false;
    }

// require enabled ressource
    OSyncPluginResource *resource = osync_plugin_config_find_active_resource ( config, osync_objtype_sink_get_name ( sink ) );
    if ( ! resource || ! osync_plugin_resource_is_enabled(resource) )
        return false;

// get url
    m_Url = osync_plugin_resource_get_url ( resource );

// set format    
    OSyncList *objfrmtList = osync_plugin_resource_get_objformat_sinks ( resource );
    const char *preferred = osync_plugin_resource_get_preferred_format(resource);
    for ( OSyncList *r = objfrmtList;r;r = r->next )
    {
        OSyncObjFormatSink *objformatsink = ( OSyncObjFormatSink * ) r->data;
        const char* tobjformat = osync_objformat_sink_get_objformat ( objformatsink );

	// NOTE "application/x-vnd.kde.contactgroup" is used for contact groups ... we can use it probably later

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
	    m_MimeType = "text/directory";
            break;
        }
        case Calendars:
        {
            if ( !strcmp ( "vevent10", tobjformat ) )
                m_Format = "vevent10";
            // always prefer newer format
            if ( !strcmp ( "vevent20", tobjformat ) )
                m_Format = "vevent20";
	    m_MimeType = "application/x-vnd.akonadi.calendar.event";
            break;
        }
        case Notes:
        {
	    if ( !strcmp ( "vnote11", tobjformat ) ) {
                m_Format = "vnote11";
                m_MimeType = "application/x-vnd.kde.notes";
            }
            if ( !strcmp ( "vjournal", tobjformat ) ) {
                m_Format = "vjournal";
                m_MimeType = "application/x-vnd.akonadi.calendar.journal";
            }
            break;
        }
        case Todos:
        {
            if ( !strcmp ( "vtodo10", tobjformat ) )
                m_Format = "vtodo10";
            // always prefer newer format
            if ( !strcmp ( "vtodo20", tobjformat ) )
                m_Format = "vtodo20";
	    m_MimeType = "application/x-vnd.akonadi.calendar.todo";
            break;
        }

        default:
            return false;
        }
    }

// this adds preffered to the resource configuration if not set
    if ( ! preferred || strcmp(preferred,m_Format.toLatin1().data() ) )
        osync_plugin_resource_set_preferred_format( resource, m_Format.toLatin1().data() );

    kDebug() << "Has objformat: " << m_Format;

    wrapSink ( sink );
//     osync_objtype_sink_set_userdata ( sink, this );

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

    if ( !hashtable ) {
        kDebug() << "No hashtable";
            error ( OSYNC_ERROR_FILE_NOT_FOUND, "No hashtable");
        osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
        return;
    }   
    
    if ( getSlowSink() ) 
    {
        kDebug() << "we're in the middle of slow-syncing...";
        osync_trace ( TRACE_INTERNAL, "resetting hashtable" );
        if ( ! osync_hashtable_slowsync ( hashtable, &oerror ) )
        {
            osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
            error ( OSYNC_ERROR_GENERIC, osync_error_print ( &oerror) );
            osync_error_unref(&oerror);
	    return;
        }
    }

        Akonadi::Collection col = collection() ;
	
// 	col.setContentMimeTypes( QStringList() << getMimeWithFormat(format) );
        if ( !col.isValid() )
        {
            kDebug() << "No collection";
            osync_trace ( TRACE_EXIT_ERROR, "%s: %s", __PRETTY_FUNCTION__, osync_error_print ( &oerror ) );
	    error ( OSYNC_ERROR_GENERIC, "No collection");
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
    Akonadi::MimeTypeChecker checker;
    checker.addWantedMimeType( m_MimeType );

    Q_FOREACH ( const Item& item, items ) {
      // report only items of given mimeType
        if (  checker.isWantedItem( item ) )
	  reportChange ( item );
	else
            kDebug() << item.id() <<  item.mimeType() << "skipped!";
    }
    kDebug() << "slotItemsReceived done";
}

void DataSink::reportChange ( const Item& item )
{
    kDebug() << ">>>>>>>>>>>>>>>>>>>";
    kDebug() << "Id:" << item.id() << "\n";
    kDebug() << "RemoteId:" << item.remoteId() << "\n";
    kDebug() << "Mime:" << item.mimeType() << "\n";
    kDebug() << "Revision:" << item.revision() << "\n";
    kDebug() << "StorageCollectionId:" << item.storageCollectionId() << "\n";
    kDebug() << "Url:" << item.url() << "\n";
    kDebug() << "mtime:" << item.modificationTime().toString(Qt::ISODate) << "\n";
//     kDebug() << "mtime:" << (uint) item.modificationTime().toTime_t () << "\n";

    if ( item.remoteId().isEmpty() )
    {
        error( OSYNC_ERROR_EXPECTED, "item remote identifier missing" );
        return;
    }
    OSyncChange *change = 0;
    QString hash = getHash( item.id(), item.revision() ) ;

    kDebug() << hash;
    kDebug() << "item.payloadData().data()" << "\n" << item.payloadData().data();

    OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env ( pluginInfo() );

    OSyncError *oerror = 0;
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

    change = osync_change_new ( &oerror );
    if ( !change )
    {
        osync_change_unref ( change );
        warning ( oerror );
        return;
    }

    osync_change_set_uid ( change,  item.remoteId().toLatin1().data() );
//     osync_change_set_uid ( change, QString::number( item.id() ).toLatin1() );
    osync_change_set_hash ( change, getHash( item.id(), item.revision() ).toLatin1().data() );

    OSyncChangeType changetype = osync_hashtable_get_changetype(hashtable, change);
    osync_change_set_changetype(change, changetype);

    osync_hashtable_update_change ( hashtable, change );

    if ( changetype == OSYNC_CHANGE_TYPE_UNMODIFIED ) {
        kDebug()<< "skipped (unmodified)" << change;
        osync_change_unref(change);
        osync_error_unref(&oerror);
        return;
    }
    // Now you can set the data for the object

    OSyncObjFormat *format = osync_format_env_find_objformat ( formatenv, m_Format.toLatin1().data() );
    int newDataSize = item.payloadData().size();
    char newData[newDataSize];
    memcpy(newData, item.payloadData().data(), newDataSize);
//     OSyncData *odata = osync_data_new ( item.payloadData().data() , item.payloadData().size(), format, &error );
    OSyncData *odata = osync_data_new ( newData, newDataSize, format, &oerror );
    if ( !odata )
    {
      osync_change_unref((OSyncChange*) change);
      warning(oerror);
      return;
    }
    osync_error_unref(&oerror);

    kDebug()<< "context" << context();
//     do I need this here
//     osync_data_set_objtype( odata, m_Name.toLatin1().data() );
    osync_change_set_data ( change, odata );
    kDebug()<< "data  " << odata;
//     not sure but it gets probably delete together with change below
//     osync_data_unref ( (OSyncData *) odata );
//     osync_hashtable_update_change ( hashtable, change ); //Do we need an update after setting data?

    osync_context_report_change ( context(), change );
//     osync_hashtable_update_change ( hashtable, change );
//     kDebug()<< "change" << change;
    osync_change_unref ( change );

    kDebug()<< "<<<<<<<<<<<<<< change done";

}

void DataSink::slotGetChangesFinished ( KJob * )
{
    kDebug();
    OSyncError *oerror = 0;

    OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env( pluginInfo() );
    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
    OSyncList *u, *uids = osync_hashtable_get_deleted ( hashtable );
    for ( u = uids; u; u = u->next )
    {
        QString uid ( ( char * ) u->data );
        kDebug() << "going to delete with uid:" << uid;

        OSyncChange *change = osync_change_new ( &oerror );
        if ( !change )
        {
            osync_change_unref ( change );
            warning ( oerror );
            continue;
        }           

        osync_change_set_uid ( change, uid.toLatin1().data() );
	QString hash = osync_change_get_hash(change);
	kDebug() << "hash:" << hash;
        osync_change_set_changetype ( change, OSYNC_CHANGE_TYPE_DELETED );
	
        OSyncObjFormat *format = osync_format_env_find_objformat( formatenv, m_Format.toLatin1().data() );
        oerror = 0;
        OSyncData *data = osync_data_new( NULL, 0, format, &oerror );
        if ( !data ) {
            osync_change_unref( change );
            warning( oerror );
            continue;
        }

        osync_data_set_objtype( data, m_Name.toLatin1().data() );
        osync_change_set_data( change, data );
        osync_data_unref((OSyncData *)data);
//         osync_hashtable_update_change ( hashtable, change );

        osync_context_report_change ( context(), change );

        osync_hashtable_update_change ( hashtable, change );
	
        osync_change_unref ( change );
    }
    osync_list_free ( uids );
    osync_error_unref(&oerror);
    
    kDebug() << "got all changes success().";
    success();
}

void DataSink::commit ( OSyncChange *change )
{
    kDebug();

    OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

    QString remoteId = QString::fromLatin1 ( osync_change_get_uid ( change ) );
    QString hash = QString::fromLatin1 ( osync_change_get_hash ( change ) );
    
    //TODO: use id to identify items
    int id = idFromHash(hash);
    
    kDebug() << "change   id:" << id;
    kDebug() << "change  uid:" << remoteId;
    kDebug() << "change hash:" << hash;
    kDebug() << "objform:" << osync_objformat_get_name ( osync_change_get_objformat ( change ) );
//     kDebug();

    Akonadi::Collection col = collection();

    if ( !col.isValid() ) {
        error( OSYNC_ERROR_GENERIC, "Invalid collection.");
        return;
    }

    switch ( (OSyncChangeType) osync_change_get_changetype ( change ) )
    {
    case OSYNC_CHANGE_TYPE_ADDED:
    {
        Item item;
        char *plain = 0; // plain is freed by data
        osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
        QString str = QString::fromUtf8( plain );
//         QString str = QString::fromLatin1( plain );
//         QString str = QString::fromLocal8Bit( plain );
	kDebug() << "data: " << str;
        setPayload ( &item, str );
        item.setRemoteId( remoteId );

        ItemCreateJob *job = new Akonadi::ItemCreateJob ( item, col );
        if ( ! job->exec() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to create job for item.");
            return;
        } else {
	  item = job->item(); // handle !job->exec in return too..
	  if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
	  }
          osync_change_set_uid ( change, item.remoteId().toLatin1().data() );
          osync_change_set_hash ( change, getHash( item.id(), item.revision() ).toLatin1().data() );

	}
        break;
    }

    case OSYNC_CHANGE_TYPE_MODIFIED:
    {
        char *plain = 0; // plain is freed by data
        osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
        QString str = QString::fromUtf8( plain );

	Item item = fetchItem ( remoteId );

        if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to fetch item.");
            return;
        }
        setPayload ( &item, str );
        kDebug() << "data" << str;

        ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob ( item );
        if ( ! modifyJob->exec() ) {
            error ( OSYNC_ERROR_GENERIC, "Unable to run modify job.");
            return;
        } else {
	  item = modifyJob->item();
	  if ( ! item.isValid() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to modify item.");
            return;
	  }
          osync_change_set_uid ( change, item.remoteId().toLatin1().data() );
          osync_change_set_hash ( change, getHash( item.id(), item.revision() ).toLatin1().data() );

	}
        break;
    }

    case OSYNC_CHANGE_TYPE_DELETED:
    {
        Item item = fetchItem ( remoteId );
        if ( ! item.isValid() ) {
	  // FIXME break or return?
//             error( OSYNC_ERROR_GENERIC, "Unable to fetch item");
            break; 
        }

//      kDebug() << "deleted: (testing skipped)" << remoteId;
// comment out to skip deleting for testing
        ItemDeleteJob *job = new ItemDeleteJob( item );
        if ( ! job->exec() ) {
            error( OSYNC_ERROR_GENERIC, "Unable to delete item");
            return;
        }
        osync_change_set_uid ( change, item.remoteId().toLatin1().data() );
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

bool DataSink::setPayload ( Item *item, const QString &str )
{
    kDebug();
    item->setMimeType ( m_MimeType );
    kDebug()<< "To mimetype: " << m_MimeType;
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

const Item DataSink::fetchItem ( int id )
{
    kDebug();
  ItemFetchJob *fetchJob = new ItemFetchJob( Item( id ) );
  fetchJob->fetchScope().fetchFullPayload();

  if( fetchJob->exec() ) {
    foreach ( const Item &item, fetchJob->items() ) {
      if(  item.id() == id ) {
        kDebug() << "got item";
        return item;
      }
    }
  }

  // no such item found?
  // we'll check after calling this function
  return Item();
}

const Item DataSink::fetchItem ( const QString& remoteId )
{
    kDebug();

    ItemFetchJob *fetchJob = new ItemFetchJob ( collection() );
    fetchJob->fetchScope().fetchFullPayload();
    if ( fetchJob->exec() )
        foreach ( const Item &item, fetchJob->items() )
        if ( item.remoteId() == remoteId )
            return item;
    // no such item found?
    // we'll check after calling this function
    return Item();
}

void DataSink::syncDone()
{
    kDebug() << "sync for sink member done";
    // Do we need this in 0.40???
//     OSyncError *error = 0;
//     osync_objtype_sink_save_hashtable ( sink() , &error );
//     if ( error ) {
//         warning ( error );
//         return;
//     }
    success();
}

QString DataSink::getHash( int id, int rev ) {
  return QString::number(id) + "-" + QString::number( rev ) ;
}

int DataSink::idFromHash( const QString hash) {
  QString str = hash;
  str.remove(QRegExp("-.*"));
  kDebug() << str;
  return str.toInt();
}
#include "datasink.moc"
