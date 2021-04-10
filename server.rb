# frozen_string_literal: true

require 'sinatra'
require 'sinatra/json' # gem install sinatra-contrib

HTML_MOBILE_HEAD = %(<meta name="viewport" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no">\r\n)

HTML_OK               = 0x00
HTML_SUCCESS          = 0x01
HTML_UNAUTHORIZED     = 0x02
HTML_MISMATCH         = 0x03
HTML_DATA_MISSING     = 0x10
HTML_DATA_OUTOFBOUND  = 0x11
HTML_DATA_FORMATERROR = 0x12
HTML_RFCODE_ERROR     = 0x13
HTML_PAGE_NOT_FOUND   = 0x20
HTML_NOT_PERMITTED    = 0x30
HTML_UPLOAD_FAILED    = 0x40
HTML_REDIRECT_HOME    = 0xFF

HEADER_STANDARD_HEADERS = { 'Content-Type' => 'text/html',
                            'Cache-Control' => 'max-age=0, no-cache, no-store, must-revalidate',
                            'Access-Control-Allow-Origin' => '*' }.freeze
HTML_MOBILE_HEADER = "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,user-scalable=no\">\r\n"

OS_FW_VERSION = 219
DEFAULT_JAVASCRIPT_URL = 'https://ui.opensprinkler.com/js'
SOPT_JAVASCRIPTURL = DEFAULT_JAVASCRIPT_URL
DEFAULT_WEATHER_URL = 'weather.opensprinkler.com'
SOPT_WEATHERURL = DEFAULT_WEATHER_URL

# doc
class OpenSprinkler
  attr_accessor :iopts, :sopts
  def initialize
    @iopts = { ignore_password: false }
    @sopts = { password: 'opendoor' }
  end
end

os = OpenSprinkler.new

set :port, 8080

before do
  pass if [nil, 'db', 'su'].include? request.path_info.split('/')[1]
  redirect '/' unless os.opts[:ignore_password] || os.password_verify(params[:pw])
end

get '/' do
  # server_home
  headers HEADER_STANDARD_HEADERS
  %(<!DOCTYPE html>
      <html><head>#{HTML_MOBILE_HEAD}</head>
        <body>
          <script> var ver=#{OS_FW_VERSION},ipas=#{os.iopts[:ignore_password]}; </script>
          <script src="#{SOPT_JAVASCRIPTURL}/home.js"></script>
        </body>
    </html>)
end

# server_change_values,         // cv
# Change controller variables
# Command: /cv?pw=xxx&rsn=x&rbt=x&en=x&rd=x&re=x&ap=x
# pw:   password
# rsn:  reset all stations (0 or 1)
# rbt:  reboot controller (0 or 1)
# en:   enable (0 or 1)
# rd:   rain delay hours (0 turns off rain delay)
# re:   remote extension mode
# update: launch update script (for OSPi/OSBo/Linux only)
get '/cv' do
  reset_all_stations if params[:rsn]

  os.update_dev if params[:update]

  if params[:rbt]
    # print_html_standard_header();
    # bfill.emit_p(PSTR("Rebooting..."));
    # send_packet (true);
    os.reboot_dev REBOOT_CAUSE_WEB
  end

  case params[:en]
  when '1'
    os.enable
  when '0'
    os.disable
  end

  if params[:rd]
    rd = params[:rd].to_i
    if rd.positive?
      os.nvdata.rd_stop_time = os.now_tz + rd * 3600
      os.raindelay_start
    elsif rd.zero?
      os.raindelay_stop
    else
      json result: HTML_DATA_OUTOFBOUND
      return
    end
  end

  if params[:re]
    if params[:re] == '1' && !os.opts[:remote_ext_mode]
      os.iopts[:remote_ext_mode] = 1;
      os.iopts_save;
    elsif params[:re] == '0' && os.opts[:remote_ext_mode]
      os.iopts[:remote_ext_mode] = 0;
      os.iopts_save;
    end
  end

  json result: HTML_SUCCESS
end


# HELPER
def server_json_controller_main()
    curr_time = os.now_tz()
    retval = {
      devt:    curr_time,
      nbrd:    os.nbrds,
      en:      os.status.enabled,
      sn1:     os.status.sensor1_active,
      sn2:     os.status.sensor2_active,
      rd:      os.status.rain_delayed,
      rdst:    os.nvdata.rd_stop_time,
      sunrise: os.nvdata.sunrise_time,
      sunset:  os.nvdata.sunset_time,
      eip:     os.nvdata.external_ip,
      lwc:     os.checkwt_lasttime,
      lswc:    os.checkwt_success_lasttime,
      lupt:    os.powerup_lasttime,
      lrbtc:   os.last_reboot_cause,
      lrun:    [pd.lastrun.station,pd.lastrun.program, pd.lastrun.duration, pd.lastrun.endtime],
      loc:     SOPT_LOCATION,
      jsp:     SOPT_JAVASCRIPTURL,
      wsp:     SOPT_WEATHERURL,
      wto:     SOPT_WEATHER_OPTS,
      ifkey:   SOPT_IFTTT_KEY,
      mqtt:    SOPT_MQTT_OPTS,
      wtdata:  wt_rawData.empty? ? "{}" : wt_rawData,
      wterr:   wt_errCode,
      blynk:   SOPT_BLYNK_TOKEN,
      mqtt:    SOPT_MQTT_IP
    }

    mac = os.load_hardware_mac !m_server.nil?
    retval[:mac] = [mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]].join(':')

    if (os.iopts[IOPT_SENSOR1_TYPE] == SENSOR_TYPE_FLOW)
      retval[:flcrt] = os.flowcount_rt
      retval[:flwrt] = FLOWCOUNT_RT_WINDOW
    end

    retval[:sbits] = os.station_bits[0..os.nboards] + [0]
    retval[:ps] = [0..os.nstations].map do |sid|
      rem = 0
      qid = pd.station_qid[sid]
      q   = pd.queue + qid;
      if (qid < 255)
        rem = (curr_time >= q.st) ? (q.st + q.dur - curr_time) : q.dur
        if (rem > 65535)
          rem = 0;
        end
      end
      [(qid < 255) ? q.pid : 0, rem, (qid < 255) ? q.st : 0]
    end

    retval.to_json
