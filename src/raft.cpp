#include <random>
#include <future>
#include <memory>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include "raft.h"
#include "log.h"
#include "parser.h"

raft::ConfigInfo raft::config_file(const std::string & filepath) {
	std::ifstream file(filepath);
	if(!file.is_open()) {
		throw "Error: could not open configuration file!";
	}
	ConfigInfo config;
	std::vector<std::string> vec;
	std::string tmp;
	std::string line;
	while(std::getline(file, line)) {
		if('!' == line[0]) {
			continue;
		}
		std::istringstream iss(line);
		std::string key, value;
		if(!(iss >> key >> value)) {
			throw "Error: invalid line in configuration file!";
		}
		if("self_info" == key) {
			tmp = value;
		}
		vec.emplace_back(value);
	}
	std::sort(vec.begin(), vec.end());
	for(size_t i = 0, size = vec.size(); i < size; ++i) {
		NetInfo node_info;
		size_t colon_pos = vec[i].find(":");
		if(std::string::npos == colon_pos) {
			throw "Error: invalid line in configuration file!";
		}
		node_info.ip = vec[i].substr(0, colon_pos);
		node_info.client_port = std::stoi(vec[i].substr(colon_pos + 1));
		node_info.raft_port = node_info.client_port + 1000;
		if (tmp == vec[i]) {
			config.id = i + 1;
		}
		config.info_map.emplace(i + 1, node_info);
	}
	return config;
}

raft::Server::Server(int id, const std::map<int, NetInfo> & info_map)
	: self_id(id), info_map(info_map)
{
	rpc_server = std::make_unique<rpc::server>(this->info_map[id].raft_port);
	rpc_server->bind("append_entry_rpc", [this](const AppendEntryRequest & entry) {
		return append_entry_rpc(entry); 
	});

	rpc_server->bind("vote_request_rpc", [this](const VoteRequest & vote) {
		return vote_request_rpc(vote);
	});

	state = raft::Follower;
	current_term = 0;
	voted_for = 0;
	// valid index start from 0
	commit_index = -1;
	last_applied_index = -1;
	election_timeout = 2000;  // ms
	request_timeout = 500;    // ms

	output_info_map();
	INFO("initialize success!");
}

void raft::Server::run() {
	INFO("run");
	std::thread([this](){
		rpc_server->run();
	}).detach();
	init_server_socket();
	std::thread([this](){
		recv_client_request();
	}).detach();
	become_follower();
	while(true) {
		std::function<void()> event;
		if (events_queue.wait_pop(event)) {
			event();
		}
	}
}

void raft::Server::become_follower() {
	INFO("become Follower");
	timer.stop();
	state = raft::Follower;
	++follow_circle;
	voted_for = 0;
	int interval = random_interval(election_timeout);
	int circle = follow_circle;
	timer.start(interval, [this, circle](){
		events_queue.push([this, circle](){
			if (raft::Follower == state && circle == follow_circle){
				INFO("election timeout!");
				++current_term;  // Increment currentTerm
				election_circle = 0;
				become_candidate();
			}
		});
	});
}

void raft::Server::become_candidate() {
	INFO("become Candidate");
	timer.stop();
	state = raft::Candidate;  
	++election_circle;             
	voted_for = self_id;   // Vote for self
	vote_num = 1;
	for(auto it = info_map.begin(); it != info_map.end(); ++it) {
		if(it->first == self_id) {
			continue;
		}
		pool.submit([this, it](){
			VoteRequest vote{current_term, self_id, (int)logs.size() - 1, logs.empty() ? 0 : logs.back().term};
			VoteResponse result;
			int clock = timer.clock();
			INFO("try to send vote request RPC to node ", it->first);
			try {
				rpc::client client(it->second.ip, it->second.raft_port);
				result = client.call("vote_request_rpc", vote).as<VoteResponse>();
			} catch(...) {
				ERROR("bad vote request RPC to node ", it->first);
				return;
			}
			INFO("recieve vote response from node ", it->first);
			events_queue.push([this, result, clock](){
				collect_votes(result, clock);
			});
		});
	}
	int interval = random_interval(request_timeout);
	int circle = election_circle;
	timer.start(interval, [this, circle](){
		// std::cout << "election request timeout!" << std::endl;
		events_queue.push([this, circle](){
			if(raft::Candidate == state && circle == election_circle) {
				INFO("election request timeout!");
				become_candidate();
			}else if (raft::Candidate != state) {
				std::cout << "raft::Candidate != state\n";
			}else if(circle != election_circle) {
				std::cout << "circle: " << circle << " election_circle: " << std::endl;
			}
		});
	});
}

