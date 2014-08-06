
#include "peer_communication.h"
#include "pk_mgr.h"
#include "network.h"
#include "network_udp.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "io_accept.h"
#include "io_connect.h"
#include "io_connect_udp.h"
#include "logger_client.h"
#include "io_nonblocking.h"
#include "io_nonblocking_udp.h"

#include "udt_lib/udt.h"

using namespace UDT;
using namespace std;

peer_communication::peer_communication(network *net_ptr,network_udp *net_udp_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, logger_client * logger_client_ptr){
	_net_ptr = net_ptr;
	_net_udp_ptr = net_udp_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_logger_client_ptr = logger_client_ptr;
	total_manifest = 0;
	session_id_count = 1;
	self_info =NULL;
	self_info = new struct level_info_t;
	if(!(self_info) ){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::self_info  new error", __FUNCTION__, __LINE__);
	}
	_io_accept_ptr =NULL;
	_io_connect_ptr =NULL;
	_io_connect_udp_ptr =NULL;
	_io_nonblocking_ptr=NULL;
	_io_nonblocking_udp_ptr=NULL;
	_io_nonblocking_ptr = new io_nonblocking(_net_ptr,log_ptr ,this,logger_client_ptr);
	_io_nonblocking_udp_ptr = new io_nonblocking_udp(_net_udp_ptr,log_ptr ,this,logger_client_ptr);
	_io_accept_ptr = new io_accept(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_ptr = new io_connect(net_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	_io_connect_udp_ptr = new io_connect_udp(net_udp_ptr,log_ptr,prep_ptr,peer_mgr_ptr,peer_ptr,pk_mgr_ptr,this,logger_client_ptr);
	if(!(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr) || !(_io_connect_udp_ptr)){
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] !(_io_nonblocking_ptr) || !(_io_accept_ptr) || !(_io_connect_ptr) || !(_io_connect_ptr)  new error", __FUNCTION__, __LINE__);
	}
	fd_list_ptr =NULL;
	fd_list_ptr = pk_mgr_ptr->fd_list_ptr ;
	//peer_com_log = fopen("./peer_com_log.txt","wb");
}

peer_communication::~peer_communication(){

	if(self_info)
		delete self_info;

	if(_io_accept_ptr)
		delete _io_accept_ptr;
	if(_io_connect_ptr)
		delete _io_connect_ptr;
	if(_io_connect_udp_ptr)
		delete _io_connect_udp_ptr;
	if(_io_nonblocking_ptr)
		delete _io_nonblocking_ptr;
	_io_accept_ptr =NULL;
	_io_connect_ptr =NULL;
	_io_nonblocking_ptr=NULL;

	for(session_id_candidates_set_iter = session_id_candidates_set.begin() ;session_id_candidates_set_iter !=session_id_candidates_set.end();session_id_candidates_set_iter++){
		
		for(int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
	}
	session_id_candidates_set.clear();


	for(map_fd_info_iter=map_fd_info.begin() ; map_fd_info_iter!=map_fd_info.end();map_fd_info_iter++){
		delete map_fd_info_iter->second;
	}
	map_fd_info.clear();
	for (map_udpfd_info_iter = map_udpfd_info.begin(); map_udpfd_info_iter != map_udpfd_info.end(); map_udpfd_info_iter++) {
		delete map_udpfd_info_iter->second;
	}
	map_udpfd_info.clear();


	for(map_fd_NonBlockIO_iter=map_fd_NonBlockIO.begin() ; map_fd_NonBlockIO_iter!=map_fd_NonBlockIO.end();map_fd_NonBlockIO_iter++){
		delete map_fd_NonBlockIO_iter->second;
	}
	map_fd_NonBlockIO.clear();
	
	for (map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.begin(); map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end(); map_udpfd_NonBlockIO_iter++) {
		delete map_udpfd_NonBlockIO_iter->second;
	}
	map_udpfd_NonBlockIO.clear();

	printf("==============deldet peer_communication success==========\n");
}

void peer_communication::set_self_info(unsigned long public_ip){
	self_info->public_ip = public_ip;
	//self_info->private_ip = _net_ptr->getLocalIpv4();
	self_info->private_ip = _pk_mgr_ptr->my_private_ip;
}

//flag 0 rescue peer(caller is child), flag 1 candidate's peer(caller is parent)
void peer_communication::set_candidates_handler(unsigned long rescue_manifest, struct chunk_level_msg_t *testing_info, unsigned int candidates_num, int caller, int session_id)
{	
	
	debug_printf("size:%d  \n", _net_udp_ptr->_map_fd_bc_tbl.size());
	for (map<int, basic_class *>::iterator iter = _net_udp_ptr->_map_fd_bc_tbl.begin(); iter != _net_udp_ptr->_map_fd_bc_tbl.end(); iter++) {
		debug_printf("size:%d  _map_fd_bc_tbl[%d] \n", _net_udp_ptr->_map_fd_bc_tbl.size(), iter->first);
	}
	
	if (candidates_num < 1) {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Candidates_num cannot less than 1", __FUNCTION__, __LINE__);
	}

	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
															"session_id =", session_id,
															"manifest =", rescue_manifest,
															"caller =", caller, 
															"candidates_num =", candidates_num);
	for (unsigned int i = 0; i < candidates_num; i++) {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid", testing_info->level_info[i]->pid);
	}
	
	// caller is the child of the candidate-peer, 0
	if (caller == CHILD_PEER) {
		/*
		if ((total_manifest & rescue_manifest) > 0) {
			debug_printf("[DEBUG] Received manifest which is in progress. manifest = %d total_manifest = %d \n", rescue_manifest, total_manifest);
			_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__,
															"[DEBUG] Received manifest which is in progress. manifest =", rescue_manifest,
															"total_manifest =", total_manifest);
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s u s u \n", "[DEBUG] Received manifest which is in progress. manifest =", total_manifest,
																					"total_manifest =", total_manifest);
			debug_printf("111 \n");
			//PAUSE
			//_logger_client_ptr->log_exit();
		}
		
		debug_printf("112 \n");
		//else {
		//debug_printf2("rescue manifest: %d already rescue manifest: %d \n", rescue_manifest, total_manifest);
		//_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "rescue-peer calls peer_communication");
		_log_ptr->write_log_format("s(u) s u s u \n", __FUNCTION__, __LINE__, "rescue manifest =", rescue_manifest, "already rescue manifest =", total_manifest);
		debug_printf("113 \n");
		total_manifest = total_manifest | rescue_manifest;	//total_manifest has to be erased in stop_attempt_connect
		debug_printf("114 \n");
		*/
		session_id_candidates_set_iter = session_id_candidates_set.find(session_id);	//manifest_candidates_set has to be erased in stop_attempt_connect
		if (session_id_candidates_set_iter != session_id_candidates_set.end()) {
			debug_printf("[ERROR] session id already in the record in set_candidates_test \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] session id already in the record in set_candidates_test");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] session id already in the record in set_candidates_test \n");
			_logger_client_ptr->log_exit();
		}
		else {
			session_id_candidates_set[session_id] = new struct peer_com_info;
			if (!session_id_candidates_set[session_id]) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id] new failed", __FUNCTION__, __LINE__);
			}
			/*	redundant code
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if (session_id_candidates_set_iter == session_id_candidates_set.end()) {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : session id cannot find in the record in set_candidates_test\n");
				_logger_client_ptr->log_exit();
			}
			*/
			int level_msg_size;
			int offset = 0;
			level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

			session_id_candidates_set[session_id]->peer_num = candidates_num;
			session_id_candidates_set[session_id]->manifest = rescue_manifest;
			session_id_candidates_set[session_id]->role = caller;
			session_id_candidates_set[session_id]->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
			if (!session_id_candidates_set[session_id]->list_info) {
				_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info new failed", __FUNCTION__, __LINE__);
			}
			memset(session_id_candidates_set[session_id]->list_info, 0, level_msg_size);
			memcpy(session_id_candidates_set[session_id]->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

			offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

			for (unsigned int i = 0; i < candidates_num; i++) {
				debug_printf("candidates_num: %d, i: %d \n", candidates_num, i);
				session_id_candidates_set[session_id]->list_info->level_info[i] = new struct level_info_t;
				if (!session_id_candidates_set[session_id]->list_info->level_info[i]) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info->level_info[i] new failed", __FUNCTION__, __LINE__);
				}
				memset(session_id_candidates_set[session_id]->list_info->level_info[i], 0, sizeof(struct level_info_t));
				memcpy(session_id_candidates_set[session_id]->list_info->level_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
				
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "[DEBUG] pid =", session_id_candidates_set[session_id]->list_info->level_info[i]->pid);
				offset += sizeof(struct level_info_t);
			}

			for (unsigned int i = 0; i < candidates_num; i++) {
				char public_IP[16] = {0};
				char private_IP[16] = {0};
				memcpy(public_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip)));
				memcpy(private_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip)));
				
				
				for (map<unsigned long, int>::iterator iter = _pk_mgr_ptr->parents_table.begin(); iter != _pk_mgr_ptr->parents_table.end(); iter++) {
					_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "parents_table pid", iter->first, "state", iter->second);
				}
				
				// Build connection if the peer doesn't exist in parents_table
				if (_pk_mgr_ptr->parents_table.find(testing_info->level_info[i]->pid) == _pk_mgr_ptr->parents_table.end()) {
					_pk_mgr_ptr->parents_table[testing_info->level_info[i]->pid] = PEER_CONNECTING;
					
					// Self not behind NAT, candidate-peer not behind NAT
					if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Both are not behind NAT, active connect");
						non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 0, session_id);
					}
					// Self not behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Candidate-peer is behind NAT, passive connect");
						accept_check(testing_info->level_info[i],0,rescue_manifest,testing_info->level_info[i]->pid,session_id);
					}
					// Self is behind NAT, candidate-peer not behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip == testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, active connect");
						non_blocking_build_connection(testing_info->level_info[i], caller, rescue_manifest,testing_info->level_info[i]->pid, 0, session_id);
					}
					// Self is behind NAT, candidate-peer is behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[i]->private_ip != testing_info->level_info[i]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", public_IP, private_IP, "Both are behind NAT");
						
						// if both are in the same NAT
						if (self_info->public_ip == testing_info->level_info[i]->public_ip) {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__,"candidate-peer IP =", public_IP, private_IP, "Both are behind NAT(identical private IP)");
							//non_blocking_build_connection_udp(testing_info->level_info[i], caller, rescue_manifest, testing_info->level_info[i]->pid, 0, session_id);
							non_blocking_build_connection_udp(testing_info->level_info[i], caller, rescue_manifest, testing_info->level_info[i]->pid, 1, session_id);
						}
						else {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "candidate-peer IP =", public_IP, private_IP, "Both are behind NAT(different private IP)");
							non_blocking_build_connection_udp(testing_info->level_info[i], caller, rescue_manifest, testing_info->level_info[i]->pid, 1, session_id);
						}
						
					}
				}
			}
		}
		
		//}
	}
	// Caller is the parent of the candidate-peer
	else if (caller == PARENT_PEER) {
		if (candidates_num != 1) {
			_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] candidates_num is not equal to 1 when caller is candidate-peer", __FUNCTION__, __LINE__);
		}
		else {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "candidate-peer calls peer_communication");
			_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "rescue manifest =", rescue_manifest);
		
			session_id_candidates_set_iter = session_id_candidates_set.find(session_id);	//manifest_candidates_set has to be erased in stop_attempt_connect
			if (session_id_candidates_set_iter != session_id_candidates_set.end()) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] session id in the record in set_candidates_test");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] session id in the record in set_candidates_test \n");
				_logger_client_ptr->log_exit();
			}
			else {
				session_id_candidates_set[session_id] = new struct peer_com_info;
				if (!session_id_candidates_set[session_id]) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id] new failed", __FUNCTION__, __LINE__);
				}  
				
				int level_msg_size;
				int offset = 0;
				level_msg_size = sizeof(struct chunk_header_t) + sizeof(unsigned long) + sizeof(unsigned long) + candidates_num * sizeof(struct level_info_t *);

				session_id_candidates_set[session_id]->peer_num = candidates_num;
				session_id_candidates_set[session_id]->manifest = rescue_manifest;
				session_id_candidates_set[session_id]->role = caller;
				session_id_candidates_set[session_id]->list_info = (struct chunk_level_msg_t *) new unsigned char[level_msg_size];
				if (!session_id_candidates_set[session_id]->list_info) {
					_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info new failed", __FUNCTION__, __LINE__);
				}
				memset(session_id_candidates_set[session_id]->list_info, 0, level_msg_size);
				memcpy(session_id_candidates_set[session_id]->list_info, testing_info, (level_msg_size - candidates_num * sizeof(struct level_info_t *)));

				offset += (level_msg_size - candidates_num * sizeof(struct level_info_t *));

				for (unsigned int i = 0; i < candidates_num; i++) {
					session_id_candidates_set[session_id]->list_info->level_info[i] = new struct level_info_t;
					if (!session_id_candidates_set[session_id]->list_info->level_info[i]) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] session_id_candidates_set[session_id]->list_info->level_info[i] new failed", __FUNCTION__, __LINE__);
					}
					memset(session_id_candidates_set[session_id]->list_info->level_info[i], 0, sizeof(struct level_info_t));
					memcpy(session_id_candidates_set[session_id]->list_info->level_info[i], testing_info->level_info[i], sizeof(struct level_info_t));
					
					offset += sizeof(struct level_info_t);
				}
				
				char public_IP[16] = {0};
				char private_IP[16] = {0};
				memcpy(public_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->public_ip)));
				memcpy(private_IP, inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip), strlen(inet_ntoa(*(struct in_addr *)&testing_info->level_info[0]->private_ip)));
				
				for (map<unsigned long, int>::iterator iter = _pk_mgr_ptr->children_table.begin(); iter != _pk_mgr_ptr->children_table.end(); iter++) {
					_log_ptr->write_log_format("s(u) s u s d \n", __FUNCTION__, __LINE__, "children_table pid", iter->first, "state", iter->second);
				}
				
				if (_pk_mgr_ptr->children_table.find(testing_info->level_info[0]->pid) == _pk_mgr_ptr->children_table.end()) {
					_pk_mgr_ptr->children_table[testing_info->level_info[0]->pid] = PEER_CONNECTING;
				
					// Self not behind NAT, rescue-peer not behind NAT
					if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", public_IP, private_IP, "Both are not behind NAT, passive connect");
						accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id);
					}
					// Self not behind NAT, rescue-peer is behind NAT
					else if (self_info->private_ip == self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, passive connect");
						accept_check(testing_info->level_info[0],1,rescue_manifest,testing_info->level_info[0]->pid,session_id);
					}
					// Self is behind NAT, rescue-peer not behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip == testing_info->level_info[0]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", public_IP, private_IP, "Rescue-peer is behind NAT, active connect");
						non_blocking_build_connection(testing_info->level_info[0], caller, rescue_manifest,testing_info->level_info[0]->pid, 0, session_id);
					}
					// Self is behind NAT, rescue-peer is behind NAT
					else if (self_info->private_ip != self_info->public_ip && testing_info->level_info[0]->private_ip != testing_info->level_info[0]->public_ip) {	
						_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__,"Rescue-peer IP =", public_IP, private_IP, "Both are behind NAT");
						//non_blocking_build_connectionNAT_udp(testing_info->level_info[0], caller, rescue_manifest, testing_info->level_info[0]->pid, 0, session_id);
						//accept_check(testing_info->level_info[0], 1, rescue_manifest,testing_info->level_info[0]->pid, session_id);
						
						// if both are in the same NAT
						if (self_info->public_ip == testing_info->level_info[0]->public_ip) {
							_log_ptr->write_log_format("s(u) s u(u) \n", __FUNCTION__, __LINE__,"my IP =", (self_info->public_ip), (self_info->private_ip));
							_log_ptr->write_log_format("s(u) s u(u) \n", __FUNCTION__, __LINE__,"rescue-peer IP =", (testing_info->level_info[0]->public_ip), testing_info->level_info[0]->private_ip);
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__, __LINE__,"rescue-peer IP =", public_IP, private_IP, "Both are behind NAT(identical private IP)");
							fake_conn_udp(testing_info->level_info[0], caller, rescue_manifest, testing_info->level_info[0]->pid, 0, session_id);
						}
						else {
							_log_ptr->write_log_format("s(u) s s(s) s \n", __FUNCTION__,__LINE__, "rescue-peer IP =", public_IP, private_IP, "Both are behind NAT(different private IP)");
							fake_conn_udp(testing_info->level_info[0], caller, rescue_manifest, testing_info->level_info[0]->pid, 0, session_id);
						}
						
					}
				}
			}
			
		}
	}
	else {
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] unknow flag in set_candidates_test");
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] unknow flag in set_candidates_test \n");
		_logger_client_ptr->log_exit();
	}

	return ;
}

