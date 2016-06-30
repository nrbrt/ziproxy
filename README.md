README for Ziproxy

Ziproxy - the HTTP acceleration proxy
Copyright (c)2005-2014 Daniel Mealha Cabrita
For licensing terms and additional copyrights that may apply,
please read the "CREDITS" and "COPYING" files.



WHAT IS ZIPROXY ?

Ziproxy is a fully configurable, high performance, forwarding (non-caching)
HTTP proxy for traffic acceleration purposes.

Its primary feature is the capability of recompressing images
to lower-quality ones and compressing data to gzip, thus reducing the
data size and providing the aforementioned acceleration.

The additional features Ziproxy provides are:

- Recompression to JPEG 2000 instead of "regular" JPEG (default Ziproxy)
  for better image quality (compared to similarly-sized JPEG).

- ToS packet tagging. Makes possible L7 traffic shaping and new QoS
  possibilities.

- Transparent proxy support.

- Block/filter specified URLs and Content-Types.
  May be used as an ad-blocker.

- Basic HTTP authentication.

- A number of unusual features: HTML/JS/CSS optimization, outgoing IP
  rotation, preemptive name resolution etc.

- A number of security provisions (see config options).

- Detailed logging with compression statistics.


 Why?

HTML is plain text, and as such is large and can be compressed very 
well. Most web browsers have the ability to receive content 
compressed in gzip form and then view it as normal. Not many people 
know about this ability, so it isn't used very much. Using this 
feature will speed up web access as text files that are uncompressed 
from a web-server, can be compressed using this proxy and then sent 
over a slower internet connection (like dial-up). To give an example 
of the speed increase, a 100K HTML page can be compressed down to 
around 7Kb after using this proxy. Well, it's shameless advert, we 
aren't counting for modem hardware compression - but that is quite 
less efficient. Even for browsers that don't support this there is 
workaround using SSH port forwarding, and it can yield even better 
compression and response times.

Moreover, images on most pages are of unnecessary high quality 
and/or saved in unsuitable format. Average compression of all images 
using ziproxy is one third of original size, with only marginal 
visible quality decrease. Animated GIFs are stopped, too.

The idea is that you install this at your ISP, or on a fast server 
on the internet ("remote host"). Then use this proxy for your 
dial-up connections to the web from "local host".

 Requirements on remote host

* libungif 

* libpng 

* libjasper (if JPEG2000 support is desired, optional)

* libjpeg-6b 

* libsasl2 (if SASL authentication is desired, optional)

* zlib

* GCC and GNU make. BSD make may work. Sun Make/CC doesn't.

 Installation

To see your options, run: 

$ ./configure --help 

Then, running:

$ ./configure 
$ make 
$ make install 


 Command line

ziproxy <-d|-i|-k> [-c config_file] [-u user_name] [-g group_name] [-p pid_filename] [-h]

-d, --daemon-mode
        Used when running in standalone mode.

-k, --stop-daemon
        Stops daemon.

-i, --inetd-mode
        Used when running from inetd or xinetd.

-c <config_file>, --config-file=<config_file>
        Full path to ziproxy.conf file (instead of default one).

-u <user_name>, --user=<user_name>
        Run daemon as the specified user.

-g <group_name>, --group=<group_name>
        Run daemon as the specified group.
	If unspecified and user_name is specified, uses user_name's group.

-p <pid_filename>, --pid-file=<pid_filename>
        Use the specified PID file for daemon control.

-f, --only-from
        Obsolete. Use "OnlyFrom" config option instead.

-h, --help
        Display summarized help (this text).


 Configuration file

Default location for configuration file is current directory. 

 daemon mode-only options

  WhereZiproxy
  This option is obsolete.

  Port=8080
  Port number Ziproxy uses to listen for connections.

  Address
  Local address to listen for proxy connections.
  If you have more than one network interface,
  it's useful for restricting to which interface you want to bind to.
  Default: binds to all interfaces

  OnlyFrom="an.IP.address" Accept requests only from specified 
  hostname/IP address. You can also specify a range of IP addresses 
  by OnlyFrom="begin.IP.address-end.IP.address". Default is empty 
  (connections are acepted from everywhere).

  NetdTimeout
  This option is obsolete.

  MSIETest
  This option is obsolete.

  MaxActiveUserConnections = 20 (example)
  Limits the number of simultaneous active user connections.
  This does not affect the connection queue (see: SOMAXCONN).
  This also (indirectly) limits the number of processes Ziproxy will run
  at once. Formula for the worst-case scenario:
  MaxZiproxyProcesses = 1 + MaxActiveUserConnections
  OR if PreemptNameRes is enabled (worst-case scenario):
  MaxZiproxyProcesses = 1 + MaxActiveUserConnections * (1 + PreemptNameResMax)
  Valid values: 0 (no limit), >0 (max ative connections).
  Default: 0 (no limit -- relies on OS limit instead)

  PIDFile = "/var/run/ziproxy.pid"
  Defines the file where to dump the daemon PID number.
  If unspecified, will dump the PID to stdout (legacy behavior) and
  you will be unable to stop the daemon invoking 'ziproxy -k'.
  If you run two or more instances of Ziproxy simultaneously,
  you will need to set this to different names (for obvious reasons).
  Default: unspecified (dumps PID to stdout)

  RunAsUser = "ziproxy"
  Run daemon as `RunAsUser` user.
  Switch from current user (in this case, typically `root`)
  to a less privileged one, as a security measure.
  Default: unspecified (does not change user)

  RunAsGroup = "ziproxy"
  Run daemon as `RunAsGroup` group.
  Switch from current group (in this case, typically `root`)
  to a less privileged one, as a security measure.
  Default: unspecified (does not change group)

 general options


  Gzip = true/false
  Whether to try to apply lossless compression with gzip.
  This option concerns traffic between Ziproxy and the client only.
  This optimization is not limited by MaxSize.
  Gzip compression applies only to content-types specified with
  the parameter LosslessCompressCT.
  See also: LosslessCompressCT
  Default: true

  LosslessCompressCT = {"text/*", "application/javascript", "etc/etc"}
  This parameter specifies what kind of content-type is to be
  considered lossless compressible (that is, data worth applying gzip).
  Images, movies etc, normally are NOT compressible such way and those
  content-types should not be added (such data would turn slightly bigger
  and CPU would be wasted).
  See: LosslessCompressCTAlsoXST, Gzip
  Default: an internal list of the most common compressible content-types.

  LosslessCompressCTAlsoXST = true/false
  When using LosslessCompressCT, this defines whether to also automatically add
  content-type entries with 'x-' prefix appended to subtypes
  (aaaa/bbbb also adding aaaa/x-bbbb).
  Usually it's convenient to do this way, that avoids worrying about
  having to create duplicated entries, or whether which variant is valid.
  Note: If LosslessCompressCT is undefined (thus the internal defaults
  are being used) this option has no effect.
  You may want to disable this is you wish to have a precise control
  of what types of content-type you wish to include.
  See also: LosslessCompressCT
  Default: true

  Compressible
  This option is deprecated and will be removed in the future.
  LosslessCompressCT is the replacement for this option.
  You may either leave this option unspecified (thus using
  LosslessCompressCT internal defaults) or migrate the list.
  Migration:
  If you choose to migrate the list, you should change the entries
  from "aaaa" to "application/aaaa". Also, you will have to
  add "text/*" and "application/javascript".
  See: LosslessCompressCT

  ImageQuality = {30,25,25,20}
  This option must have either 4 values 
  or must be not present at all. The numbers give requested quality 
  of outcoming JPEG images, based on size of image(width*height in 
  pixels), respectively:

