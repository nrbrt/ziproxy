/* cfgfile.c
 * Configuration file parsing functions.
 *
 * Ziproxy - the HTTP acceleration proxy
 * This code is under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c)2005-2014 Daniel Mealha Cabrita
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * ---------------------------------------------------------------------
 *
 * This code also contains portions under the following conditions:
 *
 * ---------------------------------------------------------------------
 * Copyright (c) Juraj Variny, 2004
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY JURAJ VARINY
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ---------------------------------------------------------------------
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

#include "qparser.h"
#include "image.h"
#include "jp2tools.h"
#include "cfgfile.h"
#include "urltables.h"
#include "simplelist.h"
#include "cttables.h"
#include "auth.h"
#include "log.h"


t_qp_bool DoGzip, UseContentLength, AllowLookCh, ProcessJPG, ProcessPNG, ProcessGIF, PreemptNameRes, PreemptNameResBC, TransparentProxy, ConventionalProxy, ProcessHTML, ProcessCSS, ProcessJS, ProcessHTML_CSS, ProcessHTML_JS, ProcessHTML_tags, ProcessHTML_text, ProcessHTML_PRE, ProcessHTML_NoComments, ProcessHTML_TEXTAREA, AllowMethodCONNECT, OverrideAcceptEncoding, DecompressIncomingGzipData, WA_MSIE_FriendlyErrMsgs, InterceptCrashes, TOSMarking, TOSMarkAsDiffCTAlsoXST, URLReplaceDataCTListAlsoXST, LosslessCompressCTAlsoXST, ConvertToGrayscale;

int Port, NextPort, ConnTimeout, MaxSize, PreemptNameResMax, MaxUncompressedGzipRatio, MinUncompressedGzipStreamEval, MaxUncompressedImageRatio;
int ZiproxyTimeout; // deprecated
int ImageQuality[4];
int AlphaRemovalMinAvgOpacity;
int JP2ImageQuality[4];
int RestrictOutPortHTTP [MAX_RESTRICTOUTPORTHTTP_LEN];
int RestrictOutPortCONNECT [MAX_RESTRICTOUTPORTCONNECT_LEN];
int RestrictOutPortHTTP_len;
int RestrictOutPortCONNECT_len;
char *ServHost, *OnlyFrom, *NextProxy, *ServUrl;
char **Compressible;	// deprecated
char *LosslessCompressCT;
char *CustomError400, *CustomError403, *CustomError404, *CustomError407, *CustomError408, *CustomError409, *CustomError500, *CustomError503;
char *Address;
char *AccessLogFileName; // deprecated
char *DebugLog;
char *AccessLog;
char *ErrorLog;
char *RedefineUserAgent;
char *URLNoProcessing;
char *URLReplaceData;
char *URLReplaceDataCT;
char *URLDeny;
char *tmpBindOutgoingExList;
char *tmpBindOutgoingExAddr;
t_st_strtable *BindOutgoingExList;
in_addr_t *BindOutgoingExAddr;
in_addr_t *BindOutgoing;
int BindOutgoing_entries;

int TOSFlagsDefault;
int TOSFlagsDiff;
char *TOSMarkAsDiffURL;
char *TOSMarkAsDiffCT;
int TOSMarkAsDiffSizeBT;

int MaxActiveUserConnections;

char *PIDFile;
char *cli_PIDFile;

char *RunAsUser;
char *cli_RunAsUser;
char *RunAsGroup;
char *cli_RunAsGroup;

int AuthMode;
char *AuthPasswdFile;
#ifdef SASL
char *AuthSASLConfPath;
#endif

#ifdef JP2K
t_qp_bool ProcessJP2, ForceOutputNoJP2, ProcessToJP2, AnnounceJP2Capability, JP2OutRequiresExpCap;
int JP2Colorspace_cfg;
t_color_space JP2Colorspace;	// 0=RGB 1=YUV
int JP2Upsampler_cfg;
t_upsampler JP2Upsampler;	// upsampler method: 0-linear, 1-lanczos
int JP2BitResYA[8];	// 4x qualities 2 components: YA YA YA YA
int JP2BitResRGBA[16];	// 4x qualities 4 components: RGBA RGBA RGBA RGBA
int JP2BitResYUVA[16];	// 4x qualities 4 components: YUVA YUVA YUVA YUVA
int JP2CSamplingYA[32];	// 4x qualities 2 components 4 settings: YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh
int JP2CSamplingRGBA[64];	// 4x qualities 4 components 4 settings: RxRyRwRh GxGyGwGh BxByBwBh AxAyAwAh ... (4 times)
int JP2CSamplingYUVA[64];	// 4x qualities 4 components 4 settings: YxYyYwYh UxUyUwUh VxVyVwVh AxAyAwAh ... (4 times)
#endif

#ifndef DefaultCfgLocation
char DefaultCfgLocation[] = "/etc/ziproxy/ziproxy.conf";
#endif

int LoadParmMatrix(int *destarray, t_qp_configfile *conf_handler, const char *conf_key, const int *dvalues, const int min_entries, const int max_entries, const int min_value, const int max_value);
const t_st_strtable *LoadParmMatrixStr(t_qp_configfile *conf_handler, const char *conf_key, const char **dvalues, int dvalues_len, const int min_entries, const int max_entries);
const t_ut_urltable *load_URLtable (const char *urltable_filename);
const t_ct_cttable *LoadParmMatrixCT (t_qp_configfile *conf_handler, const char *conf_key, const char **dvalues, int dvalues_len, const int min_entries, const int max_entries, const int subtype_add_x_prefix);
int check_int_ranges (const char *conf_key, const int inval, const int vlow, const int vhigh);
int check_int_minimum (const char *conf_key, const int inval, const int vlow);
int check_directory (const char *conf_key, const char *dirname);

const t_ut_urltable *tos_markasdiff_url;
const t_ct_cttable *tos_maskasdiff_ct;

const t_ut_urltable *urltable_noprocessing;
const t_ut_urltable *urltable_replacedata;
const t_ut_urltable *urltable_replacedatact;
const t_ct_cttable *urltable_replacedatactlist;
const t_ut_urltable *urltable_deny;

const t_ct_cttable *lossless_compress_ct;

/* dump parsing errors (if any) to stderr
 * returns: ==0: no errors, !=0 errors */