void peer_communication::clear_fd_in_peer_com(int fd)
{
	map<int ,  struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	map<int, struct fd_information *>::iterator map_fd_info_iter;
	
	debug_printf("Close fd %d \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", fd);
	debug_printf("Before close fd %d. Table information: \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close fd", fd);
	for (map<int, struct fd_information*>::iterator iter = map_fd_info.begin(); iter != map_fd_info.end(); iter++) {
		debug_printf("map_fd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_fd_NonBlockIO.begin(); iter != map_fd_NonBlockIO.end(); iter++) {
		debug_printf("map_fd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_NonBlockIO  fd", iter->first);
	}
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		debug_printf("map_fd_unknown  fd : %d \n", *iter);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_unknown  fd", *iter);
	}
	
	// Remove the fd in map_fd_info
	map_fd_info_iter = map_fd_info.find(fd);
	if (map_fd_info_iter != map_fd_info.end()) {
		_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__,
															"fd ", fd,
															"session id ",map_fd_info_iter->second->session_id,
															"pid", map_fd_info_iter->second->pid);
		delete map_fd_info_iter->second;
		map_fd_info.erase(map_fd_info_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_NonBlockIO
	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(fd);
	if (map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()) {
		delete map_fd_NonBlockIO_iter->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_unknown
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		if (*iter == fd) {
			_io_accept_ptr->map_fd_unknown.erase(iter);
			break;
		}
	}
	
	debug_printf("Close fd %d \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", fd);
	debug_printf("After close fd %d. Table information: \n", fd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close fd", fd);
	for (map<int, struct fd_information*>::iterator iter = map_fd_info.begin(); iter != map_fd_info.end(); iter++) {
		debug_printf("map_fd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_fd_NonBlockIO.begin(); iter != map_fd_NonBlockIO.end(); iter++) {
		debug_printf("map_fd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_NonBlockIO  fd", iter->first);
	}
	for (list<int>::iterator iter = _io_accept_ptr->map_fd_unknown.begin(); iter != _io_accept_ptr->map_fd_unknown.end(); iter++) {
		debug_printf("map_fd_unknown  fd : %d \n", *iter);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_fd_unknown  fd", *iter);
	}
	/*
	_log_ptr->write_log_format("s =>u \n", __FUNCTION__,__LINE__);
	_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"start close in peer_communication::clear_fd_in_peer_com fd : ",sock);

	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(sock);
	if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
		delete map_fd_NonBlockIO_iter ->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	}

	list<int>::iterator map_fd_unknown_iter;

	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		if( sock == *map_fd_unknown_iter){
			_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
			break;
		}
	}



	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"close fail (clear_fd_in_peer_com) fd : ",sock);
	}
	else{
		_log_ptr->write_log_format("s =>u s d s d s d s\n", __FUNCTION__,__LINE__,"fd : ",sock," session id : ",map_fd_info_iter->second->session_id," pid : ",map_fd_info_iter->second->pid," close succeed (clear_fd_in_peer_com)\n");
		
		delete map_fd_info_iter->second;
		map_fd_info.erase(map_fd_info_iter);
	}
	_log_ptr->write_log_format("s =>u \n", __FUNCTION__,__LINE__);
	*/
}

void peer_communication::clear_udpfd_in_peer_com(int udpfd)
{
	map<int ,  struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
	map<int, struct fd_information *>::iterator map_udpfd_info_iter;
	
	debug_printf("Close fd %d \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", udpfd);
	debug_printf("Before close fd %d. Table information: \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Before close fd", udpfd);
	for (map<int, struct fd_information*>::iterator iter = map_udpfd_info.begin(); iter != map_udpfd_info.end(); iter++) {
		debug_printf("map_udpfd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
		debug_printf("map_udpfd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_NonBlockIO  fd", iter->first);
	}
	
	// Remove the fd in map_udpfd_info
	map_udpfd_info_iter = map_udpfd_info.find(udpfd);
	if (map_udpfd_info_iter != map_udpfd_info.end()) {
		_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__,
															"fd ", udpfd,
															"session id ",map_udpfd_info_iter->second->session_id,
															"pid", map_udpfd_info_iter->second->pid);
		delete map_udpfd_info_iter->second;
		map_udpfd_info.erase(map_udpfd_info_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
	}
	
	// Remove the fd in map_fd_NonBlockIO
	map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(udpfd);
	if (map_udpfd_NonBlockIO_iter != map_udpfd_NonBlockIO.end()) {
		delete map_udpfd_NonBlockIO_iter->second;
		map_udpfd_NonBlockIO.erase(map_udpfd_NonBlockIO_iter);
	}
	else {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	debug_printf("Close fd %d \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "Close fd", udpfd);
	debug_printf("After close fd %d. Table information: \n", udpfd);
	_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "After close fd", udpfd);
	for (map<int, struct fd_information*>::iterator iter = map_udpfd_info.begin(); iter != map_udpfd_info.end(); iter++) {
		debug_printf("map_udpfd_info  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_info  fd", iter->first);
	}
	for (map<int, struct ioNonBlocking*>::iterator iter = map_udpfd_NonBlockIO.begin(); iter != map_udpfd_NonBlockIO.end(); iter++) {
		debug_printf("map_udpfd_NonBlockIO  fd : %d \n", iter->first);
		_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "map_udpfd_NonBlockIO  fd", iter->first);
	}
}

void peer_communication::accept_check(struct level_info_t *level_info_ptr,int fd_role,unsigned long manifest,unsigned long fd_pid, unsigned long session_id){
	//map<int, int>::iterator map_fd_unknown_iter;
	list<int>::iterator map_fd_unknown_iter;
	/*map<int, unsigned long>::iterator map_fd_session_id_iter;
	map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
	map<int, unsigned long>::iterator map_fd_manifest_iter;*/
	
	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		//if(*map_fd_unknown_iter == 1){
			
			
			map_fd_info_iter = map_fd_info.find(*map_fd_unknown_iter);
			if(map_fd_info_iter == map_fd_info.end()){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			if((manifest == map_fd_info_iter->second->manifest)&&(fd_role == map_fd_info_iter->second->role)&&(fd_pid == map_fd_info_iter->second->pid)){
				_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"update session of fd in peer_communication::accept_check fd : ",*map_fd_unknown_iter);
				printf("fd : %d update session of fd in peer_communication::accept_check\n",*map_fd_unknown_iter);

				map_fd_info_iter->second->session_id = session_id;

				/*
				bind to peer_com~ object
				*/
				_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"bind to peer_com in peer_communication::accept_check fd : ",*map_fd_unknown_iter);
				_net_ptr->set_nonblocking(map_fd_info_iter->first);

				_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(map_fd_info_iter->first, dynamic_cast<basic_class *> (this));
				_peer_mgr_ptr->fd_list_ptr->push_back(map_fd_info_iter->first);

				_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
				break;
			}
		
	}

	_log_ptr->write_log_format("s =>u s  \n", __FUNCTION__,__LINE__,"accept_check end ");


	/*
	call nat accept check
	*/
}

//flag 0 public ip flag 1 private ip //caller 0 rescue peer caller 1 
// pid: ��誺pid
int peer_communication::non_blocking_build_connection(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{	
	struct sockaddr_in peer_saddr;
	int retVal;
	int _sock;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid, "flag =", flag);

	// Check is there any connection already built
	if (CheckConnectionExist(caller, level_info_ptr->pid) == 1) {
		return 1;
	}
	
	if ((_sock = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);
		
		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
	}
	
	_net_udp_ptr->set_nonblocking(_sock);
	
	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_fd_NonBlockIO.find(_sock) != map_fd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	map_fd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_fd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_fd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_fd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_fd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	_net_ptr ->set_nonblocking(_sock);		//non-blocking connect
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));

    if (flag == 0) {	
	    peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		//_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"public_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else if (flag == 1) {	//in the same NAT
		peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		//debug_printf("connect to private_ip %s  port= %d \n", inet_ntoa(ip),level_info_ptr->tcp_port);	
		//_log_ptr->write_log_format("s =>u s u s s s u\n", __FUNCTION__,__LINE__,"connect to PID ",level_info_ptr ->pid,"private_ip",inet_ntoa (ip),"port= ",level_info_ptr->tcp_port );
	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknown flag in non_blocking_build_connection\n");
		_logger_client_ptr->log_exit();
	}
	
	peer_saddr.sin_port = htons(level_info_ptr->tcp_port);
	peer_saddr.sin_family = AF_INET;
	
	_log_ptr->write_log_format("s(u) s d s s(s) d \n", __FUNCTION__, __LINE__,
												"Connecting to pid", level_info_ptr->pid, 
												"IP", inet_ntoa(*(struct in_addr *)&level_info_ptr->public_ip), inet_ntoa(*(struct in_addr *)&level_info_ptr->private_ip), 
												level_info_ptr->tcp_port);
	
	if ((retVal = ::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
		if (socketErr == WSAEWOULDBLOCK) {
			//_net_ptr->set_nonblocking(_sock);
			_net_ptr->epoll_control(_sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
			_net_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_ptr));
			_peer_mgr_ptr->fd_list_ptr->push_back(_sock);	
			//_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"build_ connection failure : WSAEWOULDBLOCK");
		}
		else {

			::closesocket(_sock);
			::WSACleanup();
			
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] Build connection failed", retVal, socketErr);
			debug_printf("[ERROR] Build connection failed %d %d \n", retVal, socketErr);
			_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed", retVal, socketErr);
			_logger_client_ptr->log_exit();
		}	
#else
		int socketErr = errno;
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d d \n", "[ERROR] Build connection failed", retVal, socketErr);
		debug_printf("[ERROR] Build connection failed %d %d \n", retVal, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed", retVal, socketErr);
		_logger_client_ptr->log_exit();
		::close(_sock);
#endif
		

	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[DEBUG] Build connection too fast", retVal);
		debug_printf("[DEBUG] Build connection too fast %d \n", retVal);
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[DEBUG] Build connection too fast", retVal);
		_logger_client_ptr->log_exit();
	}
	
	
	/*
	this part stores the info in each table.
	*/
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect (before) fd =", _sock,
																"manifest =", manifest,
																"session_id =", session_id,
																"role =", caller,
																"pid =", pid,
																"non_blocking_build_connection (candidate peer)");

	map_fd_info_iter = map_fd_info.find(_sock);
	if(map_fd_info_iter != map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : fd %d already in map_fd_info in non_blocking_build_connection\n",_sock);
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	map_fd_info[_sock] = new struct fd_information;
	if (!map_fd_info[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
	}

	map_fd_info_iter = map_fd_info.find(_sock);
	if (map_fd_info_iter == map_fd_info.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_info", __FUNCTION__, __LINE__);
	}

	memset(map_fd_info_iter->second, 0, sizeof(struct fd_information));
	map_fd_info_iter->second->role = caller;
	map_fd_info_iter->second->manifest = manifest;
	map_fd_info_iter->second->pid = pid;
	map_fd_info_iter->second->session_id = session_id;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect fd =", map_fd_info_iter->first,
																"manifest =", map_fd_info_iter->second->manifest,
																"session_id =", map_fd_info_iter->second->session_id,
																"role =", map_fd_info_iter->second->role,
																"pid =", map_fd_info_iter->second->pid,
																"non_blocking_build_connection (candidate peer)");
	
	return RET_OK;
}

