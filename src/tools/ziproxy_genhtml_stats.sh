#!/bin/sh

LOG_STATS_EXE=ziproxylogtool

# HTML settings
GLOBAL_WIDTH=750
BARS_SPACING=12

# $2 total value (100%), $1 partial value
percent_of ()
{
	if [ $1 != 0 ]; then
		TOTAL_VAL=$(($2 * 1))
		PART_VALUE=$(($1 * 100))

		printf "$((${PART_VALUE} / ${TOTAL_VAL}))"
	else
		printf "0"
	fi
}

# $2 total value (100%), $1 partial value
percent_of_precise ()
{
	if [ $1 != 0 ]; then
		TOTAL_VAL=$(($2 * 1))
		# 100% + 00 (precision digits)
		PART_VALUE=$(($1 * 10000))
		ALL_DIGITS=`echo $((${PART_VALUE} / ${TOTAL_VAL}))`
		if [ ${ALL_DIGITS} -lt 100 ]; then
			INT_PART=0
			FRAC_PART=${ALL_DIGITS}
		else
			INT_PART=`echo ${ALL_DIGITS} | sed 's/..$//g'`
			if [ x${INT_PART} = x ]; then
				INT_PART=0
			fi
			FRAC_PART=`echo $((${ALL_DIGITS} - ${INT_PART}00))`
		fi
		if [ ${FRAC_PART} -lt 10 ]; then
			FRAC_PART=0${FRAC_PART}
		fi

		printf "${INT_PART}.${FRAC_PART}"
	else
		printf "0.00"
	fi
}

# $1 - entry
# $2 - max
# $3 - description
# $4 - unit (kiB, MiB, GiB etc)
bar_horiz_gen ()
{
	ENTRY=$1
	MAXVAL=$2
	DESCR="$3"
	STRUNIT="$4"

	PERCENT_ENTRY=`percent_of ${ENTRY} ${MAXVAL}`
	PERCENT_ENTRY_PRECISE=`percent_of_precise ${ENTRY} ${MAXVAL}`
	PERCENT_REMAINING=$((100 - ${PERCENT_ENTRY}))

	printf '\n<tr><td>\n\n'

	printf '<!-- ouline -->\n'
	printf '<table border=0 cellpadding='${BARS_SPACING}' cellspacing=0><tr><td bgcolor="#ffffff">\n'
	printf '<!-- info on bar -->\n'
	printf '<table border=0 cellpadding=0 cellspacing=0 width='$((${GLOBAL_WIDTH} - $((${BARS_SPACING} * 2))))'><tr bgcolor="#eeeeee">\n'
	printf '<td width="50%%"><font size=2>'${ENTRY}' '${STRUNIT}' ('${PERCENT_ENTRY_PRECISE}'%% of '${MAXVAL}' '${STRUNIT}')</td>\n'
	printf '<td width="50%%" align="right"><font size=2><b>'"${DESCR}"'</b></td>\n'
	printf '</tr></table>\n'
	printf '<!-- /info on bar -->\n'
	printf '<!-- bar -->\n'
	printf '<table border=0 cellpadding=0 cellspacing=0 width="100%%">\n'
	printf '<tr height=10>\n'
	printf '<td bgcolor="#ee5555" width="'${PERCENT_ENTRY}'%%"></td>\n'
	printf '<td bgcolor="#3333aa" width="'${PERCENT_REMAINING}'%%"></td>\n'
	printf '</tr></table>\n'
	printf '<!-- /bar -->\n'
	printf '</td></tr></table>\n'
	printf '<!-- /outline -->\n'

	printf '\n</td></tr>\n\n'
}

# $1 - title
html_start ()
{
	TITLE="$1"
	printf '<html>\n\n<head>\n\n<title>'"${TITLE}"'</title>\n</head>\n\n<body bgcolor="#ddddee">\n\n'
	printf '<center>\n\n'

	# ziproxy head banner
	printf '<table border=0 cellpadding=2 cellspacing=2 width='${GLOBAL_WIDTH}'><tr><td bgcolor="#000000">\n'
	printf '<table border=0 cellpadding=0 cellspacing=0 width="100%%"><tr><td bgcolor="#ffffff">\n'
	#
	printf '<table border=0 cellpadding=0 cellspacing=0><tr>\n'
	printf '<td bgcolor="#ffffff"><font size=1><b>ZIP</b></font></td>'
	printf '<td bgcolor="#ffffff"><font size=3><b>ROXY</b></font></td>'
	printf '</table>'
	#
	printf '<font size=1>\n'
	printf 'Copyright (c)2005-2014 Daniel Mealha Cabrita<br>\n'
	printf 'Licensed under the GNU GPL v2 or later version<br>\n'
	printf '</font>'

	printf '</td></tr></table>\n'
	printf '</td></tr></table>\n'
	printf '<br>\n\n'
}