int dump_errors (t_qp_configfile *conf_handler)
{
	int counter = 0;
	const char *conf_key;
	t_qp_error error_code;
	int has_errors = 0;

	while (qp_get_error (conf_handler, counter++, &conf_key, &error_code)) {
		switch (error_code) {
		case QP_ERROR_SYNTAX:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Incorrect syntax of parameter -> %s\n", conf_key);
			break;
		case QP_ERROR_MISSING_CONFKEY:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Required parameter missing -> %s\n", conf_key);
			break;
		case QP_ERROR_INVALID_PARM:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid parameter -> %s\n", conf_key);
			break;
		case QP_ERROR_DUPLICATED:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Parameter defined twice or more -> %s\n", conf_key);
			break;
		case QP_ERROR_OBSOLETE:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"OBSOLETE parameter (see documentation for details) -> %s\n", conf_key);
			break;
		case QP_ERROR_NOT_SUPPORTED:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"UNSUPPORTED parameter (disabled during compilation) -> %s\n", conf_key);
			break;
		case QP_ERROR_MORE_ERRORS:
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"More configuration errors present, but not displayed.");
			break;
		default:
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Parser bug.");
 		}

		has_errors = 1;
	}

	return (has_errors);
}

int ReadCfgFile(char * cfg_file)
{
	t_qp_configfile *conf_handler;
	int i, n;
#ifdef JP2K
	const int DefaultJP2ImageQuality[] = { 30, 25, 25, 20 };
	const int DefaultJP2BitResYA[] = { 6, 4, 7, 5, 8, 6, 8, 6 };
	const int DefaultJP2BitResRGBA[] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
	const int DefaultJP2BitResYUVA[] = { 6, 5, 5, 4, 7, 6, 6, 5, 8, 7, 7, 6, 8, 8, 8, 6 };
	const int DefaultJP2CSamplingYA[] = { 	0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 2, 2, \
						0, 0, 1, 1,  0, 0, 2, 2 };
	const int DefaultJP2CSamplingRGBA[] = {	0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1 };
	const int DefaultJP2CSamplingYUVA[] = {	0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 1, 2,  0, 0, 2, 1,  0, 0, 1, 1, \
						0, 0, 1, 1,  0, 0, 2, 2,  0, 0, 2, 1,  0, 0, 2, 2, \
						0, 0, 1, 1,  0, 0, 2, 2,  0, 0, 2, 2,  0, 0, 2, 2 };

	
#endif
	const int DefaultImageQuality[] = { 30, 25, 25, 20 };

	Port = NextPort = 8080;
	ConnTimeout = 90;
	MaxSize = 1048576;
	PreemptNameResMax = 50;
	MaxUncompressedGzipRatio = 2000;
	MinUncompressedGzipStreamEval = 10000000;
	MaxUncompressedImageRatio = 500;
	PreemptNameRes = QP_FALSE;
	DoGzip = UseContentLength = ProcessJPG = ProcessPNG = ProcessGIF = QP_TRUE;
	ProcessHTML = ProcessCSS = ProcessJS = QP_FALSE;
	WA_MSIE_FriendlyErrMsgs = QP_TRUE;
	ProcessHTML_CSS = ProcessHTML_JS = ProcessHTML_tags = ProcessHTML_text = ProcessHTML_PRE = ProcessHTML_NoComments = ProcessHTML_TEXTAREA = QP_TRUE;
	AllowLookCh = PreemptNameResBC = TransparentProxy = QP_FALSE;
	ConvertToGrayscale = QP_FALSE;
	ConventionalProxy = QP_TRUE;
	ServHost = ServUrl = OnlyFrom = NextProxy = NULL;
	Compressible = NULL;	// deprecated
	lossless_compress_ct = NULL;
	LosslessCompressCTAlsoXST = QP_TRUE;
	CustomError400 = CustomError403 = CustomError404 = CustomError407 = CustomError408 = CustomError409 = CustomError500 = CustomError503 = NULL;
	Address = NULL;
	DebugLog = NULL;
	AccessLog = NULL;
	ErrorLog = NULL;
	AllowMethodCONNECT = QP_TRUE;
	OverrideAcceptEncoding = QP_TRUE;
	RedefineUserAgent = NULL;
	DecompressIncomingGzipData = QP_TRUE;
	BindOutgoing = NULL;
	BindOutgoing_entries = 0;
	tmpBindOutgoingExAddr = NULL;
	tmpBindOutgoingExList = NULL;
	BindOutgoingExAddr = NULL;
	BindOutgoingExList = NULL;
	URLNoProcessing = NULL;
	URLReplaceData = NULL;
	URLReplaceDataCT = NULL;
	URLReplaceDataCTListAlsoXST = QP_TRUE;
	URLDeny = NULL;
	urltable_noprocessing = NULL;
	urltable_replacedata = NULL;
	urltable_replacedatact = NULL;
	urltable_replacedatactlist = NULL;
	InterceptCrashes = QP_FALSE;
	TOSMarking = QP_FALSE;
	TOSFlagsDefault = -1;
	TOSFlagsDiff = -1;
	TOSMarkAsDiffURL = NULL;
	TOSMarkAsDiffSizeBT = -1;
	tos_markasdiff_url = NULL;
	tos_maskasdiff_ct = NULL;
	TOSMarkAsDiffCTAlsoXST = QP_TRUE;
	MaxActiveUserConnections = 0;
	PIDFile = cli_PIDFile;		/* defaults to CLI parameter, if specified */
	RunAsUser = cli_RunAsUser;	/* defaults to CLI parameter, if specified */
	RunAsGroup = cli_RunAsGroup;	/* defaults to CLI parameter, if specified */
	AuthMode = AUTH_NONE;
	AuthPasswdFile = NULL;
	AlphaRemovalMinAvgOpacity = 1000000;
#ifdef SASL
	AuthSASLConfPath = NULL;
#endif
#ifdef JP2K
	ProcessJP2 = QP_FALSE;
	ForceOutputNoJP2 = QP_FALSE;
	ProcessToJP2 = QP_FALSE;
	AnnounceJP2Capability = QP_FALSE;
	JP2OutRequiresExpCap = QP_FALSE;
	JP2Colorspace_cfg = 1;	// defaults to YUV
	JP2Upsampler = 0;	// defaults to linear
#endif

	if ((conf_handler = qp_init (cfg_file, QP_INITFLAG_IGNORECASE)) == NULL) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG, "Unable to open config file '%s'\n", cfg_file);
		return (1);
	}

	qp_getconf_str (conf_handler, "ViaServer", &ServHost, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "ServerUrl", &ServUrl, QP_FLAG_NONE);
        qp_getconf_int (conf_handler, "Port", &Port, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "OnlyFrom", &OnlyFrom, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "DebugLog", &DebugLog, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "AccessLog", &AccessLog, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "ErrorLog", &ErrorLog, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "NextProxy", &NextProxy, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "NextPort", &NextPort, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "ConnTimeout", &ConnTimeout, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "ZiproxyTimeout", &ZiproxyTimeout, QP_FLAG_NONE); // deprecated
	qp_getconf_bool (conf_handler, "Gzip", &DoGzip, QP_FLAG_NONE);
	qp_getconf_array_str (conf_handler, "Compressible", 0, NULL, QP_FLAG_NONE);	// deprecated
	qp_getconf_array_str (conf_handler, "LosslessCompressCT", 0, NULL, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "LosslessCompressCTAlsoXST", &LosslessCompressCTAlsoXST, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "MaxSize", &MaxSize, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "UseContentLength", &UseContentLength, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "ImageQuality", 0, NULL, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "AlphaRemovalMinAvgOpacity", &AlphaRemovalMinAvgOpacity, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2ImageQuality", 0, NULL, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "AllowLookChange", &AllowLookCh, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ConvertToGrayscale", &ConvertToGrayscale, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessJPG", &ProcessJPG, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessPNG", &ProcessPNG, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessGIF", &ProcessGIF, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "PreemptNameRes", &PreemptNameRes, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "PreemptNameResMax", &PreemptNameResMax, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "PreemptNameResBC", &PreemptNameResBC, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "TransparentProxy", &TransparentProxy, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ConventionalProxy", &ConventionalProxy, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError400", &CustomError400, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError403", &CustomError403, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError404", &CustomError404, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError407", &CustomError407, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError408", &CustomError408, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError409", &CustomError409, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError500", &CustomError500, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "CustomError503", &CustomError503, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "Address", &Address, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "AccessLogFileName", &AccessLogFileName, QP_FLAG_NONE);	// deprecated
	qp_getconf_bool (conf_handler, "ProcessHTML", &ProcessHTML, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessCSS", &ProcessCSS, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessJS", &ProcessJS, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_CSS", &ProcessHTML_CSS, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_JS", &ProcessHTML_JS, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_tags", &ProcessHTML_tags, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_text", &ProcessHTML_text, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_PRE", &ProcessHTML_PRE, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_NoComments", &ProcessHTML_NoComments, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessHTML_TEXTAREA", &ProcessHTML_TEXTAREA, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "AllowMethodCONNECT", &AllowMethodCONNECT, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "OverrideAcceptEncoding", &OverrideAcceptEncoding, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "MaxUncompressedGzipRatio", &MaxUncompressedGzipRatio, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "MaxUncompressedImageRatio", &MaxUncompressedImageRatio, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "RedefineUserAgent", &RedefineUserAgent, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "DecompressIncomingGzipData", &DecompressIncomingGzipData, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "WA_MSIE_FriendlyErrMsgs", &WA_MSIE_FriendlyErrMsgs, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "InterceptCrashes", &InterceptCrashes, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "MinUncompressedGzipStreamEval", &MinUncompressedGzipStreamEval, QP_FLAG_NONE);
	qp_getconf_array_str (conf_handler, "BindOutgoing", 0, NULL, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "BindOutgoingExAddr", &tmpBindOutgoingExAddr, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "BindOutgoingExList", &tmpBindOutgoingExList, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "URLNoProcessing", &URLNoProcessing, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "URLReplaceData", &URLReplaceData, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "URLReplaceDataCT", &URLReplaceDataCT, QP_FLAG_NONE);
	qp_getconf_array_str (conf_handler, "URLReplaceDataCTList", 0, NULL, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "URLReplaceDataCTListAlsoXST", &URLReplaceDataCTListAlsoXST, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "URLDeny", &URLDeny, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "RestrictOutPortHTTP", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "RestrictOutPortCONNECT", 0, NULL, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "TOSMarking", &TOSMarking, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "TOSFlagsDefault", &TOSFlagsDefault, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "TOSFlagsDiff", &TOSFlagsDiff, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "TOSMarkAsDiffURL", &TOSMarkAsDiffURL, QP_FLAG_NONE);
	qp_getconf_array_str (conf_handler, "TOSMarkAsDiffCT", 0, NULL, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "TOSMarkAsDiffCTAlsoXST", &TOSMarkAsDiffCTAlsoXST, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "TOSMarkAsDiffSizeBT", &TOSMarkAsDiffSizeBT, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "MaxActiveUserConnections", &MaxActiveUserConnections, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "PIDFile", &PIDFile, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "RunAsUser", &RunAsUser, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "RunAsGroup", &RunAsGroup, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "AuthMode", &AuthMode, QP_FLAG_NONE);
	qp_getconf_str (conf_handler, "AuthPasswdFile", &AuthPasswdFile, QP_FLAG_NONE);
#ifdef SASL
	qp_getconf_str (conf_handler, "AuthSASLConfPath", &AuthSASLConfPath, QP_FLAG_NONE);
#else
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "AuthSASLConfPath"); /* no SASL support */
#endif
#ifdef JP2K
	qp_getconf_bool (conf_handler, "ProcessJP2", &ProcessJP2, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ForceOutputNoJP2", &ForceOutputNoJP2, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "ProcessToJP2", &ProcessToJP2, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "AnnounceJP2Capability", &AnnounceJP2Capability, QP_FLAG_NONE);
	qp_getconf_bool (conf_handler, "JP2OutRequiresExpCap", &JP2OutRequiresExpCap, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "JP2Colorspace", &JP2Colorspace_cfg, QP_FLAG_NONE);
	qp_getconf_int (conf_handler, "JP2Upsampler", &JP2Upsampler_cfg, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2BitResYA", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2BitResRGBA", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2BitResYUVA", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2CSamplingYA", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2CSamplingRGBA", 0, NULL, QP_FLAG_NONE);
	qp_getconf_array_int (conf_handler, "JP2CSamplingYUVA", 0, NULL, QP_FLAG_NONE);
#else
	/* JP2K not enabled, but we still recognize those parameters
           in order to provide a more informative error message */
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "ProcessJP2");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "ForceOutputNoJP2");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "ProcessToJP2");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "AnnounceJP2Capability");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2OutRequiresExpCap");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2Colorspace");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2Upsampler");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2BitResYA");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2BitResRGBA");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2BitResYUVA");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2CSamplingYA");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2CSamplingRGBA");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "JP2CSamplingYUVA");
#endif
#ifdef EN_NAMESERVERS
	qp_getconf_array_str (conf_handler, "Nameservers", 0, NULL, QP_FLAG_NONE);