//flag 0 public ip flag 1 private ip //caller 0 rescue peer caller 1 
// pid: ��誺pid
int peer_communication::non_blocking_build_connection_udp(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{	
	
	struct sockaddr_in peer_saddr;
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };
	

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);
	
	// Check is there any connection already built
	if (CheckConnectionExist(caller, level_info_ptr->pid) == 1) {
		return 1;
	}
	
	// Put into queue, and start building connection until timeout. This for building connection in NAT environment
	struct build_udp_conn build_udp_conn_temp;
	memset(&build_udp_conn_temp, 0, sizeof(struct build_udp_conn));
	build_udp_conn_temp.caller = caller;
	build_udp_conn_temp.flag = flag;
	build_udp_conn_temp.manifest = manifest;
	build_udp_conn_temp.peer_saddr.sin_addr.s_addr = flag == 1 ? level_info_ptr->public_ip : level_info_ptr->private_ip;
	//build_udp_conn_temp.peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
	build_udp_conn_temp.peer_saddr.sin_port = htons(level_info_ptr->udp_port);
	build_udp_conn_temp.peer_saddr.sin_family = AF_INET;
	build_udp_conn_temp.pid = pid;
	build_udp_conn_temp.session_id = session_id;
	_log_ptr->timerGet(&(build_udp_conn_temp.timer));

	mmap_build_udp_conn.insert((pair<int, build_udp_conn>(session_id, build_udp_conn_temp)));
	

	/*
	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);
		
		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		PAUSE
	}
	
	_net_udp_ptr->set_nonblocking(_sock);
	
	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_udpfd_NonBlockIO.find(_sock) != map_udpfd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}
	
	map_udpfd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_udpfd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_udpfd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	//_net_ptr ->set_nonblocking(_sock);		//non-blocking connect
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));


    if (flag == 0) {	
	    peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
		_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", level_info_ptr->pid, inet_ntoa(peer_saddr.sin_addr), level_info_ptr->udp_port);
	}
	else if (flag == 1) {	//in the same NAT
		peer_saddr.sin_addr.s_addr = level_info_ptr->private_ip;
		_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", level_info_ptr->pid, inet_ntoa(peer_saddr.sin_addr), level_info_ptr->udp_port);
	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknown flag in non_blocking_build_connection\n");
		_logger_client_ptr->log_exit();
	}
	


	peer_saddr.sin_port = htons(level_info_ptr->udp_port);
	peer_saddr.sin_family = AF_INET;
	
	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", level_info_ptr->pid, inet_ntoa(peer_saddr.sin_addr), level_info_ptr->udp_port);

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}
	
	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));
	
	
	if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
		_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
		_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ptr));
		//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
		_net_udp_ptr->fd_list_ptr->push_back(_sock);	
	}
	else {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
		debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	
	// This part stores the info in each table.
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect (before) fd =", _sock,
																"manifest =", manifest,
																"session_id =", session_id,
																"my role =", caller,
																"pid =", pid,
																"non_blocking_build_connection (candidate peer)");

	if (map_udpfd_info.find(_sock) != map_udpfd_info.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : fd %d already in map_udpfd_info in non_blocking_build_connection\n",_sock);
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	map_udpfd_info[_sock] = new struct fd_information;
	if (!map_udpfd_info[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
	}

	map_udpfd_info_iter = map_udpfd_info.find(_sock);
	
	memset(map_udpfd_info_iter->second, 0, sizeof(struct fd_information));
	map_udpfd_info_iter->second->role = caller == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
	map_udpfd_info_iter->second->manifest = manifest;
	map_udpfd_info_iter->second->pid = pid;
	map_udpfd_info_iter->second->session_id = session_id;
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
																"non blocking connect fd =", map_udpfd_info_iter->first,
																"manifest =", map_udpfd_info_iter->second->manifest,
																"session_id =", map_udpfd_info_iter->second->session_id,
																"role =", map_udpfd_info_iter->second->role,
																"pid =", map_udpfd_info_iter->second->pid,
																"non_blocking_build_connection (candidate peer)");
	*/
	return RET_OK;
}