1. less than 5000 pixels

2. between 5000 and 50000 pixels or one dimension is smaller than 
  150 pixels

3. between 50000 and 250000 pixels

4. more than 250000 pixels

Either number has following meaning:

* 0: do nothing with image

* between 1 and 100: convert image to color JPEG with given quality. 
  If the source image is grayscale, the resulting JPEG may be 
  grayscale too. But ziproxy isn't always able to detect grayscale 
  source images.

For example, ImageQuality={30,25,20,15} means: Images less than 5000 
pixels will be converted JPEG with quality of 15. 
Images between 5000 and 50000 pixels will be converted to JPEG
with quality of 20. Images between 50000 and 250000 pixels will be 
converted to JPEG with quality of 25. Images larger than
250000 pixels will be converted to JPEG with quality of 15.

  AlphaRemovalMinAvgOpacity = <0-1000000>
  Alpha channel (image transparency data) removal threshold.
  Removes alpha channel from images with a minimum opacity
  of AlphaRemovalMinAvgOpacity
  (1000000: completely opaque, 0: completely transparent).
  This reduces data by removing unnecessary alpha channel from
  fully-opaque images; and from (subjectively) not-so-relevant transparency
  information.
  This also allows recompression to JPEG for PNG/JP2k images originally
  with alpha channel (which is not supported by JPEG image format).
  Note: Debug log reports the average alpha opacity for each
        image with alpha channel.
  Default: 1000000 (remove alpha only from fully-opaque images)

  JP2Rate
  This option is obsolete.
  See: JP2ImageQuality

  JP2ImageQuality={20,15,15,15}
  Image quality for JP2 (JPEG 2000) compression.
  Image quality is specified in integers between 100 (best) and 0 (worst).
  This option is similar to "ImageQuality" except it applies to JP2K files, instead.
  JP2K, internally, works differently and has a "rate" setting instead of "quality".
  Within Ziproxy's context we want to use a fixed quality, not a fixed bitrate.
  Thus, prior to compression, the image is analysed in order to know which rate
  (loosely) reflects the quality had this picture be compressed using jpeg.
  This option obsoletes "JP2Rate".
  * This option requires Ziproxy to be compiled with libjasper.

  ZiproxyTimeout
  This option is deprecated and will be removed in the future.
  ConnTimeout is the replacement for this option.
  Migration:
  Just rename "ZiproxyTimeout" to "ConnTimeout"

  ConnTimeout=seconds
  If processing of request exceeds specified time in seconds,
  or connection is idle beyond that time (stalled) it will abort.
  This avoids processes staying forever (or for a very long time)
  in case of a stalled connection or software bug.
  This will NOT necessarily abort the streaming of very big files,
  it will ONLY if the connection stalls or there's a software bug.
  If "0", no timeout.
  Default: 90 (seconds)

  UseContentLength=true/false By default, if ziproxy is 
  modifying/compressing, it begins sending data only after their 
  length can be determined(UseContentLength=true). If you turn 
  option off, ziproxy will start sending data sooner, what will make 
  browsing feel more responsive. But, because browser doesn't know 
  data length, it will be unable to distinguish broken connection 
  from properly closed one. If you use SSH compression instead and 
  your browser identifies itself as HTTP/1.1, you need not unset 
  this (ziproxy will then send "chunked" content to browser).

  MaxSize=bytes
  Max file size to try to (re)compress, in bytes;
  If "0", means that this limitation won't apply.
  This regards to the file size as received from the remote HTTP server
  (which may arrive gzipped or not -- it doesn't matter).
  If a file is bigger than this limit, Ziproxy will simply stream it unmodified,
  unless the user also requested gzip compression (see below).
  Attention: If setting a very big size, the request answer latency will
    increase since Ziproxy needs to fetch the whole file before
    attempting to (re)compress it.
    A too low value will prevent data bigger that that to de processed
    (jpg/png/gif recompression, htmlopt, preemptdns..).
  Note that if:
    - Only gzipping is to be applied *OR*
    - Gzipping and other is to be applied, but data is > MaxSize
    Gzip compression (and only that) will be applied while streaming.
  Default: 1048576 (bytes)
    (default used to be "0" in ziproxy 2.3.0 and earlier)

  MinTextStream
  This option is obsolete.

  ViaServer="something" If specified, ziproxy will send and check 
  Via: header with given string as host identification. It is 
  sometimes useful to avoid request loops.
  Default: not specified

  ModifySuffixes
  This option is obsolete.
 
  AllowLookChange=true/false
  If enabled, will discard PNG/GIF/JP2K transparency and de-animate
  GIF images if necessary for recompression, at the cost of some image
  distortion.
  Note: Images with useless transparency/alpha data (all pixels
        being opaque) do not require this option. In such cases Ziproxy
        will detect that and remove the useless data automatically.
  Default: false (true in pre-2.0.0 versions).

  ConvertToGrayscale=true/false
  If enabled, convert images to grayscale before recompressing.
  This provides extra compression, at the cost of losing color data.
  Note: Not all images sent will be in grayscale, only the ones
        considered worth recompression that way.
  Default: false

  ProcessJPG=true/false If false, ziproxy will not try to recompress
  JPEG format files.
  Default: true.

  ProcessPNG=true/false If false, ziproxy will not try to recompress
  PNG format files.
  Default: true.

  ProcessGIF=true/false If false, ziproxy will not try to recompress
  GIF format files.
  Default: true.

  ProcessJP2=true/false If false, ziproxy will not try to recompress
  JP2 (JPEG 2000) files.
  * This option requires Ziproxy to be compiled with libjasper.
  Default: false.

  ProcessToJP2=true/false
  Whether to try to compress a image to JP2K (JPEG 2000)
  Even when enabled, other formats may sill be tried.
  Web browsers' support vary and an external plugin may be required
  in order to display JP2K pictures.
  If "ForceOutputNoJP2 = true", this option will be overrided
  and stay disabled.
  * This option requires Ziproxy to be compiled with libjasper.
  Default: false

  ForceOutputNoJP2=true/false
  When enabled, this option forces the conversion of all incoming
  JP2K images to another format (usually JPEG).
  JP2K images with unsupported internal data will be forwarded unmodified.
  One may use this option to create "JP2K-compressed tunnels" between
  two Ziproxies with narrow bandwidth in between and serve clients
  which otherwise do not support JP2K while still taking advantage of that
  format. In such scenario, if the clients and their Ziproxy share a LAN,
  for best image quality it is recommended to set a very low (highest quality)
  _local_ output compression.
  This option requires "ProcessJP2 = true" in order to work.
  * This option requires Ziproxy to be compiled with libjasper.
  Default: false

  AnnounceJP2Capability=true/false
  When enabled, every request as a client will include an extra header "X-Ziproxy-Flags"
  announcing it as a Ziproxy with JP2 support enabled.
  This option makes sense when chaining to another Ziproxy.
  Note: when the request is intercepted by another Ziproxy,
        the extra header won't be sent further.
  See also: JP2OutRequiresExpCap
  Default: false

  JP2OutRequiresExpCap=true/false
  "JP2 Output Requires Explicit Capability"
  When enabled (and when JP2 output is enabled) will only compress to JP2 to
  clients which explicity support for that -- that means Ziproxy with
  AnnounceJP2Capability = true.
  This option is useful when you want to compress to JP2 only for clients
  behind a local Ziproxy with ForceOutputNoJP2 = true, but at the same time
  you have clients connecting directly and those do not support JP2.
  Default: false (does not make such discrimination for JP2 output)

  JP2Colorspace=VALUE
  Color model to be used while compressing images to JP2K.
  Accepted values:
    0 - RGB
    1 - YUV
  If different than RGB, it adds extra processing due to conversion.
  By itself doesn't change much the output data size, and the
  conversion is not 100.0% lossless.
  If you plan using JP2CSampling* or JP2BitRes* options, a non-RGB
  color model is highly prefereable.
  Default: 1 (YUV)
  Note: certain jp2-aware software do NOT support a color model
        other than RGB and will either fail or display a distorted image.

  JP2Upsampler=VALUE
  Upsampler to be used while resampling each component of a JP2K picture.
  This is used ONLY when decompressing JP2K pictures, it does not affect
  JP2K compression at all (that uses a downsampler, which is linear-only).
  Accepted values:
    0 - Linear
    1 - Lanczos (Lanczos3)
  For modest scaling such as 2:1, linear is usually better,
  resulting in a overall clear component.
  Lanczos may be interesting when scaling 4:1 or more, though
  it tends to sharpen the JP2K artifacts and add harmonic
  interference to the component.
  Default: 0 (Linear)

  JP2BitResYA={Y1,A1, Y2,A2, Y3,A3, Y4,A4}
  This applies to B&W pictures compressed to JP2K.
  Defines the channel resolution for each component:
  Y (luma) and A (alpha, if present)
  in number of bit (min: 1, max: 8)
  Defines for each file size (see JP2ImageQuality).
  Smallest image is the first components in array.
  Sequence is YAYAYAYA.
  Default: all to eight bits

  JP2BitResRGBA={R1,G1,B1,A1, R2,G2,B2,A2, R3,G3,B3,A3, R4,G4,B4,A4}
  This applies to color pictures compressed to JP2K
  using the RGB model (see JP2Colorspace).
  Defines the channel resolution for each component:
  R (red), G (green), B (blue) and A (alpha, if present)
  in number of bit (min: 1, max: 8)
  Defines for each file size (see JP2ImageQuality).
  Smallest image is the first components in array.
  Sequence is RGBARGBARGBARGBA.
  Default: all to eight bits

  JP2BitResYUVA={Y1,U1,V1,A1, Y2,U2,V2,A2, Y3,U3,V3,A3, Y4,U4,V4,A4}
  This applies to color pictures compressed to JP2K
  using the YUV color model (see JP2Colorspace).
  Defines the channel resolution for each component:
  Y (luma), U (chroma, Cb), V (chroma, Cr), and A (alpha, if present)
  in number of bit (min: 1, max: 8)
  Defines for each file size (see JP2ImageQuality).
  Smallest image is the first components in array.
  Sequence is YUVAYUVAYUVAYUVA.
  Default: sensible values for best quality/compression

  JP2CSamplingYA={ (see below) }
  This applies to B&W pictures compressed to JP2K.
  Here you may define the sampling rate for each component,
  for each picture size.
  The sequence is:
  Y_xpos, Y_ypos, Y_xstep, Y_ystep,  A_xpos, A_ypos, A_xstep, A_ystep, (smallest picture)
  ... ... ... (medium-sized picture)
  etc.
  Default: all x/ypos=0 x/ystep=1 (no components suffer subsampling)
  Note: certain jp2-aware software do NOT support component subsampling and will fail.

  JP2CSamplingRGBA={ (see below) }
  This applies to color pictures compressed to JP2K
  using the RGB model (see JP2Colorspace).
  Here you may define the sampling rate for each component,
  for each picture size.
  The sequence is:
  R_xpos, R_ypos, R_xstep, R_ystep,  G_xpos, G_ypos, G_xstep, G_ystep,  B...  A... (smallest picture)
  ... ... ... (medium-sized picture)
  etc.
  Default: all x/ypos=0 x/ystep=1 (no components suffer subsampling)
  Note: certain jp2-aware software do NOT support component subsampling and will fail.

  JP2CSamplingYUVA={ (see below) }
  This applies to color pictures compressed to JP2K
  using the YUV color model (see JP2Colorspace).
  Here you may define the sampling rate for each component,
  for each picture size.
  The sequence is:
  Y_xpos, Y_ypos, Y_xstep, Y_ystep,  U_xpos, U_ypos, U_xstep, U_ystep,  V...  A... (smallest picture)
  ... ... ... (medium-sized picture)
  etc.
  Default: sensible values for a good image quality.
  Note: certain jp2-aware software do NOT support component subsampling and will fail.

  ProcessHTML=true/false If true, ziproxy will optimize the HTML code
  before attempting (if enabled) to gzip it. This option also covers
  javascript and CSS data embedded into HTML code.
  This option is affected by the other options: ProcessHTML_*.
  Default: false.
  *** THIS OPTION IS EXPERIMENTAL ***

  ProcessCSS=true/false If true, ziproxy will optimize the CSS code
  before attempting (if enabled) to gzip it. This option is affected
  by the other options ProcessHTML_*.
  This ONLY affects stand-alone CSS files, not CSS embedded into
  HTML code.
  Default: false.
  *** THIS OPTION IS EXPERIMENTAL ***

  ProcessJS=true/false If true, ziproxy will optimize the javascript code
  before attempting (if enabled) to gzip it. This option is affected
  by the other options ProcessHTML_*.
  This ONLY affects stand-alone javascript files, not javascript
  embedded into HTML code.
  Default: false.
  *** THIS OPTION IS EXPERIMENTAL ***

  ProcessHTML_CSS=true/false If true, CSS data embedded into HTML
  code will be optimized.
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_JS=true/false If true, javascript code embedded
  into HTML code will be optimized.
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_tags=true/false If true, HTML tags themselves will
  be optimized. It means quote chars suppression (when possible),
  redundant spacing suppression, conversion to lowecase, suppression
  of trailing dash from "/>".
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_text=true/false If true, the text itself will be
  optimized, in practice meaning all the redundant spaces to
  be removed.
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_PRE=true/false If true, PREformatted HTML text
  will be optimized.
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_TEXTAREA=true/false If true, data within TEXTAREA HTML
  tags context will be optimized.
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  ProcessHTML_NoComments=true/false If true, all the irrelevant
  HTML-level comments will be suppressed. This will not remove
  javascript code embedded into a HTML-comment block, nor certain
  other cases (which are not really comments).
  In order to take effect, this option depends on the ProcessHTML
  option to be enabled aswell.
  Default: true.

  PreemptNameRes=true/false Preemptive name resolution. If true and
  the processed file is a html one, it will try to resolve all
  the hostnames present in the html file (in the hope the resolved
  name will be cached by the DNS or name cache, external to Ziproxy).
  Ziproxy will _not_ cache any hostname by itself! (Try PDNSD, etc)
  If the user clicks a link from a page previously processed by
  Ziproxy, there will be no delay due to name resolution.
  Warning: This option will increase the DNS traffic by many times.
  Default: false (true in pre-2.0.0 versions).

  PreemptNameResMax=50 Maximum hostnames Ziproxy will try to resolve
  in a preemptive manner (see PreemptNameRes).
  Default: 50

  PreemptNameResBC=true/false Bogus check for hostnames Ziproxy
  will try to resolve in a preemptive manner (see PreemptNameRes).
  Currently, if enabled, ignore hostnames other than the ones
  ending with .nnnn, .nnn or .nn (eg. .info, .com, .br...)
  Default: false

  TransparentProxy=true/false Allow processing of requests as
  transparent proxy (will still accept normal proxy requests)
  In order to use Ziproxy as transparent proxy it's also needed
  to reroute the connections from x.x.x.x:80 to ziproxy.host:PROXY_PORT
  Default: false
  See also: RestrictOutPortHTTP

  ConventionalProxy=true/false
  Whether to process normal proxy requests or not
  Only makes sense when TransparentProxy is enabled.
  If transparent proxy is enabled, it's usually a good idea to disable
  conventional proxying since, depending on the layout of your network,
  it can be abused by ill-meant users to circumvent restrictions
  presented by another proxy placed between Ziproxy and the users.
  Default: true

  AllowMethodCONNECT=true/false
  Whether to allow the CONNECT method.
  This method is used by HTTPS, but may be used for other
  types of service (like instant messenging) which allow tunneling through http proxy.
  If you plan on serving only HTTP requests (no HTTPS nor anything else)
  you may want to disable this, in order to prevent potential
  abuse of the service.
  Default: true
  See also: RestrictOutPortCONNECT

  RestrictOutPortHTTP = { <list of ports> }
  If defined, restricts the outgoing connections (except CONNECT methods - used by HTTPS)
  to the listed destination ports.
  If TransparentProxy is used, for security reasons it's recommended to restrict
  to the ports (typically port 80) which are being intercepted.
  Default: all ports are allowed.
  See also: RestrictOutPortCONNECT

  RestrictOutPortCONNECT = { <list of ports> }
  If defined, restricts the outgoing connections using the CONNECT method (used by HTTPS)
  to the listed destination ports.
  If AllowMethodCONNECT=false, then no ports are allowed at all regardless this list.
  Default: all ports are allowed.
  See also: AllowMethodCONNECT, RestrictOutPortHTTP

  OverrideAcceptEncoding=true/false
  Whether to override the Accept-Encoding more to Ziproxy's liking.
  If disabled, Ziproxy will just forward Accept-Encoding received from the client
  (thus the data may or not come gzipped, depending on your HTTP client).
  Currently, this option is used to always advertise Gzip capability to
  the remote HTTP server.
  This has _no_ relation to Gzip support between Ziproxy and the client,
  Ziproxy will compress/decompress the data according to the client.
  Default: true

  DecompressIncomingGzipData=true/false
  Enable/disable the internal gzip decompression by Ziproxy.
  This decompression is needed when the remote server sends data already gzipped,
  but further processing is desired (like HTMLopt, PreemptDNS etc).
  Disabling this will save some processing load, and reduce some latency since
  Ziproxy will directly stream that data to the client.
  - But processing features WILL NOT work with such data.
  Attention:
  If you disable this, but configure Ziproxy to advertise as a gzip-supporting
  client to the remote server: While using a non-gzip-supporting client, the client
  may receive gzip-encoded data and it won't know how to deal with that
  (== it will receive useless garbage).
  Default: true (enabled)

  RedefineUserAgent="SuperBrowser/2.07 (blah blah blah blah)"
  Replaces the User-Agent data sent by the client with a custom string,
  OR defines User-Agent with that string if that entry was not defined.
  If disabled, Ziproxy will just forward the User-Agent sent by the client.
  Normally you will want to leave this option DISABLED (commented).
  It's useful if you, for some reason, want to identify all the clients as
  some specific browser/version/OS.
  Certain websites may appear broken if the client uses a different browser than
  the one specified here.
  Default: <undefined> (just forwards User-Agent as defined by the client)

  MaxUncompressedGzipRatio=2000
  When Ziproxy receives Gzip data it will try to decompress in order to do
  further processing (HTMLopt, PreemptDNS etc).
  This makes Ziproxy vulnerable to 'gzip-bombs' (eg. like 10 GB of zeroes, compressed)
  which could be used to slow down or even crash the server.
  In order to avoid/minimise such problems, you can limit the max
  decompression proportion, related to the original file.
  If a Gzipped file exceedes that proportion while decompressing, its
  decompression is aborted.
  The user will receive an error page instead or (if already transferring)
  transfer will simply be aborted.
  You may disable this feature defining its value to '0'.
  Default: 2000 (that's 2000% == 20 times the compressed size)

  MinUncompressedGzipStreamEval=10000000
  When limiting decompression rate with MaxUncompressedGzipRatio
  _and_ gunzipping while streaming it's not possible to know the
  file size until the transfer is finished. So Ziproxy verifies this while
  decompressing.
  The problem by doing this is the possible false positives:
  certain files compress a lot at their beginning, but then not-so
  shortly after.
  In order to prevent/minimize such problems, we define the minimum
  output (the decompressed data) generated before starting to
  check the decompression rate.
  If defined as '0', it will check the rate immediately.
  A too large value will increase the rate-limit precision, at the cost of less
  protection.
  Streams with output less that this value won't have decompression
  rate checking at all.
  This feature is only active if MaxUncompressedGzipRatio is defined.
  This does not affect data wholly loaded to memory (for further processing).
  Default: 10000000 (bytes)
  Note: The previous default (until version 2.7.9_BETA) was 250000
  See also: MaxUncompressedGzipRatio

  MaxUncompressedImageRatio = 500
  This is the maximum compression rate allowable for an incoming
  (before recompression) image file.
  If an image has a higher compression rate than this, it will not
  be unpacked and it will be forwarded to the client as is.
  This feature protects against (or mitigates) the problem with
  "image bombs" (gif bombs, etc) done with huge bitmaps with the same
  pixel color (thus very small once compressed).
  Since Ziproxy may try to recompress the image, if several of this
  kind are requested, the server may run out of memory, so this
  may be used as a DoS attack against Ziproxy.
  This feature will not protect the client, since it will receive
  the unmodified picture.
  There are rare legitimate cases matching such high compression rate,
  including poor website design. But in such cases is not really worth
  recompressing anyway (the processing costs are not worth the savings).
  Usually "image bomb" pictures have a >1000:1 compression ratio.
  Setting this to less than 100 risks not processing legitimate pictures.
  Setting 0 disables this feature.
  Default: 500 (500:1 ratio)

  CustomError400="/full/path/error_file.html"
  Custom error message for "Bad request"
  (malformed URL, or unknown URL type)
  Default: <undefined> (internal error message)

  CustomError403="/full/path/error_file.html"
  Custom error message for "Forbidden"
  (includes access to forbidden ports set in Ziproxy)
  Default: <undefined> (internal error message)

  CustomError404="/full/path/error_file.html"
  Custom error message for "Unknown host"
  (Ziproxy will not issue 'page not found' errors itself)
  Default: <undefined> (internal error message)

  CustomError407="/full/path/error_file.html"
  Custom error message for "Proxy Authentication Required"
  Default: <undefined> (internal error message)

  CustomError408="/full/path/error_file.html"
  Custom error message for "Request timed out"
  Default: <undefined> (internal error message)

  CustomError409="/full/path/error_file.html"
  Custom error message for "Conflict"
  Default: <undefined> (internal error message)

  CustomError500="/full/path/error_file.html"
  Custom error message for "Internal error"
  (or empty response from server)
  Default: <undefined> (internal error message)

  CustomError503="/full/path/error_file.html"
  Custom error message for "Connection refused"
  (or service unavailable)
  Default: <undefined> (internal error message)

  AuthMode = 0
  Authentication mode to be used for proxy access:
  0: none (no authentication required)
  1: plain text file
  2: SASL (auxprop, see /etc/ziproxy/sasl/ziproxy.conf)
  Notes:
  a) SASL support is optional (enabled during compilation time).
  b) SASL authentication does not require external SASL daemon
     configuration/invocation, just Ziproxy's SASL configuration.
  Default: 0 (no authentication required)
  See also: AuthPasswdFile, AuthSASLConfPath

  AuthPasswdFile = "/etc/ziproxy/http.passwd"
  Plain text file containing authentication data.
  Should contain user:pass pairs, lines no longer than 128 chars.
  Password is unencrypted.
  Used only when AuthMode=1
  Default: (undefined)
  See also: AuthMode

  AuthSASLConfPath = "/etc/ziproxy/sasl/"
  Path to Ziproxy's SASL configuration file, where
  a file named "ziproxy.conf" (not related to this one)
  must be present and properly configured.
  Used only when AuthMode=2
  Default: (default SASL setting, OS-dependent, may be /etc/sasl2/)
  See also: AuthMode

  PasswdFile
  This option is obsolete.
  In order to enable this kind of authentication:
  a) Remove/comment this option.
  b) Set AuthPasswdFile to the this option's previous value.
  c) Set AuthMode=1

  Nameservers={"1.2.3.4", "11.22.33.44", ...}
  If enabled, used the specified nameservers instead of the default ones
  (usually at /etc/resolv.conf)
  Default: Disabled. Ziproxy uses the OS-level DNS configuration.

  BindOutgoing = {"234.22.33.44", "4.3.2.1", "44.200.34.11", ...}
  Bind outgoing connections (to remote HTTP server) to the following (local) IPs
  It applies to the _outgoing_ connections, it has _no_ relation to the listener socket.
  When 2 or more IPs are specified, Ziproxy will rotate to each of those at each
  outgoing connection. All IPs have the same priority.
  You may use this option for either of the following reasons:
  1. - To use only a specific IP when connecting to remote HTTP servers.
  2. - Use 2 or more IPs for load balancing (a rather primitive one, since it's
       connection-based and does not take into account the bytes transferred).
  3. - You have a huge intranet and certain sites (google.com, for example)
       are blocking your requests because there are so many coming from the same IP.
       So you may use 2 or more IPs here and make it appear that your requests
       come from several different machines.
  This option does _not_ spoof packets, it merely uses the host's local IPs.
  Note: While in (x)inetd mode, output may be bind-ed only to one IP.
  See also: BindOutgoingExList
  Default: Disabled. Binds to the default IP, the OS decides which one.

  BindOutgoingExList="/etc/ziproxy/bo_exception.list"
  Specifies a file containing a list of hosts which should not suffer
  IP rotation as specified by the option "BindOutgoing".
  The reason for this option is that certain services do not like
  the client IP changing in the same session.
  Certain webmail services fail or return authentication failure in this case.
  This option has no effect if BindOutgoing is not used.
  See also: BindOutgoingExAddr
  Default: empty, no hosts are exempted.

  BindOutgoingExAddr="98.7.65.43"
  Defines a specific IP to be bound to for hosts specified in BindOutgoingExList.
  As with BindOutgoing, this IP must be a local IP from the server running Ziproxy.
  This IP may be one of those specified in BindOutgoing, but that's _not_
  a requirement and may be a different IP.
  This option has no effect if BindOutgoingExList is not being used.
  Default: empty, uses the first IP specified in BindOutgoing.

  WA_MSIE_FriendlyErrMsgs=true/false
  Workaround for MSIE's pseudo-feature "Show friendly HTTP error messages."
  If User-Agent=MSIE, don't change/compress the body of error messages in any way.
  If compressed it could go down below to 256 or 512 bytes and be replaced with
  a local error message instead.
  In certain cases the body has crucial data, like HTML redirection or so, and
  that would be broken if a "friendly error" replaces it.
  If you are sure there are no users using MSIE's with "friendly error messages"
  enabled, or you don't support/have users with such configuration, you may
  disable this and have error data compressed for MSIE users.
  This workaround does not affect other clients at all, and error messages
  will be sent compressed if the client supports it.
  Default: true (enabled)

  URLNoProcessing = "/etc/ziproxy/noprocess.list"
  This option specifies a file containing a list of URLs that should be tunneled
  by Ziproxy with no kind of processing whatsoever.
  The list contain fully-formatted URLS (http://xxx/xxx), one URL per line.
  The URLs may also contain pattern-matching asterisks.
  Comments may be present if prefixed by '#' (shell-alike).
  In order to exempt a whole site from processing: "http://www.exemptedhost.xyz/*"
  This option exists when a page is known to stop working under Ziproxy processing
  and there's no specific workaround/bugfix still available.
  Thus, this is a temporary solution when you depend on the page to work in a
  production environment.
  ****** REMEMBER TO REPORT BUGS/INCOMPATIBILITIES SO THEY MAY BE FIXED *******
  *** THIS IS NOT SUPPOSED TO BE A DEFINITIVE SOLUTION TO INCOMPATIBILITIES ***
  Default: empty (no file specified, inactive)

  URLReplaceData = "/etc/ziproxy/replace.list"
  This option specifies a file containing a list of URLs which its
  data should be intercepted and replaced by another.
  Header data such as cookies is maintained.
  Currently the only replacing data available is an empty image
  (1x1 transparent pixel GIF).
  The list contain fully-formatted URLS (http://xxx/xxx), one URL per line.
  The URLs may also contain pattern-matching asterisks.
  Comments may be present if prefixed by '#' (shell-alike).
  In order to exempt a whole site from processing: "http://ad.somehost.xyz/*"
  The way it is, this option may be used as an AD-BLOCKER which is
  transparent to the remote host (data is downloaded from the remove server
  and cookies are transported) -- a stealthy ad-blocker, if you like.
  Default: empty (no file specified, inactive)
  See also: URLReplaceDataCTList, URLReplaceData

  URLReplaceDataCT = "/etc/ziproxy/replace_ct.list"
  Same as URLReplaceData, except it will only replace the data
  from matching URLs if the content-type matches
  the list in URLReplaceDataCTList (mandatory parameter) aswell.
  URLReplaceDataCT may be useful as a more compatible AD-BLOCKER
  if only visual files are replaced. Certain websites rely on
  external javascript from advertisement hosts and break when
  that data is missing, this is a way to block advertisements
  in such cases.
  Default: empty (no file specified, inactive)
  See also: URLReplaceDataCTList, URLReplaceData

  URLReplaceDataCTList = {"image/jpeg", ...}
  List of content-types to use with the URLReplaceDataCT option.
  This option is required by URLReplaceDataCT.
  Default: empty (no content-type specified, inactive)
  See also: URLReplaceDataCTListAlsoXST, URLReplaceDataCT

  URLReplaceDataCTListAlsoXST = true/false
  When using URLReplaceDataCTList, this defines whether to also automatically add
  content-type entries with 'x-' prefix appended to subtypes
  (aaaa/bbbb also adding aaaa/x-bbbb).
  Usually it's convenient to do this way, that avoids worrying about
  having to create duplicated entries, or whether which variant is valid.
  You may want to disable this is you wish to have a precise control
  of what types of content-type you wish to include.
  See also: URLReplaceDataCTList
  Default: true

  URLDeny = "/etc/ziproxy/deny.list"
  This option specifies a file containing a list of URLs which
  should be blocked.
  A "access denied" 403 error will be returned when trying to access
  one of those URLs.
  Default: empty (no file specified, inactive)

  NextProxy="host.name"
  Forward everything to another proxy server.
  Modifications/compression is still applied.
  Default: none (disabled)

  NextPort=8080
  TCP port to be used by NextProxy.
  Default: 8080



 TOS options (daemon mode-only)

  TOSMarking = true/false
  Enable this if you want to specify the (IP-level) TOS certain types
  of traffic from ziproxy -> user.
  This feature is useful if one wants to do application-level QoS.
  Setting TOS does not provide QoS alone. You must be either using
  a network with routers priorizing traffic according to their TOS,
  or set your own QoS/traffic-shaper system  and treat the packets
  with certain TOS accordingly.
  Ziproxy is RFC-agnostic regarding TOS bit meanings,
  though there may be limitations imposed by the host OS.
  See: RFC 791, RFC 1122, RFC 1349, RFC 2474 and RFC 3168.
  If disabled, all other TOS options won't have effect.
  Disabled by default.

  TOSFlagsDefault = <decimal value between 0-255>
  TOS to set by default
  This is a decimal value between 0-255.
  If unset, will use the OS default (which usually is 0).
  If you want to make sure it is set to 0, then set
  this option accordingly.
  Your OS may put restrictions on which bits you may set
  (so certain bits will remain unchanged regardless).
  Your OS may also restrict which bits and/or value ranges
  you may set if you're not running as root.
  Other (non-unixish) OSes may be unable to set TOS at all.
  Default: unset.

  TOSFlagsDiff = <decimal value between 0-255>
  TOS to set when the traffic is considered "differentiated",
  according to TOSMarkAsDiffURL, TOSMarkAsDiffCT or TOSMarkAsDiffSizeBT.
  This is a decimal value between 0-255.
  If unset, there will be no differentiated traffic at all.
  Your OS may put restrictions on which bits you may set
  (so certain bits will remain unchanged regardless).
  Your OS may also restrict which bits and/or value ranges
  you may set if you're not running as root.
  Other (non-unixish) OSes may be unable to set TOS at all.
  Default: unset.

  TOSMarkAsDiffURL = "<filename>"
  This is the file containing a list of URLs which should
  have their traffic "differentiated"
  (that is, to have their TOS changed to TOSFlagsDiff).
  Inside the file, the URLs may also contain pattern-matching asterisks.
  Comments may be present if prefixed by '#' (shell-alike).
  In order to match a whole site: "http://www.examplehost.xyz/*"
  Default: none

  TOSMarkAsDiffCT = {"video/flv", "video/x-msvideo", "audio/*", "etc"}
  This is the content-type list of data that should
  have their traffic "differentiated"
  (that is, to have their TOS changed to TOSFlagsDiff).
  This is the content-type as received by the remote HTTP server,
  if it is changed by Ziproxy later, it will not be taken into account.
  "" (empty string) will match empty content-types AND data which have
  no content-type specified.
  If no subtype is specified, all subtypes will match:
  "aaaa" will match "aaaa", "aaaa/bbbb", "aaaa/cccc" etc
  See also: TOSMarkAsDiffCTAlsoXST
  Default: none

  TOSMarkAsDiffCTAlsoXST = true/false
  When using TOSMarkAsDiffCT, this defines whether to also automatically add
  content-type entries with 'x-' prefix appended to subtypes
  (aaaa/bbbb also adding aaaa/x-bbbb).
  Usually it's convenient to do this way, that avoids worrying about
  having to create duplicated entries, or whether which variant is valid.
  You may want to disable this is you wish to have a precise control
  of what types of content-type you wish to include.
  See also: TOSMarkAsDiffCT
  Default: true

  TOSMarkAsDiffSizeBT = 4000000 (see text)
  This is the stream size threshold (in bytes) which, if reached,
  will make such traffic "differentiated"
  (that is, to have their TOS changed to TOSFlagsDiff).
  The stream size is the ziproxy -> user one (which may be
  bigger or smaller than the original one, sent by the HTTP server).
  There are two possible behaviors with this parameter:
  - The total stream size is known beforehand, so the data
    will be marked as differentiated from the beginning.
  - The total stream size is unknown, so the data will
    be marked as differentiated once it reaches that
    size.
  Current limitations (this may change in the future):
  - The maximum value to be specified here is signed int
    usually 32bit -> (2^31 - 1).
  - HTTP range requests are not taken into account so, if their effective
    streams do not reach this threshold, such data will not be
    marked as "differentiated", even if the HTTP range goes beyond that.
  - Usually the HTTP headers will not be taken into account (only the body
    size itself), except in cases such as CONNECT method
    and URLNoProcessing (cases when the data from server is treated like
    a "black box").
  Default: none



 Logging options

  LogFile
  This option is obsolete.
  Note: DebugLog replaces this, except it does not expand the filename
        to the current date when used with %j %Y etc.

  DebugLog="/something_like/var/log/ziproxy/debug.log"
  Dumps detailed processing information for each request.
  Since concurrent HTTP requests are asynchronous, the lines end up mixed
  in this log. Use the PID number to differentiate a request from another.
  Unless you really want this data, it's better leaving this disabled as
  it generates lots of data for each HTTP request.
  Disabled by default.

  ErrorLog = "/something_like/var/log/ziproxy/error.log"
  Error-like messages logging.
  This relates to error messages, warnings and such messages, including
  configuration errors and other.
  If undefined, defaults to stderr. In this case the (normally rare) errors
  occuring after the program successfully started will not be displayed,
  that to avoid the possibility of flooding the screen with error messages.
  If defined, all error-like messages will be dumped into the specified
  file. The exception are the errors occurring at the very early stages
  of Ziproxy initialization.
  WARNING: If you define a error log file and ziproxy initialization fails,
           ziproxy will fail and no error will be displayed on the console.
  Default: undefined (dumps to stderr).

  AccessLogFileName
  This option is deprecated and will be removed in the future.
  AccessLog is the replacement for this option.
  Migration:
  Just rename "AccessLogFileName" to "AccessLog"

  AccessLog="/something_like/var/log/ziproxy/access.log"
  File to be used as access log.
  Log format (columns):
    TIME (unix time as seconds.msecs),
    PROCESS_TIME (ms, from receiving request to last byte sent to client),
    [USER@]ADDRESS (address with daemon mode only, with [x]inet it displays a '?'),
    FLAGS,
    ORIGINAL_SIZE,
    SIZE_AFTER_(RE)COMPRESSION,
    METHOD,
    URL.
  Where FLAGS may be:
    P (a request as proxy)
    T (a request as transparent proxy)
    S (CONNECT method, usually HTTPS data)
    Z (transfer timeoutted - see ConnTimeout)
    B (interrupted transfer - either by user or by remote http host)
    W (content type was supposed to load into memory, but it had no content-size and, in the end, it was bigger than MaxSize. so it was streamed instead)
    N (URL not processed. See: URLNoProcessing config option)
    R (data was replaced. See: URLReplaceData config option)
    K (image too expansive. See: MaxUncompressedImageRatio config option)
    G (stream gunzip too expansive. See: MinUncompressedGzipStreamEval, MaxUncompressedGzipRatio)
    1 (SIGSEGV received. See: InterceptCrashes config option)
    2 (SIGFPE received. See: InterceptCrashes config option)
    3 (SIGILL received. See: InterceptCrashes config option)
    4 (SIGBUS received. See: InterceptCrashes config option)
    5 (SIGSYS received. See: InterceptCrashes config option)
    X (SIGTERM received. Also happens when interrupting the daemon while transferring.)
  Default: No file specified (thus no access logging)

  AccessLogUserPOV
  This option is obsolete.

  InterceptCrashes = true/false
  When enabled, Ziproxy will intercept signals indicative of
  software crash, flag the offending request in access log
  accordingly, then stop the offending process.
  This is useful for debugging purposes and it's not recommended
  to leave it enabled in normal use due to the risk of garbage
  being written to access log (due to a more severe crash).
  Once enabled, the intercepted signals are:
  SIGSEGV (segmentation fault)
  SIGFPE (FPU exception)
  SIGILL (illegal instruction)
  SIGBUS (bus error, alignment issues)
  SIGSYS (bad system call)
  Default: disabled (those signals not intercepted by Ziproxy)

  LogPipe
  This option is obsolete.



 Compiling under cygwin -- TODO

It compiles almost the same way. However, you may want to avoid 
libungif dependence on X11. Then add -static option to LDFLAGS 
variable in Makefile:

LDFLAGS = -g $(SYSV_LIBS) -static -lgif -lpng -ljpeg -lm -lz

It has other advantage, that statically linked executable 
ziproxy.exe can be together with cygwin1.dll 
transferred to other machine, where cygwin needs not be installed. 
xinetd server is also available as cygwin package.

 Usage

 With inetd

In /etc/inetd.conf add the line where <location> is where you put 
the executable:

ziproxy stream tcp nowait.500 root /usr/sbin/tcpd <location>/ziproxy 
-i -c <location>/ziproxy.conf

in /etc/services add the line where <port> is the port you want the 
proxy to be on:

ziproxy <port>

then restart inetd.

 With xinetd

See the example config file included in this tarball.

 Daemon mode (standalone operation)

It is intended as simple inetd replacement if you want to use 
ziproxy under unprivileged user account. Every time you connect to 
internet, log in to remote machine and start it with command

- for daemon mode:

ziproxy -d -c 'somewhere/ziproxy.conf'

 Transparent proxy

In order to use Ziproxy as transparent proxy:
1. - In ziproxy.conf: TransparentProxy = true
2. - It's also needed to reroute the connections from
     x.x.x.x:80 to ziproxy.host:PROXY_PORT

Examples of traffic rerouting (Linux kernel >= 2.4 OSes):
THESE ARE INCOMPLETE SCRIPTS AND DO NOT PROVIDE ANY SECURITY !!!

### Requests from a local machine --> remote Ziproxy host
$ /sbin/modprobe ip_tables
$ /sbin/modprobe iptable_nat
$ IPTABLES=/usr/sbin/iptables
$ ZIPROXY_HOST=200.56.78.90
$ ZIPROXY_PORT=8080
$ $IPTABLES -t nat -A OUTPUT -s 0/0 -p tcp --dport 80 -j DNAT --to ${ZIPROXY_HOST}:${ZIPROXY_PORT}

### A transparent machine routing HTTP traffic AND running Ziproxy
$ /sbin/modprobe ip_tables
$ /sbin/modprobe iptable_nat
$ IPTABLES=/usr/sbin/iptables
$ NET_INTERFACE=eth0
$ ZIPROXY_HOST=200.56.78.90
$ ZIPROXY_PORT=8080
$ $IPTABLES -t nat -A PREROUTING -i $NET_INTERFACE -d ! $ZIPROXY_HOST -p tcp --dport 80 -j REDIRECT --to-port $ZIPROXY_PORT



 FAQ - Frequently Asked Questions

* Ziproxy is hogging my DNS!
Currently Ziproxy does not have an internal name cache so, for each request
it receives, it will try to resolve the hostname.
If your Ziproxy instance serves many users it may be a problem indeed.
You may solve this simply installing a local name cache
(a simple one - not a full DNS - will suffice).

* The pictures seem to come somewhat slower, specially with
  high-latency links.
Currently Ziproxy does not support persistent HTTP connections.
While this doesn't usually hurt WAN acceleration, single clients per link
may perceive a serialization effect of pictures so it feels slower.
A workaround is to increase the maximum simultaneous connections per proxy
(or server, if Ziproxy is running as a transparent proxy) in your web browser,
and to disable pipelining and keep-alive. Firefox is one browser which
supports such settings.

* Will Ziproxy ever cache pages? What if I need caching aswell?
There are no plans for such feature.
You may chain a caching proxy (such as Squid) to Ziproxy in order to have
such capability.

* Why is Ziproxy not gzipping text/html files sent to Squid?
Squid does not support gzip. Ziproxy detects that and does not gzip
data sent to Squid.
You may chain another Ziproxy (where appropriate) in order to "add"
gzip capability to Squid.

* Chaining Squid to Ziproxy does not work or it behaves strange.
Configure /etc/squid/squid.conf as follows:
--------------->8-----------------------
# "Hooks" Squid to Ziproxy running in 'localhost', port '8081'.
# Note: Ziproxy does not support ICP nor cache querying
# since it is not a caching proxy.
cache_peer localhost parent 8081 0 no-query no-digest

# Prevents Squid trying to access directly the remote HTTP host
# if Squid is unable to connect to Ziproxy,
# otherwise you won't know whether Ziproxy is down/has_problems
# (the lack of gzip support is not obvious to the user).
never_direct allow all
--------------->8-----------------------
Note: The other way around (Ziproxy to Squid) requires no special configuration.

* The pictures look awful after compression!
Ziproxy recompresses the pictures in order to reduce their sizes, naturally
some quality loss is expected.
You may customize the quality settings in order to match your taste.
Also, you may try using JPEG 2000 instead of conventional JPEG for better
quality.

* Can I run Ziproxy in a 16MB RAM 66Mhz MIPS-based router?
  (or some limited hardware like that)
If it runs an unix-ish OS, most probably you can (and some people do!).
Although Ziproxy is quite optimized, it cannot do miracles with slow hardware.
Please have reasonable expectations.

* Does Ziproxy provide QoS / traffic shaping?
Ziproxy does provide ToS marking based on certain HTTP characteristics of
each request. It's an optional feature, you must enable and configure that.
ToS marking alone does nothing, though. You'll need routers and/or servers
which treat the ToS-marked traffic differently, in order to implement QoS.
In other words, Ziproxy is part of the solution.


(end of documentation)

