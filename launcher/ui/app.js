"use strict";

const modeOptions = [...document.querySelectorAll(".mode-option")];
const status = document.getElementById("launch-status");
const playButton = document.getElementById("play-button");
const updateButton = document.getElementById("update-button");
const cardStatus = document.getElementById("card-status");
let gameInstalled = false;
let updateAvailable = false;
let latestVersion = "";
let updateBusy = true;

function selectedMode() {
  return document.querySelector('input[name="world"]:checked')?.value ?? "finite";
}

function updateModeStyles() {
  for (const option of modeOptions) {
    const input = option.querySelector("input");
    option.classList.toggle("selected", input.checked);
  }
  status.textContent = "";
}

for (const option of modeOptions) {
  option.querySelector("input").addEventListener("change", updateModeStyles);
}

playButton.addEventListener("click", () => {
  if (!window.chrome?.webview) {
    status.textContent = "The launcher connection is unavailable.";
    return;
  }
  if (gameInstalled) {
    status.textContent = "Starting MinecraftCopy…";
    window.chrome.webview.postMessage(`play:${selectedMode()}`);
  } else if (latestVersion) {
    beginUpdate("updates:install", "Downloading MinecraftCopy…");
  } else {
    beginUpdate("updates:check", "Checking GitHub releases…");
  }
});

updateButton.addEventListener("click", () => {
  if (updateAvailable || !gameInstalled) {
    beginUpdate("updates:install", "Downloading and verifying game files…");
  } else {
    beginUpdate("updates:check", "Checking GitHub releases…");
  }
});

function beginUpdate(action, message) {
  if (updateBusy) return;
  updateBusy = true;
  playButton.disabled = true;
  updateButton.disabled = true;
  status.textContent = message;
  window.chrome?.webview?.postMessage(action);
}

function applyUpdateState(state) {
  gameInstalled = Boolean(state.gameInstalled);
  updateAvailable = Boolean(state.updateAvailable);
  latestVersion = state.latestVersion || "";
  updateBusy = false;

  const installedLabel = gameInstalled ? state.installedVersion : "Not installed";
  document.getElementById("game-version").textContent = installedLabel;
  document.getElementById("library-version").textContent = installedLabel;
  cardStatus.textContent = gameInstalled ? "INSTALLED" : "NOT INSTALLED";

  playButton.disabled = !gameInstalled && !state.updateCheckSucceeded;
  playButton.innerHTML = gameInstalled
    ? '<span aria-hidden="true">▶</span> PLAY GAME'
    : state.updateCheckSucceeded
      ? '<span aria-hidden="true">↓</span> INSTALL GAME'
      : '<span aria-hidden="true">↻</span> CHECK RELEASES';
  updateButton.disabled = false;
  if (updateAvailable && gameInstalled) {
    updateButton.textContent = `UPDATE TO ${latestVersion}`;
  } else if (!gameInstalled && latestVersion) {
    updateButton.textContent = `INSTALL ${latestVersion}`;
  } else if (!state.updateCheckSucceeded) {
    updateButton.textContent = "RETRY CHECK";
  } else {
    updateButton.textContent = "CHECK FOR UPDATES";
  }
  status.textContent = state.message || "";
}

window.chrome?.webview?.addEventListener("message", (event) => {
  if (event.data && typeof event.data === "object" && event.data.type === "update-state") {
    applyUpdateState(event.data);
    return;
  }
  if (typeof event.data !== "string") {
    status.textContent = "The game launch request could not be completed.";
    return;
  }
  if (event.data.startsWith("game-version:")) {
    const gameVersion = event.data.slice("game-version:".length);
    for (const id of ["game-version", "library-version"]) {
      const label = document.getElementById(id);
      if (label) label.textContent = gameVersion;
    }
    return;
  }
  if (event.data.startsWith("launcher-version:")) {
    const launcherVersion = event.data.slice("launcher-version:".length);
    const profileLabel = document.getElementById("app-version");
    if (profileLabel) profileLabel.textContent = `Launcher ${launcherVersion}`;
    const footerLabel = document.getElementById("footer-version");
    if (footerLabel) footerLabel.textContent = launcherVersion;
    return;
  }
  status.textContent = event.data;
});

window.chrome?.webview?.postMessage("app:ready");