end

# server_json_controller,       // jc
# Output controller variables in json
def server_json_controller()
  print_json_header
  server_json_controller_main
  json result: HTML_OK
end


# server_delete_program,        // dp
# Delete a program
#   Command: /dp?pw=xxx&pid=xxx
#   pw: password
#   pid:program index (-1 will delete all programs)
get '/dp' do
  if params[:pid].nil?
    json result: HTML_DATA_MISSING
  else
    pid = params[:pid].to_i
    if pid == -1
      pd.eraseall
    elsif pid < pd.nprograms
      pd.del pid
    else
      json result: HTML_DATA_OUTOFBOUND
      return
    end
    json result: HTML_SUCCESS
  end
end

# server_change_program,        // cp

# server_change_runonce,        // cr
# Change run-once program
# Command: /cr?pw=xxx&t=[x,x,x...]
#
# pw: password
# t:  station water time
get '/cr' do
    # reset all stations and prepare to run one-time program
    reset_all_stations_immediate

    match_found = false
    dur = (params[:t].sub(/[\[\]]/, '').split ',').map { |t| t.to_i }
    [0..os.nstations].each do |sid|
      bid = sid >> 3
      s   = sid & 0x07
      # if non-zero duration is given and if the station has not been disabled
      if (dur[sid] > 0 && !(os.attrib_dis[bid] & (1 << s)))
        q = pd.enqueue
        if q
          q.st  = 0
          q.dur = water_time_resolve dur[sid]
          q.pid = 254
          q.sid = sid
          match_found = true
        end
      end
    end

    if match_found
      schedule_all_stations os.now_tz
      json result: HTML_SUCCESS
      return
    end

    json result: HTML_DATA_MISSING
end

# server_manual_program,        // mp
# Manual start program
# Command: /mp?pw=xxx&pid=xxx&uwt=xxx
# pw:  password
# pid: program index (0 refers to the first program)
# uwt: use weather (i.e. watering percentage)
get '/mp' do
  if param[:pid].nil
    json result: HTML_DATA_MISSING
    return
  end

  pid = param[:pid].to_i
  unless (0..pd.nprograms-1).include? pid
    json result: HTML_DATA_OUTOFBOUND
    return
  end

  # reset all stations and prepare to run one-time program
  reset_all_stations_immediate

  manual_start_program pid + 1, param[:uwt]

  json result: HTML_SUCCESS
end

# server_moveup_program,        // up
# Move up a program
# Command: /up?pw=xxx&pid=xxx
#   pw:  password
#   pid: program index (must be 1 or larger, because we can't move up program 0)
get '/up' do
  if params[:pid].nil?
    json result: HTML_DATA_MISSING
  else
    pid = params[:pid].to_i
    if (1..pd.nprograms-1).include? pid
      pd.moveup pid
      json result: HTML_SUCCESS
    else
      json result: HTML_DATA_OUTOFBOUND
    end
  end
end

# server_json_programs,         // jp
# server_change_options,        // co
# server_json_options,          // jo

# server_change_password,       // sp
# Change password
#   Command: /sp?pw=xxx&npw=x&cpw=x
#   pw:  password
#   npw: new password
#   cpw: confirm new password
get '/sp' do
  if params[:npw].nil? || params[:cpw].nil?
    json result: HTML_DATA_MISSING
  elsif params[:npw] == params[:cpw]
    os.sopts[:password] = params[:npw]
    json result: HTML_SUCCESS
  else
    json result: HTML_MISMATCH
  end