int peer_communication::non_blocking_build_connection_udp_now(struct build_udp_conn build_udp_conn_temp)
{
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };

	int caller = build_udp_conn_temp.caller;
	unsigned long manifest = build_udp_conn_temp.manifest;
	unsigned long pid = build_udp_conn_temp.pid;
	int flag = build_udp_conn_temp.flag;
	unsigned long session_id = build_udp_conn_temp.session_id;
	struct sockaddr_in peer_saddr;
	memcpy(&peer_saddr, &build_udp_conn_temp.peer_saddr, sizeof(struct sockaddr_in));

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);

	
	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);

		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		PAUSE
	}

	_net_udp_ptr->set_nonblocking(_sock);

	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_udpfd_NonBlockIO.find(_sock) != map_udpfd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}

	map_udpfd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_udpfd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_udpfd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	
	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", pid, inet_ntoa(peer_saddr.sin_addr), ntohs(peer_saddr.sin_port));

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));


	if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
		_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
		_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ptr));
		//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
		_net_udp_ptr->fd_list_ptr->push_back(_sock);
	}
	else {
		_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
		debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
		_logger_client_ptr->log_exit();
		PAUSE
	}

	
	// This part stores the info in each table.
	
	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
		"non blocking connect (before) fd =", _sock,
		"manifest =", manifest,
		"session_id =", session_id,
		"my role =", caller,
		"pid =", pid,
		"non_blocking_build_connection (candidate peer)");

	if (map_udpfd_info.find(_sock) != map_udpfd_info.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "error : fd %d already in map_udpfd_info in non_blocking_build_connection\n", _sock);
		_logger_client_ptr->log_exit();
		PAUSE
	}

	map_udpfd_info[_sock] = new struct fd_information;
	if (!map_udpfd_info[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
	}

	map_udpfd_info_iter = map_udpfd_info.find(_sock);

	memset(map_udpfd_info_iter->second, 0, sizeof(struct fd_information));
	map_udpfd_info_iter->second->role = caller == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
	map_udpfd_info_iter->second->manifest = manifest;
	map_udpfd_info_iter->second->pid = pid;
	map_udpfd_info_iter->second->session_id = session_id;

	_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
		"non blocking connect fd =", map_udpfd_info_iter->first,
		"manifest =", map_udpfd_info_iter->second->manifest,
		"session_id =", map_udpfd_info_iter->second->session_id,
		"role =", map_udpfd_info_iter->second->role,
		"pid =", map_udpfd_info_iter->second->pid,
		"non_blocking_build_connection (candidate peer)");
	
	return RET_OK;
}


int peer_communication::fake_conn_udp(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };
	struct sockaddr_in peer_saddr;

	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);


	peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
	peer_saddr.sin_port = htons(level_info_ptr->udp_port);
	peer_saddr.sin_family = AF_INET;

	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);

		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		PAUSE
	}

	_net_udp_ptr->set_nonblocking(_sock);

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));

	UDT::close(_sock);
	return RET_OK;
}

