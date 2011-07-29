/*
 * Copyright (c) 2011 by Michael Berlin, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#ifndef CPP_INCLUDE_LIBXTREEMFS_FILE_INFO_H_
#define CPP_INCLUDE_LIBXTREEMFS_FILE_INFO_H_

#include <boost/cstdint.hpp>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <gtest/gtest_prod.h>
#include <list>
#include <map>
#include <string>

#include "xtreemfs/GlobalTypes.pb.h"

namespace xtreemfs {

class FileHandleImplementation;
class VolumeImplementation;

namespace pbrpc {
class Lock;
class Stat;
class UserCredentials;
}  // namespace pbrpc

/** Different states regarding osd_write_response_ and its write back. */
enum FilesizeUpdateStatus {
  kClean, kDirty, kDirtyAndAsyncPending, kDirtyAndSyncPending
};

class FileInfo {
 public:
  FileInfo(VolumeImplementation* volume,
           boost::uint64_t file_id,
           const std::string& path,
           bool replicate_on_close,
           const xtreemfs::pbrpc::XLocSet& xlocset,
           const std::string& client_uuid);
  ~FileInfo();

  inline int reference_count() {
    boost::mutex::scoped_lock lock(mutex_);
    return reference_count_;
  }

  /** Only used for debug output. */
  inline std::string path() {
    boost::mutex::scoped_lock lock(mutex_);
    return path_;
  }

  /** Copies the XlocSet into new_xlocset. */
  inline void GetXLocSet(xtreemfs::pbrpc::XLocSet* new_xlocset) {
    assert(new_xlocset);
    boost::mutex::scoped_lock lock(xlocset_mutex_);
    new_xlocset->CopyFrom(xlocset_);
  }

  /** Copies the XlocSet into new_xlocset and the index of the current replica
   *  to "current_replica_index". */
  inline void GetXLocSet(xtreemfs::pbrpc::XLocSet* new_xlocset,
                         int* current_replica_index) {
    assert(new_xlocset);
    boost::mutex::scoped_lock lock(xlocset_mutex_);
    new_xlocset->CopyFrom(xlocset_);
    *current_replica_index = current_replica_index_;
  }

  /** Change the index of the current replica. */
  inline void set_current_replica_index(int new_index) {
    boost::mutex::scoped_lock lock(xlocset_mutex_);
    current_replica_index_ = new_index;
  }

  /** Returns a new FileHandle object to which xcap belongs.
   *
   * @remark Ownership is transferred to the caller.
   */
  FileHandleImplementation* CreateFileHandle(const xtreemfs::pbrpc::XCap& xcap);

  /** See CreateFileHandle(xcap). Does not add file_handle to list of open
   *  file handles if used_for_pending_filesize_update=true.
   *
   *  This function will be used if a FileHandle was solely created to
   *  asynchronously write back a dirty file size update (osd_write_response_).
   *
   * @remark Ownership is transferred to the caller.
   */
  FileHandleImplementation* CreateFileHandle(
      const xtreemfs::pbrpc::XCap& xcap,
      bool used_for_pending_filesize_update);

  /** Deregisters a closed FileHandle. Called by FileHandle::Close(). */
  void CloseFileHandle(FileHandleImplementation* file_handle);

  /** Decreases the reference count and returns the current value. */
  int DecreaseReferenceCount();

  /** Copies osd_write_response_ into response if not NULL. */
  void GetOSDWriteResponse(xtreemfs::pbrpc::OSDWriteResponse* response);

  /** Writes path_ to path. */
  void GetPath(std::string* path);

  /** Changes path_ to new_path if path_ == path. */
  void RenamePath(const std::string& path, const std::string& new_path);

  /** Compares "response" against the current "osd_write_response_". Returns
   *  true if response is newer and assigns "response" to "osd_write_response_".
   *
   *  If successful, a new file handle will be created and xcap is required to
   *  send the osd_write_response to the MRC in the background.
   *
   *  @remark   Ownership of response is transferred to this object if this
   *            method returns true. */
  bool TryToUpdateOSDWriteResponse(xtreemfs::pbrpc::OSDWriteResponse* response,
                                   const xtreemfs::pbrpc::XCap& xcap);

  /** Merge into a possibly outdated Stat object (e.g. from the StatCache) the
   *  current file size and truncate_epoch from a stored OSDWriteResponse. */
  void MergeStatAndOSDWriteResponse(xtreemfs::pbrpc::Stat* stat);

  /** Sends pending file size updates to the MRC asynchronously. */
  void WriteBackFileSizeAsync();

  /** Renews xcap of all file handles of this file asynchronously. */
  void RenewXCapsAsync();

  /** Releases all locks using file_handle to issue ReleaseLock(). */
  void ReleaseAllLocks(FileHandleImplementation* file_handle);

  /** Blocks until all asynchronous file size updates are completed. */
  void WaitForPendingFileSizeUpdates();

  /** Called by the file size update callback of FileHandle. */
  void AsyncFileSizeUpdateResponseHandler(
      const xtreemfs::pbrpc::OSDWriteResponse& owr,
      FileHandleImplementation* file_handle,
      bool success);