#else
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_NOT_SUPPORTED, "Nameservers");
#endif

	/* list of OBSOLETE parameters, but we still recognize those parameters
	   in order to provide a more informative error message */
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "WhereZiproxy");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "JP2Rate");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "MinTextStream");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "ModifySuffixes");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "LogPipe");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "LogFile");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "MSIETest");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "AccessLogUserPOV");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "NetdTimeout");
	qp_set_parameter_status (conf_handler, QP_PARM_STATUS_OBSOLETE, "PasswdFile");

	/* init error_log as soon as possible! */
	if (error_log_init (ErrorLog) != 0) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Unable to open ErrorLog file for writing.\n"
			"ErrorLog filename: '%s'\n"
			"Please correct this or leave ErrorLog undefined (for stderr output instead).\n", ErrorLog);
		return (1);
	}

	qp_fail_unrecognized_conf (conf_handler);
	if (dump_errors (conf_handler))
		return (1);

	/* check conflicts */
	if (qp_check_parameter_existence (conf_handler, "LosslessCompressCT") && \
		qp_check_parameter_existence (conf_handler, "Compressible")) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Parameter conflict. \"%s\" cannot be used with \"%s\".\n",
			"LosslessCompressCT", "Compressible");
		return (1);
	}
	if (qp_check_parameter_existence (conf_handler, "AccessLog") && \
		qp_check_parameter_existence (conf_handler, "AccessLogFileName")) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Parameter conflict. \"%s\" cannot be used with \"%s\".\n",
			"AccessLog", "AccessLogFileName");
		return (1);
	}
	if (qp_check_parameter_existence (conf_handler, "ConnTimeout") && \
		qp_check_parameter_existence (conf_handler, "ZiproxyTimeout")) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Parameter conflict. \"%s\" cannot be used with \"%s\".\n",
			"ConnTimeout", "ZiproxyTimeout");
		return (1);
	}

	/* check deprecations based on parameter renaming  */
	if (qp_check_parameter_existence (conf_handler, "AccessLogFileName")) {
		AccessLog = AccessLogFileName;
		error_log_printf (LOGMT_WARN, LOGSS_CONFIG, "\"AccessLogFileName\" option is DEPRECATED.\n"
			"\tIt still works, but it will be removed in the future.\n"
			"\tPlease rename this option to \"AccessLog\" (see documentation for details).\n");
	}
	if (qp_check_parameter_existence (conf_handler, "ZiproxyTimeout")) {
		ConnTimeout = ZiproxyTimeout;
		error_log_printf (LOGMT_WARN, LOGSS_CONFIG, "\"ZiproxyTimeout\" option is DEPRECATED.\n"
			"\tIt still works, but it will be removed in the future.\n"
			"\tPlease rename this option to \"ConnTimeout\" (see documentation for details).\n");
	}

	/* warn if user/group/PIDFile is specified in both CLI and config parameter */
	if ((cli_RunAsUser != NULL) && (qp_check_parameter_existence (conf_handler, "RunAsUser"))) {
		error_log_printf (LOGMT_WARN, LOGSS_CONFIG,
			"User specified in both: CLI parameter and RunAsUser. "
			"Using RunAsUser (%s) instead of CLI parameter (%s).\n", RunAsUser, cli_RunAsUser);
	}
	if ((cli_RunAsGroup != NULL) && (qp_check_parameter_existence (conf_handler, "RunAsGroup"))) {
		error_log_printf (LOGMT_WARN, LOGSS_CONFIG,
			"Group specified in both: CLI parameter and RunAsGroup. "
			"Using RunAsGroup (%s) instead of CLI parameter (%s).\n", RunAsGroup, cli_RunAsGroup);
	}
	if ((cli_PIDFile != NULL) && (qp_check_parameter_existence (conf_handler, "PIDFile"))) {
		error_log_printf (LOGMT_WARN, LOGSS_CONFIG,
			"PID file specified in both: CLI parameter and PIDFile. "
			"Using PIDFile (%s) instead of CLI parameter (%s).\n", PIDFile, cli_PIDFile);
	}

	/* check if user/group exist
	   NOTE: at this point we're NOT changing to user/group, it's just a test */
	if (switch_to_user_group (RunAsUser, RunAsGroup, 1) != 0)
		return (1);

	if (URLNoProcessing != NULL) {
		if ((urltable_noprocessing = load_URLtable (URLNoProcessing)) == NULL)
			return (1);
	}

	if (URLReplaceData != NULL) {
		if ((urltable_replacedata = load_URLtable (URLReplaceData)) == NULL)
			return (1);
	}

	if (URLReplaceDataCT != NULL) {
		if (qp_get_array_size (conf_handler, "URLReplaceDataCTList") <= 0) {
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"URLReplaceDataCT requires non-empty URLReplaceDataCTList.");
			return (1);
		}
		if ((urltable_replacedatact = load_URLtable (URLReplaceDataCT)) == NULL)
			return (1);

		urltable_replacedatactlist = LoadParmMatrixCT (conf_handler,
		                               "URLReplaceDataCTList", NULL, 0, 1, -1, URLReplaceDataCTListAlsoXST);

		if (urltable_replacedatactlist == NULL)
			return (1);
	}

	if (URLDeny != NULL) {
		if ((urltable_deny = load_URLtable (URLDeny)) == NULL)
			return (1);
	}

	/* authentication settings */
	if (check_int_ranges ("AuthMode", AuthMode, AUTH_MIN, AUTH_MAX))
		return (1);
	if (AuthMode == AUTH_PASSWD_FILE) {
		if (AuthPasswdFile == NULL) {
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Specified AuthMode requires defined AuthPasswdFile.");
			return (1);
		}

		// FIXME: this table is not manually deallocated later, thus it is not zero'ed
		// when ziproxy daemon/process finishes. That's a security vulnerability.
		if (auth_passwdfile_init (AuthPasswdFile)) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Error while loading AuthPasswdFile from: %s\n", AuthPasswdFile);
			return (1);
		}
	} else if (AuthMode == AUTH_SASL) {
#ifdef SASL
		if (AuthSASLConfPath != NULL) {
			if (check_directory ("AuthSASLConfPath", AuthSASLConfPath))
				return (1);
		}

		if (auth_sasl_init (AuthSASLConfPath)) {
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Error while initializing SASL.");
			return (1);
		}
#else
		error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Specified AuthMode requires SASL, which is not supported.");
		return (1);
#endif

	}
	auth_set_mode (AuthMode);

	if (TOSMarkAsDiffURL != NULL) {
		if((tos_markasdiff_url = load_URLtable (TOSMarkAsDiffURL)) == NULL)
			return (1);
	}

	/* TODO: make the 'x-' preffix addition optional (see last parameter of LoadParmMatrixCT()
		it is currently hardcoded to ENABLED */
	if (qp_get_array_size (conf_handler, "TOSMarkAsDiffCT") > 0)
		tos_maskasdiff_ct = LoadParmMatrixCT (conf_handler, "TOSMarkAsDiffCT", NULL, 0, 1, -1, TOSMarkAsDiffCTAlsoXST);

	if (check_int_ranges ("TOSFlagsDefault", TOSFlagsDefault, -1, 255))
		return (1);

	if (check_int_ranges ("TOSFlagsDiff", TOSFlagsDiff, -1, 255))
		return (1);

	if (check_int_minimum ("TOSMarkAsDiffSizeBT", TOSMarkAsDiffSizeBT, -1))
		return (1);

	if (check_int_minimum ("MaxActiveUserConnections", MaxActiveUserConnections, 0))
		return (1);

	if (check_int_ranges ("AlphaRemovalMinAvgOpacity", AlphaRemovalMinAvgOpacity, 0, 1000000))
		return (1);

	n = qp_get_array_size (conf_handler, "BindOutgoing");
	if(n){
		char *ip_str;
		
		BindOutgoing_entries = n;
		BindOutgoing = calloc (n, sizeof (in_addr_t));
		while (n--) { 
			qp_getconf_array_str (conf_handler, "BindOutgoing", n, &ip_str, QP_FLAG_NONE);
			BindOutgoing [n] = inet_addr (ip_str);
			free (ip_str);
		}
	}

	if (tmpBindOutgoingExList != NULL) {
		if ((BindOutgoingExList = slist_create (tmpBindOutgoingExList)) == NULL) {
			error_log_puts (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Problem while processing BindOutgoingExList.");
			return (1);
		}
	}

	if (tmpBindOutgoingExAddr != NULL) {
		BindOutgoingExAddr = calloc (1, sizeof (in_addr_t));
		*BindOutgoingExAddr = inet_addr (tmpBindOutgoingExAddr);
	} else {
		if (BindOutgoing_entries > 0) {
			BindOutgoingExAddr = &(BindOutgoing [0]);
		}
	}

	/* TODO: make the 'x-' preffix addition optional (see last parameter of LoadParmMatrixCT()
		it is currently hardcoded to ENABLED */
	if (qp_get_array_size (conf_handler, "LosslessCompressCT") > 0) {
		lossless_compress_ct = LoadParmMatrixCT (conf_handler, "LosslessCompressCT", NULL, 0, 1, -1, LosslessCompressCTAlsoXST);
	} else if (qp_get_array_size (conf_handler, "Compressible") == 0) {
		t_ct_cttable *tmp_lossless_compress_ct;

		/* empty table and no (deprecated) "Compressible" either.
		   load defaults instead. */
		if ((tmp_lossless_compress_ct = ct_create (1)) != NULL) {
			/* defaults' list - includes x-prefixed subtypes (aaaa/x-bbbb) */
			ct_insert (tmp_lossless_compress_ct, "text/*");
			ct_insert (tmp_lossless_compress_ct, "application/shockwave");
			ct_insert (tmp_lossless_compress_ct, "application/msword");
			ct_insert (tmp_lossless_compress_ct, "application/msexcel");
			ct_insert (tmp_lossless_compress_ct, "application/mspowerpoint");
			ct_insert (tmp_lossless_compress_ct, "application/rtf");
			ct_insert (tmp_lossless_compress_ct, "application/postscript");
			ct_insert (tmp_lossless_compress_ct, "application/java");
			ct_insert (tmp_lossless_compress_ct, "application/javascript");
			ct_insert (tmp_lossless_compress_ct, "application/staroffice");
			ct_insert (tmp_lossless_compress_ct, "application/vnd.*");
			ct_insert (tmp_lossless_compress_ct, "application/futuresplash");
			ct_insert (tmp_lossless_compress_ct, "application/asp");
			ct_insert (tmp_lossless_compress_ct, "application/class");
			ct_insert (tmp_lossless_compress_ct, "application/font");
			ct_insert (tmp_lossless_compress_ct, "application/truetype-font");
			ct_insert (tmp_lossless_compress_ct, "application/php");
			ct_insert (tmp_lossless_compress_ct, "application/cgi");
			ct_insert (tmp_lossless_compress_ct, "application/executable");
			ct_insert (tmp_lossless_compress_ct, "application/shellscript");
			ct_insert (tmp_lossless_compress_ct, "application/perl");
			ct_insert (tmp_lossless_compress_ct, "application/python");
			ct_insert (tmp_lossless_compress_ct, "application/awk");
			ct_insert (tmp_lossless_compress_ct, "application/dvi");
			ct_insert (tmp_lossless_compress_ct, "application/css");
			ct_insert (tmp_lossless_compress_ct, "application/*+xml");
			ct_insert (tmp_lossless_compress_ct, "application/xml");
			ct_insert (tmp_lossless_compress_ct, "application/pdf");
			ct_insert (tmp_lossless_compress_ct, "application/tar");
			ct_insert (tmp_lossless_compress_ct, "application/json");
			ct_insert (tmp_lossless_compress_ct, "application/xml-dtd");
			ct_insert (tmp_lossless_compress_ct, "application/iso9660-image");
			ct_insert (tmp_lossless_compress_ct, "image/svg+xml");

			lossless_compress_ct = tmp_lossless_compress_ct;
		}
	}

	/* in case the configuration file is still using
	   the DEPRECATED "Compressible" option... */
	n = qp_get_array_size (conf_handler, "Compressible");
	if (n) {
		t_ct_cttable *tmp_lossless_compress_ct;

		error_log_printf (LOGMT_WARN, LOGSS_CONFIG, "\"Compressible\" option is DEPRECATED.\n"
			"\tIt still works, but it will be removed in the future.\n"
			"\tPlease migrate to \"LosslessCompressCT\" (see documentation for details).\n");

		/* includes x-prefixed subtypes (aaaa/x-bbbb) */
		if ((tmp_lossless_compress_ct = ct_create (1)) != NULL) {
			/* those content-types were automatically included as compressible
			   regardless the contents of "Compressible" array */
			ct_insert (tmp_lossless_compress_ct, "text/*");
			ct_insert (tmp_lossless_compress_ct, "application/javascript");

			/* populate Compressible array (like previous behavior) */
	                Compressible = malloc ((n + 1) * sizeof (char *));
	                for(i = 0; i < n; i++)
	                        qp_getconf_array_str (conf_handler, "Compressible", i, &Compressible [i], QP_FLAG_NONE);
	                Compressible [i] = NULL;

			/* migrate data from deprecated "Compressible" array to new one */
			for (i = 0; i < n; i++) {
				char full_content_type [strlen (Compressible [i]) + 12 + 1];

				sprintf (full_content_type, "application/%s", Compressible [i]);
				ct_insert (tmp_lossless_compress_ct, full_content_type);
			}

			/* no need for "Compressible" array anymore */
			free (Compressible);
			Compressible = NULL;

			lossless_compress_ct = tmp_lossless_compress_ct;
    		}
	}

	RestrictOutPortHTTP_len = LoadParmMatrix (RestrictOutPortHTTP, conf_handler, "RestrictOutPortHTTP", NULL, 1, MAX_RESTRICTOUTPORTHTTP_LEN, 1, 65535);
	if (RestrictOutPortHTTP_len < 0)
		return (1);

	RestrictOutPortCONNECT_len = LoadParmMatrix (RestrictOutPortCONNECT, conf_handler, "RestrictOutPortCONNECT", NULL, 1, MAX_RESTRICTOUTPORTCONNECT_LEN, 1, 65535);
	if (RestrictOutPortCONNECT_len < 0)
		return (1);

	if (LoadParmMatrix(ImageQuality, conf_handler, "ImageQuality", DefaultImageQuality, 4, 4, 0, 100) < 0)
			return (1);

#ifdef EN_NAMESERVERS		
	n = qp_get_array_size (conf_handler, "Nameservers");
	if (n) {
		struct sockaddr_in nsaddr_new;

		if (n > MAXNS) {
			error_log_printf (LOGMT_ERROR, LOGSS_CONFIG, "You can not specify more than %d Nameservers.\n", MAXNS);
			error_log_printf (LOGMT_INFO, LOGSS_CONFIG, "Using nameserver 1 to %d and ignoring the rest.\n", MAXNS);
		}

		nsaddr_new.sin_family = AF_INET;
		nsaddr_new.sin_port = htons(53);

		res_init();
		_res.nscount = 0;

		for (i = 0; i < n && i < MAXNS; i++) {
			char *nserver_str;
			
			qp_getconf_array_str (conf_handler, "Nameservers", i, &nserver_str, QP_FLAG_NONE);
			inet_aton(nserver_str, &nsaddr_new.sin_addr);
			_res.nsaddr_list[_res.nscount++] = nsaddr_new;
		}
	}
#endif

	if (check_int_ranges ("MaxUncompressedImageRatio", MaxUncompressedImageRatio, 0, 100000))
		return (1);

#ifdef JP2K
	switch (JP2Colorspace_cfg) {
		case 1:
			JP2Colorspace = CENC_YUV;
			break;
		case 0:
			JP2Colorspace = CENC_RGB;
			break;
		default:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Configuration error: Invalid JP2Colorspace value: %d\n", JP2Colorspace_cfg);
			return (1);
			break;
	}

	switch (JP2Upsampler_cfg) {
		case 0:
			JP2Upsampler = UPS_LINEAR;
			break;
		case 1:
			JP2Upsampler = UPS_LANCZOS;
			break;
		default:
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Configuration error: Invalid JP2Upsampler value: %d\n", JP2Upsampler_cfg);
			break;
	}

	if (LoadParmMatrix(JP2ImageQuality, conf_handler, "JP2ImageQuality", DefaultJP2ImageQuality, 4, 4, 1, 100) < 0)
		return (1);
	
	if (LoadParmMatrix(JP2BitResYA, conf_handler, "JP2BitResYA", DefaultJP2BitResYA, 8, 8, 1, 8) < 0)
		return (1);
	
	if (LoadParmMatrix(JP2BitResRGBA, conf_handler, "JP2BitResRGBA", DefaultJP2BitResRGBA, 16, 16, 1, 8) < 0)
		return (1);

	if (LoadParmMatrix(JP2BitResYUVA, conf_handler, "JP2BitResYUVA", DefaultJP2BitResYUVA, 16, 16, 1, 8) < 0)
		return (1);

	if (LoadParmMatrix(JP2CSamplingYA, conf_handler, "JP2CSamplingYA", DefaultJP2CSamplingYA, 32, 32, 0, 256) < 0)
		return (1);

	if (LoadParmMatrix(JP2CSamplingRGBA, conf_handler, "JP2CSamplingRGBA", DefaultJP2CSamplingRGBA, 64, 64, 0, 256) < 0)
		return (1);

	if (LoadParmMatrix(JP2CSamplingYUVA, conf_handler, "JP2CSamplingYUVA", DefaultJP2CSamplingYUVA, 64, 64, 0, 256) < 0)
		return (1);
#endif
	
	qp_end (conf_handler);
	return (0);
}

