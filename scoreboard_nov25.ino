/*
  WIRELESS SPORTS SCOREBOARD - ESP32
  Hardware: 8 x MAX7219 FC16 matrices (32x16), stacked vertically
  DIN=23, CLK=18, CS=21

  Wi-Fi:
  Name: Wireless-Scoreboard
  Password: 12345678
  Website: http://192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <string.h>
#include <stdio.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define DATA_PIN 23
#define CLK_PIN 18
#define CS_PIN 21
#define NUM_DEVICES 8
#define W 32
#define H 16
#define TOP_IS_FIRST_MODULE true
#define MIRROR_X true
#define FLIP_Y false

const char* AP_SSID = "Wireless-Scoreboard";
const char* AP_PASSWORD = "12345678";

WebServer server(80);
MD_MAX72XX mx(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, NUM_DEVICES);

bool fb[H][W];

// =================================================
// MATCH DATA
// =================================================

enum Sport {
  CRICKET,
  FOOTBALL,
  BASKETBALL
};

Sport currentSport = CRICKET;

char teamA[4] = "IND";
char teamB[4] = "AUS";

int scoreA = 0;
int scoreB = 0;

int wickets = 0;
int legalBalls = 0;

bool secondInnings = false;

int firstInningsRuns = 0;
int targetRuns = 0;

int battingTeam = 0;

bool teamsConfigured = false;
bool cricketMatchStarted = false;
bool inningsComplete = false;
bool cricketMatchOver = false;

int winnerTeam = -1;

int configuredOvers = 20;

String matchPeriod = "1st Half";

unsigned long timerRemaining = 90UL * 60UL;
unsigned long lastTimerTick = 0;

bool timerRunning = false;

int configuredMinutes = 90;

String lastEvent = "Ready";

// =================================================
// CRICKET ANIMATIONS
// =================================================

enum CricketAnimation {
  CRICKET_ANIM_NONE,
  CRICKET_ANIM_FOUR,
  CRICKET_ANIM_SIX,
  CRICKET_ANIM_WICKET,
  CRICKET_ANIM_WIDE,
  CRICKET_ANIM_NOBALL,
  CRICKET_ANIM_WIN
};

CricketAnimation cricketAnimation = CRICKET_ANIM_NONE;

int animationOffset = W;

unsigned long lastAnimationFrame = 0;
unsigned long animationStartedAt = 0;
unsigned long animationFrameInterval = 65;

const unsigned long ANIMATION_DURATION_MS = 5500;
const int WICKET_STEP = 14;

// =================================================
// SPORT DATA
// =================================================

struct SportState {
  char teamA[4];
  char teamB[4];

  int scoreA;
  int scoreB;

  int wickets;
  int legalBalls;

  bool secondInnings;

  int firstInningsRuns;
  int targetRuns;

  int battingTeam;

  bool teamsConfigured;
  bool matchStarted;
  bool inningsComplete;
  bool matchOver;

  int winnerTeam;
  int configuredOvers;

  String matchPeriod;

  unsigned long timerRemaining;

  bool timerRunning;

  int configuredMinutes;
};

SportState savedSports[3];

// =================================================
// WEB PAGE
// =================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wireless Sports Scoreboard</title>

<style>
*{box-sizing:border-box;font-family:Arial,sans-serif}
body{margin:0;background:#081225;color:#fff;padding:16px}
.wrap{max-width:1050px;margin:auto}
header{text-align:center;margin:10px 0 24px}
h1{color:#38bdf8;margin:0}
header p,.muted{color:#94a3b8}
.panel{background:#111f38;border:1px solid #263b59;border-radius:13px;padding:18px;margin:14px 0}
h2{color:#38bdf8;font-size:19px;margin:0 0 15px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.teams{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.team{background:#172944;border:1px solid #294463;border-radius:12px;padding:17px;text-align:center}
.team h3{color:#38bdf8}
.score{font-size:42px;font-weight:bold;margin:10px}
.details{color:#94a3b8;min-height:20px}
.controls,.row{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin-top:12px}
button{border:0;border-radius:7px;padding:11px 14px;color:#fff;font-weight:bold;cursor:pointer;background:#0284c7}
button:disabled{opacity:.4;cursor:not-allowed}
.sport{background:#263b59;min-width:145px;padding:18px}
.sport.active,.period.active{background:#0284c7}
input,select{width:100%;margin-top:7px;padding:12px;border-radius:7px;background:#081225;color:#fff;border:1px solid #334b6c}
label{font-size:14px;color:#cbd5e1}
.green{background:#16a34a}
.red{background:#dc2626}
.orange{background:#b45309}
.purple{background:#7c3aed}
.slate{background:#475569}
.target{color:#facc15;text-align:center;font-weight:bold;font-size:19px;margin-top:12px}
.timer{font:bold 48px monospace;text-align:center;color:#22c55e;background:#020617;padding:18px;border:2px solid #334155;border-radius:12px}
.status{text-align:center;color:#4ade80;min-height:20px;margin-top:12px}
.period{background:#475569;flex:1;min-width:90px}
.hide{display:none!important}
.preview{font-family:monospace;background:#020617;border:2px solid #475569;border-radius:10px;padding:15px;text-align:center;color:#22c55e}
.history{color:#cbd5e1;font-size:13px;max-height:160px;overflow:auto}
.history div{padding:7px;border-bottom:1px solid #334155}
.result{text-align:center;padding:20px;border:1px solid #16a34a;background:#052e20;border-radius:10px}
.result h2{color:#4ade80;font-size:25px}
.result p{color:#bbf7d0;font-size:18px}
.primaryButton{background:#16a34a;padding:15px 20px;font-size:16px}
.teamChoice{width:100%;min-height:65px;font-size:18px}
.resultButtons button{min-width:180px;padding:14px}
@media(max-width:650px){
body{padding:9px}
.grid,.teams{grid-template-columns:1fr}
.timer{font-size:40px}
.sport{width:100%}
.resultButtons button{width:100%}
}
</style>
</head>

<body>
<div class="wrap">

<header>
<h1>🏆 WIRELESS SCOREBOARD</h1>
<p>Smart Sports Score Control System</p>
</header>

<section class="panel" id="selectPanel">
<h2 style="text-align:center">SELECT YOUR SPORT</h2>

<div class="row">
<button class="sport" onclick="choose('cricket')">🏏 CRICKET</button>
<button class="sport" onclick="choose('football')">⚽ FOOTBALL</button>
<button class="sport" onclick="choose('basketball')">🏀 BASKETBALL</button>
</div>
</section>

<main id="app" class="hide">

<section class="panel">
<div class="row" style="justify-content:space-between">
<h2 id="sportTitle">SCOREBOARD</h2>
<button class="slate" onclick="back()">← BACK TO SPORT SELECTION</button>
</div>
</section>

<!-- CRICKET TEAM AND OVERS SETUP -->

<section class="panel hide" id="cricketSetupPanel">

<h2>🏏 Cricket Match Setup</h2>

<div class="grid">

<label>
Team A
<input id="nameA" maxlength="3" value="IND" placeholder="Enter Team A">
</label>

<label>
Team B
<input id="nameB" maxlength="3" value="AUS" placeholder="Enter Team B">
</label>

</div>

<div style="margin-top:17px">
<label for="oversSelect">Select Overs Per Innings</label>

<select id="oversSelect" onchange="toggleCustomOvers()">
<option value="5">5 Overs</option>
<option value="10">10 Overs</option>
<option value="15">15 Overs</option>
<option value="20" selected>20 Overs</option>
<option value="30">30 Overs</option>
<option value="50">50 Overs</option>
<option value="custom">Custom Overs</option>
</select>

<input
id="customOvers"
class="hide"
type="number"
min="1"
max="99"
value="20"
placeholder="Enter custom overs (1-99)"
>

<p class="muted">
Both innings will have the selected overs limit.
</p>
</div>

<div class="row">
<button class="green primaryButton" onclick="saveNames()">
CONTINUE
</button>
</div>

</section>

<!-- FIRST BATTING TEAM SELECTION -->

<section class="panel hide" id="cricketBattingPanel">

<h2>🏏 Choose First Batting Team</h2>

<p class="muted">Select which team will bat first.</p>

<div class="grid">
<button class="teamChoice green" id="startTeamA" onclick="startCricket(0)">
TEAM A BATS FIRST
</button>

<button class="teamChoice" id="startTeamB" onclick="startCricket(1)">
TEAM B BATS FIRST
</button>
</div>

<p class="target" id="selectedOversText"></p>

<div class="row">
<button class="slate" onclick="editTeams()">EDIT TEAM / OVERS</button>
</div>

</section>

<!-- FOOTBALL AND BASKETBALL TEAM NAMES -->

<section class="panel hide" id="teamNamePanel">
<h2>Team Names (maximum 3 letters)</h2>

<div class="grid">
<label>
Team A
<input id="nameA2" maxlength="3" value="IND">
</label>

<label>
Team B
<input id="nameB2" maxlength="3" value="AUS">
</label>
</div>

<div class="row">
<button onclick="saveOtherNames()">UPDATE TEAM NAMES</button>
</div>
</section>

<!-- FOOTBALL AND BASKETBALL TIMER -->

<section class="panel hide" id="timerPanel">
<h2>⏱ Match Timer</h2>

<div class="grid">
<div>
<label for="minutes">Match Duration (minutes)</label>
<input id="minutes" type="number" min="1" max="99" value="90">

<button class="green" style="width:100%;margin-top:9px" onclick="updateTimerDuration()">
UPDATE TIMER
</button>
</div>

<label>
Selected Sport
<input id="sportName" readonly>
</label>
</div>

<div class="row">
<button class="period active" id="p1" onclick="period('1st Half')">1ST HALF</button>
<button class="period" id="ph" onclick="period('Half Time')">HALF TIME</button>
<button class="period" id="p2" onclick="period('2nd Half')">2ND HALF</button>
</div>

<p id="periodText" class="target">1ST HALF</p>

<div class="timer" id="timer">90:00</div>

<div class="row">
<button class="green" onclick="timerAction('start')">▶ START</button>
<button class="orange" onclick="timerAction('pause')">Ⅱ PAUSE</button>
<button class="slate" onclick="timerAction('reset')">↻ RESET TIMER</button>
</div>
</section>

<!-- SCOREBOARD -->

<section class="panel hide" id="scoreboardPanel">

<h2 id="scoreHeading">Score Control</h2>

<div class="teams">

<div class="team" id="teamCardA">
<h3 id="teamHeadingA">IND</h3>
<div class="score" id="scoreA">0</div>
<div class="details" id="detailA"></div>
<div class="controls" id="controlA"></div>
</div>

<div class="team" id="teamCardB">
<h3 id="teamHeadingB">AUS</h3>
<div class="score" id="scoreB">0</div>
<div class="details" id="detailB"></div>
<div class="controls" id="controlB"></div>
</div>

</div>

<div class="target" id="targetText"></div>
</section>

<!-- FIRST INNINGS COMPLETED -->

<section class="panel hide" id="cricketFlowPanel">

<h2 style="text-align:center">FIRST INNINGS COMPLETED</h2>

<p class="target" id="firstInningsSummary"></p>

<p class="muted" style="text-align:center">
The first batting team has completed its innings.
Start the second innings to chase the target.
</p>

<div class="row">
<button class="green primaryButton" onclick="startSecondInnings()">
START SECOND INNINGS
</button>
</div>

</section>

<!-- MATCH RESULT -->

<section class="panel hide" id="resultPanel">

<div class="result">
<h2>🏆 MATCH FINISHED</h2>
<p id="winnerText"></p>
<p id="finalScoreText"></p>
</div>

<div class="row resultButtons">
<button class="green primaryButton" onclick="playAgain()">
▶ PLAY AGAIN
</button>

<button class="orange" onclick="newMatch()">
NEW MATCH / CHANGE TEAMS
</button>

<button class="slate" onclick="back()">
SPORT SELECTION
</button>
</div>

</section>

<!-- SCORE ACTIONS -->

<section class="panel hide" id="scoreControlsPanel">

<h2>Score Controls</h2>

<div class="row">
<button class="orange" onclick="send('undo')">↶ UNDO LAST ACTION</button>
<button class="red" onclick="resetScores()">RESET MATCH</button>
</div>

<div class="row">
<button class="orange hide" id="endInningsButton" onclick="endInnings()">
END INNINGS
</button>
</div>

<div class="status" id="status">Connected to ESP32</div>

</section>

<!-- LED PREVIEW -->

<section class="panel hide" id="previewPanel">

<h2>LED Display Preview</h2>

<div class="preview">
<div id="previewTeam">IND</div>
<div style="font-size:25px;margin-top:12px" id="previewScore">0/0</div>
<div id="previewExtra" style="color:#facc15;margin-top:8px"></div>
</div>

<p class="muted">Physical MAX7219 display is updated by ESP32.</p>
</section>

<!-- HISTORY -->

<section class="panel hide" id="historyPanel">
<h2>📋 Action History</h2>
<div id="history" class="history">No actions yet.</div>
</section>

</main>
</div>

<script>

let sport='cricket';

// =================================================
// API HELPERS
// =================================================

async function api(path){

  try{
    let r=await fetch(path);

    if(!r.ok)throw Error('HTTP '+r.status);

    return await r.json();

  }catch(e){
    msg('Connection error: '+e.message);
    return null;
  }
}

function msg(s){
  document.getElementById('status').textContent=s;
}

async function send(type,value=''){

  let d=await api(
    '/action?type='+
    encodeURIComponent(type)+
    '&value='+
    encodeURIComponent(value)
  );

  if(d){
    msg(d.message||'Updated');
    await refresh();
  }
}

function choose(s){

  sport=s;

  document.getElementById('selectPanel').classList.add('hide');
  document.getElementById('app').classList.remove('hide');

  document.getElementById('sportTitle').textContent=
    s.toUpperCase()+' SCOREBOARD';

  api('/choose?sport='+s).then(refresh);
}

function back(){

  api('/choose?sport=none');

  document.getElementById('app').classList.add('hide');
  document.getElementById('selectPanel').classList.remove('hide');
}

// =================================================
// CRICKET OVERS SETUP
// =================================================

function toggleCustomOvers(){

  const selected=document.getElementById('oversSelect').value;

  document.getElementById('customOvers').classList.toggle(
    'hide',
    selected!=='custom'
  );
}

function getOversValue(){

  const selected=document.getElementById('oversSelect').value;

  let n;

  if(selected==='custom'){
    n=Number(document.getElementById('customOvers').value);
  }else{
    n=Number(selected);
  }

  if(!Number.isInteger(n)||n<1||n>99){
    msg('Select overs between 1 and 99');
    return 0;
  }

  return n;
}

function saveNames(){

  let a=document.getElementById('nameA').value
    .toUpperCase()
    .replace(/[^A-Z]/g,'')
    .slice(0,3);

  let b=document.getElementById('nameB').value
    .toUpperCase()
    .replace(/[^A-Z]/g,'')
    .slice(0,3);

  if(!a||!b){
    msg('Enter both team names');
    return;
  }

  let overs=getOversValue();

  if(!overs)return;

  send('setup',a+','+b+','+overs);
}

function startCricket(team){
  send('startmatch',String(team));
}

function editTeams(){
  send('editteams');
}

function endInnings(){
  send('endinnings');
}

function startSecondInnings(){
  send('startsecond');
}

function playAgain(){

  if(confirm('Start another match with the same teams and overs?')){
    send('playagain');
  }
}

function newMatch(){

  if(confirm('Start a new match with different teams or overs?')){
    send('editteams');
  }
}

// =================================================
// FOOTBALL / BASKETBALL TEAM NAMES
// =================================================

function saveOtherNames(){

  let a=document.getElementById('nameA2').value
    .toUpperCase()
    .replace(/[^A-Z]/g,'')
    .slice(0,3);

  let b=document.getElementById('nameB2').value
    .toUpperCase()
    .replace(/[^A-Z]/g,'')
    .slice(0,3);

  if(!a||!b){
    msg('Enter both team names');
    return;
  }

  send('names',a+','+b);
}

// =================================================
// FOOTBALL / BASKETBALL TIMER
// =================================================

function period(p){
  send('period',p);
}

function timerAction(a){
  send('timer',a);
}

async function updateTimerDuration(){

  const input=document.getElementById('minutes');
  const n=Number(input.value);

  if(!Number.isInteger(n)||n<1||n>99){
    msg('Enter duration from 1 to 99 minutes');
    input.focus();
    return;
  }

  if(sport==='cricket'){
    msg('Match timer is available only for Football and Basketball');
    return;
  }

  const result=await api(
    '/action?type=duration&value='+encodeURIComponent(n)
  );

  if(result&&result.ok){
    msg('Timer updated to '+n+' minutes');
    await refresh();
  }
}

function resetScores(){

  if(confirm('Reset this match?')){
    send('reset');
  }
}

// =================================================
// SCORE CONTROL BUTTONS
// =================================================

function makeControls(d){

  for(let i=0;i<2;i++){

    let s='';

    if(sport==='cricket'){

      if(
        d.matchStarted &&
        !d.inningsComplete &&
        !d.matchOver &&
        i===d.batting
      ){

        for(let n=1;n<=6;n++){
          s+=`<button onclick="send('runs','${i},${n}')">+${n}</button>`;
        }

        s+=`
          <button class="red" onclick="send('wicket','${i}')">
            WICKET
          </button>
        `;

        if(d.legalBalls < d.oversLimit*6){
          s+=`
            <button class="slate" onclick="send('ball','${i}')">
              + BALL
            </button>
          `;
        }else{
          s+=`
            <button class="slate" disabled>
              OVERS COMPLETE
            </button>
          `;
        }

        s+=`
          <button class="purple" onclick="send('extra','${i},wide')">
            WIDE +1
          </button>

          <button class="purple" onclick="send('extra','${i},noball')">
            NO BALL +1
          </button>
        `;
      }
    }

    else if(sport==='football'){

      s=`
        <button onclick="send('points','${i},1')">
          +1 GOAL
        </button>

        <button class="orange" onclick="send('points','${i},-1')">
          -1 GOAL
        </button>
      `;
    }

    else{

      s=`
        <button onclick="send('points','${i},1')">+1</button>
        <button onclick="send('points','${i},2')">+2</button>
        <button onclick="send('points','${i},3')">+3</button>
        <button class="orange" onclick="send('points','${i},-1')">-1</button>
      `;
    }

    document.getElementById(i?'controlB':'controlA').innerHTML=s;
  }
}

function fmt(sec){

  let m=Math.floor(sec/60);
  let s=sec%60;

  return String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
}

// =================================================
// REFRESH WEBSITE DATA
// =================================================

async function refresh(){

  let d=await api('/state');

  if(!d)return;

  sport=d.sport;

  const isCricket=(sport==='cricket');
  const started=d.matchStarted;

  document.getElementById('sportTitle').textContent=
    sport.toUpperCase()+' SCOREBOARD';

  document.getElementById('scoreHeading').textContent=
    sport[0].toUpperCase()+sport.slice(1)+' Score Control';

  document.getElementById('cricketSetupPanel').classList.toggle(
    'hide',
    !isCricket || started || d.teamsConfigured
  );

  document.getElementById('cricketBattingPanel').classList.toggle(
    'hide',
    !isCricket || started || !d.teamsConfigured
  );

  document.getElementById('teamNamePanel').classList.toggle(
    'hide',
    isCricket
  );

  document.getElementById('timerPanel').classList.toggle(
    'hide',
    isCricket
  );

  document.getElementById('scoreboardPanel').classList.toggle(
    'hide',
    isCricket && !started
  );

  document.getElementById('scoreControlsPanel').classList.toggle(
    'hide',
    isCricket && !started
  );

  document.getElementById('cricketFlowPanel').classList.toggle(
    'hide',
    !isCricket || !started || !d.inningsComplete || d.matchOver
  );

  document.getElementById('resultPanel').classList.toggle(
    'hide',
    !isCricket || !d.matchOver
  );

  document.getElementById('previewPanel').classList.toggle(
    'hide',
    isCricket && !started
  );

  document.getElementById('historyPanel').classList.toggle(
    'hide',
    isCricket && !started
  );

  const nameAInput=document.getElementById('nameA');
  const nameBInput=document.getElementById('nameB');

  if(document.activeElement!==nameAInput){
    nameAInput.value=d.aName;
  }

  if(document.activeElement!==nameBInput){
    nameBInput.value=d.bName;
  }

  document.getElementById('nameA2').value=d.aName;
  document.getElementById('nameB2').value=d.bName;

  document.getElementById('teamHeadingA').textContent=d.aName;
  document.getElementById('teamHeadingB').textContent=d.bName;

  document.getElementById('scoreA').textContent=d.a;
  document.getElementById('scoreB').textContent=d.b;

  document.getElementById('detailA').textContent=
    (isCricket&&d.batting===0)
    ?d.wickets+' Wickets | '+d.overs+' Overs'
    :'';

  document.getElementById('detailB').textContent=
    (isCricket&&d.batting===1)
    ?d.wickets+' Wickets | '+d.overs+' Overs'
    :'';

  document.getElementById('teamCardA').classList.toggle(
    'hide',
    isCricket && d.batting!==0
  );

  document.getElementById('teamCardB').classList.toggle(
    'hide',
    isCricket && d.batting!==1
  );

  document.getElementById('selectedOversText').textContent=
    'Match overs: '+d.oversLimit+' per innings';

  const oversSelect=document.getElementById('oversSelect');
  const customOvers=document.getElementById('customOvers');

  if(
    document.activeElement!==oversSelect &&
    document.activeElement!==customOvers
  ){

    const commonOvers=[5,10,15,20,30,50];

    if(commonOvers.includes(Number(d.oversLimit))){
      oversSelect.value=String(d.oversLimit);
      customOvers.classList.add('hide');
    }else{
      oversSelect.value='custom';
      customOvers.classList.remove('hide');
      customOvers.value=d.oversLimit;
    }
  }

  let battingScore=(d.batting===0)?d.a:d.b;

  if(isCricket){

    if(d.second){

      let needed=Math.max(0,d.target-battingScore);

      document.getElementById('targetText').textContent=
        'TARGET: '+d.target+
        ' | NEED '+needed+
        ' RUNS | OVERS: '+d.oversLimit;

    }else if(d.inningsComplete){

      document.getElementById('targetText').textContent=
        'FIRST INNINGS COMPLETED';

    }else{

      document.getElementById('targetText').textContent=
        'FIRST INNINGS | '+d.oversLimit+' OVERS';
    }

  }else{

    document.getElementById('targetText').textContent='';
  }

  document.getElementById('firstInningsSummary').textContent=
    d.battingName+
    ' scored '+
    d.firstRuns+
    ' runs. Target: '+
    d.target+
    ' in '+
    d.oversLimit+
    ' overs.';

  document.getElementById('startTeamA').textContent=
    d.aName+' BATS FIRST';

  document.getElementById('startTeamB').textContent=
    d.bName+' BATS FIRST';

  document.getElementById('endInningsButton').classList.toggle(
    'hide',
    !isCricket ||
    !started ||
    d.second ||
    d.inningsComplete ||
    d.matchOver
  );

  if(d.matchOver){

    document.getElementById('winnerText').textContent=
      d.winnerName+' WON THE MATCH!';

    document.getElementById('finalScoreText').textContent=
      d.aName+' '+d.a+'  -  '+d.bName+' '+d.b;
  }

  const minutesInput=document.getElementById('minutes');

  if(document.activeElement!==minutesInput){
    minutesInput.value=d.duration;
  }

  document.getElementById('sportName').value=
    sport[0].toUpperCase()+sport.slice(1);

  document.getElementById('timer').textContent=fmt(d.timer);

  document.getElementById('periodText').textContent=
    d.period.toUpperCase();

  ['p1','ph','p2'].forEach((id,i)=>{

    document.getElementById(id).classList.toggle(
      'active',
      d.period===['1st Half','Half Time','2nd Half'][i]
    );
  });

  document.getElementById('previewTeam').textContent=
    isCricket?d.battingName:d.aName+'-'+d.bName;

  document.getElementById('previewScore').textContent=
    isCricket
    ?(d.batting===0?d.a:d.b)+'/'+d.wickets
    :d.a+'-'+d.b;

  document.getElementById('previewExtra').textContent=
    isCricket
    ?(d.second?'T='+d.target+'  ':'')+
      d.overs+' / '+d.oversLimit+' Overs'
    :(d.period+'  '+fmt(d.timer));

  document.getElementById('history').innerHTML=
    d.history.length
    ?d.history.map(x=>'<div>'+x+'</div>').join('')
    :'No actions yet.';

  makeControls(d);
}

// Refresh website
setInterval(()=>{

  if(!document.getElementById('app').classList.contains('hide')){
    refresh();
  }

},1500);

</script>
</body>
</html>
)HTML";

// =================================================
// CRICKET LED FONT
// =================================================

struct Glyph {
  char ch;
  uint8_t w;
  uint8_t rows[5];
};

struct Font {
  uint8_t h;
  const Glyph *g;
  uint8_t n;
};

const Glyph FONT5_G[] = {
  {'0',3,{7,5,5,5,7}}, {'1',1,{1,1,1,1,1}},
  {'2',3,{7,1,7,4,7}}, {'3',3,{7,1,7,1,7}},
  {'4',3,{5,5,7,1,1}}, {'5',3,{7,4,7,1,7}},
  {'6',3,{7,4,7,5,7}}, {'7',3,{7,1,1,2,2}},
  {'8',3,{7,5,7,5,7}}, {'9',3,{7,5,7,1,7}},
  {'.',1,{0,0,0,0,1}}, {'/',2,{1,1,2,2,2}},
  {'(',2,{1,2,2,2,1}}, {')',2,{2,1,1,1,2}},
  {'[',2,{3,2,2,2,3}}, {']',2,{3,1,1,1,3}},
  {'{',1,{0,1,1,1,0}}, {'}',1,{0,1,1,1,0}},
  {'=',2,{0,3,0,3,0}},
  {'T',3,{7,2,2,2,2}}, {'I',3,{7,2,2,2,7}},
  {'N',4,{9,13,11,9,9}}, {'D',3,{6,5,5,5,6}},
  {'A',3,{2,5,7,5,5}}, {'B',3,{6,5,6,5,6}},
  {'C',3,{7,4,4,4,7}}, {'E',3,{7,4,6,4,7}},
  {'F',3,{7,4,6,4,4}}, {'G',3,{7,4,5,5,7}},
  {'H',3,{5,5,7,5,5}}, {'J',3,{1,1,1,5,7}},
  {'K',3,{5,5,6,5,5}}, {'L',3,{4,4,4,4,7}},
  {'M',5,{17,27,21,17,17}}, {'O',3,{7,5,5,5,7}},
  {'P',3,{6,5,6,4,4}}, {'Q',3,{7,5,7,1,1}},
  {'R',3,{6,5,6,5,5}}, {'S',3,{7,4,7,1,7}},
  {'U',3,{5,5,5,5,7}}, {'V',3,{5,5,5,5,2}},
  {'W',5,{17,17,21,27,17}}, {'X',3,{5,5,2,5,5}},
  {'Y',3,{5,5,2,2,2}}, {'Z',3,{7,1,2,4,7}},
  {'|',1,{1,1,1,1,1}}
};

const Font FONT5 = {
  5,
  FONT5_G,
  sizeof(FONT5_G) / sizeof(FONT5_G[0])
};

const Glyph FONT4_G[] = {
  {'0',3,{7,5,5,7}}, {'1',3,{2,6,2,7}},
  {'2',3,{6,1,2,7}}, {'3',3,{6,1,3,6}},
  {'4',3,{5,5,7,1}}, {'5',3,{7,6,1,7}},
  {'6',3,{3,4,7,7}}, {'7',3,{7,1,2,2}},
  {'8',3,{7,5,7,7}}, {'9',3,{7,7,1,6}},
  {'.',1,{0,0,0,1}}, {'/',3,{1,2,2,4}},
  {'(',2,{1,2,2,1}}, {')',2,{2,1,1,2}},
  {'|',1,{1,1,1,1}}
};

const Font FONT4 = {
  4,
  FONT4_G,
  sizeof(FONT4_G) / sizeof(FONT4_G[0])
};

int clippedPixels = 0;
int overlapPixels = 0;
int missingChars = 0;

const Glyph *findGlyph(const Font &f, char c) {

  for(uint8_t i=0;i<f.n;i++){
    if(f.g[i].ch==c)return &f.g[i];
  }

  return nullptr;
}

void setPx(int x,int y){

  if(x<0||x>=W||y<0||y>=H){
    clippedPixels++;
    return;
  }

  if(fb[y][x])overlapPixels++;

  fb[y][x]=true;
}

void drawGlyph(
  int x,
  int y,
  const Font &f,
  const Glyph &g
){

  for(uint8_t r=0;r<f.h;r++){

    for(uint8_t c=0;c<g.w;c++){

      if(g.rows[r]&(1<<(g.w-1-c))){
        setPx(x+c,y+r);
      }
    }
  }
}

int gapBetween(
  const Font &f,
  const Glyph &a,
  const Glyph &b
){

  uint8_t last=f.h-1;

  if(a.ch=='.'&&!((b.rows[last]>>(b.w-1))&1))return 0;

  if(b.ch=='.'&&!(a.rows[last]&1))return 0;

  return 1;
}

int textWidth(const char *s,const Font &f){

  int w=0;
  const Glyph *prev=nullptr;

  for(;*s;s++){

    const Glyph *g=findGlyph(f,*s);

    if(!g)continue;

    if(prev)w+=gapBetween(f,*prev,*g);

    w+=g->w;
    prev=g;
  }

  return w;
}

int drawText(int x,int y,const char *s,const Font &f){

  int cx=x;
  const Glyph *prev=nullptr;

  for(;*s;s++){

    const Glyph *g=findGlyph(f,*s);

    if(!g){
      missingChars++;
      continue;
    }

    if(prev)cx+=gapBetween(f,*prev,*g);

    drawGlyph(cx,y,f,*g);

    cx+=g->w;
    prev=g;
  }

  return cx-x;
}

void drawFullSeparator(int x,int y){

  for(int r=0;r<8;r++){
    setPx(x,y+r);
  }
}

int oversTextWidth(const char *s,const Font &f){

  const char *dot=strchr(s,'.');

  if(!dot)return textWidth(s,f);

  int width=0;
  int beforeCount=0;

  for(const char *p=s;p<dot;p++){

    const Glyph *g=findGlyph(f,*p);

    if(g){
      width+=g->w;
      beforeCount++;
    }
  }

  if(beforeCount>1)width+=beforeCount-1;

  width++;

  const Glyph *dotGlyph=findGlyph(f,'.');

  if(dotGlyph)width+=dotGlyph->w;

  for(const char *p=dot+1;*p;p++){

    const Glyph *g=findGlyph(f,*p);

    if(g)width+=g->w;
  }

  return width;
}

int drawOversText(int x,int y,const char *s,const Font &f){

  const char *dot=strchr(s,'.');

  if(!dot)return drawText(x,y,s,f);

  int cx=x;
  bool first=true;

  for(const char *p=s;p<dot;p++){

    const Glyph *g=findGlyph(f,*p);

    if(!g){
      missingChars++;
      continue;
    }

    if(!first)cx++;

    drawGlyph(cx,y,f,*g);

    cx+=g->w;
    first=false;
  }

  cx++;

  const Glyph *dotGlyph=findGlyph(f,'.');

  if(dotGlyph){
    drawGlyph(cx,y,f,*dotGlyph);
    cx+=dotGlyph->w;
  }

  for(const char *p=dot+1;*p;p++){

    const Glyph *g=findGlyph(f,*p);

    if(!g){
      missingChars++;
      continue;
    }

    drawGlyph(cx,y,f,*g);
    cx+=g->w;
  }

  return cx-x;
}

void drawTallBracket(int x,int y,bool leftBracket){

  for(int r=0;r<8;r++){

    if(r==0||r==7){
      setPx(x,y+r);
      setPx(x+1,y+r);
    }else{
      setPx(x+(leftBracket?0:1),y+r);
    }
  }
}

void drawSmallBracket(int x,int y,bool leftBracket){

  for(int r=0;r<4;r++){

    if(r==0||r==3){
      setPx(x,y+r);
      setPx(x+1,y+r);
    }else{
      setPx(x+(leftBracket?0:1),y+r);
    }
  }
}

void flushDisplay(){

  mx.control(MD_MAX72XX::UPDATE,MD_MAX72XX::OFF);
  mx.clear();

  for(int y=0;y<H;y++){

    for(int x=0;x<W;x++){

      if(!fb[y][x])continue;

      bool upper=(y<8);

      int module=(upper==TOP_IS_FIRST_MODULE)?0:1;

      int row=y%8;

      if(FLIP_Y)row=7-row;

      int localCol=MIRROR_X?(31-x):x;

      mx.setPoint(row,module*32+localCol,true);
    }
  }

  mx.control(MD_MAX72XX::UPDATE,MD_MAX72XX::ON);
}

// =================================================
// NORMAL CRICKET DISPLAY
// =================================================

void renderCricket(){

  memset(fb,0,sizeof(fb));

  clippedPixels=overlapPixels=missingChars=0;

  char targetBuf[12];
  char oversBuf[12];
  char runsBuf[8];
  char wicketsBuf[8];

  int battingScore=(battingTeam==0)?scoreA:scoreB;

  const char *battingName=(battingTeam==0)?teamA:teamB;

  if(secondInnings){
    snprintf(targetBuf,sizeof(targetBuf),"T=%d",targetRuns);
  }else{
    targetBuf[0]='\0';
  }

  snprintf(runsBuf,sizeof(runsBuf),"%d",battingScore);
  snprintf(wicketsBuf,sizeof(wicketsBuf),"%d",wickets);

  snprintf(
    oversBuf,
    sizeof(oversBuf),
    "%d.%d",
    legalBalls/6,
    legalBalls%6
  );

  drawText(1,2,battingName,FONT5);

  if(secondInnings){

    int targetWidth=textWidth(targetBuf,FONT5);
    drawText(W-targetWidth,0,targetBuf,FONT5);
  }

  int wRuns=textWidth(runsBuf,FONT5);
  int wWickets=textWidth(wicketsBuf,FONT5);

  int wCombined=wRuns+1+1+1+wWickets;

  int wOvers=oversTextWidth(oversBuf,FONT5);

  int leftBracketX=wCombined+1;
  int oversX=leftBracketX+2;
  int rightBracketX=W-2;

  if(oversX+wOvers<=rightBracketX){

    drawText(0,9,runsBuf,FONT5);

    int separatorX=wRuns+1;

    drawFullSeparator(separatorX,8);

    drawText(separatorX+2,9,wicketsBuf,FONT5);

    drawTallBracket(leftBracketX,8,true);

    drawOversText(oversX,9,oversBuf,FONT5);

    drawTallBracket(rightBracketX,8,false);

  }else{

    int wRuns4=textWidth(runsBuf,FONT4);

    drawText(0,8,runsBuf,FONT4);

    int separatorX=wRuns4+1;

    drawFullSeparator(separatorX,8);

    drawText(separatorX+2,8,wicketsBuf,FONT4);

    int wOvers4=oversTextWidth(oversBuf,FONT4);

    int rightX=W-2;
    int oversStartX=rightX-wOvers4;
    int leftX=oversStartX-2;

    drawSmallBracket(leftX,12,true);

    drawOversText(oversStartX,12,oversBuf,FONT4);

    drawSmallBracket(rightX,12,false);
  }

  flushDisplay();
}

// =================================================
// CRICKET EVENT FONT
// =================================================

struct EventGlyph {
  char character;
  uint8_t rows[7];
};

const EventGlyph EVENT_FONT[] = {

  {'F',{0b11111,0b10000,0b11110,0b10000,0b10000,0b10000,0b10000}},
  {'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}},
  {'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}},
  {'I',{0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}},
  {'X',{0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001}},
  {'W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010}},
  {'N',{0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001}},
  {'B',{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}},
  {'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
  {'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}},
  {'D',{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110}},
  {'E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}}
};

const EventGlyph *findEventGlyph(char c){

  for(
    unsigned i=0;
    i<sizeof(EVENT_FONT)/sizeof(EVENT_FONT[0]);
    i++
  ){

    if(EVENT_FONT[i].character==c){
      return &EVENT_FONT[i];
    }
  }

  return nullptr;
}

const char *getAnimationText(){

  switch(cricketAnimation){

    case CRICKET_ANIM_FOUR:
      return "FOUR";

    case CRICKET_ANIM_SIX:
      return "SIX";

    case CRICKET_ANIM_WICKET:
      return "W";

    case CRICKET_ANIM_WIDE:
      return "WIDE";

    case CRICKET_ANIM_NOBALL:
      return "NO BALL";

    case CRICKET_ANIM_WIN:
      return "WIN";

    default:
      return "";
  }
}

const char *getWinnerName(){

  if(winnerTeam==0)return teamA;
  if(winnerTeam==1)return teamB;

  return "";
}

int getAnimationWidth(){

  if(cricketAnimation==CRICKET_ANIM_WIN){

    int teamWidth=textWidth(getWinnerName(),FONT5);
    int winWidth=textWidth("WIN",FONT5);

    return teamWidth+3+winWidth;
  }

  const char *text=getAnimationText();

  int width=0;
  bool previousWasLetter=false;

  for(int i=0;text[i]!='\0';i++){

    if(text[i]==' '){
      width+=8;
      previousWasLetter=false;
      continue;
    }

    if(previousWasLetter)width+=2;

    width+=10;
    previousWasLetter=true;
  }

  return width;
}

void drawLargeEventGlyph(
  int x,
  int y,
  const EventGlyph &glyph
){

  const int scale=2;

  for(int row=0;row<7;row++){

    for(int col=0;col<5;col++){

      if(glyph.rows[row]&(1<<(4-col))){

        for(int sy=0;sy<scale;sy++){

          for(int sx=0;sx<scale;sx++){

            setPx(
              x+col*scale+sx,
              y+row*scale+sy
            );
          }
        }
      }
    }
  }
}

void renderCricketAnimation(){

  if(
    currentSport!=CRICKET ||
    cricketAnimation==CRICKET_ANIM_NONE
  ){
    renderCricket();
    return;
  }

  memset(fb,0,sizeof(fb));

  const int yStart=1;

  if(cricketAnimation==CRICKET_ANIM_WIN){

    const char *name=getWinnerName();

    int nameWidth=textWidth(name,FONT5);

    drawText(animationOffset,5,name,FONT5);

    drawText(
      animationOffset+nameWidth+3,
      5,
      "WIN",
      FONT5
    );

  }else if(cricketAnimation==CRICKET_ANIM_WICKET){

    const EventGlyph *wGlyph=findEventGlyph('W');

    if(wGlyph!=nullptr){

      for(int i=-2;i<=4;i++){

        int x=animationOffset+(i*WICKET_STEP);

        drawLargeEventGlyph(x,yStart,*wGlyph);
      }
    }

  }else{

    const char *text=getAnimationText();

    int x=animationOffset;

    for(int i=0;text[i]!='\0';i++){

      if(text[i]==' '){
        x+=8;
        continue;
      }

      const EventGlyph *glyph=findEventGlyph(text[i]);

      if(glyph==nullptr)continue;

      drawLargeEventGlyph(x,yStart,*glyph);

      x+=12;
    }
  }

  flushDisplay();
}

void startCricketAnimation(CricketAnimation animation){

  cricketAnimation=animation;

  if(animation==CRICKET_ANIM_WICKET){

    animationOffset=0;
    animationFrameInterval=65;

  }else if(animation==CRICKET_ANIM_WIN){

    animationOffset=W;
    animationFrameInterval=75;

  }else{

    animationOffset=W;

    if(animation==CRICKET_ANIM_SIX){
      animationFrameInterval=83;
    }else if(animation==CRICKET_ANIM_NOBALL){
      animationFrameInterval=50;
    }else{
      animationFrameInterval=70;
    }
  }

  animationStartedAt=millis();
  lastAnimationFrame=animationStartedAt;

  renderCricketAnimation();
}

void stopCricketAnimation(){

  cricketAnimation=CRICKET_ANIM_NONE;
  animationOffset=W;
  animationStartedAt=0;
}

// =================================================
// FOOTBALL / BASKETBALL LED FONT
// =================================================

struct FootballGlyph {
  char character;
  byte width;
  byte rows[5];
};

const FootballGlyph FOOT_FONT[] = {

  {'0',3,{7,5,5,5,7}}, {'1',3,{2,6,2,2,7}},
  {'2',3,{7,1,7,4,7}}, {'3',3,{7,1,7,1,7}},
  {'4',3,{5,5,7,1,1}}, {'5',3,{7,4,7,1,7}},
  {'6',3,{7,4,7,5,7}}, {'7',3,{7,1,1,2,2}},
  {'8',3,{7,5,7,5,7}}, {'9',3,{7,5,7,1,7}},

  {'A',3,{2,5,7,5,5}}, {'B',3,{6,5,6,5,6}},
  {'C',3,{7,4,4,4,7}}, {'D',3,{6,5,5,5,6}},
  {'E',3,{7,4,6,4,7}}, {'F',3,{7,4,6,4,4}},
  {'G',3,{7,4,5,5,7}}, {'H',3,{5,5,7,5,5}},
  {'I',3,{7,2,2,2,7}}, {'J',3,{1,1,1,5,7}},
  {'K',3,{5,5,6,5,5}}, {'L',3,{4,4,4,4,7}},
  {'M',5,{17,27,21,17,17}}, {'N',4,{9,13,11,9,9}},
  {'O',3,{7,5,5,5,7}}, {'P',3,{6,5,6,4,4}},
  {'Q',3,{7,5,7,1,1}}, {'R',3,{6,5,6,5,5}},
  {'S',3,{7,4,7,1,7}}, {'T',3,{7,2,2,2,2}},
  {'U',3,{5,5,5,5,7}}, {'V',3,{5,5,5,5,2}},
  {'W',5,{17,17,21,27,17}}, {'X',3,{5,5,2,5,5}},
  {'Y',3,{5,5,2,2,2}}, {'Z',3,{7,1,2,4,7}},

  {'-',3,{0,0,7,0,0}}, {':',1,{0,1,0,1,0}}
};

const FootballGlyph *findFootballGlyph(char c){

  for(
    unsigned i=0;
    i<sizeof(FOOT_FONT)/sizeof(FOOT_FONT[0]);
    i++
  ){

    if(FOOT_FONT[i].character==c){
      return &FOOT_FONT[i];
    }
  }

  return nullptr;
}

int footballTextWidth(const char *text,int gap){

  int width=0;

  for(int i=0;text[i]!='\0';i++){

    const FootballGlyph *g=findFootballGlyph(text[i]);

    if(g){
      width+=g->width;

      if(text[i+1]!='\0')width+=gap;
    }
  }

  return width;
}

int drawFootballCharacter(int x,int y,char c){

  const FootballGlyph *g=findFootballGlyph(c);

  if(!g)return 0;

  for(int r=0;r<5;r++){

    for(int col=0;col<g->width;col++){

      if(g->rows[r]&(1<<(g->width-1-col))){
        setPx(x+col,y+r);
      }
    }
  }

  return g->width;
}

void drawFootballText(
  int x,
  int y,
  const char *text,
  int gap
){

  for(int i=0;text[i]!='\0';i++){
    x+=drawFootballCharacter(x,y,text[i])+gap;
  }
}

void drawCenteredFootballText(
  int startX,
  int regionWidth,
  int y,
  const char *text,
  int gap
){

  int width=footballTextWidth(text,gap);

  if(width<=regionWidth){

    drawFootballText(
      startX+(regionWidth-width)/2,
      y,
      text,
      gap
    );
  }
}

// =================================================
// FOOTBALL LED TIMER
// =================================================

void drawFootballTimer(){

  char timerText[8];

  snprintf(
    timerText,
    sizeof(timerText),
    "%02lu:%02lu",
    (timerRemaining/60UL)%100UL,
    timerRemaining%60UL
  );

  int x=13;
  const int y=9;

  for(int i=0;timerText[i]!='\0';i++){

    x+=drawFootballCharacter(x,y,timerText[i]);

    if(timerText[i+1]!='\0')x++;
  }
}

// =================================================
// FOOTBALL / BASKETBALL LED RENDERING
// =================================================

void renderFootball(){

  memset(fb,0,sizeof(fb));

  char teamText[12];
  char scoreText[12];

  snprintf(teamText,sizeof(teamText),"%s-%s",teamA,teamB);
  snprintf(scoreText,sizeof(scoreText),"%d-%d",scoreA,scoreB);

  drawCenteredFootballText(0,32,1,teamText,1);

  drawCenteredFootballText(0,13,9,scoreText,0);

  drawFootballTimer();

  flushDisplay();
}

void renderBasketball(){
  renderFootball();
}

void renderAll(){

  if(currentSport==CRICKET){

    if(cricketMatchOver){

      if(cricketAnimation!=CRICKET_ANIM_WIN){
        startCricketAnimation(CRICKET_ANIM_WIN);
      }else{
        renderCricketAnimation();
      }

    }else if(cricketAnimation!=CRICKET_ANIM_NONE){

      renderCricketAnimation();

    }else{

      renderCricket();
    }

  }else if(currentSport==FOOTBALL){

    renderFootball();

  }else{

    renderBasketball();
  }
}

// =================================================
// UNDO SYSTEM AND HISTORY
// =================================================

String historyLines[12];

int historyCount=0;

void logEvent(const String &s);

struct Snapshot {

  int a,b,wk,balls,firstRuns,target,batting,duration,overs;

  bool second,running;

  bool configured;
  bool matchStarted;
  bool inningsDone;
  bool matchOver;

  int winner;

  unsigned long timer;

  String period;

  char nameA[4],nameB[4];
};

Snapshot previous;

bool snapshotValid=false;

void takeSnapshot(){

  previous.a=scoreA;
  previous.b=scoreB;

  previous.wk=wickets;
  previous.balls=legalBalls;

  previous.firstRuns=firstInningsRuns;
  previous.target=targetRuns;

  previous.batting=battingTeam;

  previous.duration=configuredMinutes;
  previous.overs=configuredOvers;

  previous.second=secondInnings;
  previous.running=timerRunning;

  previous.configured=teamsConfigured;
  previous.matchStarted=cricketMatchStarted;
  previous.inningsDone=inningsComplete;
  previous.matchOver=cricketMatchOver;

  previous.winner=winnerTeam;

  previous.timer=timerRemaining;

  previous.period=matchPeriod;

  strcpy(previous.nameA,teamA);
  strcpy(previous.nameB,teamB);

  snapshotValid=true;
}

void restoreSnapshot(){

  if(!snapshotValid){
    logEvent("Nothing to undo");
    return;
  }

  scoreA=previous.a;
  scoreB=previous.b;

  wickets=previous.wk;
  legalBalls=previous.balls;

  firstInningsRuns=previous.firstRuns;
  targetRuns=previous.target;

  battingTeam=previous.batting;

  configuredMinutes=previous.duration;
  configuredOvers=previous.overs;

  secondInnings=previous.second;
  timerRunning=previous.running;

  teamsConfigured=previous.configured;
  cricketMatchStarted=previous.matchStarted;
  inningsComplete=previous.inningsDone;
  cricketMatchOver=previous.matchOver;

  winnerTeam=previous.winner;

  timerRemaining=previous.timer;

  matchPeriod=previous.period;

  strcpy(teamA,previous.nameA);
  strcpy(teamB,previous.nameB);

  lastTimerTick=millis();

  snapshotValid=false;

  stopCricketAnimation();

  if(currentSport==CRICKET&&cricketMatchOver){
    startCricketAnimation(CRICKET_ANIM_WIN);
  }

  logEvent("Last action undone");
}

void logEvent(const String &s){

  lastEvent=s;

  for(int i=10;i>=0;i--){
    historyLines[i+1]=historyLines[i];
  }

  historyLines[0]=s;

  if(historyCount<12){
    historyCount++;
  }
}

String jsonEscape(const String &s){

  String o;

  for(size_t i=0;i<s.length();i++){

    char c=s[i];

    if(c=='"'||c=='\\')o+='\\';

    if(c=='\n')o+=" ";
    else o+=c;
  }

  return o;
}

// =================================================
// CRICKET INNINGS AND RESULT LOGIC
// =================================================

int getCurrentBattingScore(){
  return (battingTeam==0)?scoreA:scoreB;
}

int getOversBallLimit(){
  return configuredOvers*6;
}

void completeFirstInnings(){

  if(
    !cricketMatchStarted ||
    secondInnings ||
    inningsComplete ||
    cricketMatchOver
  ){
    return;
  }

  firstInningsRuns=getCurrentBattingScore();

  targetRuns=firstInningsRuns+1;

  inningsComplete=true;

  logEvent(
    "First innings complete. Target "+
    String(targetRuns)
  );
}

void finishCricketMatch(int winner){

  if(cricketMatchOver)return;

  cricketMatchOver=true;
  winnerTeam=winner;

  inningsComplete=false;

  stopCricketAnimation();

  startCricketAnimation(CRICKET_ANIM_WIN);

  String winnerName=
    (winnerTeam==0)?String(teamA):String(teamB);

  logEvent(winnerName+" won the match");
}

void evaluateCricketResult(){

  if(
    !cricketMatchStarted ||
    cricketMatchOver ||
    inningsComplete
  ){
    return;
  }

  int battingScore=getCurrentBattingScore();

  bool allOut=(wickets>=10);

  bool oversFinished=(legalBalls>=getOversBallLimit());

  if(secondInnings){

    if(
      targetRuns>0 &&
      battingScore>=targetRuns
    ){
      finishCricketMatch(battingTeam);
      return;
    }

    if(allOut||oversFinished){
      finishCricketMatch(1-battingTeam);
      return;
    }

  }else{

    if(allOut||oversFinished){
      completeFirstInnings();
    }
  }
}

// =================================================
// WEBSITE STATE JSON
// =================================================

String stateJson(){

  String s="{";

  s+="\"sport\":\"";

  if(currentSport==CRICKET)s+="cricket";
  else if(currentSport==FOOTBALL)s+="football";
  else s+="basketball";

  s+="\",";

  s+="\"aName\":\"";
  s+=jsonEscape(String(teamA));
  s+="\",";

  s+="\"bName\":\"";
  s+=jsonEscape(String(teamB));
  s+="\",";

  s+="\"a\":";
  s+=String(scoreA);
  s+=",";

  s+="\"b\":";
  s+=String(scoreB);
  s+=",";

  s+="\"wickets\":";
  s+=String(wickets);
  s+=",";

  s+="\"legalBalls\":";
  s+=String(legalBalls);
  s+=",";

  s+="\"overs\":\"";
  s+=String(legalBalls/6);
  s+=".";
  s+=String(legalBalls%6);
  s+="\",";

  s+="\"oversLimit\":";
  s+=String(configuredOvers);
  s+=",";

  s+="\"second\":";
  s+=(secondInnings?"true":"false");
  s+=",";

  s+="\"target\":";
  s+=String(targetRuns);
  s+=",";

  s+="\"firstRuns\":";
  s+=String(firstInningsRuns);
  s+=",";

  s+="\"batting\":";
  s+=String(battingTeam);
  s+=",";

  s+="\"battingName\":\"";
  s+=jsonEscape(String(battingTeam==0?teamA:teamB));
  s+="\",";

  s+="\"teamsConfigured\":";
  s+=(teamsConfigured?"true":"false");
  s+=",";

  s+="\"matchStarted\":";
  s+=(cricketMatchStarted?"true":"false");
  s+=",";

  s+="\"inningsComplete\":";
  s+=(inningsComplete?"true":"false");
  s+=",";

  s+="\"matchOver\":";
  s+=(cricketMatchOver?"true":"false");
  s+=",";

  s+="\"winner\":";
  s+=String(winnerTeam);
  s+=",";

  s+="\"winnerName\":\"";

  if(winnerTeam==0)s+=jsonEscape(String(teamA));
  else if(winnerTeam==1)s+=jsonEscape(String(teamB));

  s+="\",";

  s+="\"timer\":";
  s+=String(timerRemaining);
  s+=",";

  s+="\"duration\":";
  s+=String(configuredMinutes);
  s+=",";

  s+="\"period\":\"";
  s+=jsonEscape(matchPeriod);
  s+="\",";

  s+="\"history\":[";

  for(int i=0;i<historyCount;i++){

    if(i>0)s+=",";

    s+="\"";
    s+=jsonEscape(historyLines[i]);
    s+="\"";
  }

  s+="]}";

  return s;
}

void sendState(){
  server.send(200,"application/json",stateJson());
}

void stopTimer(){
  timerRunning=false;
}

// =================================================
// SAVE AND LOAD SPORT DATA
// =================================================

void saveCurrentSportState(){

  int index=(int)currentSport;

  SportState &d=savedSports[index];

  strncpy(d.teamA,teamA,sizeof(d.teamA));
  strncpy(d.teamB,teamB,sizeof(d.teamB));

  d.teamA[3]='\0';
  d.teamB[3]='\0';

  d.scoreA=scoreA;
  d.scoreB=scoreB;

  d.wickets=wickets;
  d.legalBalls=legalBalls;

  d.secondInnings=secondInnings;

  d.firstInningsRuns=firstInningsRuns;
  d.targetRuns=targetRuns;

  d.battingTeam=battingTeam;

  d.teamsConfigured=teamsConfigured;
  d.matchStarted=cricketMatchStarted;
  d.inningsComplete=inningsComplete;
  d.matchOver=cricketMatchOver;

  d.winnerTeam=winnerTeam;
  d.configuredOvers=configuredOvers;

  d.matchPeriod=matchPeriod;

  d.timerRemaining=timerRemaining;
  d.timerRunning=timerRunning;

  d.configuredMinutes=configuredMinutes;
}

void loadSportState(Sport sport){

  currentSport=sport;

  SportState &d=savedSports[(int)sport];

  strncpy(teamA,d.teamA,sizeof(teamA));
  strncpy(teamB,d.teamB,sizeof(teamB));

  teamA[3]='\0';
  teamB[3]='\0';

  scoreA=d.scoreA;
  scoreB=d.scoreB;

  wickets=d.wickets;
  legalBalls=d.legalBalls;

  secondInnings=d.secondInnings;

  firstInningsRuns=d.firstInningsRuns;
  targetRuns=d.targetRuns;

  battingTeam=d.battingTeam;

  teamsConfigured=d.teamsConfigured;
  cricketMatchStarted=d.matchStarted;
  inningsComplete=d.inningsComplete;
  cricketMatchOver=d.matchOver;

  winnerTeam=d.winnerTeam;
  configuredOvers=d.configuredOvers;

  matchPeriod=d.matchPeriod;

  timerRemaining=d.timerRemaining;
  timerRunning=d.timerRunning;

  configuredMinutes=d.configuredMinutes;

  lastTimerTick=millis();
}

void initializeSportStates(){

  for(int i=0;i<3;i++){

    savedSports[i].scoreA=0;
    savedSports[i].scoreB=0;

    savedSports[i].wickets=0;
    savedSports[i].legalBalls=0;

    savedSports[i].secondInnings=false;

    savedSports[i].firstInningsRuns=0;
    savedSports[i].targetRuns=0;

    savedSports[i].battingTeam=0;

    savedSports[i].teamsConfigured=false;
    savedSports[i].matchStarted=false;
    savedSports[i].inningsComplete=false;
    savedSports[i].matchOver=false;

    savedSports[i].winnerTeam=-1;

    savedSports[i].configuredOvers=20;

    savedSports[i].timerRunning=false;
  }

  // Cricket
  strcpy(savedSports[CRICKET].teamA,"IND");
  strcpy(savedSports[CRICKET].teamB,"AUS");

  savedSports[CRICKET].matchPeriod="1st Half";
  savedSports[CRICKET].configuredMinutes=90;
  savedSports[CRICKET].timerRemaining=90UL*60UL;
  savedSports[CRICKET].configuredOvers=20;

  // Football
  strcpy(savedSports[FOOTBALL].teamA,"IND");
  strcpy(savedSports[FOOTBALL].teamB,"BRA");

  savedSports[FOOTBALL].matchPeriod="1st Half";
  savedSports[FOOTBALL].configuredMinutes=90;
  savedSports[FOOTBALL].timerRemaining=90UL*60UL;

  // Basketball
  strcpy(savedSports[BASKETBALL].teamA,"IND");
  strcpy(savedSports[BASKETBALL].teamB,"USA");

  savedSports[BASKETBALL].matchPeriod="1st Half";
  savedSports[BASKETBALL].configuredMinutes=10;
  savedSports[BASKETBALL].timerRemaining=10UL*60UL;

  loadSportState(CRICKET);
}

// =================================================
// SPORT SELECTION
// =================================================

void setSport(const String &name){

  Sport nextSport;

  if(name=="cricket")nextSport=CRICKET;
  else if(name=="football")nextSport=FOOTBALL;
  else if(name=="basketball")nextSport=BASKETBALL;
  else return;

  if(nextSport!=currentSport){

    stopTimer();

    saveCurrentSportState();

    stopCricketAnimation();

    loadSportState(nextSport);

    snapshotValid=false;

    stopCricketAnimation();
  }

  renderAll();
}

// =================================================
// WEB ACTION HANDLER
// =================================================

void handleAction(){

  String type=server.arg("type");
  String value=server.arg("value");

  int teamIndex=0;
  int amount=0;

  if(type=="undo"){

    restoreSnapshot();

  }else{

    takeSnapshot();

    // ---------------------------------------------
    // CRICKET SETUP: TEAMS + OVERS
    // ---------------------------------------------

    if(type=="setup"&&currentSport==CRICKET){

      if(cricketMatchStarted){

        logEvent("Cannot change setup during match");

      }else{

        int firstComma=value.indexOf(',');
        int secondComma=value.indexOf(',',firstComma+1);

        if(firstComma>0&&secondComma>firstComma){

          String a=value.substring(0,firstComma);
          String b=value.substring(firstComma+1,secondComma);

          int selectedOvers=value.substring(secondComma+1).toInt();

          a.toUpperCase();
          b.toUpperCase();

          a.replace(" ","");
          b.replace(" ","");

          if(
            a.length()>0 &&
            b.length()>0 &&
            selectedOvers>=1 &&
            selectedOvers<=99
          ){

            a=a.substring(0,3);
            b=b.substring(0,3);

            a.toCharArray(teamA,sizeof(teamA));
            b.toCharArray(teamB,sizeof(teamB));

            configuredOvers=selectedOvers;

            teamsConfigured=true;

            logEvent(
              "Teams set. "+
              String(configuredOvers)+
              " overs per innings"
            );

          }else{

            logEvent("Invalid team names or overs");
          }

        }else{

          logEvent("Invalid setup details");
        }
      }
    }

    // ---------------------------------------------
    // TEAM NAMES FOR FOOTBALL / BASKETBALL
    // ---------------------------------------------

    else if(type=="names"&&currentSport!=CRICKET){

      int comma=value.indexOf(',');

      if(comma>0){

        String a=value.substring(0,comma);
        String b=value.substring(comma+1);

        a.toUpperCase();
        b.toUpperCase();

        a.replace(" ","");
        b.replace(" ","");

        if(a.length()>0&&b.length()>0){

          a=a.substring(0,3);
          b=b.substring(0,3);

          a.toCharArray(teamA,sizeof(teamA));
          b.toCharArray(teamB,sizeof(teamB));

          logEvent("Team names updated");
        }
      }
    }

    // ---------------------------------------------
    // PLAY AGAIN: SAME TEAMS AND SAME OVERS
    // ---------------------------------------------

    else if(type=="playagain"&&currentSport==CRICKET){

      if(teamsConfigured){

        scoreA=0;
        scoreB=0;

        wickets=0;
        legalBalls=0;

        secondInnings=false;

        firstInningsRuns=0;
        targetRuns=0;

        battingTeam=0;

        inningsComplete=false;
        cricketMatchOver=false;

        winnerTeam=-1;

        cricketMatchStarted=false;

        teamsConfigured=true;

        stopCricketAnimation();

        logEvent("Ready for another match with same teams and overs");

      }else{

        logEvent("Set up teams before starting another match");
      }
    }

    // ---------------------------------------------
    // EDIT TEAM / OVERS SETUP
    // ---------------------------------------------

    else if(type=="editteams"&&currentSport==CRICKET){

      scoreA=0;
      scoreB=0;

      wickets=0;
      legalBalls=0;

      secondInnings=false;

      firstInningsRuns=0;
      targetRuns=0;

      battingTeam=0;

      teamsConfigured=false;
      cricketMatchStarted=false;
      inningsComplete=false;
      cricketMatchOver=false;

      winnerTeam=-1;

      stopCricketAnimation();

      logEvent("Ready for new team and overs setup");
    }

    // ---------------------------------------------
    // START CRICKET MATCH
    // ---------------------------------------------

    else if(type=="startmatch"&&currentSport==CRICKET){

      if(teamsConfigured&&!cricketMatchStarted){

        int selectedTeam=value.toInt();

        if(selectedTeam!=0&&selectedTeam!=1){
          selectedTeam=0;
        }

        scoreA=0;
        scoreB=0;

        wickets=0;
        legalBalls=0;

        firstInningsRuns=0;
        targetRuns=0;

        battingTeam=selectedTeam;

        secondInnings=false;
        inningsComplete=false;
        cricketMatchOver=false;

        winnerTeam=-1;

        cricketMatchStarted=true;

        stopCricketAnimation();

        logEvent(
          String(battingTeam==0?teamA:teamB)+
          " batting first"
        );

      }else{

        logEvent("Complete team and overs setup first");
      }
    }

    // ---------------------------------------------
    // RUNS / POINTS / BALLS / WICKETS / EXTRAS
    // ---------------------------------------------

    else if(
      type=="runs"||
      type=="points"||
      type=="ball"||
      type=="wicket"||
      type=="extra"
    ){

      int comma=value.indexOf(',');

      if(comma>=0){

        teamIndex=value.substring(0,comma).toInt();
        amount=value.substring(comma+1).toInt();

      }else{

        teamIndex=value.toInt();
      }

      if(teamIndex<0||teamIndex>1){
        teamIndex=0;
      }

      int &teamScore=(teamIndex==0)?scoreA:scoreB;

      const char *teamName=(teamIndex==0)?teamA:teamB;

      // CRICKET RUNS
      if(type=="runs"){

        if(currentSport==CRICKET){

          if(
            !cricketMatchStarted||
            inningsComplete||
            cricketMatchOver
          ){

            logEvent("Cricket scoring is not active");

          }else if(teamIndex!=battingTeam){

            logEvent("Only batting team can score");

          }else if(
            amount>=1&&
            amount<=6&&
            teamScore+amount<=999
          ){

            teamScore+=amount;

            if(amount==4){
              startCricketAnimation(CRICKET_ANIM_FOUR);
            }else if(amount==6){
              startCricketAnimation(CRICKET_ANIM_SIX);
            }else{
              stopCricketAnimation();
            }

            logEvent(
              String(teamName)+
              " +"+
              String(amount)+
              " runs"
            );

            evaluateCricketResult();
          }
        }
      }

      // FOOTBALL / BASKETBALL POINTS
      else if(type=="points"){

        if(
          currentSport!=CRICKET&&
          teamScore+amount>=0&&
          teamScore+amount<=999
        ){

          teamScore+=amount;

          String sign=(amount>=0)?"+":"";

          logEvent(
            String(teamName)+
            " score "+
            sign+
            String(amount)
          );
        }
      }

      // LEGAL BALL
      else if(type=="ball"){

        if(
          currentSport==CRICKET&&
          cricketMatchStarted&&
          !inningsComplete&&
          !cricketMatchOver&&
          teamIndex==battingTeam&&
          legalBalls<getOversBallLimit()
        ){

          legalBalls++;

          stopCricketAnimation();

          logEvent("Legal ball added");

          evaluateCricketResult();

        }else if(currentSport==CRICKET&&teamIndex!=battingTeam){

          logEvent("Only batting team can add a ball");

        }else if(currentSport==CRICKET&&legalBalls>=getOversBallLimit()){

          evaluateCricketResult();
        }
      }

      // WICKET
      else if(type=="wicket"){

        if(
          currentSport==CRICKET&&
          cricketMatchStarted&&
          !inningsComplete&&
          !cricketMatchOver&&
          teamIndex==battingTeam&&
          wickets<10
        ){

          wickets++;

          evaluateCricketResult();

          if(!cricketMatchOver){
            startCricketAnimation(CRICKET_ANIM_WICKET);
          }
        }
      }

      // WIDE AND NO BALL
      else if(type=="extra"){

        if(
          currentSport==CRICKET&&
          cricketMatchStarted&&
          !inningsComplete&&
          !cricketMatchOver&&
          teamIndex==battingTeam&&
          teamScore<999
        ){

          teamScore++;

          if(value.endsWith("wide")){

            startCricketAnimation(CRICKET_ANIM_WIDE);
            logEvent("Wide ball +1");

          }else{

            startCricketAnimation(CRICKET_ANIM_NOBALL);
            logEvent("No ball +1");
          }

          evaluateCricketResult();
        }
      }
    }

    // ---------------------------------------------
    // MANUAL END FIRST INNINGS
    // ---------------------------------------------

    else if(type=="endinnings"&&currentSport==CRICKET){

      if(
        cricketMatchStarted&&
        !secondInnings&&
        !inningsComplete&&
        !cricketMatchOver
      ){

        stopCricketAnimation();

        completeFirstInnings();

      }else{

        logEvent("Cannot end innings now");
      }
    }

    // ---------------------------------------------
    // START SECOND INNINGS
    // ---------------------------------------------

    else if(type=="startsecond"&&currentSport==CRICKET){

      if(
        cricketMatchStarted&&
        inningsComplete&&
        !secondInnings&&
        !cricketMatchOver
      ){

        battingTeam=1-battingTeam;

        wickets=0;
        legalBalls=0;

        secondInnings=true;

        inningsComplete=false;

        stopCricketAnimation();

        logEvent(
          "Second innings started. Target "+
          String(targetRuns)
        );

      }else{

        logEvent("Second innings is not ready");
      }
    }

    // ---------------------------------------------
    // FOOTBALL / BASKETBALL PERIOD
    // ---------------------------------------------

    else if(type=="period"&&currentSport!=CRICKET){

      if(
        value=="1st Half"||
        value=="Half Time"||
        value=="2nd Half"
      ){

        matchPeriod=value;

        if(matchPeriod=="Half Time"){
          stopTimer();
        }

        logEvent("Period: "+matchPeriod);
      }
    }

    // ---------------------------------------------
    // UPDATE TIMER DURATION
    // ---------------------------------------------

    else if(type=="duration"&&currentSport!=CRICKET){

      int minutes=value.toInt();

      if(minutes>=1&&minutes<=99){

        configuredMinutes=minutes;

        timerRemaining=(unsigned long)minutes*60UL;

        stopTimer();

        lastTimerTick=millis();

        logEvent(
          "Timer updated to "+
          String(minutes)+
          " minutes"
        );
      }
    }

    // ---------------------------------------------
    // TIMER START / PAUSE / RESET
    // ---------------------------------------------

    else if(type=="timer"&&currentSport!=CRICKET){

      if(
        value=="start"&&
        timerRemaining>0&&
        matchPeriod!="Half Time"
      ){

        timerRunning=true;
        lastTimerTick=millis();

        logEvent("Timer started");

      }else if(value=="pause"){

        stopTimer();

        logEvent("Timer paused");

      }else if(value=="reset"){

        stopTimer();

        timerRemaining=(unsigned long)configuredMinutes*60UL;

        lastTimerTick=millis();

        logEvent(
          "Timer reset to "+
          String(configuredMinutes)+
          " minutes"
        );
      }
    }

    // ---------------------------------------------
    // RESET MATCH
    // ---------------------------------------------

    else if(type=="reset"){

      scoreA=0;
      scoreB=0;

      wickets=0;
      legalBalls=0;

      secondInnings=false;

      firstInningsRuns=0;
      targetRuns=0;

      battingTeam=0;

      inningsComplete=false;
      cricketMatchOver=false;

      winnerTeam=-1;

      stopCricketAnimation();
      stopTimer();

      if(currentSport==CRICKET){

        cricketMatchStarted=false;

      }else{

        timerRemaining=(unsigned long)configuredMinutes*60UL;

        matchPeriod="1st Half";
      }

      logEvent("Match reset");
    }
  }

  saveCurrentSportState();

  renderAll();

  String response="{\"ok\":true,\"message\":\"";
  response+=jsonEscape(lastEvent);
  response+="\"}";

  server.send(200,"application/json",response);
}

// =================================================
// WEB SERVER
// =================================================

void handleChoose(){

  String s=server.arg("sport");

  if(s!="none"){
    setSport(s);
  }

  server.send(200,"application/json","{\"ok\":true}");
}

void setupServer(){

  server.on(
    "/",
    HTTP_GET,
    [](){
      server.send_P(200,"text/html",INDEX_HTML);
    }
  );

  server.on("/state",HTTP_GET,sendState);

  server.on("/action",HTTP_GET,handleAction);

  server.on("/choose",HTTP_GET,handleChoose);

  server.onNotFound(
    [](){
      server.send(404,"text/plain","Not found");
    }
  );

  server.begin();
}

// =================================================
// SETUP
// =================================================

void setup(){

  Serial.begin(115200);

  initializeSportStates();

  mx.begin();

  mx.control(MD_MAX72XX::INTENSITY,4);

  mx.clear();

  renderAll();

  WiFi.mode(WIFI_AP);

  WiFi.setSleep(false);

  IPAddress localIP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);

  WiFi.softAPConfig(localIP,gateway,subnet);

  bool ok=WiFi.softAP(
    AP_SSID,
    AP_PASSWORD,
    6,
    false,
    4
  );

  delay(300);

  Serial.println();

  Serial.println("WIRELESS SPORTS SCOREBOARD");

  Serial.printf("Hotspot: %s\n",AP_SSID);

  Serial.printf("Password: %s\n",AP_PASSWORD);

  Serial.printf("AP started: %s\n",ok?"YES":"NO");

  Serial.print("Open website: http://");

  Serial.println(WiFi.softAPIP());

  setupServer();
}

// =================================================
// MAIN LOOP
// =================================================

void loop(){

  server.handleClient();

  // FOOTBALL AND BASKETBALL TIMER
  if(timerRunning&&currentSport!=CRICKET){

    unsigned long now=millis();

    while(now-lastTimerTick>=1000){

      lastTimerTick+=1000;

      if(timerRemaining>0){
        timerRemaining--;
      }

      if(timerRemaining==0){

        stopTimer();

        logEvent("Match timer finished");
      }

      saveCurrentSportState();

      renderAll();
    }
  }

  // CRICKET EVENT ANIMATIONS
  if(
    currentSport==CRICKET&&
    cricketAnimation!=CRICKET_ANIM_NONE
  ){

    unsigned long now=millis();

    if(
      cricketAnimation!=CRICKET_ANIM_WIN&&
      now-animationStartedAt>=ANIMATION_DURATION_MS
    ){

      stopCricketAnimation();

      renderCricket();

    }else if(now-lastAnimationFrame>=animationFrameInterval){

      lastAnimationFrame=now;

      if(cricketAnimation==CRICKET_ANIM_WICKET){

        animationOffset--;

        if(animationOffset<=-WICKET_STEP){
          animationOffset+=WICKET_STEP;
        }

      }else{

        animationOffset--;

        if(
          cricketAnimation==CRICKET_ANIM_WIN&&
          animationOffset<-getAnimationWidth()
        ){
          animationOffset=W;
        }
      }

      renderCricketAnimation();
    }
  }
}