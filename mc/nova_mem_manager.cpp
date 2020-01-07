
//
// Created by Haoyu Huang on 4/8/19.
// Copyright (c) 2019 University of Southern California. All rights reserved.
//

#include <nova/nova_mem_config.h>
#include "nova_mem_manager.h"
#include "nova/nova_common.h"

namespace nova {

    Slab::Slab(char *base) {
        next_ = base;
    }

    void Slab::Init(uint32_t item_size) {
        uint64_t size = NovaConfig::config->log_buf_size;
        item_size_ = item_size;
        auto num_items = static_cast<uint32_t>(size / item_size);
        available_bytes_ = item_size * num_items;
    }

    char *Slab::AllocItem() {
        if (available_bytes_ < item_size_) {
            return nullptr;
        }
        char *buf = next_;
        next_ += item_size_;
        available_bytes_ -= item_size_;
        memset(buf, 0, item_size_);
        return buf;
    }

    char *SlabClass::AllocItem() {
        // check free list first.
        if (free_list.size() > 0) {
            char *ptr = free_list.poll();
            RDMA_ASSERT(ptr != nullptr);
            return ptr;
        }

        if (slabs.size() == 0) {
            return nullptr;
        }

        Slab *slab = slabs.value(slabs.size() - 1);
        return slab->AllocItem();
    }

    void SlabClass::FreeItem(char *buf) {
        free_list.append(buf);
    }

    void SlabClass::AddSlab(Slab *slab) {
        slabs.append(slab);
    }

    NovaMemManager::NovaMemManager(char *buf) {
        // the first two mbs are hash tables.
//    uint64_t seg_size = CUCKOO_SEGMENT_SIZE_MB * 1024 * 1024;
//    uint32_t nsegs = nindexsegments;
//    auto *index_backing_buf = (char *) malloc(seg_size * nsegs);
//    RDMA_ASSERT(index_backing_buf != nullptr);
//    local_cuckoo_table_ = new NovaCuckooHashTable(index_backing_buf, nsegs, 2, 4, 16);

        uint64_t data_size =
                NovaConfig::config->cache_size_gb * 1024 * 1024 * 1024;
        uint64_t index_size = NovaConfig::config->index_size_mb * 1024 * 1024;
        uint64_t location_cache_size =
                NovaConfig::config->lc_size_mb * 1024 * 1024;
        local_ht_ = new ChainedHashTable(buf, index_size, /*enable_eviction=*/
                                         false, /*index_only=*/false,
                                         NovaConfig::config->nindex_entry_per_bucket,
                                         NovaConfig::config->main_bucket_mem_percent);
        if (location_cache_size != 0) {
            char *location_cache_buf = buf + index_size;
            location_cache_ = new ChainedHashTable(location_cache_buf,
                                                   location_cache_size, /*enable_eviction=*/
                                                   true, /*index_only=*/true,
                                                   NovaConfig::config->lc_nindex_entry_per_bucket,
                                                   NovaConfig::config->lc_main_bucket_mem_percent);
        }
        char *data_buf = buf + index_size + location_cache_size;
        uint64_t slab_size = NovaConfig::config->log_buf_size; //SLAB_SIZE_MB * 1024 * 1024;
        uint64_t size = NovaConfig::config->log_buf_size;
        for (int i = 0; i < MAX_NUMBER_OF_SLAB_CLASSES; i++) {
//            if (size % CHUNK_ALIGN_BYTES) {
//                size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);
//            }
            slab_classes_[i].size = size;
            slab_classes_[i].nitems_per_slab = slab_size / size;
            RDMA_LOG(INFO) << "slab class " << i << " size:" << size
                           << " nitems:"
                           << slab_size / size;
            size *= SLAB_SIZE_FACTOR;
        }
        uint64_t ndataslabs = data_size / slab_size;
        RDMA_LOG(INFO) << " nslabs: " << ndataslabs;
        free_slabs_ = (Slab **) malloc(ndataslabs * sizeof(Slab *));
        free_slab_index_ = ndataslabs - 1;
        char *slab_buf = data_buf;
        for (int i = 0; i < ndataslabs; i++) {
            auto *slab = new Slab(slab_buf);
            free_slabs_[i] = slab;
            slab_buf += slab_size;
        }

        pthread_mutex_init(&free_slabs_mutex_, nullptr);
        for (auto &i : slab_class_mutex_) {
            pthread_mutex_init(&i, nullptr);
        }
    }

    uint32_t NovaMemManager::slabclassid(uint32_t size) {
        uint32_t res = 0;
        if (size == 0 || size > NovaConfig::config->log_buf_size)
            return 0;
        while (size > slab_classes_[res].size) {
            res++;
            if (res == MAX_NUMBER_OF_SLAB_CLASSES) {
                /* won't fit in the biggest slab */
                RDMA_LOG(WARNING) << "item larger than 2MB " << size;
                return MAX_NUMBER_OF_SLAB_CLASSES;
            }
        }
        return res;
    }