/* create strtable from parameters
 * dvalues = array of strings with default values (if any)
 * dvalues_len = size of dvalues (in elements)
 * returns: <pointer to t_st_strtable structure>, or NULL (fail) -- array length may be collected using strtable functions */
const t_st_strtable *LoadParmMatrixStr(t_qp_configfile *conf_handler, const char *conf_key, const char **dvalues, int dvalues_len, const int min_entries, const int max_entries)
{
	int entries;
	int position;
	char *indata;
	t_st_strtable *outdata;

	if ((outdata = st_create ()) == NULL)
		return (NULL);

	entries = qp_get_array_size (conf_handler, conf_key);

	// checks whether array is defined or not (an empty array will be considered 'undefined')
	// fill with default values if undefined
	if (entries == 0) {
		if (dvalues != NULL) {
			for (position = 0; position < dvalues_len; position++)
				st_insert (outdata, dvalues [position]);
			return (outdata);
		} else {
			st_destroy (outdata);
			return (NULL);
		}
	}
	
	if (((entries > max_entries) && (max_entries >= 0)) || ((entries < min_entries) && (min_entries >= 0))) {
		if (min_entries == max_entries) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n",
				"\tThe number of entries must be exactly: %d\n",
				conf_key, min_entries);
		} else {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n",
				"\tThe number of entries must be between: %d - %d\n",
				conf_key, min_entries, max_entries);
		}
		st_destroy (outdata);

		return (NULL);
	}
	
	for (position = 0; position < entries; position++) {
		qp_getconf_array_str (conf_handler, conf_key, position, &indata, QP_FLAG_NONE);
		st_insert (outdata, indata);
	}

	return (outdata);
}