end

# server_json_status,           // js

# server_change_manual,         // cm
# Test station (previously manual operation)
# Command: /cm?pw=xxx&sid=x&en=x&t=x
# pw: password
# sid:station index (starting from 0)
# en: enable (0 or 1)
# t:  timer (required if en=1)
get '/cm' do
  sid = -1
  if params[:sid].nil?
    json result: HTML_DATA_MISSING
    return
  else
    sid = params[:sid].to_i
    unless (0..os.nstations-1).include? sid
      json result: HTML_DATA_OUTOFBOUND
      return
    end
  end

  # FIXME
    byte en = 0;
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("en"), true)) {
        en = atoi (tmp_buffer);
    } else {
        handle_return (HTML_DATA_MISSING);
    }

    uint16_t      timer     = 0;
    unsigned long curr_time = os.now_tz();
    if (en) {  // if turning on a station, must provide timer
        if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("t"), true)) {
            timer = (uint16_t)atol (tmp_buffer);
            if (timer == 0 || timer > 64800) {
                handle_return (HTML_DATA_OUTOFBOUND);
            }
            // schedule manual station
            // skip if the station is a master station
            // (because master cannot be scheduled independently)
            if ((os.status.mas == sid + 1) || (os.status.mas2 == sid + 1))
                handle_return (HTML_NOT_PERMITTED);

            RuntimeQueueStruct * q   = NULL;
            byte                 sqi = pd.station_qid[sid];
            // check if the station already has a schedule
            if (sqi != 0xFF) {  // if we, we will overwrite the schedule
                q = pd.queue + sqi;
            } else {  // otherwise create a new queue element
                q = pd.enqueue();
            }
            // if the queue is not full
            if (q) {
                q->st  = 0;
                q->dur = timer;
                q->sid = sid;
                q->pid = 99;  // testing stations are assigned program index 99
                schedule_all_stations (curr_time);
            } else {
                handle_return (HTML_NOT_PERMITTED);
            }
        } else {
            handle_return (HTML_DATA_MISSING);
        }
    } else {  // turn off station
        turn_off_station (sid, curr_time);
    }
    handle_return (HTML_SUCCESS);
end

# server_change_stations,       // cs
# Change Station Name and Attributes
# Command: /cs?pw=xxx&s?=x&m?=x&i?=x&n?=x&d?=x
# pw: password
# s?: station name (? is station index, starting from 0)
# m?: master operation bit field (? is board index, starting from 0)
# i?: ignore rain bit field
# j?: ignore sensor1 bit field
# k?: ignore sensor2 bit field
# n?: master2 operation bit field
# d?: disable station bit field
# q?: station sequential bit field
# p?: station special flag bit field
get '/cs' do
  # process station names
  (0..os.nstations-1).each do |sid|
    os.set_station_name sid, params["s#{sid}"] unless params["s#{sid}"].nil?
  end

  (0..os.nboards-1).each do |bid|
    os.attrib_mas  = params["m#{bid}"].to_i unless params["m#{bid}"].nil?  # master1
    os.attrib_igrd = params["i#{bid}"].to_i unless params["i#{bid}"].nil?  # ignore rain delay
    os.attrib_igs  = params["j#{bid}"].to_i unless params["j#{bid}"].nil?  # ignore sensor1
    os.attrib_igs2 = params["k#{bid}"].to_i unless params["k#{bid}"].nil?  # ignore sensor2
    os.attrib_mas2 = params["n#{bid}"].to_i unless params["n#{bid}"].nil?  # master2
    os.attrib_dis  = params["d#{bid}"].to_i unless params["d#{bid}"].nil?  # disable
    os.attrib_seq  = params["q#{bid}"].to_i unless params["q#{bid}"].nil?  # sequential
    os.attrib_spe  = params["p#{bid}"].to_i unless params["p#{bid}"].nil?  # special
  end

  # handle special data
  if params[:sid]
    sid = params[:sid].to_i
    unless (0..os.nstations-1).include? sid
      json result: HTML_DATA_OUTOFBOUND
      return
    end
    if params[:st] && params[:sd]
      type = params[:st].to_i
      case type
      when STN_TYPE_GPIO
        gpio = params[:sd].to_i / 10
        active_state = params[:sd].to_i % 10
        if PIN_FREE_LIST.include?(gpio) && (0..1).include?(active_state)
          stations.set_station_data(gpio, active_state)
        else
          json result: HTML_DATA_OUTOFBOUND
          return
        end
      when STN_TYPE_HTTP
        stations.set_station_data params[:sd]
      else
        json result: HTML_DATA_OUTOFBOUND
        return
      end
      stations.write_file
    else
      json result: HTML_DATA_MISSING
      return
    end
  end

  os.attribs_save
  json result: HTML_SUCCESS
