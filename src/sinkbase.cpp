/*
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>

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

#define WRAP0(X) \
  osync_trace( TRACE_ENTRY, "%s(%p,%p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx); \
  SinkBase *sb = reinterpret_cast<SinkBase*>(userdata ); \
  sb->setContext( ctx ); \
  sb->setPluginInfo( info ); \
  sb->X; \
  osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );

#define WRAP1(X) \
  osync_trace( TRACE_ENTRY, "%s(%p,%p, %p, %p)", __PRETTY_FUNCTION__, sink, userdata, info, ctx); \
  SinkBase *sb = reinterpret_cast<SinkBase*>(userdata ); \
  if ( slow_sync ) sb->setSlowSink(slow_sync); \
  sb->setContext( ctx ); \
  sb->setPluginInfo( info ); \
  sb->X; \
  osync_trace( TRACE_EXIT, "%s", __PRETTY_FUNCTION__ );



extern "C"
{

  static void connect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata )
{
   WRAP0( connect() )
}

static void disconnect_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
   WRAP0( disconnect() )
}

static void contact_get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {
    
    WRAP1 ( getChanges() )
}
// 

static void contact_sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
  WRAP0( syncDone() )
}

static void event_get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {
  WRAP1( getChanges() )
}
// 
static void event_sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
  WRAP0( syncDone() )
}


static void todo_get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {
  WRAP1( getChanges() )
}
//

static void todo_sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
  WRAP0( syncDone() )
}

static void journal_get_changes_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, osync_bool slow_sync, void *userdata) {
  WRAP1( getChanges() )
}
// 

static void journal_sync_done_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx, void *userdata) {
  WRAP0( syncDone() )
}
// 
static void commit_wrapper(OSyncObjTypeSink *sink, OSyncPluginInfo *info, OSyncContext *ctx,  OSyncChange *change, void *userdata) {
  WRAP0( commit(change) )
}

} // extern C


SinkBase::SinkBase( int features ) :
    mContext( 0 ),
    mSink( 0 ),
    mPluginInfo( 0 )
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

  if ( m_canConnect )
    osync_objtype_sink_set_connect_func(sink, connect_wrapper);
  if ( m_canDisconnect )
    osync_objtype_sink_set_disconnect_func(sink, disconnect_wrapper);
  
  if ( m_canGetChanges && m_hasContact ) 
      osync_objtype_sink_set_get_changes_func(sink, contact_get_changes_wrapper);
  if (  m_canGetChanges && m_hasEvent )
      osync_objtype_sink_set_get_changes_func(sink, event_get_changes_wrapper);
  if (  m_canGetChanges && m_hasTodo )
      osync_objtype_sink_set_get_changes_func(sink, todo_get_changes_wrapper);
  if (  m_canGetChanges && m_hasNote )
      osync_objtype_sink_set_get_changes_func(sink, journal_get_changes_wrapper);
  
  if ( m_canCommit )
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
  if ( m_canSyncDone && m_hasContact )
    osync_objtype_sink_set_sync_done_func(sink, contact_sync_done_wrapper);
  if ( m_canSyncDone && m_hasEvent )
    osync_objtype_sink_set_sync_done_func(sink, event_sync_done_wrapper);
  if ( m_canSyncDone && m_hasTodo )
    osync_objtype_sink_set_sync_done_func(sink, todo_sync_done_wrapper);
  if ( m_canSyncDone && m_hasNote )
    osync_objtype_sink_set_sync_done_func(sink, journal_sync_done_wrapper);
  
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