/* load url table from file
 * returns: <pointer to t_ut_urltable structure>, or NULL (table not loaded) */
const t_ut_urltable *load_URLtable (const char *urltable_filename)
{
	t_ut_urltable *urltable;
	
	if ((urltable = ut_create_populate_from_file (urltable_filename)) == NULL) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Unable to load the following file: %s\n", urltable_filename);
		return (NULL);
	}

	return (urltable);
}

/* create cttable from parameters
 * dvalues = array of strings with default values (if any)
 * dvalues_len = size of dvalues (in elements)
 * returns: <pointer to t_ct_cttable structure>, or NULL (fail) -- array length may be collected using cttable functions */
const t_ct_cttable *LoadParmMatrixCT (t_qp_configfile *conf_handler, const char *conf_key, const char **dvalues, int dvalues_len, const int min_entries, const int max_entries, const int subtype_add_x_prefix)
{
	int entries;
	int position;
	char *indata;
	t_ct_cttable *outdata;

	if ((outdata = ct_create (subtype_add_x_prefix)) == NULL)
		return (NULL);

	entries = qp_get_array_size (conf_handler, conf_key);

	// checks whether array is defined or not (an empty array will be considered 'undefined')
	// fill with default values if undefined
	if (entries == 0) {
		if (dvalues != NULL) {
			for (position = 0; position < dvalues_len; position++)
				ct_insert (outdata, dvalues [position]);
			return (outdata);
		} else {
			ct_destroy (outdata);
			return (NULL);
		}
	}
	
	if (((entries > max_entries) && (max_entries >= 0)) || ((entries < min_entries) && (min_entries >= 0))) {
		if (min_entries == max_entries) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n"
				"The number of entries must be exactly: %d\n",
				conf_key, min_entries);
		} else {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n"
				"The number of entries must be between: %d - %d\n",
				conf_key, min_entries, max_entries);
		}
		ct_destroy (outdata);

		return (NULL);
	}

	for (position = 0; position < entries; position++) {
		qp_getconf_array_str (conf_handler, conf_key, position, &indata, QP_FLAG_NONE);
		ct_insert (outdata, indata);
	}

	return (outdata);
}