end

# server_json_stations,         // jn
# server_json_station_special,  // je

# server_json_log,              // jl
# Get log data
# Command: /jl?start=x&end=x&hist=x&type=x
#
# hist:  history (past n days)
#        when hist is speceified, the start
#        and end parameters below will be ignored
# start: start time (epoch time)
# end:   end time (epoch time)
# type:  type of log records (optional)
#        rs, rd, wl
#        if unspecified, output all records
get '/jl' do
  start_time = nil
  end_time = nil

  # past n day history
  if params[:hist]
    hist = params[:hist].to_i
    if hist < 0 || hist > 365
      json result: HTML_DATA_OUTOFBOUND
      return
    end
    end_time = os.now_tz / 86400
    start_time = end_time - hist
  else
    unless params[:start] && params[:end]
      json result: HTML_DATA_MISSING
      return
    end

    start_time = params[:start].to_i / 86400
    end_time = params[:end] / 86400

    # start must be prior to end, and can't retrieve more than 365 days of data
    if ((start_time > end_time) || (end_time - start_time) > 365)
      json result: HTML_DATA_OUTOFBOUND
      return
    end
  end

  # FIXME:
  print_json_header(false)

  bfill.emit_p (PSTR ("["));
  comma = false
  [start_time..end_time].each do |ii|
    tmp_buffer = make_logfile_name ii

    FILE * file = fopen (get_filename_fullpath (tmp_buffer), "rb");
    next unless file

    int res;
    while (true) {
            if (fgets (tmp_buffer, TMP_BUFFER_SIZE, file)) {
                 res = strlen (tmp_buffer);
               } else {
                res = 0;
            }
            if (res <= 0) {
                fclose (file);
                break;
            }
            # check record type
            # records are all in the form of [x,"xx",...]
            # where x is program index (>0) if this is a station record
            # and "xx" is the type name if this is a special record (e.g. wl, fl, rs)

            # search string until we find the first comma
            char * ptype                    = tmp_buffer;
            tmp_buffer[TMP_BUFFER_SIZE - 1] = 0;  // make sure the search will end
            while (*ptype && *ptype != ',') ptype++;
            if (*ptype != ',')
                continue;  // did not find comma, move on
            ptype++;       // move past comma

            next if params[:type] && (params[:type] != ptype)

            // if type is not specified, output everything except "wl" and "fl" records
            next if params[:type].nil? && ['wl', 'fl'].include(ptype)

            // if this is the first record, do not print comma
            if (comma)
                bfill.emit_p (PSTR (","));
            else {
                comma = 1;
            }

            bfill.emit_p (PSTR ("$S"), tmp_buffer);
              }

        end


    bfill.emit_p (PSTR ("]"));
    retval.to_json
 #   handle_return (HTML_OK);
end

# server_delete_log,            // dl
# Delete log
# Command: /dl?pw=xxx&day=xxx
#          /dl?pw=xxx&day=all
# pw: password
# day:day (epoch time / 86400)
#     if day=all: delete all log files
get '/dl' do
  if params[:day].nil?
    json result: HTML_DATA_MISSING
  else
    delete_log params[:day]
    json result: HTML_SUCCESS
  end
end

# server_view_scripturl,        // su - no password
# Output script url form
get '/su' do
  status HTML_OK
  headers HEADER_STANDARD_HEADERS
  %(<form name=of action=cu method=get>
      <table cellspacing="12">
        <tr><td><b>JavaScript</b>:</td><td><input type="text" size=40 maxlength=40 value="#{SOPT_JAVASCRIPTURL}" name="jsp"></td></tr>
        <tr><td>Default:</td><td>#{DEFAULT_JAVASCRIPT_URL}</td></tr>
        <tr><td><b>Weather</b>:</td><td><input type="text" size=40 maxlength=40 value="#{SOPT_WEATHERURL}" name="wsp"></td></tr>
        <tr><td>Default:</td><td>#{DEFAULT_WEATHER_URL}</td></tr>
        <tr><td><b>Password</b>:</td><td><input type="password" size=32 name="pw"> <input type="submit"></td></tr>
        </table>
    </form>
    <script src="https://ui.opensprinkler.com/js/hasher.js"></script>)
end

# server_change_scripturl,      // cu
# Change script url
# Command: /cu?pw=xxx&jsp=x
#
# pw:  password
# jsp: Javascript path
# wsp: Weather path
get '/cu' do
  os.sopt_save SOPT_JAVASCRIPTURL, params[:jsp] unless params[:jsp].nil?
  os.sopt_save SOPT_WEATHERURL, params[:wsp] unless params[:wsp].nil?
  redirect '/'
end

# server_json_all,              // ja



