/**
 * Copyright (c) 2017-2022 Paul-Louis Ageneau
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "peerconnection.hpp"

#include <emscripten/emscripten.h>

#include <exception>
#include <iostream>
#include <stdexcept>

extern "C" {
extern int js_rtcCreatePeerConnection(const char **pUrls, const char **pUsernames,
                                   const char **pPasswords, int nIceServers);
extern void js_rtcDeletePeerConnection(int pc);
extern char *js_rtcGetLocalDescription(int pc);
extern char *js_rtcGetLocalDescriptionType(int pc);
extern char *js_rtcGetRemoteDescription(int pc);
extern char *js_rtcGetRemoteDescriptionType(int pc);
extern int js_rtcCreateDataChannel(int pc, const char *label, bool unordered, int maxRetransmits,
                                int maxPacketLifeTime);
extern void js_rtcSetDataChannelCallback(int pc, void (*dataChannelCallback)(int, void *));
extern void js_rtcSetLocalDescriptionCallback(int pc,
                                           void (*descriptionCallback)(const char *, const char *,
                                                                       void *));
extern void js_rtcSetLocalCandidateCallback(int pc, void (*candidateCallback)(const char *,
                                                                           const char *, void *));
extern void js_rtcSetStateChangeCallback(int pc, void (*stateChangeCallback)(int, void *));
extern void js_rtcSetIceStateChangeCallback(int pc, void (*iceStateChangeCallback)(int, void *));
extern void js_rtcSetGatheringStateChangeCallback(int pc,
                                               void (*gatheringStateChangeCallback)(int, void *));
extern void js_rtcSetSignalingStateChangeCallback(int pc,
                                               void (*signalingStateChangeCallback)(int, void *));
extern void js_rtcSetRemoteDescription(int pc, const char *sdp, const char *type);
extern void js_rtcAddRemoteCandidate(int pc, const char *candidate, const char *mid);
extern void js_rtcSetUserPointer(int i, void *ptr);
}

namespace rtc {

using std::function;
using std::vector;

void PeerConnection::DataChannelCallback(int dc, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerDataChannel(std::make_shared<DataChannel>(dc));
}

void PeerConnection::DescriptionCallback(const char *sdp, const char *type, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerLocalDescription(Description(sdp, type));
}

void PeerConnection::CandidateCallback(const char *candidate, const char *mid, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerLocalCandidate(Candidate(candidate, mid));
}

void PeerConnection::StateChangeCallback(int state, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerStateChange(static_cast<State>(state));
}

void PeerConnection::IceStateChangeCallback(int state, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerIceStateChange(static_cast<IceState>(state));
}

void PeerConnection::GatheringStateChangeCallback(int state, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerGatheringStateChange(static_cast<GatheringState>(state));
}

void PeerConnection::SignalingStateChangeCallback(int state, void *ptr) {
	PeerConnection *p = static_cast<PeerConnection *>(ptr);
	if (p)
		p->triggerSignalingStateChange(static_cast<SignalingState>(state));
}

PeerConnection::PeerConnection(const Configuration &config) {
	vector<string> urls;
	urls.reserve(config.iceServers.size());
	for (const IceServer &iceServer : config.iceServers) {
		string url;
		if (iceServer.type == IceServer::Type::Dummy) {
			url = iceServer.hostname;
		} else {
			string scheme =
			    iceServer.type == IceServer::Type::Turn
			        ? (iceServer.relayType == IceServer::RelayType::TurnTls ? "turns" : "turn")
			        : "stun";

			url += scheme + ":" + iceServer.hostname;

			if (iceServer.port != 0)
				url += string(":") + std::to_string(iceServer.port);

			if (iceServer.type == IceServer::Type::Turn &&
			    iceServer.relayType != IceServer::RelayType::TurnUdp)
				url += "?transport=tcp";
		}
		urls.push_back(url);
	}

	vector<const char *> url_ptrs;
	vector<const char *> username_ptrs;
	vector<const char *> password_ptrs;
	url_ptrs.reserve(config.iceServers.size());
	username_ptrs.reserve(config.iceServers.size());
	password_ptrs.reserve(config.iceServers.size());
	for (const string &s : urls)
		url_ptrs.push_back(s.c_str());
	for (const IceServer &iceServer : config.iceServers) {
		username_ptrs.push_back(iceServer.username.c_str());
		password_ptrs.push_back(iceServer.password.c_str());
	}
	mId = js_rtcCreatePeerConnection(url_ptrs.data(), username_ptrs.data(), password_ptrs.data(),
	                              config.iceServers.size());
	if (!mId)
		throw std::runtime_error("WebRTC not supported");

	js_rtcSetUserPointer(mId, this);
	js_rtcSetDataChannelCallback(mId, DataChannelCallback);
	js_rtcSetLocalDescriptionCallback(mId, DescriptionCallback);
	js_rtcSetLocalCandidateCallback(mId, CandidateCallback);
	js_rtcSetStateChangeCallback(mId, StateChangeCallback);
	js_rtcSetIceStateChangeCallback(mId, IceStateChangeCallback);
	js_rtcSetGatheringStateChangeCallback(mId, GatheringStateChangeCallback);
	js_rtcSetSignalingStateChangeCallback(mId, SignalingStateChangeCallback);
}

PeerConnection::~PeerConnection() { js_rtcDeletePeerConnection(mId); }

void PeerConnection::close() {}

PeerConnection::State PeerConnection::state() const { return mState; }

PeerConnection::IceState PeerConnection::iceState() const { return mIceState; }

PeerConnection::GatheringState PeerConnection::gatheringState() const { return mGatheringState; }

PeerConnection::SignalingState PeerConnection::signalingState() const { return mSignalingState; }

optional<Description> PeerConnection::localDescription() const {
	char *sdp = js_rtcGetLocalDescription(mId);
	char *type = js_rtcGetLocalDescriptionType(mId);
	if (!sdp || !type) {
		free(sdp);
		free(type);
		return std::nullopt;
	}
	Description description(sdp, type);
	free(sdp);
	free(type);
	return description;
}

optional<Description> PeerConnection::remoteDescription() const {
	char *sdp = js_rtcGetRemoteDescription(mId);
	char *type = js_rtcGetRemoteDescriptionType(mId);
	if (!sdp || !type) {
		free(sdp);
		free(type);
		return std::nullopt;
	}
	Description description(sdp, type);
	free(sdp);
	free(type);
	return description;
}

shared_ptr<DataChannel> PeerConnection::createDataChannel(const string &label,
                                                          DataChannelInit init) {
	const Reliability &reliability = init.reliability;
	if (reliability.maxPacketLifeTime && reliability.maxRetransmits)
		throw std::invalid_argument("Both maxPacketLifeTime and maxRetransmits are set");

	int maxRetransmits = reliability.maxRetransmits ? int(*reliability.maxRetransmits) : -1;
	int maxPacketLifeTime =
	    reliability.maxPacketLifeTime ? int(reliability.maxPacketLifeTime->count()) : -1;

	return std::make_shared<DataChannel>(js_rtcCreateDataChannel(
	    mId, label.c_str(), init.reliability.unordered, maxRetransmits, maxPacketLifeTime));
}

void PeerConnection::setLocalDescription(Description::Type type, LocalDescriptionInit init) {}

void PeerConnection::setRemoteDescription(const Description &description) {
	js_rtcSetRemoteDescription(mId, string(description).c_str(), description.typeString().c_str());
}

void PeerConnection::addRemoteCandidate(const Candidate &candidate) {
	js_rtcAddRemoteCandidate(mId, candidate.candidate().c_str(), candidate.mid().c_str());
}

void PeerConnection::onDataChannel(function<void(shared_ptr<DataChannel>)> callback) {
	mDataChannelCallback = callback;
}

void PeerConnection::onLocalDescription(function<void(const Description &)> callback) {
	mLocalDescriptionCallback = callback;
}

void PeerConnection::onLocalCandidate(function<void(const Candidate &)> callback) {
	mLocalCandidateCallback = callback;
}

void PeerConnection::onStateChange(function<void(State state)> callback) {
	mStateChangeCallback = callback;
}

void PeerConnection::onIceStateChange(function<void(IceState state)> callback) {
	mIceStateChangeCallback = callback;
}

void PeerConnection::onGatheringStateChange(function<void(GatheringState state)> callback) {
	mGatheringStateChangeCallback = callback;
}

void PeerConnection::onSignalingStateChange(function<void(SignalingState state)> callback) {
	mSignalingStateChangeCallback = callback;
}

void PeerConnection::triggerDataChannel(shared_ptr<DataChannel> dataChannel) {
	if (mDataChannelCallback)
		mDataChannelCallback(dataChannel);
}

void PeerConnection::triggerLocalDescription(const Description &description) {
	if (mLocalDescriptionCallback)
		mLocalDescriptionCallback(description);
}

void PeerConnection::triggerLocalCandidate(const Candidate &candidate) {
	if (mLocalCandidateCallback)
		mLocalCandidateCallback(candidate);
}

void PeerConnection::triggerStateChange(State state) {
	mState = state;
	if (mStateChangeCallback)
		mStateChangeCallback(state);
}

void PeerConnection::triggerIceStateChange(IceState state) {
	mIceState = state;
	if (mIceStateChangeCallback)
		mIceStateChangeCallback(state);
}

void PeerConnection::triggerGatheringStateChange(GatheringState state) {
	mGatheringState = state;
	if (mGatheringStateChangeCallback)
		mGatheringStateChangeCallback(state);
}

void PeerConnection::triggerSignalingStateChange(SignalingState state) {
	mSignalingState = state;
	if (mSignalingStateChangeCallback)
		mSignalingStateChangeCallback(state);
}
} // namespace rtc
