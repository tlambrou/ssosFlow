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

function setCurrentStep(step) {
  document.querySelectorAll(".step.is-current").forEach((active) => {
    active.classList.remove("is-current");
  });
  if (step) {
    step.classList.add("is-current");
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
    setCurrentStep(step);
    step.scrollIntoView({ behavior: "smooth", block: "start" });
  });
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
