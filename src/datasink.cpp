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

// #include <glib.h>
// #include <glib/ghash.h>

#include "datasink.h"

// #include <opensync/opensync.h>
// #include <opensync/opensync-helper.h>
// #include <opensync/opensync-plugin.h>

#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>

// calendar includes
#include <kcal/incidence.h>
#include <kcal/icalformat.h>

// contact includes
#include <kabc/vcardconverter.h>
#include <kabc/addressee.h>

// #include <glib/gtypes.h>
// #include <glib/glist.h>
// notes includes
// TODO

#include <KDebug>
#include <KLocale>
#include <KUrl>

using namespace Akonadi;

typedef boost::shared_ptr<KCal::Incidence> IncidencePtr;

DataSink::DataSink ( int type ) :
    SinkBase ( GetChanges | Commit | SyncDone )
{
  kDebug() << "Create obj:" << type;
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
  kDebug() << "TYPE        " << m_type;
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
      return false;
    }

  wrapSink ( sink );

    
  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink );

  if ( ! hashtable )
    kDebug() << ">> NO hashtable for objtype:" << osync_objtype_sink_get_name ( sink );

  OSyncPluginResource *resource = osync_plugin_config_find_active_resource ( config, osync_objtype_sink_get_name ( sink ) );
  OSyncList *objfrmtList = osync_plugin_resource_get_objformat_sinks ( resource );

  OSyncList *r;
  bool hasObjFormat;
  for ( r = objfrmtList;r;r = r->next )
    {
      OSyncObjFormatSink *objformatsink = ( OSyncObjFormatSink * ) r->data;

      switch ( m_type )
        {
        case Contacts:
        {
          if ( !strcmp ( "vcard21", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vcard21";
              break;
            }
          else
            kDebug() << "No objformat vcard21";

          if ( !strcmp ( "vcard30", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vcard30";
              break;
            }
          else
            kDebug() << "No objformat vcard30";

          break;
        }
        case Calendars:
        {
          if ( !strcmp ( "vevent10", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vevent10";
              break;
            }
          else
            kDebug() << "No objformat vevent10";
          break;
          if ( !strcmp ( "vevent20", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vevent20";
              break;
            }
          else
            kDebug() << "No objformat vevent20";
          break;
        }
        case Notes:
        {
          if ( !strcmp ( "vnote11", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vnote11";
              break;
            }
          else
            kDebug() << "No objformat vnote11";
          break;
        }
        case Journals:
        {
          if ( !strcmp ( "vjournal", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vjournal";
              break;
            }
          else
            kDebug() << "No objformat vjournal";
          break;
        }
        case Todos:
        {
          if ( !strcmp ( "vtodo10", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vtodo10";
              break;
            }
          else
            kDebug() << "No objformat vtodo10";
          if ( !strcmp ( "vtodo20", osync_objformat_sink_get_objformat ( objformatsink ) ) )
            {
              hasObjFormat = true;
              kDebug() << "Has objformat vtodo20";
              break;
            }
          else
            kDebug() << "No objformat vtodo20";
          break;
        }

        default:
          continue;
        }
    }

//          if (!hasObjFormat) {
// 			 kDebug() << "No objformat vcard30";
//                  return false;
//          }

  osync_objtype_sink_set_userdata ( sink, this );
  osync_objtype_sink_enable_hashtable ( sink , TRUE );

  return true;
}

Akonadi::Collection DataSink::collection() const
  {
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
  kDebug() << " DataSink::getChanges() called";
  OSyncError *oerror = 0;

  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

  Collection col = collection(); // osync_objtype_sink_get_name(sink())
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

  // FIXME give me a real eventloop please!
  if ( !job->exec() )
    {
      error ( OSYNC_ERROR_IO_ERROR, job->errorText() );
      return;
    }

  success();
}

void DataSink::slotItemsReceived ( const Item::List &items )
{
  kDebug() << "retrieved" << items.count() << "items";
  Q_FOREACH ( const Item& item, items )
  {
    kDebug() << "Id:" << item.id() << "\n";
    kDebug() << "Mime:" << item.mimeType() << "\n";
    kDebug() << "Revision:" << item.revision() << "\n";
    kDebug() << "StorageCollectionId:" << item.storageCollectionId() << "\n";
    kDebug() << "Url:" << item.url() << "\n";
    reportChange ( item );
  }
}

void DataSink::reportChange ( const Item& item )
{

//     kDebug() << item.id() << "\n" ;
  kDebug() << "item.payloadData().data():" << item.payloadData().data() << "\n" ;

  OSyncError *error = 0;
  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );


  OSyncChange *change = osync_change_new ( &error );
  if ( !change )
    {
      osync_change_unref ( change );
      warning ( error );
      return;
    }

  osync_change_set_uid ( change, QString::number ( item.id() ).toLatin1() );


  OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env ( pluginInfo() );
  OSyncObjFormat *format = osync_format_env_find_objformat ( formatenv, formatName().toLatin1() );

//
  error = 0;
  // Now you can set the data for the object
  // Set the last argument to FALSE if the real data
  // should be queried later in a "get_data" function
//     odata = osync_data_new(NULL, 0, format, &error);
  OSyncData *odata = osync_data_new ( item.payloadData().data() , item.payloadData().size(), format, &error );
  if ( !odata )
    {
      osync_change_unref ( change );
      return;
    }

// //     osync_data_set_objtype( odata, osync_objtype_sink_get_name( sink() ) );
//
  osync_change_set_data ( change, odata );
  osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
  OSyncChangeType changetype = osync_hashtable_get_changetype ( hashtable, change );
  osync_change_set_changetype ( change, changetype );
  osync_hashtable_update_change ( hashtable, change );

  osync_data_unref ( odata );

  /* */
  kDebug() << "changeid:" << osync_change_get_uid ( change )  << "; "
  << "itemid:" << item.id() << "; "
  << "revision:" << item.revision() << "; "
  << "changetype:" << changetype << "; "
  << "hash:" << osync_hashtable_get_hash ( hashtable, osync_change_get_uid ( change ) ) << "; "
  << "objtype:" << osync_change_get_objtype ( change ) << "; "
  << "objform:" << osync_objformat_get_name ( osync_change_get_objformat ( change ) ) << "; "
  << "sinkname:" << osync_objtype_sink_get_name ( sink() );
  /* */

  if ( changetype != OSYNC_CHANGE_TYPE_UNMODIFIED )
    osync_context_report_change ( context(), change );


}

void DataSink::slotGetChangesFinished ( KJob * )
{
  OSyncFormatEnv *formatenv = osync_plugin_info_get_format_env ( pluginInfo() );
  OSyncObjFormat *format = osync_format_env_find_objformat ( formatenv, formatName().toLatin1() );

  // after the items have been processed, see what's been deleted and send them to opensync
  OSyncError *error = 0;
//   FIXME:
  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );
  OSyncList *u, *uids = osync_hashtable_get_deleted ( hashtable );
//
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
//
      osync_change_set_uid ( change, uid.toLatin1() );
      osync_change_set_changetype ( change, OSYNC_CHANGE_TYPE_DELETED );
      error = 0;
      OSyncData *odata = osync_data_new ( NULL, 0, format, &error );
      if ( !odata )
        {
          osync_change_unref ( change );
          warning ( error );
          continue;
        }

      osync_data_set_objtype ( odata, osync_objtype_sink_get_name ( sink() ) );
      osync_change_set_data ( change, odata );
      osync_data_unref ( odata );

//         osync_context_report_change(context(), change);
      osync_hashtable_update_change ( hashtable, change );

      osync_change_unref ( change );
    }
  osync_list_free ( uids );

  kDebug() << "got all changes..";

}

void DataSink::commit ( OSyncChange *change )
{
  kDebug() << "change uid:" << osync_change_get_uid ( change );
  kDebug() << "objtype:" << osync_change_get_objtype ( change );
  kDebug() << "objform:" << osync_objformat_get_name ( osync_change_get_objformat ( change ) );
  OSyncHashTable *hashtable = osync_objtype_sink_get_hashtable ( sink() );

  switch ( osync_change_get_changetype ( change ) )
    {
    case OSYNC_CHANGE_TYPE_ADDED:
    {
      const Item item = createItem ( change );
      osync_change_set_uid ( change, QString::number ( item.id() ).toLatin1() );
      osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
      kDebug() << "ADDED:" << osync_change_get_uid ( change );
      break;
    }

    case OSYNC_CHANGE_TYPE_MODIFIED:
    {
      const Item item = modifyItem ( change );
      osync_change_set_uid ( change, QString::number ( item.id() ).toLatin1() );
      osync_change_set_hash ( change, QString::number ( item.revision() ).toLatin1() );
      kDebug() << "MODIFIED:" << osync_change_get_uid ( change );
      break;
    }

    case OSYNC_CHANGE_TYPE_DELETED:
    {
      deleteItem ( change );
      kDebug() << "DELETED:" << osync_change_get_uid ( change );
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
    }

  osync_hashtable_update_change ( hashtable, change );
  success();
}

const Item DataSink::createAkonadiItem ( OSyncChange *change )
{
  char *plain = 0;
  osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
  QString str = QString::fromLatin1 ( plain );
  Akonadi::Item item;
  setPayload ( &item, str.toUtf8() );
  return item;
}

const Item DataSink::createItem ( OSyncChange *change )
{
  Collection col = collection();
  if ( !col.isValid() ) // error handling
    return Item();
  kDebug() << "cuid:" << osync_change_get_uid ( change );

  ItemCreateJob *job = new Akonadi::ItemCreateJob ( createAkonadiItem ( change ), col );

  if ( ! job->exec() )
    kDebug() << "creating an item failed";

  return job->item(); // handle !job->exec in return too..
}

const Item DataSink::modifyItem ( OSyncChange *change )
{
  char *plain = 0;
  osync_data_get_data ( osync_change_get_data ( change ), &plain, /*size*/0 );
  QString str = QString::fromLatin1 ( plain );

  QString id = QString ( osync_change_get_uid ( change ) );
  Item item = fetchItem ( id );
  if ( ! item.isValid() ) // TODO proper error handling
    return Item();

  //event.setMimeType( "application/x-vnd.akonadi.calendar.event" );
  //Item newItem = createAkonadiItem( change );
  setPayload ( &item, str.toUtf8() );
  ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob ( item );
  if ( modifyJob->exec() )
    {
      kDebug() << "modification completed";
      return modifyJob->item();
    }
  else
    kDebug() << "unable to modify";


  return Item();
}

void DataSink::deleteItem ( OSyncChange *change )
{
  QString id = QString ( osync_change_get_uid ( change ) );
  Item item = fetchItem ( id );
  if ( ! item.isValid() ) // TODO proper error handling
    return;

  kDebug() << "delete with id: " << item.id();
  // TODO
  /*ItemDeleteJob *job = new ItemDeleteJob( item );

  if( job->exec() ) {
   kDebug() << "item deleted";
  }
  else
   kDebug() << "unable to delete item";*/

  success(); // TODO
}

bool DataSink::setPayload ( Item *item, const QString &str )
{
  switch ( m_type )
    {
    case Contacts:
    {
      KABC::VCardConverter converter;
      KABC::Addressee vcard = converter.parseVCard ( str.toUtf8() );
      item->setMimeType ( KABC::Addressee::mimeType() );
      item->setPayloadFromData ( vcard.toString().toLatin1() );
      break;
    }
    case Calendars:
    {
      KCal::ICalFormat format;
      KCal::Incidence *calEntry = format.fromString ( str.toUtf8() );
      item->setMimeType ( "application/x-vnd.akonadi.calendar.event" );
      item->setPayload<IncidencePtr> ( IncidencePtr ( calEntry->clone() ) );

      break;
    }
    case Todos:
    {
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

const QString DataSink::formatName()
{
  // FIXME -- this should be saved somewhere in the config,  why settign and checking here:
  QString formatName;
  switch ( m_type )
    {
    case Calendars:
      formatName = "vevent20";
      break;
    case Contacts:
      formatName = "vcard30";
      break;
    case Notes:
      formatName = "vjournal";
      break;
    case Todos:
      formatName = "vtodo20";
      break;
    default:
      kDebug() << "invalid datasink type";
      return QString();
    }

  return formatName;
}

const Item DataSink::fetchItem ( const QString& id )
{
  ItemFetchJob *fetchJob = new ItemFetchJob ( Item ( id ) );
  fetchJob->fetchScope().fetchFullPayload();

  if ( fetchJob->exec() )
    {
      foreach ( const Item &item, fetchJob->items() )
      {
        if ( QString::number ( item.id() ) == id )
          {
            kDebug() << "got item";
            return item;
          }
      }
    }

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