html_end ()
{
	printf '</center>\n\n'
	printf '\n</body>\n\n</html>\n'
}

# $1 - title of block (string)
# $2 - border color hex 'nnnnnn' (if empty, uses '000000')
block_start ()
{
	BLK_TITLE=$1
	BLK_COLOR=$2

	if [ x${BLK_COLOR} = x ]; then
		BLK_COLOR="000000"
	fi

	printf '<table border=0 cellpadding=2 cellspacing=0 bgcolor="#'${BLK_COLOR}'" width='${GLOBAL_WIDTH}'><tr><td>\n\n'
	printf '<table border=0 cellpadding=0 cellspacing=0 bgcolor="#ffffff">\n\n'
	printf '<tr bgcolor="#'${BLK_COLOR}'"><td><center><font color="#ffffff"><b>'"${BLK_TITLE}"'</b></font></center></td></tr>\n'

}

block_end ()
{
	printf '\n</table>\n\n'
	printf '\n</td></tr></table>\n\n'
	printf '<br>\n'
}

# stdin - output from log analyser
# stdout - result (digits only)
get_log_stat_accesses ()
{
	cat | grep "^Total\ accesses" | sed 's/^.*\:\ *//g'
}

# stdin - output from log analyser
# stdout - result (digits only, in kB)
get_log_stat_incoming_kb ()
{
	TEMPY=`cat | grep "^Total\ incoming\ bytes" | sed 's/^.*\:\ *//g'`
	TEMPY=$((${TEMPY} / 1024))
	printf "${TEMPY}"
}

# stdin - output from log analyser
# stdout - result (digits only, in kB)
get_log_stat_outgoing_kb ()
{
	TEMPY=`cat | grep "^Total\ outgoing\ bytes" | sed 's/^.*\:\ *//g'`
	TEMPY=$((${TEMPY} / 1024))
	printf "${TEMPY}"
}

# $1 - list of sites (hostname), separated by ' ' (space)
# stdin - log to be filtered
# stdout - filtered log
match_sites ()
{
	PROCESSED_LIST=`echo ${1} | sed 's/\./\\\./g'`
	EGREP_EXPR=""
	TOTAL_SITES=0
	FIRST_SITE=""

	for SITE in ${PROCESSED_LIST}; do
		if [ x${FIRST_SITE} = x ]; then
			FIRST_SITE=${SITE}
		fi
		TOTAL_SITES=$((${TOTAL_SITES} + 1))
	done

	if [ ${TOTAL_SITES} = 1 ]; then
		egrep "\ http\:\/\/.*${FIRST_SITE}\/"
	else
		for SITE in ${PROCESSED_LIST}; do
			TOTAL_SITES=$((${TOTAL_SITES} - 1))
			EGREP_EXPR="${EGREP_EXPR}(\ http\:\/\/.*${SITE}\/)"
			if [ ${TOTAL_SITES} != 0 ]; then
				EGREP_EXPR="${EGREP_EXPR}|"
			fi
		done
		egrep "${EGREP_EXPR}"
	fi
}