// This function is for RENDEZVOUS connection for both peers behind NAT
// Both peers must connect to each other. Wait until the sockstate is "CONNECTED"
int peer_communication::non_blocking_build_connectionNAT_udp(struct level_info_t *level_info_ptr, int caller, unsigned long manifest, unsigned long pid, int flag, unsigned long session_id)
{

	struct sockaddr_in peer_saddr;
	int retVal;
	int _sock;		// UDP socket
	char public_IP[16] = { 0 };
	char private_IP[16] = { 0 };


	_log_ptr->write_log_format("s(u) s d s d s d \n", __FUNCTION__, __LINE__, "caller =", caller, "manifest =", manifest, "pid =", pid);

	// Check is there any connection already built
	if (CheckConnectionExist(caller, level_info_ptr->pid) == 1) {
		return 1;
	}

	if ((_sock = UDT::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		debug_printf("[ERROR] Create socket failed %d %d \n", _sock, socketErr);
		_log_ptr->write_log_format("s(u) s d d \n", __FUNCTION__, __LINE__, "[ERROR] Create socket failed", _sock, socketErr);
		_pk_mgr_ptr->handle_error(SOCKET_ERROR, "[ERROR] Create socket failed", __FUNCTION__, __LINE__);

		_net_ptr->set_nonblocking(_sock);
#ifdef _WIN32
		::WSACleanup();
#endif
		PAUSE
	}

	_net_udp_ptr->set_nonblocking(_sock);
	_net_udp_ptr->set_rendezvous(_sock);

	//map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(_sock);
	if (map_udpfd_NonBlockIO.find(_sock) != map_udpfd_NonBlockIO.end()) {
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found map_fd_NonBlockIO", __FUNCTION__, __LINE__);
	}

	map_udpfd_NonBlockIO[_sock] = new struct ioNonBlocking;
	if (!map_udpfd_NonBlockIO[_sock]) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] ioNonBlocking new error", __FUNCTION__, __LINE__);
	}
	memset(map_udpfd_NonBlockIO[_sock], 0, sizeof(struct ioNonBlocking));
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
	map_udpfd_NonBlockIO[_sock]->io_nonblockBuff.nonBlockingSendCtrl.recv_ctl_info.ctl_state = READY;
	
	memset((struct sockaddr_in*)&peer_saddr, 0, sizeof(struct sockaddr_in));
	peer_saddr.sin_addr.s_addr = level_info_ptr->public_ip;
	peer_saddr.sin_port = htons(level_info_ptr->udp_port);
	peer_saddr.sin_family = AF_INET;

	_log_ptr->write_log_format("s(u) s u s d\n", __FUNCTION__, __LINE__, "connect to pid", level_info_ptr->pid, inet_ntoa(peer_saddr.sin_addr), level_info_ptr->udp_port);

	string svc_udp_port;
	_prep->read_key("svc_udp_port", svc_udp_port);
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((unsigned short)atoi(svc_udp_port.c_str()));
	debug_printf("----- port : %d \n", (unsigned short)atoi(svc_udp_port.c_str()));
	if (UDT::bind(_sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == UDT::ERROR) {
		debug_printf("ErrCode: %d  ErrMsg: %s \n", UDT::getlasterror().getErrorCode(), UDT::getlasterror().getErrorMessage());
		//log_ptr->write_log_format("s(u) s d s s \n", __FUNCTION__, __LINE__, "ErrCode:", UDT::getlasterror().getErrorCode(), "ErrMsg", UDT::getlasterror().getErrorMessage());
		PAUSE
	}

	if (UDT::ERROR == UDT::connect(_sock, (struct sockaddr*)&peer_saddr, sizeof(peer_saddr))) {
		cout << "connect: " << UDT::getlasterror().getErrorMessage() << "  " << UDT::getlasterror().getErrorCode();
		PAUSE
	}

	_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__, __LINE__, "sock", _sock, "state =", UDT::getsockstate(_sock));
	debug_printf("sock = %d  state = %d \n", _sock, UDT::getsockstate(_sock));


	if (caller == CHILD_PEER) {
		if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
			_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_connect_udp_ptr));
			//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
			_net_udp_ptr->fd_list_ptr->push_back(_sock);
		}
		else {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
			debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
			_logger_client_ptr->log_exit();
			PAUSE
		}

		
		// Stores the info in each table.
		_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
			"non blocking connect (before) fd =", _sock,
			"manifest =", manifest,
			"session_id =", session_id,
			"my role =", caller,
			"pid =", pid,
			"non_blocking_build_connection (candidate peer)");

		if (map_udpfd_info.find(_sock) != map_udpfd_info.end()) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "error : fd %d already in map_udpfd_info in non_blocking_build_connection\n", _sock);
			_logger_client_ptr->log_exit();
			PAUSE
		}

		map_udpfd_info[_sock] = new struct fd_information;
		if (!map_udpfd_info[_sock]) {
			_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] fd_information new error", __FUNCTION__, __LINE__);
		}

		map_udpfd_info_iter = map_udpfd_info.find(_sock);

		memset(map_udpfd_info_iter->second, 0, sizeof(struct fd_information));
		map_udpfd_info_iter->second->role = caller == CHILD_PEER ? PARENT_PEER : CHILD_PEER;
		map_udpfd_info_iter->second->manifest = manifest;
		map_udpfd_info_iter->second->pid = pid;
		map_udpfd_info_iter->second->session_id = session_id;

		_log_ptr->write_log_format("s(u) s d s d s d s d s d s \n", __FUNCTION__, __LINE__,
			"non blocking connect fd =", map_udpfd_info_iter->first,
			"manifest =", map_udpfd_info_iter->second->manifest,
			"session_id =", map_udpfd_info_iter->second->session_id,
			"role =", map_udpfd_info_iter->second->role,
			"pid =", map_udpfd_info_iter->second->pid,
			"non_blocking_build_connection (candidate peer)");
	}
	else {
		if (UDT::getsockstate(_sock) == CONNECTING || UDT::getsockstate(_sock) == CONNECTED) {
			_net_udp_ptr->epoll_control(_sock, EPOLL_CTL_ADD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
			_net_udp_ptr->set_fd_bcptr_map(_sock, dynamic_cast<basic_class *> (_io_nonblocking_udp_ptr));
			//_peer_mgr_ptr->fd_udp_list_ptr->push_back(_sock);	
			_net_udp_ptr->fd_list_ptr->push_back(_sock);
		}
		else {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "[ERROR] Build connection failed. state =", UDT::getsockstate(_sock));
			debug_printf("[ERROR] Build connection failed. state = %d \n", UDT::getsockstate(_sock));
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d \n", "[ERROR] Build connection failed. state", UDT::getsockstate(_sock));
			_logger_client_ptr->log_exit();
			PAUSE
		}

		// Check wheather new_fd is already in the map_udpfd_NonBlockIO or not
		
		struct ioNonBlocking* ioNonBlocking_ptr = new struct ioNonBlocking;
		if (!ioNonBlocking_ptr) {
			debug_printf("ioNonBlocking_ptr new error \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "ioNonBlocking_ptr new error");
			PAUSE
		}

		_log_ptr->write_log_format("s(u) s d s \n", __FUNCTION__, __LINE__, "Add fd", _sock, "into map_udpfd_NonBlockIO");

		memset(ioNonBlocking_ptr, 0, sizeof(struct ioNonBlocking));
		ioNonBlocking_ptr->io_nonblockBuff.nonBlockingRecv.recv_packet_state = READ_HEADER_READY;
		map_udpfd_NonBlockIO[_sock] = ioNonBlocking_ptr;
		
	}

	return RET_OK;
}

// If it is able to not build connection, return 1
int peer_communication::CheckConnectionExist(int caller, unsigned long pid)
{
	if (caller == CHILD_PEER) {
		map<unsigned long, int>::iterator map_pid_fd_iter;
		
		if (_pk_mgr_ptr->map_pid_parent.find(pid) != _pk_mgr_ptr->map_pid_parent.end()) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent pid", pid, "is already in map_pid_parent");
			return 1;
		}
		
		if (_pk_mgr_ptr->map_pid_parent_temp.find(pid) != _pk_mgr_ptr->map_pid_parent_temp.end()) {
			if (_pk_mgr_ptr->map_pid_parent_temp.count(pid) > 1) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "parent pid", pid, "is already in map_pid_parent_temp");
				return 1;
			}
		}
		
		/*
		// ���e�w�g�إ߹L�s�u�� �bmap_in_pid_fd�̭� �h���A�إ�(�O�ҹ�P��parent���A�إ߲ĤG���u)
		// map_in_pid_fd: parent-peer which alreay established connection, including temp parent-peer
		if (_peer_ptr->map_in_pid_fd.find(pid) != _peer_ptr->map_in_pid_fd.end()) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
			return 1;
		}
		//for(map_pid_fd_iter = _peer_ptr->map_in_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_in_pid_fd.end(); map_pid_fd_iter++){
		//	if(map_pid_fd_iter->first == pid ){
		//		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "fd already in map_in_pid_fd in non_blocking_build_connection (rescue peer)");
		//		return 1;
		//	}
		//}
		
		// this may have problem
		// map_pid_parent_temp: temp parent-peer
		if (_pk_mgr_ptr->map_pid_parent_temp.find(pid) != _pk_mgr_ptr->map_pid_parent_temp.end()) {
			//��ӥH�W�N�u�βĤ@�Ӫ��s�u
			if(_pk_mgr_ptr ->map_pid_parent_temp.count(pid) >= 2 ){
				return 1;
			}
		}

		// �Y�bmap_pid_parent �h���A���إ߳s�u
		// map_pid_parent: real parent-peer
		pid_peerDown_info_iter = _pk_mgr_ptr ->map_pid_parent.find(pid);
		if(pid_peerDown_info_iter != _pk_mgr_ptr ->map_pid_parent.end()){
			printf("pid =%d already in connect find in map_pid_parent",pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_parent");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_parent in non_blocking_build_connection (rescue peer)");
			return 1;
		}
		*/
	}
	else if (caller == PARENT_PEER) {
		multimap<unsigned long, struct peer_info_t *>::iterator pid_peer_info_iter;
		multimap<unsigned long, struct peer_info_t *>::iterator map_pid_child_temp_iter;
		map<unsigned long, int>::iterator map_pid_fd_iter;
		map<unsigned long, struct peer_info_t *>::iterator map_pid_child1_iter;

		map_pid_child1_iter = _pk_mgr_ptr ->map_pid_child2.find(pid);
		if (_pk_mgr_ptr->map_pid_child2.find(pid) != _pk_mgr_ptr->map_pid_child2.end()) {
			_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "child pid", pid, "is already in map_pid_child2");
			return 1;
		}
		
		if(_pk_mgr_ptr->map_pid_child_temp.find(pid) != _pk_mgr_ptr->map_pid_child_temp.end()){
			if (_pk_mgr_ptr->map_pid_child_temp.count(pid) > 1) {
				_log_ptr->write_log_format("s(u) s u s \n", __FUNCTION__, __LINE__, "child pid", pid, "is already in map_pid_child_temp");
				return 1;
			}
		}
		
		/*
		//���e�w�g�إ߹L�s�u�� �bmap_out_pid_fd�̭� �h���A�إ�(�O�ҹ�P��child���A�إ߲ĤG���u)
		for(map_pid_fd_iter = _peer_ptr->map_out_pid_fd.begin();map_pid_fd_iter != _peer_ptr->map_out_pid_fd.end(); map_pid_fd_iter++){
			if(map_pid_fd_iter->first == pid ){
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_out_pid_fd in non_blocking_build_connection (candidate peer)");
				return 1;
			}
		}

		//�Y�bmap_pid_child1 �h���A���إ߳s�u
		map_pid_child1_iter = _pk_mgr_ptr ->map_pid_child1.find(pid);
		if(map_pid_child1_iter != _pk_mgr_ptr ->map_pid_child1.end()){
			printf("pid =%d already in connect find in map_pid_child1",pid);
			_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_child1");
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_child1 in non_blocking_build_connection (candidate peer)");\
			return 1;
		}


		map_pid_child_temp_iter = _pk_mgr_ptr ->map_pid_child_temp.find(pid);
		if(map_pid_child_temp_iter !=  _pk_mgr_ptr ->map_pid_child_temp.end()){
			if(_pk_mgr_ptr ->map_pid_child_temp.count(pid) >=2){
				printf("pid =%d  already in connect find in map_pid_child_temp  testing",pid);
				_log_ptr->write_log_format("s =>u s u s\n", __FUNCTION__,__LINE__,"pid =",pid,"already in connect find in map_pid_child_temp ");
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"fd already in map_pid_child_temp in non_blocking_build_connection (candidate peer)");	
				return 1;
			}
		}
		*/
	}
	return 0;
}

