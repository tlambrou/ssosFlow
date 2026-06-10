document.querySelectorAll("[data-copy]").forEach((button) => {
  button.addEventListener("click", async () => {
    const text = button.getAttribute("data-copy") || "";
    try {
      await navigator.clipboard.writeText(text);
      const previous = button.textContent;
      button.textContent = "Copied";
      setTimeout(() => {
        button.textContent = previous;
      }, 1400);
    } catch {
      button.textContent = "Select";
    }
  });
});

const checklist = Array.from(document.querySelectorAll(".checks input[type='checkbox']"));
const progress = document.querySelector("#guide-progress");
const progressText = document.querySelector("#progress-text");
const reset = document.querySelector("#reset-checklist");
const storageKey = "ssosflow-reactive-ui-guide-checks";
const steps = Array.from(document.querySelectorAll(".step"));
const viewer = document.querySelector("#screenshot-viewer");
const viewerImage = document.querySelector("#screenshot-viewer-image");
const viewerTitle = document.querySelector("#screenshot-viewer-title");
const viewerClose = document.querySelector("#close-screenshot-viewer");
let currentStepIndex = 0;
let lastScreenshotButton = null;

function readStoredChecks() {
  try {
    return JSON.parse(window.localStorage.getItem(storageKey) || "{}");
  } catch {
    return {};
  }
}

function writeStoredChecks() {
  const state = {};
  checklist.forEach((input, index) => {
    state[index] = input.checked;
  });
  window.localStorage.setItem(storageKey, JSON.stringify(state));
}

function updateProgress() {
  const completed = checklist.filter((input) => input.checked).length;
  const total = checklist.length;
  if (progress) {
    progress.max = total || 1;
    progress.value = completed;
  }
  if (progressText) {
    progressText.textContent = `${completed} of ${total} checks complete`;
  }
}

function setCurrentStep(step, options = {}) {
  document.querySelectorAll(".step.is-current").forEach((active) => {
    active.classList.remove("is-current");
  });
  if (step) {
    step.classList.add("is-current");
    const index = steps.indexOf(step);
    if (index >= 0) {
      currentStepIndex = index;
    }
    if (options.scroll) {
      step.scrollIntoView({ behavior: "smooth", block: "start" });
    }
  }
}

function moveStep(offset) {
  if (!steps.length) {
    return;
  }
  const nextIndex = Math.min(Math.max(currentStepIndex + offset, 0), steps.length - 1);
  setCurrentStep(steps[nextIndex], { scroll: true });
}

function openScreenshot(button) {
  if (!viewer || !viewerImage || !viewerTitle) {
    return;
  }
  const src = button.getAttribute("data-screenshot-src") || "";
  const title = button.getAttribute("data-screenshot-title") || "Screenshot";
  lastScreenshotButton = button;
  viewerImage.src = src;
  viewerImage.alt = `${title} full-size screenshot`;
  viewerTitle.textContent = title;
  viewer.hidden = false;
  if (viewerClose) {
    viewerClose.focus();
  }
}

function closeScreenshot() {
  if (!viewer || !viewerImage) {
    return;
  }
  viewer.hidden = true;
  viewerImage.removeAttribute("src");
  if (lastScreenshotButton) {
    lastScreenshotButton.focus();
  }
}

const storedChecks = readStoredChecks();
checklist.forEach((input, index) => {
  input.checked = Boolean(storedChecks[index]);
  input.addEventListener("change", () => {
    writeStoredChecks();
    updateProgress();
  });
});

document.querySelectorAll("[data-step-target]").forEach((button) => {
  button.addEventListener("click", () => {
    const step = document.getElementById(button.getAttribute("data-step-target"));
    if (!step) {
      return;
    }
    setCurrentStep(step, { scroll: true });
  });
});

document.querySelectorAll("[data-screenshot-src]").forEach((button) => {
  button.addEventListener("click", () => {
    openScreenshot(button);
  });
});

if (viewerClose) {
  viewerClose.addEventListener("click", closeScreenshot);
}

if (viewer) {
  viewer.addEventListener("click", (event) => {
    if (event.target === viewer) {
      closeScreenshot();
    }
  });
}

document.addEventListener("keydown", (event) => {
  const viewerOpen = viewer && !viewer.hidden;
  if (event.key === "Escape" && viewerOpen) {
    event.preventDefault();
    closeScreenshot();
    return;
  }
  if (viewerOpen) {
    return;
  }
  if (event.key === "ArrowRight" || event.key === "ArrowDown") {
    event.preventDefault();
    moveStep(1);
  }
  if (event.key === "ArrowLeft" || event.key === "ArrowUp") {
    event.preventDefault();
    moveStep(-1);
  }
});

if (reset) {
  reset.addEventListener("click", () => {
    checklist.forEach((input) => {
      input.checked = false;
    });
    writeStoredChecks();
    updateProgress();
  });
}

updateProgress();
setCurrentStep(steps[0]);
