/***************************************************************************
 *   Copyright (C) 2025 Jes Ramsing                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <wx/log.h>
#include <wx/string.h>
#include <wx/mstream.h>

#include "config.h"

#include "model/logger.h"

#include "model/rest_server_wms.h"

#include "mongoose.h"

#define RESTSERVERWMS

static const char* const ServerAddr = "http://0.0.0.0:8091";

unsigned int RestServerWms::m_hitcount = 0;

unsigned int RestServerWms::lastSize_W = 0;
unsigned int RestServerWms::lastSize_H = 0;

std::mutex RestServerWms::ret_mutex;

std::function<void(WmsReqParams)> RestServerWms::fCallback;

/** Extract a HTTP variable from query string. */
inline std::string HttpVarToString(const struct mg_str& query,
                                   const char* var) {
  std::string string;
  struct mg_str mgs = mg_http_var(query, mg_str(var));
  if (mgs.len && mgs.ptr) string = std::string(mgs.ptr, mgs.len);

  for (auto& c : string) {
    c = tolower(c);
  }

  return string;
}

// https://gist.github.com/onderaltintas/6649521//
void coord3857To4326(double lon3857, double lat3857, double& lon4326,
                     double& lat4326) {
  lon4326 = lon3857 * 180 / 20037508.34;
  // lat4326 = Math.atan(Math.exp(y * Math.PI / 20037508.34)) * 360 / Math.PI -
  // 90;
  lat4326 = atan(exp(lat3857 * PI / 20037508.34)) * 360 / PI - 90;
}

std::string unescape(const std::string& StrIn) {
  int newLen = 0;
  char* pszUnescaped = CPLUnescapeString(StrIn.c_str(), &newLen, CPLES_URL);
  std::string sNew(pszUnescaped);
  CPLFree(pszUnescaped);
  return sNew;
}

