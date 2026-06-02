const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");

const chat = document.getElementById("chat");
const stateEl = document.getElementById("state");
const routeEl = document.getElementById("route");
const stepEl = document.getElementById("step");
const posEl = document.getElementById("pos");
const cmdInput = document.getElementById("cmdInput");

// Başlangıç: yuva önündeki mavi kutu
const start = { x: 125, y: 610, angle: -90 };
let robot = { ...start };

let running = false;
let activeRoute = "";
let steps = [];
let stepIndex = 0;
let target = null;
let trail = [];
let photos = [];

// Bu sürüm kod değerlerine körü körüne bağlı değildir.
// Sunum için anlaşılır ev planı ve gerçek hedef bölgeler kullanır.
const WAYPOINTS = {
  START_FRONT: { x: 125, y: 610, angle: -90 },
  HOME_ENTRY:  { x: 125, y: 610, angle: 90 },
  HALL_1:      { x: 125, y: 455, angle: -90 },
  HALL_2:      { x: 280, y: 455, angle: 0 },
  HALL_3:      { x: 500, y: 455, angle: 0 },
  HALL_4:      { x: 760, y: 455, angle: 0 },

  MUTFAK_DOOR: { x: 280, y: 300, angle: -90 },
  MUTFAK_TARGET: { x: 205, y: 165, angle: -90 },

  UTU_DOOR: { x: 500, y: 300, angle: -90 },
  UTU_TARGET: { x: 495, y: 165, angle: -90 },

  SALON_DOOR: { x: 760, y: 360, angle: -90 },
  SALON_TARGET: { x: 790, y: 200, angle: -90 },

  HOME_BACK_START: { x: 125, y: 585, angle: 90 },
  HOME_PARKED: { x: 125, y: 635, angle: 90 }
};

const ROUTES = {
  MUTFAK: [
    ["move", "HALL_1", "Koridora çık"],
    ["move", "HALL_2", "Mutfak hizasına ilerle"],
    ["move", "MUTFAK_DOOR", "Mutfak kapısına dön"],
    ["move", "MUTFAK_TARGET", "Ocak kontrol noktasına git"],
    ["photo", null, "Mutfak raporu / fotoğraf"],
    ["turn", 180, "Güvenli 180° dönüş"],
    ["move", "MUTFAK_DOOR", "Mutfaktan çık"],
    ["move", "HALL_2", "Koridora dön"],
    ["move", "HALL_1", "Yuvaya yaklaş"],
    ["move", "HOME_BACK_START", "Yuva önüne gel"],
    ["reverse", "HOME_PARKED", "Tatlı geri yanaş"]
  ],
  UTU: [
    ["move", "HALL_1", "Koridora çık"],
    ["move", "HALL_3", "Oda hizasına ilerle"],
    ["move", "UTU_DOOR", "Odaya gir"],
    ["move", "UTU_TARGET", "Ütü masası kontrol noktasına git"],
    ["photo", null, "Ütü masası raporu / fotoğraf"],
    ["turn", 180, "Güvenli 180° dönüş"],
    ["move", "UTU_DOOR", "Odadan çık"],
    ["move", "HALL_3", "Koridora dön"],
    ["move", "HALL_1", "Yuvaya yaklaş"],
    ["move", "HOME_BACK_START", "Yuva önüne gel"],
    ["reverse", "HOME_PARKED", "Tatlı geri yanaş"]
  ],
  SALON: [
    ["move", "HALL_1", "Koridora çık"],
    ["move", "HALL_4", "Salon hizasına ilerle"],
    ["move", "SALON_DOOR", "Salona gir"],
    ["move", "SALON_TARGET", "Salon/lamba kontrol noktasına git"],
    ["photo", null, "Salon raporu / fotoğraf"],
    ["turn", 180, "Güvenli 180° dönüş"],
    ["move", "SALON_DOOR", "Salondan çık"],
    ["move", "HALL_4", "Koridora dön"],
    ["move", "HALL_1", "Yuvaya yaklaş"],
    ["move", "HOME_BACK_START", "Yuva önüne gel"],
    ["reverse", "HOME_PARKED", "Tatlı geri yanaş"]
  ]
};

const MOVE_SPEED = 2.7;
const REVERSE_SPEED = 1.6;
const TURN_SPEED = 4.0;

function addChat(text, type = "bot") {
  const div = document.createElement("div");
  div.className = "msg " + type;
  div.innerHTML = text;
  chat.appendChild(div);
  chat.scrollTop = chat.scrollHeight;
}

function updatePanel(state, route, step) {
  stateEl.textContent = state;
  routeEl.textContent = route;
  stepEl.textContent = step;
  posEl.textContent = "x:" + Math.round(robot.x) + " y:" + Math.round(robot.y);
}

