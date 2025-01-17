/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global __dirname, process */

"use strict";

/**
 * This is a test runner dedicated to run DevTools node tests continuous integration
 * platforms. It will parse the logs to output errors compliant with treeherder tooling.
 *
 * See taskcluster/ci/source-test/node.yml for the definition of the task running those
 * tests on try.
 */

const { execFileSync } = require("child_process");
const { chdir } = require("process");
const path = require("path");

const os = require("os");

// All Windows platforms report "win32", even for 64bit editions.
const isWin = os.platform() === "win32";

// On Windows, the ".cmd" suffix is mandatory to invoke yarn ; or executables in
// general.
const YARN_PROCESS = isWin ? "yarn.cmd" : "yarn";

// Supported node test suites for DevTools
const TEST_TYPES = {
  JEST: "jest",
  TYPESCRIPT: "typescript",
};

const SUITES = {
  aboutdebugging: {
    path: "../aboutdebugging/test/node",
    type: TEST_TYPES.JEST,
  },
  accessibility: {
    path: "../accessibility/test/node",
    type: TEST_TYPES.JEST,
  },
  application: {
    path: "../application/test/node",
    type: TEST_TYPES.JEST,
  },
  compatibility: {
    path: "../inspector/compatibility/test/node",
    type: TEST_TYPES.JEST,
  },
  debugger: {
    path: "../debugger",
    type: TEST_TYPES.JEST,
  },
  framework: {
    path: "../framework/test/node",
    type: TEST_TYPES.JEST,
  },
  netmonitor: {
    path: "../netmonitor/test/node",
    type: TEST_TYPES.JEST,
  },
  performance: {
    path: "../performance-new",
    type: TEST_TYPES.TYPESCRIPT,
  },
  shared_components: {
    path: "../shared/components/test/node",
    type: TEST_TYPES.JEST,
  },
  webconsole: {
    path: "../webconsole/test/node",
    type: TEST_TYPES.JEST,
    dependencies: ["../debugger"],
  },
};

function execOut(...args) {
  let out;
  let err;
  try {
    out = execFileSync(...args);
  } catch (e) {
    out = e.stdout;
    err = e.stderr;
  }
  return { out: out.toString(), err: err && err.toString() };
}

function getErrors(suite, out, err) {
  switch (SUITES[suite].type) {
    case TEST_TYPES.JEST:
      return getJestErrors(out, err);
    case TEST_TYPES.TYPESCRIPT:
      return getTypescriptErrors(out, err);
    default:
      throw new Error("Unsupported suite type: " + SUITES[suite].type);
  }
}

function getJestErrors(out, err) {
  // The string out has extra content before the JSON object starts.
  const jestJsonOut = out.substring(out.indexOf("{"), out.lastIndexOf("}") + 1);
  const results = JSON.parse(jestJsonOut);

  // The individual failing tests are jammed into the same message string :/
  return results.testResults.reduce((p, testResult) => {
    const failures = testResult.message
      .split("\n")
      .filter(l => l.includes("●"));
    return p.concat(failures);
  }, []);
}

function getTypescriptErrors(out, err) {
  // Typescript error lines look like:
  //   popup/panel.jsm.js(103,7): error TS2531: Object is possibly 'null'.
  // Which means:
  //   {file_path}({line},{col}): error TS{error_code}: {message}
  const tsErrorRegex = /error TS\d+\:/;
  return out.split("\n").filter(l => tsErrorRegex.test(l));
}

function runTests() {
  console.log("[devtools-node-test-runner] Extract suite argument");
  const suiteArg = process.argv.find(arg => arg.includes("suite="));
  const suite = suiteArg.split("=")[1];
  if (!SUITES[suite]) {
    throw new Error(
      "Invalid suite argument to devtools-node-test-runner: " + suite
    );
  }

  console.log("[devtools-node-test-runner] Found test suite: " + suite);

  console.log("[devtools-node-test-runner] Check `yarn` is available");
  try {
    // This will throw if yarn is unavailable
    execFileSync(YARN_PROCESS, ["--version"]);
  } catch (e) {
    console.log(
      "[devtools-node-test-runner] ERROR: `yarn` is not installed. " +
        "See https://yarnpkg.com/docs/install/ "
    );
    return false;
  }

  if (SUITES[suite].dependencies) {
    console.log("[devtools-node-test-runner] Running `yarn` for dependencies");
    for (const dep of SUITES[suite].dependencies) {
      const depPath = path.join(__dirname, dep);
      chdir(depPath);

      console.log("[devtools-node-test-runner] Run `yarn` in " + depPath);
      execOut(YARN_PROCESS);
    }
  }

  const testPath = path.join(__dirname, SUITES[suite].path);
  chdir(testPath);

  console.log("[devtools-node-test-runner] Run `yarn` in test folder");
  execOut(YARN_PROCESS);

  console.log(`TEST START | ${SUITES[suite].type} | ${suite}`);

  console.log("[devtools-node-test-runner] Run `yarn test` in test folder");
  const { out, err } = execOut(YARN_PROCESS, ["test-ci"]);

  if (err) {
    console.log("[devtools-node-test-runner] Error log");
    console.log(err);
  }

  console.log("[devtools-node-test-runner] Parse errors from the test logs");
  const errors = getErrors(suite, out, err) || [];
  for (const error of errors) {
    console.log(
      `TEST-UNEXPECTED-FAIL | ${SUITES[suite].type} | ${suite} | ${error}`
    );
  }

  const success = errors.length === 0;
  if (success) {
    console.log(`[devtools-node-test-runner] Test suite [${suite}] succeeded`);
  } else {
    console.log(`[devtools-node-test-runner] Test suite [${suite}] failed`);
    console.log(
      "[devtools-node-test-runner] You can find documentation about the " +
        "devtools node tests at https://firefox-source-docs.mozilla.org/devtools/tests/node-tests.html"
    );
  }
  return success;
}

process.exitCode = runTests() ? 0 : 1;