# $1 - log file
# $2 - description (string)
# $3 - total outgoing data (for generating % of that)
stats_by_content_type ()
{
	LOG_FILE=$1
	DESCRIPTION=$2
	TOTAL_OUTGOING_KB=$3

	block_start "${DESCRIPTION}"
	
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites ".turboupload.com .badongo.com .megaupload.com .rapidshare.com .rapidshare.de zupload.com .depositfiles.com discovirtual.uol.com.br .4shared.com .putfile.com" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "turboupload.com and alikes" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites ".mail.yahoo.com mail.google.com googlemail.com .hotmail.msn.com 64.4.[0-9]*.[0-9]* 65.54.[0-9]*.[0-9]* 65.55.[0-9]*.[0-9]* 68.142.[0-9]*.[0-9]* 207.46.[0-9]*.[0-9]* mail[0-9]*.uol.com.br popmail[0-9].pop.com.br wm[0-9]*.ig.com.br email.terra.com.br" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "E-mails Google, Hotmail etc" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites "download.microsoft.com .windowsupdate.com" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "download.microsoft.com and windowsupdate.com" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites "youtube.com video.google.com .video.l.google.com .videolog.tv .wewin.com .videoslegais.com.br .videoslegais.com .ftvmovies.com .videopop.com.br .tvlegal.com.br freempegs.aebn.net .livedigital.com .stream-music.net video.yahoo.com .bcst.yahoo.com .bcst.a2s.yahoo.com .thatvideosite.com .llnwd.net" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "YouTube, Google Video etc" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites ".yimg.com row.bc.yahoo.com .adserver.yahoo.com ad.doubleclick.net pagead2.googlesyndication.com de.uol.com.br i.s8.com.br de.i.uol.com.br ads1.msn.com ad.adnetwork.com.br www.google-analytics.com ads1.msn.com adclient.lp.uol.com.br ads.globo.com .2mdn.net ads.img.globo.com ad.yieldmanager.com ad2.pop.com.br .adbasket.net .atdmt.com" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Advertisement hosts" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | match_sites ".msn.com .ebuddy.com .koolim.com 207.46.110.36 207.46.110.75 207.46.110.76 207.46.110.14 207.46.110.55 207.46.110.35 207.46.110.15 207.46.110.54 72.232.16.75 207.46.107.77 207.46.107.34 216.32.68.173 207.46.107.56 207.46.107.13 .messengerfx.com .mangeloo.com .goowy.com" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "MSN, ebuddy, koolim and alikes" kB
	#
	LOG_RESULTS=`cat ${LOG_FILE} | egrep "(\.flv$)|(\.wmv$)|(\.mp3$)|(\.mpg$)|(\.mpeg$)|(\.avi$)|(\.asf$)|(\.mov$)|(\.WMV$)|(\.MP3$)|(\.MPG$)|(\.MPEG$)|(\.AVI$)|(\.ASF$)|(\.MOV$)|(\.FLV$)" | ${LOG_STATS_EXE} -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "MP3, WMV music and movies" kB

	block_end
}

# $1 - log file
# $2 - description (string)
# $3 - total outgoing data (for generating % of that)
stats_by_file_size ()
{
	LOG_FILE=$1
	DESCRIPTION=$2
	TOTAL_OUTGOING_KB=$3

	block_start "${DESCRIPTION}"

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 0 --bytes-in-max 1024 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Below 1kB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 1024 --bytes-in-max 10240 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 1kB - 10kB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 10240 --bytes-in-max 102400 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 10kB - 100kB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 102400 --bytes-in-max 1048576 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 100kB - 1MB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 1048576 --bytes-in-max 2097152 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 1MB - 2MB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 2097152 --bytes-in-max 5242880 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 2MB - 5MB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 5242880 --bytes-in-max 10485760 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 5MB - 10MB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 10485760 --bytes-in-max 104857600 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Between 10MB - 100MB" kB

	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} --bytes-in-min 104857600 -mg`
	bar_horiz_gen `echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb` ${TOTAL_OUTGOING_KB} "Over 100MB" kB

	block_end
}

# $1: total sites to list
# $2: 0-top 1-bottom
sort_engine ()
{
	if [ x${2} = x0 ]; then
		sort -n | tail -n ${1} | sort -nr
	else
		sort -nr | tail -n ${1} | sort -n
	fi
}

# top/bottom N sites by access
# $1 - total sites to list
# $2 - 0-top 1-bottom
# $3 - title (string)
top_bottom_by_access ()
{
	block_start "${3}" 226622

	printf "<tr><td>\n"
	printf '<table border=0 cellspacing=0 width='${GLOBAL_WIDTH}'>\n'

	${LOG_STATS_EXE} -mh | sed 's/\ [0-9]*\ [0-9]*\ [0-9]*\ /\ /g' | sort_engine ${1} ${2} | sed 's/^/<tr><td>/g' | sed 's/\ /<\/td><td>/g' | sed 's/$/<\/td><\/tr>/g'

	printf '</table>\n'
	printf "</td></tr>\n"
	block_end
}

# top/bottom N sites by outgoing (compressed) data
# $1 - total sites to list
# $2 - 0-top 1-bottom
# $3 - title (string)
top_bottom_by_out_bytes ()
{
	block_start "${3}" 226622

	printf "<tr><td>\n"
	printf '<table border=0 cellspacing=0 width='${GLOBAL_WIDTH}'>\n'

	${LOG_STATS_EXE} -mh | sed 's/^[0-9]*\ [0-9]*\ //g' | sed 's/\ [0-9]*\ /\ /g' | sort_engine ${1} ${2} | sed 's/^/<tr><td>/g' | sed 's/\ /<\/td><td>/g' | sed 's/$/<\/td><\/tr>/g'

	printf '</table>\n'
	printf "</td></tr>\n"
	block_end
}

# top/bottom N sites by incoming (pre-compression) data
# $1 - total sites to list
# $2 - 0-top 1-bottom
# $3 - title (string)
top_bottom_by_in_bytes ()
{
	block_start "${3}" 226622

	printf "<tr><td>\n"
	printf '<table border=0 cellspacing=0 width='${GLOBAL_WIDTH}'>\n'

	${LOG_STATS_EXE} -mh | sed 's/^[0-9]*\ //g' | sed 's/\ [0-9]*\ [0-9]*\ /\ /g' | sort_engine ${1} ${2} | sed 's/^/<tr><td>/g' | sed 's/\ /<\/td><td>/g' | sed 's/$/<\/td><\/tr>/g'

	printf '</table>\n'
	printf "</td></tr>\n"
	block_end
}