function resetSim() {
  robot = { ...start };
  running = false;
  activeRoute = "";
  steps = [];
  stepIndex = 0;
  target = null;
  trail = [{ x: robot.x, y: robot.y }];
  photos = [];
  updatePanel("Beklemede", "Yok", "-");
  draw();
}

function sendCommand(raw) {
  let cmd = String(raw).trim().toUpperCase();
  if (cmd.startsWith("/")) cmd = cmd.slice(1);
  if (cmd === "ÜTÜ") cmd = "UTU";

  addChat("/" + cmd.toLowerCase(), "user");

  if (cmd === "RESET") {
    resetSim();
    addChat("Simülasyon sıfırlandı.", "bot");
    return;
  }

  if (cmd === "DUR") {
    running = false;
    target = null;
    steps = [];
    updatePanel("Acil durdu", activeRoute || "Yok", "DUR");
    addChat("⚠️ Acil fren yapıldı. Motorlar durduruldu.", "bot");
    return;
  }

  if (cmd === "FOTO") {
    photos.push({ x: robot.x, y: robot.y, label: "Anlık Foto" });
    addChat("📸 Anlık durum görüntüsü gönderildi.", "bot");
    draw();
    return;
  }

  if (!ROUTES[cmd]) {
    addChat("Bilinmeyen komut. /mutfak, /utu, /salon, /foto, /dur kullan.", "bot");
    return;
  }

  activeRoute = cmd;
  steps = ROUTES[cmd].map(s => [...s]);
  stepIndex = 0;
  target = null;
  running = true;
  trail = [{ x: robot.x, y: robot.y }];
  photos = [];

  const label = cmd === "UTU" ? "ÜTÜ MASASI" : cmd;
  updatePanel("Görevde", label, "Başladı");
  addChat("🔍 " + label + " görevi başladı. Robot yuvadan çıkıyor.", "bot");
}

function normalizeAngle(a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

function angleToPoint(from, to) {
  return Math.atan2(to.y - from.y, to.x - from.x) * 180 / Math.PI;
}

function prepareStep() {
  if (stepIndex >= steps.length) {
    running = false;
    target = null;
    updatePanel("Görev tamamlandı", activeRoute, "BITTI");
    addChat("✅ Görev tamamlandı. Asistan yuvaya döndü ve beklemede.", "bot");
    return;
  }

  const [kind, value, desc] = steps[stepIndex];
  updatePanel("Görevde", activeRoute, desc);

  if (kind === "photo") {
    photos.push({ x: robot.x, y: robot.y, label: activeRoute + " raporu" });
    addChat("📸 Hedef konum raporu gönderildi.<br>Sıcaklık: 24°C<br>Gaz: 180<br>Görsel kanıt eklendi.", "bot");
    stepIndex++;
    target = null;
    return;
  }

  if (kind === "turn") {
    target = { kind: "turn", angle: robot.angle + value };
    return;
  }

  const wp = WAYPOINTS[value];
  const desiredAngle = kind === "reverse" ? robot.angle : angleToPoint(robot, wp);

  target = {
    kind: "move",
    mode: kind,
    x: wp.x,
    y: wp.y,
    desiredAngle,
    speed: kind === "reverse" ? REVERSE_SPEED : MOVE_SPEED
  };
}

function updateRobot() {
  if (!running) return;

  if (!target) {
    prepareStep();
    return;
  }

  if (target.kind === "turn") {
    let diff = normalizeAngle(target.angle - robot.angle);
    if (Math.abs(diff) <= TURN_SPEED) {
      robot.angle = target.angle;
      stepIndex++;
      target = null;
    } else {
      robot.angle += Math.sign(diff) * TURN_SPEED;
    }
    return;
  }

  if (target.kind === "move") {
    // Önce hareket yönüne dön; geri yanaşmada gövde aynı yönde kalır.
    if (target.mode !== "reverse") {
      let diff = normalizeAngle(target.desiredAngle - robot.angle);
      if (Math.abs(diff) > TURN_SPEED) {
        robot.angle += Math.sign(diff) * TURN_SPEED;
        return;
      } else {
        robot.angle = target.desiredAngle;
      }
    }

    const dx = target.x - robot.x;
    const dy = target.y - robot.y;
    const d = Math.hypot(dx, dy);

    if (d <= target.speed) {
      robot.x = target.x;
      robot.y = target.y;
      trail.push({ x: robot.x, y: robot.y });
      stepIndex++;
      target = null;
    } else {
      robot.x += (dx / d) * target.speed;
      robot.y += (dy / d) * target.speed;
      trail.push({ x: robot.x, y: robot.y });
    }
  }

  posEl.textContent = "x:" + Math.round(robot.x) + " y:" + Math.round(robot.y);
}

function drawMap() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // grid
  ctx.strokeStyle = "#e5edf5";
  ctx.lineWidth = 1;
  for (let x = 0; x <= canvas.width; x += 40) {
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
  }
  for (let y = 0; y <= canvas.height; y += 40) {
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
  }

  // dış ev
  ctx.strokeStyle = "#263747";
  ctx.lineWidth = 3;
  ctx.strokeRect(60, 55, 930, 570);

  // odalar
  drawRoom(90, 80, 260, 230, "MUTFAK", "#fff5e6");
  drawRoom(380, 80, 250, 230, "ODA / ÜTÜ", "#eef8ff");
  drawRoom(660, 80, 290, 320, "SALON", "#f1f8ee");
  drawRoom(90, 350, 860, 210, "KORİDOR", "#fafafa");

  // hedef objeler
  drawAppliance(175, 125, "Ocak");
  drawAppliance(470, 125, "Ütü masası");
  drawAppliance(755, 125, "Lamba");

  // yuva
  ctx.fillStyle = "rgba(42,171,238,.18)";
  ctx.fillRect(90, 585, 70, 55);
  ctx.strokeStyle = "#2aabee";
  ctx.lineWidth = 2;
  ctx.strokeRect(90, 585, 70, 55);
  ctx.fillStyle = "#1c2938";
  ctx.font = "bold 14px Arial";
  ctx.fillText("YUVA", 107, 660);

  // rota kapıları
  ctx.fillStyle = "#2aabee";
  ctx.fillRect(265, 305, 28, 6);
  ctx.fillRect(486, 305, 28, 6);
  ctx.fillRect(746, 397, 28, 6);
}

