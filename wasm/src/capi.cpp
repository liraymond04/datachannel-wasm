/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc.h"
#include "rtc.hpp"

// #include "impl/internals.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>

using namespace rtc;
using namespace std::chrono_literals;
using std::chrono::milliseconds;

namespace {

std::unordered_map<int, shared_ptr<PeerConnection>> peerConnectionMap;
std::unordered_map<int, shared_ptr<DataChannel>> dataChannelMap;
#if RTC_ENABLE_WEBSOCKET
std::unordered_map<int, shared_ptr<WebSocket>> webSocketMap;
#endif
std::unordered_map<int, void *> userPointerMap;
std::mutex mutex;
int lastId = 0;

optional<void *> getUserPointer(int id) {
	std::lock_guard lock(mutex);
	auto it = userPointerMap.find(id);
	return it != userPointerMap.end() ? std::make_optional(it->second) : nullopt;
}

void setUserPointer(int i, void *ptr) {
	std::lock_guard lock(mutex);
	userPointerMap[i] = ptr;
}

shared_ptr<PeerConnection> getPeerConnection(int id) {
	std::lock_guard lock(mutex);
	if (auto it = peerConnectionMap.find(id); it != peerConnectionMap.end())
		return it->second;
	else
		throw std::invalid_argument("PeerConnection ID does not exist");
}

shared_ptr<DataChannel> getDataChannel(int id) {
	std::lock_guard lock(mutex);
	if (auto it = dataChannelMap.find(id); it != dataChannelMap.end())
		return it->second;
	else
		throw std::invalid_argument("DataChannel ID does not exist");
}

int emplacePeerConnection(shared_ptr<PeerConnection> ptr) {
	std::lock_guard lock(mutex);
	int pc = ++lastId;
	peerConnectionMap.emplace(std::make_pair(pc, ptr));
	userPointerMap.emplace(std::make_pair(pc, nullptr));
	return pc;
}

int emplaceDataChannel(shared_ptr<DataChannel> ptr) {
	std::lock_guard lock(mutex);
	int dc = ++lastId;
	dataChannelMap.emplace(std::make_pair(dc, ptr));
	userPointerMap.emplace(std::make_pair(dc, nullptr));
	return dc;
}

void erasePeerConnection(int pc) {
	std::lock_guard lock(mutex);
	if (peerConnectionMap.erase(pc) == 0)
		throw std::invalid_argument("Peer Connection ID does not exist");
	userPointerMap.erase(pc);
}

void eraseDataChannel(int dc) {
	std::lock_guard lock(mutex);
	if (dataChannelMap.erase(dc) == 0)
		throw std::invalid_argument("Data Channel ID does not exist");
	userPointerMap.erase(dc);
}

size_t eraseAll() {
	std::lock_guard lock(mutex);
	size_t count = dataChannelMap.size() + peerConnectionMap.size();
	dataChannelMap.clear();
	peerConnectionMap.clear();
#if RTC_ENABLE_WEBSOCKET
	count += webSocketMap.size();
	webSocketMap.clear();
#endif
	userPointerMap.clear();
	return count;
}

shared_ptr<Channel> getChannel(int id) {
	std::lock_guard lock(mutex);
	if (auto it = dataChannelMap.find(id); it != dataChannelMap.end())
		return it->second;
#if RTC_ENABLE_WEBSOCKET
	if (auto it = webSocketMap.find(id); it != webSocketMap.end())
		return it->second;
#endif
	throw std::invalid_argument("DataChannel, or WebSocket ID does not exist");
}

void eraseChannel(int id) {
	std::lock_guard lock(mutex);
	if (dataChannelMap.erase(id) != 0) {
		userPointerMap.erase(id);
		return;
	}
#if RTC_ENABLE_WEBSOCKET
	if (webSocketMap.erase(id) != 0) {
		userPointerMap.erase(id);
		return;
	}
#endif
	throw std::invalid_argument("DataChannel, or WebSocket ID does not exist");
}

int copyAndReturn(string s, char *buffer, int size) {
	if (!buffer)
		return int(s.size() + 1);

	if (size < int(s.size() + 1))
		return RTC_ERR_TOO_SMALL;

	std::copy(s.begin(), s.end(), buffer);
	buffer[s.size()] = '\0';
	return int(s.size() + 1);
}

int copyAndReturn(binary b, char *buffer, int size) {
	if (!buffer)
		return int(b.size());

	if (size < int(b.size()))
		return RTC_ERR_TOO_SMALL;

	auto data = reinterpret_cast<const char *>(b.data());
	std::copy(data, data + b.size(), buffer);
	return int(b.size());
}

template <typename T> int copyAndReturn(std::vector<T> b, T *buffer, int size) {
	if (!buffer)
		return int(b.size());

	if (size < int(b.size()))
		return RTC_ERR_TOO_SMALL;
	std::copy(b.begin(), b.end(), buffer);
	return int(b.size());
}

template <typename F> int wrap(F func) {
	try {
		return int(func());

	} catch (const std::invalid_argument &e) {
		return RTC_ERR_INVALID;
	} catch (const std::exception &e) {
		return RTC_ERR_FAILURE;
	}
}

#if RTC_ENABLE_WEBSOCKET

shared_ptr<WebSocket> getWebSocket(int id) {
	std::lock_guard lock(mutex);
	if (auto it = webSocketMap.find(id); it != webSocketMap.end())
		return it->second;
	else
		throw std::invalid_argument("WebSocket ID does not exist");
}

int emplaceWebSocket(shared_ptr<WebSocket> ptr) {
	std::lock_guard lock(mutex);
	int ws = ++lastId;
	webSocketMap.emplace(std::make_pair(ws, ptr));
	userPointerMap.emplace(std::make_pair(ws, nullptr));
	return ws;
}

void eraseWebSocket(int ws) {
	std::lock_guard lock(mutex);
	if (webSocketMap.erase(ws) == 0)
		throw std::invalid_argument("WebSocket ID does not exist");
	userPointerMap.erase(ws);
}

#endif

} // namespace

