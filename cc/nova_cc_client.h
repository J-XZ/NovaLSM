
//
// Created by Haoyu Huang on 1/8/20.
// Copyright (c) 2020 University of Southern California. All rights reserved.
//

#ifndef LEVELDB_NOVA_CC_CLIENT_H
#define LEVELDB_NOVA_CC_CLIENT_H

#include "util/env_mem.h"
#include "db/version_edit.h"
#include "include/leveldb/db_types.h"
#include "include/leveldb/cc_client.h"
#include "nova/nova_common.h"
#include "dc/nova_dc.h"
#include "nova/nova_rdma_rc_store.h"
#include "cc/nova_cc_log_writer.h"

#include "log/nova_log.h"
#include "nova_rtable.h"

namespace leveldb {

    class NovaCCClient : public CCClient {
    public:
        NovaCCClient(uint32_t dc_client_id, nova::NovaRDMAStore *rdma_store,
                     nova::NovaMemManager *mem_manager,
                     NovaRTableManager *rtable_manager,
                     leveldb::log::RDMALogWriter *rdma_log_writer,
                     uint32_t lower_req_id, uint32_t upper_req_id,
                     CCServer *cc_server)
                : cc_client_id_(dc_client_id), rdma_store_(rdma_store),
                  mem_manager_(mem_manager), rtable_manager_(rtable_manager),
                  rdma_log_writer_(rdma_log_writer),
                  lower_req_id_(lower_req_id), upper_req_id_(upper_req_id),
                  current_req_id_(lower_req_id), cc_server_(cc_server) {
        }

        uint32_t
        InitiateDeleteTables(uint32_t server_id,
                             const std::vector<SSTableRTablePair> &rtable_ids) override;

        uint32_t
        InitiateRTableReadDataBlock(const RTableHandle &rtable_handle,
                                    uint64_t offset, uint32_t size,
                                    char *result) override;

        uint32_t
        InitiateRTableWriteDataBlocks(uint32_t server_id, uint32_t thread_id,
                                      uint32_t *rtable_id, char *buf,
                                      const std::string &dbname,
                                      uint64_t file_number,
                                      uint32_t size) override;

        uint32_t
        InitiatePersist(uint32_t server_id,
                        const std::vector<SSTableRTablePair> &rtable_ids) override;

        uint32_t
        InitiateReplicateLogRecords(const std::string &log_file_name,
                                    uint64_t thread_id,
                                    const Slice &slice) override;


        uint32_t
        InitiateCloseLogFile(const std::string &log_file_name) override;

        bool OnRecv(ibv_wc_opcode type, uint64_t wr_id,
                    int remote_server_id, char *buf,
                    uint32_t imm_data) override;

        bool IsDone(uint32_t req_id, CCResponse *response) override;

        uint32_t GetCurrentReqId();

        void IncrementReqId();

    private:
//        uint32_t
//        InitiateReadBlocks(const std::string &dbname, uint64_t file_number,
//                           const FileMetaData &meta,
//                           const std::vector<CCBlockHandle> &block_handls,
//                           char *result);
//
//        uint32_t
//        InitiateReadBlock(const std::string &dbname, uint64_t file_number,
//                          const FileMetaData &meta,
//                          const CCBlockHandle &block_handle,
//                          char *result);
//
//        // Read the SSTable and return the total size.
//        uint32_t
//        InitiateReadSSTable(const std::string &dbname, uint64_t file_number,
//                            const FileMetaData &meta, char *result);
//
//        uint32_t InitiateFlushSSTable(const std::string &dbname,
//                                      uint64_t file_number,
//                                      const FileMetaData &meta,
//                                      char *backing_mem);
//
//        uint32_t
//        InitiateAllocateSSTableBuffer(uint32_t remote_server_id,
//                                      const std::string &dbname,
//                                      uint64_t file_number,
//                                      uint64_t file_size);
//
//        uint32_t
//        InitiateWRITESSTableBuffer(uint32_t remote_server_id, char *src,
//                                   uint64_t dest,
//                                   uint64_t file_size);
//
//        uint32_t InitiateReleaseSSTableBuffer(uint32_t remote_server_id,
//                                              const std::string &dbname,
//                                              uint64_t file_number,
//                                              uint64_t file_size);
//
//        uint32_t
//        InitiateDeleteFiles(const std::string &dbname,
//                            const std::vector<FileMetaData> &filenames);

//        uint32_t HomeCCNode(const FileMetaData &meta);

        uint32_t cc_client_id_ = 0;
        nova::NovaRDMAStore *rdma_store_;
        nova::NovaMemManager *mem_manager_;
        NovaRTableManager *rtable_manager_;
        CCServer *cc_server_;
        leveldb::log::RDMALogWriter *rdma_log_writer_ = nullptr;

        leveldb::SSTableManager *sstable_manager_;


        uint32_t current_req_id_ = 1;
        uint32_t lower_req_id_;
        uint32_t upper_req_id_;
        std::map<uint32_t, CCRequestContext> request_context_;
        struct ServerWRRequest {
            int remote_server_id;
            uint64_t wr_id;
            uint32_t req_id;
        };
        std::list<ServerWRRequest> requests_;

    };
}


#endif //LEVELDB_NOVA_CC_CLIENT_H