__END__


extern OpenSprinkler os;
extern ProgramData   pd;
extern ulong         flow_count;

BufferFiller bfill;

void print_json_header (bool bracket = true) {
    m_client->write ((const uint8_t *)html200OK, strlen (html200OK));
    m_client->write ((const uint8_t *)htmlContentJSON, strlen (htmlContentJSON));
    m_client->write ((const uint8_t *)htmlNoCache, strlen (htmlNoCache));
    m_client->write ((const uint8_t *)htmlAccessControl, strlen (htmlAccessControl));
    if (bracket)
        m_client->write ((const uint8_t *)"\r\n{", 3);
    else
        m_client->write ((const uint8_t *)"\r\n", 2);
}

void server_json_stations_attrib (const char * name, byte * attrib) {
    bfill.emit_p (PSTR ("\"$F\":["), name);
    for (byte i = 0; i < os.nboards; i++) {
        bfill.emit_p (PSTR ("$D"), attrib[i]);
        if (i != os.nboards - 1)
            bfill.emit_p (PSTR (","));
    }
    bfill.emit_p (PSTR ("],"));
}

void server_json_stations_main() {
    server_json_stations_attrib (PSTR ("masop"), os.attrib_mas);
    server_json_stations_attrib (PSTR ("masop2"), os.attrib_mas2);
    server_json_stations_attrib (PSTR ("ignore_rain"), os.attrib_igrd);
    server_json_stations_attrib (PSTR ("ignore_sn1"), os.attrib_igs);
    server_json_stations_attrib (PSTR ("ignore_sn2"), os.attrib_igs2);
    server_json_stations_attrib (PSTR ("stn_dis"), os.attrib_dis);
    server_json_stations_attrib (PSTR ("stn_seq"), os.attrib_seq);
    server_json_stations_attrib (PSTR ("stn_spe"), os.attrib_spe);

    bfill.emit_p (PSTR ("\"snames\":["));
    byte sid;
    for (sid = 0; sid < os.nstations; sid++) {
        os.get_station_name (sid, tmp_buffer);
        bfill.emit_p (PSTR ("\"$S\""), tmp_buffer);
        if (sid != os.nstations - 1)
            bfill.emit_p (PSTR (","));
        if (available_ether_buffer() < 60) {
            send_packet();
        }
    }
    bfill.emit_p (PSTR ("],\"maxlen\":$D}"), STATION_NAME_SIZE);
}

/** Output stations data */
void server_json_stations() {
    print_json_header();
    server_json_stations_main();
    handle_return (HTML_OK);
}

/** Output station special attribute */
void server_json_station_special() {
    byte          sid;
    byte          comma = 0;
    StationData * data  = (StationData *)tmp_buffer;
    print_json_header();
    for (sid = 0; sid < os.nstations; sid++) {
        if (os.get_station_type (sid) != STN_TYPE_STANDARD) {  // check if this is a special station
            os.get_station_data (sid, data);
            if (comma)
                bfill.emit_p (PSTR (","));
            else {
                comma = 1;
            }
            bfill.emit_p (PSTR ("\"$D\":{\"st\":$D,\"sd\":\"$S\"}"), sid, data->type, data->sped);
        }
    }
    bfill.emit_p (PSTR ("}"));
    handle_return (HTML_OK);
}


/** Parse one number from a comma separate list */
uint16_t parse_listdata (char ** p) {
    char * pv;
    int    i      = 0;
    tmp_buffer[i] = 0;
    // copy to tmp_buffer until a non-number is encountered
    for (pv = (*p); pv < (*p) + 10; pv++) {
        if ((*pv) == '-' || (*pv) == '+' || ((*pv) >= '0' && (*pv) <= '9'))
            tmp_buffer[i++] = (*pv);
        else
            break;
    }
    tmp_buffer[i] = 0;
    *p            = pv + 1;
    return (uint16_t)atol (tmp_buffer);
}

/**
 * Change a program
 * Command: /cp?pw=xxx&pid=x&v=[flag,days0,days1,[start0,start1,start2,start3],[dur0,dur1,dur2..]]&name=x
 *
 * pw:		password
 * pid:		program index
 * flag:	program flag
 * start?:up to 4 start times
 * dur?:	station water time
 * name:	program name
 */