    char *NovaMemManager::ItemAlloc(uint32_t scid) {
        char *free_item = nullptr;
        Slab *slab = nullptr;

        pthread_mutex_lock(&slab_class_mutex_[scid]);
        free_item = slab_classes_[scid].AllocItem();
        if (free_item != nullptr) {
            pthread_mutex_unlock(&slab_class_mutex_[scid]);
            return free_item;
        }
        pthread_mutex_unlock(&slab_class_mutex_[scid]);

        // Grab a slab from the free list.
        pthread_mutex_lock(&free_slabs_mutex_);
        if (free_slab_index_ == -1) {
            pthread_mutex_unlock(&free_slabs_mutex_);
            return nullptr;
        }
        slab = free_slabs_[free_slab_index_];
        free_slab_index_--;
        pthread_mutex_unlock(&free_slabs_mutex_);

        //
        slab->Init(static_cast<uint32_t>(slab_classes_[scid].size));
        pthread_mutex_lock(&slab_class_mutex_[scid]);
        slab_classes_[scid].AddSlab(slab);
        free_item = slab->AllocItem();
        pthread_mutex_unlock(&slab_class_mutex_[scid]);
        return free_item;
    }

    void NovaMemManager::FreeItem(char *buf, uint32_t scid) {
        memset(buf, 0, slab_classes_[scid].size);
        pthread_mutex_lock(&slab_class_mutex_[scid]);
        slab_classes_[scid].FreeItem(buf);
        pthread_mutex_unlock(&slab_class_mutex_[scid]);
    }

    void NovaMemManager::FreeItems(const std::vector<char *> &items,
                                   uint32_t scid) {
        pthread_mutex_lock(&slab_class_mutex_[scid]);
        for (auto buf : items) {
            slab_classes_[scid].FreeItem(buf);
        }
        pthread_mutex_unlock(&slab_class_mutex_[scid]);
    }

    char *NovaMemManager::ItemEvict(uint32_t scid) {
        if (slab_classes_[scid].nslabs() == 0) {
            return nullptr;
        }

        pthread_mutex_lock(&slab_class_mutex_[scid]);
        uint32_t last_access_time = UINT32_MAX;
        char *lru_buf = nullptr;
        IndexEntry lru_entry{};
        uint64_t lru_hv = 0;

        int candidates = 0;
        uint64_t locked_buckets[MAX_EVICT_CANDIDATES];
        while (candidates < MAX_EVICT_CANDIDATES) {
            locked_buckets[candidates] = 0;
            candidates++;
            uint32_t randV = fastrand();
            uint32_t item_ind = safe_mod(randV,
                                         slab_classes_[scid].nitems_per_slab);
            Slab *slab = slab_classes_[scid].get_slab(
                    safe_mod(randV, slab_classes_[scid].nslabs()));
            char *buf = slab->base + slab_classes_[scid].size * item_ind;
            DataEntry data_entry = DataEntry::chars_to_dataitem(buf);
            if (data_entry.nkey == 0) {
                continue;
            }
            uint64_t hv = NovaConfig::keyhash(data_entry.user_key(),
                                              data_entry.nkey);
            if (!local_ht_->TryLock(hv)) {
                continue;
            }
            locked_buckets[candidates] = hv;
//        GetResult entry = local_ht_->Lookup(hv, key, data_entry.nkey,
//                                             true, /*update_time=*/false);
//        if (entry.hash != 0) {
//            if (entry.time < last_access_time) {
//                last_access_time = entry.time;
//                lru_buf = buf;
//                lru_entry = entry;
//                lru_hv = hv;
//            }
//        }
        }

        if (lru_hv != 0) {
            for (uint32_t i = 0; i < MAX_EVICT_CANDIDATES; i++) {
                if (locked_buckets[i] != 0 && locked_buckets[i] != lru_hv) {
                    local_ht_->UnlockItem(locked_buckets[i]);
                }
            }
            DataEntry lru_data_entry = DataEntry::chars_to_dataitem(
                    (char *) lru_entry.data_ptr);
            char *key = lru_data_entry.user_key();
            local_ht_->Delete(lru_hv, key, lru_data_entry.nkey, true);
            local_ht_->UnlockItem(lru_hv);
        }
        pthread_mutex_unlock(&slab_class_mutex_[scid]);
        return lru_buf;
    }

    GetResult
    NovaMemManager::LocalGet(char *key, uint32_t nkey,
                             bool increment_ref_count) {
        uint64_t hv = NovaConfig::keyhash(key, nkey);
        local_ht_->LockItem(hv);
        GetResult result = local_ht_->Find(hv, key, nkey, true, true,
                                           increment_ref_count, false);
        local_ht_->UnlockItem(hv);
        return result;
    }

