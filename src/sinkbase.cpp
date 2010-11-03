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

#define WRAP() \
  osync_trace( TRACE_ENTRY, "%s(%p,%p, %p, %p)", __PRETTY_FUNCTION__, sink, info, ctx, userdata); \
  kDebug() << osync_objtype_sink_get_name( sink );\
  SinkBase *sb = reinterpret_cast<SinkBase*>(userdata); \
  sb->setSink(sink);\
  sb->setPluginInfo( info );\
  sb->setContext( ctx );

extern "C"
{

    static void connect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata )
    {
        WRAP( )
        sb->connect();
        osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
    }

    static void disconnect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
        WRAP( )
        sb->disconnect();
	osync_objtype_sink_unref(sink);
        osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
    }

    static void get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {

        WRAP ( )
        sb->setSlowSink(slow_sync);
        sb->getChanges();
        osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
    }

    static void sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
        WRAP( )
        sb->syncDone();
        osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
    }

    static void commit_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  OSyncChange *change, void *userdata) {
        WRAP( )
        sb->commit(change);
        osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
    }

//     static void commitAll_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  void *userdata) {
//         WRAP(  )
//         sb->commitAll();
//         osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
//     }
// 
//     static void read_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  OSyncChange *change, void *userdata) {
//         WRAP(  )
//         sb->commit(change);
// 	osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
//     }
//
//     static void commit_all_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  OSyncChange *change, void *userdata) {
//         WRAP(  )
//         sb->commitAll(change);
// 	osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );
//     }

} // extern C


SinkBase::SinkBase( int features ) :
        mSink( 0 ),
        mPluginInfo( 0 ),
        mContext( 0 ),
        m_canConnect(false),
        m_canDisconnect(false),
        m_canCommit    (false),
        m_canCommitAll (false),
//  unused   m_canBatchCommit(false),
        m_canGetChanges(false),
        m_canWrite     (false),
        m_canRead      (false),
        m_canSyncDone  (false),
        m_SlowSync     (false)
{

    m_canConnect    = ( features & Connect ) ? true : false;
    m_canDisconnect = ( features & Disconnect ) ? true : false;
    m_canGetChanges = ( features & GetChanges ) ? true : false;
    m_canCommit     = ( features & Commit ) ? true : false;
    m_canWrite      = ( features & Write ) ? true : false;
    m_canCommitAll  = ( features & CommittedAll ) ? true : false;
    m_canRead       = ( features & Read ) ? true : false;
//  unused   m_canBatchCommit = ( features & BatchCommit ) ? true : false;
    m_canSyncDone   = ( features & SyncDone ) ? true : false;

}

SinkBase::~SinkBase()
{
//     if ( mContext)
//         osync_context_unref(mContext);
    if ( mSink )
        osync_objtype_sink_unref( mSink );
//     if (mPluginInfo)
//         osync_plugin_info_unref(mPluginInfo);
}

void SinkBase::connect()
{
    Q_ASSERT( false );
}

void SinkBase::setSlowSink(osync_bool s)
{
  kDebug();
    m_SlowSync = s;
}

osync_bool SinkBase::getSlowSink()
{
  kDebug();
    return m_SlowSync;
}

void SinkBase::disconnect()
{
  kDebug();
    Q_ASSERT( false );
}

void SinkBase::getChanges()
{
  kDebug();
    Q_ASSERT( false );
}

void SinkBase::commit(OSyncChange * chg)
{
  kDebug();
    Q_UNUSED( chg );
    Q_ASSERT( false );
}

// void SinkBase::commitAll()
// {
//     Q_ASSERT( false );
// }
// 
// void SinkBase::write()
// {
//     Q_ASSERT( false );
// }
// 
// void SinkBase::read()
// {
//     Q_ASSERT( false );
// }


void SinkBase::syncDone()
{
  kDebug();
    Q_ASSERT( false );
}

void SinkBase::success() const
{
    kDebug();
    Q_ASSERT( mContext );
    osync_context_report_success( mContext );
    mContext = 0;
}

void SinkBase::error(OSyncErrorType type, QString msg) const
{
    kDebug();
    Q_ASSERT( mContext );
    OSyncError *oerror;
    osync_error_set(&oerror, type, "%s", msg.toUtf8().data() );
    osync_context_report_osyncerror(mContext, oerror );
    osync_error_unref( &oerror );
    mContext = 0;
}

void SinkBase::warning(OSyncError * error) const
{
    kDebug();
    Q_ASSERT( mContext );
    osync_context_report_osyncwarning( mContext, error );
    osync_error_unref( &error );
    mContext = 0;
}

void SinkBase::wrapSink(OSyncObjTypeSink* sink)
{
    kDebug();
    Q_ASSERT( sink );
    Q_ASSERT( mSink == 0 );
    mSink = sink;

    kDebug() << ">> m_canConnect:" << m_canConnect;
    kDebug() << ">> m_canDisconnect:" << m_canDisconnect;
    kDebug() << ">> m_canCommit:" << m_canCommit;
    kDebug() << ">> m_canGetChanges:" << m_canGetChanges;
    kDebug() << ">> m_canWrite:" << m_canWrite;
    kDebug() << ">> m_canCommitAll:" << m_canCommitAll;
    kDebug() << ">> m_canRead:" << m_canRead;
    kDebug() << ">> m_canSyncDone:" << m_canSyncDone;

    if ( m_canConnect ) {
        osync_objtype_sink_set_connect_func(sink, connect_wrapper);
        osync_objtype_sink_set_connect_timeout(sink, 15);
    }
    if ( m_canDisconnect ) {
        osync_objtype_sink_set_disconnect_func(sink, disconnect_wrapper);
        osync_objtype_sink_set_disconnect_timeout(sink, 15);
    }
    if ( m_canGetChanges ) {
        osync_objtype_sink_set_get_changes_func(sink, get_changes_wrapper);
        osync_objtype_sink_set_getchanges_timeout(sink, 15);
    }
    if ( m_canCommit ) {
        osync_objtype_sink_set_commit_func(sink, commit_wrapper);
        osync_objtype_sink_set_commit_timeout(sink, 15);
    }
//     if ( m_canCommitAll ) {
//         osync_objtype_sink_set_committed_all_func(sink, commitAll_wrapper);
//         osync_objtype_sink_set_committedall_timeout(sink, 15);
//     }
    if ( m_canSyncDone ) {
        osync_objtype_sink_set_sync_done_func(sink, sync_done_wrapper);
        osync_objtype_sink_set_syncdone_timeout(sink, 15);
    }
//   TODO: check if relevant for akonadi
//   if ( m_canWrite )
//     osync_objtype_sink_set_write(sink, TRUE);
//   if ( m_canRead ) {
//     osync_objtype_sink_set_read_func(sink, read_wrapper);
// 	osync_objtype_sink_set_read_timeout(sink, 5);
//   }

    osync_objtype_sink_set_userdata ( sink, this );

}

void SinkBase::setSink(OSyncObjTypeSink *sink)
{
  kDebug();
    Q_ASSERT( sink == mSink );
    mSink = sink;
}

void SinkBase::setPluginInfo(OSyncPluginInfo * info)
{
  kDebug();
    Q_ASSERT( mPluginInfo == 0 || mPluginInfo == info );
    mPluginInfo = info;
}

void SinkBase::setContext(OSyncContext * context)
{
  kDebug();
    Q_ASSERT( mContext == 0 || context == mContext );
    // ### do I need to ref() that here? and then probably deref() the old one somewhere above?
    mContext = context;
}


#include "sinkbase.moc"
