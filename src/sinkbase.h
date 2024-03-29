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

#ifndef SINKBASE_H
#define SINKBASE_H

#include <QObject>

#include <opensync/opensync.h>
#include <opensync/opensync-plugin.h>
#include <opensync/opensync-helper.h>

/**
 * Base class providing wraping of OSyncObjTypeSink.
 */
class SinkBase : public QObject
{
    Q_OBJECT

public:
    enum Feature {
        Connect = 1,
        Disconnect = 2,
        GetChanges = 4,
        Commit = 8,
        Write = 16,
        CommittedAll = 32,
        Read = 64,
        BatchCommit = 128,
        SyncDone = 256
    };

    SinkBase( int features );
    virtual ~SinkBase();

    virtual void connect();
    virtual void disconnect();
    virtual void getChanges();
    virtual void commit( OSyncChange *chg );
//     virtual void write();
//     virtual void read();
//     virtual void commitAll();
    virtual void syncDone();

    OSyncContext* context() const {
        return mContext;
    }

    OSyncPluginInfo *pluginInfo() const {
        return mPluginInfo;
    }
    osync_bool getSlowSink ();
    
    void setPluginInfo( OSyncPluginInfo *info );
    void setContext( OSyncContext *context );
    void setSink( OSyncObjTypeSink *sink);
    void setSlowSink (osync_bool);

protected:
    void success() const;
    void error(OSyncErrorType type, QString msg) const;
    void warning( OSyncError *error ) const;
    void wrapSink(OSyncObjTypeSink* sink );
    OSyncObjTypeSink* sink() const {
        return mSink;
    }

private:

    OSyncObjTypeSink *mSink;
    OSyncPluginInfo *mPluginInfo;
    mutable OSyncContext *mContext;
//     what do we have and what can we do
    bool m_canConnect, m_canDisconnect, m_canCommit, m_canCommitAll,
    m_canGetChanges, m_canWrite, m_canRead, m_canSyncDone, m_canBatchCommit;
//  unused   bool  m_canCommitRead;
    osync_bool m_SlowSync;
};

#endif //End of SINKBASE_H