io_accept * peer_communication::get_io_accept_handler()
{
	return _io_accept_ptr;
}

void peer_communication::fd_close(int sock){
	_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"close in peer_communication::fd_close fd ",sock);
	_net_ptr->close(sock);

	list<int>::iterator fd_iter;
	for(fd_iter = _peer_ptr->fd_list_ptr->begin(); fd_iter != _peer_ptr->fd_list_ptr->end(); fd_iter++) {
		if(*fd_iter == sock) {
			_peer_ptr->fd_list_ptr->erase(fd_iter);
			break;
		}
	}

	map_fd_info_iter = map_fd_info.find(sock);
	if(map_fd_info_iter == map_fd_info.end()){
		//_log_ptr->write_log_format("s =>u s d\n", __FUNCTION__,__LINE__,"error : close cannot find table in peer_communication::fd_close fd ",sock);
		//_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : close cannot find table in peer_communication::fd_close fd ",sock);
		//_logger_client_ptr->log_exit();
		
	}
	else{
		delete map_fd_info_iter->second;
		map_fd_info.erase(sock);
	}

	map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(sock);
	if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
		delete map_fd_NonBlockIO_iter->second;
		map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);
	
	}



	list<int>::iterator map_fd_unknown_iter;

	for(map_fd_unknown_iter = _io_accept_ptr->map_fd_unknown.begin();map_fd_unknown_iter != _io_accept_ptr->map_fd_unknown.end();map_fd_unknown_iter++){
		if( sock == *map_fd_unknown_iter){
			_io_accept_ptr->map_fd_unknown.erase(map_fd_unknown_iter);
			break;
		}
	}
			


}

// Remove certain iterator in "session_id_candidates_set", "map_fd_info", "and map_fd_NonBlockIO"
// When connect time triggered, this function will be called
void peer_communication::stop_attempt_connect(unsigned long stop_session_id)
{
	int delete_fd_flag = 0;
	map<int, struct fd_information *>::iterator map_fd_info_iter;
	map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
	
	session_id_candidates_set_iter = session_id_candidates_set.find(stop_session_id);
	if(session_id_candidates_set_iter == session_id_candidates_set.end()){
		_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in session_id_candidates_set", __FUNCTION__, __LINE__);
		return ;
	}
	
	if (session_id_candidates_set_iter->second->role == CHILD_PEER) {	//caller is rescue peer
		//total_manifest = total_manifest & (~session_id_candidates_set_iter->second->manifest);
		_log_ptr->write_log_format("s =>u s d s d s d s d\n", __FUNCTION__,__LINE__,"session_id : ",stop_session_id,", manifest : ",session_id_candidates_set_iter->second->manifest,", role: ",session_id_candidates_set_iter->second->role,", list_number: ",session_id_candidates_set_iter->second->peer_num);
		
		// Remove the session id in session_id_candidates_set
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "pid", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}
		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		/*map<int, unsigned long>::iterator map_fd_session_id_iter;
		map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
		map<int, unsigned long>::iterator map_fd_manifest_iter;*/

		map_fd_info_iter = map_fd_info.begin();
		while(map_fd_info_iter != map_fd_info.end()){
			if(map_fd_info_iter->second->session_id == stop_session_id){

				delete_fd_flag = 1;

				_log_ptr->write_log_format("s =>u s d s u \n", __FUNCTION__, __LINE__,
					"connect faild delete table and close fd", map_fd_info_iter->first,
					"pid", map_fd_info_iter->second->pid);

				if(_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()){
					/*
					connect faild delete table and close fd
					*/


					_log_ptr->write_log_format("s =>u s d s u \n", __FUNCTION__, __LINE__,
																"connect faild delete table and close fd", map_fd_info_iter->first,
																"pid", map_fd_info_iter->second->pid);
					/*
					close fd
					*/
					list<int>::iterator fd_iter;

					//_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
					cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
//						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
					_net_ptr->close(map_fd_info_iter->first);

					for(fd_iter = _peer_mgr_ptr->fd_list_ptr->begin(); fd_iter != _peer_mgr_ptr->fd_list_ptr->end(); fd_iter++) {
						if(*fd_iter == map_fd_info_iter->first) {
							_peer_mgr_ptr->fd_list_ptr->erase(fd_iter);
							break;
						}
					}
				}
				else{
					/*
					connect succeed just delete table
					*/
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect succeed just delete table");
					
				}
				
				map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

				}


				delete [] map_fd_info_iter ->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}

	}
	else if (session_id_candidates_set_iter->second->role == PARENT_PEER){	//caller is candidate
		_log_ptr->write_log_format("s(u) s d s d s d s d \n", __FUNCTION__, __LINE__,
															"Stop session_id", stop_session_id,
															"my role", session_id_candidates_set_iter->second->role,
															"manifest", session_id_candidates_set_iter->second->manifest,
															"list_number", session_id_candidates_set_iter->second->peer_num);
		for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
			_log_ptr->write_log_format("s(u) s d \n", __FUNCTION__, __LINE__, "list pid : ", session_id_candidates_set_iter->second->list_info->level_info[i]->pid);
			delete session_id_candidates_set_iter->second->list_info->level_info[i];
		}

		delete session_id_candidates_set_iter->second->list_info;
		delete session_id_candidates_set_iter->second;
		session_id_candidates_set.erase(session_id_candidates_set_iter);

		// Remove certain iterator in map_fd_info and map_fd_NonBlockIO
		map_fd_info_iter = map_fd_info.begin();
		while (map_fd_info_iter != map_fd_info.end()) {
			if (map_fd_info_iter->second->session_id == stop_session_id) {

				delete_fd_flag = 1;

				if (_peer_ptr->map_fd_pid.find(map_fd_info_iter->first) == _peer_ptr->map_fd_pid.end()) {
					/*
					connect faild delete table and close fd
					*/
					

					_log_ptr->write_log_format("s =>u s d \n", __FUNCTION__,__LINE__,"connect faild delete table and close fd ",map_fd_info_iter->first);
					/*
					close fd
					*/
					list<int>::iterator fd_iter;

					_log_ptr->write_log_format("s => s \n", (char*)__PRETTY_FUNCTION__, "peer_com");
					cout << "peer_com close fd since timeout " << map_fd_info_iter->first <<  endl;
//						_net_ptr->epoll_control(map_fd_info_iter->first, EPOLL_CTL_DEL, 0);
					_net_ptr->close(map_fd_info_iter->first);

					for(fd_iter = _peer_mgr_ptr->fd_list_ptr->begin(); fd_iter != _peer_mgr_ptr->fd_list_ptr->end(); fd_iter++) {
						if(*fd_iter == map_fd_info_iter->first) {
							_peer_mgr_ptr->fd_list_ptr->erase(fd_iter);
							break;
						}
					}
				}
				else{
					/*
					connect succeed just delete table
					*/
					_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"connect succeed just delete table ");
					

				}

				map_fd_NonBlockIO_iter= map_fd_NonBlockIO.find(map_fd_info_iter->first);
				if(map_fd_NonBlockIO_iter != map_fd_NonBlockIO.end()){
					delete map_fd_NonBlockIO_iter->second;
					map_fd_NonBlockIO.erase(map_fd_NonBlockIO_iter);

				}

				delete map_fd_info_iter->second;
				map_fd_info.erase(map_fd_info_iter);
				map_fd_info_iter = map_fd_info.begin();
			}
			else{
				map_fd_info_iter++;
			}
		}
	}
	else {
	
	}

	//map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;
	if(delete_fd_flag==0){
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"cannot find fd info table ");
	}
	else{
		_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"delete fd info table");
	}

	for(map_fd_info_iter = map_fd_info.begin();map_fd_info_iter != map_fd_info.end();map_fd_info_iter++){
		_log_ptr->write_log_format("s =>u s d s d \n", __FUNCTION__,__LINE__,"fd : ",map_fd_info_iter->first,", pid: ",map_fd_info_iter->second->pid);
	}
	
}