/* get position in quality tables according to the image size */
int getImgSizeCategory (int width, int height)
{
	int imgcat;
	long i = width * height;
	
	if (i < 5000)
		imgcat = 0;
	else if ((i < 50000)|| (width < 150) || (height < 150))
		imgcat = 1;
	else if (i < 250000)
		imgcat = 2;
	else imgcat = 3;

	return (imgcat);
}

#ifdef JP2K

/* return a pointer to a array[4] with the bit lenght for JP2K YA components */
const int *getJP2KBitLenYA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2BitResYA + (2 * imgcat));
}

/* return a pointer to a array[4] with the bit lenght for JP2K RGBA components */
const int *getJP2KBitLenRGBA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2BitResRGBA + (4 * imgcat));
}

/* return a pointer to a array[4] with the bit lenght for JP2K YUVA components */
const int *getJP2KBitLenYUVA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2BitResYUVA + (4 * imgcat));
}

/* return a pointer to a array[8] with the components' sampling parameters JP2K YA components */
const int *getJP2KCSamplingYA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2CSamplingYA + (8 * imgcat));
}

/* return a pointer to a array[16] with the components' sampling parameters JP2K RGBA components */
const int *getJP2KCSamplingRGBA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2CSamplingRGBA + (16 * imgcat));
}

