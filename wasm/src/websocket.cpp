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

#include "websocket.hpp"

#include <cstring>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#include <exception>
#include <memory>

namespace rtc {

EmscriptenWebSocketCreateAttributes ws_attrs = {"", NULL, EM_TRUE};

EM_BOOL WebSocket::OpenCallback(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent,
                                void *userData) {
	WebSocket *w = static_cast<WebSocket *>(userData);
	if (w)
		w->triggerOpen();
	return 0;
}

EM_BOOL WebSocket::ErrorCallback(int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent,
                                 void *userData) {
	WebSocket *w = static_cast<WebSocket *>(userData);

	char errormsg[256];
	snprintf(errormsg, 256, "error(socket=%d, eventType=%d, userData=%p)\n", websocketEvent->socket,
	         eventType, userData);

	if (w)
		w->triggerError(errormsg);

	return 0;
}

EM_BOOL WebSocket::MessageCallback(int eventType, const EmscriptenWebSocketMessageEvent *e,
                                   void *userData) {
	WebSocket *w = static_cast<WebSocket *>(userData);

	if (w) {
		if (e->data) {
			if (e->numBytes >= 0) {
				auto b = reinterpret_cast<const byte *>(e->data);
				w->triggerMessage(binary(b, b + e->numBytes));
			} else {
				w->triggerMessage(string((char *)e->data));
			}
		} else {
			w->close();
			w->triggerClosed();
		}
	}
	return 0;
}

WebSocket::WebSocket() : mId(0), mConnected(false) {}

WebSocket::~WebSocket() { close(); }

void WebSocket::open(const string &url) {
	close();
	if (!emscripten_websocket_is_supported()) {
		throw std::runtime_error("WebSocket not supported");
	}

	strcpy((char *)ws_attrs.url, url.c_str());
	EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&ws_attrs);
	mId = ws;

	emscripten_websocket_set_onopen_callback(ws, this, OpenCallback);
	emscripten_websocket_set_onerror_callback(ws, this, ErrorCallback);
	emscripten_websocket_set_onmessage_callback(ws, this, MessageCallback);
}

void WebSocket::close() {
	mConnected = false;
	if (mId) {
		emscripten_websocket_close(mId, 0, 0);
		mId = 0;
	}
}

bool WebSocket::isOpen() const { return mConnected; }

bool WebSocket::isClosed() const { return mId == 0; }

bool WebSocket::send(message_variant message) {
	if (!mId)
		return false;

	return std::visit(
	    overloaded{[this](const binary &b) {
		               auto data = reinterpret_cast<const char *>(b.data());
		               return emscripten_websocket_send_binary(mId, (void *)data, b.size()) >= 0;
	               },
	               [this](const string &s) {
		               return emscripten_websocket_send_utf8_text(mId, s.c_str()) >= 0;
	               }},
	    std::move(message));
}

bool WebSocket::send(const byte *data, size_t size) {
	if (!mId)
		return false;

	return emscripten_websocket_send_binary(mId, (void *) data, size) >= 0;
}

void WebSocket::triggerOpen() {
	mConnected = true;
	Channel::triggerOpen();
}

} // namespace rtc
