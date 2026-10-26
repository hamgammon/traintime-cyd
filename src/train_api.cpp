#include "train_api.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Cooldown timer when public Huxley2 returns 5xx (15 minutes)
static unsigned long sHuxleyCooldownUntil = 0;
static bool sHuxleyInCooldown = false;

// Small cache of calling points by RailFeed service UID to avoid redundant HTTP calls
struct CachedCallingPoints {
    String uid;
    CallingPoint points[MAX_CALLING_POINTS];
    uint8_t count = 0;
};
static CachedCallingPoints sCpCache[MAX_SERVICES];

String TrainApiClient::stripHtmlAndClean(const String& input) {
    String out;
    out.reserve(input.length());
    bool inTag = false;
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input.charAt(i);
        if (c == '<') {
            inTag = true;
            continue;
        }
        if (c == '>') {
            inTag = false;
            out += ' ';
            continue;
        }
        if (!inTag) {
            if (c == '\r' || c == '\n' || c == '\t') {
                out += ' ';
            } else {
                out += c;
            }
        }
    }
    while (out.indexOf("  ") != -1) {
        out.replace("  ", " ");
    }
    out.replace("&amp;", "&");
    out.replace("&quot;", "\"");
    out.replace("&#39;", "'");
    out.replace("&nbsp;", " ");
    out.trim();
    return out;
}

String TrainApiClient::toTitleCase(const String& input) {
    String out = input;
    bool capNext = true;
    for (size_t i = 0; i < out.length(); ++i) {
        char c = out.charAt(i);
        if (isAlpha(c)) {
            out.setCharAt(i, capNext ? (char)toupper(c) : (char)tolower(c));
            capNext = false;
        } else if (c == ' ' || c == '-' || c == '/' || c == '(') {
            capNext = true;
        } else {
            capNext = false;
        }
    }
    out.replace(" Of ", " of ");
    out.replace(" And ", " and ");
    out.replace(" Upon ", " upon ");
    out.replace(" On ", " on ");
    out.replace(" In ", " in ");
    return out;
}

String TrainApiClient::formatHhMm(const char* hhmmss) {
    if (!hhmmss || strlen(hhmmss) < 5) {
        return "";
    }
    String s(hhmmss);
    return s.substring(0, 5);
}