# top/bottom N sites by compression rate
# $1 - total sites to list
# $2 - 0-top 1-bottom
# $3 - title (string)
top_bottom_by_compression ()
{
	block_start "${3}" 226622

	printf "<tr><td>\n"
	printf '<table border=0 cellspacing=0 width='${GLOBAL_WIDTH}'>\n'

	${LOG_STATS_EXE} -mh | sed 's/^[0-9]*\ [0-9]*\ [0-9]*\ //g' | sort_engine ${1} ${2} | sed 's/^/<tr><td>/g' | sed 's/\ /\ \%<\/td><td>/g' | sed 's/$/<\/td><\/tr>/g'

	printf '</table>\n'
	printf "</td></tr>\n"
	block_end
}

filter_last_24h ()
{
	${LOG_STATS_EXE} -mf -1 $((`date +%s` - 86400))
}

# $1 - logfile
generate_page ()
{
	LOG_FILE=$1

	html_start "Ziproxy statistics"

	if [ "$2" != "" ]; then
		printf '<table bgcolor="#2222dd" border=0 cellspacing=0 width='${GLOBAL_WIDTH}'><tr><td>'
		printf '<center><font color="#ffff00"><b>'"${2}"'</b></center>\n'
		printf '</tr></td></table><br>'
	fi

	################################################################
	###   IN THIS SECTION YOU MAY CUSTOMIZE THE HTML GENERATOR   ###
	################################################################

	block_start "General consumption and compression rate"
	#
	printf "<tr><td>"
	block_start "TOTAL" 226622
	LOG_RESULTS=`cat ${LOG_FILE} | ${LOG_STATS_EXE} -mg`
	TOTAL_INCOMING_KB=`echo "${LOG_RESULTS}" | get_log_stat_incoming_kb`
	TOTAL_OUTGOING_KB=`echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb`
	bar_horiz_gen ${TOTAL_OUTGOING_KB} ${TOTAL_INCOMING_KB} "Compression rate" kB
	block_end
	printf "</td></tr>"
	#
	printf "<tr><td>"
	block_start "During last 24h" 226622
	LOG_RESULTS=`cat ${LOG_FILE} | filter_last_24h | ${LOG_STATS_EXE} -mg`
	TOTAL_INCOMING_KB_24H=`echo "${LOG_RESULTS}" | get_log_stat_incoming_kb`
	TOTAL_OUTGOING_KB_24H=`echo "${LOG_RESULTS}" | get_log_stat_outgoing_kb`
	bar_horiz_gen ${TOTAL_OUTGOING_KB_24H} ${TOTAL_INCOMING_KB_24H} "Compression rate" kB
	block_end
	printf "</td></tr>"
	#
	printf "</td></tr>"
	block_end

	stats_by_content_type ${LOG_FILE} "Consumption by content type" ${TOTAL_OUTGOING_KB}

	stats_by_file_size ${LOG_FILE} "Consumption by file size" ${TOTAL_OUTGOING_KB}

	block_start "Top and bottom hosts"
	printf "<tr><td>"
	#
	cat ${LOG_FILE} | top_bottom_by_access 20 0 "Top 20 hosts by access"
	cat ${LOG_FILE} | top_bottom_by_access 20 1 "Bottom 20 hosts by access"
	#
	cat ${LOG_FILE} | top_bottom_by_out_bytes 20 0 "Top 20 hosts by outgoing (compressed) bytes"
	cat ${LOG_FILE} | top_bottom_by_out_bytes 20 1 "Bottom 20 hosts by outgoing (compressed) bytes"
	#
	cat ${LOG_FILE} | top_bottom_by_compression 20 0 "Top 20 hosts by worst compression"
	cat ${LOG_FILE} | top_bottom_by_compression 20 1 "Top 20 hosts by best compression"
	#
	cat ${LOG_FILE} | top_bottom_by_in_bytes 10 0 "Top 10 hosts by incoming (pre-compression) bytes"
	cat ${LOG_FILE} | top_bottom_by_in_bytes 10 1 "Bottom 10 hosts by incoming (pre-compression) bytes"
	#
	printf "</tr></tr>"
	block_end

	html_end
}

if [ "$1" = "-h" ]; then
	printf "Ziproxy HTML stats generator 1.0.2\nCopyright (c)2006-2014 Daniel Mealha Cabrita\nLicensed under GNU GPL license. See documentation for details.\n\n"
	printf "Usage: ziproxy_genhtml_stats.sh <log_filename> [page_title]\n\n"
	exit 0
fi

generate_page "$1" "$2"


# (end)