const char _str_program[] PROGMEM = "Program ";
void       server_change_program() {
    char * p = get_buffer;

    byte i;

    ProgramStruct prog;

    // parse program index
    if (!findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("pid"), true))
        handle_return (HTML_DATA_MISSING);

    int pid = atoi (tmp_buffer);
    if (!(pid >= -1 && pid < pd.nprograms))
        handle_return (HTML_DATA_OUTOFBOUND);

    // check if "en" parameter is present
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("en"), true)) {
        if (pid < 0)
            handle_return (HTML_DATA_OUTOFBOUND);
        pd.set_flagbit (pid, PROGRAMSTRUCT_EN_BIT, (tmp_buffer[0] == '0') ? 0 : 1);
        handle_return (HTML_SUCCESS);
    }

    // check if "uwt" parameter is present
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("uwt"), true)) {
        if (pid < 0)
            handle_return (HTML_DATA_OUTOFBOUND);
        pd.set_flagbit (pid, PROGRAMSTRUCT_UWT_BIT, (tmp_buffer[0] == '0') ? 0 : 1);
        handle_return (HTML_SUCCESS);
    }

    // parse program name
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("name"), true)) {
        urlDecode (tmp_buffer);
        strncpy (prog.name, tmp_buffer, PROGRAM_NAME_SIZE);
    } else {
        strcpy_P (prog.name, _str_program);
        itoa ((pid == -1) ? (pd.nprograms + 1) : (pid + 1), prog.name + 8, 10);
    }

    // do a full string decoding
    if (p)
        urlDecode (p);

    // parse ad-hoc v=[...
    // search for the start of v=[
    char *  pv;
    boolean found = false;

    for (pv = p; (*pv) != 0 && pv < p + 100; pv++) {
        if (strncmp (pv, "v=[", 3) == 0) {
            found = true;
            break;
        }
    }

    if (!found)
        handle_return (HTML_DATA_MISSING);
    pv += 3;

    // parse headers
    *(char *)(&prog) = parse_listdata (&pv);
    prog.days[0]     = parse_listdata (&pv);
    prog.days[1]     = parse_listdata (&pv);
    // parse start times
    pv++;  // this should be a '['
    for (i = 0; i < MAX_NUM_STARTTIMES; i++) { prog.starttimes[i] = parse_listdata (&pv); }
    pv++;  // this should be a ','
    pv++;  // this should be a '['
    for (i = 0; i < os.nstations; i++) {
        uint16_t pre      = parse_listdata (&pv);
        prog.durations[i] = pre;
    }
    pv++;  // this should be a ']'
    pv++;  // this should be a ']'
    // parse program name

    // i should be equal to os.nstations at this point
    for (; i < MAX_NUM_STATIONS; i++) {
        prog.durations[i] = 0;  // clear unused field
    }

    // process interval day remainder (relative-> absolute)
    if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1) {
        pd.drem_to_absolute (prog.days);
    }

    if (pid == -1) {
        if (!pd.add (&prog))
            handle_return (HTML_DATA_OUTOFBOUND);
    } else {
        if (!pd.modify (pid, &prog))
            handle_return (HTML_DATA_OUTOFBOUND);
    }
    handle_return (HTML_SUCCESS);
}

void server_json_options_main() {
    byte oid;
    for (oid = 0; oid < NUM_IOPTS; oid++) {
        if (oid == IOPT_USE_NTP || oid == IOPT_USE_DHCP || (oid >= IOPT_STATIC_IP1 && oid <= IOPT_STATIC_IP4) ||
            (oid >= IOPT_GATEWAY_IP1 && oid <= IOPT_GATEWAY_IP4) || (oid >= IOPT_DNS_IP1 && oid <= IOPT_DNS_IP4) ||
            (oid >= IOPT_SUBNET_MASK1 && oid <= IOPT_SUBNET_MASK4))
            continue;

#if !defined(PIN_SENSOR2)
        // only OS 3.x or controllers that have PIN_SENSOR2 defined support sensor 2 options
        if (oid == IOPT_SENSOR2_TYPE || oid == IOPT_SENSOR2_OPTION || oid == IOPT_SENSOR2_ON_DELAY || oid == IOPT_SENSOR2_OFF_DELAY)
            continue;
#endif

        int32_t v = os.iopts[oid];
        if (oid == IOPT_MASTER_OFF_ADJ || oid == IOPT_MASTER_OFF_ADJ_2 || oid == IOPT_MASTER_ON_ADJ ||
            oid == IOPT_MASTER_ON_ADJ_2 || oid == IOPT_STATION_DELAY_TIME) {
            v = water_time_decode_signed (v);
        }

        if (oid == IOPT_BOOST_TIME)
            continue;

        if (oid == IOPT_SEQUENTIAL_RETIRED || oid == IOPT_URS_RETIRED || oid == IOPT_RSO_RETIRED)
            continue;

        // for Linux-based platforms, there is no LCD currently
        if (oid == IOPT_LCD_CONTRAST || oid == IOPT_LCD_BACKLIGHT || oid == IOPT_LCD_DIMMING)
            continue;

        // each json name takes 5 characters
        strncpy_P0 (tmp_buffer, iopt_json_names + oid * 5, 5);
        bfill.emit_p (PSTR ("\"$S\":$D"), tmp_buffer, v);
        if (oid != NUM_IOPTS - 1)
            bfill.emit_p (PSTR (","));
    }

    bfill.emit_p (PSTR (",\"dexp\":$D,\"mexp\":$D,\"hwt\":$D}"), os.detect_exp(), MAX_EXT_BOARDS, os.hw_type);
}

