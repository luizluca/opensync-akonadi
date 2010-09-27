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

#include "sinkbase.h"
#include <KDebug>

#define WRAP(X) \
  osync_trace( TRACE_ENTRY, "%s(%p,%p, %p, %p)", __PRETTY_FUNCTION__, sink, info, ctx, userdata); \
  SinkBase *sb = reinterpret_cast<SinkBase*>(userdata ); \
  sb->setContext( ctx ); \
  sb->setPluginInfo( info ); \
  sb->X; \
  osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );

extern "C"
{

    static void connect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata )
    {
        WRAP( connect() )
        osync_context_report_success(ctx);
    }

    static void disconnect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
        WRAP( disconnect() )
//         osync_context_report_success(ctx);
    }

    static void get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {

        WRAP ( getChanges() )
        if ( slow_sync )
            sb->setSlowSink(slow_sync);
//         osync_context_report_success(ctx);
    }

    static void sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
        WRAP( syncDone() )
    }

    static void commit_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  OSyncChange *change, void *userdata) {
        WRAP( commit(change) )
    }

} // extern C


SinkBase::SinkBase( int features ) :
        mContext( 0 ),
        mSink( 0 ),
        mPluginInfo( 0 ),
        m_SlowSync( false )
{

    m_canConnect = ( features & Connect ) ? true : false;
    m_canDisconnect = ( features & Disconnect ) ? true : false;
    m_canGetChanges = ( features & GetChanges ) ? true : false;
    m_canCommit = ( features & Commit ) ? true : false;
    m_canWrite = ( features & Write ) ? true : false;
    m_canCommitAll = ( features & CommittedAll ) ? true : false;
    m_canRead = ( features & Read ) ? true : false;
    m_canBatchCommit = ( features & BatchCommit ) ? true : false;
    m_canSyncDone = ( features & SyncDone ) ? true : false;

}

SinkBase::~SinkBase()
{
    if ( mSink )
        osync_objtype_sink_unref( mSink );
}

void SinkBase::connect()
{
    Q_ASSERT( false );
}

void SinkBase::setSlowSink(osync_bool s)
{
    Q_ASSERT( m_SlowSync );
    m_SlowSync = s;
}

osync_bool SinkBase::getSlowSink()
{
    Q_ASSERT( m_SlowSync );
    return m_SlowSync;
}

void SinkBase::disconnect()
{
    Q_ASSERT( false );
}

void SinkBase::getChanges()
{
    Q_ASSERT( false );
}

void SinkBase::commit(OSyncChange * chg)
{
    Q_UNUSED( chg );
    Q_ASSERT( false );
}

void SinkBase::syncDone()
{
    Q_ASSERT( false );
}

void SinkBase::success() const
{
    kDebug();
    Q_ASSERT( mContext );
    osync_context_report_success( mContext );
    mContext = 0;
}

void SinkBase::error(OSyncErrorType type, const QString &msg) const
{
    kDebug();
    Q_ASSERT( mContext );
    osync_context_report_error(mContext, type, "%s", msg.toLatin1().data() );
    mContext = 0;
}

void SinkBase::warning(OSyncError * error) const
{
    kDebug();
    Q_ASSERT( mContext );
    osync_context_report_osyncwarning( mContext, error );
    osync_error_unref( &error );
}

void SinkBase::wrapSink(OSyncObjTypeSink* sink)
{
    kDebug();
    Q_ASSERT( sink );
    Q_ASSERT( mSink == 0 );
    mSink = sink;
    
        
    kDebug() << ">> m_canConnect:" << m_canConnect;
    kDebug() << ">> m_canGetChanges:" << m_canGetChanges;
//     kDebug() << ">> NO hashtable for objtype:" << m_canConnect;

    if ( ! m_canConnect )
        osync_objtype_sink_set_connect_func(sink, connect_wrapper);
    if ( ! m_canDisconnect  || m_isContact || m_isEvent )
        osync_objtype_sink_set_disconnect_func(sink, disconnect_wrapper);
    if ( ! m_canGetChanges || m_isContact || m_isEvent )
        osync_objtype_sink_set_get_changes_func(sink, get_changes_wrapper);

    if ( ! m_canCommit  || m_isContact || m_isEvent )
        osync_objtype_sink_set_commit_func(sink, commit_wrapper);
//   TODO: check if relevant for akonadi
//   if ( m_canWrite )
//     osync_objtype_sink_set_commit_func(sink, 0);
//   if ( m_canCommitAll )
//     osync_objtype_sink_set_commit_func(sink, 0);
//   if ( m_canRead )
//     osync_objtype_sink_set_commit_func(sink, 0);
//   if ( m_canBatchCommit )
//     osync_objtype_sink_set_commit_func(sink, 0);
    if ( ! m_canSyncDone  || m_isContact || m_isEvent)
        osync_objtype_sink_set_sync_done_func(sink, sync_done_wrapper);

}

void SinkBase::setPluginInfo(OSyncPluginInfo * info)
{
    Q_ASSERT( mPluginInfo == 0 || mPluginInfo == info );
    mPluginInfo = info;
}

void SinkBase::setContext(OSyncContext * context)
{
    Q_ASSERT( mContext == 0 || context == mContext );
    // ### do I need to ref() that here? and then probably deref() the old one somewhere above?
    mContext = context;
}


#include "sinkbase.moc"