/* return a pointer to a array[16] with the components' sampling parameters JP2K YUVA components */
const int *getJP2KCSamplingYUVA (int width, int height)
{
	int imgcat;

	imgcat = getImgSizeCategory (width, height);

	return (JP2CSamplingYUVA + (16 * imgcat));
}

/*
 * Fills in the values of JP2ImageQuality according to image dimensions. 
 * Returns index used to get the values ranging from 0 (smallest images)
 * to 3 (largest images).
 */
int getJP2ImageQuality (int width, int height)
{
	int imgcat;
	
	imgcat = getImgSizeCategory (width, height);
	return (JP2ImageQuality [imgcat]);
}

#endif

/*
 * Fills in the values of ImageQuality according to image dimensions. 
 * Returns index used to get the values ranging from 0 (smallest images)
 * to 3 (largest images).
 */
int getImageQuality (int width, int height)
{
	int imgcat;
	
	imgcat = getImgSizeCategory (width, height);
	return (ImageQuality [imgcat]);
}

/* returns:
 * lenght of array OR
 * < 0 if error */
int LoadParmMatrix(int *destarray, t_qp_configfile *conf_handler, const char *conf_key, const int *dvalues, const int min_entries, const int max_entries, const int min_value, const int max_value)
{
	int entries;
	int position;
	int indata;

	entries = qp_get_array_size (conf_handler, conf_key);

	// checks whether array is defined or not (an empty array will be considered 'undefined')
	// if no data available, use internal default values instead.
	if (entries == 0) {
		if (dvalues != NULL) {
			for (position = 0; position < max_entries; position++)
				destarray [position] = dvalues [position];
			return (max_entries);
		} else {
			return (0);
		}
	}

	if ((entries > max_entries) || (entries < min_entries)) {
		if (min_entries == max_entries) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n"
				"The number of entries must be exactly: %d\n",
				conf_key, min_entries);
		} else {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Invalid number of entries in %s\n"
				"The number of entries must be between: %d - %d\n",
				conf_key, min_entries, max_entries);
		}

		return (-1);
	}
	
	for (position = 0; position < entries; position++) {
		qp_getconf_array_int (conf_handler, conf_key, position, &indata, QP_FLAG_NONE);
		if ((indata < min_value) || (indata > max_value)) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
				"Out of range values in %s\n", conf_key);
			return (-1);
		}
		destarray [position] = indata;
	}

	return (entries);
}