void rtcSetUserPointer(int i, void *ptr) { setUserPointer(i, ptr); }

void *rtcGetUserPointer(int i) { return getUserPointer(i).value_or(nullptr); }

int rtcCreatePeerConnection(const rtcConfiguration *config) {
	return wrap([config] {
		Configuration c;
		for (int i = 0; i < config->iceServersCount; ++i)
			c.iceServers.emplace_back(string(config->iceServers[i]));
		return emplacePeerConnection(std::make_shared<PeerConnection>(std::move(c)));
	});
}

int rtcClosePeerConnection(int pc) {
	return wrap([pc] {
		auto peerConnection = getPeerConnection(pc);
		peerConnection->close();
		return RTC_ERR_SUCCESS;
	});
}

int rtcDeletePeerConnection(int pc) {
	return wrap([pc] {
		auto peerConnection = getPeerConnection(pc);
		peerConnection->close();
		erasePeerConnection(pc);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetLocalDescriptionCallback(int pc, rtcDescriptionCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onLocalDescription([pc, cb](Description desc) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, string(desc).c_str(), desc.typeString().c_str(), *ptr);
			});
		else
			peerConnection->onLocalDescription(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetLocalCandidateCallback(int pc, rtcCandidateCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onLocalCandidate([pc, cb](Candidate cand) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, cand.candidate().c_str(), cand.mid().c_str(), *ptr);
			});
		else
			peerConnection->onLocalCandidate(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetStateChangeCallback(int pc, rtcStateChangeCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onStateChange([pc, cb](PeerConnection::State state) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, static_cast<rtcState>(state), *ptr);
			});
		else
			peerConnection->onStateChange(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetIceStateChangeCallback(int pc, rtcIceStateChangeCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onIceStateChange([pc, cb](PeerConnection::IceState state) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, static_cast<rtcIceState>(state), *ptr);
			});
		else
			peerConnection->onIceStateChange(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetGatheringStateChangeCallback(int pc, rtcGatheringStateCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onGatheringStateChange([pc, cb](PeerConnection::GatheringState state) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, static_cast<rtcGatheringState>(state), *ptr);
			});
		else
			peerConnection->onGatheringStateChange(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetSignalingStateChangeCallback(int pc, rtcSignalingStateCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onSignalingStateChange([pc, cb](PeerConnection::SignalingState state) {
				if (auto ptr = getUserPointer(pc))
					cb(pc, static_cast<rtcSignalingState>(state), *ptr);
			});
		else
			peerConnection->onSignalingStateChange(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetDataChannelCallback(int pc, rtcDataChannelCallbackFunc cb) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		if (cb)
			peerConnection->onDataChannel([pc, cb](shared_ptr<DataChannel> dataChannel) {
				int dc = emplaceDataChannel(dataChannel);
				if (auto ptr = getUserPointer(pc)) {
					rtcSetUserPointer(dc, *ptr);
					cb(pc, dc, *ptr);
				}
			});
		else
			peerConnection->onDataChannel(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetLocalDescription(int pc, const char *type) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);
		peerConnection->setLocalDescription(type ? Description::stringToType(type)
		                                         : Description::Type::Unspec);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetRemoteDescription(int pc, const char *sdp, const char *type) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (!sdp)
			throw std::invalid_argument("Unexpected null pointer for remote description");

		peerConnection->setRemoteDescription({string(sdp), type ? string(type) : ""});
		return RTC_ERR_SUCCESS;
	});
}

int rtcAddRemoteCandidate(int pc, const char *cand, const char *mid) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (!cand)
			throw std::invalid_argument("Unexpected null pointer for remote candidate");

		peerConnection->addRemoteCandidate({string(cand), mid ? string(mid) : ""});
		return RTC_ERR_SUCCESS;
	});
}

