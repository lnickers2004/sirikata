/*  Sirikata ransfer -- Content Transfer management system
 *  HTTPRequest.cpp
 *
 *  Copyright (c) 2008, Patrick Reiter Horn
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*  Created on: Dec 31, 2008 */
#include "util/Standard.hh"
#include "HTTPRequest.hpp"
#include "TransferData.hpp"
#include "util/ThreadSafeQueue.hpp"

#include <boost/thread.hpp>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <fcntl.h>

#include <curl/curl.h>

namespace Sirikata {
namespace Transfer {


size_t HTTPRequest::write_cb(unsigned char *data, size_t length, size_t count, HTTPRequest *handle) {
	return handle->write(data, length*count);
}

size_t HTTPRequest::read_cb(unsigned char *data, size_t length, size_t count, HTTPRequest *handle) {
	return handle->read(data, length*count);
}

static curl_off_t HTTPRequest_seek_cb(HTTPRequest *handle, curl_off_t curloffset, int origin) {
	cache_ssize_type offset = curloffset;
	switch (origin) {
	case SEEK_SET:
		handle->seek(handle->getUploadData().startbyte() + offset);
		break;
	case SEEK_CUR:
		handle->seek(handle->getUploadOffset() + offset);
		break;
	case SEEK_END:
		handle->seek(handle->getUploadData().endbyte() + offset);
		break;
	}
	return handle->getUploadOffset();
}

size_t HTTPRequest::header_cb(char *data, size_t length, size_t count, HTTPRequest *handle) {
	handle->gotHeader(std::string((unsigned char*)data, (unsigned char*)data + length*count));
	return length*count;
}

size_t HTTPRequest::write(const unsigned char *copyFrom, size_t length) {
	/*
	if (mOffset < mData->startbyte()) {
		return 0;
	}
	*/
	cache_ssize_type startByte = (mOffset - mData->startbyte());
	if (startByte < 0) {
		copyFrom -= startByte;
		length += (size_t)startByte;
		startByte = 0;
	}
	cache_usize_type totalNeeded = startByte + length;
	if (mData->length() < totalNeeded) {
		mData->setLength(totalNeeded, mRequestedRange.goesToEndOfFile());
	}
	// FIXME: do not adjust the length until actually copying data.
	unsigned char *copyTo = mData->writableData() + startByte;
	std::copy(copyFrom, copyFrom + length, copyTo);
	mOffset += length;
	return length;
}

template <class T>
class ZeroIterator {
	cache_usize_type mPos;
public:
	typedef std::forward_iterator_tag iterator_category;
	typedef T value_type;
	typedef cache_ssize_type difference_type;
	typedef T* pointer;
	typedef T& reference;
	ZeroIterator(cache_usize_type i=0)
		: mPos(i) {
	}
	inline cache_ssize_type operator-(const ZeroIterator &other) {
		return mPos - other.mPos;
	}
	inline bool operator!=(const ZeroIterator &other) {
		return other.mPos != mPos;
	}
	inline bool operator==(const ZeroIterator &other) {
		return other.mPos == mPos;
	}
	inline ZeroIterator& operator++() {
		++mPos;
		return *this;
	}
	inline T operator *() {
		return 0;
	}
};

size_t HTTPRequest::read(unsigned char *copyTo, size_t length) {
	if (mUploadData.empty()) {
		return 0;
	}
	if (mUploadOffset < mUploadData.startbyte()) {
		return 0;
	}
	size_t origLength = length;
//	cache_ssize_type startByte = (mUploadOffset - mUploadData->startbyte());
//	cache_usize_type totalNeeded = startByte + length;
	while (length > 0) {
		// Fixme: this loop should be a lot simpler. overwrite empty chunks (copyFrom==NULL) with NUL bytes.
		cache_usize_type chunkLength = 0;
		const unsigned char *copyFrom = mUploadData.dataAt(mUploadOffset, chunkLength);
		if (copyFrom == NULL && chunkLength == 0) {
			return origLength - length;
		}
		if (chunkLength > length) {
			chunkLength = length;
		}
		if (copyFrom == NULL) {
			std::copy(ZeroIterator<unsigned char>(), ZeroIterator<unsigned char>(chunkLength), copyTo);
		} else {
			std::copy(copyFrom, copyFrom + chunkLength, copyTo);
		}
		mUploadOffset += chunkLength;
		length -= chunkLength;
	}
	return origLength;
}

void HTTPRequest::gotHeader(const std::string &header) {
	std::string::size_type colon = header.find(':');
	if (colon == std::string::npos) {
		// either "HTTP/1.1 200 OK" or else a malformed header.
		std::istringstream istr(header);
		std::string ver;
		int code = 0; // assume a good response unless curl says otherwise.
		istr >> ver >> code;
		// **untested**
		if (code == 200 && (!mRequestedRange.goesToEndOfFile() || mRequestedRange.startbyte() != 0)) {
			mRequestedRange = Range(true); // Server is giving us the whole file.
			mData->setBase(0);
			mData->setLength(0, true);
		}
		return;
	}
	std::string headername = header.substr(0, colon);
	std::string headervalue = header.substr(colon+1);
	for (std::string::size_type i = 0; i < colon; i++) {
		headername[i] = tolower(headername[i]);
	}
	if (headername == "content-length") {
		std::istringstream istr(headervalue);
		cache_usize_type dataToReserve = 0;
		istr >> dataToReserve;
		if (dataToReserve) {
			// FIXME: only reserve() here -- do not adjust the actual length until copying data.
			mData->setLength(dataToReserve, mRequestedRange.goesToEndOfFile());
			SILOG(transfer,debug,"DOWNLOADING FILE RANGE " << (Range)(*mData) << " FROM "<<mURI);
		}
	}
	SILOG(transfer,insane,"Got header [" << headername << "] = " << headervalue);
}


#define RETRY_TIME 1.0

namespace {

