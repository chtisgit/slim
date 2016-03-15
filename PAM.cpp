/* SLiM - Simple Login Manager
   Copyright (C) 2007 Martin Parm

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/
#include <string>
#include <iostream>
#include "PAM.h"

namespace PAM {

	Exception::~Exception()
	{}

	Auth_Exception::Auth_Exception(pam_handle_t* _pam_handle,
					const char* _func_name,
					int _errnum):
		Exception(_pam_handle, _func_name, _errnum)
	{}

	Cred_Exception::Cred_Exception(pam_handle_t* _pam_handle,
					const char* _func_name,
					int _errnum):
		Exception(_pam_handle, _func_name, _errnum)
	{}

	int Authenticator::_end()
	{
		int result = pam_end(pam_handle, last_result);
		pam_handle = nullptr;
		return result;
	}

	Authenticator::Authenticator(conversation* conv, void* data):
		pam_handle(nullptr),
		last_result(PAM_SUCCESS)
	{
		pam_conversation.conv = conv;
		pam_conversation.appdata_ptr = data;
	}

	Authenticator::~Authenticator()
	{
		if(pam_handle) _end();
	}

	void Authenticator::start(const char* service)
	{
		last_result = pam_start(service, NULL, &pam_conversation, &pam_handle);
		
		if(last_result != PAM_SUCCESS){
			throw Exception(pam_handle, "pam_start()", last_result);
		}
	}
	void Authenticator::start(const std::string& service)
	{
		start(service.c_str());
	}

	void Authenticator::end()
	{
		last_result = _end();

		if(last_result != PAM_SUCCESS){
			throw Exception(pam_handle, "pam_end()", last_result);
		}
	}

	void Authenticator::set_item(Authenticator::ItemType item, const void* value)
	{
		last_result = pam_set_item(pam_handle, item, value);

		if(last_result != PAM_SUCCESS){
			_end();
			throw Exception(pam_handle, "pam_set_item()", last_result);
		}
	}

	const void* Authenticator::get_item(Authenticator::ItemType item)
	{
		const void* data;
		last_result = pam_get_item(pam_handle, item, &data);

		if(last_result != PAM_SUCCESS && last_result != PAM_PERM_DENIED){
			/* PAM_PERM_DENIED -> The value of item was NULL */
			_end();
			throw Exception(pam_handle, "pam_get_item()", last_result);
		}

		return data;
	}

#ifdef __LIBPAM_VERSION
	void Authenticator::fail_delay(unsigned int micro_sec)
	{
		last_result = pam_fail_delay(pam_handle, micro_sec);

		if(last_result != PAM_SUCCESS){
			_end();
			throw Exception(pam_handle, "fail_delay()", last_result);
		}
	}