// entrypoint for mongoose
static void fn(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
#ifdef RESTSERVERWMS

  if (ev == MG_EV_HTTP_MSG) {
    std::lock_guard<std::mutex> lock =
        std::lock_guard<std::mutex>(RestServerWms::ret_mutex);
    ++RestServerWms::m_hitcount;

    struct mg_http_message* hm = (struct mg_http_message*)ev_data;

    if (mg_match(hm->uri, mg_str("/api/hitcount"), NULL)) {
      mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                    "{hitcount:%lu}\n", RestServerWms::m_hitcount);
    } else if (mg_match(hm->uri, mg_str("/api/wms"), NULL)) {
      try {
        if (mg_match(hm->uri, mg_str("favicon.ico"), NULL)) {
          mg_http_reply(c, 404, "", "");
          return;
        }

        std::string s = "wms req:" + std::string(hm->query.ptr, hm->query.len);
        DEBUG_LOG << s.c_str();

        std::string strService = HttpVarToString(hm->query, "service");
        std::string strVersion = HttpVarToString(hm->query, "version");
        std::string strRequest = HttpVarToString(hm->query, "request");

        // if a getcapabilities then process rapidly
        if (strRequest == "getcapabilities") {
          std::stringstream ss;
          ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
          ss << "<WMS_Capabilities version=\"1.1.1\">\n";
          ss << "<Service>\n";
          ss << "<Name>OGC:WMS</Name>\n";
          ss << "<Abstract>OpenCPN based Navigational Chart rendering "
                "engine</Abstract>\n";
          ss << "<Keywords>S57 OpenCPN WMS</Keywords>\n";
          ss << "<OnlineResource>https://github.com/Laro88/OpenCPN</"
                "OnlineResource>\n";
          ss << "<ContactInformation>github "
                "Laro88/OpenCPN</ContactInformation>\n";
          ss << "<Title>Using the awesome OpenCPN chartplotter with extensions "
                "to enable WMS rendering</Title>\n";
          ss << "</Service>\n";

          ss << "<Capability>\n";
          ss << "<Request>\n";
          ss << "<GetCapabilities>\n<Format>application/vnd.ogc.wms_xml";
          ss << "</Format>\n";
          ss << "<DCPType><HTTP><GET>";
          // ss << "<OnlineResource
          // xlink:href=\"http://hamster.com/xlink/\"/>\n";
          ss << "<OnlineResource xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                "xlink:type=\"simple\" "
                "xlink:href=\"http://localhost:8091/api/wms\"/>\n";

          ss << "</GET>\n</HTTP>\n</DCPType>\n";
          ss << "</GetCapabilities>\n";
          ss << "<GetMap>\n<Format>image/jpg</"
                "Format>\n<DCPType>\n<HTTP>\n<Get>\n";
          //<OnlineResource xmlns:xlink="http://www.w3.org/1999/xlink"
          // xlink:type="simple"
          // xlink:href="http://geoint.lmic.state.mn.us/cgi-bin/mncomp"/>
          ss << "<OnlineResource xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                "xlink:type=\"simple\" "
                "xlink:href=\"http://localhost:8091/api/wms\"/>\n";
          // ss << "<OnlineResource xlink:href=\"http://example.com/wms?\"/>\n";
          ss << "</Get>\n</HTTP>\n</DCPType>\n</GetMap>\n";
          ss << "</Request>\n";

          ss << "<Layer>\n";
          ss << "<Title>OpenCPN NAV layer</Title>\n";
          ss << "<Abstract>Navigational layer</Abstract>\n";
          ss << "<Name>NAV</Name>\n";
          ss << "<SRS>EPSG:3857</SRS>\n";
          ss << "<SRS>EPSG:4326</SRS>\n";
          ss << "<Format>image/jpeg</Format>\n";
          ss << "<Format>image/jpg</Format>\n";
          ss << "<BoundingBox SRS=\"EPSG:4326\" minx=\"-180.0\" "
                "miny=\"-90.0\" maxx=\"180.0\" maxy=\"90.0\"/>";
          ss << "</Layer>\n";
          ss << "</Capability>\n";
          ss << "</WMS_Capabilities>\n";

          mg_http_reply(c, 200, NULL, ss.str().c_str());
          return;
        } else if (strRequest == "getmap") {
          DEBUG_LOG << "getmap called" << std::endl;
          // regular map request (TODO check request = map / getmap / ??
          std::string strFormat = HttpVarToString(hm->query, "format");
          std::string strWidthPx = HttpVarToString(hm->query, "width");
          std::string strHeightPx = HttpVarToString(hm->query, "height");
          std::string strSrs = HttpVarToString(hm->query, "srs");
          std::string strBbox = HttpVarToString(hm->query, "bbox");
          std::string strColor = HttpVarToString(hm->query, "color");

          strBbox = unescape(strBbox);
          strSrs = unescape(strSrs);

          // check for resizing
          int _w = std::stoi(strWidthPx);
          int _h = std::stoi(strHeightPx);
          if (_w != RestServerWms::lastSize_W ||
              _h != RestServerWms::lastSize_H) {
            INFO_LOG << "Size req change  from (w,h)"
                     << RestServerWms::lastSize_W << ", "
                     << RestServerWms::lastSize_H << " to " << _w << ", " << _h;

            RestServerWms::lastSize_W = _w;
            RestServerWms::lastSize_H = _h;
          }

          // BBox manging
          std::stringstream ss(strBbox);
          std::vector<double> data;

          while (ss.good()) {
            std::string substr;
            getline(ss, substr, ',');
            double d = std::stod(substr);
            // bbox_split += substr + "\n";

            data.push_back(d);
          }

          if (data.size() != 4) {
            mg_http_reply(c, 422, "",
                          "Unable to continue, bbox data not having mandatory "
                          "in 4 params");
            return;
          }

          double lonSW, latSW, lonNE, latNE;

          if (strSrs == "epsg:4326") {
            latSW = data[0];
            lonSW = data[1];
            latNE = data[2];
            lonNE = data[3];

          } else if (strSrs == "epsg:3857") {
            // coord convertion
            coord3857To4326(data[0], data[1], lonSW, latSW);
            coord3857To4326(data[2], data[3], lonNE, latNE);
          } else {
            std::string err = "Unsupported Srs param:" + strSrs;

            mg_http_reply(c, 422, "", err.c_str());

            return;
          }

          INFO_LOG << "WMS req " << RestServerWms::m_hitcount << " SW:" << latSW
                   << "," << lonSW << " NE" << latNE << "," << lonNE
                   << "(lat, lon)";

          WmsReqParams p;
          p.w = _w;
          p.h = _h;
          p.latNE = latNE;
          p.lonNE = lonNE;
          p.latSW = latSW;
          p.lonSW = lonSW;
          p.hitcount = RestServerWms::m_hitcount;

          p.color = strColor;  // DAY" "DUSK" "NIGHT"

          p.c = c;

          DEBUG_LOG << "GetMap passing data on to callback" << std::endl;
          RestServerWms::fCallback(p);
        } else {  // final processing of unsupported request
          mg_http_reply(c, 400, NULL, "service not handled / unknown");
        }
      } catch (const std::exception& ex) {
        ERROR_LOG << "std::exception in rendering, details:" << ex.what();
      } catch (...) {
        ERROR_LOG << "... exception in rendering";
      }
    } else {
      mg_http_reply(c, 500, NULL, "\n");
    }
  }
#endif
}

//========================================================================
/* RestServer implementation */

void RestServerWms::Run() {
#ifdef RESTSERVERWMS
  struct mg_mgr mgr = {0};  // Event manager
  mg_log_set(MG_LL_DEBUG);  // Set log level
  mg_mgr_init(&mgr);        // Initialise event manager

  // Create HTTPS listener
  MESSAGE_LOG << "Listening on " << ServerAddr << "\n";
  mg_http_listen(&mgr, ServerAddr, fn, this);

  while (m_alive) {
    mg_mgr_poll(&mgr, 1);  // Infinite event loop //TODO set the time depending
                           // on activity bursts, WMS often load 9 times rapidly
  }
  mg_mgr_free(&mgr);
#endif
}

void RestServerWms::StopServer() {
  m_alive =
      false;  // TODO teardown might hang, need the rest_server style teardown
  if (m_workerthread.joinable()) {
    m_workerthread.join();
  }
}

RestServerWms::RestServerWms() {
  INFO_LOG << "RestServerWms construction, call to StartServer will start the "
              "rendering engine";
}

RestServerWms::~RestServerWms() { StopServer(); }

bool RestServerWms::StartServer(std::function<void(WmsReqParams)> FCallback) {
  RestServerWms::fCallback = FCallback;
  m_workerthread = std::thread([&]() { RestServerWms::Run(); });
  return true;
}