int rtcGetLocalDescription(int pc, char *buffer, int size) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (auto desc = peerConnection->localDescription())
			return copyAndReturn(string(*desc), buffer, size);
		else
			return RTC_ERR_NOT_AVAIL;
	});
}

int rtcGetRemoteDescription(int pc, char *buffer, int size) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (auto desc = peerConnection->remoteDescription())
			return copyAndReturn(string(*desc), buffer, size);
		else
			return RTC_ERR_NOT_AVAIL;
	});
}

int rtcGetLocalDescriptionType(int pc, char *buffer, int size) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (auto desc = peerConnection->localDescription())
			return copyAndReturn(desc->typeString(), buffer, size);
		else
			return RTC_ERR_NOT_AVAIL;
	});
}

int rtcGetRemoteDescriptionType(int pc, char *buffer, int size) {
	return wrap([&] {
		auto peerConnection = getPeerConnection(pc);

		if (auto desc = peerConnection->remoteDescription())
			return copyAndReturn(desc->typeString(), buffer, size);
		else
			return RTC_ERR_NOT_AVAIL;
	});
}

int rtcSetOpenCallback(int id, rtcOpenCallbackFunc cb) {
	return wrap([&] {
		auto channel = getChannel(id);
		if (cb)
			channel->onOpen([id, cb]() {
				if (auto ptr = getUserPointer(id))
					cb(id, *ptr);
			});
		else
			channel->onOpen(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetClosedCallback(int id, rtcClosedCallbackFunc cb) {
	return wrap([&] {
		auto channel = getChannel(id);
		if (cb)
			channel->onClosed([id, cb]() {
				if (auto ptr = getUserPointer(id))
					cb(id, *ptr);
			});
		else
			channel->onClosed(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetErrorCallback(int id, rtcErrorCallbackFunc cb) {
	return wrap([&] {
		auto channel = getChannel(id);
		if (cb)
			channel->onError([id, cb](string error) {
				if (auto ptr = getUserPointer(id))
					cb(id, error.c_str(), *ptr);
			});
		else
			channel->onError(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetMessageCallback(int id, rtcMessageCallbackFunc cb) {
	return wrap([&] {
		auto channel = getChannel(id);
		if (cb)
			channel->onMessage(
			    [id, cb](binary b) {
				    if (auto ptr = getUserPointer(id))
					    cb(id, reinterpret_cast<const char *>(b.data()), int(b.size()), *ptr);
			    },
			    [id, cb](string s) {
				    if (auto ptr = getUserPointer(id))
					    cb(id, s.c_str(), -int(s.size() + 1), *ptr);
			    });
		else
			channel->onMessage(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcSendMessage(int id, const char *data, int size) {
	return wrap([&] {
		auto channel = getChannel(id);

		if (!data && size != 0)
			throw std::invalid_argument("Unexpected null pointer for data");

		if (size >= 0) {
			auto b = reinterpret_cast<const byte *>(data);
			channel->send(binary(b, b + size));
		} else {
			channel->send(string(data));
		}
		return RTC_ERR_SUCCESS;
	});
}

int rtcClose(int id) {
	return wrap([&] {
		auto channel = getChannel(id);
		channel->close();
		return RTC_ERR_SUCCESS;
	});
}

int rtcDelete(int id) {
	return wrap([id] {
		auto channel = getChannel(id);
		channel->close();
		eraseChannel(id);
		return RTC_ERR_SUCCESS;
	});
}

bool rtcIsOpen(int id) {
	return wrap([id] { return getChannel(id)->isOpen() ? 0 : 1; }) == 0 ? true : false;
}

bool rtcIsClosed(int id) {
	return wrap([id] { return getChannel(id)->isClosed() ? 0 : 1; }) == 0 ? true : false;
}

int rtcGetBufferedAmount(int id) {
	return wrap([id] {
		auto channel = getChannel(id);
		return int(channel->bufferedAmount());
	});
}

int rtcSetBufferedAmountLowThreshold(int id, int amount) {
	return wrap([&] {
		auto channel = getChannel(id);
		channel->setBufferedAmountLowThreshold(size_t(amount));
		return RTC_ERR_SUCCESS;
	});
}

int rtcSetBufferedAmountLowCallback(int id, rtcBufferedAmountLowCallbackFunc cb) {
	return wrap([&] {
		auto channel = getChannel(id);
		if (cb)
			channel->onBufferedAmountLow([id, cb]() {
				if (auto ptr = getUserPointer(id))
					cb(id, *ptr);
			});
		else
			channel->onBufferedAmountLow(nullptr);
		return RTC_ERR_SUCCESS;
	});
}

int rtcCreateDataChannel(int pc, const char *label) {
	return rtcCreateDataChannelEx(pc, label, nullptr);
}

int rtcDeleteDataChannel(int dc) {
	return wrap([dc] {
		auto dataChannel = getDataChannel(dc);
		dataChannel->close();
		eraseDataChannel(dc);
		return RTC_ERR_SUCCESS;
	});
}

int rtcGetDataChannelLabel(int dc, char *buffer, int size) {
	return wrap([&] {
		auto dataChannel = getDataChannel(dc);
		return copyAndReturn(dataChannel->label(), buffer, size);
	});
}

int rtcGetDataChannelReliability(int dc, rtcReliability *reliability) {
	return wrap([&] {
		auto dataChannel = getDataChannel(dc);

		if (!reliability)
			throw std::invalid_argument("Unexpected null pointer for reliability");

		Reliability dcr = dataChannel->reliability();
		std::memset(reliability, 0, sizeof(*reliability));
		reliability->unordered = dcr.unordered;
		if(dcr.maxPacketLifeTime) {
			reliability->unreliable = true;
			reliability->maxPacketLifeTime = static_cast<unsigned int>(dcr.maxPacketLifeTime->count());
		} else if (dcr.maxRetransmits) {
			reliability->unreliable = true;
			reliability->maxRetransmits = *dcr.maxRetransmits;
		} else {
			reliability->unreliable = false;
		}
		return RTC_ERR_SUCCESS;
	});
}

#if RTC_ENABLE_WEBSOCKET

int rtcCreateWebSocket(const char *url) {
	return wrap([&] {
		auto webSocket = std::make_shared<WebSocket>();
		webSocket->open(url);
		return emplaceWebSocket(webSocket);
	});
}

int rtcDeleteWebSocket(int ws) {
	return wrap([&] {
		auto webSocket = getWebSocket(ws);
		webSocket->close();
		eraseWebSocket(ws);
		return RTC_ERR_SUCCESS;
	});
}

#endif

void rtcPreload() {
	try {
		rtc::Preload();
	} catch (const std::exception &e) {
	}
}

void rtcCleanup() {
	try {
		size_t count = eraseAll();
		if (count != 0) {
		}

	} catch (const std::exception &e) {
	}
}