int peer_communication::SendPeerCon(int sock, unsigned long pid)
{
	struct chunk_request_msg_t *chunk_request_ptr = NULL;
	Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr = NULL;
	map<int, struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
	unsigned long his_pid;
	int send_byte;
	
	map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(sock);
	if (map_fd_NonBlockIO_iter == map_fd_NonBlockIO.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO \n");
		debug_printf("[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO \n");
		_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO");
		*(_net_ptr->_errorRestartFlag) = RESTART;
		PAUSE
	}

	Nonblocking_Send_Ctrl_ptr = &(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl);

	chunk_request_ptr = (struct chunk_request_msg_t *)new unsigned char[sizeof(struct chunk_request_msg_t)];
	if (!chunk_request_ptr) {
		_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer::chunk_request_ptr  new error", __FUNCTION__, __LINE__);
	}
	memset(chunk_request_ptr, 0, sizeof(struct chunk_request_msg_t));

	chunk_request_ptr->header.cmd = CHNK_CMD_PEER_CON;
	chunk_request_ptr->header.rsv_1 = REQUEST;
	chunk_request_ptr->header.length = sizeof(struct request_info_t);
	chunk_request_ptr->info.pid = pid;
	chunk_request_ptr->info.channel_id = 0;		// Not used
	chunk_request_ptr->info.private_ip = _pk_mgr_ptr->my_private_ip;
	chunk_request_ptr->info.tcp_port = 0;	// Not used
	chunk_request_ptr->info.udp_port = 0;	// Not used

	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.offset = 0;
	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.total_len = sizeof(struct role_struct);
	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.expect_len = sizeof(struct role_struct);
	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.buffer = (char *)chunk_request_ptr;
	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = (chunk_t *)chunk_request_ptr;
	Nonblocking_Send_Ctrl_ptr->recv_ctl_info.serial_num = chunk_request_ptr->header.sequence_number;

	//debug_printf("header.cmd = %d, total_len = %d \n", chunk_request_ptr->header.cmd, chunk_request_ptr->header.length);
	_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__, __LINE__, "chunk_request_ptr->info.pid =", pid);

	send_byte = _net_ptr->nonblock_send(sock, & (Nonblocking_Send_Ctrl_ptr->recv_ctl_info ));

	if (send_byte < 0) {
		if (chunk_request_ptr) {
			delete chunk_request_ptr;
		}
	
		//Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
		//data_close(sock, "PEER�@COM error", CLOSE_PARENT);
		return RET_SOCK_ERROR;
		PAUSE
	}
	else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY ) {
		debug_printf("111 \n");
		if (chunk_request_ptr) {
			debug_printf("222 \n");
			delete chunk_request_ptr;
		}
		debug_printf("333 \n");
		//if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
		//	debug_printf("444 \n");
		//	delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
		//}
		debug_printf("555 \n");
		
		Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
		return 0;
	}
}

int peer_communication::handle_pkt_in(int sock)
{	
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only receive PEER_CON protocol sent by join/rescue peer (the peer is candidate's peer).
	And handle P2P structure.
	*/
	debug_printf("peer_communication::handle_pkt_in \n");
	
	map_fd_info_iter = map_fd_info.find(sock);
	
	if(map_fd_info_iter == map_fd_info.end()){
		
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info structure in peer_communication::handle_pkt_in\n");
		_logger_client_ptr->log_exit();
	}
	else{
		printf("map_fd_info_iter->second->flag: %d \n", map_fd_info_iter->second->role);
		if(map_fd_info_iter->second->role == CHILD_PEER) {	//this fd is rescue peer
			//do nothing rebind to event out only
			_net_ptr->set_nonblocking(sock);
			//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLOUT);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is rescue peer do nothing, jsut rebind to event out only");
		}
		else if(map_fd_info_iter->second->role == PARENT_PEER){	//this fd is candidate peer
			
			map_fd_NonBlockIO_iter = map_fd_NonBlockIO.find(sock);
			if(map_fd_NonBlockIO_iter == map_fd_NonBlockIO.end()){
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_fd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;
			struct chunk_header_t* chunk_header_ptr = NULL;
			struct chunk_t* chunk_ptr = NULL;
			unsigned long buf_len=0;
			int recv_byte=0;

			Nonblocking_Recv_Ctl_ptr = &(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv) ;


			for(int i =0;i<5;i++){
				if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY){
					chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
					if(!(chunk_header_ptr ) ){
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_header_ptr  new error", __FUNCTION__, __LINE__);
					}
					memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK){
					buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
					chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
					if (!chunk_ptr) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_ptr  new error", __FUNCTION__, __LINE__);
					}
					
					memset(chunk_ptr, 0x0, buf_len);
					memcpy(chunk_ptr,Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

					if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)
						delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer ;

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

					//			printf("chunk_ptr->header.length = %d  seq = %d\n",chunk_ptr->header.length,chunk_ptr->header.sequence_number);
					Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;

				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){
					//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;

					//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;
					break;
				}

				recv_byte =_net_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);
				printf("peer_communication::handle_pkt_in  recv_byte: %d \n", recv_byte);

				if(recv_byte < 0) {
					printf("error occ in nonblocking \n");
					fd_close(sock);

					//PAUSE
					return RET_SOCK_ERROR;
				}

			}

			if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){

				chunk_ptr =(chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;

				Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;

				buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;

			}else{
				//other stats
				return RET_OK;
			}

			//determine stream direction
			if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
				cout << "CHNK_CMD_PEER_CON" << endl;
				_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_CON");
				
				//use fake cin info
				
				struct sockaddr_in fake_cin;
				memset(&fake_cin,0x00,sizeof(struct sockaddr_in));

				_peer_ptr->handle_connect(sock, chunk_ptr,fake_cin);

				
				//bind to peer_com~ object
				
				_net_ptr->set_nonblocking(sock);
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
			} else{
				
				printf("error : unknow or cannot handle cmd : in peer_communication::handle_pkt_in  cmd =%d \n",chunk_ptr->header.cmd);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : unknow or cannot handle cmd : in peer_communication::handle_pkt_in ",chunk_ptr->header.cmd);
				_logger_client_ptr->log_exit();
			}

			if(chunk_ptr)
				delete chunk_ptr;
		}
		else{
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknow flag in peer_communication::handle_pkt_in\n");
			_logger_client_ptr->log_exit();
		}
	}
	
	return RET_OK;
}

int peer_communication::handle_pkt_in_udp(int sock)
{	
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only receive PEER_CON protocol sent by join/rescue peer (the peer is candidate's peer).
	And handle P2P structure.
	*/
	debug_printf("peer_communication::handle_pkt_in \n");
	
	map<int, struct fd_information *>::iterator map_udpfd_info_iter;
	
	if ((map_udpfd_info_iter = map_udpfd_info.find(sock)) != map_udpfd_info.end()) {
		// This socket is UDP
		debug_printf("map_udpfd_info_iter->second->flag: %d \n", map_udpfd_info_iter->second->role);
		if (map_udpfd_info_iter->second->role == PARENT_PEER) {	//this fd is rescue peer
			//do nothing rebind to event out only
			//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_MOD, UDT_EPOLL_OUT);
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__,__LINE__,"this fd is rescue peer do nothing, jsut rebind to event out only");
		}
		else if (map_udpfd_info_iter->second->role == CHILD_PEER) {	//this fd is candidate peer
			
			map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(sock);
			if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_udpfd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			Nonblocking_Ctl * Nonblocking_Recv_Ctl_ptr =NULL;
			struct chunk_header_t* chunk_header_ptr = NULL;
			struct chunk_t* chunk_ptr = NULL;
			unsigned long buf_len=0;
			int recv_byte=0;

			Nonblocking_Recv_Ctl_ptr = &(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingRecv) ;

			for (int i = 0; i < 5; i++) {
				if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_READY) {
					chunk_header_ptr = (struct chunk_header_t *)new unsigned char[sizeof(chunk_header_t)];
					if(!(chunk_header_ptr ) ){
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_header_ptr  new error", __FUNCTION__, __LINE__);
					}
					memset(chunk_header_ptr, 0x0, sizeof(struct chunk_header_t));

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =0 ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_header_ptr ;
				}
				else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_RUNNING) {
					//do nothing
				}
				else if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_HEADER_OK) {
					buf_len = sizeof(chunk_header_t)+ ((chunk_t *)(Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)) ->header.length ;
					chunk_ptr = (struct chunk_t *)new unsigned char[buf_len];
					if (!chunk_ptr) {
						_pk_mgr_ptr->handle_error(MALLOC_ERROR, "[ERROR] peer_communication::chunk_ptr  new error", __FUNCTION__, __LINE__);
					}
					
					memset(chunk_ptr, 0x0, buf_len);
					memcpy(chunk_ptr,Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer,sizeof(chunk_header_t));

					if (Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer)
						delete [] (unsigned char*)Nonblocking_Recv_Ctl_ptr->recv_ctl_info.buffer ;

					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.offset =sizeof(chunk_header_t) ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.total_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.expect_len = chunk_ptr->header.length ;
					Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer = (char *)chunk_ptr ;

					//			printf("chunk_ptr->header.length = %d  seq = %d\n",chunk_ptr->header.length,chunk_ptr->header.sequence_number);
					Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_PAYLOAD_READY ;

				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_READY){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_RUNNING){
					//do nothing
				}
				else if(Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK){
					//			chunk_ptr =(chunk_t *)Recv_nonblocking_ctl_ptr ->recv_ctl_info.buffer;

					//			Recv_nonblocking_ctl_ptr->recv_packet_state = READ_HEADER_READY ;
					break;
				}

				recv_byte =_net_udp_ptr->nonblock_recv(sock,Nonblocking_Recv_Ctl_ptr);
				printf("peer_communication::handle_pkt_in  recv_byte: %d \n", recv_byte);

				if(recv_byte < 0) {
					printf("error occ in nonblocking \n");
					fd_close(sock);

					//PAUSE
					return RET_SOCK_ERROR;
				}

			}

			if (Nonblocking_Recv_Ctl_ptr->recv_packet_state == READ_PAYLOAD_OK) {

				chunk_ptr =(chunk_t *)Nonblocking_Recv_Ctl_ptr ->recv_ctl_info.buffer;

				Nonblocking_Recv_Ctl_ptr->recv_packet_state = READ_HEADER_READY ;

				buf_len =  sizeof(struct chunk_header_t) +  chunk_ptr->header.length ;

			}
			else {
				//other stats
				return RET_OK;
			}

			//determine stream direction
			if (chunk_ptr->header.cmd == CHNK_CMD_PEER_CON) {
				debug_printf("CHNK_CMD_PEER_CON \n");
				_log_ptr->write_log_format("s(u) s u \n", __FUNCTION__,__LINE__,"CHNK_CMD_PEER_CON pid", map_udpfd_info_iter->second->pid);
				
				//use fake cin info
				
				struct sockaddr_in fake_cin;
				memset(&fake_cin,0x00,sizeof(struct sockaddr_in));

				_peer_ptr->handle_connect_udp(sock, chunk_ptr,fake_cin);
				_pk_mgr_ptr->children_table[map_udpfd_info_iter->second->pid] = PEER_CONNECTED;
				
				//bind to peer_com~ object
				//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_MOD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
				_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
			}
			else {
				debug_printf("[ERROR] unknow or cannot handle cmd : in peer_communication::handle_pkt_in  cmd =%d \n",chunk_ptr->header.cmd);
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","[ERROR] unknow or cannot handle cmd : in peer_communication::handle_pkt_in ",chunk_ptr->header.cmd);
				_logger_client_ptr->log_exit();
				PAUSE
			}

			if (chunk_ptr) {
				delete chunk_ptr;
			}
		}
		else{
			debug_printf("[ERROR] unknow flag in peer_communication::handle_pkt_in \n");
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","[ERROR] unknow flag in peer_communication::handle_pkt_in");
			_logger_client_ptr->log_exit();
			PAUSE
		}
	}
	else {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	return RET_OK;
}

