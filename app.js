let token = "";
let role  = "none";
let data  = {};
let currentSubject = "";
let selectedDays   = [];
let draftPeriods   = [];

const DAY_NAMES = ["Mon","Tue","Wed","Thu","Fri","Sat","Sun"];

function el(id){ return document.getElementById(id); }
function showView(id){ document.querySelectorAll(".view").forEach(v=>v.classList.remove("active")); el(id).classList.add("active"); window.scrollTo(0,0); }
function showHODTab(name){ document.querySelectorAll("#viewHOD .panel").forEach(p=>p.classList.remove("active")); document.querySelectorAll("#viewHOD .tab").forEach(t=>t.classList.remove("active")); el("panel-"+name).classList.add("active"); const btn = document.querySelector('#viewHOD .tab[data-tab="'+name+'"]'); if (btn) btn.classList.add("active"); }
function showSubjectTab(name){ document.querySelectorAll("#viewTeacherSubject .panel").forEach(p=>p.classList.remove("active")); document.querySelectorAll("#viewTeacherSubject .tab").forEach(t=>t.classList.remove("active")); el("panel-"+name).classList.add("active"); const btn = document.querySelector('#viewTeacherSubject .tab[data-tab="'+name+'"]'); if (btn) btn.classList.add("active"); }

async function api(url){ try { const r = await fetch(url); if (!r.ok) return null; return await r.json(); } catch(e){ console.log(e); return null; } }
async function cmd(c, extra){
  const url = "/api/cmd?token=" + encodeURIComponent(token) + "&cmd=" + c + (extra || "");
  try { const r = await fetch(url); if (r.status === 401) { logout(); return { ok:false, message:"Session expired" }; } return await r.json(); } catch(e) { return { ok:false, message:"Network error" }; }
}
function esc(v){ if (v == null) return ""; return String(v).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;"); }
function hideAllLoginBoxes(){ ["hodLoginBox","teacherLoginBox"].forEach(id => el(id).style.display = "none"); }

function showHODLogin(){ hideAllLoginBoxes(); el("hodLoginBox").style.display="block"; el("hodPass").focus(); }
function showTeacherLogin(){ hideAllLoginBoxes(); el("teacherLoginBox").style.display="block"; el("teacherPass").focus(); }

async function doHODLogin(){
  const pass = el("hodPass").value;
  if (!pass) { el("hodMsg").innerText = "Enter HOD password."; return; }
  const r = await api("/api/login?role=hod&pass=" + encodeURIComponent(pass));
  if (!r || !r.ok) { el("hodMsg").innerText = (r && r.message) || "Login failed"; return; }
  token = r.token; role = "hod"; el("hodMsg").innerText = ""; el("hodPass").value = ""; hideAllLoginBoxes();
  showView("viewHOD"); showHODTab("h-students"); loadData();
}

async function doTeacherLogin(){
  const id   = el("teacherID").value.trim();
  const pass = el("teacherPass").value;
  if (!id || !pass) { el("teacherMsg").innerText = "Enter Teacher ID and password."; return; }
  const r = await api("/api/login?role=teacher&teacherId=" + encodeURIComponent(id) + "&pass=" + encodeURIComponent(pass));
  if (!r || !r.ok) { el("teacherMsg").innerText = (r && r.message) || "Login failed"; return; }
  token = r.token; role = "teacher"; el("teacherMsg").innerText = ""; el("teacherID").value = ""; el("teacherPass").value = "";
  hideAllLoginBoxes(); showView("viewTeacherSubjects"); loadData();
}

function openStudentPortal(){ role = "student"; token = ""; showView("viewStudent"); el("stuResult").innerHTML = ""; el("stuLookupMsg").innerText = ""; el("faceVerifyCard").style.display = "none"; }
function logout(){ token = ""; role = "none"; currentSubject = ""; document.querySelectorAll(".view").forEach(v=>v.classList.remove("active")); el("viewLogin").classList.add("active"); hideAllLoginBoxes(); resetCamera(); }

async function loadData(){
  const d = await api("/api/data?token=" + encodeURIComponent(token));
  if (!d) return; data = d; el("clockTime").innerText = d.time || "--"; el("clockDate").innerText = d.date || "--";
  if (role === "hod") renderHOD();
  if (role === "teacher") renderTeacher();
}

function renderHOD(){ renderHODStudents(); renderHODDevices(); renderHODTeachers(); renderHODTimetable(); renderHODAttendance(); }

function renderHODStudents(){
  let html = "";
  // Filter out any accidentally empty records
  const validStudents = data.students.filter(s => s.id && s.name);

  if (validStudents.length === 0){ html = '<tr><td colspan="6" class="empty">No students registered</td></tr>'; }
  else {
    validStudents.forEach((s,i)=>{
      const on = s.online ? '<span class="badge online">ONLINE</span>' : '<span class="badge offline">OFFLINE</span>';
      html += '<tr><td>'+(i+1)+'</td><td>'+esc(s.id)+'</td><td>'+esc(s.name)+'</td><td>'+(s.mac || "—")+'</td><td>'+on+'</td>' +
        '<td><button class="btn small primary" onclick="openEdit('+i+')">EDIT</button> <button class="btn small red" onclick="hodDeleteStudent('+i+')">DELETE</button></td></tr>';
    });
  }
  el("hStuBody").innerHTML = html; el("hStuCount").innerText = validStudents.length + " student(s)";
}

function renderHODDevices(){
  let dh = "";
  if (!data.devices || data.devices.length === 0){ dh = '<tr><td colspan="4" class="empty">No connected devices</td></tr>'; }
  else {
    data.devices.forEach((d,i)=>{
      dh += '<tr><td>'+(i+1)+'</td><td>'+esc(d.mac)+'</td><td>' + (d.registered ? '<span class="badge online">YES</span>' : '<span class="badge offline">NO</span>') + '</td>' +
        '<td>' + (d.registered ? "—" : '<button class="btn small primary" onclick="prefillFromDevice(\''+esc(d.mac)+'\')">REGISTER</button>') + '</td></tr>';
    });
  }
  el("hDevBody").innerHTML = dh;
}

function prefillFromDevice(mac){ el("hStuMAC").value = mac; el("hStuID").focus(); window.scrollTo(0,0); }
function renderHODTeachers(){
  let html = "";
  if (!data.teachers || data.teachers.length === 0){ html = '<tr><td colspan="5" class="empty">No teachers added</td></tr>'; }
  else {
    data.teachers.forEach((t,i)=>{
      const count = (data.subjects || []).filter(s => s.teacherId === t.id).length;
      html += '<tr><td>'+(i+1)+'</td><td>'+esc(t.id)+'</td><td>'+esc(t.name)+'</td><td>'+count+'</td><td><button class="btn small red" onclick="hodDeleteTeacher('+i+')">DELETE</button></td></tr>';
    });
  }
  el("hTrBody").innerHTML = html;
  const sel = el("hSubTeacher"); sel.innerHTML = '<option value="">-- Select Teacher --</option>';
  (data.teachers || []).forEach(t=>{ sel.innerHTML += '<option value="'+esc(t.id)+'">'+esc(t.name)+' ('+esc(t.id)+')</option>'; });
}

function renderHODTimetable(){
  let html = "";
  if (!data.subjects || data.subjects.length === 0){ html = '<tr><td colspan="7" class="empty">No subjects created</td></tr>'; }
  else {
    data.subjects.forEach((s,i)=>{
      const periods = (s.periodTimes && s.periodTimes.length) ? s.periodTimes.join(", ") : "—";
      const days = s.days || "—";
      html += '<tr><td>'+(i+1)+'</td><td>'+esc(s.code)+'</td><td>'+esc(s.name)+'</td><td>'+esc(s.teacherName || "—")+'</td><td>'+esc(days)+'</td><td>'+esc(periods)+'</td>' +
        '<td><button class="btn small primary" onclick="openSubModal(\''+esc(s.code)+'\')">EDIT SCHEDULE</button> <button class="btn small red" onclick="hodDeleteSubject(\''+esc(s.code)+'\')">DELETE</button></td></tr>';
    });
  }
  el("hSubBody").innerHTML = html;
}

function renderHODAttendance(){
  let html = "";
  if (!data.records || data.records.length === 0){ html = '<tr><td colspan="7" class="empty">No records yet</td></tr>'; }
  else {
    data.records.forEach((r,i)=>{
      let imgHtml = "—";
      if (r.photoFilename) {
        imgHtml = `<a href="/api/photo?filename=${r.photoFilename}" target="_blank">
                     <img src="/api/photo?filename=${r.photoFilename}" style="height:40px; width:40px; border-radius:50%; object-fit:cover;">
                   </a>`;
      }
      html += '<tr><td>'+(i+1)+'</td><td>'+esc(r.dateTime)+'</td><td>'+esc(r.subject)+'<br><span style="color:#8290a3;">'+esc(r.subjectName)+'</span></td><td>'+esc(r.period)+'</td><td><b>'+r.presentCount+'</b> / '+r.total+'</td>' +
        '<td>'+imgHtml+'</td>' +
        '<td><button class="btn small red" onclick="hodDeleteRecord('+i+')">DELETE</button></td></tr>';
    });
  }
  el("hRecBody").innerHTML = html;
  el("hRecCount").innerText = (data.records ? data.records.length : 0) + " record(s)";
  
  let sum = "";
  if (data.students.length === 0){ sum = '<tr><td colspan="6" class="empty">No students</td></tr>'; }
  else {
    const totalHeld = (data.records || []).length;
    data.students.forEach((s,i)=>{
      let attended = 0;
      (data.records || []).forEach(r=>{ if (listHasId(r.presentIds, s.id)) attended++; });
      const pct = totalHeld > 0 ? ((attended/totalHeld)*100).toFixed(1) : 0;
      sum += '<tr><td>'+(i+1)+'</td><td>'+esc(s.id)+'</td><td>'+esc(s.name)+'</td><td>'+totalHeld+'</td><td>'+attended+'</td><td><b>'+pct+'%</b></td></tr>';
    });
  }
  el("hStuSumBody").innerHTML = sum;
}

function listHasId(list, id){ if (!list) return false; return list.split(",").map(x=>x.trim()).some(x => x.toLowerCase() === id.toLowerCase()); }

async function hodRegisterStudent(){
  const id = el("hStuID").value.trim(), name = el("hStuName").value.trim(), mac = el("hStuMAC").value.trim();
  if (!id || !name || !mac){ el("hStuMsg").className = "msg err"; el("hStuMsg").innerText = "ID, name and MAC are all required."; return; }
  const r = await cmd("register", "&id=" + encodeURIComponent(id) + "&name=" + encodeURIComponent(name) + "&mac=" + encodeURIComponent(mac));
  el("hStuMsg").className = r.ok ? "msg" : "msg err"; el("hStuMsg").innerText = r.message;
  if (r.ok){ el("hStuID").value=""; el("hStuName").value=""; el("hStuMAC").value=""; } loadData();
}

async function hodDeleteStudent(i){ if (!confirm("Delete this student?")) return; await cmd("delete", "&index=" + i); loadData(); }
async function hodRegisterTeacher(){ const id = el("hTrID").value.trim(), name = el("hTrName").value.trim(); if (!id || !name){ el("hTrMsg").className = "msg err"; el("hTrMsg").innerText = "ID and name required."; return; } const r = await cmd("tradd", "&id=" + encodeURIComponent(id) + "&name=" + encodeURIComponent(name)); el("hTrMsg").className = r.ok ? "msg" : "msg err"; el("hTrMsg").innerText = r.message; if (r.ok){ el("hTrID").value=""; el("hTrName").value=""; } loadData(); }
async function hodDeleteTeacher(i){ if (!confirm("Delete this teacher?")) return; await cmd("trdel", "&index=" + i); loadData(); }
async function hodCreateSubject(){ const code = el("hSubCode").value.trim(), name = el("hSubName").value.trim(), teacher = el("hSubTeacher").value; if (!code || !name || !teacher){ el("hSubMsg").className = "msg err"; el("hSubMsg").innerText = "Code, name and teacher are required."; return; } const r = await cmd("subadd", "&code=" + encodeURIComponent(code) + "&name=" + encodeURIComponent(name) + "&teacherId=" + encodeURIComponent(teacher)); el("hSubMsg").className = r.ok ? "msg" : "msg err"; el("hSubMsg").innerText = r.message; if (r.ok){ el("hSubCode").value=""; el("hSubName").value=""; el("hSubTeacher").value=""; } loadData(); }
async function hodDeleteSubject(code){ if (!confirm("Delete subject " + code + "?")) return; await cmd("subdel", "&code=" + encodeURIComponent(code)); loadData(); }
async function hodDeleteRecord(i){ if (!confirm("Delete this record?")) return; await cmd("recdel", "&index=" + i); loadData(); }
async function hodClearRecords(){ if (!confirm("Delete ALL attendance records?")) return; await cmd("recclear"); loadData(); }

function renderTeacher(){
  const grid = el("subGrid"); let html = "";
  for (let i = 0; i < 5; i++){
    if (i < data.subjects.length){
      const s = data.subjects[i];
      html += '<div class="subcard" onclick="enterSubject(\''+esc(s.code)+'\')"><div class="code">'+esc(s.code)+'</div><div class="name">'+esc(s.name)+'</div><div class="teacher">👤 '+esc(s.teacherName || "—")+'</div><div class="sched">'+scheduleSummary(s)+'</div></div>';
    } else { html += '<div class="subcard empty-slot"><div class="code">Slot '+(i+1)+'</div><div class="name">Empty</div></div>'; }
  }
  grid.innerHTML = html;
  if (currentSubject) renderSubjectPortal();
}

function scheduleSummary(s){ const days = (s.days && s.days.length) ? s.days : "—"; const periods = (s.periodTimes && s.periodTimes.length) ? s.periodTimes.join(", ") : "no slots"; return "📅 "+days+" • "+periods; }
async function createSubject(){ const code = el("newSubCode").value.trim(), name = el("newSubName").value.trim(), teacher = el("newSubTeacher").value.trim(); if (!code || !name){ el("newSubMsg").className = "msg err"; el("newSubMsg").innerText = "Code and name are required."; return; } const r = await cmd("subadd", "&code=" + encodeURIComponent(code) + "&name=" + encodeURIComponent(name) + "&teacher=" + encodeURIComponent(teacher)); el("newSubMsg").className = r.ok ? "msg" : "msg err"; el("newSubMsg").innerText = r.message; if (r.ok){ el("newSubCode").value=""; el("newSubName").value=""; el("newSubTeacher").value=""; } loadData(); }
function enterSubject(code){ currentSubject = code; const s = data.subjects.find(x => x.code === code); if (!s) return; showView("viewTeacherSubject"); showSubjectTab("t-session"); el("subTitle").innerText = s.code + " — " + s.name; const ps = el("periodSelect"); ps.innerHTML = '<option value="">-- Period --</option>'; (s.periodTimes || []).forEach(p => { ps.innerHTML += '<option value="'+esc(p)+'">'+esc(p)+'</option>'; }); renderSubjectPortal(); }
function backToSubjects(){ currentSubject = ""; showView("viewTeacherSubjects"); }
function renderSubjectPortal(){ const s = data.subjects.find(x => x.code === currentSubject); if (!s) return; const running = data.running && data.subject === s.code; el("tRunState").innerText = running ? "RUNNING" : "STOPPED"; el("tStatusDot").className = running ? "dot on" : "dot"; el("tSessionInfo").innerText = running ? ("— " + (data.period || "?")) : ""; }
async function startSession(){ const p = el("periodSelect").value; if (!p){ alert("Select a period first."); return; } const r = await cmd("start", "&subject=" + encodeURIComponent(currentSubject) + "&period="  + encodeURIComponent(p)); if (r && !r.ok) alert(r.message); loadData(); }
async function stopSession(){ const r = await cmd("stop"); if (r && !r.ok) alert(r.message); loadData(); }

/* ============================================================
   STUDENT PORTAL & FACE CAMERA
   ============================================================ */
async function lookupStudent(){
  const id = el("stuLookup").value.trim();
  if (!id){ el("stuLookupMsg").innerText = "Enter your Student ID."; return; }
  el("stuLookupMsg").innerText = "";
  const res = await api("/api/student?id=" + encodeURIComponent(id));
  if (!res || !res.found){ el("stuResult").innerHTML = '<div class="card"><p class="empty">No student found with that ID.</p></div>'; return; }
  
  let rows = "";
  if (res.summary.length === 0){ rows = '<tr><td colspan="5" class="empty">No attendance records yet</td></tr>'; }
  else { res.summary.forEach(s=>{ rows += '<tr><td>'+esc(s.code)+'</td><td>'+esc(s.name)+'</td><td>'+s.held+'</td><td>'+s.attended+'</td><td><b>'+s.percent+'%</b></td></tr>'; }); }
  
  const live = res.present ? '<span class="badge present">PRESENT</span>' : '<span class="badge absent">ABSENT</span>';
  el("stuResult").innerHTML = '<div class="card"><h2>'+esc(res.name)+'</h2><div class="row" style="font-size:13px;color:#3c5068;"><span>ID: <b>'+esc(res.id)+'</b></span><span>Current: '+live+'</span></div></div>' +
    '<div class="card"><h2>Overall Attendance</h2><div class="stats"><div class="stat"><div class="stat-label">Classes Held</div><div class="stat-value">'+res.overall.held+'</div></div>' +
    '<div class="stat"><div class="stat-label">Attended</div><div class="stat-value" style="color:#139447;">'+res.overall.attended+'</div></div>' +
    '<div class="stat"><div class="stat-label">Missed</div><div class="stat-value" style="color:#e22c2c;">' + (res.overall.held - res.overall.attended) + '</div></div>' +
    '<div class="stat"><div class="stat-label">Percentage</div><div class="stat-value">'+res.overall.percent+'%</div></div></div></div>' +
    '<div class="card"><h2>Subject-wise Attendance</h2><div class="table-wrapper"><table><thead><tr><th>Code</th><th>Subject</th><th>Held</th><th>Attended</th><th>%</th></tr></thead><tbody>'+rows+'</tbody></table></div></div>';

  el("faceVerifyCard").style.display = "block";
  resetCamera();
}

let videoStream = null;
function startCamera() {
  const video = el('video'); video.style.display = 'block'; el('startCamBtn').style.display = 'none'; el('captureBtn').style.display = 'inline-block';
  navigator.mediaDevices.getUserMedia({ video: { facingMode: "user" } })
    .then(function (stream) { videoStream = stream; video.srcObject = stream; video.play(); el('faceMsg').innerText = "Align your face and click Capture."; })
    .catch(function (err) { el('faceMsg').className = "msg err"; el('faceMsg').innerText = "Camera access denied."; });
}

async function captureAndVerify() {
  const video = el('video'); const canvas = el('canvas'); const context = canvas.getContext('2d');
  canvas.width = 300; canvas.height = 240; context.drawImage(video, 0, 0, canvas.width, canvas.height);
  canvas.style.display = 'block'; video.style.display = 'none'; el('captureBtn').style.display = 'none';
  if (videoStream) { videoStream.getTracks().forEach(track => track.stop()); }

  el('faceMsg').className = "msg"; el('faceMsg').innerText = "Verifying face with AI...";
  const base64Image = canvas.toDataURL('image/jpeg', 0.85);

  try {
    const res = await fetch("/api/recognize", { method: 'POST', body: base64Image });
    const data = await res.json();

    if (data.ok) {
      el('faceMsg').innerText = "✅ Verified as " + data.name + "! Updating Firebase...";
      const markRes = await fetch("/api/mark", {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ studentId: data.studentId, mac: data.mac, faceMatched: true, subject: "General", period: "Face-Verify", photoFilename: data.photoFilename })
      });
      const markData = await markRes.json();
      
      if (markData.present) { el('faceMsg').className = "msg"; el('faceMsg').innerText = "🎉 Success! Attendance marked present."; }
      else { el('faceMsg').className = "msg err"; el('faceMsg').innerText = "Face matched, but your registered MAC address is different."; el('retryBtn').style.display = 'inline-block'; }
    } else {
      el('faceMsg').className = "msg err"; el('faceMsg').innerText = "❌ " + (data.msg || "Face not recognized."); el('retryBtn').style.display = 'inline-block';
    }
  } catch (e) {
    el('faceMsg').className = "msg err"; el('faceMsg').innerText = "Network Error. Make sure the laptop server is running."; el('retryBtn').style.display = 'inline-block';
  }
}

function resetCamera() { el('canvas').style.display = 'none'; el('video').style.display = 'none'; el('retryBtn').style.display = 'none'; el('startCamBtn').style.display = 'inline-block'; el('faceMsg').innerText = ""; }

// Refresh every 15 seconds to prevent ESP32 overload
setInterval(() => { if (role === "hod" || role === "teacher") loadData(); }, 15000);