  /** Passes FileHandle::GetAttr() through to Volume. */
  void GetAttr(
      const xtreemfs::pbrpc::UserCredentials& user_credentials,
      xtreemfs::pbrpc::Stat* stat);

  /** Compares "lock" against list of active locks.
   *
   *  Sets conflict_found to true and copies the conflicting, active lock into
   *  "conflicting_lock".
   *  If no conflict was found, "lock_for_pid_cached" is set to true if there
   *  exists already a lock for lock.client_pid(). Additionally,
   *  "cached_lock_for_pid_equal" will be set to true, lock is equal to the lock
   *  active for this pid. */
  void CheckLock(const xtreemfs::pbrpc::Lock& lock,
                 xtreemfs::pbrpc::Lock* conflicting_lock,
                 bool* lock_for_pid_cached,
                 bool* cached_lock_for_pid_equal,
                 bool* conflict_found);

  /** Add a copy of "lock" to list of active locks. */
  void PutLock(const xtreemfs::pbrpc::Lock& lock);

  /** Remove locks equal to "lock" from list of active locks. */
  void DelLock(const xtreemfs::pbrpc::Lock& lock);

  /** Flushes pending file size updates and written data. */
  void Flush(FileHandleImplementation* file_handle);

 private:
  /** Same as Flush(), takes special actions if called by Close(). */
  void Flush(FileHandleImplementation* file_handle, bool close_file);

  /** See WaitForPendingFileSizeUpdates(). */
  void WaitForPendingFileSizeUpdatesHelper(boost::mutex::scoped_lock* lock);

  /** Volume which did open this file. */
  VolumeImplementation* volume_;

  /** XtreemFS File ID of this file (does never change). */
  boost::uint64_t file_id_;

  /** Path of the File, used for debug output and writing back the
   *  OSDWriteResponse to the MetadataCache. */
  std::string path_;

  /** Extracted from the FileHandle's XCap: true if an explicit close() has to
   *  be send to the MRC in order to trigger the on close replication. */
  bool replicate_on_close_;

  /** Number of file handles which hold a pointer on this object. */
  int reference_count_;

  /** Use this to protect reference_count_ and path_. */
  boost::mutex mutex_;

  /** List of corresponding OSDs. */
  xtreemfs::pbrpc::XLocSet xlocset_;

  /** Index of the current replica in the XlocSet. Defaults to 0 and may change
   *  due to failed reads or writes. */
  int current_replica_index_;

  /** Use this to protect xlocset_ and current_replica_index_. */
  boost::mutex xlocset_mutex_;

  /** List of active locks (acts as a cache). */
  std::map<unsigned int, xtreemfs::pbrpc::Lock*> active_locks_;

  /** Use this to protect active_locks_. */
  boost::mutex active_locks_mutex_;

  /** Random UUID of this client to distinguish them while locking. */
  const std::string& client_uuid_;

  /** List of open FileHandles for this file. */
  std::list<FileHandleImplementation*> open_file_handles_;

  /** Use this to protect open_file_handles_. */
  boost::mutex open_file_handles_mutex_;

  /** List of open FileHandles which solely exist to propagate a pending
   *  file size update (a OSDWriteResponse object) to the MRC.
   *
   * This extra list is needed to distinguish between the regular file handles
   * (see open_file_handles_) and the ones used for file size updates.
   * The intersection of both lists is empty.
   */
  std::list<FileHandleImplementation*> pending_filesize_updates_;

  /** Pending file size update after a write() operation, may be NULL.
   *
   * If osd_write_response_ != NULL, the file_size and truncate_epoch of the
   * referenced OSDWriteResponse have to be respected, e.g. when answering
   * a GetAttr request.
   * This osd_write_response_ also corresponds to the "maximum" of all known
   * OSDWriteReponses. The maximum has the highest truncate_epoch, or if equal
   * compared to another response, the higher size_in_bytes value.
   */
  boost::scoped_ptr<xtreemfs::pbrpc::OSDWriteResponse> osd_write_response_;

  /** Denotes the state of the stored osd_write_response_ object. */
  FilesizeUpdateStatus osd_write_response_status_;

  /** XCap required to send an OSDWriteResponse to the MRC. */
  xtreemfs::pbrpc::XCap osd_write_response_xcap_;

  /** Always lock to access osd_write_response_, osd_write_response_status_,
   *  osd_write_response_xcap_ or pending_filesize_updates_. */
  boost::mutex osd_write_response_mutex_;

  /** Used by NotifyFileSizeUpdateCompletition() to notify waiting threads. */
  boost::condition osd_write_response_cond_;

  FRIEND_TEST(VolumeImplementationTestFastPeriodicFileSizeUpdate,
              WorkingPendingFileSizeUpdates);
  FRIEND_TEST(VolumeImplementationTest, FileSizeUpdateAfterFlush);
  FRIEND_TEST(VolumeImplementationTestFastPeriodicFileSizeUpdate,
              FileSizeUpdateAfterFlushWaitsForPendingUpdates);
};

}  // namespace xtreemfs

#endif  // CPP_INCLUDE_LIBXTREEMFS_FILE_INFO_H_