void raft::Server::become_leader() {
	INFO("become Leader");
	timer.stop();
	state = raft::Leader;
	leader_id = self_id;
	++append_entry_circle;
	success_num = 1;
	for (auto it = info_map.begin(); it != info_map.end(); ++it) {
		if(it->first == self_id) {
			continue;
		}
		// AppendEntryRequest req{self_id, current_term, -1, -1, -1};
		AppendEntryRequest req;
		req.leader_id = self_id;
		req.term = current_term;
		req.prev_log_index = next_commit_index[it->first] - 1;
		req.prev_log_term = req.prev_log_index >= 0 && req.prev_log_index < (int)logs.size() ? logs[req.prev_log_index].term : 0;
		req.leader_commit = commit_index;
		if (logs.size() > next_commit_index[it->first]) {
			req.append_logs.push_back(logs[next_commit_index[it->first]]);
		}
		pool.submit([this, it, req](){
			AppendEntryResponse result;
			int clock = timer.clock();
			INFO("try to send append entry RPC to node ", it->first, ", next index is ", next_commit_index[it->first]);
			try {
				rpc::client client(it->second.ip, it->second.raft_port);
				result = client.call("append_entry_rpc", req).as<AppendEntryResponse>();
			} catch(...) {
				ERROR("bad append entry RPC to node ", it->first);
				return;
			}
			events_queue.push([this, result, clock](){
				recv_append_entry_response(result, clock);
			});
		});
	}
	int interval = random_interval(request_timeout);
	int circle = append_entry_circle;
	timer.start(interval, [this, circle](){
		events_queue.push([this, circle](){
			if (raft::Leader == state && circle == append_entry_circle) {
				INFO("append entry request timeout!");
				become_leader();
			}
		});
	});
}

void raft::Server::check_term(int term) {
	if (term > current_term) {
		current_term = term;
		if (raft::Follower != state) {
			follow_circle = 0;
			become_follower();
		}
	}
}

raft::VoteResponse raft::Server::vote_request_rpc(const raft::VoteRequest & vote) {
	auto task = std::make_shared<std::packaged_task<raft::VoteResponse()>>(
		std::bind(&raft::Server::handle_request_vote_rpc, this, vote));
	std::future<raft::VoteResponse> fut = task->get_future();
	events_queue.push([task]() {
		(*task)(); 
	});
	return fut.get();
}

raft::VoteResponse raft::Server::handle_request_vote_rpc(const raft::VoteRequest & vote) {
	INFO("recieve vote request RPC from node ", vote.candidate_id);
	if(raft::Follower == state) timer.stop();  // Follower restarts election timeout
	raft::VoteResponse resp{current_term, false};
	// judge wether vote for candidate
	if (vote.term >= current_term) {  // $5.1 reply false if term < currentTerm
		if (0 == voted_for || vote.candidate_id == voted_for) {  // $5.2 votedFor is null or candidateId, for FIFO
			int last_log_term = logs.empty() ? 0 : logs.back().term;
			if (vote.last_log_term > last_log_term ||  // $5.4 candidate's log is at least as up-to-date as receiver's log, grant vote
				(vote.last_log_term == last_log_term && vote.last_log_index >= (int)(logs.size()) - 1))
			{
				resp.vote_granted = true;
			}
		}
	}
	if(raft::Follower == state) become_follower();   // start timer, and keep follwer state
	check_term(vote.term);    // $5.1 if RPC request or response contains term T > currentTerm: set currentTerm = T, convert to follower 
	return resp;
}

raft::AppendEntryResponse raft::Server::append_entry_rpc(const raft::AppendEntryRequest & entry) {
	auto task = std::make_shared<std::packaged_task<raft::AppendEntryResponse()>>(
		std::bind(&raft::Server::handle_append_entry_rpc, this, entry));
	std::future<raft::AppendEntryResponse> fut = task->get_future();
	events_queue.push([task](){
		(*task)();
	});
	return fut.get();
}

raft::AppendEntryResponse raft::Server::handle_append_entry_rpc(const raft::AppendEntryRequest & entry) {
	INFO("recieve append entry RPC from node ", entry.leader_id);
	raft::AppendEntryResponse resp{self_id, current_term, false, commit_index, (int)logs.size()};
	if (entry.term < current_term) {  // $5.1 reply false if term < currentTerm
		return resp;
	}
	if(raft::Follower == state) timer.stop();  // Follower restarts election timeout 
	leader_id = entry.leader_id;
	if (!entry.append_logs.empty()) {
		if (commit_index >= entry.append_logs.back().index) {  // do not update logs if appdending logs has been commited
			resp.success = true;
		}
		else if (check_prev_log_index(entry.prev_log_index, entry.prev_log_term)) {
			resp.success = true;
			std::cout << "update_append_log" << std::endl;
			update_append_log(entry.append_logs);
			std::cout << "entry.leader_commit: " << entry.leader_commit << std::endl;
			std::cout << "entry.prev_log_index: " << entry.prev_log_index << std::endl;
			
		}
	}
	if (entry.leader_commit > commit_index) {  // if leaderCommit > commitIndex, set commitIndex = min(leaderCommit, index of last new entry)
		int index = std::min(entry.leader_commit, (int)logs.size());
		commit_log(index);
	}
	resp.commit_index = commit_index;
	resp.next_index = (int)logs.size();
	if(raft::Follower == state) become_follower();  // start timer, and keep follwer state
	check_term(entry.term);    // $5.1 if RPC request or response contains term T > currentTerm: set currentTerm = T, convert to follower
	return resp;
}

