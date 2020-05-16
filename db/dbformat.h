// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "leveldb/comparator.h"
#include "leveldb/db_types.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

// Grouping of constants.  We may want to make some of these
// parameters set via options.
    namespace config {
        static const int kNumLevels = 2;

// Level-0 compaction is started when we hit this many files.
        static const int kL0_CompactionTrigger = 4;

// Soft limit on number of level-0 files.  We slow down writes at this point.
        static const int kL0_SlowdownWritesTrigger = 8;

// Maximum number of level-0 files.  We stop writes at this point.
        static const int kL0_StopWritesTrigger = 12;

// Maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  We try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  We do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
        static const int kMaxMemCompactLevel = 0;

// Approximate gap in bytes between samples of data read during iteration.
        static const int kReadBytesPeriod = 1048576;

    }  // namespace config

    static size_t TargetFileSize(const Options *options) {
        return options->max_file_size;
    }

// Maximum bytes of overlaps in grandparent (i.e., level+2) before we
// stop building a single file in a level->level+1 compaction.
    static int64_t MaxGrandParentOverlapBytes(const Options *options) {
        return 10 * TargetFileSize(options);
    }

// Maximum number of bytes in all compacted files.  We avoid expanding
// the lower level file set of a compaction if it would make the
// total compaction cover more than this many bytes.
    static int64_t ExpandedCompactionByteSizeLimit(const Options *options) {
        return 25 * TargetFileSize(options);
    }

    static double MaxBytesForLevel(const Options &options, int level) {
        // Note: the result for level zero is not really used since we set
        // the level-0 compaction threshold based on number of files.

        // Result for both level-0 and level-1
        double result = 10. * 1048576.0; // 10 MB.
        while (level > 1) {
            result *= 10;
            level--;
        }
        return result;
    }

    static uint64_t MaxFileSizeForLevel(const Options *options, int level) {
        // We could vary per level to reduce number of files?
        return TargetFileSize(options);
    }

    static int64_t TotalFileSize(const std::vector<FileMetaData *> &files) {
        int64_t sum = 0;
        for (size_t i = 0; i < files.size(); i++) {
            sum += files[i]->file_size;
        }
        return sum;
    }

    class InternalKey;

// kValueTypeForSeek defines the ValueType that should be passed when
// constructing a ParsedInternalKey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// ValueType, not the lowest).
    static const ValueType kValueTypeForSeek = kTypeValue;

// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
    static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

// Return the length of the encoding of "key".
    inline size_t InternalKeyEncodingLength(const ParsedInternalKey &key) {
        return key.user_key.size() + 8;
    }

// Append the serialization of "key" to *result.
    void AppendInternalKey(std::string *result, const ParsedInternalKey &key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
    bool ParseInternalKey(const Slice &internal_key, ParsedInternalKey *result);

// Returns the user key portion of an internal key.
    inline Slice ExtractUserKey(const Slice &internal_key) {
        assert(internal_key.size() >= 8);
        return Slice(internal_key.data(), internal_key.size() - 8);
    }

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
    class InternalKeyComparator : public Comparator {
    private:
        const Comparator *user_comparator_;

    public:
        explicit InternalKeyComparator(const Comparator *c) : user_comparator_(
                c) {}

        const char *Name() const override;

        int Compare(const Slice &a, const Slice &b) const override;

        void FindShortestSeparator(std::string *start,
                                   const Slice &limit) const override;

        void FindShortSuccessor(std::string *key) const override;

        const Comparator *user_comparator() const { return user_comparator_; }

        int Compare(const InternalKey &a, const InternalKey &b) const;
    };

// Filter policy wrapper that converts from internal keys to user keys
    class InternalFilterPolicy : public FilterPolicy {
    private:
        const FilterPolicy *const user_policy_;

    public:
        explicit InternalFilterPolicy(const FilterPolicy *p) : user_policy_(
                p) {}

        const char *Name() const override;

        void
        CreateFilter(const Slice *keys, int n, std::string *dst) const override;

        bool KeyMayMatch(const Slice &key, const Slice &filter) const override;
    };

    inline int InternalKeyComparator::Compare(const InternalKey &a,
                                              const InternalKey &b) const {
        return Compare(a.Encode(), b.Encode());
    }

    inline bool ParseInternalKey(const Slice &internal_key,
                                 ParsedInternalKey *result) {
        const size_t n = internal_key.size();
        if (n < 8) return false;
        uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
        uint8_t c = num & 0xff;
        result->sequence = num >> 8;
        result->type = static_cast<ValueType>(c);
        result->user_key = Slice(internal_key.data(), n - 8);
        return (c <= static_cast<uint8_t>(kTypeValue));
    }

// A helper class useful for DBImpl::Get()
    class LookupKey {
    public:
        // Initialize *this for looking up user_key at a snapshot with
        // the specified sequence number.
        LookupKey(const Slice &user_key, SequenceNumber sequence);

        LookupKey(const LookupKey &) = delete;

        LookupKey &operator=(const LookupKey &) = delete;

        ~LookupKey();

        // Return a key suitable for lookup in a MemTable.
        Slice memtable_key() const { return Slice(start_, end_ - start_); }

        // Return an internal key (suitable for passing to an internal iterator)
        Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

        // Return the user key
        Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

    private:
        // We construct a char array of the form:
        //    klength  varint32               <-- start_
        //    userkey  char[klength]          <-- kstart_
        //    tag      uint64
        //                                    <-- end_
        // The array is a suitable MemTable key.
        // The suffix starting with "userkey" can be used as an InternalKey.
        const char *start_;
        const char *kstart_;
        const char *end_;
        char space_[200];  // Avoid allocation for short keys
    };

    inline LookupKey::~LookupKey() {
        if (start_ != space_) delete[] start_;
    }

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DBFORMAT_H_
