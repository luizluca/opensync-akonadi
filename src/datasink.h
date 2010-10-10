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

#ifndef DATASINK_H
#define DATASINK_H

#include "sinkbase.h"

#include <akonadi/collection.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemmodifyjob.h>
#include <akonadi/itemfetchscope.h>

#include <opensync/opensync.h>
#include <opensync/opensync-plugin.h>
#include <opensync/opensync-data.h>
#include <opensync/opensync-format.h>

#include <KUrl>

#include <boost/shared_ptr.hpp>

using namespace Akonadi;

/**
 * Base class for data sink classes, dealing with the type-independent stuff.
 */
class DataSink : public SinkBase
{
  Q_OBJECT

  public:
    enum Type { Calendars = 0, Contacts, Todos,  Notes };

    DataSink( int type );
    ~DataSink();

    bool initialize( OSyncPlugin *plugin, OSyncPluginInfo *info, OSyncObjTypeSink *sink, OSyncError **error );

    void getChanges();
    void commit( OSyncChange *change );
    void syncDone();

  public slots:
    void slotGetChangesFinished( KJob * );
    void slotItemsReceived( const Akonadi::Item::List & );

  protected:

    /**
     * Returns the collection we are supposed to sync with.
     */
    Akonadi::Collection collection() const;

    /**
     * This reports the change back to opensync
    */
    void reportChange( const Item& item, const QString& format );

    /**
     * Reimplement in subclass to provide data about changes to opensync.
     * Note, that you must call DataSink::reportChange( Item, QString, int ) 
     * after you have organized data to be send to opensync.
     */
    void reportChange( const Item & item );

    /**
     * Deletes an item based on the data given by opensync.
     */
    void deleteItem( OSyncChange *change );

  private:
    const Item fetchItem( const QString& id );
    bool setPayload( Item *item, const QString &str );

  private:
    OSyncHashTable *m_hashtable;
    int m_type;
    QString m_Format;
    bool m_Enabled;
    QString m_Url;
    QString m_MimeType;
};

#endif