bool raft::Server::check_prev_log_index(int prev_log_index, int prev_log_term) {
	if(prev_log_index < 0) {
		return true;
	}
	if ((int)logs.size() - 1 >= prev_log_index) {
		if (prev_log_term == logs[prev_log_index].term) {
			return true;
		}
	}
	return false;
}

void raft::Server::update_append_log(const std::vector<Log> & append_logs) {
	int new_index = append_logs[0].index;
	logs.erase(logs.begin() + new_index, logs.end());
	logs.insert(logs.end(), append_logs.begin(), append_logs.end());
}

void raft::Server::collect_votes(VoteResponse resp, int clock) {
	check_term(resp.term);
	if(clock < timer.clock()) {
		return;
	}
	if (raft::Candidate == state && resp.vote_granted) {
		if(++vote_num > info_map.size() / 2) {
			append_entry_circle = 0;
			for (auto & next_index : next_commit_index) {
				next_index.second = commit_index + 1;
			}
			commit_res.clear();
			leader_start_index = logs.size();
			become_leader();
		}
	}
}

void raft::Server::recv_append_entry_response(raft::AppendEntryResponse resp, int clock) {
	check_term(resp.term);
	if(state != raft::Leader || clock < timer.clock()) {
		return;
	}
	INFO("recieve append entry response from node ", resp.id, ", index is ", resp.commit_index);
	if (resp.success) {
		if (commit_index + 1 == next_commit_index[resp.id]) {
			if (++success_num > info_map.size() / 2) {
				commit_log(commit_index + 1);
			}
		}
		++next_commit_index[resp.id];
	} else {
		next_commit_index[resp.id] = resp.next_index;
	}
}

int raft::Server::random_interval(int average) {
	std::random_device rd;
	std::default_random_engine eng(rd());
	std::uniform_int_distribution<int> dist(average / 2, average * 3 / 2);
	int interval = dist(eng);
	return interval ;
}

void raft::Server::output_info_map() {
	std::cout << "node " << self_id << std::endl;
	for(auto & [id, net_info] : info_map) {
		std::cout << "node " << id << " ip: " << net_info.ip
		<< " client port: " << net_info.client_port 
		<< " raft port: " << net_info.raft_port << std::endl;
	}
}

void raft::Server::init_server_socket() {
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(info_map[self_id].ip.c_str());         
	servaddr.sin_port = htons(info_map[self_id].client_port);
	int opt = 1;
	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		throw std::runtime_error("socket() error");
	}
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		throw std::runtime_error("setsockopt() error");
	}
	while(bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		sleep(1000);
	}
	if(listen(socket_fd, 128) != 0) {
		throw std::runtime_error("listen() error");
	}
}

void raft::Server::recv_client_request() {
	int client_sockfd;	
	while(true) {
		struct sockaddr_in cliaddr;
		socklen_t cliaddr_len = sizeof(cliaddr);
		if ((client_sockfd = accept(socket_fd, (struct sockaddr *)&cliaddr, &cliaddr_len)) < 0) {
			ERROR("accept error");
			break;
		}
		char * buffer = new char[1024];
		memset(buffer, 0, 1024 * sizeof(char));
		recv(client_sockfd, buffer, 1024, 0);
		std::string recvmsg(buffer);
		delete [] buffer;
		std::string resp = resp_client_request(recvmsg);
		send(client_sockfd, resp.c_str(), resp.size(), 0);
		close(client_sockfd);
	}
}

std::string raft::Server::resp_client_request(const std::string & msg) {
	std::unique_ptr<std::promise<std::string>> prom(new std::promise<std::string>());
	std::future<std::string> fut = prom->get_future();
	events_queue.push([this, &msg, &prom](){
		handle_client_request(msg, prom);
	});
	return fut.get();
}

void raft::Server::handle_client_request(const std::string & msg, std::unique_ptr<std::promise<std::string>> & prom) {
	INFO("recieve client request ", msg);
	if (raft::Leader != state) {
		std::string leader_info = info_map[leader_id].ip + ":" + std::to_string(info_map[leader_id].client_port);
		prom->set_value(leader_info);
	}
	else {
		raft::Log log{(int)logs.size(), current_term, msg};
		logs.emplace_back(log);
		commit_res.emplace_back(std::move(prom));
	}
}

void raft::Server::commit_log(int index) {
	if (index > commit_index && index < logs.size()) {
		INFO("commit [", commit_index + 1, ", ", index, "]");
		for (int i = commit_index + 1; i <= index; ++i) {
			std::string result = kvdb.commit(logs[i].content);
			std::cout << "commit success" << std::endl;
			if (raft::Leader == state && i >= leader_start_index) {
				commit_res[i - leader_start_index]->set_value(result);
			}
		}
		commit_index = index;
	}
}