int check_int_ranges (const char *conf_key, const int inval, const int vlow, const int vhigh)
{
	if ((inval < vlow) || (inval > vhigh)) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Out of range value for %s (Acceptable: %d to %d)\n", conf_key, vlow, vhigh);
		return (1);
	}
	return (0);
}

int check_int_minimum (const char *conf_key, const int inval, const int vlow)
{
	if (inval < vlow) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
			"Out of range value for %s (Minimum acceptable: %d)\n", conf_key, vlow);
		return (1);
	}
	return (0);
}

/* returns: ==0 if dirname is a directory,
   or !=0 (error) if does not exist or not a directory */
int check_directory (const char *conf_key, const char *dirname)
{
	struct stat f_stat;

	if (stat (dirname, &f_stat) == 0) {
		if (S_ISDIR (f_stat.st_mode))
			return (0);
	}

	error_log_printf (LOGMT_FATALERROR, LOGSS_CONFIG,
		"%s specifies an invalid directory path: %s\n", conf_key, dirname);
	return (1);
}

/* try to switch process to specified user and group.
   if group is unspecified (NULL), use the user's group.
   if both user_name and group_name == NULL, then do nothing and return 0.
   if check_only != 0, then just check things, but do not attempt to change to user/group.
   returns: ==0, ok; !=0 error (and software is expected to abort soon afterwards) */
int switch_to_user_group (const char *user_name, const char *group_name, int check_only)
{
	const char *effective_group_name = group_name;
	struct passwd *pw_data = NULL;
	struct group *gr_data = NULL;

	if ((user_name == NULL) && (effective_group_name == NULL))
		return (0);	/* nothing to do */

	/* get pw_data */
	if (user_name != NULL) {
		if ((pw_data = getpwnam (user_name)) == NULL) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED, "User not found: %s\n", user_name);
			return (1);
		}
	}

	/* get gr_data */
	if (effective_group_name == NULL) {
		if (user_name != NULL) {
			/* group is unspecified, collect user's group and use that instead */
			if ((gr_data = getgrgid (pw_data->pw_gid)) == NULL) {
				error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED,
					"Group not specified and user's group not found. User: %s\n", user_name);
				return (1);
			}
			effective_group_name = gr_data->gr_name;
		}
	} else {
		if ((gr_data = getgrnam (effective_group_name)) == NULL) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED,
				"Group not found: %s\n", effective_group_name);
			return (1);
		}
	}

	if (check_only != 0)
		return (0);

	/* change group (at this point certainly gr_data != NULL*/
	if (setgid (gr_data->gr_gid) != 0) {
		error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED, "Unable to switch to group: %s\n", effective_group_name);
		return (1);
	}

	/* change user */
	if (pw_data != NULL) {
		if (setuid (pw_data->pw_uid) != 0) {
			error_log_printf (LOGMT_FATALERROR, LOGSS_UNSPECIFIED, "Unable to switch to user: %s\n", user_name);
			return (1);
		}
	}

	return (0);
}

