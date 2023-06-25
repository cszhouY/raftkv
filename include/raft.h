#ifndef _RAFT_H_
#define _RAFT_H_

#include <vector>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#include "timer.h"
#include "ThreadPool.h"
#include "SafeQueue.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "KVDB.h"

namespace raft
{
	struct NetInfo {
		std::string ip;
		int client_port;
		int raft_port;
	};

	struct ConfigInfo {
		int id;
		std::map<int, NetInfo> info_map;
	};

	ConfigInfo config_file(const std::string & filepath); 

	struct ClientMsg {
		int client_sockfd;
		std::string content;
	};

	struct Log {
		int index;
		int term;
		std::string content;
		MSGPACK_DEFINE(index, term, content);
	};

	struct VoteRequest {
		int term;
		int candidate_id;
		int last_log_index;
		int last_log_term;
		MSGPACK_DEFINE(term, candidate_id, last_log_index, last_log_term);
	};

	struct VoteResponse {
		int term;
		bool vote_granted;
		MSGPACK_DEFINE(term, vote_granted);
	};

	struct AppendEntryRequest {
		int leader_id;
		int term;
		int prev_log_index;
		int prev_log_term;
		int leader_commit;
		std::vector<Log> append_logs;
		MSGPACK_DEFINE(leader_id, term, prev_log_index, prev_log_term, leader_commit, append_logs);
	};

	struct AppendEntryResponse {
		int id;
		int term;
		bool success;
		int commit_index;
		int next_index;
		MSGPACK_DEFINE(id, term, success, commit_index, next_index);
	};

	enum State {
		None = 0,
		Leader = 1,
		Candidate = 2,
		Follower  =3
	};

	class Server {
	public:
		Server() = delete;
		explicit Server(int self_id, const std::map<int, NetInfo> & info_map);
		void run();

	private:
		// basic info
		int self_id;
		std::map<int, NetInfo> info_map;
		int socket_fd;

		// raft module
		Timer timer;
		int election_timeout;     // the average election timeout, the random value will be in [election_time/2, election_time*3/2] actually.
		int request_timeout;      // the average request timeout, similar to election_timeout 
		ThreadPool pool;    // the leader and candidate should communicate with other nodes concurrently
		SafeQueue<std::function<void()>> events_queue;
		std::unique_ptr<rpc::server> rpc_server;   // accept requests by RPC

		// persisitent raft info
		State state;
		int current_term;
		int voted_for;
		std::vector<Log> logs;

		// temporary raft info
		int leader_id;
		int follow_circle;

		int election_circle;
		int vote_num;

		int append_entry_circle;
		int success_num;
		
		int commit_index;
		int last_applied_index;

		// raft info just for leader
		std::map<int, int> next_commit_index;
		std::map<int, int> match_index;

		int leader_start_index;
		std::vector<std::unique_ptr<std::promise<std::string>>> commit_res;

		// kv store data
		db::KVDB kvdb;

		VoteResponse vote_request_rpc(const VoteRequest & vote);

		VoteResponse handle_request_vote_rpc(const VoteRequest & vote);

		AppendEntryResponse append_entry_rpc(const AppendEntryRequest & entry);

		AppendEntryResponse handle_append_entry_rpc(const AppendEntryRequest & entry);

		void become_follower();

		void become_candidate();

		void collect_votes(VoteResponse resp, int clock);

		void become_leader();

		void recv_append_entry_response(AppendEntryResponse resp, int clock);

		void check_term(int term);

		bool check_prev_log_index(int prev_log_index, int prev_log_term);

		void update_append_log(const std::vector<Log> & append_logs); 

		int random_interval(int average);

		void output_info_map();

		void init_server_socket();

		void recv_client_request();

		std::string resp_client_request(const std::string & msg);

		void handle_client_request(const std::string & msg, std::unique_ptr<std::promise<std::string>> & prom);
	
		void commit_log(int index);
	};
} // namespce raft

#endif