	static boost::once_flag flag = BOOST_ONCE_INIT;
	CURLM *curlm = NULL;
	CURL *parent_easy_curl = NULL;

	//static ThreadSafeQueue<HTTPRequest*> requestQueue;
	struct CurlGlobals {
		boost::mutex http_lock;
		boost::thread *main_loop;
		volatile bool cleaningUp;

		boost::mutex fd_lock;
		bool woken;
		int waitFd;
		int wakeupFd;

		void initWakeupFd() {
			boost::lock_guard<boost::mutex> change_fd(fd_lock);
#ifdef _WIN32
			waitFd = wakeupFd = socket(0, 0, 0);
#else
			int pipeFds[2];
			pipe(pipeFds);
			waitFd = pipeFds[0];
			wakeupFd = pipeFds[1];
			fcntl(waitFd, F_SETFL, O_NONBLOCK);
			fcntl(wakeupFd, F_SETFL, O_NONBLOCK);
#endif
		}
		void destroyWakeupFd() {
			boost::lock_guard<boost::mutex> change_fd(fd_lock);
#ifdef _WIN32
			closesocket(waitFd);
			closesocket(wakeupFd);
#else
			close(waitFd);
			close(wakeupFd);
#endif
		}
		void doWakeup() {
			boost::lock_guard<boost::mutex> change_fd(fd_lock);
			if (woken == true) {
				return;
			}
			woken = true;
#ifdef _WIN32
			closesocket(waitFd);
#else
			write(wakeupFd, "", 1);
#endif
		}

		void handleWakeup() {
			boost::lock_guard<boost::mutex> change_fd(fd_lock);
			if (woken == false) {
				return;
			}
			woken = false;
#ifdef _WIN32
			waitFd = wakeupFd = socket(0, 0, 0);
#else
			char c;
			read(waitFd, &c, 1);
#endif
		}

		CurlGlobals() {
			cleaningUp = false;
			woken = false;
			initWakeupFd();
		}