function drawRoom(x, y, w, h, label, fill) {
  ctx.fillStyle = fill;
  ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = "#263747";
  ctx.lineWidth = 2;
  ctx.strokeRect(x, y, w, h);
  ctx.fillStyle = "#1c2938";
  ctx.font = "bold 18px Arial";
  ctx.fillText(label, x + 18, y + 32);
}

function drawAppliance(x, y, label) {
  ctx.fillStyle = "#ffffff";
  ctx.strokeStyle = "#555";
  ctx.lineWidth = 2;
  ctx.fillRect(x, y, 95, 55);
  ctx.strokeRect(x, y, 95, 55);
  ctx.fillStyle = "#111";
  ctx.font = "13px Arial";
  ctx.fillText(label, x + 10, y + 32);
}

function drawTrail() {
  if (trail.length < 2) return;
  ctx.strokeStyle = "#f39c12";
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.moveTo(trail[0].x, trail[0].y);
  for (const p of trail) ctx.lineTo(p.x, p.y);
  ctx.stroke();
}

function drawPhotos() {
  for (const p of photos) {
    ctx.fillStyle = "#e94235";
    ctx.beginPath();
    ctx.arc(p.x, p.y, 14, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "white";
    ctx.font = "15px Arial";
    ctx.fillText("📸", p.x - 10, p.y + 6);
    ctx.fillStyle = "#111";
    ctx.font = "12px Arial";
    ctx.fillText(p.label, p.x + 18, p.y + 5);
  }
}

function drawRobot() {
  ctx.save();
  ctx.translate(robot.x, robot.y);
  ctx.rotate(robot.angle * Math.PI / 180);

  // gövde
  ctx.fillStyle = "#2aabee";
  ctx.fillRect(-25, -18, 50, 36);

  // kamera yüzü
  ctx.fillStyle = "#111";
  ctx.fillRect(13, -9, 17, 18);

  // kamera lens
  ctx.fillStyle = "#55ffcc";
  ctx.beginPath();
  ctx.arc(22, 0, 4, 0, Math.PI * 2);
  ctx.fill();

  // tekerler
  ctx.fillStyle = "#222";
  ctx.fillRect(-21, -29, 15, 10);
  ctx.fillRect(6, -29, 15, 10);
  ctx.fillRect(-21, 19, 15, 10);
  ctx.fillRect(6, 19, 15, 10);

  ctx.restore();
}

function draw() {
  drawMap();
  drawTrail();
  drawPhotos();
  drawRobot();
}

function loop() {
  updateRobot();
  draw();
  requestAnimationFrame(loop);
}

cmdInput.addEventListener("keydown", e => {
  if (e.key === "Enter") {
    sendCommand(cmdInput.value);
    cmdInput.value = "";
  }
});

window.sendCommand = sendCommand;
resetSim();
loop();