//first write, then set fd to readable & excecption only
int peer_communication::handle_pkt_out(int sock)	
{
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only send PEER_CON protocol to candidates, if the fd is in the list. (the peer is join/rescue peer)
	And handle P2P structure.
	*/
	
	debug_printf("peer_communication::handle_pkt_out \n");
	
	map_fd_info_iter = map_fd_info.find(sock);
	if (map_fd_info_iter == map_fd_info.end()) {
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find map_fd_info_iter structure in peer_communication::handle_pkt_out\n");
		_logger_client_ptr->log_exit();
	}
	else {
		printf("map_fd_info_iter->second->flag: %d \n", map_fd_info_iter->second->role);
		if(map_fd_info_iter->second->role == 0){	//this fd is rescue peer
			//send peer con
			int ret,
				send_flag = 0;
			int i;
			
			
			session_id_candidates_set_iter = session_id_candidates_set.find(map_fd_info_iter->second->session_id);
			if(session_id_candidates_set_iter == session_id_candidates_set.end()){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find session_id_candidates_set structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			


			map_fd_NonBlockIO_iter =map_fd_NonBlockIO.find(sock);
			if(map_fd_NonBlockIO_iter==map_fd_NonBlockIO.end()){
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_fd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			for(i=0;i<session_id_candidates_set_iter->second->peer_num;i++){
				if(session_id_candidates_set_iter->second->list_info->level_info[i]->pid == map_fd_info_iter->second->pid){
					ret = _peer_ptr->handle_connect_request(sock, session_id_candidates_set_iter->second->list_info->level_info[i], session_id_candidates_set_iter->second->list_info->pid,&(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl));
					send_flag =1;
				}
			}

			if(send_flag == 0){
				
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","cannot find level info structure in peer_communication::handle_pkt_out\n");
				_logger_client_ptr->log_exit();
			}

			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"send CHNK_CMD_PEER_CON");
			printf("ret: %d \n", ret);
			
			if(ret < 0) {
				cout << "handle_connect_request error!!!" << endl;
				fd_close(sock);
				return RET_ERROR;
				
			} else if(map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == RUNNING){
			
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				
			}else if (map_fd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY){
				
				_net_ptr->set_nonblocking(sock);
				//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);	
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}
		}
		else if(map_fd_info_iter->second->role == 1){	//this fd is candidate
			//do nothing rebind to event in only
			_net_ptr->set_nonblocking(sock);
			_log_ptr->write_log_format("s =>u s \n", __FUNCTION__,__LINE__,"this fd is candidate do nothing rebind to event in only");
			//_net_ptr->epoll_control(sock, EPOLL_CTL_MOD, EPOLLIN);
		}
		else{	
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : unknow flag in peer_communication::handle_pkt_out\n");
			_logger_client_ptr->log_exit();
		}
	}
	
	return RET_OK;
}

//first write, then set fd to readable & excecption only
int peer_communication::handle_pkt_out_udp(int sock)	
{
	/*
	this part shows that the peer may connect to others (connect) or be connected by others (accept)
	it will only send PEER_CON protocol to candidates, if the fd is in the list. (the peer is join/rescue peer)
	And handle P2P structure.
	*/
	
	debug_printf("peer_communication::handle_pkt_out \n");
	
	map<int, struct fd_information *>::iterator map_udpfd_info_iter;
	
	if ((map_udpfd_info_iter = map_udpfd_info.find(sock)) != map_udpfd_info.end()) {
		debug_printf("map_udpfd_info_iter->second->flag: %d \n", map_udpfd_info_iter->second->role);
		if (map_udpfd_info_iter->second->role == PARENT_PEER) {
			//send peer con
			int ret;
			int	send_flag = 0;
			map<int, struct ioNonBlocking*>::iterator map_udpfd_NonBlockIO_iter;
			
			session_id_candidates_set_iter = session_id_candidates_set.find(map_udpfd_info_iter->second->session_id);
			if (session_id_candidates_set_iter == session_id_candidates_set.end()) {
				debug_printf("[ERROR] Not found sock %d in session_id_candidates_set", sock);
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] Not found in session_id_candidates_set", __FUNCTION__, __LINE__);
			}
			debug_printf("111 \n");
			map_udpfd_NonBlockIO_iter = map_udpfd_NonBlockIO.find(sock);
			if (map_udpfd_NonBlockIO_iter == map_udpfd_NonBlockIO.end()) {
				_pk_mgr_ptr->handle_error(MACCESS_ERROR, "[ERROR] can't  find map_udpfd_NonBlockIO_iter in peer_commiication", __FUNCTION__, __LINE__);
			}

			for (int i = 0; i < session_id_candidates_set_iter->second->peer_num; i++) {
				if (session_id_candidates_set_iter->second->list_info->level_info[i]->pid == map_udpfd_info_iter->second->pid) {
					
					ret = _peer_ptr->handle_connect_request_udp(sock, session_id_candidates_set_iter->second->list_info->level_info[i], session_id_candidates_set_iter->second->list_info->pid,&(map_udpfd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl));
					_pk_mgr_ptr->parents_table[session_id_candidates_set_iter->second->list_info->level_info[i]->pid] = PEER_CONNECTED;
					send_flag = 1;
				}
			}
			debug_printf("111 \n");
			if (send_flag == 0) {
				debug_printf("[ERROR] cannot find level info structure in peer_communication::handle_pkt_out");
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","[ERROR] cannot find level info structure in peer_communication::handle_pkt_out");
				_logger_client_ptr->log_exit();
				PAUSE
			}

			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "send CHNK_CMD_PEER_CON");
			debug_printf("ret: %d \n", ret);
			
			if (ret < 0) {
				debug_printf("[ERROR] handle_connect_request error");
				fd_close(sock);
				return RET_ERROR;
			}
			else if (map_udpfd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == RUNNING) {
				//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_MOD, UDT_EPOLL_IN | UDT_EPOLL_OUT);	
			}
			else if (map_udpfd_NonBlockIO_iter ->second->io_nonblockBuff.nonBlockingSendCtrl.recv_packet_state == READY){
				
				//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_MOD, UDT_EPOLL_IN | UDT_EPOLL_OUT);
				_net_udp_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_ptr));
				return RET_OK;
			}
		}
		else if(map_udpfd_info_iter->second->role == CHILD_PEER) {
			//do nothing rebind to event in only
			_log_ptr->write_log_format("s(u) s d s d \n", __FUNCTION__,__LINE__,"this fd", sock, "is candidate do nothing rebind to event in only, state", UDT::getsockstate(sock));
			//_net_udp_ptr->epoll_control(sock, EPOLL_CTL_MOD, UDT_EPOLL_IN);
		}
		else {	
			_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Unknown command", __FUNCTION__, __LINE__);
		}
	}
	else {
		_pk_mgr_ptr->handle_error(UNKNOWN, "[ERROR] Not found in map_udpfd_info", __FUNCTION__, __LINE__);
		PAUSE
	}
	
	return RET_OK;
}


void peer_communication::handle_pkt_error(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in peer_communication error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void peer_communication::handle_pkt_error_udp(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in peer_communication error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void peer_communication::handle_job_realtime()
{

}


void peer_communication::handle_job_timer()
{

}

void peer_communication::handle_sock_error(int sock, basic_class *bcptr){
}