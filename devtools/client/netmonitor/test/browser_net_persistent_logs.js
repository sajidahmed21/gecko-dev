/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network monitor leaks on initialization and sudden destruction.
 * You can also use this initialization format as a template for other tests.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SINGLE_GET_URL);
  info("Starting test... ");

  let { document, windowRequire } = monitor.panelWin;

  Services.prefs.setBoolPref("devtools.webconsole.persistlog", false);

  yield reloadAndWait();

  is(document.querySelectorAll(".request-list-item").length, 2,
    "The request list should have two items at this point.");

  yield reloadAndWait();

  // Since the reload clears the log, we still expect two requests in the log
  is(document.querySelectorAll(".request-list-item").length, 2,
    "The request list should still have two items at this point.");

  // Now we toggle the persistence logs on
  Services.prefs.setBoolPref("devtools.webconsole.persistlog", true);

  yield reloadAndWait();

  // Since we togged the persistence logs, we expect four items after the reload
  is(document.querySelectorAll(".request-list-item").length, 4,
    "The request list should now have four items at this point.");

  Services.prefs.setBoolPref("devtools.webconsole.persistlog", false);
  return teardown(monitor);

  /**
   * Reload the page and wait for 2 GET requests. Race-free.
   */
  function reloadAndWait() {
    let wait = waitForNetworkEvents(monitor, 2);
    tab.linkedBrowser.reload();
    return wait;
  }
});