String TrainApiClient::addMinutesToHhMm(const String& hhmm, int delayMins) {
    if (hhmm.length() < 5 || hhmm.charAt(2) != ':') {
        return hhmm;
    }
    int hh = hhmm.substring(0, 2).toInt();
    int mm = hhmm.substring(3, 5).toInt();
    int total = (hh * 60 + mm + delayMins) % (24 * 60);
    if (total < 0) {
        total += 24 * 60;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    return String(buf);
}

String TrainApiClient::buildTickerText(const TrainService& svc) {
    if (svc.isCancelled || svc.etd.equalsIgnoreCase("Cancelled")) {
        String msg = "SERVICE CANCELLED";
        if (!svc.cancelReason.isEmpty()) {
            msg += " - " + svc.cancelReason;
        }
        if (!svc.operatorName.isEmpty()) {
            msg += " (" + svc.operatorName + ")";
        }
        return msg;
    }

    String text = "Calling at: ";
    if (svc.callingPointCount == 0) {
        text += svc.destinationName + " only";
    } else {
        for (uint8_t i = 0; i < svc.callingPointCount; ++i) {
            const CallingPoint& cp = svc.callingPoints[i];
            if (i > 0) {
                text += (i == svc.callingPointCount - 1) ? " and " : ", ";
            }
            text += cp.locationName;
            if (cp.et.equalsIgnoreCase("Cancelled")) {
                text += " (Cancelled)";
            } else if (!cp.et.isEmpty() && !cp.et.equalsIgnoreCase("On time") && cp.et != cp.st) {
                text += " (Exp " + cp.et + ")";
            } else if (!cp.st.isEmpty()) {
                text += " (" + cp.st + ")";
            }
        }
    }

    if (svc.length > 0) {
        text += "  --  Formed of " + String(svc.length) + " coaches";
    }
    if (!svc.delayReason.isEmpty()) {
        text += "  --  " + svc.delayReason;
    }
    if (!svc.operatorName.isEmpty()) {
        text += "  [" + svc.operatorName + "]";
    }
    return text;
}

bool TrainApiClient::fetchDepartures(const AppConfig& config, BoardData& board) {
    if (WiFi.status() != WL_CONNECTED) {
        board.fetchSuccess = false;
        board.errorMessage = "Wi-Fi disconnected";
        return false;
    }

    // Try Huxley2 first unless the public demo server is in a 5xx cooldown and no personal token is set
    bool tryHuxley = !config.darwinToken.isEmpty() ||
                     !sHuxleyInCooldown ||
                     (long)(millis() - sHuxleyCooldownUntil) >= 0;

    if (tryHuxley) {
        if (fetchFromHuxley2(config, board)) {
            sHuxleyInCooldown = false;
            return true;
        }
        // Put public Huxley2 in a 15-minute cooldown if it failed without a custom token
        if (config.darwinToken.isEmpty()) {
            sHuxleyInCooldown = true;
            sHuxleyCooldownUntil = millis() + 900000UL;
        }
        Serial.println("[TrainAPI] Huxley2 unavailable, falling back to RailFeed API...");
    }

    return fetchFromRailFeed(config, board);
}

bool TrainApiClient::fetchFromHuxley2(const AppConfig& config, BoardData& board) {
    String baseUrl = config.huxleyUrl;
    if (baseUrl.isEmpty()) {
        baseUrl = DEFAULT_HUXLEY_URL;
    }
    while (baseUrl.endsWith("/")) {
        baseUrl.remove(baseUrl.length() - 1);
    }

    String crs = config.stationCrs;
    crs.toUpperCase();

    String url = baseUrl + "/departures/" + crs;
    if (config.filterCrs.length() == 3) {
        String filter = config.filterCrs;
        filter.toUpperCase();
        url += "/to/" + filter;
    }
    url += "/" + String(MAX_SERVICES) + "?expand=true";

    if (!config.darwinToken.isEmpty()) {
        url += "&accessToken=" + config.darwinToken;
    }

    // Declare network clients BEFORE HTTPClient so ~HTTPClient() runs first on scope exit
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    HTTPClient http;

    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.printf("[TrainAPI] Huxley2 GET: %s\n", url.c_str());

    bool begun = false;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        begun = http.begin(secureClient, url);
    } else {
        begun = http.begin(plainClient, url);
    }

    if (!begun) {
        board.fetchSuccess = false;
        board.errorMessage = "HTTP init failed";
        return false;
    }

    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "TrainTime-CYD/1.1");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        board.fetchSuccess = false;
        if (httpCode > 0) {
            board.errorMessage = "API Error HTTP " + String(httpCode);
        } else {
            board.errorMessage = "Network Error: " + http.errorToString(httpCode);
        }
        Serial.printf("[TrainAPI] Huxley2 %s\n", board.errorMessage.c_str());
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    secureClient.stop();

    JsonDocument filter;
    filter["locationName"] = true;
    filter["crs"] = true;
    filter["filterLocationName"] = true;
    filter["filtercrs"] = true;
    filter["areServicesAvailable"] = true;
    filter["nrccMessages"][0]["value"] = true;
    filter["nrccMessages"][0]["Value"] = true;

    JsonObject svcFilter = filter["trainServices"][0].to<JsonObject>();
    svcFilter["std"] = true;
    svcFilter["etd"] = true;
    svcFilter["platform"] = true;
    svcFilter["operator"] = true;
    svcFilter["operatorCode"] = true;
    svcFilter["isCancelled"] = true;
    svcFilter["cancelReason"] = true;
    svcFilter["delayReason"] = true;
    svcFilter["length"] = true;
    svcFilter["destination"][0]["locationName"] = true;
    svcFilter["destination"][0]["crs"] = true;
    svcFilter["destination"][0]["via"] = true;

    JsonObject cpFilter = svcFilter["subsequentCallingPoints"][0]["callingPoint"][0].to<JsonObject>();
    cpFilter["locationName"] = true;
    cpFilter["crs"] = true;
    cpFilter["st"] = true;
    cpFilter["et"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc,
        payload,
        DeserializationOption::Filter(filter)
    );
    payload = "";

    if (err) {
        board.fetchSuccess = false;
        board.errorMessage = String("JSON Error: ") + err.c_str();
        return false;
    }

    board.crs = doc["crs"] | crs;
    board.crs.toUpperCase();

    const char* locName = doc["locationName"];
    if (locName && strlen(locName) > 0) {
        board.locationName = locName;
    } else {
        const char* presetName = lookupStationName(board.crs);
        board.locationName = presetName ? String(presetName) : board.crs;
    }

    board.filterLocationName = doc["filterLocationName"] | "";
    board.filterCrs = doc["filtercrs"] | config.filterCrs;
    board.areServicesAvailable = doc["areServicesAvailable"] | true;

    board.nrccMessage = "";
    JsonArray nrccArr = doc["nrccMessages"].as<JsonArray>();
    if (!nrccArr.isNull() && nrccArr.size() > 0) {
        const char* rawMsg = nrccArr[0]["value"] | nrccArr[0]["Value"] | "";
        if (strlen(rawMsg) > 0) {
            board.nrccMessage = stripHtmlAndClean(String(rawMsg));
        }
    }

    board.serviceCount = 0;
    JsonArray servicesArr = doc["trainServices"].as<JsonArray>();
    if (!servicesArr.isNull()) {
        for (JsonObject svcObj : servicesArr) {
            if (board.serviceCount >= MAX_SERVICES) {
                break;
            }
            TrainService& svc = board.services[board.serviceCount];
            svc.std = svcObj["std"] | "--:--";
            svc.etd = svcObj["etd"] | "On time";
            svc.platform = svcObj["platform"] | "";
            svc.operatorName = svcObj["operator"] | "";
            svc.operatorCode = svcObj["operatorCode"] | "";
            svc.isCancelled = svcObj["isCancelled"] | false;
            svc.cancelReason = stripHtmlAndClean(svcObj["cancelReason"] | "");
            svc.delayReason = stripHtmlAndClean(svcObj["delayReason"] | "");
            svc.length = svcObj["length"] | 0;

            svc.destinationName = "Unknown";
            svc.destinationCrs = "";
            svc.via = "";
            JsonArray destArr = svcObj["destination"].as<JsonArray>();
            if (!destArr.isNull() && destArr.size() > 0) {
                svc.destinationName = destArr[0]["locationName"] | "Unknown";
                svc.destinationCrs = destArr[0]["crs"] | "";
                svc.via = destArr[0]["via"] | "";
                if (destArr.size() > 1) {
                    const char* secondDest = destArr[1]["locationName"] | "";
                    if (strlen(secondDest) > 0) {
                        svc.destinationName += String(" & ") + secondDest;
                    }
                }
            }

            svc.callingPointCount = 0;
            JsonArray subCpList = svcObj["subsequentCallingPoints"].as<JsonArray>();
            if (!subCpList.isNull() && subCpList.size() > 0) {
                JsonArray cpArr = subCpList[0]["callingPoint"].as<JsonArray>();
                if (!cpArr.isNull()) {
                    for (JsonObject cpObj : cpArr) {
                        if (svc.callingPointCount >= MAX_CALLING_POINTS) {
                            break;
                        }
                        CallingPoint& cp = svc.callingPoints[svc.callingPointCount++];
                        cp.locationName = cpObj["locationName"] | "";
                        cp.crs = cpObj["crs"] | "";
                        cp.st = cpObj["st"] | "";
                        cp.et = cpObj["et"] | "";
                    }
                }
            }

            svc.tickerText = buildTickerText(svc);
            board.serviceCount++;
        }
    }

    board.fetchSuccess = true;
    board.errorMessage = "";
    board.lastUpdatedMillis = millis();
    return true;
}

