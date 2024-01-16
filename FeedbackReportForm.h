#pragma once

bool SendFeedbackReportForm(const char* mail_from_name, const char* mail_from_addr, const char* mail_subject, const char* mail_body,
	const char* mail_txt_attachment_filename, const char* mail_txt_attachment, const int mail_txt_attachment_len,
	const char* mail_bin_attachment_filename, const unsigned char* mail_bin_attachment, const int mail_bin_attachment_len);
