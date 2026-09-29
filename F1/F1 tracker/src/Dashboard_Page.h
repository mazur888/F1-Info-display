#pragma once

#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

extern WebServer server;
extern String    API_BASE;
extern String    API_RACES;
extern String    API_DRIVER_STAND;
extern String    API_CONSTR_STAND;
extern unsigned  lastRound, nextRound;
extern String    nextName, nextDate, nextTime;

bool httpGetJson(const char* url, JsonDocument& target);
void FetchCalendar();

inline void handleF1Page() {
  JsonDocument calDoc;
  if (!httpGetJson(API_RACES.c_str(), calDoc)) {
    server.send(500, "text/plain", "Calendar fetch failed");
    return;
  }
  JsonArray races = calDoc["MRData"]["RaceTable"]["Races"].as<JsonArray>();

  FetchCalendar();

  JsonDocument ddoc;
  httpGetJson(API_DRIVER_STAND.c_str(), ddoc);
  JsonObject topD = ddoc["MRData"]["StandingsTable"]
                     ["StandingsLists"][0]
                     ["DriverStandings"][0]
                     .as<JsonObject>();
  String topDrv = String(topD["Driver"]["familyName"].as<const char*>())
                + " — " + topD["points"].as<const char*>() + "p";

  JsonDocument cdoc;
  httpGetJson(API_CONSTR_STAND.c_str(), cdoc);
  JsonObject topC = cdoc["MRData"]["StandingsTable"]
                     ["StandingsLists"][0]
                     ["ConstructorStandings"][0]
                     .as<JsonObject>();
  String topCon = String(topC["Constructor"]["name"].as<const char*>())
                + " — " + topC["points"].as<const char*>() + "p";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  server.sendContent(R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>F1 Dashboard</title>
<style>
  :root {
    --f1-red: #e10600;
    --dark-bg: #1a1a1a;
    --card-bg: #2c2c2c;
    --text-primary: #e0e0e0;
    --text-secondary: #b3b3b3;
    --border-color: #444;
    --radius: 8px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: Arial, sans-serif; background: var(--dark-bg); color: var(--text-primary); line-height: 1.6; }
  .container { max-width: 1200px; margin: auto; padding: 20px; }
  header { text-align: center; margin-bottom: 40px; }
  header h1 { font-size: 2.5em; text-transform: uppercase; letter-spacing: 2px; }
  h2 { font-size: 1.8em; border-bottom: 3px solid var(--f1-red); padding-bottom: 10px; margin-bottom: 20px; }

  .summary-cards { display: flex; gap: 20px; margin-bottom: 40px; }
  .card { flex: 1; background: var(--card-bg); border-radius: var(--radius); padding: 20px;
          border: 1px solid var(--border-color); box-shadow: 2px 2px 8px rgba(0,0,0,0.5); }
  .card h2 { font-size: 1.4em; margin-bottom: 10px; color: white; }
  .card p { font-size: 1em; color: var(--text-secondary); }

  .grid { display: grid; grid-template-columns: repeat(3,1fr); gap: 20px; margin-bottom: 40px; }
  .tile { background: var(--card-bg); border-radius: var(--radius); padding: 20px;
          border: 1px solid var(--border-color); border-top: 4px solid var(--f1-red);
          box-shadow: 2px 2px 6px rgba(0,0,0,0.4); transition: transform .2s, box-shadow .2s; }
  .tile:hover { transform: translateY(-5px); box-shadow: 0 10px 20px rgba(0,0,0,0.6); }
  .tile h3 { margin-bottom: 10px; font-size: 1.2em; color: white; }
  .tile p { margin: 5px 0; font-size: .9em; color: var(--text-secondary); }
  .tile p strong { color: var(--text-primary); }

  .standings { display: flex; gap: 20px; flex-wrap: wrap; margin-bottom: 40px; }
  .standings-table { flex: 1; min-width: 300px; background: var(--card-bg);
                     border-radius: var(--radius); padding: 20px; border: 1px solid var(--border-color); }
  .standings-table h3 { margin-bottom: 15px; color: white; }
  table { width: 100%; border-collapse: collapse; }
  th, td { padding: 8px; text-align: left; border-bottom: 1px solid var(--border-color); }
  th { font-size: .8em; color: var(--text-secondary); text-transform: uppercase; }

  footer { text-align: center; color: var(--text-secondary); font-size: .9em; margin-top: 40px; }

  @media(max-width:1024px) { .grid { grid-template-columns: repeat(2,1fr); } }
  @media(max-width:768px)  { .grid { grid-template-columns: 1fr; } h2 { font-size: 1.4em; } }
</style>
</head>
<body>
  <div class="container">
    <header><h1>F1 Dashboard</h1></header>
    <main>
)rawliteral");

  {
    char buf[512];
    snprintf(buf, sizeof(buf),
      "<div class=\"summary-cards\">"
        "<div class=\"card\">"
          "<h2>Next Race</h2>"
          "<p>%s<br>%s (Round %u)</p>"
          "<p id=\"countdown\" style=\"font-size:1.2em;color:var(--f1-red)\"></p>"
        "</div>"
        "<div class=\"card\"><h2>Top Driver</h2><p>%s</p></div>"
        "<div class=\"card\"><h2>Top Constructor</h2><p>%s</p></div>"
      "</div>"
      "<section><h2>Season Calendar</h2><div class=\"grid\">",
      nextName.c_str(), nextDate.c_str(), nextRound,
      topDrv.c_str(), topCon.c_str()
    );
    server.sendContent(buf);
  }

  for (JsonVariant vr : races) {
    JsonObject r = vr.as<JsonObject>();
    const char* rnd  = r["round"];
    const char* nm   = r["raceName"];
    const char* dt   = r["date"];

    // pole
    JsonDocument qd;
    httpGetJson((API_BASE + "/" + rnd + "/qualifying/").c_str(), qd);
    JsonArray qa = qd["MRData"]["RaceTable"]["Races"][0]
                     ["QualifyingResults"]
                     .as<JsonArray>();
    String pole = qa.size()
                ? qa[0]["Driver"]["familyName"].as<const char*>()
                : "TBD";

    // podium
    JsonDocument rd;
    httpGetJson((API_BASE + "/" + rnd + "/results/").c_str(), rd);
    JsonArray ra = rd["MRData"]["RaceTable"]["Races"][0]
                     ["Results"]
                     .as<JsonArray>();
    String p1 = ra.size()>0 ? ra[0]["Driver"]["familyName"].as<const char*>() : "—";
    String p2 = ra.size()>1 ? ra[1]["Driver"]["familyName"].as<const char*>() : "—";
    String p3 = ra.size()>2 ? ra[2]["Driver"]["familyName"].as<const char*>() : "—";

    char tile[512];
    snprintf(tile, sizeof(tile),
      "<div class=\"tile\">"
        "<h3>Round %s: %s</h3>"
        "<p><strong>Date:</strong> %s</p>"
        "<p><strong>Pole:</strong> %s</p>"
        "<p><strong>Podium:</strong> %s, %s, %s</p>"
      "</div>",
      rnd, nm, dt, pole.c_str(), p1.c_str(), p2.c_str(), p3.c_str()
    );
    server.sendContent(tile);
  }
  server.sendContent("</div></section>");

  // Driver standings table
  server.sendContent(R"rawliteral(
    <section><h2>Championship Standings</h2>
    <div class="standings">
      <div class="standings-table">
        <h3>Driver Standings</h3>
        <table><thead>
          <tr><th>#</th><th>Driver</th><th>Pts</th><th>W</th></tr>
        </thead><tbody>
)rawliteral");
  {
    for (JsonVariant ve : ddoc["MRData"]["StandingsTable"]
                         ["StandingsLists"][0]
                         ["DriverStandings"]
                         .as<JsonArray>()) {
      JsonObject e = ve.as<JsonObject>();
      char row[128];
      snprintf(row, sizeof(row),
        "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
        e["position"].as<const char*>(),
        e["Driver"]["familyName"].as<const char*>(),
        e["points"].as<const char*>(),
        e["wins"].as<const char*>()
      );
      server.sendContent(row);
    }
  }
  server.sendContent("</tbody></table></div>");

  // Constructor standings table
  server.sendContent(R"rawliteral(
      <div class="standings-table">
        <h3>Constructor Standings</h3>
        <table><thead>
          <tr><th>#</th><th>Team</th><th>Pts</th><th>W</th></tr>
        </thead><tbody>
)rawliteral");
  {
    for (JsonVariant ve : cdoc["MRData"]["StandingsTable"]
                         ["StandingsLists"][0]
                         ["ConstructorStandings"]
                         .as<JsonArray>()) {
      JsonObject e = ve.as<JsonObject>();
      char row[128];
      snprintf(row, sizeof(row),
        "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",
        e["position"].as<const char*>(),
        e["Constructor"]["name"].as<const char*>(),
        e["points"].as<const char*>(),
        e["wins"].as<const char*>()
      );
      server.sendContent(row);
    }
  }
  server.sendContent(R"rawliteral(
        </tbody></table>
      </div>
    </div>
  </section>
)rawliteral");

  //Countdown script
  {
    String when = nextDate + "T" + nextTime;
    char js[512];
    snprintf(js, sizeof(js), R"rawliteral(
<script>
  const target = new Date("%s").getTime();
  setInterval(() => {
    let d = target - Date.now();
    const el = document.getElementById("countdown");
    if (d < 0) return el.innerText="LIVE!";
    let days = Math.floor(d/86400000),
        hrs  = Math.floor((d%86400000)/3600000),
        mins = Math.floor((d%3600000)/60000),
        secs = Math.floor((d%60000)/1000);
    el.innerText = `${days}d ${hrs}h ${mins}m ${secs}s`;
  }, 1000);
</script>
)rawliteral", when.c_str());
    server.sendContent(js);
  }

  server.sendContent(R"rawliteral(
    </main>
    <footer><p>Data © Ergast API • ESP32 F1 Dashboard by mazur888 -- 2025</p></footer>
  </div>
</body>
</html>
)rawliteral");

 server.sendContent("");
}