// ============================================================================
// RailFeed Fallback Provider (https://railfeed.co.uk)
// ============================================================================

bool TrainApiClient::fetchFromRailFeed(const AppConfig& config, BoardData& board) {
    String crs = config.stationCrs;
    crs.toUpperCase();
    String filterCrs = config.filterCrs;
    filterCrs.toUpperCase();

    // First try a 2-hour window (compact & fast during the day)
    if (!fetchRailFeedWindow(crs, filterCrs, 2, board)) {
        return false;
    }

    // If fewer than 3 passenger services found (e.g. overnight or rural station), expand to 10 hours
    if (board.serviceCount < 3) {
        fetchRailFeedWindow(crs, filterCrs, 10, board);
    }

    return board.fetchSuccess;
}

bool TrainApiClient::fetchRailFeedWindow(const String& crs, const String& filterCrs, uint8_t hours, BoardData& board) {
    String url = "https://railfeed.co.uk/data/live_board?crs=" + crs +
                 "&hours=" + String(hours) + "&type=departures";

    WiFiClientSecure secureClient;
    HTTPClient http;

    secureClient.setInsecure();
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true); // Stream unchunked JSON directly into ArduinoJson filter

    Serial.printf("[TrainAPI] RailFeed GET: %s\n", url.c_str());

    if (!http.begin(secureClient, url)) {
        board.fetchSuccess = false;
        board.errorMessage = "HTTP init failed";
        return false;
    }

    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "TrainTime-CYD/1.1");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        board.fetchSuccess = false;
        board.errorMessage = (httpCode > 0)
            ? ("API Error HTTP " + String(httpCode))
            : ("Network Error: " + http.errorToString(httpCode));
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["station"] = true;
    filter["date"] = true;

    JsonObject svcFilter = filter["services"][0].to<JsonObject>();
    svcFilter["uid"] = true;
    svcFilter["toc_name"] = true;
    svcFilter["toc_id"] = true;
    svcFilter["toc_type"] = true;
    svcFilter["destination"] = true;
    svcFilter["destination_crs"] = true;
    svcFilter["public_departure"] = true;
    svcFilter["scheduled_departure"] = true;
    svcFilter["actual_departure"] = true;
    svcFilter["projected_departure"] = true;
    svcFilter["delay_mins"] = true;
    svcFilter["projected_delay_mins"] = true;
    svcFilter["platform"] = true;
    svcFilter["actual_platform"] = true;
    svcFilter["live_platform"] = true;
    svcFilter["is_pass"] = true;
    svcFilter["status"] = true;
    svcFilter["cancel_reason"] = true;
    svcFilter["late_reason"] = true;
    svcFilter["consist"][0]["vehicle_count"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc,
        http.getStream(),
        DeserializationOption::Filter(filter)
    );
    http.end();
    secureClient.stop();

    if (err) {
        board.fetchSuccess = false;
        board.errorMessage = String("JSON Error: ") + err.c_str();
        Serial.printf("[TrainAPI] RailFeed parse error: %s\n", err.c_str());
        return false;
    }

    board.crs = crs;
    const char* rawStation = doc["station"] | "";
    if (strlen(rawStation) > 0) {
        board.locationName = toTitleCase(String(rawStation));
    } else {
        const char* presetName = lookupStationName(crs);
        board.locationName = presetName ? String(presetName) : crs;
    }

    board.filterCrs = filterCrs;
    board.filterLocationName = "";
    board.areServicesAvailable = true;
    board.nrccMessage = "";
    board.serviceCount = 0;

    String boardDate = doc["date"] | "";
    String serviceUids[MAX_SERVICES];

    JsonArray servicesArr = doc["services"].as<JsonArray>();
    if (!servicesArr.isNull()) {
        for (JsonObject svcObj : servicesArr) {
            if (board.serviceCount >= MAX_SERVICES) {
                break;
            }

            // Skip non-stopping pass-throughs and non-passenger/empty-stock movements
            bool isPass = svcObj["is_pass"] | false;
            if (isPass) {
                continue;
            }
            const char* pubDep = svcObj["public_departure"] | "";
            if (strlen(pubDep) < 5) {
                continue;
            }
            const char* tocType = svcObj["toc_type"] | "PASSENGER";
            if (strcmp(tocType, "FREIGHT") == 0) {
                continue;
            }

            // If destination equals origin (e.g. terminating arrival), skip
            const char* destCrsRaw = svcObj["destination_crs"] | "";
            if (crs.equalsIgnoreCase(destCrsRaw)) {
                continue;
            }

            TrainService& svc = board.services[board.serviceCount];
            String uid = svcObj["uid"] | "";
            serviceUids[board.serviceCount] = uid;

            svc.std = formatHhMm(pubDep);
            svc.destinationName = toTitleCase(String(svcObj["destination"] | "Unknown"));
            svc.destinationCrs = destCrsRaw;
            svc.via = "";
            svc.operatorName = svcObj["toc_name"] | "";
            svc.operatorCode = svcObj["toc_id"] | "";

            // Platform precedence: live_platform -> actual_platform -> booked platform
            const char* livePlat = svcObj["live_platform"] | "";
            const char* actPlat = svcObj["actual_platform"] | "";
            const char* bookedPlat = svcObj["platform"] | "";
            if (strlen(livePlat) > 0) {
                svc.platform = livePlat;
            } else if (strlen(actPlat) > 0) {
                svc.platform = actPlat;
            } else {
                svc.platform = bookedPlat;
            }

            // Status / ETD
            String statusStr = svcObj["status"] | "expected";
            svc.isCancelled = statusStr.equalsIgnoreCase("cancelled");
            svc.cancelReason = stripHtmlAndClean(svcObj["cancel_reason"] | "");
            svc.delayReason = stripHtmlAndClean(svcObj["late_reason"] | "");

            int delayMins = svcObj["delay_mins"] | 0;
            int projDelayMins = svcObj["projected_delay_mins"] | 0;
            const char* actDep = svcObj["actual_departure"] | "";
            const char* projDep = svcObj["projected_departure"] | "";

            if (svc.isCancelled) {
                svc.etd = "Cancelled";
            } else if (strlen(actDep) >= 5 && delayMins > 0) {
                svc.etd = formatHhMm(actDep);
            } else if (strlen(projDep) >= 5 && (projDelayMins > 0 || delayMins > 0)) {
                svc.etd = formatHhMm(projDep);
            } else if (delayMins > 0) {
                svc.etd = addMinutesToHhMm(svc.std, delayMins);
            } else {
                svc.etd = "On time";
            }

            // Sum coach count from consist array
            svc.length = 0;
            JsonArray consistArr = svcObj["consist"].as<JsonArray>();
            if (!consistArr.isNull()) {
                for (JsonObject cObj : consistArr) {
                    uint8_t vc = cObj["vehicle_count"] | 0;
                    svc.length += vc;
                }
            }

            svc.callingPointCount = 0;
            board.serviceCount++;
        }
    }

    // Free live_board JSON document before fetching calling points
    doc.clear();

    // Populate calling points for each service on the board (using cache or service_live)
    uint8_t writeIdx = 0;
    for (uint8_t i = 0; i < board.serviceCount; ++i) {
        TrainService& svc = board.services[i];
        const String& uid = serviceUids[i];

        bool foundInCache = false;
        // Always refresh the 1st service's live calling points; use cache for rows 2..5
        if (i > 0 && !uid.isEmpty()) {
            for (uint8_t c = 0; c < MAX_SERVICES; ++c) {
                if (sCpCache[c].uid == uid && sCpCache[c].count > 0) {
                    svc.callingPointCount = sCpCache[c].count;
                    for (uint8_t k = 0; k < svc.callingPointCount; ++k) {
                        svc.callingPoints[k] = sCpCache[c].points[k];
                    }
                    foundInCache = true;
                    break;
                }
            }
        }

        if (!foundInCache && !uid.isEmpty() && !boardDate.isEmpty()) {
            if (fetchRailFeedCallingPoints(uid, boardDate, crs, svc)) {
                sCpCache[i].uid = uid;
                sCpCache[i].count = svc.callingPointCount;
                for (uint8_t k = 0; k < svc.callingPointCount; ++k) {
                    sCpCache[i].points[k] = svc.callingPoints[k];
                }
            }
        }

        // If user set a 3-letter "calling at" filterCrs, keep only services calling there
        if (filterCrs.length() == 3) {
            bool callsAtFilter = svc.destinationCrs.equalsIgnoreCase(filterCrs);
            for (uint8_t k = 0; k < svc.callingPointCount && !callsAtFilter; ++k) {
                if (svc.callingPoints[k].crs.equalsIgnoreCase(filterCrs)) {
                    callsAtFilter = true;
                }
            }
            if (!callsAtFilter) {
                continue;
            }
        }

        svc.tickerText = buildTickerText(svc);
        if (writeIdx != i) {
            board.services[writeIdx] = svc;
        }
        writeIdx++;
    }
    board.serviceCount = writeIdx;

    board.fetchSuccess = true;
    board.errorMessage = "";
    board.lastUpdatedMillis = millis();
    return true;
}

