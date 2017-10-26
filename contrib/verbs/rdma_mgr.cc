/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifdef TENSORFLOW_USE_VERBS

#include "tensorflow/contrib/verbs/rdma_mgr.h"
#include <vector>
#include "tensorflow/contrib/verbs/grpc_verbs_client.h"
#include "tensorflow/contrib/verbs/verbs_service.pb.h"
#include "tensorflow/core/distributed_runtime/rpc/grpc_worker_cache.h"
#include "tensorflow/core/distributed_runtime/session_mgr.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {

RdmaMgr::RdmaMgr(const WorkerEnv* const worker_env,
                 GrpcChannelCache* const channel_cache)
    : worker_env_(worker_env), channel_cache_(channel_cache) {
  rdma_adapter_ = new RdmaAdapter(worker_env_);
  // hardcoded to default session (legacy_session_)
  // TODO: use WorkerSessionForSession
  // need to pass in session handle
  local_worker_ = worker_env_->session_mgr->LegacySession()->worker_name;
  std::vector<string> workers;
  worker_env_->session_mgr->LegacySession()->worker_cache->ListWorkers(
      &workers);
  num_remote_workers_ = workers.size() - 1;
  VLOG(2) << "rmda_mgr on local worker: " << local_worker_;
  for (size_t i = 0; i < workers.size(); i++) {
    if (local_worker_.compare(workers[i]) != 0) {
      channel_table_.insert(
          {workers[i],
           new RdmaChannel(rdma_adapter_, local_worker_, workers[i])});
    }
  }
}

// Setup Rdma channels between peers.
// This is done at the beginning of the server setup.

void RdmaMgr::SetupChannels() {
  for (const auto& p : channel_table_) {
    string worker_name = p.first;
    LOG(INFO) << "connecting to remote node " << worker_name;
    RdmaChannel* rc = p.second;
    GetRemoteAddressRequest req;
    GetRemoteAddressResponse resp;
    // get the channel cache
    SharedGrpcChannelPtr client_channel =
        channel_cache_->FindWorkerChannel(worker_name);
    GrpcVerbsClient* client = new GrpcVerbsClient(client_channel);
    CHECK(client != nullptr) << "No worker known as " << worker_name;

    // setting up request
    req.set_host_name(local_worker_);
    Channel* channel_info = req.mutable_channel();
    channel_info->set_lid(rc->self_.lid);
    channel_info->set_qpn(rc->self_.qpn);
    channel_info->set_psn(rc->self_.psn);
    channel_info->set_snp(rc->self_.snp);
    channel_info->set_iid(rc->self_.iid);
    for (int i = 0; i < RdmaChannel::kNumMessageBuffers; i++) {
      MemoryRegion* mr = req.add_mr();
      mr->set_remote_addr(
          reinterpret_cast<uint64_t>(rc->message_buffers_[i]->buffer_));
      mr->set_rkey(rc->message_buffers_[i]->self_->rkey);
    }
    // synchronous call
    Status s = client->GetRemoteAddress(&req, &resp);
    // save obtained remote addresses
    // connect to the remote channel
    if (s.ok()) {
      CHECK(worker_name.compare(resp.host_name()) == 0);
      RdmaAddress ra;
      ra.lid = resp.channel().lid();
      ra.qpn = resp.channel().qpn();
      ra.psn = resp.channel().psn();
      ra.snp = resp.channel().snp();
      ra.iid = resp.channel().iid();
      rc->SetRemoteAddress(ra, false);
      rc->Connect();
      int i = 0;
      int idx[] = {1, 0, 3, 2};
      for (const auto& mr : resp.mr()) {
        // the connections are crossed, i.e.
        // local tx_message_buffer <---> remote rx_message_buffer_
        // local rx_message_buffer <---> remote tx_message_buffer_
        // local tx_ack_buffer <---> remote rx_ack_buffer_
        // local rx_ack_buffer <---> remote tx_ack_buffer_
        // hence idx[] = {1, 0, 3, 2}.
        RdmaBuffer* rb = rc->message_buffers_[idx[i]];
        RemoteMR rmr;
        rmr.remote_addr = mr.remote_addr();
        rmr.rkey = mr.rkey();
        rb->SetRemoteMR(rmr, false);
        i++;
      }
      CHECK(i == RdmaChannel::kNumMessageBuffers);
    } else {
      LOG(ERROR) << s.error_message();
    }
    delete client;
  }
}