/** Output Options */
void server_json_options() {
    print_json_header();
    server_json_options_main();
    handle_return (HTML_OK);
}

void server_json_programs_main() {
    bfill.emit_p (PSTR ("\"nprogs\":$D,\"nboards\":$D,\"mnp\":$D,\"mnst\":$D,\"pnsize\":$D,\"pd\":["),
                  pd.nprograms,
                  os.nboards,
                  MAX_NUM_PROGRAMS,
                  MAX_NUM_STARTTIMES,
                  PROGRAM_NAME_SIZE);
    byte          pid, i;
    ProgramStruct prog;
    for (pid = 0; pid < pd.nprograms; pid++) {
        pd.read (pid, &prog);
        if (prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1) {
            pd.drem_to_relative (prog.days);
        }

        byte bytedata = *(char *)(&prog);
        bfill.emit_p (PSTR ("[$D,$D,$D,["), bytedata, prog.days[0], prog.days[1]);
        // start times data
        for (i = 0; i < (MAX_NUM_STARTTIMES - 1); i++) { bfill.emit_p (PSTR ("$D,"), prog.starttimes[i]); }
        bfill.emit_p (PSTR ("$D],["), prog.starttimes[i]);  // this is the last element
        // station water time
        for (i = 0; i < os.nstations - 1; i++) { bfill.emit_p (PSTR ("$L,"), (unsigned long)prog.durations[i]); }
        bfill.emit_p (PSTR ("$L],\""), (unsigned long)prog.durations[i]);  // this is the last element
        // program name
        strncpy (tmp_buffer, prog.name, PROGRAM_NAME_SIZE);
        tmp_buffer[PROGRAM_NAME_SIZE] = 0;  // make sure the string ends
        bfill.emit_p (PSTR ("$S"), tmp_buffer);
        if (pid != pd.nprograms - 1) {
            bfill.emit_p (PSTR ("\"],"));
        } else {
            bfill.emit_p (PSTR ("\"]"));
        }
        // push out a packet if available
        // buffer size is getting small
        if (available_ether_buffer() < 250) {
            send_packet();
        }
    }
    bfill.emit_p (PSTR ("]}"));
}

/** Output program data */
void server_json_programs() {
    print_json_header();
    server_json_programs_main();
    handle_return (HTML_OK);
}

/**
 * Change options
 * Command: /co?pw=xxx&o?=x&loc=x&ttt=x
 *
 * pw:	password
 * o?:	option name (? is option index)
 * loc: location
 * ttt: manual time (applicable only if ntp=0)
 */