bool TrainApiClient::fetchRailFeedCallingPoints(
    const String& uid,
    const String& dateStr,
    const String& originCrs,
    TrainService& svc
) {
    String url = "https://railfeed.co.uk/data/service_live?uid=" + uid + "&date=" + dateStr;

    WiFiClientSecure secureClient;
    HTTPClient http;

    secureClient.setInsecure();
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);

    if (!http.begin(secureClient, url)) {
        return false;
    }

    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "TrainTime-CYD/1.1");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    // Stream filter ignores route_from_prev GPS polylines so RAM usage stays tiny
    JsonDocument filter;
    filter["cancel_reason"] = true;
    filter["late_reason"] = true;
    JsonObject stopFilter = filter["stops"][0].to<JsonObject>();
    stopFilter["location_name"] = true;
    stopFilter["crs"] = true;
    stopFilter["call_type"] = true;
    stopFilter["public_arrival"] = true;
    stopFilter["public_departure"] = true;
    stopFilter["actual_arrival"] = true;
    stopFilter["actual_departure"] = true;
    stopFilter["arrival_delay_mins"] = true;
    stopFilter["departure_delay_mins"] = true;
    stopFilter["status"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc,
        http.getStream(),
        DeserializationOption::Filter(filter)
    );
    http.end();
    secureClient.stop();

    if (err) {
        return false;
    }

    if (svc.cancelReason.isEmpty()) {
        svc.cancelReason = stripHtmlAndClean(doc["cancel_reason"] | "");
    }
    if (svc.delayReason.isEmpty()) {
        svc.delayReason = stripHtmlAndClean(doc["late_reason"] | "");
    }

    svc.callingPointCount = 0;
    bool passedCurrentStation = false;

    JsonArray stopsArr = doc["stops"].as<JsonArray>();
    if (!stopsArr.isNull()) {
        for (JsonObject stObj : stopsArr) {
            const char* stopCrs = stObj["crs"] | "";
            if (!passedCurrentStation) {
                if (originCrs.equalsIgnoreCase(stopCrs)) {
                    passedCurrentStation = true;
                }
                continue;
            }

            const char* callType = stObj["call_type"] | "";
            if (strcmp(callType, "CALL") != 0 && strcmp(callType, "TERMINATES") != 0) {
                continue;
            }

            const char* pubArr = stObj["public_arrival"] | "";
            const char* pubDep = stObj["public_departure"] | "";
            const char* bookedTime = (strlen(pubArr) >= 5) ? pubArr : pubDep;
            if (strlen(bookedTime) < 5) {
                continue;
            }

            if (svc.callingPointCount >= MAX_CALLING_POINTS) {
                break;
            }

            CallingPoint& cp = svc.callingPoints[svc.callingPointCount++];
            cp.locationName = toTitleCase(String(stObj["location_name"] | stopCrs));
            cp.crs = stopCrs;
            cp.st = formatHhMm(bookedTime);

            String stopStatus = stObj["status"] | "expected";
            int arrDelay = stObj["arrival_delay_mins"] | (stObj["departure_delay_mins"] | 0);
            const char* actArr = stObj["actual_arrival"] | (stObj["actual_departure"] | "");

            if (stopStatus.equalsIgnoreCase("cancelled")) {
                cp.et = "Cancelled";
            } else if (strlen(actArr) >= 5 && arrDelay > 0) {
                cp.et = formatHhMm(actArr);
            } else if (arrDelay > 0) {
                cp.et = addMinutesToHhMm(cp.st, arrDelay);
            } else {
                cp.et = "On time";
            }
        }
    }

    return true;
}
