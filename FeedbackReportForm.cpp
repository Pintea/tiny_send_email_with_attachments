/*
Tiny 'send email with feedback/bugreport' - v1.1 - MIT License (i.e. can use it for whatever purpose)
Author: Mihai Gosa pintea@inthekillhouse.com
*/

#include <curl/curl.h>

#define NEW_LINE "\r\n"

static int strappend(char* dest, int destCurrLen, const int destMaxLen, const char* src)
{
	if (!src)
		return destCurrLen;

	for (; destCurrLen < (destMaxLen - 1) && *src != '\0'; ++destCurrLen)
		dest[destCurrLen] = *src++;
	dest[destCurrLen] = '\0';

	return destCurrLen;
}

static int Base64Encode_Len(int inputLen)
{
	return ((inputLen+2) / 3) * 4; /* 3-byte blocks to 4-byte */
}

static const char c_base64To[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int Base64Encode(const unsigned char * data, int len, char* output, int outLen)
{
	if (Base64Encode_Len(len) > outLen)
		return 0;

	const unsigned char* end = data + len;
	const unsigned char* in = data;
	char* pos = output;
	while (end - in >= 3) {
		*pos++ = c_base64To[in[0] >> 2];
		*pos++ = c_base64To[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = c_base64To[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = c_base64To[in[2] & 0x3f];
		in += 3;
	}

	if (end - in) {
		*pos++ = c_base64To[in[0] >> 2];
		if (end - in == 1) {
			*pos++ = c_base64To[(in[0] & 0x03) << 4];
			*pos++ = '=';
		}
		else {
			*pos++ = c_base64To[((in[0] & 0x03) << 4) |
				(in[1] >> 4)];
			*pos++ = c_base64To[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
	}
	return int(pos - output);
}

struct upload_status {
	const char* payload;
	size_t payload_len;
	size_t bytes_read;
};

static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
	upload_status *upload_ctx = (upload_status *)userp;
	const char *data;
	size_t room = size * nmemb;
 
	if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1))
		return 0;
 
	data = &upload_ctx->payload[upload_ctx->bytes_read];
 
	if (data)
	{
		size_t len = upload_ctx->payload_len - upload_ctx->bytes_read;
		if(room < len)
			len = room;
		memcpy(ptr, data, len);
		upload_ctx->bytes_read += len;
 
		return len;
	}
 
	return 0;
}

bool SendFeedbackReportForm(const char* mail_from_name, const char* mail_from_addr, const char* mail_subject, const char* mail_body, 
	const char* mail_txt_attachment_filename, const char* mail_txt_attachment, const int mail_txt_attachment_len,
	const char* mail_bin_attachment_filename, const unsigned char* mail_bin_attachment, const int mail_bin_attachment_len)
{
	const char* mail_url = "smtp://mail.yourwebsite.com:587"; // need a mail server with smtp no-ssl, doesn't work with gmail for example (in the current form)
	const char* mail_to = "devteam@yourwebsite.com"; // who's receiving the feedback
	const char* mail_proxy = "feedback@yourwebsite.com"; // email we're actually using to send this email from
	const char* mail_proxypass = "feedbackyourwebsitepassword"; // pass for email above

	// see https://www.rfc-editor.org/rfc/rfc5322
	const int maxlen = 1000 /* approximate max len for everything else */ 
		+ mail_txt_attachment_len 
		+ Base64Encode_Len(mail_bin_attachment_len);
	char* payload = new char[maxlen];
	int payload_len = 0;
	payload_len = strappend(payload, payload_len, maxlen, "To: <");
	payload_len = strappend(payload, payload_len, maxlen, mail_to);
	payload_len = strappend(payload, payload_len, maxlen, ">" NEW_LINE);

	if (mail_from_addr)
	{
		payload_len = strappend(payload, payload_len, maxlen, "From: ");
		payload_len = strappend(payload, payload_len, maxlen, mail_from_name);
		payload_len = strappend(payload, payload_len, maxlen, "<");
		payload_len = strappend(payload, payload_len, maxlen, mail_from_addr);
		payload_len = strappend(payload, payload_len, maxlen, ">" NEW_LINE);
	}

	if (mail_subject)
	{
		payload_len = strappend(payload, payload_len, maxlen, "Subject: ");
		payload_len = strappend(payload, payload_len, maxlen, mail_subject);
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);
	}

	if (mail_txt_attachment || mail_bin_attachment)
	{
		// send as MIME, see https://www.rfc-editor.org/rfc/rfc2045
		payload_len = strappend(payload, payload_len, maxlen, "MIME-Version: 1.0" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, "Content-Type: multipart/mixed;" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, " boundary=\"xxxxboundaryxxxx\"" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE "This is a multi-part message in MIME format." NEW_LINE);

		payload_len = strappend(payload, payload_len, maxlen, "--xxxxboundaryxxxx" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, "Content-Type: text/plain; charset=utf-8;" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, "Content-Transfer-Encoding: 8bit" NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);
		payload_len = strappend(payload, payload_len, maxlen, mail_body);
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);

		if (mail_txt_attachment)
		{
			payload_len = strappend(payload, payload_len, maxlen, "--xxxxboundaryxxxx" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, "Content-Type: text/plain; charset=utf-8;" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, "Content-Disposition: attachment; filename=\"");
			payload_len = strappend(payload, payload_len, maxlen, mail_txt_attachment_filename);
			payload_len = strappend(payload, payload_len, maxlen, "\"" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, mail_txt_attachment);
		}

		if (mail_bin_attachment)
		{
			payload_len = strappend(payload, payload_len, maxlen, "--xxxxboundaryxxxx" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, "Content-Type: application/octet-stream;" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, "Content-Transfer-Encoding: base64" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, "Content-Disposition: attachment; filename=\"");
			payload_len = strappend(payload, payload_len, maxlen, mail_bin_attachment_filename);
			payload_len = strappend(payload, payload_len, maxlen, "\"" NEW_LINE);
			payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);
			payload_len += Base64Encode(mail_bin_attachment, mail_bin_attachment_len, payload + payload_len, (maxlen - payload_len));
			payload[payload_len] = '\0';
		}

		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE "--xxxxboundaryxxxx--" NEW_LINE);
	}
	else
	{
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE /* empty line to divide headers from body, see RFC5322 */);
		payload_len = strappend(payload, payload_len, maxlen, mail_body);
		payload_len = strappend(payload, payload_len, maxlen, NEW_LINE);
	}

	// if calling on another thread, make sure to have called curl_global_init beforehand on main thread
	CURL* curl = curl_easy_init();
	if (!curl)
		return false;

	CURLcode res = CURLE_OK;
	curl_slist *recipients = NULL;
	upload_status upload_ctx;
	upload_ctx.payload = payload;
	upload_ctx.payload_len = payload_len;
	upload_ctx.bytes_read = 0;

	// need a mail server with smtp no-ssl, doesn't work with gmail for example (in the current form)
	res = curl_easy_setopt(curl, CURLOPT_URL, mail_url);
 
	/* Note that this option is not strictly required, omitting it will result
		* in libcurl sending the MAIL FROM command with empty sender data. All
		* autoresponses should have an empty reverse-path, and should be directed
		* to the address in the reverse-path which triggered them. Otherwise,
		* they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
		* details.
		*/
	if (mail_from_addr)
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from_addr); // <email> brackets auto-added by curl

	recipients = curl_slist_append(recipients, mail_to); // <email> brackets auto-added by curl
	res = curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

	res = curl_easy_setopt(curl, CURLOPT_USERNAME, mail_proxy);
	res = curl_easy_setopt(curl, CURLOPT_PASSWORD, mail_proxypass);
 
	/* We are using a callback function to specify the payload (the headers and
		* body of the message). You could just use the CURLOPT_READDATA option to
		* specify a FILE pointer to read from. */
	res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
	res = curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
	res = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	// send it (blocking!)
	res = curl_easy_perform(curl);
 
	// cleanup
	curl_slist_free_all(recipients);
	curl_easy_cleanup(curl);
	delete[] payload;
	return (res == CURLE_OK);
}