    IndexEntry NovaMemManager::RemoteGet(char *key, uint32_t nkey) {
        if (!location_cache_) {
            return {};
        }
        uint64_t hv = NovaConfig::keyhash(key, nkey);
        location_cache_->LockItem(hv);
        GetResult result = location_cache_->Find(hv, key, nkey, false, true,
                                                 true,
                                                 true);
        location_cache_->UnlockItem(hv);
        return result.index_entry;
    }

    PutResult NovaMemManager::RemotePut(const IndexEntry &entry) {
        if (!location_cache_) {
            return {};
        }
        location_cache_->LockItem(entry.hash);
        PutResult put_result = location_cache_->Put(entry);
        location_cache_->UnlockItem(entry.hash);
        return put_result;
    }

    void NovaMemManager::FreeDataEntry(const IndexEntry &index_entry,
                                       const DataEntry &data_entry) {
        uint64_t hv = NovaConfig::keyhash(data_entry.user_key(),
                                          data_entry.nkey);
        local_ht_->LockItem(hv);
        uint32_t refs = data_entry.decrement_ref_count();
        local_ht_->UnlockItem(hv);
        if (refs == 0) {
            FreeItem((char *) (data_entry.data), index_entry.slab_class_id);
            RDMA_LOG(DEBUG) << "Free item in slabclass "
                            << index_entry.slab_class_id
                            << " free-list-size:"
                            << slab_classes_[index_entry.slab_class_id].free_list.size();
        }
    }

    PutResult
    NovaMemManager::LocalPut(char *key, uint32_t nkey, char *val, uint32_t nval,
                             bool acquire_ht_lock, bool delete_old_item) {
        uint64_t hv = NovaConfig::keyhash(key, nkey);
        uint32_t ntotal = DataEntry::sizeof_data_entry(nkey, nval);
        uint32_t scid = slabclassid(ntotal);
        if (scid == MAX_NUMBER_OF_SLAB_CLASSES) {
            return {};
        }
        char *buf = ItemAlloc(scid);
        if (buf == nullptr) {
            RDMA_ASSERT(false) << "Evict not supported";
            buf = ItemEvict(scid);
        }
        if (buf != nullptr) {
            RDMA_LOG(DEBUG) << "Putting entry to slabclass " << scid
                            << " scsize "
                            << slab_classes_[scid].size << " free-list-size:"
                            << slab_classes_[scid].free_list.size() << " size "
                            << ntotal;
            DataEntry::dataitem_to_chars(buf, key, nkey, val, nval, 0);
            uint64_t data_size = DataEntry::sizeof_data_entry(nkey, nval);
            uint32_t refs = 1;

            if (acquire_ht_lock) {
                local_ht_->LockItem(hv);
            }
            PutResult result = local_ht_->Put(buf, scid, hv, key, nkey,
                                              data_size,
                                              true);
            if (result.old_index_entry.type == IndexEntryType::DATA) {
                refs = result.old_data_entry.decrement_ref_count();
            }
            RDMA_ASSERT(result.success);
            if (acquire_ht_lock) {
                local_ht_->UnlockItem(hv);
            }
            if (result.old_index_entry.type == IndexEntryType::DATA) {
                if (refs == 0 || delete_old_item) {
                    auto *data = (char *) result.old_index_entry.data_ptr;
                    FreeItem(data, result.old_index_entry.slab_class_id);
                    RDMA_LOG(DEBUG) << "Free item in slabclass "
                                    << result.old_index_entry.slab_class_id
                                    << " free-list-size:"
                                    << slab_classes_[scid].free_list.size();
                }
            }
            return result;
        }
        RDMA_LOG(WARNING) << "not enough memory";
        return {};
    }

    PutResult
    NovaMemManager::Delete(char *key, uint32_t nkey, bool acquire_ht_lock) {
        uint64_t hv = NovaConfig::keyhash(key, nkey);
        uint32_t ntotal = 0;
        uint32_t scid = 0;
        uint32_t refs = 1;
        if (acquire_ht_lock) {
            local_ht_->LockItem(hv);
        }
        PutResult result = local_ht_->Delete(hv, key, nkey, true);
        if (result.old_index_entry.type == IndexEntryType::DATA) {
            refs = result.old_data_entry.decrement_ref_count();
            ntotal = DataEntry::sizeof_data_entry(nkey,
                                                  result.old_data_entry.nval);
            scid = slabclassid(ntotal);
        }
        RDMA_ASSERT(result.success);
        if (acquire_ht_lock) {
            local_ht_->UnlockItem(hv);
        }
        if (result.old_index_entry.type == IndexEntryType::DATA) {
            if (refs == 0) {
                auto *data = (char *) result.old_index_entry.data_ptr;
                FreeItem(data, result.old_index_entry.slab_class_id);
                RDMA_LOG(DEBUG) << "Free item in slabclass "
                                << result.old_index_entry.slab_class_id
                                << " free-list-size:"
                                << slab_classes_[scid].free_list.size();
            }
        }
    }
}