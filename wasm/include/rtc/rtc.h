/**
 * Copyright (c) 2019-2021 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RTC_C_API
#define RTC_C_API

#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifdef RTC_STATIC
#define RTC_C_EXPORT
#else // dynamic library
#ifdef _WIN32
#ifdef RTC_EXPORTS
#define RTC_C_EXPORT __declspec(dllexport) // building the library
#else
#define RTC_C_EXPORT __declspec(dllimport) // using the library
#endif
#else // not WIN32
#define RTC_C_EXPORT
#endif
#endif

#ifndef RTC_ENABLE_WEBSOCKET
#define RTC_ENABLE_WEBSOCKET 1
#endif

#ifndef RTC_ENABLE_MEDIA
#define RTC_ENABLE_MEDIA 1
#endif

#define RTC_DEFAULT_MTU 1280 // IPv6 minimum guaranteed MTU

#if RTC_ENABLE_MEDIA
#define RTC_DEFAULT_MAX_FRAGMENT_SIZE ((uint16_t)(RTC_DEFAULT_MTU - 12 - 8 - 40)) // SRTP/UDP/IPv6
#define RTC_DEFAULT_MAX_STORED_PACKET_COUNT 512
// Deprecated, do not use
#define RTC_DEFAULT_MAXIMUM_FRAGMENT_SIZE RTC_DEFAULT_MAX_FRAGMENT_SIZE
#define RTC_DEFAULT_MAXIMUM_PACKET_COUNT_FOR_NACK_CACHE RTC_DEFAULT_MAX_STORED_PACKET_COUNT
#endif

#ifdef _WIN32
#ifdef CAPI_STDCALL
#define RTC_API __stdcall
#else
#define RTC_API
#endif
#else // not WIN32
#define RTC_API
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RTC_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define RTC_DEPRECATED __declspec(deprecated)
#else
#define DEPRECATED
#endif

// libdatachannel C API

typedef enum {
	RTC_NEW = 0,
	RTC_CONNECTING = 1,
	RTC_CONNECTED = 2,
	RTC_DISCONNECTED = 3,
	RTC_FAILED = 4,
	RTC_CLOSED = 5
} rtcState;

typedef enum {
	RTC_ICE_NEW = 0,
	RTC_ICE_CHECKING = 1,
	RTC_ICE_CONNECTED = 2,
	RTC_ICE_COMPLETED = 3,
	RTC_ICE_FAILED = 4,
	RTC_ICE_DISCONNECTED = 5,
	RTC_ICE_CLOSED = 6
} rtcIceState;

typedef enum {
	RTC_GATHERING_NEW = 0,
	RTC_GATHERING_INPROGRESS = 1,
	RTC_GATHERING_COMPLETE = 2
} rtcGatheringState;

typedef enum {
	RTC_SIGNALING_STABLE = 0,
	RTC_SIGNALING_HAVE_LOCAL_OFFER = 1,
	RTC_SIGNALING_HAVE_REMOTE_OFFER = 2,
	RTC_SIGNALING_HAVE_LOCAL_PRANSWER = 3,
	RTC_SIGNALING_HAVE_REMOTE_PRANSWER = 4,
} rtcSignalingState;

typedef enum {
	RTC_CERTIFICATE_DEFAULT = 0, // ECDSA
	RTC_CERTIFICATE_ECDSA = 1,
	RTC_CERTIFICATE_RSA = 2,
} rtcCertificateType;

typedef enum {
	RTC_DIRECTION_UNKNOWN = 0,
	RTC_DIRECTION_SENDONLY = 1,
	RTC_DIRECTION_RECVONLY = 2,
	RTC_DIRECTION_SENDRECV = 3,
	RTC_DIRECTION_INACTIVE = 4
} rtcDirection;

typedef enum { RTC_TRANSPORT_POLICY_ALL = 0, RTC_TRANSPORT_POLICY_RELAY = 1 } rtcTransportPolicy;

#define RTC_ERR_SUCCESS 0
#define RTC_ERR_INVALID -1   // invalid argument
#define RTC_ERR_FAILURE -2   // runtime error
#define RTC_ERR_NOT_AVAIL -3 // element not available
#define RTC_ERR_TOO_SMALL -4 // buffer too small

typedef void(RTC_API *rtcDescriptionCallbackFunc)(int pc, const char *sdp, const char *type,
                                                  void *ptr);
typedef void(RTC_API *rtcCandidateCallbackFunc)(int pc, const char *cand, const char *mid,
                                                void *ptr);
typedef void(RTC_API *rtcStateChangeCallbackFunc)(int pc, rtcState state, void *ptr);
typedef void(RTC_API *rtcIceStateChangeCallbackFunc)(int pc, rtcIceState state, void *ptr);
typedef void(RTC_API *rtcGatheringStateCallbackFunc)(int pc, rtcGatheringState state, void *ptr);
typedef void(RTC_API *rtcSignalingStateCallbackFunc)(int pc, rtcSignalingState state, void *ptr);
typedef void(RTC_API *rtcDataChannelCallbackFunc)(int pc, int dc, void *ptr);
typedef void(RTC_API *rtcOpenCallbackFunc)(int id, void *ptr);
typedef void(RTC_API *rtcClosedCallbackFunc)(int id, void *ptr);
typedef void(RTC_API *rtcErrorCallbackFunc)(int id, const char *error, void *ptr);
typedef void(RTC_API *rtcMessageCallbackFunc)(int id, const char *message, int size, void *ptr);
typedef void *(RTC_API *rtcInterceptorCallbackFunc)(int pc, const char *message, int size,
                                                    void *ptr);
typedef void(RTC_API *rtcBufferedAmountLowCallbackFunc)(int id, void *ptr);
typedef void(RTC_API *rtcAvailableCallbackFunc)(int id, void *ptr);
typedef void(RTC_API *rtcPliHandlerCallbackFunc)(int tr, void *ptr);
typedef void(RTC_API *rtcRembHandlerCallbackFunc)(int tr, unsigned int bitrate, void *ptr);

// Log

// User pointer
RTC_C_EXPORT void rtcSetUserPointer(int id, void *ptr);
RTC_C_EXPORT void *rtcGetUserPointer(int i);

// PeerConnection

typedef struct {
	const char **iceServers;
	int iceServersCount;
} rtcConfiguration;

RTC_C_EXPORT int rtcCreatePeerConnection(const rtcConfiguration *config); // returns pc id
RTC_C_EXPORT int rtcClosePeerConnection(int pc);
RTC_C_EXPORT int rtcDeletePeerConnection(int pc);

RTC_C_EXPORT int rtcSetLocalDescriptionCallback(int pc, rtcDescriptionCallbackFunc cb);
RTC_C_EXPORT int rtcSetLocalCandidateCallback(int pc, rtcCandidateCallbackFunc cb);
RTC_C_EXPORT int rtcSetStateChangeCallback(int pc, rtcStateChangeCallbackFunc cb);
RTC_C_EXPORT int rtcSetIceStateChangeCallback(int pc, rtcIceStateChangeCallbackFunc cb);
RTC_C_EXPORT int rtcSetGatheringStateChangeCallback(int pc, rtcGatheringStateCallbackFunc cb);
RTC_C_EXPORT int rtcSetSignalingStateChangeCallback(int pc, rtcSignalingStateCallbackFunc cb);

RTC_C_EXPORT int rtcSetLocalDescription(int pc, const char *type);
RTC_C_EXPORT int rtcSetRemoteDescription(int pc, const char *sdp, const char *type);
RTC_C_EXPORT int rtcAddRemoteCandidate(int pc, const char *cand, const char *mid);

RTC_C_EXPORT int rtcGetLocalDescription(int pc, char *buffer, int size);
RTC_C_EXPORT int rtcGetRemoteDescription(int pc, char *buffer, int size);

RTC_C_EXPORT int rtcGetLocalDescriptionType(int pc, char *buffer, int size);
RTC_C_EXPORT int rtcGetRemoteDescriptionType(int pc, char *buffer, int size);

RTC_C_EXPORT int rtcGetLocalAddress(int pc, char *buffer, int size);
RTC_C_EXPORT int rtcGetRemoteAddress(int pc, char *buffer, int size);

RTC_C_EXPORT int rtcGetSelectedCandidatePair(int pc, char *local, int localSize, char *remote,
                                             int remoteSize);

RTC_C_EXPORT bool rtcIsNegotiationNeeded(int pc);

RTC_C_EXPORT int rtcGetMaxDataChannelStream(int pc);
RTC_C_EXPORT int rtcGetRemoteMaxMessageSize(int pc);

// DataChannel, and WebSocket common API

RTC_C_EXPORT int rtcSetOpenCallback(int id, rtcOpenCallbackFunc cb);
RTC_C_EXPORT int rtcSetClosedCallback(int id, rtcClosedCallbackFunc cb);
RTC_C_EXPORT int rtcSetErrorCallback(int id, rtcErrorCallbackFunc cb);
RTC_C_EXPORT int rtcSetMessageCallback(int id, rtcMessageCallbackFunc cb);
RTC_C_EXPORT int rtcSendMessage(int id, const char *data, int size);
RTC_C_EXPORT int rtcClose(int id);
RTC_C_EXPORT int rtcDelete(int id);
RTC_C_EXPORT bool rtcIsOpen(int id);
RTC_C_EXPORT bool rtcIsClosed(int id);

RTC_C_EXPORT int rtcMaxMessageSize(int id);
RTC_C_EXPORT int rtcGetBufferedAmount(int id); // total size buffered to send
RTC_C_EXPORT int rtcSetBufferedAmountLowThreshold(int id, int amount);
RTC_C_EXPORT int rtcSetBufferedAmountLowCallback(int id, rtcBufferedAmountLowCallbackFunc cb);

// DataChannel, and WebSocket common extended API

RTC_C_EXPORT int rtcGetAvailableAmount(int id); // total size available to receive
RTC_C_EXPORT int rtcSetAvailableCallback(int id, rtcAvailableCallbackFunc cb);
RTC_C_EXPORT int rtcReceiveMessage(int id, char *buffer, int *size);

// DataChannel

typedef struct {
	bool unordered;
	bool unreliable;
	unsigned int maxPacketLifeTime; // ignored if reliable
	unsigned int maxRetransmits;    // ignored if reliable
} rtcReliability;

typedef struct {
	rtcReliability reliability;
	const char *protocol; // empty string if NULL
	bool negotiated;
	bool manualStream;
	uint16_t stream; // numeric ID 0-65534, ignored if manualStream is false
} rtcDataChannelInit;

RTC_C_EXPORT int rtcSetDataChannelCallback(int pc, rtcDataChannelCallbackFunc cb);
RTC_C_EXPORT int rtcCreateDataChannel(int pc, const char *label); // returns dc id
RTC_C_EXPORT int rtcCreateDataChannelEx(int pc, const char *label,
                                        const rtcDataChannelInit *init); // returns dc id
RTC_C_EXPORT int rtcDeleteDataChannel(int dc);

RTC_C_EXPORT int rtcGetDataChannelStream(int dc);
RTC_C_EXPORT int rtcGetDataChannelLabel(int dc, char *buffer, int size);
RTC_C_EXPORT int rtcGetDataChannelProtocol(int dc, char *buffer, int size);
RTC_C_EXPORT int rtcGetDataChannelReliability(int dc, rtcReliability *reliability);

#if RTC_ENABLE_WEBSOCKET

// WebSocket

typedef struct {
	bool disableTlsVerification; // if true, don't verify the TLS certificate
	const char *proxyServer;     // only non-authenticated http supported for now
	const char **protocols;
	int protocolsCount;
	int connectionTimeoutMs; // in milliseconds, 0 means default, < 0 means disabled
	int pingIntervalMs;      // in milliseconds, 0 means default, < 0 means disabled
	int maxOutstandingPings; // 0 means default, < 0 means disabled
	int maxMessageSize;      // <= 0 means default
} rtcWsConfiguration;

RTC_C_EXPORT int rtcCreateWebSocket(const char *url); // returns ws id
RTC_C_EXPORT int rtcCreateWebSocketEx(const char *url, const rtcWsConfiguration *config);
RTC_C_EXPORT int rtcDeleteWebSocket(int ws);

RTC_C_EXPORT int rtcGetWebSocketRemoteAddress(int ws, char *buffer, int size);
RTC_C_EXPORT int rtcGetWebSocketPath(int ws, char *buffer, int size);

#endif

// Optional global preload and cleanup

RTC_C_EXPORT void rtcPreload(void);
RTC_C_EXPORT void rtcCleanup(void);

// SCTP global settings

typedef struct {
	int recvBufferSize;          // in bytes, <= 0 means optimized default
	int sendBufferSize;          // in bytes, <= 0 means optimized default
	int maxChunksOnQueue;        // in chunks, <= 0 means optimized default
	int initialCongestionWindow; // in MTUs, <= 0 means optimized default
	int maxBurst;                // in MTUs, 0 means optimized default, < 0 means disabled
	int congestionControlModule; // 0: RFC2581 (default), 1: HSTCP, 2: H-TCP, 3: RTCC
	int delayedSackTimeMs;       // in milliseconds, 0 means optimized default, < 0 means disabled
	int minRetransmitTimeoutMs;  // in milliseconds, <= 0 means optimized default
	int maxRetransmitTimeoutMs;  // in milliseconds, <= 0 means optimized default
	int initialRetransmitTimeoutMs; // in milliseconds, <= 0 means optimized default
	int maxRetransmitAttempts;      // number of retransmissions, <= 0 means optimized default
	int heartbeatIntervalMs;        // in milliseconds, <= 0 means optimized default
} rtcSctpSettings;

// Note: SCTP settings apply to newly-created PeerConnections only
RTC_C_EXPORT int rtcSetSctpSettings(const rtcSctpSettings *settings);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