		~CurlGlobals() {
			cleaningUp = true;

			if (main_loop) {
				doWakeup();
				main_loop->join();
				delete main_loop;
				destroyWakeupFd();

				if (parent_easy_curl) {
					curl_easy_cleanup(parent_easy_curl);
				}
				if (curlm) {
					curl_multi_cleanup(curlm);
				}
				curl_global_cleanup();
			}
		}
	} globals;
}

CURL *HTTPRequest::allocDefaultCurl() {
	CURL *mycurl = curl_easy_init( );
	curl_easy_setopt(mycurl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(mycurl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(mycurl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(mycurl, CURLOPT_WRITEFUNCTION, &HTTPRequest::write_cb);
	curl_easy_setopt(mycurl, CURLOPT_READFUNCTION, &HTTPRequest::read_cb);
	// only used for uploads.
	//curl_easy_setopt(mycurl, CURLOPT_SEEKFUNCTION, &HTTPRequest_seek_cb);
	curl_easy_setopt(mycurl, CURLOPT_HEADERFUNCTION, &HTTPRequest::header_cb);
	curl_easy_setopt(mycurl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(mycurl, CURLOPT_USERAGENT, "Sirikata/0.1 (" __DATE__ ")");
	curl_easy_setopt(mycurl, CURLOPT_CONNECTTIMEOUT, 5);
	// curl_easy_setopt(mycurl, CURLOPT_TIMEOUT, ...); // if the connection is tarpitted by a nasty firewall...

	// CURLOPT_DNS_USE_GLOBAL_CACHE: WARNING: this option is considered obsolete. Stop using it. Switch over to using the share interface instead! See CURLOPT_SHARE and curl_share_init(3).
	// From curl_multi_add_handle: If the easy handle is not set to use a shared (CURLOPT_SHARE) or global DNS cache (CURLOPT_DNS_USE_GLOBAL_CACHE), it will be made to use the DNS cache that is shared between all easy handles within the multi handle when curl_multi_add_handle(3) is called.

	/*
	if (OPTIONS->proxydata) {
		//CURLPROXY_HTTP, CURLPROXY_SOCKS4 (added in 7.15.2), CURLPROXY_SOCKS5, CURLPROXY_SOCKS4A (added in 7.18.0) and CURLPROXY_SOCKS5_HOSTNAME
		curl_easy_setopt(mycurl, CURLOPT_PROXY, OPTIONS->proxydata);
		curl_easy_setopt(mycurl, CURLOPT_PROXYTYPE, OPTIONS->proxytype);
		// CURLOPT_PROXYUSERNAME
		// CURLOPT_PROXYPASSWORD
		// CURLOPT_PROXYAUTH
	}
	*/
	return mycurl;
}
/*
ThreadSafeQueue shasumQueue;
void shasumLoop() {
	HTTPRequest *req;
	while (true) {
		req = shasumQueue.blockingPop();
		computeShasum(req->mData);
		...
	}
}
*/

void HTTPRequest::initCurl () {
	/*
	 * FIXME: don't use CURL_WIN32_INIT as we use sockets for things other than curl.
	 */
	/*
	 * FIXME: don't run curl_global_init here! The manpage says it *SHOULD* be run when
	 * only one thread is executing in the entire program (main() is best) as it is
	 * not thread-safe with a lot of libraries!
	 */
	curl_global_init(CURL_GLOBAL_ALL);
	curlm = curl_multi_init();
#ifndef _WIN32
	curl_multi_setopt(curlm, CURLMOPT_PIPELINING, 0);
	curl_multi_setopt(curlm, CURLMOPT_MAXCONNECTS, 8); // make higher if a server.
#endif
	// CURLOPT_PROGRESSFUNCTION may be useful for determining whether to timeout during an active connection.
	parent_easy_curl = allocDefaultCurl();

	globals.main_loop = new boost::thread(&curlLoop);

}

void HTTPRequest::curlLoop () {
	while (!globals.cleaningUp) {
		int numevents;

		while (true) {
			boost::unique_lock<boost::mutex> access_curl_handle(globals.http_lock);
			CURLMsg *transferMsg = curl_multi_info_read(curlm, &numevents);
			if (transferMsg == NULL) {
				break;
			}
			CURL *handle = transferMsg->easy_handle;
			void *dataptr;
			curl_easy_getinfo(handle, CURLINFO_PRIVATE, &dataptr);

			if (transferMsg->msg == CURLMSG_DONE) {
				HTTPRequest *request = (HTTPRequest*)dataptr;
				bool success;
				if (transferMsg->data.result == 0) {
					success = true;
				} else {
					// CURLE_RANGE_ERROR
					// CURLE_HTTP_RETURNED_ERROR
					SILOG(transfer,debug,"failed " << request << ": " <<
							curl_easy_strerror(transferMsg->data.result));
					success = false;
				}
				curl_easy_cleanup(handle);
				request->mCurlRequest = NULL; // handle is freed.

				CallbackFunc temp (request->mCallback);
				DenseDataPtr finishedData(request->getData());
				request->mCallback = nullCallback;

				std::tr1::shared_ptr<HTTPRequest> tempPtr (request->mPreventDeletion);
				request->mPreventDeletion.reset(); // won't be freed until tempPtr goes out of scope.

				access_curl_handle.unlock(); // UNLOCK: the callback may start a new HTTP transfer.
				temp(request, finishedData, success); // may delete request.

				// now tempPtr is allowed to free request.
				//access_curl_handle.lock();
			}
		}

		fd_set read_fd_set, write_fd_set, exc_fd_set;
		long timeout_ms=0;
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);
		int max_fd;
		{
			boost::lock_guard<boost::mutex> access_curl_handle(globals.http_lock);
			curl_multi_fdset(curlm,
					&read_fd_set,
					&write_fd_set,
					&exc_fd_set,
					&max_fd);
			curl_multi_timeout(curlm, &timeout_ms);
		}
#ifdef _WIN32
		FD_SET(globals.waitFd, &exc_fd_set);
#else
		FD_SET(globals.waitFd, &read_fd_set);
#endif
		if (globals.waitFd > max_fd) {
			max_fd = globals.waitFd;
		}
		if (timeout_ms >= 0) {
			struct timeval timeout_tv;
			timeout_tv.tv_usec = 1000*(timeout_ms%1000);
			timeout_tv.tv_sec = timeout_ms/1000;
			select(max_fd+1, &read_fd_set, &write_fd_set, &exc_fd_set, &timeout_tv);
		} else {
			select(max_fd+1, &read_fd_set, &write_fd_set, &exc_fd_set, NULL);
		}

		if (globals.woken || FD_ISSET(globals.waitFd, &read_fd_set)) {
			globals.handleWakeup();
		}

		{
			boost::lock_guard<boost::mutex> access_curl_handle(globals.http_lock);
			while (curl_multi_perform(curlm, &numevents) == CURLM_CALL_MULTI_PERFORM) {
				// do nothing...
			}
		}

	}
}

void HTTPRequest::abort() {
	if (mCurlRequest) {
		boost::lock_guard<boost::mutex> access_curl_handle(globals.http_lock);
		if (mCurlRequest) {
			curl_multi_remove_handle(curlm, mCurlRequest);
			curl_easy_cleanup(mCurlRequest);
			mCurlRequest = NULL;
		}
	}

	DenseDataPtr finishedData(getData());
	CallbackFunc temp (mCallback);
	mCallback = nullCallback;
	HTTPRequestPtr ptr = mPreventDeletion;
	mPreventDeletion.reset(); // may delete this.

	temp(this, finishedData, false);
	// ptr will now be deallocated.
}

HTTPRequest::~HTTPRequest() {
	if (mCurlRequest) {
		abort();
	}
	if (mHeaders) {
		curl_slist_free_all((struct curl_slist *)mHeaders);
	}
}
void HTTPRequest::initCurlHandle() {
	boost::call_once(&initCurl, flag);

	mCurlRequest = allocDefaultCurl();
	curl_easy_setopt(mCurlRequest, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(mCurlRequest, CURLOPT_READDATA, this);
	// only used for uploads.
	//curl_easy_setopt(mCurlRequest, CURLOPT_SEEKDATA, this);
	curl_easy_setopt(mCurlRequest, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(mCurlRequest, CURLOPT_PRIVATE, this);
	// c_str is guaranteed to remain valid because mURI is const.
	std::string uriString = this->mURI.toString();
	// Curl makes its own copy of the strings.
	curl_easy_setopt(mCurlRequest, CURLOPT_URL, uriString.c_str());

	std::ostringstream orangestring;
	bool nontrivialRange=false;
	if (mRequestedRange.startbyte() != 0) {
		nontrivialRange=true;
		orangestring << mRequestedRange.startbyte();
		mOffset = mRequestedRange.startbyte();
	}
	orangestring << '-';
	if (!mRequestedRange.goesToEndOfFile()) {
		if (mRequestedRange.length() <= 0) {
			// invalid range
			mCallback(this, DenseDataPtr(new DenseData(mRequestedRange)), true);
			return;
		}
		nontrivialRange=true;
		orangestring << (mRequestedRange.endbyte());
	}

	if (nontrivialRange) {
		assert(mRangeString.length()==0);//make sure this hasn't been called twice because then curl would point to a dangly string
		mRangeString = orangestring.str();
		curl_easy_setopt(mCurlRequest, CURLOPT_RANGE, mRangeString.c_str());
	}
}

void HTTPRequest::setPUTData(const SparseData &uploadData) {
	/*
	if (!uploadData.contiguous()) {
		// Should not happen here--if non-contiguous data makes it this far in the process, let it be filled with 0's.
		throw std::runtime_exception("non-contiguous data passed to PUT!");
	}
	*/
	mUploadData = uploadData;
	curl_easy_setopt(mCurlRequest, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(mCurlRequest, CURLOPT_INFILESIZE_LARGE, uploadData.endbyte());
}

void HTTPRequest::setDELETE() {
	curl_easy_setopt(mCurlRequest, CURLOPT_NOBODY, 1);
	if (mURI.proto() == "http" || mURI.proto() == "https") {
		curl_easy_setopt(mCurlRequest, CURLOPT_CUSTOMREQUEST, "DELETE");
	} else if (mURI.proto() == "ftp"){
		addHeader("DELE "+mURI.filename());
	} else if (mURI.proto() == "sftp") {
		addHeader("rm "+mURI.filename());
	}
}

void HTTPRequest::addHeader(const std::string &header) {
	mHeaders = (void*)curl_slist_append((struct curl_slist *)mHeaders, header.c_str());
}

void HTTPRequest::go(const HTTPRequestPtr &holdReference) {
	boost::lock_guard<boost::mutex> curl_lock (globals.http_lock);

	curl_multi_add_handle(curlm, mCurlRequest);
	mPreventDeletion = HTTPRequestPtr(holdReference);

	if (mHeaders) {
		if (mURI.proto() == "http" || mURI.proto() == "https") {
			curl_easy_setopt(mCurlRequest, CURLOPT_HTTPHEADER, mHeaders);
		} else if (mURI.proto() == "ftp" || mURI.proto() == "sftp") {
			curl_easy_setopt(mCurlRequest, CURLOPT_POSTQUOTE, mHeaders);
		}
	}
	globals.doWakeup();

	//requestQueue.push(this);
	// CURLOPT_REFERER
}


//Task::timer_queue.schedule(Task::AbsTime::now() + RETRY_TIME,
//	std::tr1::bind(&getData, this, fileId, requestedRange, callback, triesLeft-1));

}
}