#define PING_RECV_WRID 0
#define PING_BUFF_SIZE 1024

int RdmaMgr::PostRecv(RdmaChannel* rc, struct ibv_sge list) {
  struct ibv_recv_wr wr, *bad_wr;
  memset(&wr, 0, sizeof(wr));
  wr.sg_list = &list;
  wr.num_sge = 1;
  wr.wr_id = PING_RECV_WRID;

  return ibv_post_recv(rc->qp_, &wr, &bad_wr);
}

int RdmaMgr::PostSend(RdmaChannel* rc, struct ibv_sge list) {
  struct ibv_send_wr wr, *bad_wr;
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = (uint64_t)rc;
  wr.sg_list = &list;
  wr.num_sge = 1;
  wr.opcode = IBV_WR_SEND;
  wr.send_flags = IBV_SEND_SIGNALED;

  return ibv_post_send(rc->qp_, &wr, &bad_wr);
}

// Check connectivity by pinging every channel
bool RdmaMgr::ConnectivityCheck() {
  int i, rcnt = 0, scnt = 0;
  void* buff;
  struct ibv_sge list;
  buff = malloc(PING_BUFF_SIZE);
  CHECK(buff) << "Malloc failed!";
  struct ibv_mr* mr = ibv_reg_mr(rdma_adapter_->pd_, buff, PING_BUFF_SIZE,
                                 IBV_ACCESS_LOCAL_WRITE);
  CHECK(mr) << "Failed to register memory region";

  memset(&list, 0, sizeof(list));
  list.addr = (uintptr_t)buff;
  list.length = PING_BUFF_SIZE;
  list.lkey = mr->lkey;

  for (const auto& p : channel_table_) {
    string worker_name = p.first;
    RdmaChannel* rc = p.second;

    VLOG(2) << "Ping to " << worker_name;
    CHECK(PostRecv(rc, list) == 0) << "Couldn't post receive from "
                                   << worker_name << " with error "
                                   << std::strerror(errno);
    CHECK(PostSend(rc, list) == 0) << "Couldn't post send  to " << worker_name
                                   << " with error: " << std::strerror(errno);
    for (int i = 0; i < 100; i++) {
      rc->Recv();
    }
  }

  while (rcnt < num_remote_workers_ || scnt < num_remote_workers_) {
    int ne;
    do {
      ne = ibv_poll_cq(rdma_adapter_->cq_, 2 * num_remote_workers_,
                       rdma_adapter_->wc_);
      CHECK(ne >= 0) << "poll CQ failed " << ne << "with error"
                     << std::strerror(errno);
    } while (ne < 1);

    for (i = 0; i < ne; ++i) {
      ibv_wc_status s = rdma_adapter_->wc_[i].status;
      // recv complete
      if ((int)rdma_adapter_->wc_[i].wr_id == PING_RECV_WRID) {
        CHECK(s == IBV_WC_SUCCESS) << ": " << ibv_wc_status_str(
                                                  rdma_adapter_->wc_[i].status)
                                   << "(" << rdma_adapter_->wc_[i].status
                                   << ") for PING_RECV_WRID";
        ++rcnt;
        // send complete
      } else {
        RdmaChannel* rc =
            reinterpret_cast<RdmaChannel*>(rdma_adapter_->wc_[i].wr_id);
        CHECK(s == IBV_WC_SUCCESS) << ": " << ibv_wc_status_str(
                                                  rdma_adapter_->wc_[i].status)
                                   << "(" << rdma_adapter_->wc_[i].status
                                   << ") to " << rc->remote_name_;
        ++scnt;
      }
    }  // for
  }    // while
  CHECK(rcnt == scnt) << "Connectivity check failed!";
  ibv_dereg_mr(mr);
  free(buff);
  rdma_adapter_->StartPolling();
  return (num_remote_workers_ == rcnt) && (num_remote_workers_ == scnt);
}

RdmaMgr::~RdmaMgr() {
  for (const auto& p : channel_table_) delete p.second;
  channel_table_.clear();
  delete rdma_adapter_;
}

// Find a channel via the given name.
// Args:
//   name: peer name, e.g. worker1
// Returns
//   channel object that is connected to the named peer.
RdmaChannel* RdmaMgr::FindChannel(const string& name) {
  ChannelTable::iterator iter = channel_table_.find(name);
  CHECK(iter != channel_table_.end());
  return iter->second;
}

}  // end namespace tensorflow

#endif
