/* cfgfile.h
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

//To stop multiple inclusions.
#ifndef SRC_CFGFILE_H
#define SRC_CFGFILE_H

#include "qparser.h"
#include "image.h"
#include "urltables.h"
#include "simplelist.h"
#include "cttables.h"
#include "jp2tools.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_RESTRICTOUTPORTHTTP_LEN 16
#define MAX_RESTRICTOUTPORTCONNECT_LEN 16

extern int Port, NextPort, ConnTimeout, MaxSize, PreemptNameResMax, MaxUncompressedGzipRatio, MinUncompressedGzipStreamEval, MaxUncompressedImageRatio;

extern int ImageQuality[4];
extern int AlphaRemovalMinAvgOpacity;

extern int RestrictOutPortHTTP [MAX_RESTRICTOUTPORTHTTP_LEN];
extern int RestrictOutPortCONNECT [MAX_RESTRICTOUTPORTCONNECT_LEN];
extern int RestrictOutPortHTTP_len;
extern int RestrictOutPortCONNECT_len;

extern t_qp_bool DoGzip, UseContentLength, AllowLookCh, ProcessJPG, ProcessPNG, ProcessGIF, PreemptNameRes, PreemptNameResBC, TransparentProxy, ConventionalProxy, ProcessHTML, ProcessCSS, ProcessJS, ProcessHTML_CSS, ProcessHTML_JS, ProcessHTML_tags, ProcessHTML_text, ProcessHTML_PRE, ProcessHTML_NoComments, ProcessHTML_TEXTAREA, AllowMethodCONNECT, OverrideAcceptEncoding, DecompressIncomingGzipData, WA_MSIE_FriendlyErrMsgs, InterceptCrashes, TOSMarking, TOSMarkAsDiffCTAlsoXST, URLReplaceDataCTListAlsoXST, LosslessCompressCTAlsoXST, ConvertToGrayscale;

extern char *ServHost, *ServUrl, *OnlyFrom, *NextProxy;
extern char *LosslessCompressCT;
extern char *CustomError400, *CustomError403, *CustomError404, *CustomError407, *CustomError408, *CustomError409, *CustomError500, *CustomError503;
extern char *Address;
extern char *DebugLog;
extern char *AccessLog;
extern char *ErrorLog;
extern char *RedefineUserAgent;
extern char *URLNoProcessing;
extern char *URLReplaceData;
extern char *URLReplaceDataCT;
extern char *URLDeny;
extern char *tmpBindOutgoingExList;
extern char *tmpBindOutgoingExAddr;
extern t_st_strtable *BindOutgoingExList;
extern in_addr_t *BindOutgoingExAddr;
extern in_addr_t *BindOutgoing;
extern int BindOutgoing_entries;

extern int TOSFlagsDefault;
extern int TOSFlagsDiff;
extern char *TOSMarkAsDiffURL;
extern int TOSMarkAsDiffSizeBT;
extern int MaxActiveUserConnections;
extern char *PIDFile;
extern char *cli_PIDFile;

extern char *RunAsUser;
extern char *cli_RunAsUser;
extern char *RunAsGroup;
extern char *cli_RunAsGroup;

extern const t_ut_urltable *tos_markasdiff_url;
extern const t_ct_cttable *tos_maskasdiff_ct;

extern const t_ut_urltable *urltable_noprocessing;
extern const t_ut_urltable *urltable_replacedata;
extern const t_ut_urltable *urltable_replacedatact;
extern const t_ct_cttable *urltable_replacedatactlist;
extern const t_ut_urltable *urltable_deny;

extern const t_ct_cttable *lossless_compress_ct;

extern int AuthMode;
extern char *AuthPasswdFile;
#ifdef SASL
extern char *AuthSASLConfPath;
#endif

#ifdef JP2K
extern int JP2ImageQuality[4];
extern t_qp_bool ProcessJP2, ForceOutputNoJP2, ProcessToJP2, AnnounceJP2Capability, JP2OutRequiresExpCap;
// extern int JP2Colorspace_cfg;
extern t_color_space JP2Colorspace;    // 0=RGB 1=YUV
// extern int JP2Upsampler_cfg;
extern t_upsampler JP2Upsampler;       // upsampler method: 0-linear, 1-lanczos
extern int JP2BitResYA[8];	// 4x qualities 2 components: YA YA YA YA
extern int JP2BitResRGBA[16];	// 4x qualities 4 components: RGBA RGBA RGBA RGBA
extern int JP2BitResYUVA[16];	// 4x qualities 4 components: YUVA YUVA YUVA YUVA
extern int JP2CSamplingYA[32];	// 4x qualities 2 components 4 settings: YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh YxYyYwYh AxAyAwAh
extern int JP2CSamplingRGBA[64];	// 4x qualities 4 components 4 settings: RxRyRwRh GxGyGwGh BxByBwBh AxAyAwAh ... (4 times)
extern int JP2CSamplingYUVA[64];	// 4x qualities 4 components 4 settings: YxYyYwYh UxUyUwUh VxVyVwVh AxAyAwAh ... (4 times)
#endif

extern int ReadCfgFile(char * cfg_file);

#ifndef DefaultCfgLocation
extern char DefaultCfgLocation[];
#endif

extern int getImageQuality (int width, int height);
#ifdef JP2K
extern const int *getJP2KBitLenYA (int width, int height);
extern const int *getJP2KBitLenRGBA (int width, int height);
extern const int *getJP2KBitLenYUVA (int width, int height);
extern const int *getJP2KCSamplingYA (int width, int height);
extern const int *getJP2KCSamplingRGBA (int width, int height);
extern const int *getJP2KCSamplingYUVA (int width, int height);
extern int getJP2ImageQuality (int width, int height);
#endif

extern int switch_to_user_group (const char *user_name, const char *group_name, int check_only);

#endif //SRC_CFGFILE_H