#endif

	void Authenticator::authenticate()
	{
		last_result = pam_authenticate(pam_handle, 0);

		switch(last_result){
		case PAM_USER_UNKNOWN:
		case PAM_MAXTRIES:
		case PAM_CRED_INSUFFICIENT:
		case PAM_AUTH_ERR:
			throw Auth_Exception(pam_handle, "pam_authentication()", last_result);

		case PAM_SUCCESS:
			break;

		case PAM_ABORT:
		case PAM_AUTHINFO_UNAVAIL:
		default:
			_end();
			throw Exception(pam_handle, "pam_authenticate()", last_result);
		}

		last_result = pam_acct_mgmt(pam_handle, PAM_SILENT);
		switch(last_result){
		/* The documentation and implementation of Linux PAM differs:
		   PAM_NEW_AUTHTOKEN_REQD is described in the documentation but
		   don't exists in the actual implementation. This issue needs
		   to be fixes at some point. */

		case PAM_AUTH_ERR:
		case PAM_PERM_DENIED:
			throw Auth_Exception(pam_handle, "pam_acct_mgmt()", last_result);

		case PAM_SUCCESS:
			break;
		
		/* case PAM_NEW_AUTHTOKEN_REQD: */
		case PAM_ACCT_EXPIRED:
		case PAM_USER_UNKNOWN:
		default:
			_end();
			throw Exception(pam_handle, "pam_acct_mgmt()", last_result);

		}
	}

	void Authenticator::open_session()
	{
		last_result = pam_setcred(pam_handle, PAM_ESTABLISH_CRED);

		switch(last_result){
		case PAM_CRED_EXPIRED:
		case PAM_USER_UNKNOWN:
			throw Cred_Exception(pam_handle, "pam_setcred()", last_result);

		case PAM_SUCCESS:
			break;

		case PAM_CRED_ERR:
		case PAM_CRED_UNAVAIL:
		default:
			_end();
			throw Exception(pam_handle, "pam_setcred()", last_result);

		}

		last_result = pam_open_session(pam_handle, 0);

		switch(last_result){
		/* The documentation and implementation of Linux PAM differs:
		   PAM_SESSION_ERROR is described in the documentation but
		   don't exists in the actual implementation. This issue needs
		   to be fixes at some point. */

		case PAM_SUCCESS:
			break;

		default:
		/* case PAM_SESSION_ERROR: */
			pam_setcred(pam_handle, PAM_DELETE_CRED);
			_end();
			throw Exception(pam_handle, "pam_open_session()", last_result);
		}
	}

	void Authenticator::close_session()
	{
		last_result = pam_close_session(pam_handle, 0);

		switch(last_result){
		/* The documentation and implementation of Linux PAM differs:
		   PAM_SESSION_ERROR is described in the documentation but
		   don't exists in the actual implementation. This issue needs
		   to be fixes at some point. */

		case PAM_SUCCESS:
			break;

		default:
		/* case PAM_SESSION_ERROR: */
			pam_setcred(pam_handle, PAM_DELETE_CRED);
			_end();
			throw Exception(pam_handle, "pam_close_session", last_result);
		}

		last_result = pam_setcred(pam_handle, PAM_DELETE_CRED);

		switch(last_result){
		case PAM_SUCCESS:
			break;

		case PAM_CRED_ERR:
		case PAM_CRED_UNAVAIL:
		case PAM_CRED_EXPIRED:
		case PAM_USER_UNKNOWN:
		default:
			_end();
			throw Exception(pam_handle, "pam_setcred()", last_result);
		}
	}

	void Authenticator::setenv(const std::string& key, const std::string& value)
	{
		auto name_value = key + "=" + value;
		last_result = pam_putenv(pam_handle, name_value.c_str());

		switch(last_result){
		case PAM_SUCCESS:
			break;

		case PAM_PERM_DENIED:
		case PAM_ABORT:
		case PAM_BUF_ERR:
#ifdef __LIBPAM_VERSION
		case PAM_BAD_ITEM:
#endif
		default:
			_end();
			throw Exception(pam_handle, "pam_putenv()", last_result);
		}
	}

	void Authenticator::delenv(const char* key)
	{
		last_result = pam_putenv(pam_handle, key);

		switch(last_result){
		case PAM_SUCCESS:
			break;

		case PAM_PERM_DENIED:
		case PAM_ABORT:
		case PAM_BUF_ERR:
#ifdef __LIBPAM_VERSION
		case PAM_BAD_ITEM:
#endif
		default:
			_end();
			throw Exception(pam_handle, "pam_putenv()", last_result);
		}
	}
	void Authenticator::delenv(const std::string& key)
	{
		delenv(key.c_str());
	}

	const char* Authenticator::getenv(const char* key)
	{
		return pam_getenv(pam_handle, key);
	}
	const char* Authenticator::getenv(const std::string& key)
	{
		return getenv(key.c_str());
	}

	char** Authenticator::getenvlist()
	{
		return pam_getenvlist(pam_handle);
	}
}

std::ostream& operator<<( std::ostream& os, const PAM::Exception& e)
{
	os << e.func_name << ": " << e.errstr;
	return os;
}
