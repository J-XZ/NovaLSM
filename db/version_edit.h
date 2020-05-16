// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_VERSION_EDIT_H_
#define STORAGE_LEVELDB_DB_VERSION_EDIT_H_

#include <set>
#include <utility>
#include <vector>

#include "db/dbformat.h"

namespace leveldb {

    class VersionSet;

    class VersionEdit {
    public:
        VersionEdit() { Clear(); }

        ~VersionEdit() = default;

        void Clear();

        void SetComparatorName(const Slice &name) {
            has_comparator_ = true;
            comparator_ = name.ToString();
        }

        void SetNextFile(uint64_t num) {
            has_next_file_number_ = true;
            next_file_number_ = num;
        }

        void SetLastSequence(SequenceNumber seq) {
            has_last_sequence_ = true;
            last_sequence_ = seq;
        }

        void SetCompactPointer(int level, const InternalKey &key) {
            compact_pointers_.emplace_back(std::make_pair(level, key));
        }

        void
        UpdateSubRange(uint32_t subrange_id, std::vector<Range> &tiny_ranges,
                       uint32_t num_duplicates) {
            SubRange sr = {};
            sr.decoded_subrange_id = subrange_id;
            sr.tiny_ranges = tiny_ranges;
            sr.num_duplicates = num_duplicates;
            new_subranges_.push_back(sr);
        }

        // Add the specified file at the specified number.
        // REQUIRES: This version has not been saved (see VersionSet::SaveTo)
        // REQUIRES: "smallest" and "largest" are smallest and largest keys in file
        void
        AddFile(int level,
                const std::set<uint32_t> &memtable_ids,
                uint64_t file,
                uint64_t file_size,
                uint64_t converted_file_size,
                uint64_t flush_timestamp,
                const InternalKey &smallest,
                const InternalKey &largest,
                RTableHandle meta_block_handle,
                const std::vector<RTableHandle> &data_block_group_handles) {
            FileMetaData f;
            f.level = level;
            f.memtable_ids = memtable_ids;
            f.number = file;
            f.file_size = file_size;
            f.converted_file_size = converted_file_size;
            f.flush_timestamp = flush_timestamp;
            f.smallest = smallest;
            f.largest = largest;
            f.meta_block_handle = meta_block_handle;
            f.data_block_group_handles = data_block_group_handles;
            new_files_.emplace_back(std::make_pair(level, f));
        }

        // Delete the specified "file" from the specified "level".
        void DeleteFile(int level, uint64_t file) {
            DeletedFileIdentifier f = {};
            f.fnumber = file;
            deleted_files_.emplace_back(std::make_pair(level, f));
        }

        uint32_t EncodeTo(char *dst) const;

        Status DecodeFrom(const Slice &src, Slice *result = nullptr);

        std::string DebugString() const;

    private:
        friend class VersionSet;

        std::string comparator_;
        uint64_t next_file_number_;
        SequenceNumber last_sequence_;
        bool has_comparator_;
        bool has_prev_log_number_;
        bool has_next_file_number_;
        bool has_last_sequence_;

        std::vector<std::pair<int, InternalKey>> compact_pointers_;
        std::vector<std::pair<int, DeletedFileIdentifier>> deleted_files_;
        std::vector<std::pair<int, FileMetaData>> new_files_;

        std::vector<SubRange> new_subranges_;

    };

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_EDIT_H_