void server_change_options() {
    char * p = get_buffer;

    // temporarily save some old options values
    bool time_change    = false;
    bool weather_change = false;
    bool sensor_change  = false;

    // ! p and bfill share the same buffer, so do not write
    // to bfill before you are done analyzing the buffer !
    // process option values
    byte err = 0;
    byte prev_value;
    byte max_value;
    for (byte oid = 0; oid < NUM_IOPTS; oid++) {

        // skip options that cannot be set through /co command
        if (oid == IOPT_FW_VERSION || oid == IOPT_HW_VERSION || oid == IOPT_SEQUENTIAL_RETIRED || oid == IOPT_DEVICE_ENABLE ||
            oid == IOPT_FW_MINOR || oid == IOPT_REMOTE_EXT_MODE || oid == IOPT_RESET || oid == IOPT_WIFI_MODE ||
            oid == IOPT_URS_RETIRED || oid == IOPT_RSO_RETIRED)
            continue;
        prev_value = os.iopts[oid];
        max_value  = pgm_read_byte (iopt_max + oid);

        // will no longer support oxx option names
        // json name only
        char tbuf2[6];
        strncpy_P0 (tbuf2, iopt_json_names + oid * 5, 5);
        if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, tbuf2)) {
            int32_t v = atol (tmp_buffer);
            if (oid == IOPT_MASTER_OFF_ADJ || oid == IOPT_MASTER_OFF_ADJ_2 || oid == IOPT_MASTER_ON_ADJ ||
                oid == IOPT_MASTER_ON_ADJ_2 || oid == IOPT_STATION_DELAY_TIME) {
                v = water_time_encode_signed (v);
            }  // encode station delay time
            if (oid == IOPT_BOOST_TIME) {
                v >>= 2;
            }
            if (v >= 0 && v <= max_value) {
                os.iopts[oid] = v;
            } else {
                err = 1;
            }
        }

        if (os.iopts[oid] != prev_value) {  // if value has changed
            if (oid == IOPT_TIMEZONE || oid == IOPT_USE_NTP)
                time_change = true;
            if (oid >= IOPT_NTP_IP1 && oid <= IOPT_NTP_IP4)
                time_change = true;
            if (oid == IOPT_USE_WEATHER)
                weather_change = true;
            if (oid >= IOPT_SENSOR1_TYPE && oid <= IOPT_SENSOR2_OFF_DELAY)
                sensor_change = true;
        }
    }

    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("loc"), true)) {
        urlDecode (tmp_buffer);
        if (os.sopt_save (SOPT_LOCATION, tmp_buffer)) {  // if location string has changed
            weather_change = true;
        }
    }
    uint8_t keyfound = 0;
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("wto"), true)) {
        urlDecode (tmp_buffer);
        if (os.sopt_save (SOPT_WEATHER_OPTS, tmp_buffer)) {
            weather_change = true;  // if wto has changed
        }
    }

    keyfound = 0;
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("ifkey"), true, &keyfound)) {
        urlDecode (tmp_buffer);
        os.sopt_save (SOPT_IFTTT_KEY, tmp_buffer);
    } else if (keyfound) {
        tmp_buffer[0] = 0;
        os.sopt_save (SOPT_IFTTT_KEY, tmp_buffer);
    }

    keyfound = 0;
    if (findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("mqtt"), true, &keyfound)) {
        urlDecode (tmp_buffer);
        os.sopt_save (SOPT_MQTT_OPTS, tmp_buffer);
        os.status.req_mqtt_restart = true;
    } else if (keyfound) {
        tmp_buffer[0] = 0;
        os.sopt_save (SOPT_MQTT_OPTS, tmp_buffer);
        os.status.req_mqtt_restart = true;
    }

    // if not using NTP and manually setting time
    if (!os.iopts[IOPT_USE_NTP] && findKeyVal (p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ("ttt"), true)) {
        // before chaging time, reset all stations to avoid messing up with timing
        reset_all_stations_immediate();
    }
    if (err)
        handle_return (HTML_DATA_OUTOFBOUND);

    os.iopts_save();

    if (time_change) {
        os.status.req_ntpsync = 1;
    }

    if (weather_change) {
        os.iopts[IOPT_WATER_PERCENTAGE] = 100;  // reset watering percentage to 100%
        wt_rawData[0]                   = 0;    // reset wt_rawData and errCode
        wt_errCode                      = HTTP_RQT_NOT_RECEIVED;
        os.checkwt_lasttime             = 0;  // force weather update
    }

    if (sensor_change) {
        os.sensor_resetall();
    }

    handle_return (HTML_SUCCESS);
}

void server_json_status_main() {
    bfill.emit_p (PSTR ("\"sn\":["));
    byte sid;

    for (sid = 0; sid < os.nstations; sid++) {
        bfill.emit_p (PSTR ("$D"), (os.station_bits[(sid >> 3)] >> (sid & 0x07)) & 1);
        if (sid != os.nstations - 1)
            bfill.emit_p (PSTR (","));
    }
    bfill.emit_p (PSTR ("],\"nstations\":$D}"), os.nstations);
}

/** Output station status */
void server_json_status() {
    print_json_header();
    server_json_status_main();
    handle_return (HTML_OK);
}

/** Output all JSON data, including jc, jp, jo, js, jn */
void server_json_all() {
    print_json_header();
    bfill.emit_p (PSTR ("\"settings\":{"));
    server_json_controller_main();
    send_packet();
    bfill.emit_p (PSTR (",\"programs\":{"));
    server_json_programs_main();
    send_packet();
    bfill.emit_p (PSTR (",\"options\":{"));
    server_json_options_main();
    send_packet();
    bfill.emit_p (PSTR (",\"status\":{"));
    server_json_status_main();
    send_packet();
    bfill.emit_p (PSTR (",\"stations\":{"));
    server_json_stations_main();
    bfill.emit_p (PSTR ("}"));
    handle_return (HTML_OK);
}

// handle Ethernet request
void handle_web_request (char * p) {
                } else if ((com[0] == 'j' && com[1] == 'o') ||
                           (com[0] == 'j' && com[1] == 'a')) {  // for /jo and /ja we output fwv if password fails
                    if (check_password (dat) == false) {
                        print_json_header();
                        bfill.emit_p (PSTR ("\"$F\":$D}"), iopt_json_names + 0, os.iopts[0]);
                        ret = HTML_OK;
                    } else {
                        get_buffer = dat;
                        (urls[i])();
                        ret = return_code;
                    }

        if (i == sizeof (urls) / sizeof (URLHandler)) {
            // no server funtion found
            print_json_header();
            bfill.emit_p (PSTR ("\"result\":$D}"), HTML_PAGE_NOT_FOUND);
        }
        send_packet (true);
    }
}
