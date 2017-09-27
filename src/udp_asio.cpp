/*!
 * @file udp_asio.cpp 基于boost::asio封装UDP通信
 * @version 0.1
 * @date May 23, 2017
 */
#include "udp_asio.h"
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace boost;

udp_session::udp_session(const int port, const bool who) {
	whoami_  = who;
	locport_ = port;
	bufrcv_.reset(new uint8_t[UDP_BUFF_SIZE]);
	re_open();
	if (who) async_receive();
}

udp_session::~udp_session() {
	close();
}

void udp_session::handle_receive(const boost::system::error_code& ec, const int n) {
	if (!ec || ec == asio::error::message_size) {
		bytercv_ = n;
		cbrcv_((const long) this, n);
		async_receive();
	}
}

void udp_session::handle_send(const boost::system::error_code& ec, const int n) {
	if (!ec) cbsnd_((const long) this, 0);
}

bool udp_session::re_open() {
	if (!is_open()) {
		sock_.reset(new udp::socket(keep_.get_service(), udp::endpoint(udp::v4(), locport_)));
		bytercv_   = 0;
	}

	return sock_.unique();
}

void udp_session::async_receive() {
	sock_->async_receive_from(asio::buffer(bufrcv_.get(), UDP_BUFF_SIZE), epremote_,
			boost::bind(&udp_session::handle_receive, this,
				asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void udp_session::set_peer(const char *peerIP, const int peerPort) {
	if (!whoami_ && re_open()) {
		udp::resolver resolver(keep_.get_service());
		udp::resolver::query query(peerIP, lexical_cast<std::string>(peerPort));
		epremote_ = *resolver.resolve(query);
		peerip_   = peerIP;
		peerport_ = peerPort;
		async_receive();
	}
}

void udp_session::close() {
	if (is_open()) sock_->close();
}

bool udp_session::is_open() {
	return sock_.unique() && sock_->is_open();
}

void udp_session::register_receive(const slottype &slot) {
	mutex_lock lck(mtxrcv_);
	if (!cbrcv_.empty()) cbrcv_.disconnect_all_slots();
	cbrcv_.connect(slot);
}

void udp_session::register_send(const slottype &slot) {
	mutex_lock lck(mtxsnd_);
	if (!cbsnd_.empty()) cbsnd_.disconnect_all_slots();
	cbsnd_.connect(slot);
}

const uint8_t *udp_session::read(int &n) {
	mutex_lock lck(mtxrcv_);
	n = bytercv_;
	bytercv_ = 0;
	return n == 0 ? NULL : bufrcv_.get();
}

const uint8_t *udp_session::block_read(int &n) {
	mutex_lock lck(mtxrcv_);
	int i(0);
	boost::chrono::milliseconds t(1);

	while (++i < 500 && !bytercv_) boost::this_thread::sleep_for(t);
	n = bytercv_;
	bytercv_ = 0;
	return n == 0 ? NULL : bufrcv_.get();
}

void udp_session::write(const void *data, const int n) {
	mutex_lock lck(mtxsnd_);

	sock_->async_send_to(asio::buffer(data, n), epremote_,
			boost::bind(&udp_session::handle_send, this,
					asio::placeholders::error, asio::placeholders::bytes_transferred));
}
